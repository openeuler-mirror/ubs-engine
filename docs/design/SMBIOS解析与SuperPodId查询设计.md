# SMBIOS 解析与 SuperPodId 查询设计

## 1. 背景与需求

SuperPodId（二层超节点ID）标识节点所属的计算集群，由 BIOS 写入 SMBIOS 表的 OEM 自定义结构（Type 131，SUPER_POD_BASIC_INFO_T）中。上层应用（集群管理、拓扑上报、资源调度）需要在本地查询该值，用于：

- 判定节点归属集群，支撑跨节点组网拓扑上报（urma_uvs 的 superNodeId 填充）
- 作为 topo 查询的全局附属信息，随节点信息一并下发至 SDK 消费方

需求归纳为两点：

1. **解析 BIOS 文件**：从 sysfs 导出的 SMBIOS 表中解析 Type 131 结构，提取 superPodId 等字段
2. **对外提供 include 接口**：通过 SDK 头文件（`ubs_engine_topo.h`）向外部应用暴露查询能力

本文档描述该链路的端到端设计（现状为已实现设计，代码位于 630 分支）。

## 2. 总体架构

```text
┌─────────────────────────────────────────────────────────────────────┐
│                            BIOS / BMC                                │
│              开机写入 SMBIOS 表（Type 131 超节点基础信息）              │
└────────────────────────────┬────────────────────────────────────────┘
                             │ 内核导出
┌────────────────────────────▼────────────────────────────────────────┐
│                     sysfs DMI 表文件（只读）                          │
│   /sys/firmware/dmi/tables/smbios_entry_point   (SMBIOS 3.0 入口点)  │
│   /sys/firmware/dmi/tables/DMI                  (DMI 结构表)         │
└────────────────────────────┬────────────────────────────────────────┘
                             │ 文件读取 + 解析
┌────────────────────────────▼────────────────────────────────────────┐
│              SMBIOS 解析层（src/adapter_plugins/mti/smbios）         │
│  ubse_smbios_def.h/.cpp   结构定义 / DMI 表遍历 / 字段提取            │
│  ubse_smbios_impl.h       单例 + per-type 缓存（ReadWriteLock）      │
└────────────────────────────┬────────────────────────────────────────┘
                             │ 门面接口（单例）
┌────────────────────────────▼────────────────────────────────────────┐
│        UbseSmbios 门面（src/include/adapter_plugins/mti/ubse_smbios.h）│
│   GetSuperPodId / GetPodId / GetServerIdx / GetMeshType /            │
│   IsClosType / GetSystemManufacturer / IsQemuVm / Is1650V100Cpu      │
└──────┬──────────────────────────────┬───────────────────────────────┘
       │ daemon 进程内直接调用          │ IPC 打包时携带
┌──────▼──────────────┐   ┌───────────▼───────────────────────────────┐
│  daemon 内部消费方    │   │  api_server / node controller             │
│  · urma_uvs 拓扑上报  │   │  UbseNodePack / UbseNodeListPack          │
│  · election/com 判断  │   │  （superPodId 追加在 node 数据之后）        │
│    CLOS 组网类型      │   └───────────┬───────────────────────────────┘
└─────────────────────┘               │ UDS IPC (UBSE_NODE 模块)
                                      ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    SDK（src/sdk/c/include/ubs_engine_topo.h）        │
│      ubs_topo_node_t.super_pod_id  ←  ubs_topo_node_list /           │
│                                       ubs_topo_node_local_get       │
│              （Python / Go SDK 同结构透出）                            │
└────────────────────────────┬────────────────────────────────────────┘
                             │
                        外部应用进程
```

设计要点：

- **单次解析、进程内缓存**：SMBIOS 表在单次开机周期内不变，解析结果按结构类型缓存，后续查询零文件 IO
- **全局值语义**：superPodId 是本机所属集群的标识，不是 per-node 值；集群内所有节点该值相同
- **内外分层**：解析层不感知业务；门面接口供 daemon 内部模块使用；外部应用一律经 IPC 由 SDK 透出，不直接读 SMBIOS 文件

## 3. SMBIOS 解析层设计

### 3.1 数据源与文件布局

