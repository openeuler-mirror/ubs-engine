项目内部通用工具包

该 misc 下的文件为 src 内的代码使用，外部代码如 plugin 不能使用。

---

## 自动序列化框架

基于 C++17 反射的自动序列化/反序列化框架，替代手工 SIMPO 代码。

### 文件

| 文件 | 用途 |
|------|------|
| `ubse_auto_serial_util.hpp` | 框架核心：trait 主模板、所有 `SerializeField`/`DeSerializeField` 重载、`UBSE_SERIALIZE` 宏 |
| `ubse_auto_simpo.hpp` | 消息包装：`UbseAutoSimpo<T>`（单值）、`UbseAutoSimpoTuple<Args...>`（多值 tuple） |
| `src/framework/serde/ubse_serial_util.h` | 底层二进制流读写，提供 `UbseSerialization`/`UbseDeSerialization`、`addr_len`、`enum_v` 等 |
| `.../message/ubse_mem_auto_serial.h` | 模块级：trait 特化 + `UBSE_SERIALIZE` 类型注册（各模块按此模式自建） |

### 声明可序列化类型

使用 `UBSE_SERIALIZE(Class, Base, memberPtr1, memberPtr2, ...)` 宏：

```cpp
// 当前命名空间中的类型
UBSE_SERIALIZE(MyStruct, void, &MyStruct::fieldA, &MyStruct::fieldB, &MyStruct::fieldC);

// 其他命名空间中的类型需用完全限定名
UBSE_SERIALIZE(::other_ns::MyStruct, ::other_ns::BaseStruct,
               &::other_ns::MyStruct::fieldA, &::other_ns::MyStruct::fieldB);
```

- `Class` — 类型名（完全限定）
- `Base` — 直接基类（完全限定），无基类填 `void`
- 后续参数 — 成员指针列表，使用 `&Class::member` 语法（按序列化顺序）

宏展开后在 `namespace ubse::serial::util` 中生成 `SerializableMembers<Class>`（成员指针 tuple）和 `SerializationTraits<Class>`（基类类型别名），框架通过 SFINAE 找到对应的 `SerializeField`/`DeSerializeField` 重载。

### 自动支持的类型

框架为以下类型提供了开箱即用的序列化重载（全部在 `namespace ubse::serial::util` 中）：
- **基础类型**：所有算术类型、`std::string`
- **枚举**：通过 `enum_v()` 包装
- **raw_memcopy**：按原始字节拷贝，需特化 `is_raw_memcopy<T>` 为 `std::true_type`
- **bitfield**：按 `uint16_t` 打包，需特化 `is_bitfield<T>` 为 `std::true_type` 并实现 `bitfield_traits<T>`
- **容器**：`std::vector<T>`、`std::unordered_map<K,V>`、`std::unordered_set<T>`
- **C 数组**：`T[N]`
- **嵌套反射**：成员为其他 `UBSE_SERIALIZE` 声明的类型时自动递归

### 无法使用 UBSE_SERIALIZE 宏的类型

以下三类类型无法用成员指针做逐字段序列化，框架提供两个 trait 走特殊路径。

#### raw_memcopy：按原始字节拷贝

在模块的 auto_serial 头文件（如 `ubse_mem_auto_serial.h`）中，在 `namespace ubse::serial::util` 内特化：

```cpp
template <>
struct is_raw_memcopy<MyPodType> : std::true_type {};
```

适用场景：
- `extern "C"` 类型（如 `ubse_mem_obmm_mem_desc`）— C 链接不可特化 C++ 模板
- 含 `union` 的 POD（如 `ubs_topo_ip_address_t`）— union 成员地址相同
- 纯 `uint64_t` 组合（如 `UbseMemAddrInfo`）— 零开销，无需逐字段编解码

框架自动以 `addr_len<uint8_t>` 做整块内存拷贝，无需手写序列化函数。

#### bitfield：按位域打包为 uint16_t

在模块的 auto_serial 头文件中，在 `namespace ubse::serial::util` 内特化：

```cpp
template <>
struct is_bitfield<MyBitStruct> : std::true_type {};

template <>
struct bitfield_traits<MyBitStruct> {
    static uint16_t pack(const MyBitStruct& data);
    static void unpack(uint16_t val, MyBitStruct& data);
};
```

适用场景：含位域成员的结构体（如 `UbseMemPrivData`）— 成员指针无法指向位域。

### 命名空间注意事项

所有序列化注册代码（`UBSE_SERIALIZE` 宏和 trait 特化）**必须**在 `namespace ubse::serial::util { }` 内编写。`UBSE_SERIALIZE` 宏展开为 `template<> struct SerializableMembers<Class> { ... }`，本身不带命名空间声明，依赖外围的命名空间上下文。