| 文件 | 内容 | 读取上限 |
|------|------|---------|
| `/sys/firmware/dmi/tables/smbios_entry_point` | SMBIOS 3.0 入口点（`_SM3_` 签名，含结构表大小与地址） | 0x20 字节 |
| `/sys/firmware/dmi/tables/DMI` | DMI 结构表（连续的结构体序列） | 入口点中 `structure_table_max_size`，硬上限 256MB |

选择 sysfs 而非 `/dev/mem` 物理内存访问：内核已将 DMI 表导出为只读文件，无需物理地址翻译，且权限收敛。

注意：sysfs 的 DMI 文件内容即结构表本体，因此忽略入口点中的 64 位结构表**物理地址**，直接从文件偏移 0 读取（`FLAG_NO_FILE_OFFSET`）。

### 3.2 SMBIOS 3.0 入口点结构

```text
偏移    长度   字段                          代码常量
0x00    5     anchor "_SM3_"                签名校验
0x05    1     checksum
0x06    1     entry point length (≤0x20)    ENTRY_POINT_LENGTH / _MAX
0x07    1     major version (=3)            SMBIOS_MAJOR_VERSION
0x08    1     minor version                 SMBIOS_MINOR_VERSION
...
0x0C    4     structure table max size      STRUCTURE_TABLE_MAXIMUM_SIZE
0x10    8     structure table address(64位) STRUCTURE_TABLE_ADDRESS
```

仅支持 SMBIOS 3.0（`_SM3_` 签名）；2.x 入口点（`_SM_`）不支持，命中即报错返回。

### 3.3 解析主流程

入口为模板函数 `UbseSmbiosImpl::GetSmbiosTypeInfo<type>()`（编译期类型分发，`SmbiosTypeMap` 将枚举映射到结构类）：

```text
GetSmbiosTypeInfo<type>
│
├─① 读锁查缓存 map<type, shared_ptr>
│     hit → dynamic_pointer_cast 返回（零文件 IO 快路径）
│
├─② miss → LoadSysEntryFile 读入口点文件（≤0x20 字节）
│     ├─ 空/长度 < 0x10+8 → UBSE_ERROR_IO / INVAL
│
├─③ 校验 "_SM3_" 签名（非 3.0 → 报错返回，不缓存）
│
├─④ CreateSmbiosStruct<type>() 创建结构对象
│     └─ DecodeSmbios3(entryBuf, DMI路径, FLAG_NO_FILE_OFFSET, type)
│          ├─ 入口点长度合法性校验
│          ├─ QWORD 读结构表地址 / DWORD 读表大小
│          └─ GetDmiTable：从 DMI 文件偏移 0 读 max size 字节
│               └─ DecodeDmiTable：遍历结构表（见 3.5）
│                    └─ FillSmbiosStructFromBuf：子类提取字段（见 3.4）
│
└─⑤ 解析成功 → 写锁插入缓存，返回结构指针
      解析失败 → 不缓存，直接返回 nullptr（下次调用可重试）
```

### 3.4 Type 131（SUPER_POD_BASIC_INFO_T）结构布局

Type 131 为 OEM 自定义结构，格式化区域布局如下（`rawData` 数组下标，含 4 字节 SMBIOS 公共头；实际数据按头长度拷入 1024 字节缓冲，未覆盖区域保持 0xff）：

| rawData 下标 | 字段 | 类型 | 说明 |
|--------------|------|------|------|
| [0..3] | header | - | type=0x83(131)、length、handle |
| [4] | flag | uint8 | **有效性标志**：0x00 有效；0xff 表示 BMC 未成功写入数据 |
| [5..6] | podId | uint16 | 机架号/超节点ID（小端） |
| [7] | slotId | uint8 | 槽位ID |
| [8] | meshType | uint8 | 组网类型：0x80 FULL_MESH / 0x81 CLOS |
| [9..10] | **superPodId** | **uint16** | **二层超节点ID（小端），位于组网类型字段之后** |
| [13..14] | serverIdx | uint16 | 服务器索引（小端） |
| [15..78] | reserved | uint8[64] | 保留字段 |

解析规则（`SmbiosSuperPodBasicInfo::FillSmbiosStructFromBuf`）：