模块 auto_serial 头文件的标准写法：

```cpp
// ============================================================
// 1. trait 特化块（需要 using namespace 简化类型名）
// ============================================================
namespace ubse::serial::util {

using namespace ::other_ns;   // 让特化中可直接写短类型名

template <> struct is_raw_memcopy<SomePodType> : std::true_type {};
template <> struct is_bitfield<SomeBitStruct> : std::true_type {};
template <> struct bitfield_traits<SomeBitStruct> { ... };

} // namespace ubse::serial::util

// ============================================================
// 2. UBSE_SERIALIZE 注册块（建议用完全限定名，防止重名歧义）
// ============================================================
namespace ubse::serial::util {

UBSE_SERIALIZE(::other_ns::MyStruct, void,
               &::other_ns::MyStruct::fieldA,
               &::other_ns::MyStruct::fieldB);

} // namespace ubse::serial::util
```

几个要点：
- **不能**把 trait 特化或 `UBSE_SERIALIZE` 写在 `ubse::serial` 中 — `ubse_auto_simpo.hpp` 使用限定调用 `ubse::serial::util::SerializeField`，限定查找只搜 `ubse::serial::util`，搜不到 `ubse::serial` 中的重载
- 底层基础类型（`UbseSerialization`、`addr_len`、`enum_v` 等）定义在 `namespace ubse::serial`，由 `ubse_auto_serial_util.hpp` 顶部的 `using namespace ubse::serial;` 导入，对使用方透明
- 被序列化的类型本身可以来自任意命名空间 — 只需在注册时写出完全限定名即可

### ubse_auto_simpo.hpp — 消息包装器

`UbseAutoSimpo<T>` 和 `UbseAutoSimpoTuple<Args...>` 继承自 `UbseBaseMessage`，与现有消息框架无缝集成。序列化/反序列化逻辑由框架自动生成，无需手写 SIMPO 代码。

#### UbseAutoSimpo<T> — 单值消息

```cpp
// 1. 序列化路径：C++ 对象 → 二进制 buffer
auto msg = MakeRef<ubse::simpo::util::UbseAutoSimpo<MyStruct>>();
msg->SetUbseMesgInfo(myData);                              // 写入待序列化数据
msg->Serialize();                                           // 自动序列化
Send(msg->GetOutputRawData(), msg->GetOutputRawDataSize()); // 获取 buffer 发送

// 2. 反序列化路径：二进制 buffer → C++ 对象
auto msg = MakeRef<ubse::simpo::util::UbseAutoSimpo<MyStruct>>(rawData, size);
msg->Deserialize();                      // 自动反序列化
MyStruct result = msg->GetUbseMesgInfo(); // 取出结果
```

- `SetUbseMesgInfo(Arg)` — 设置待序列化的值（值拷贝）
- `GetUbseMesgInfo()` — 获取反序列化后的值（值返回）
- `Serialize()` / `Deserialize()` — 由框架自动调用 `ubse::serial::util::SerializeField` / `DeSerializeField`

#### UbseAutoSimpoTuple<Args...> — 多值 tuple 消息

不需要定义 struct，直接按位置传递多个值：

```cpp
auto msg = MakeRef<ubse::simpo::util::UbseAutoSimpoTuple<int, std::string, MyEnum>>();
msg->SetUbseMesgInfo({42, "hello", MyEnum::VALUE_A});
msg->Serialize();

// 按索引取值
auto [a, b, c] = msg->GetUbseMesgInfo();
int x = msg->Get<0>();
```

- `SetUbseMesgInfo(std::tuple<Args...>)` / `GetUbseMesgInfo()` — 整体读写
- `Get<I>()` / `Set<I>(value)` — 按索引读写单个元素

#### 智能指针别名

```cpp
ubse::simpo::util::UbseAutoSimpoPtr<T>          // = Ref<UbseAutoSimpo<T>>
ubse::simpo::util::UbseAutoSimpoTuplePtr<Args...> // = Ref<UbseAutoSimpoTuple<Args...>>
```

#### 与现有消息框架的关系

两者都继承自 `UbseBaseMessage`，构造时 `(data, size)` 重载调用 `SetInputRawData()` 注册输入 buffer，`Serialize()` 后将结果写入 `mOutputRawData` / `mOutputRawDataSize`，与手工 SIMPO 消息的使用方式完全一致。

### 安全边界

反序列化时容器元素数量受 `ONCE_LIMIT_LEN`（65536）约束，超过该值的消息直接拒绝，防止恶意/损坏消息导致内存耗尽。