1. 长度守卫：格式化区域至少 7 字节，不足报 `UBSE_ERROR_INVAL`
2. **flag 校验先行**：flag=0xff 时返回 `UBSE_ERROR_AGAIN`（BMC 数据未就绪），不再解析后续字段——这是可重试错误，与"失败不缓存"配合，BMC 延迟写入场景可在后续调用中自愈
3. 逐字段按绝对偏移小端提取，解析成功后打印 INFO 日志输出全部字段值（`LogSmbiosStructTypeInfo`）

### 3.5 DMI 表遍历

DMI 表是连续结构体序列，每个结构体 = 4 字公共头 + 格式化区域 + 字符串表（以双 `\0` 结尾）：

```text
┌────────┬──────────────┬─────────────────┬────────┬─────···─────┐
│ header │ format body  │ string table    │ header │  ...        │
│ 4字节  │ length-4 字节 │ 以 "\0\0" 结束  │        │             │
└────────┴──────────────┴─────────────────┴────────┴─────────────┘
```

遍历算法（`DecodeDmiTable`）：

- 循环条件：剩余长度 ≥ 公共头 4 字节
- 头部 length < 4 → `UBSE_ERROR_INVAL`（防越界）
- **type 不匹配**：跳过 = `length` + 扫描字符串表至双 `\0` + 2 字节，推进到下一结构体；越界即报错
- **type 匹配**：校验 length 不超出剩余空间，记录 `availableLength`（供字符串表解析使用），调用子类 `FillSmbiosStructFromBuf` 后返回
- **遇到 Type 127（End-of-Table）**：SMBIOS 3.0 路径下（`FLAG_STOP_AT_EOT`）判定表异常直接报错——v3 表以表长度界定边界，不应包含 EOT 结构

字符串表解析（`GetSmbiosString`，供 Type 1/4 使用）：按编号逐个跳过 `\0` 结尾的字符串；表首单个 `\0` 可能是合法空字符串，仅以"连续双 `\0`"或到达缓冲区末尾判定表结束，防止误判。

### 3.6 缓存与并发

| 设计项 | 方案 | 理由 |
|--------|------|------|
| 实例模型 | Meyers 单例（`UbseSmbiosImpl::GetInstance` / `UbseSmbios::GetInstance`） | 解析结果进程级共享，无显式初始化时序依赖 |
| 缓存粒度 | `map<UbseSmbiosType, shared_ptr<SmbiosStructure>>`，per-type | 不同类型互不影响，解析一次终身复用 |
| 锁策略 | `ReadWriteLock`：读锁查缓存，写锁插入 | 查询为高频路径（每次 topo IPC 都会触发），读多写少 |
| 失败语义 | 解析失败**不缓存** | flag=0xff（BMC 未就绪）等瞬时故障可随后续调用重试；SMBIOS 表本身开机不变，成功后无失效问题 |
| 返回值 | `shared_ptr` + `dynamic_pointer_cast` | 缓存基类指针，模板类型安全还原为派生结构 |

并发场景说明：读锁内仅做 map 查找与指针拷贝；文件解析在锁外执行，多线程同时 miss 时重复解析各写各的结果，最后写入者胜出——结果幂等（同一文件、同一解析逻辑），无正确性问题。

### 3.7 安全设计

| 风险 | 对策 |
|------|------|
| sysfs DMI 文件通常仅 root 可读 | `OpenSmbiosFile` 临时向 effective capabilities 注入 `CAP_DAC_OVERRIDE`，open 成功后立即恢复（对称恢复，失败路径同样恢复） |
| 路径注入/符号链接 | 读取前 `realpath` 解析真实路径 |
| 超长文件/内存耗尽 | 读取上限 `min(入口点表大小, 文件实际大小, 256MB)`；分配使用 `new (std::nothrow)` |
| 短读/信号打断 | `read` 循环累计至目标长度，`EINTR` 重试，其余 errno 报 `UBSE_ERROR_IO` |
| fd 泄漏 | 所有失败路径统一 `CloseFd`，close 失败打 ERROR 日志 |

## 4. 对外接口设计

### 4.1 内部门面接口（C++，daemon 进程内）

声明于 [ubse_smbios.h](../../src/include/adapter_plugins/mti/ubse_smbios.h)，全部为单例方法：

| 接口 | 数据源结构 | 主要消费方 |
|------|-----------|-----------|
| `GetSuperPodId(uint16_t&)` | Type 131 | topo IPC 打包、urma_uvs 拓扑上报 |
| `GetPodId(uint16_t&)` | Type 131 | 组网/拓扑模块 |
| `GetServerIdx(uint32_t&)` | Type 131 | 服务器索引上报 |
| `GetMeshType(UbseMeshType&)` / `IsClosType()` | Type 131 | election/com/cluster info 的组网类型分支 |
| `GetSystemManufacturer(std::string&)` | Type 1 | 厂商信息查询 |
| `IsQemuVm()` | Type 1 | 仿真环境判定（制造商为 "QEMU"，大小写不敏感） |
| `Is1650V100Cpu()` | Type 4 | EID 共享能力判定（匹配 "kunpeng 950 7592c" / "1650v100"） |

错误规约：结构不存在/解析失败统一返回 `UBSE_ERROR` 并打 ERROR 日志；bool 型接口失败时取安全默认值（`IsClosType` → false 即 FULL_MESH，`IsQemuVm` → false 即非仿真）。

### 4.2 daemon 内部消费方

| 消费方 | 用途 |
|--------|------|
| `ubse_node_api_convert.cpp` `UbseNodePack` / `UbseNodeListPack` | topo 响应尾部追加 superPodId（见 4.4），失败时打包 0 并继续（`(void)` 吞掉返回值，不阻断 topo 主流程） |
| `ubse_urma_uvs.cpp` `FillClusterInfo` | 向 uvs 上报拓扑时填充每个节点的 superNodeId；获取失败仅 WARN，填 0 继续 |
| `ubse_node_api.cpp` `UbseQueryClusterInfo`（CLI 集群信息查询） | 使用 `IsClosType` 决定 role/bondingEid 列的展示 |
| election / com engine | CLOS 组网类型分支判断 |

### 4.3 SDK include 接口（对外）

SDK C 头文件 [ubs_engine_topo.h](../../src/sdk/c/include/ubs_engine_topo.h)：

```c
typedef struct {
    ...
    uint16_t super_pod_id;         // 超节点ID（全局值，集群内所有节点相同）
} ubs_topo_node_t;

int32_t ubs_topo_node_local_get(ubs_topo_node_t* node);      // 本节点（含 super_pod_id）
int32_t ubs_topo_node_list(ubs_topo_node_t** node_list, uint32_t* node_cnt);  // 全量节点（含 super_pod_id）
```

多语言透出：

| SDK | 文件 | 说明 |
|-----|------|------|
| C | `src/sdk/c/libubse_helper.cpp` | `ubse_node_unpack` / `ubse_node_list_unpack` 尾部解包 superPodId，列表场景设置到所有节点 |
| Python | `src/sdk/python/ffi/ubs_engine_types.py`、`models/ubs_engine_model_topo.py` | `UbsTopoNodeT` 增加 `ctypes.c_uint16` 字段并转模型 |
| Go | `src/sdk/go/topo/ubs_engine_topo.go` | `UbsTopoNode` 增加 `SuperPodId uint16` 字段 |

### 4.4 IPC 链路与数据格式

```text
应用进程                        ubse daemon
─────────                      ─────────────────────────────────────────
ubs_topo_node_local_get
 └─ ubse_invoke_call(UBSE_NODE,          ┌──────────────────────────────┐
        UBSE_NODE_GET=0x0004)  ──UDS──▶  │ UbseServerNodeGet             │
                                        │ (TOPO_PERMISSION 鉴权)        │
                                        │  └─ UbseNodePack              │
                                        │      ├─ node 原有字段          │
                                        │      └─ 尾部 UbsePackUint16(   │
                                        │         GetSuperPodId())      │
ubse_node_unpack ◀─────────────────────└──────────────────────────────┘
 └─ node 字段 + 尾部 super_pod_id
ubs_topo_node_list 同理（UBSE_NODE_LIST=0x0001，UbseNodeListPack）
```

数据格式与前后向兼容（superPodId 追加在所有 node 数据之后、而非嵌入每个 node 内部，保证旧 SDK 解包无偏移错位）：

| 场景 | 行为 |
|------|------|
| 新SDK + 旧后端 | 尾部字段缺失，解包失败时默认 `super_pod_id=0` + INFO 日志 |
| 旧SDK + 新后端 | 旧 SDK 不读尾部 2 字节，node 按原长度解包，正常工作 |

字段级兼容细节见已有设计文档 [topo查询接口支持SuperPodId.md](topo查询接口支持SuperPodId.md)。

## 5. 错误码语义

| 错误码 | 产生位置 | 语义 |
|--------|---------|------|
| `UBSE_ERROR_IO` | 文件读取 | 入口点/DMI 文件打开、读取、关闭失败 |
| `UBSE_ERROR_INVAL` | 解析层 | 入口点长度无效、结构长度越界、字符串编号无效 |
| `UBSE_ERR_NOT_SUPPORTED` | 解析层 | 非 SMBIOS 3.0 入口点；结构类型不支持 |
| `UBSE_ERROR_AGAIN` | Type 131 | flag=0xff，BMC 数据未写入，可重试 |
| `UBSE_ERROR` | 门面/遍历 | 未找到目标类型结构、通用失败 |

门面接口（`GetSuperPodId` 等）对上层统一收敛为 `UBSE_OK` / `UBSE_ERROR` 两类，原始错误码仅体现在日志中，便于调用方简化处理。

## 6. 测试设计

### 6.1 解析层 UT（test/UT/adapter_plugins/mti/smbios/）

通过 `MakeEntryPoint` / `MakeSuperPodData` / `MakeDmiBuf` / `MakeSkipEntry` 等构造函数在内存中伪造入口点与 DMI 表（子类暴露 protected `DecodeDmiTable` / `FillSmbiosStructFromBuf`），覆盖：

| 类别 | 用例 |
|------|------|
| 入口点校验 | 签名非法、长度越界、长度不足 |
| DMI 遍历 | 跳过不匹配 type（含字符串表）、结构越界、EOT 报错、正常命中 |
| Type 131 字段解析 | 各字段小端提取、长度不足、flag=0xff 返回 AGAIN |
| Type 1/4 字符串表 | 编号解析、空字符串、双 `\0` 结束判定、编号越界 |
| 缓存 | 命中不重复读文件、失败不缓存可重试 |

### 6.2 IT stub（test/IT/stubs/smbios/ubse_smbios_mock.cpp）

IT 环境无真实 SMBIOS Type 131 表，通过 `--whole-archive` 符号覆盖将 `UbseSmbios` 门面方法替换为环境变量注入版本（`UBSE_IT_SUPER_POD_ID` / `UBSE_IT_POD_ID` / `UBSE_IT_MESH_TYPE` / `UBSE_IT_SERVER_IDX`），业务代码零改动。

### 6.3 SDK UT（test/UT/sdk/test_ubs_engine_topo.cpp）

覆盖 super_pod_id 解包正确性及新旧后端兼容（截断尾部 2 字节模拟旧后端，默认 0）。

### 6.4 真实环境验证

真机（含 BMC 写入的 Type 131 表）验证 `ubs_topo_node_local_get` 返回的 super_pod_id 与 `dmidecode -t 131` 解析值一致；QEMU 仿真环境验证无 Type 131 时 topo 接口仍正常返回（super_pod_id=0）。

## 7. 边界与约束

| 约束 | 说明 |
|------|------|
| 仅支持 SMBIOS 3.0 | `_SM3_` 入口点；2.x 不支持 |
| 单次开机数据不变 | 解析成功后进程生命周期内使用缓存，不感知 BMC 运行期改写 |
| flag=0xff 可重试 | 解析失败不缓存，BMC 延迟写入场景随后续调用自愈 |
| 通用服务器/QEMU 无 Type 131 | 查询返回失败，topo 链路降级为 super_pod_id=0，不阻断主流程 |
| 权限依赖 | 依赖 `CAP_DAC_OVERRIDE`（root 或 capability 配置）读取 DMI 表文件 |
| 全局值语义 | superPodId 为集群级标识，不作为 per-node 差异化数据使用 |
