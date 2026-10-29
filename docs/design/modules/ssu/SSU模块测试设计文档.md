# SSU 模块测试设计文档

> 源码路径：`src/controllers/ssu/`、`src/adapter_plugins/ssu/`、`src/cli/ssu/`
> 测试路径：`test/UT/controllers/ssu/`、`test/UT/adapter_plugins/ssu/`、`test/UT/cli/test_ubse_cli_ssu_*`
> 文档版本：V1.1
> 编写日期：2026-07-25
> 更新日期：2026-10-27（CLI 特性测试设计已并入本文档，原《SSU模块CLI特性测试设计文档》删除）

---

## 1. 引言

### 1.1 文档目的

本文档描述 UBS-Engine 中 SSU（Storage Subsystem Unit）控制器的测试设计，明确测试策略、测试分层、用例设计方法、测试场景与验收标准，作为开发、测试与持续集成（CI）的依据，确保 SSU 模块的功能正确性、可靠性、性能与可维护性满足需求规约。

### 1.2 文档范围

涵盖 SSU 模块的以下测试域：

- 单元测试（UT）：调度器、债务账本、预留管理、采集器、VFE 直通管理、适配器实现；CLI 命令注册/处理（cmd_reg）、wire 契约（struct）、校验函数（limits）、IPC 模板
- 集成测试（IT）：模块装配、插件加载、RPC 通信、跨模块联动；CLI ↔ CLI Framework 装配、CLI ↔ daemon UDS 协议互通
- 性能测试（PT）：响应时间、并发能力、调度算法执行时间；CLI 单命令响应时间、启动到退出时间
- 模糊测试（FUZZ）：异常输入、超长字符串、空指针、整型溢出；wire 反序列化输入、CLI 字符串参数
- 端到端测试（E2E）：从 SDK/HTTP/CLI 接入到硬件操作的完整链路

### 1.3 术语与缩略语

| 术语 | 含义 |
|------|------|
| UT | Unit Test，单元测试 |
| IT | Integration Test，集成测试 |
| PT | Performance Test，性能测试 |
| FUZZ | Fuzzing Test，模糊测试 |
| E2E | End-to-End Test，端到端测试 |
| GTest | Google Test，C++ 单元测试框架 |
| MockCpp | 轻量级 C++ Mock 框架，用于接口打桩 |
| MOCKER_CPP_VIRTUAL | MockCpp 宏，mock 虚函数 |
| MOCKER_CPP | MockCpp 宏，mock 普通函数 |
| -fno-access-control | 编译选项，打破 C++ 访问权限控制，便于测试私有成员 |
| 覆盖率 | 代码被执行的行/分支/函数占比 |
| Filter | 责任链中的处理节点 |
| Fixture | GTest 测试夹具，提供 SetUp/TearDown |
| UDS | Unix Domain Socket，CLI 与 daemon 间的本机 IPC 通道 |
| wire 契约 | CLI 与 daemon 之间 IPC body 的二进制字段序列约定 |
| RAII | Resource Acquisition Is Initialization，资源获取即初始化 |

### 1.4 参考资料

- [SSU模块需求分析文档.md](./SSU模块需求分析文档.md) —— 需求与验收标准
- [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) —— 设计与时序图
- [单元测试开发指南.md](../../../test/单元测试开发指南.md) —— UT 框架与规范
- [AGENTS.md](../../../../AGENTS.md) —— 构建与测试命令

---

## 2. 测试策略

### 2.1 测试金字塔

```
                    ┌──────────┐
                    │   E2E    │  少量，覆盖关键业务链路
                    └──────────┘
                ┌──────────────────┐
                │   PT / FUZZ      │  中量，关注性能与健壮性
                └──────────────────┘
            ┌──────────────────────────┐
            │         IT               │  中量，模块间联动
            └──────────────────────────┘
        ┌──────────────────────────────────┐
        │              UT                  │  大量，覆盖核心逻辑
        └──────────────────────────────────┘
```

### 2.2 分层测试目标

| 测试层 | 目标 | 执行频率 | 责任人 |
|--------|------|----------|--------|
| UT | 验证子模块函数级行为，覆盖率 ≥ 80% | 每次提交（CI 强制） | 开发 |
| IT | 验证模块装配、RPC、插件加载 | 每日构建 | 开发 + 测试 |
| PT | 验证响应时间、并发能力达标 | 每版本发布前 | 测试 |
| FUZZ | 发现异常输入导致的崩溃与未定义行为 | 每版本发布前 | 测试 |
| E2E | 验证完整业务链路达到验收标准 | 每版本发布前 | 测试 |

### 2.3 测试通过准则

- UT：100% 通过，核心子模块行覆盖率 ≥ 80%，分支覆盖率 ≥ 70%
- IT：100% 通过，关键集成点全部覆盖
- PT：所有性能指标满足 NFR-PERF-01 ~ NFR-PERF-08
- FUZZ：24 小时无崩溃，无 ASAN/UBSAN 报错
- E2E：所有 AC-* 验收项全部通过

### 2.4 测试环境

| 项目 | 要求 |
|------|------|
| 操作系统 | openEuler Linux ARM64 |
| 编译器 | GCC 10+，支持 C++17 / C11 |
| 构建工具 | CMake 3.14+，Ninja（可选） |
| 测试框架 | GoogleTest 1.10+，MockCpp 3.0+ |
| 覆盖率工具 | gcov / lcov |
| 内存检测 | AddressSanitizer (ASAN)，UndefinedBehaviorSanitizer (UBSAN) |
| 硬件 | 真实 SSU 设备（E2E）、Mock 设备（UT/IT） |
| Mock daemon 响应 | CLI UT/IT 通过 `mock_ssu_*_invoke_call` 桩函数模拟 daemon 响应 |

---

## 3. 测试框架与规范

### 3.1 框架选型

| 工具 | 用途 |
|------|------|
| GoogleTest | 提供 `TEST_F`、`TEST`、断言 `EXPECT_*` / `ASSERT_*`、测试套件管理 |
| MockCpp | 提供 `MOCKER_CPP_VIRTUAL`、`MOCKER_CPP`、`stubs().will(...)`、`with(...)`、`GlobalMockObject::verify()` |
| CMake | 通过 `add_ut` 宏自动注册测试目标与依赖 |
| build.sh | 封装 `bash build.sh ut` / `bash build.sh ut -C` 命令 |

### 3.2 测试代码组织

```
test/UT/
├── controllers/ssu/                       # 控制器层 UT
│   ├── CMakeLists.txt                     # 链接 ssu_plugin、ubse_ssu、ubse_mti
│   ├── test_ubse_ssu_scheduler.{h,cpp}    # 调度器测试
│   └── test_ubse_ssu_direct_to_vm_manager.{h,cpp}  # VFE 直通测试
├── adapter_plugins/ssu/                   # 适配器层 UT
│   ├── CMakeLists.txt                     # 通过 add_ut(ubse_ssu) 注册
│   └── test_ubse_ssu_adapter_impl.{h,cpp} # 适配器实现测试
└── cli/
    ├── CMakeLists.txt
    ├── test_ubse_cli_ssu_cmd_reg.{h,cpp}      # SSU 命令注册与处理 UT
    ├── test_ubse_cli_ssu_struct.cpp           # SSU wire 契约 UT（无 Fixture）
    ├── test_ubse_cli_ssu_mock_invoke.{h,cpp}  # SSU 共享 mock 桩与捕获器
    └── ...                                    # 其他 CLI 模块测试
```

`test/UT/cli/` 镜像 `src/cli/` 层级（`src/cli/ssu/` → `test/UT/cli/test_ubse_cli_ssu_*.{h,cpp}`），符合目录规范文档约束。

### 3.3 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| TestSuite | `Test<模块名><子模块>`；CLI 侧为 `TestUbseCliSsu<子模块>` | `TestUbseSsuScheduler`、`TestUbseCliSsuCmdReg` |
| TestCase | `<方法>_<场景>_<期望>`；CLI 侧为 `<命令>_<场景>_<期望>` | `PreCheckAllocSizeZero`、`FeDeviceAlloc_Success`、`CreateRejectsMissingName` |
| 文件 | `test_<被测文件名>.{h,cpp}` | `test_ubse_ssu_scheduler.cpp` |
| 命名空间 | `ubse::<模块>::ut`；CLI 侧为 `ubse::ut::cli` | `ubse::ssu::scheduler::ut` |
| Mock 方法 | 保持与被测签名一致 | `MOCKER_CPP_VIRTUAL(obj, &Class::Method)` |
| Mock 桩 | `mock_ssu_<命令>_<场景>_invoke_call` | `mock_ssu_alloc_summary_invoke_call_normal` |

### 3.4 用例注释规范

每个 `TEST_F` 上方必须包含三段注释：

```cpp
/*
 * 用例描述：
 * <一句话描述被测行为>
 * 测试步骤：
 * 1、<步骤1>
 * 2、<步骤2>
 * 预期结果：
 * 1、<断言1>
 * 2、<断言2>
 */
TEST_F(TestUbseSsuScheduler, PreCheckAllocSizeZero) { ... }
```

### 3.5 SetUp / TearDown 规范

- `SetUp()`：构造 Mock 对象、初始化被测单例状态、注入 stub。
- `TearDown()`：调用 `GlobalMockObject::verify()` 校验所有 Mock 期望、恢复单例状态。
- 跨用例共享的 Mock 通过 `static` 成员或 helper 工厂方法构造，避免状态污染。
- CLI 用例 SetUp 必须调用 `ResetSsuMockCapture()` 清空 `g_ssuMockLast*` 全局捕获，避免用例间状态串扰。
- CLI 用例 TearDown 必须先 `MOCKER(&ubse_invoke_call).reset()` 复位桩（否则后续用例 mock 失效），再 `GlobalMockObject::verify()` 校验。

### 3.6 测试数据构造

通过 helper 工厂方法构造测试数据，保证可读性与可维护性：

```cpp
// 示例：构造在线 SSU 设备
static UbseSsuDevInfo MakeDev(const std::string &eid, uint64_t totalBytes,
                              uint64_t usedBytes, uint32_t nsCount = 0,
                              UbseSsuState state = UbseSsuState::ONLINE);

// 示例：构造带租户的设备
static UbseSsuDevInfo MakeDevWithTenant(const std::string &eid, uint64_t totalBytes,
                                        uint64_t usedBytes, const std::string &tenant,
                                        uint32_t nsCount = 1,
                                        UbseSsuState state = UbseSsuState::ONLINE);

// CLI 侧（匿名命名空间，位于 test_ubse_cli_ssu_cmd_reg.cpp / test_ubse_cli_ssu_struct.cpp）
std::map<std::string, std::string> DetailParams(const std::string &name = "alloc-space-1");  // alloc_detail 入参
std::map<std::string, std::string> CreateParams();                                           // create ssu 最小合法入参
UbseCliSsuAllocResult BuildSsuAllocResult();                                                 // 含 2 个 namespace
UbseCliSsuAllocResult MakeAllocation(const std::string &name, UbseSsuAllocStrategy strategy,
                                     uint32_t namespaceId);                                  // 带 N 个 namespace
```

---

## 4. 测试场景设计

### 4.1 功能场景覆盖矩阵

| 需求编号 | 测试模块 | TestSuite | 用例数 | 覆盖率目标 |
|----------|----------|-----------|--------|-----------|
| FR-ALLOC | 调度器 | TestUbseSsuScheduler | 24 | ≥ 90% |
| FR-FREE | 服务实现 | TestUbseSsuServiceImpFree | 待补 | ≥ 80% |
| FR-ATTACH | 服务实现 | TestUbseSsuServiceImpAttach | 待补 | ≥ 80% |
| FR-DETACH | 服务实现 | TestUbseSsuServiceImpDetach | 待补 | ≥ 80% |
| FR-PERM | 服务实现 | TestUbseSsuServiceImpPerm | 待补 | ≥ 80% |
| FR-FE-DEVICE | 直通管理 | TestUbseSsuDirectToVmManager | 45 | ≥ 90% |
| FR-DISTRIBUTED | RPC 处理器 | TestUbseSsuRpcProcessor | 待补 | ≥ 80% |
| FR-COLLECTOR | 采集器 | TestUbseSsuCollector | 待补 | ≥ 80% |
| FR-SCHEDULER | 调度器 | TestUbseSsuScheduler | 24 | ≥ 90% |
| FR-NORTHBOUND | HTTP 处理器 | TestUbseSsuHttpHandler | 待补 | ≥ 80% |
| FR-QUERY | 服务实现 | TestUbseSsuServiceImpQuery | 待补 | ≥ 80% |
| 适配器 | 适配器实现 | TestUbseSsuAdapterImpl | 72 | ≥ 90% |
| FR-CLI-STARTUP | main 入口 | —（IT 优先，见 5.11） | 待补 6 | ≥ 60% |
| FR-CLI-CMD-REG | CLI 命令注册 | TestUbseCliSsuCmdReg | 1 | ≥ 90% |
| FR-CLI-DISPLAY | display 命令 | TestUbseCliSsuCmdReg | 16 | ≥ 90% |
| FR-CLI-CREATE | create 命令 | TestUbseCliSsuCmdReg | 23 | ≥ 90% |
| FR-CLI-ATTACH | attach 命令 | TestUbseCliSsuCmdReg（待新增用例） | 待补 12 | ≥ 85% |
| FR-CLI-DETACH | detach 命令 | TestUbseCliSsuCmdReg（待新增用例） | 待补 9 | ≥ 85% |
| FR-CLI-VALIDATE | CLI 校验函数 | TestUbseCliSsuValidate（待新增） | 待补 14 | ≥ 95% |
| FR-CLI-IPC | CLI IPC 模板 | TestUbseCliSsuIpcTemplate（待新增） | 待补 7 | ≥ 80% |
| FR-CLI-WIRE | wire 契约 | TestUbseCliSsuStruct | 8，待补 4 | ≥ 90% |
| FR-CLI-OUTPUT | 输出格式化 | TestUbseCliSsuCmdReg（间接覆盖） | 间接 | ≥ 85% |
| FR-CLI-AUTH | 隐式认证 | IT / E2E（见 6.2.6、9.3.8） | 待补 3 | — |
| FR-CLI-COVERAGE | 命令覆盖 | E2E（见 9.3.9） | 待补 1 | — |
| NFR-PERF-09/11 | CLI 单命令响应时间 | TestUbseCliSsuPerf（待新增） | 待补 3 | — |
| NFR-MAINT-09 | CLI UT 覆盖 | 全部 CLI TestSuite | 50，待补约 53 | ≥ 85% |

### 4.2 测试用例设计方法

| 方法 | 适用场景 | 示例 |
|------|----------|------|
| 等价类划分 | 参数取值域划分 | `allocSize=0`（无效）vs `allocSize>0`（有效） |
| 边界值分析 | 边界条件 | `nsCount=127/128/129`、`name 长度=48` |
| 决策表 | 多条件组合 | STRIPED × allocSize 整除性 × chunkSize 整除性；CLI attach 命令 `type × devName × level × chunkSize` 合法/非法组合 |
| 状态迁移 | 状态机 | Namespace：IDLE→CREATING→CREATED→ATTACHING→ATTACHED |
| 错误猜测 | 异常路径 | RPC 超时、MTI 调用失败、collector 返回空列表；daemon 返回非 0 错误码、响应体损坏、超长列表 |
| 因果图 | 因果关系 | 预扣容量 × collector 刷新 × 容量可见性；CLI `-t Linear` 指定 → 必须有 `-d`，禁止 `--level/--chunk_size` |
| 防御性测试 | 不可信输入 | 响应列表长度超过 `SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS`、枚举值越界 |
| 往返一致 | 序列化对等 | CLI `Serialize` 的字节流与服务端 `Ssu*Unpack` 期望字段顺序一致 |

---

## 5. 单元测试设计

### 5.1 调度器测试（TestUbseSsuScheduler）

#### 5.1.1 测试目标

验证 `UbseSsuScheduler`、`PreCheckHandler`、`UbseSsuTenantIsolationFilter`、`UbseSsuAllocateAlgorithmHandler` 的功能正确性，覆盖需求 FR-ALLOC、FR-SCHEDULER。

#### 5.1.2 测试场景

**A. PreCheckHandler（预检查过滤器）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| PreCheckAllocSizeZero | allocSize=0 | 返回 false，ret=INVALID_PARAM |
| PreCheckNsNumZero | nsNum=0 | 返回 false，ret=INVALID_PARAM |
| PreCheckEmptyDevices | 设备列表为空 | 返回 false，ret=INSUFFICIENT_SPACE |
| PreCheckAllOfflineDevices | 全部设备 OFFLINE | 返回 false，ret=INSUFFICIENT_SPACE |
| PreCheckFilterMaxNsDevices | 设备 nsCount=128 被过滤 | 返回 false，ret=INSUFFICIENT_SPACE |
| PreCheckInsufficientFilteredDevices | 过滤后设备数 < nsNum | 返回 false，ret=INSUFFICIENT_SPACE |
| PreCheckSuccessSortByFreeAndNsCount | 正常过滤并排序 | 返回 true，按 freeBytes 降序、nsCount 升序 |
| PreCheckStripedAllocSizeNotDivisible | STRIPED allocSize 不可整除 nsNum | 返回 false，ret=INVALID_PARAM |

**B. UbseSsuAllocateAlgorithmHandler（分配算法处理器）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| AlgorithmLinearLbaSizeZero | lbaSize=0 | 返回 false，ret=INVALID_PARAM |
| AlgorithmLinearInsufficientSpace | LINEAR 总可用空间不足 | 返回 false，ret=INSUFFICIENT_SPACE |
| AlgorithmLinearSuccess | LINEAR 正常分配 | 返回 true，总和=allocSize，每个对齐 lbaSize |
| AlgorithmLinearAlignUpToSector | LINEAR 非整除时向上对齐 | 返回 true，总和=对齐后大小 |
| AlgorithmStripedSuccess | STRIPED 正常分配 | 返回 true，每个 eid 分配 allocSize/nsNum |
| AlgorithmStripedInsufficientSpace | STRIPED 单设备空间不足 | 返回 false，ret=INSUFFICIENT_SPACE |
| AlgorithmStripedEvenlyDivisible | STRIPED 均分 | 返回 true，每个 eid 分配 singleNsSize |
| AlgorithmSelectedDevsFilteredAfterSuccess | 成功后 selectedDevs 仅保留选中 | selectedDevs.size()==nsNum |

**C. UbseSsuTenantIsolationFilter（租户隔离过滤器）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| TenantIsolationEmptyReqTenantFiltersTenantDevices | req.tenant 为空，所有设备带租户 | 返回 false，selectedDevs 为空 |
| TenantIsolationEmptyReqTenantKeepsNonTenantDevices | req.tenant 为空，部分设备无租户 | 返回 true，保留无租户设备 |
| TenantIsolationFiltersMismatchedDevices | req.tenant=A，过滤 tenant=B 设备 | 返回 true，仅保留 tenant=A |
| TenantIsolationInsufficientMatchedDevices | 匹配设备数 < nsNum | 返回 false，ret=INSUFFICIENT_SPACE |
| TenantIsolationKeepsEmptyTenantDevices | 空 tenant 设备可被任意租户使用 | 返回 true，保留 tenant=A 和空 tenant |

**D. UbseSsuScheduler（调度器集成）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| SchedulerExecuteSuccess | LINEAR 集成分配 | ret=OK，eidNsSizeList.size()==nsNum |
| SchedulerExecuteParamInvalid | 非法参数 | ret=INVALID_PARAM，selectedDevs 为空 |
| SchedulerExecuteStripedSuccess | STRIPED 集成分配 | ret=OK，eidNsSizeList 数量为 nsNum |
| SchedulerAddCustomFilter | 添加自定义 Filter 到 POST_CHECK | ret=OK，msg 包含 "CUSTOM" |
| SchedulerAddNullFilterIgnored | 传入 nullptr | ret=OK，调度器不崩溃 |
| SchedulerTenantIsolationIntegration | 集成租户隔离策略 | ret=OK，分配结果均属 tenant-A |

#### 5.1.3 测试夹具

```cpp
class TestUbseSsuScheduler : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;  // 调用 GlobalMockObject::verify()
protected:
    static UbseSsuDevInfo MakeDev(const std::string &eid, uint64_t totalBytes,
                                  uint64_t usedBytes, uint32_t nsCount = 0,
                                  UbseSsuState state = UbseSsuState::ONLINE);
    static UbseSsuDevInfo MakeDevWithTenant(...);
};
```

#### 5.1.4 Mock 依赖

调度器为纯算法模块，无外部依赖需 Mock，直接构造 `UbseSsuAllocationContext` 输入即可。

---

### 5.2 VFE 直通管理测试（TestUbseSsuDirectToVmManager）

#### 5.2.1 测试目标

验证 `UbseSsuDirectToVmManager` 的状态机、PFE/VFE 查询、VM bus instance 生命周期、FeDeviceAlloc/Free 流程、空实例定时清理，覆盖需求 FR-FE-DEVICE、NFR-REL-07。

#### 5.2.2 测试场景

**A. 状态管理**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| GetState_DefaultAvailable | 默认状态 | AVAILABLE |
| SetState_RunningAlloc | 设置 RUNNING_ALLOC | 状态正确切换 |
| GuidToStr_ValidGuid | 有效 GUID | 32 字符 hex 串 |
| GuidToStr_AllZeros | 全 0 GUID | 32 个 '0' |

**B. PFE/VFE 查询与转换**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| QueryPfeList_Success | GetIdevFeList 返回 OK | 返回 UBSE_OK |
| QueryPfeList_Failed | GetIdevFeList 返回 ERROR | 返回 UBSE_ERROR |
| ConvertVfe_FieldsMapped | VFE 字段映射 | slotId/chipId/dieId/pfeId/vfeId 正确 |
| ConvertFe_FieldsMapped | PFE 字段映射 | 字段映射正确，vfeList.size()==1 |
| GetFeDeviceList_Success | 成功获取 FE 列表 | 返回 UBSE_OK，feList.size()==1 |
| GetFeDeviceList_QueryFailed | 查询失败 | 返回 UBSE_ERROR |
| GetFeDeviceList_FillBusInstanceGuidFailed | bus instance 查询失败 | 返回 UBSE_ERROR |

**C. bus instance 与 VFE 管理**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| FindVmBusInst_Found | 找到 VM bus instance | 返回 OK |
| FindVmBusInst_NotFound | 未找到 | 返回错误 |
| FindVmBusInst_GetListFailed | GetBusInstanceList 失败 | 返回错误 |
| FindVfeInPfeList_Found | 找到 VFE | 返回 OK |
| FindVfeInPfeList_NotFound | 未找到 | 返回错误 |
| CheckVfeOccupied_NotOccupied | VFE 未被占用 | 返回 false |
| CheckVfeOccupied_Occupied | VFE 已被占用 | 返回 true |
| HasOtherVfe_NoOther | 无其他 VFE | 返回 false |
| HasOtherVfe_HasOther | 有其他 VFE | 返回 true |
| CreateVmBusInst_Success | 创建成功 | 返回 UBSE_OK |
| CreateVmBusInst_Failed | 创建失败 | 返回 UBSE_ERROR |
| DestroyVmBusInst_Success | 销毁成功 | 返回 UBSE_OK |
| DestroyVmBusInst_Failed | 销毁失败 | 返回 UBSE_ERROR |
| RegVfeToVmBusInst_Success | 注册成功 | 返回 UBSE_OK |
| RegVfeToVmBusInst_ResListFalse | 注册结果 false | 返回错误 |
| UnRegVfeFromVmBusInst_Success | 反注册成功 | 返回 UBSE_OK |
| UnRegVfeFromVmBusInst_Failed | 反注册失败 | 返回错误 |

**D. FeDeviceAlloc / FeDeviceFree 流程**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| FeDeviceAlloc_InvalidUpi | upi=0 | 返回错误 |
| FeDeviceAlloc_PreparationQueryFailed | 准备阶段查询失败 | 返回错误 |
| FeDeviceAlloc_VfeNotFound | VFE 未找到 | 返回错误 |
| FeDeviceAlloc_AllocExecutionFailed | 执行阶段失败 | 状态恢复 AVAILABLE |
| FeDeviceAlloc_Success | 全流程成功 | 返回 UBSE_OK，状态恢复 AVAILABLE |
| FeDeviceFree_InvalidUpi | upi=0 | 返回错误 |
| FeDeviceFree_BusInstNotFound | bus instance 未找到 | 返回错误 |
| FeDeviceFree_upiMismatch | bus instance.upi ≠ req.upi | 返回错误（NFR-SEC-04） |
| FeDeviceFree_PreparationQueryFailed | 准备阶段失败 | 返回错误 |
| FeDeviceFree_Success_WithDestroy | 成功并销毁空 bus instance | 返回 UBSE_OK |
| FeDeviceFree_Success_SkipDestroy | 成功但保留 bus instance（仍有其他 VFE） | 返回 UBSE_OK |
| FeDeviceFree_VfeNotFound | VFE 未找到 | 返回错误 |

**E. 空 bus instance 定时清理**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ClearEmptyVMBusInstance_NoVmBusInst | 无 bus instance | UBSE_OK，不报错 |
| ClearEmptyVMBusInstance_WithEmpty | 存在空 bus instance | 销毁空实例 |
| ClearEmptyVMBusInstance_GetListFailed | 获取列表失败 | UBSE_OK，不崩溃 |
| StartClearTimer_Success | 启动定时器 | UBSE_OK |
| StopClearTimer_NotStarted | 未启动时停止 | 不崩溃 |

#### 5.2.3 测试夹具

```cpp
class TestUbseSsuDirectToVmManager : public testing::Test {
public:
    void SetUp() override {
        MOCKER_CPP(&nanosleep).stubs().will(returnValue(0));  // 跳过 sleep
        mgr.state_ = SsuDirectToVmManagerState::AVAILABLE;
    }
    void TearDown() override {
        mgr.state_ = SsuDirectToVmManagerState::AVAILABLE;  // 恢复状态
        GlobalMockObject::verify();
    }
    UbseMtiIdevPfe MakePfe(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint8_t pfeId);
    UbseMtiIdevVfe MakeVfe(uint8_t slotId, uint8_t chipId, uint8_t dieId,
                           uint8_t pfeId, uint8_t vfeId);
    UbseMtiBusInst MakeVmBusInst(uint16_t upi, const UbseMtiGuid &guid, ...);
    UbseMtiGuid MakeGuid(uint8_t val);
};
```

#### 5.2.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `UbseMtiUrma::GetInstance()` | `GetIdevFeList` | 模拟 PFE 列表查询 |
| `UbseMtiBusInstance::GetInstance()` | `GetBusInstanceList` | 模拟 bus instance 查询 |
| `UbseMtiBusInstance::GetInstance()` | `CreateBusInstance` | 模拟创建 |
| `UbseMtiBusInstance::GetInstance()` | `DestroyBusInstance` | 模拟销毁 |
| `UbseMtiBusInstance::GetInstance()` | `RegisterVfe` / `UnRegisterVfe` | 模拟 VFE 注册 |
| `nanosleep` | C 函数 | 跳过重试 sleep，加速测试 |

---

### 5.3 适配器实现测试（TestUbseSsuAdapterImpl）

#### 5.3.1 测试目标

验证 `UbseSsuAdapterImpl` 的设备地址构造、namespace 信息构造、设备信息转换、路径校验、块设备创建/删除，覆盖需求 FR-ALLOC、FR-ATTACH、FR-DETACH 中的适配器层逻辑。

#### 5.3.2 测试场景

**A. BuildDevAddrList（设备地址构造）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| BuildDevAddrList_EmptyList | 空列表 | UBSE_OK，devList.size()==0 |
| BuildDevAddrList_SingleValidDev | 单个有效设备 | UBSE_OK，eid/nqn 正确 |
| BuildDevAddrList_MultipleValidDevs | 多个有效设备 | UBSE_OK，devList.size()==2 |
| BuildDevAddrList_InvalidEidLength | eid 长度不足 | 返回错误 |
| BuildDevAddrList_EmptySubNqn | subNqn 为空 | 返回错误 |
| BuildDevAddrList_EidTooLong | eid 超长 | 返回错误 |
| BuildDevAddrList_SubNqnAtMaxSize | subNqn 恰好达上限 | UBSE_OK |
| BuildDevAddrList_SrcEidZeroed | srcEid 全 0 | UBSE_OK |

**B. BuildNamespaceInfoForCreate（创建 namespace 信息）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| BuildNamespaceInfoForCreate_ValidInput | 有效输入 | 字段全部正确映射 |
| BuildNamespaceInfoForCreate_InvalidEidLength | eid 长度不足 | 返回错误 |
| BuildNamespaceInfoForCreate_EmptySubNqn | subNqn 为空 | 返回错误 |
| BuildNamespaceInfoForCreate_ZeroNsze | nsze=0 | 返回错误 |
| BuildNamespaceInfoForCreate_ZeroNcap | ncap=0 | 返回错误 |
| BuildNamespaceInfoForCreate_EidTooLong | eid 超长 | 返回错误 |
| BuildNamespaceInfoForCreate_JettyIdSet | jettyId 设置 | jettyId 正确 |
| BuildNamespaceInfoForCreate_SrcEidZeroed | srcEid 全 0 | UBSE_OK |
| BuildNamespaceInfoForCreate_NmicFalse | nmic=false | nmic 字段为 false |
| BuildNamespaceInfoForCreate_CustomDataCopied | customData 拷贝 | 字段正确拷贝 |
| BuildNamespaceInfoForCreate_SubNqnAtMaxSize | subNqn 上限 | UBSE_OK |
| BuildNamespaceInfoForCreate_NmicNonZero | nmic 非 0 | nmic 字段为 true |

**C. BuildNamespaceInfoForBasic（基础 namespace 信息）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| BuildNamespaceInfoForBasic_ValidInput | 有效输入 | 字段映射正确 |
| BuildNamespaceInfoForBasic_InvalidEidLength | eid 长度不足 | 返回错误 |
| BuildNamespaceInfoForBasic_ZeroNamespaceId | namespaceId=0 | 返回错误 |
| BuildNamespaceInfoForBasic_EidTooLong | eid 超长 | 返回错误 |
| BuildNamespaceInfoForBasic_WithGuid | 带 guid | guid 正确 |
| BuildNamespaceInfoForBasic_EmptyGuid | guid 为空 | UBSE_OK |
| BuildNamespaceInfoForBasic_JettyIdSet | jettyId 设置 | jettyId 正确 |
| BuildNamespaceInfoForBasic_SrcEidZeroed | srcEid 全 0 | UBSE_OK |
| BuildNamespaceInfoForBasic_GuidShorterThanMaxSize | guid 短于上限 | 正确拷贝 |
| BuildNamespaceInfoForBasic_GuidLongerThanMaxSize | guid 超长 | 截断处理 |
| BuildNamespaceInfoForBasic_EmptySubNqn | subNqn 为空 | 返回错误 |
| BuildNamespaceInfoForBasic_SubNqnAtMaxSize | subNqn 上限 | UBSE_OK |

**D. ConvertDevInfo（设备信息转换）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ConvertDevInfo_OnlineDevice | ONLINE 设备 | state=ONLINE |
| ConvertDevInfo_OfflineDevice | OFFLINE 设备 | state=OFFLINE |
| ConvertDevInfo_DiscoverError | 发现错误 | state=ERROR |
| ConvertDevInfo_IdentifyError | 标识错误 | state=ERROR |
| ConvertDevInfo_WithNamespaces | 含 namespace | namespace 列表正确 |
| ConvertDevInfo_ZeroNamespaces | 无 namespace | namespace 列表为空 |
| ConvertDevInfo_NamespaceGuidUuid | namespace guid/uuid | UUID 正确生成 |
| ConvertDevInfo_EmptyEidInRaw | raw eid 为空 | 返回错误 |
| ConvertDevInfo_LargeCapacity | 大容量设备 | 容量正确转换 |
| ConvertDevInfo_NamespaceUserDataCopied | customData 拷贝 | 字段正确拷贝 |

**E. ValidatePersistentPaths（持久化路径校验）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ValidatePersistentPaths_ValidByPaths | 有效 by-id 路径 | 全部通过 |
| ValidatePersistentPaths_SingleValidPath | 单个有效路径 | 通过 |
| ValidatePersistentPaths_EmptyList | 空列表 | UBSE_OK |
| ValidatePersistentPaths_InvalidPathDevNvme | /dev/nvme0 路径 | 返回错误 |
| ValidatePersistentPaths_MixedValidInvalid | 混合路径 | 返回错误 |
| ValidatePersistentPaths_ByPathInsteadOfById | by-path 路径 | 返回错误 |
| ValidatePersistentPaths_RelativePath | 相对路径 | 返回错误 |
| ValidatePersistentPaths_EmptyString | 空字符串 | 返回错误 |
| ValidatePersistentPaths_OnlyByIdPrefix | 仅 by-id 前缀 | 返回错误 |

**F. Attach/Detach/Permission 操作**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| AttachDevNameSpace_EmptyHostNqn | hostNqn 为空 | 返回错误 |
| AttachDevNameSpace_InvalidEid | eid 非法 | 返回错误 |
| DetachDevNameSpace_EmptyHostNqn | hostNqn 为空 | 返回错误 |
| DetachDevNameSpace_InvalidEid | eid 非法 | 返回错误 |
| AddNameSpaceAllowHost_EmptyHostNqn | hostNqn 为空 | 返回错误 |
| AddNameSpaceAllowHost_InvalidEid | eid 非法 | 返回错误 |
| RemoveNameSpaceAllowHost_EmptyHostNqn | hostNqn 为空 | 返回错误 |
| RemoveNameSpaceAllowHost_InvalidEid | eid 非法 | 返回错误 |
| GetNameSpaceAllowHostList_InvalidEid | eid 非法 | 返回错误 |

**G. CreateBlockDevice / DeleteBlockDevice（聚合块设备）**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| CreateBlockDevice_InvalidPathNotById | 非 by-id 路径 | 返回错误 |
| CreateBlockDevice_LinearModeInvalidPath | LINEAR 模式非法路径 | 返回错误 |
| CreateBlockDevice_EmptyPathList | 空路径列表 | 返回错误 |
| CreateBlockDevice_StripedModeInvalidPath | STRIPED 模式非法路径 | 返回错误 |
| CreateBlockDevice_Raid5InvalidPath | RAID5 非法路径 | 返回错误 |
| DeleteBlockDevice_NonExistentDevice | 不存在设备 | 返回错误 |

**H. VerifyNamespaceGuid / 其他**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| VerifyNamespaceGuid_EmptyEid | eid 为空 | 返回错误 |
| VerifyNamespaceGuid_EmptyGuid | guid 为空 | 返回错误 |
| VerifyNamespaceGuid_BothEidAndGuidEmpty | eid 和 guid 均空 | 返回错误 |
| DlOpenLib_FailWhenLibNotFound | libssu.so 不存在 | 返回错误 |
| GetDevList_EmptyInputList | 输入列表为空 | UBSE_OK |
| GetSrcEid_Success | 获取 srcEid | UBSE_OK |

---

### 5.4 服务实现测试（TestUbseSsuServiceImp）—— 待补

#### 5.4.1 测试目标

验证 `UbseSsuServiceImp` 的 AllocSpace/FreeSpace/AttachSpace/DetachSpace/AddAccessPermission/RemoveAccessPermission/ListAllocInfo/GetAllocInfoByName/GetNsStats/GetConnectInfo 等接口，覆盖需求 FR-ALLOC ~ FR-QUERY。

#### 5.4.2 关键测试场景

| 用例分组 | 用例名 | 场景 | 预期结果 |
|----------|--------|------|----------|
| AllocSpace | Alloc_MasterSuccess | master 本地分配成功 | UBSE_OK，账本 Put CREATED |
| Alloc_AgentRpcSuccess | agent RPC 转发成功 | UBSE_OK |
| Alloc_IdempotentCreated | 重复分配已存在条目 | UBSE_OK（幂等） |
| Alloc_ConcurrentCreating | 并发分配 CREATING 状态 | UBSE_ERR_EXISTED |
| Alloc_RollbackOnCreateFail | namespace 创建失败 | 回滚已创建 namespace |
| Alloc_PreDeductCapacity | 预扣容量 | reservedBytes 增加 |
| Alloc_RpcTimeout | RPC 超时 30s | UBSE_ERR_TIMED_OUT |
| FreeSpace | Free_Success | 释放成功 | 账本移除 |
| Free_IdempotentNotExist | 释放不存在 | UBSE_OK（幂等） |
| Free_PartialFail | 部分 namespace 删除失败 | 账本保留剩余 |
| AttachSpace | Attach_VerifyIdentityPass | 身份验证通过 | 返回 nsDevPaths |
| Attach_VerifyIdentityFail | 身份验证失败 | 返回错误 |
| Attach_StateMachineCreatedToAttaching | 状态转换 | 账本 ATTACHING |
| Attach_StateMachineAttachingToAttached | 状态转换 | 账本 ATTACHED |
| DetachSpace | Detach_Success | 卸载成功 | UBSE_OK |
| Detach_NotAttached | 未挂载时卸载 | UBSE_OK（幂等） |
| Permission | AddPerm_Idempotent | 重复添加权限 | UBSE_OK |
| RemovePerm_Idempotent | 重复移除权限 | UBSE_OK |
| Query | ListAllocInfo_Empty | 无分配 | 返回空列表 |
| ListAllocInfo_WithAlloc | 有分配 | 返回列表 |
| GetNsStats_Success | 查询统计 | 返回 statsList |
| GetConnectInfo_Success | 查询连接信息 | 返回 connectInfoList |

#### 5.4.3 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `UbseSsuAdapterInterface::GetInstance()` | `CreateDevNameSpace` / `DeleteDevNameSpace` / `AttachDevNameSpace` / `DetachDevNameSpace` | 模拟硬件操作 |
| `UbseSsuCollector` | `GetDevListWithReservations` / `AddReserveSpace` / `OnReserveBegin` / `OnReserveEnd` | 模拟采集与预留 |
| `UbseSsuDebtLedger::GetInstance()` | `Get` / `Put` / `Modify` / `Remove` | 模拟账本操作 |
| `UbseGetRole` | C 函数 | 模拟节点角色 |
| `UbseRpcEndpointFactory::GetRpcEndpoint` | 静态方法 | 模拟 RPC 端点 |
| `UbseFutureMgr::CreateInstance` | 静态方法 | 模拟 Future 管理 |

---

### 5.5 债务账本测试（TestUbseSsuDebtLedger）—— 待补

#### 5.5.1 测试目标

验证 `UbseSsuDebtLedger` 的 COW（Copy-On-Write）写入、并发读、Rebuild 重建、状态机推进，覆盖需求 NFR-REL-01、FR-DISTRIBUTED-04。

#### 5.5.2 关键测试场景

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Put_NewEntry | 写入新条目 | 后续 Get 返回该条目 |
| Put_ReplaceExisting | 替换已有条目 | COW 写入，旧 shared_ptr 不变 |
| Get_NotExist | 查询不存在 | 返回空 |
| Modify_StateTransition | 状态机转换 | state 正确更新 |
| Remove_Success | 移除条目 | 后续 Get 返回空 |
| Rebuild_FromDevList | 从设备列表重建 | 账本条目与 namespace 对应 |
| ConcurrentReadWrite | 并发读写 | shared_lock 不阻塞读，写不阻塞读 |

---

### 5.6 预留管理测试（TestUbseSsuReservationMgr）—— 待补

#### 5.6.1 测试目标

验证 `ReservationMgr` 的预扣除/预释放账本、collector 刷新后 Clear、GetPendingAdjustment 计算。

#### 5.6.2 关键测试场景

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| AddReserveSpace_Success | 预扣 | reservedBytes[eid] 增加 |
| ReleaseReservation_Success | 预释放 | releasedBytes[eid] 增加 |
| GetPendingAdjustment_Positive | 预扣 > 预释放 | adjustment > 0 |
| GetPendingAdjustment_Negative | 预释放 > 预扣 | adjustment < 0 |
| Clear_OnCollectorRefresh | collector 刷新后 Clear | reservedBytes/releasedBytes 清零 |

---

### 5.7 采集器测试（TestUbseSsuCollector）—— 待补

#### 5.7.1 测试目标

验证 `UbseSsuCollector` 的周期采集、空列表保护、缓存维护、OnReserveBegin/End 引用计数。

#### 5.7.2 关键测试场景

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Start_Success | master 启动 | UBSE_OK |
| Start_AgentSkip | agent 启动 | 跳过，UBSE_OK |
| CollectDevList_Success | 采集成功 | 缓存更新 |
| CollectDevList_EmptyKeepOldCache | 空列表保护 | 保持旧缓存（NFR-REL-08） |
| OnReserveBegin_IncPendingOps | 开始预扣 | pendingOps_++ |
| OnReserveEnd_DecPendingOps | 结束预扣 | pendingOps_-- |
| GetDevListWithReservations_ApplyAdjustment | 应用预留调整 | 返回调整后列表 |

---

### 5.8 CLI 命令注册与处理测试（TestUbseCliSsuCmdReg）

#### 5.8.1 测试目标

验证 `UbseCliRegSsuModule` 的命令注册、display/create/attach/detach 4 类命令处理函数、参数校验、IPC 调用、输出格式化，覆盖需求 FR-CLI-CMD-REG、FR-CLI-DISPLAY、FR-CLI-CREATE、FR-CLI-ATTACH、FR-CLI-DETACH、FR-CLI-VALIDATE、FR-CLI-OUTPUT。

#### 5.8.2 测试场景

**A. 命令注册（FR-CLI-CMD-REG）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| SignUpRegistersDisplayAndCreateSsuCommands | 调用 `UbseCliSignUp()` | 框架登记 display/create/attach/detach 4 条命令 | 已实现 |

**B. display ssu 概览查询（FR-CLI-DISPLAY-01/04）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| DisplaySummaryRejectsMissingType | 缺失 `-t` | 返回 ERR_TYPE_REQUIRED 固定文案 | 已实现 |
| DisplaySummaryRejectsUnsupportedType | `-t unknown` | 返回 ERR_UNSUPPORTED_TYPE 固定文案 | 已实现 |
| DisplaySummaryReturnsInfoWhenEmpty | daemon 返回空列表 | 输出 INFO_EMPTY 提示 | 已实现 |
| DisplaySummaryPrintsNameSizeStrategy | daemon 返回多条分配 | 输出 3 列表格，size 聚合为 `NG` | 已实现 |
| DisplaySummaryNeverEmitsMOrKOrBareBytes | sub-GiB 容量场景 | 输出仅 `NG`，不泄露 `M`/`K`/裸字节 | 已实现 |
| DisplaySummaryReturnsInternalErrorWithCode | daemon 返回非 0 状态码 | 输出 `ERROR: Internal error with error code N` | 已实现 |
| DisplaySummaryReturnsDeserializationErrorForBadResponse | 响应体损坏 | 输出 ERR_DESERIALIZATION | 已实现 |
| DisplaySummaryReturnsDeserializationErrorForEmptySuccessResponse | 状态码 0 但 body 为空 | 输出 ERR_DESERIALIZATION | 已实现 |
| DisplaySummaryRequestUsesEmptyBody | 概览请求 | `ubse_invoke_call` 入参 buffer 为 `{nullptr, 0}`，mock 捕获 `deserialized=true` | 已实现 |

**C. display ssu 详情查询（FR-CLI-DISPLAY-02/03/05/06）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| DisplayDetailRejectsMissingName | `-t alloc_detail` 缺 `-n` | 返回 ERR_NAME_REQUIRED | 已实现 |
| DisplayDetailRejectsInvalidName | `-n` 含非法字符 | 返回 ERR_INVALID_NAME | 已实现 |
| DisplayDetailSerializesNameAndUsesDetailOpcode | 合法 `-n` | 请求体携带 name，opCode=`UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ` | 已实现 |
| DisplayDetailPrintsDetailTable | daemon 返回详情 | 输出 7 列表格 + 表头合并行 | 已实现 |
| DisplayDetailReturnsInfoWhenNoNamespaces | daemon 返回空 namespace 列表 | 输出 INFO_EMPTY | 已实现 |
| DisplayDetailReturnsDeserializationError | 响应体损坏 | 输出 ERR_DESERIALIZATION | 已实现 |
| DisplayDetailNeverEmitsMOrKOrBareBytes | sub-GiB 容量场景 | 输出仅 `NG`，不泄露 `M`/`K`/裸字节 | 已实现 |
| DisplayDetailRequestCarriesName | 请求体格式 | 第一字段为长度前缀 + name（无 `\0` 终止符） | 已实现 |

**D. create ssu 命令（FR-CLI-CREATE-01~07）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| CreateRejectsMissingName | 缺 `-n` | 返回 ERR_NAME_REQUIRED | 已实现 |
| CreateRejectsMissingSize | 缺 `-s` | 返回 ERR_SIZE_REQUIRED | 已实现 |
| CreateRejectsInvalidNameCharacters | `-n` 含 `#` 等非法字符 | 返回 ERR_INVALID_NAME | 已实现 |
| CreateAcceptsNameAtFortyEightCharLimit | `-n` 长度恰好 48 | 通过校验，进入 IPC | 已实现 |
| CreateRejectsNameLongerThanFortyEight | `-n` 长度 49 | 返回 ERR_INVALID_NAME | 已实现 |
| CreateRejectsLowercaseOrMissingSizeSuffix | `-s 10g`/`-s 10` | 返回 ERR_INVALID_SIZE | 已实现 |
| CreateRejectsZeroSize | `-s 0G` | 返回 ERR_INVALID_SIZE | 已实现 |
| CreateRejectsSizeOverflowingUint64Bytes | `-s` 数值 × GiB 溢出 uint64 | 返回 ERR_INVALID_SIZE | 已实现 |
| CreateRejectsSizeBeyondStoullRange | `-s` 超出 stoull 范围 | 返回 ERR_INVALID_SIZE | 已实现 |
| CreateAcceptsMinimumOneGibSize | `-s 1G` | 通过校验，`nsSize=1GiB` | 已实现 |
| CreateRejectsInvalidNsNum | `-m 0` / `-m 129` | 返回 ERR_INVALID_NS_NUM | 已实现 |
| CreateRejectsNsNumBeyondStoullRange | `-m` 超出 stoull 范围 | 返回 ERR_INVALID_NS_NUM | 已实现 |
| CreateRejectsInvalidLba | `-l 1K` | 返回 ERR_INVALID_LBA | 已实现 |
| CreateAcceptsCanonicalLbaInput | `-l 512B`/`-l 4K` | 通过校验，`lbaFormat` 枚举正确 | 已实现 |
| CreateRejectsBareLba512Input | `-l 512`（无 B 后缀） | 返回 ERR_INVALID_LBA | 已实现 |
| CreateRejectsInvalidStrategy | `-r raid0` | 返回 ERR_INVALID_STRATEGY | 已实现 |
| CreateAcceptsCanonicalStrategyInput | `-r Linear`/`-r Striped` | 通过校验，`strategy` 枚举正确 | 已实现 |
| CreateRejectsLowercaseStrategyInput | `-r striped` | 返回 ERR_INVALID_STRATEGY | 已实现 |
| CreateIgnoresLegacyUsingTypeOption | 透传 `--using_type=shared` | 不触发校验错误，命令按默认路径成功 | 已实现 |
| CreateAppliesDefaults | 仅传必填项 | `nsNum=1`、`lbaFormat=512B`、`strategy=Linear`、`tenant=""` | 已实现 |
| CreateSerializesAllExplicitOptions | 显式传所有可选参数 | 各字段正确序列化到请求体 | 已实现 |
| CreatePrintsAllocatedDetailTable | 分配成功 | 回显详情表，布局与详情查询一致 | 已实现 |
| CreateReturnsDeserializationError | 响应体损坏 | 输出 ERR_DESERIALIZATION | 已实现 |
| CreateReturnsInternalErrorWithCode | daemon 返回非 0 状态码 | 输出 `ERROR: Internal error with error code N` | 已实现 |

**E. attach ssu 命令（FR-CLI-ATTACH-01~06）—— 待补**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| AttachRejectsMissingName | 缺 `-n` | 返回 ERR_NAME_REQUIRED | 待补 |
| AttachRejectsInvalidName | `-n` 含非法字符 | 返回 ERR_INVALID_NAME | 待补 |
| AttachRejectsOversizedHostNqn | `--host_nqn` 长度 > 68 | 返回 ERR_INVALID_HOST_NQN | 待补 |
| AttachRejectsOversizedSrcEid | `--src_eid` 长度 > 16 | 返回 ERR_INVALID_SRC_EID | 待补 |
| AttachSpaceRejectsAggregationOptions | 未指定 `-t` 但带 `--dev_name` | 返回 ERR_ATTACH_AGGREGATION_REQUIRES_TYPE | 待补 |
| AttachSpaceSuccess | `attach ssu -n test` | IPC opCode=`UBSE_SSU_ATTACH_SPACE_REQ`，输出 `ns_dev_paths:` | 待补 |
| AttachLinearRejectsMissingDevName | `-t Linear` 缺 `-d` | 返回 ERR_LINEAR_DEV_NAME_REQUIRED | 待补 |
| AttachLinearRejectsInvalidDevName | `-d` 含非法字符 | 返回 ERR_INVALID_DEV_NAME | 待补 |
| AttachLinearRejectsStripedOnlyOptions | `-t Linear` 同时带 `--level` | 返回 ERR_STRIPED_ONLY_OPTIONS | 待补 |
| AttachLinearSuccess | `attach ssu -t Linear -n test -d dev1` | IPC opCode=`UBSE_SSU_ATTACH_LINEAR_SPACE_REQ`，输出 ns_dev_paths + dev_path | 待补 |
| AttachStripedRejectsMissingLevel | `-t Striped` 缺 `-l` | 返回 ERR_STRIPED_LEVEL_REQUIRED | 待补 |
| AttachStripedRejectsMissingChunkSize | `-t Striped` 缺 `-c` | 返回 ERR_STRIPED_CHUNK_SIZE_REQUIRED | 待补 |
| AttachStripedRejectsInvalidLevel | `-l raid1` | 返回 ERR_INVALID_LEVEL | 待补 |
| AttachStripedRejectsInvalidChunkSize | `-c 8K` | 返回 ERR_INVALID_CHUNK_SIZE | 待补 |
| AttachStripedSuccess | `attach ssu -t Striped -n test -d dev1 -l raid0 -c 4K` | IPC opCode=`UBSE_SSU_ATTACH_STRIPED_SPACE_REQ` | 待补 |
| AttachStripedAllCanonicalChunkSizes | `-c 4K/16K/32K/64K/128K/256K/512K` | 全部通过校验，`chunkSize` 枚举正确 | 待补 |

**F. detach ssu 命令（FR-CLI-DETACH-01~06）—— 待补**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| DetachRejectsMissingName | 缺 `-n` | 返回 ERR_NAME_REQUIRED | 待补 |
| DetachRejectsInvalidName | `-n` 含非法字符 | 返回 ERR_INVALID_NAME | 待补 |
| DetachSpaceRejectsDevNameOption | 未指定 `-t` 但带 `--dev_name` | 返回 ERR_DETACH_DEV_NAME_REQUIRES_TYPE | 待补 |
| DetachSpaceSuccess | `detach ssu -n test` | IPC opCode=`UBSE_SSU_DETACH_SPACE_REQ`，无响应体输出 | 待补 |
| DetachLinearRejectsMissingDevName | `-t Linear` 缺 `-d` | 返回 ERR_LINEAR_DEV_NAME_REQUIRED | 待补 |
| DetachLinearSuccess | `detach ssu -t Linear -n test -d dev1` | IPC opCode=`UBSE_SSU_DETACH_LINEAR_SPACE_REQ` | 待补 |
| DetachStripedRejectsMissingDevName | `-t Striped` 缺 `-d` | 返回 ERR_STRIPED_DEV_NAME_REQUIRED | 待补 |
| DetachStripedRejectsInvalidType | `-t raid1` | 返回 ERR_INVALID_ATTACH_TYPE | 待补 |
| DetachStripedSuccess | `detach ssu -t Striped -n test -d dev1` | IPC opCode=`UBSE_SSU_DETACH_STRIPED_SPACE_REQ` | 待补 |

**G. 输出格式化（FR-CLI-OUTPUT-01~07）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| 摘要表布局校验 | 已通过 `DisplaySummaryPrintsNameSizeStrategy` 等用例间接覆盖 | 3 列固定列宽 80 | 已实现（间接） |
| 详情表布局校验 | 已通过 `DisplayDetailPrintsDetailTable`、`CreatePrintsAllocatedDetailTable` 覆盖 | 7 列 + 表头合并行 `Name: <name> Strategy: <Linear\|Striped>` | 已实现（间接） |
| 错误码透传文案 | 已通过 `DisplaySummaryReturnsInternalErrorWithCode`、`CreateReturnsInternalErrorWithCode` 覆盖 | `ERROR: Internal error with error code N` | 已实现（间接） |

#### 5.8.3 测试夹具

```cpp
class TestUbseCliSsuCmdReg : public testing::Test {
public:
    TestUbseCliSsuCmdReg() = default;
    void SetUp() override;    // ResetSsuMockCapture() 清空全局捕获
    void TearDown() override; // MOCKER(&ubse_invoke_call).reset() + GlobalMockObject::verify()
};
```

#### 5.8.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `&ubse_invoke_call` | `MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_*_invoke_call_*))` | 替换 UDS IPC 调用，模拟 daemon 响应 |
| `&ubse_invoke_call` | `MOCKER(&ubse_invoke_call).stubs().will(returnValue(N))` | 模拟 daemon 返回非 0 状态码 |
| `g_ssuMockLast*` | `CaptureSsuRequest()` 捕获 | 断言请求体的 moduleCode/opCode/payload |

**Mock 桩函数清单**（位于 `test_ubse_cli_ssu_mock_invoke.cpp`）：

| 桩函数 | 用途 |
|--------|------|
| `mock_ssu_alloc_summary_invoke_call_empty` | 概览返回空列表 |
| `mock_ssu_alloc_summary_invoke_call_normal` | 概览返回多条分配 |
| `mock_ssu_alloc_summary_invoke_call_subgib` | 概览返回 sub-GiB 容量 |
| `mock_ssu_alloc_summary_invoke_call_bad_response` | 概览返回非法列表长度 |
| `mock_ssu_alloc_summary_invoke_call_empty_body` | 概览返回空 body |
| `mock_ssu_alloc_detail_invoke_call_empty` | 详情返回空 namespace 列表 |
| `mock_ssu_alloc_detail_invoke_call_normal` | 详情返回正常 namespace 列表 |
| `mock_ssu_alloc_detail_invoke_call_subgib` | 详情返回 sub-GiB 容量 |
| `mock_ssu_alloc_detail_invoke_call_bad_response` | 详情返回非法列表长度 |
| `mock_ssu_alloc_create_invoke_call_normal` | create 返回分配详情 |
| `mock_ssu_attach_*_invoke_call_*`（待补） | attach 各类响应 |
| `mock_ssu_detach_*_invoke_call_*`（待补） | detach 各类响应 |

---

### 5.9 CLI wire 契约测试（TestUbseCliSsuStruct）

#### 5.9.1 测试目标

验证 `UbseCliSsuAllocDetailReq`、`UbseCliSsuAllocCreateReq`、`UbseCliSsuAllocResult`、`UbseCliSsuAllocListRsp` 的 `Serialize`/`Deserialize` 行为，覆盖需求 FR-CLI-WIRE-01~05、防御性反序列化（不可信响应防护）、NFR-SEC-06（安全函数）。

#### 5.9.2 测试场景

**A. 请求序列化（FR-CLI-WIRE-01/02）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| DetailRequestIsBareLengthPrefixedStringWithoutNullTerminator | `UbseCliSsuAllocDetailReq` 序列化 | 字节流 = `uint32_t length` + `name`，无 `\0` 终止符 | 已实现 |
| CreateRequestMatchesHandlerFieldOrderAndEnumWidths | `UbseCliSsuAllocCreateReq` 序列化 | 字段顺序：name/nsSize/nsNum/lbaFormat(uint32)/strategy(uint8)/tenant；枚举宽度与服务端一致 | 已实现 |

**B. 响应反序列化（FR-CLI-WIRE-03/04）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| AllocationListConsumesMultipleNamespacesAndAllowedHostNqns | 多 namespace + 多 host_nqn 列表 | 反序列化成功，字段值与原对象一致 | 已实现 |
| ResponsesRejectTruncationAndInvalidStringLength | 响应体被截断 1 字节 | 反序列化失败（返回 false） | 已实现 |
| ResponsesRejectOversizedLists | allocList 长度 > `SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS` 或 namespaceList 长度 > `SSU_CLI_MAX_NS_NUM` | 反序列化失败 | 已实现 |
| ResponsesRejectOversizedAllowedHostListAndInvalidEnums | allowHostNqnList 超限、strategy 枚举越界、lbaFormat 枚举越界 | 反序列化失败 | 已实现 |

**C. 请求侧防御性校验（FR-CLI-WIRE-03）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| RequestsRejectOverlongStringsAndInvalidEnums | `name` 长度 > 48、`lbaFormat`/`strategy` 枚举越界 | 序列化失败（返回 false） | 已实现 |

**D. 常量一致性（FR-CLI-WIRE-05，见设计文档 §14.11.1 业务约束）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| PublicConstantsReuseSsuLimits | `SSU_CLI_MAX_NAME_LENGTH`、`SSU_CLI_MAX_NS_NUM`、`SSU_CLI_MIN_SIZE_BYTES` 取值 | 与 `ubse_ssu_def.h`、`ubse_ssu_service.h` 一致 | 已实现 |

**E. 双源一致性（FR-CLI-WIRE-05）—— 待补**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| AttachSpaceRequestMatchesHandlerFieldOrder | attach 请求字段顺序与服务端 `SsuAttachSpacePack` 一致 | 字段对齐 | 待补 |
| AttachStripedRequestMatchesHandlerFieldOrder | attach striped 请求字段顺序与服务端一致 | 字段对齐 | 待补 |
| DetachSpaceRequestMatchesHandlerFieldOrder | detach 请求字段顺序与服务端一致 | 字段对齐 | 待补 |
| DetachAggregatedRequestMatchesHandlerFieldOrder | detach aggregated 请求字段顺序与服务端一致 | 字段对齐 | 待补 |

#### 5.9.3 测试夹具

`TestUbseCliSsuStruct` 不使用 Fixture（用 `TEST` 而非 `TEST_F`），所有辅助函数在匿名命名空间内：

```cpp
namespace {
uint32_t StringSize(const std::string &value);
uint32_t NameSpaceSize(const UbseCliSsuNameSpaceInfo &value);
uint32_t AllocationSize(const UbseCliSsuAllocResult &value);
bool PackString(UbsePackUtil &pack, const std::string &value);
bool PackNameSpace(UbsePackUtil &pack, const UbseCliSsuNameSpaceInfo &value);
bool PackAllocation(UbsePackUtil &pack, const UbseCliSsuAllocResult &value);
std::vector<uint8_t> PackAllocationList(const std::vector<UbseCliSsuAllocResult> &values);
UbseCliSsuAllocResult MakeAllocation(const std::string &name, UbseSsuAllocStrategy strategy, uint32_t namespaceId);
template <typename Request>
UbseUnpackUtil SerializeAndOpen(const Request &request, std::vector<uint8_t> &payload);
} // namespace
```

#### 5.9.4 Mock 依赖

无 Mock 依赖，`ubse_cli_ssu_struct.cpp` 为纯算法模块，直接通过 `UbsePackUtil`/`UbseUnpackUtil` 构造字节流。

---

### 5.10 CLI 校验函数测试（TestUbseCliSsuValidate，新增）

#### 5.10.1 测试目标

直接验证 `IsValidName`、`IsValidDevName`、`IsValidLengthIfPresent`、`ParseSize`、`ParseNsNum`、`ParseLba`、`ParseStrategy`、`ParseLevel`、`ParseChunkSize` 9 个校验函数，覆盖需求 FR-CLI-VALIDATE-01~10、设计文档 §14.11.1 业务约束。

#### 5.10.2 测试场景

**A. 名称校验（FR-CLI-VALIDATE-01/06）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| IsValidName_Empty_ReturnsFalse | 空字符串 | false | 待补 |
| IsValidName_OneChar_ReturnsTrue | 1 字符合法字符 | true | 待补 |
| IsValidName_FortyEightChars_ReturnsTrue | 48 字符合法字符 | true | 待补 |
| IsValidName_FortyNineChars_ReturnsFalse | 49 字符 | false | 待补 |
| IsValidName_ContainsHash_ReturnsFalse | 含 `#` | false | 待补 |
| IsValidName_ContainsUnderscore_ReturnsTrue | 含 `_` | true | 待补 |
| IsValidDevName_Empty_ReturnsFalse | 空字符串 | false | 待补 |
| IsValidDevName_ThirtyTwoChars_ReturnsTrue | 32 字符合法字符 | true | 待补 |
| IsValidDevName_ThirtyThreeChars_ReturnsFalse | 33 字符 | false | 待补 |

**B. 容量校验（FR-CLI-VALIDATE-02）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| ParseSize_LowercaseG_ReturnsFalse | `10g` | false | 待补 |
| ParseSize_NoSuffix_ReturnsFalse | `10` | false | 待补 |
| ParseSize_ZeroG_ReturnsFalse | `0G` | false（< 1G） | 待补 |
| ParseSize_OneG_ReturnsTrue | `1G` | true，`sizeBytes=1GiB` | 待补 |
| ParseSize_Uint64Overflow_ReturnsFalse | 数值 × GiB 溢出 uint64 | false | 待补 |
| ParseSize_StoullOverflow_ReturnsFalse | 数值超出 stoull 范围 | false | 待补 |

**C. 数量与枚举校验（FR-CLI-VALIDATE-03/04/05/07/08）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| ParseNsNum_Zero_ReturnsFalse | `0` | false | 待补 |
| ParseNsNum_One_ReturnsTrue | `1` | true，`nsNum=1` | 待补 |
| ParseNsNum_OneHundredTwentyEight_ReturnsTrue | `128` | true | 待补 |
| ParseNsNum_OneHundredTwentyNine_ReturnsFalse | `129` | false | 待补 |
| ParseLba_512B_ReturnsTrue | `512B` | true，`lbaFormat=LBA_FORMAT_512` | 待补 |
| ParseLba_4K_ReturnsTrue | `4K` | true，`lbaFormat=LBA_FORMAT_4K` | 待补 |
| ParseLba_512Bare_ReturnsFalse | `512`（无 B） | false | 待补 |
| ParseLba_1K_ReturnsFalse | `1K` | false | 待补 |
| ParseStrategy_Linear_ReturnsTrue | `Linear` | true，`strategy=LINEAR` | 待补 |
| ParseStrategy_Striped_ReturnsTrue | `Striped` | true，`strategy=STRIPED` | 待补 |
| ParseStrategy_lowercase_ReturnsFalse | `striped` | false | 待补 |
| ParseLevel_raid0_ReturnsTrue | `raid0` | true，`level=RAID0` | 待补 |
| ParseLevel_raid5_ReturnsTrue | `raid5` | true，`level=RAID5` | 待补 |
| ParseLevel_raid1_ReturnsFalse | `raid1` | false | 待补 |
| ParseChunkSize_AllCanonicalValues_ReturnsTrue | `4K/16K/32K/64K/128K/256K/512K` | 全部 true，`chunkSize` 枚举正确 | 待补 |
| ParseChunkSize_8K_ReturnsFalse | `8K` | false | 待补 |
| ParseChunkSize_4KBare_ReturnsFalse | `4`（无 K） | false | 待补 |

**D. 长度校验（FR-CLI-VALIDATE-09）**

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| IsValidLengthIfPresent_Absent_ReturnsTrue | 参数不存在 | true | 待补 |
| IsValidLengthIfPresent_AtLimit_ReturnsTrue | 长度恰好等于上限 | true | 待补 |
| IsValidLengthIfPresent_BeyondLimit_ReturnsFalse | 长度超上限 | false | 待补 |

#### 5.10.3 测试夹具

```cpp
class TestUbseCliSsuValidate : public testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};
```

#### 5.10.4 Mock 依赖

无 Mock 依赖，校验函数为纯函数。

---

### 5.11 CLI main 入口测试（TestUbseCliSsuMain，新增）

#### 5.11.1 测试目标

验证 `main()` 函数的启动校验、参数解析、超时定时器、命令分发，覆盖需求 FR-CLI-STARTUP-01~12。

#### 5.11.2 测试场景

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| Main_AgcLessThanOne_ReturnsError | `argc=0` | 返回 UBSE_ERROR | 待补（IT 优先） |
| Main_AgcExceedsFortyThree_ReturnsError | `argc=44` | 返回 UBSE_ERROR | 待补（IT 优先） |
| Main_HelpShortCircuits_ReturnsZero | `--help` | 输出帮助信息，退出码 0 | 待补（IT 优先） |
| Main_MissingCommand_ReturnsError | 无 command/type | 返回错误 | 待补（IT 优先） |
| Main_SsuallocSummaryDispatches | `display ssu -t alloc_summary` | 调用 `UbseCliDisplaySsuFunc` | 待补（IT 优先） |
| Main_SigalrmHandlerInstalled | 启动时注册 SIGALRM | 信号处理器已注册 | 待补（IT 优先） |

**说明**：`main()` 入口测试依赖完整 CLI Framework 装配，建议在 IT 层覆盖（见 6.2.4）。UT 层仅覆盖可独立测试的辅助函数。

#### 5.11.3 测试夹具

```cpp
class TestUbseCliSsuMain : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
};
```

#### 5.11.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `UbseCliCallAllModuleSignUp` | MockCpp | 模拟模块注册 |
| `UbseCliExecuteMatchedCommand` | MockCpp | 模拟命令分发 |

---

### 5.12 CLI IPC 模板测试（TestUbseCliSsuIpcTemplate，新增）

#### 5.12.1 测试目标

验证 `InvokeSsuIpc` 4 个重载模板的统一行为，覆盖需求 FR-CLI-IPC-01~09、RAII 资源释放（响应缓冲自动回收）、NFR-SEC-06（安全函数）。

#### 5.12.2 测试场景

| 用例名 | 场景 | 预期结果 | 状态 |
|--------|------|----------|------|
| InvokeSsuIpc_WithReqWithRsp_Success | 有 req 有 rsp 模板，daemon 返回 0 + 合法 body | 调用 lambda，返回 lambda 结果 | 待补 |
| InvokeSsuIpc_WithReqWithRsp_DeserializationFailure | 有 req 有 rsp 模板，响应体损坏 | 返回 ERR_DESERIALIZATION | 待补 |
| InvokeSsuIpc_WithReqWithRsp_StatusCodeNonZero | 有 req 有 rsp 模板，daemon 返回非 0 | 返回 `ERROR: Internal error with error code N` | 待补 |
| InvokeSsuIpc_NoReqWithRsp_Success | 无 req 有 rsp 模板（display summary） | 请求 buffer 为 `{nullptr, 0}`，调用 lambda | 待补 |
| InvokeSsuIpc_WithReqNoRsp_Success | 有 req 无 rsp 模板（detach） | 调用 `ubse_invoke_call`，无反序列化 | 待补 |
| InvokeSsuIpc_RawBuffer_Success | 原始 buffer 模板 | 直接传递 buffer，不做模板化处理 | 待补 |
| InvokeSsuIpc_ResponseBufferAutoReleased | RAII 测试 | 响应缓冲在作用域结束时自动 `ubse_api_buffer_free` | 待补 |

#### 5.12.3 测试夹具

```cpp
class TestUbseCliSsuIpcTemplate : public testing::Test {
public:
    void SetUp() override { ResetSsuMockCapture(); }
    void TearDown() override { MOCKER(&ubse_invoke_call).reset(); GlobalMockObject::verify(); }
};
```

#### 5.12.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `&ubse_invoke_call` | `MOCKER(&ubse_invoke_call).stubs().will(...)` | 模拟 UDS IPC 调用 |

---

## 6. 集成测试设计

### 6.1 测试目标

验证 SSU 模块与其他模块的协同工作，覆盖需求 FR-DISTRIBUTED、FR-NORTHBOUND；CLI 侧验证与 CLI Framework、daemon 的协同，覆盖需求 FR-CLI-STARTUP、FR-CLI-IPC、FR-CLI-WIRE（双源一致性）、FR-CLI-AUTH。

### 6.2 测试场景

#### 6.2.1 模块装配

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ModuleInit_Success | 模块初始化 | UBSE_OK，所有依赖注册成功 |
| ModuleStart_AsMaster | master 启动 | collector 启动，HTTP 路由注册 |
| ModuleStart_AsAgent | agent 启动 | collector 跳过，RPC 处理器注册 |
| ModuleStop_Cleanup | 停止清理 | collector 停止，资源释放 |

#### 6.2.2 RPC 通信

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| AgentAlloc_ForwardToMaster | agent 转发 alloc | master 收到请求，返回响应 |
| MasterAllocResp_PushToAgent | master 推送响应 | agent 收到响应，future 完成 |
| RpcTimeout_RetryAndCleanup | RPC 超时 | 重试 3 次后失败，调用 FreeSpace 清理 |
| MasterFailover_StandbyTakeover | master 故障切换 | 新 master 接管，账本重建 |

#### 6.2.3 HTTP 北向接口

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| HttpAlloc_Success | HTTP 分配 | 200 OK，返回 JSON |
| HttpAlloc_MtlsReject | mTLS 失败 | 401 Unauthorized |
| HttpFree_Idempotent | 释放不存在 | 200 OK（幂等） |
| HttpList_Success | 列表查询 | 200 OK，返回列表 |
| HttpList_PermissionDeny | 无 ssu 权限 | 403 Forbidden |

#### 6.2.4 CLI 模块装配（FR-CLI-STARTUP、FR-CLI-CMD-REG）

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ModuleInit_StaticRegistrationWorks | 程序启动 | `UbseCliCallAllModuleSignUp` 触发 `UbseCliRegSsuModule::UbseCliSignUp`，注册 4 条命令 |
| ModuleInit_HelpShortCircuits | `--help` | 输出含 display/create/attach/detach 命令清单 |
| ModuleInit_TimeoutTimerInstalled | 启动 | SIGALRM 处理函数已注册，`setitimer` 已调用 |
| ModuleInit_IpcLogSuppressed | 命令执行期间 | IPC 日志被屏蔽 |

#### 6.2.5 CLI ↔ daemon UDS 协议（FR-CLI-IPC、FR-CLI-WIRE）

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| UdsProtocol_ModuleCodeIsSsu | 任意命令 | 请求 header `module_code=0x000D` |
| UdsProtocol_OpCodeMatchesContract | display/create/attach/detach | opCode 与 `UbseSsuOpCode` 枚举一致 |
| UdsProtocol_ShortConnection | 每次调用 | connect-send-recv-disconnect 一次完整往返 |
| WireContract_FieldOrderMatchesHandler | 字段顺序 | CLI `Serialize` 字节流与 daemon `Ssu*Unpack` 期望一致 |
| WireContract_EnumWidthMatchesHandler | 枚举宽度 | `lbaFormat` 为 uint32、`strategy` 为 uint8、`level` 为 uint32 |
| WireContract_DefaultPathIsUbsesock | UDS 路径 | 默认连接 `/var/run/ubse/ubse.sock` |

#### 6.2.6 CLI 隐式认证（FR-CLI-AUTH）

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Auth_PeercredExtractedUid | daemon 侧 | `SO_PEERCRED` 提取对端 uid |
| Auth_RbacCheckByUserName | 非授权用户 | daemon 返回错误码 30，CLI 输出 `ERROR: Internal error with error code 30` |
| Auth_BuiltinUserBypassRbac | 内置用户 | daemon 自动授予权限，命令成功 |

---

## 7. 性能测试设计

### 7.1 测试目标

验证 SSU 模块满足 NFR-PERF-01 ~ NFR-PERF-08 性能指标；CLI 侧满足 NFR-PERF-09（单命令响应时间 ≤ 3 秒）、NFR-PERF-10（命令超时阈值）、NFR-PERF-11（启动到退出 ≤ 100ms）、NFR-PERF-12（短连接批量性能）。

### 7.2 测试场景

| 用例名 | 场景 | 期望指标 |
|--------|------|----------|
| PerfAlloc_10Ns | 分配 10 个 namespace | 响应时间 ≤ 3s |
| PerfFree_10Ns | 释放 10 个 namespace | 响应时间 ≤ 3s |
| PerfAttach_10Ns | 挂载 10 个 namespace | 响应时间 ≤ 5s |
| PerfScheduler_100Devs | 100 个设备调度 | 算法执行 ≤ 100ms |
| PerfConcurrent_18Threads | 18 并发请求 | 全部成功，无超时 |
| PerfQueue_1000 | 1000 任务排队 | 队列不溢出 |
| PerfQueue_1001_Reject | 1001 任务 | 第 1001 个被拒绝 |
| Perf_DisplaySummary_ResponseTime | `display ssu -t alloc_summary`（daemon 返回 1000 条分配） | 响应时间 ≤ 3 秒（NFR-PERF-09） |
| Perf_Create_ResponseTime | `create ssu -n test -s 10G` | 响应时间 ≤ 3 秒 |
| Perf_DisplayDetail_LargeNamespaceList | `display ssu -t alloc_detail -n test`（单 allocation 含 128 个 namespace） | 响应时间 ≤ 3 秒 |
| Perf_CliStartup_ExitTime | `ubsectl-ssu --help` | 启动到退出 ≤ 100ms（NFR-PERF-11） |
| Perf_Batch_NConnections | 连续 100 次 `display ssu -t alloc_summary` | 全部成功，总耗时 ≤ 30 秒，无 socket 泄漏 |
| Perf_TimeoutThreshold | daemon 卡死不响应 | 30 分钟（默认）/ 30 秒（建议）后 SIGALRM 触发退出（NFR-PERF-10） |

### 7.3 测试方法

- 使用 `std::chrono` 测量响应时间
- 使用 `std::thread` 模拟并发
- 使用 Google Benchmark 或自写循环执行 N 次取平均
- 关闭日志输出避免 IO 影响
- CLI 场景使用 `std::chrono::steady_clock` 测量响应时间，取 100 次平均 + P99
- CLI 场景使用 `valgrind --tool=massif` 检测内存占用，确认无 socket fd 泄漏
- CLI 场景使用 `strace -e trace=connect,sendto,recvfrom,close` 确认每次调用 connect-send-recv-close 一次完整往返

---

## 8. 模糊测试设计

### 8.1 测试目标

通过随机/异常输入发现 SSU 模块的崩溃、内存错误、未定义行为，覆盖需求 NFR-REL-03（幂等性）、NFR-SEC-06（安全函数）。

### 8.2 测试场景

| 用例名 | 输入域 | 期望结果 |
|--------|--------|----------|
| FuzzAlloc_Name | 随机 name（0~256 字符，含特殊字符） | 不崩溃，返回合理错误码 |
| FuzzAlloc_NsSize | 0、UINT64_MAX、随机值 | 不崩溃，不整型溢出 |
| FuzzAlloc_NsNum | 0、UINT32_MAX、随机值 | 不崩溃 |
| FuzzAlloc_Strategy | 非法 enum 值 | 不崩溃，返回错误 |
| FuzzAttach_Nqn | 超长 NQN、空、特殊字符 | 不崩溃 |
| FuzzFeDeviceAlloc_Vfe | 非法 slotId/chipId/dieId/pfeId/vfeId | 不崩溃 |
| FuzzFeDeviceAlloc_BusInstGuid | 空、超长、非 hex | 不崩溃 |
| FuzzRpc_ReqMsg | 篡改序列化字节 | 反序列化失败，不崩溃 |
| Fuzz_Deserialize_AllocList_RandomBytes | 随机字节流（1~4096 字节） | 不崩溃，`Deserialize` 返回 false |
| Fuzz_Deserialize_AllocList_Truncated | 截断 1~16 字节 | 不崩溃，返回 false |
| Fuzz_Deserialize_AllocList_OversizedCount | 列表长度字段 = `UINT32_MAX` | 不崩溃，不预分配巨量容器，返回 false |
| Fuzz_Deserialize_Namespace_OversizedHostNqnList | allowHostNqnList 长度 = `UINT32_MAX` | 不崩溃，返回 false |
| Fuzz_Deserialize_EnumOutOfBounds | strategy/lbaFormat/level/chunkSize 枚举值越界 | 不崩溃，返回 false |
| Fuzz_Deserialize_StringOverlong | 字符串长度字段 = `UINT32_MAX` | 不崩溃，返回 false |
| Fuzz_ParseSize_RandomString | 随机字符串（含特殊字符、Unicode、超长） | 不崩溃，返回 false 或正确解析 |
| Fuzz_ParseName_RandomString | 随机字符串 | 不崩溃，返回 false 或正确解析 |
| Fuzz_CmdReg_ParamsMap_RandomKeys | 随机键值对 | 不崩溃，命令按规则处理 |

### 8.3 测试方法

- 使用 libFuzzer 或自定义随机输入生成器
- 编译时启用 ASAN/UBSAN
- 持续运行 24 小时，监控崩溃与告警
- 对 CLI `Deserialize` 函数，输入为 `const uint8_t*` + `uint32_t length`，直接 fuzz
- 对 CLI `Parse*` 函数，输入为 `std::string`，通过 `std::string(uint8_t*, uint8_t*)` 构造
- 发现 crash 后保存 corpus，加入回归测试集

---

## 9. 端到端测试设计

### 9.1 测试目标

验证从 SDK/HTTP/CLI 接入到硬件操作的完整业务链路，覆盖所有 AC-* 验收项。

### 9.2 测试环境

- 真实 SSU 硬件设备（至少 4 个）
- 多节点集群（1 master + 2 agent + 1 standby）
- 真实 NVMe-oF 客户端

### 9.3 测试场景

#### 9.3.1 业务用户场景（AC-ALLOC ~ AC-QUERY）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_CSDK_AllocAttach | C SDK → AllocSpace → AttachSpace | AC-ALLOC-01, AC-ATTACH-01 |
| E2E_Http_AllocStriped | HTTP → AllocSpace(STRIPED) | AC-ALLOC-02 |
| E2E_Cli_Create | CLI → ubsectl ssu create | AC-ALLOC-01 |
| E2E_Alloc_Idempotent | 重复 AllocSpace 同名 | AC-ALLOC-03 |
| E2E_Alloc_Concurrent | 2 agent 并发分配 | AC-ALLOC-04 |
| E2E_Alloc_TenantIsolation | 跨租户分配 | AC-ALLOC-05 |
| E2E_Alloc_Rollback | 模拟创建失败 | AC-ALLOC-06 |
| E2E_Free_Reuse | 释放后重新分配 | AC-FREE-02, AC-FREE-05 |
| E2E_Free_Idempotent | 释放不存在 | AC-FREE-03 |
| E2E_Attach_Linear | 线性聚合挂载 | AC-ATTACH-04 |
| E2E_Attach_StripedRaid0 | RAID0 聚合 | AC-ATTACH-05 |
| E2E_Attach_StripedRaid5 | RAID5 聚合 | AC-ATTACH-05, AC-ATTACH-06 |
| E2E_Attach_NonAuthHost | 非授权 Host 访问 | AC-ATTACH-03 |
| E2E_Attach_IdentityVerify | 身份验证失败 | AC-ATTACH-07 |

#### 9.3.2 VFE 直通场景（AC-FE-01 ~ AC-FE-07）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_FeDeviceAlloc_Success | SDK → FeDeviceAlloc | AC-FE-01, AC-FE-02 |
| E2E_FeDeviceAlloc_Preempt | VFE 被占用，抢占 | AC-FE-03 |
| E2E_FeDeviceFree_Success | SDK → FeDeviceFree | AC-FE-04 |
| E2E_FeDeviceFree_UpiMismatch | upi 不匹配 | AC-FE-05, NFR-SEC-04 |
| E2E_FeDeviceList_Query | GetFeDeviceList | AC-FE-06 |
| E2E_EmptyBusInst_Cleanup | 空 bus instance 自动清理 | AC-FE-07, NFR-REL-07 |

#### 9.3.3 高可用场景（AC-DIST-01 ~ AC-DIST-06）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_MasterFailover | kill master，standby 接管 | AC-DIST-01, NFR-REL-02 |
| E2E_LedgerRebuild | master 重启后账本重建 | AC-DIST-02, AC-ALLOC-07 |
| E2E_RpcTimeout_Retry | RPC 超时重试 | AC-DIST-03 |
| E2E_AgentFail_Continued | agent 故障，其他 agent 正常 | AC-DIST-05 |
| E2E_VIPDrift | VIP 漂移后 HTTP 可访问 | AC-DIST-06 |

#### 9.3.4 CLI display ssu 场景（AC-CLI-DISPLAY-01 ~ 06）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_DisplaySummary_Success | `ubsectl-ssu display ssu -t alloc_summary` | AC-CLI-DISPLAY-01、AC-CLI-OUTPUT-01 |
| E2E_DisplaySummary_Empty | 无分配时执行 | AC-CLI-DISPLAY-02、AC-CLI-OUTPUT-03 |
| E2E_DisplayDetail_Success | `ubsectl-ssu display ssu -t alloc_detail -n <name>` | AC-CLI-DISPLAY-03、AC-CLI-OUTPUT-02 |
| E2E_DisplayDetail_MissingName | 不带 `-n` | AC-CLI-DISPLAY-04 |
| E2E_DisplayDetail_InvalidName | `-n` 含 `#` | AC-CLI-DISPLAY-05 |
| E2E_DisplaySummary_SizeAggregation | 多 namespace 容量聚合 | AC-CLI-DISPLAY-06 |

#### 9.3.5 CLI create ssu 场景（AC-CLI-CREATE-01 ~ 09）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_Create_Success | `create ssu -n test -s 10G` | AC-CLI-CREATE-01 |
| E2E_Create_MissingName | 不带 `-n` | AC-CLI-CREATE-02 |
| E2E_Create_MissingSize | 不带 `-s` | AC-CLI-CREATE-03 |
| E2E_Create_LowercaseSize | `-s 10g` | AC-CLI-CREATE-04 |
| E2E_Create_ZeroSize | `-s 0G` | AC-CLI-CREATE-05 |
| E2E_Create_ZeroNsNum | `-m 0` | AC-CLI-CREATE-06 |
| E2E_Create_ExceedNsNum | `-m 129` | AC-CLI-CREATE-07 |
| E2E_Create_InvalidLba | `-l 1K` | AC-CLI-CREATE-08 |
| E2E_Create_InvalidStrategy | `-r raid0` | AC-CLI-CREATE-09 |

#### 9.3.6 CLI attach ssu 场景（AC-CLI-ATTACH-01 ~ 08）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_AttachSpace_Success | `attach ssu -n test` | AC-CLI-ATTACH-01 |
| E2E_AttachLinear_Success | `attach ssu -t Linear -n test -d dev1` | AC-CLI-ATTACH-02 |
| E2E_AttachStriped_Success | `attach ssu -t Striped -n test -d dev1 -l raid0 -c 4K` | AC-CLI-ATTACH-03 |
| E2E_AttachSpace_RejectsAggregationOptions | 未指定 `-t` 但带 `--dev_name` | AC-CLI-ATTACH-04 |
| E2E_AttachLinear_MissingDevName | `-t Linear` 缺 `-d` | AC-CLI-ATTACH-05 |
| E2E_AttachStriped_MissingLevel | `-t Striped` 缺 `-l` | AC-CLI-ATTACH-06 |
| E2E_AttachStriped_InvalidLevel | `-l raid1` | AC-CLI-ATTACH-07 |
| E2E_AttachStriped_InvalidChunkSize | `-c 8K` | AC-CLI-ATTACH-08 |

#### 9.3.7 CLI detach ssu 场景（AC-CLI-DETACH-01 ~ 05）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_DetachSpace_Success | `detach ssu -n test` | AC-CLI-DETACH-01 |
| E2E_DetachLinear_Success | `detach ssu -t Linear -n test -d dev1` | AC-CLI-DETACH-02 |
| E2E_DetachStriped_Success | `detach ssu -t Striped -n test -d dev1` | AC-CLI-DETACH-03 |
| E2E_DetachSpace_RejectsDevNameOption | 未指定 `-t` 但带 `--dev_name` | AC-CLI-DETACH-04 |
| E2E_DetachLinear_MissingDevName | `-t Linear` 缺 `-d` | AC-CLI-DETACH-05 |

#### 9.3.8 CLI 隐式认证场景（AC-CLI-AUTH-01 ~ 03）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_Auth_NonAuthorizedUserRejected | 非授权用户执行 CLI | AC-CLI-AUTH-01 |
| E2E_Auth_UdsSocketPermission0660 | 检查 UDS socket 权限 | AC-CLI-AUTH-02 |
| E2E_Auth_BuiltinUserBypassRbac | 内置用户执行任意命令 | AC-CLI-AUTH-03 |

#### 9.3.9 CLI 启动与覆盖边界场景（AC-CLI-STARTUP-01 ~ 04、AC-CLI-COVERAGE-01）

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_Startup_HelpOutput | `ubsectl-ssu --help` | AC-CLI-STARTUP-01 |
| E2E_Startup_MissingCommand | `ubsectl-ssu` 无参数 | AC-CLI-STARTUP-02 |
| E2E_Startup_TooManyArgs | `argc > 43` | AC-CLI-STARTUP-03 |
| E2E_Startup_TimeoutTriggered | daemon 卡死 | AC-CLI-STARTUP-04 |
| E2E_Coverage_HelpListsFourCommands | `--help` 输出 | AC-CLI-COVERAGE-01 |

---

## 10. 测试数据管理

### 10.1 测试数据分类

| 类型 | 说明 | 示例 |
|------|------|------|
| 输入数据 | 接口参数 | name、nsSize、nsNum、tenant |
| Mock 数据 | 模拟外部依赖返回 | devList、busInsts、pfeList |
| 期望数据 | 断言期望值 | ret=UBSE_OK、state=CREATED |
| 状态数据 | 被测对象内部状态 | reservedBytes、pendingOps_、ledger entries |

### 10.2 测试数据工厂

通过 helper 方法集中管理测试数据，便于维护：

```cpp
// 设备构造
static UbseSsuDevInfo MakeDev(eid, totalBytes, usedBytes, nsCount, state);
static UbseSsuDevInfo MakeDevWithTenant(eid, totalBytes, usedBytes, tenant, nsCount, state);

// PFE/VFE 构造
UbseMtiIdevPfe MakePfe(slotId, chipId, dieId, pfeId);
UbseMtiIdevVfe MakeVfe(slotId, chipId, dieId, pfeId, vfeId);
UbseMtiBusInst MakeVmBusInst(upi, guid, subDeviceGuids);
UbseMtiGuid MakeGuid(val);

// namespace 构造
static UbseSsuDevNameSpace MakeNameSpaceForCreate(eid, subNqn, nsze, ncap);
static UbseSsuDevNameSpace MakeNameSpaceForBasic(eid, subNqn, namespaceId, guid);

// CLI 侧（匿名命名空间）
std::map<std::string, std::string> CreateParams();                 // 最小合法入参
std::map<std::string, std::string> DetailParams(name);             // alloc_detail 入参
UbseCliSsuAllocResult BuildSsuAllocResult();                       // 合法响应对象（2 个 namespace）
UbseCliSsuAllocResult MakeAllocation(name, strategy, namespaceId); // 构造 allocation
std::string MakeNameOfLength(uint32_t length);                     // 指定长度的合法 name（待补）
std::string MakeDevNameOfLength(uint32_t length);                  // 指定长度的合法 dev_name（待补）
```

### 10.3 测试数据隔离

- 每个用例独立构造数据，避免共享状态污染
- 单例对象在 TearDown 中恢复初始状态（如 `mgr.state_ = AVAILABLE`）
- Mock 期望在 TearDown 中通过 `GlobalMockObject::verify()` 校验并清理
- CLI 用例通过 `ResetSsuMockCapture()` 在 SetUp 中清空全局捕获器（`g_ssuMockLast*`）
- CLI 用例 TearDown 中 `MOCKER(&ubse_invoke_call).reset()` 复位桩
- 注册类用例起始调用 `UbseCliModuleRegistry::GetInstance().UbseCliReset()` 重置注册表

---

## 11. 测试执行与 CI 集成

### 11.1 本地执行

```shell
# 执行所有 UT
bash build.sh ut

# 执行 SSU 调度器 UT
bash build.sh ut -- --gtest_filter="TestUbseSsuScheduler.*"

# 执行单个用例
bash build.sh ut -- --gtest_filter="TestUbseSsuScheduler.PreCheckAllocSizeZero"

# 执行 VFE 直通 UT
bash build.sh ut -- --gtest_filter="TestUbseSsuDirectToVmManager.*"

# 执行适配器 UT
bash build.sh ut -- --gtest_filter="TestUbseSsuAdapterImpl.*"

# 执行 CLI UT（全部 SSU CLI 套件）
bash build.sh ut -- --gtest_filter="TestUbseCliSsu*.*"

# 执行单个 CLI TestSuite
bash build.sh ut -- --gtest_filter="TestUbseCliSsuCmdReg.*"

# 生成覆盖率报告
bash build.sh ut -C

# 覆盖率报告 + HTTP 服务查看
bash build.sh ut -C -H
```

### 11.2 CI 流水线

| 阶段 | 触发 | 动作 |
|------|------|------|
| Pre-Commit | git commit | `clang-format` + `clang-tidy` 检查 |
| CI-Build | push / PR | `bash build.sh` 编译 + 链接 |
| CI-UT | push / PR | `bash build.sh ut`，覆盖率 ≥ 80% |
| CI-IT | 每日 | `bash build.sh it` |
| CI-PT | 每版本 | 性能基线对比 |
| CI-FUZZ | 每版本 | 24 小时模糊测试 |
| CI-E2E | 每版本 | 真实硬件 E2E 测试 |

### 11.3 覆盖率度量

| 度量项 | 目标 | 工具 |
|--------|------|------|
| 行覆盖率 | ≥ 80% | gcov / lcov |
| 分支覆盖率 | ≥ 70% | gcov / lcov |
| 函数覆盖率 | ≥ 90% | gcov / lcov |
| 核心子模块 | ≥ 90% | Scheduler / DirectToVmManager / AdapterImpl |
| 行覆盖率（`ubse_cli_ssu_cmd_reg.cpp`） | ≥ 85% | gcov / lcov |
| 行覆盖率（`ubse_cli_ssu_struct.cpp`） | ≥ 90% | gcov / lcov |
| 分支覆盖率（`ubse_cli_ssu_cmd_reg.cpp`） | ≥ 75% | gcov / lcov |
| 核心函数（CLI `Parse*`、`IsValid*`） | ≥ 95% | gcov / lcov |

### 11.4 测试报告

- UT 报告：`${CMAKE_BINARY_DIR}/coverage/${module}_detail.xml`（JUnit XML 格式）
- 覆盖率报告：`./cmake-build-debug/coverage/index.html`
- CI 集成：通过 `gtest_discover_tests` 自动注册到 CTest，CI 抓取测试结果

---

## 12. 缺陷管理

### 12.1 缺陷分级

| 级别 | 定义 | 处理时限 |
|------|------|----------|
| P0 | 阻塞主流程，数据丢失 | 立即修复 |
| P1 | 核心功能不可用，无规避方案 | 1 个工作日 |
| P2 | 次要功能不可用，有规避方案 | 3 个工作日 |
| P3 | 体验问题、文档错误 | 版本发布前 |

### 12.2 缺陷跟踪

- 所有缺陷录入缺陷管理系统（如 Gitee Issue）
- 包含：标题、复现步骤、期望结果、实际结果、环境、日志、截图
- 关联到对应的 AC-* 验收项或 NFR-* 非功能需求

---

## 13. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| 真实硬件资源不足 | E2E 测试覆盖度受限 | 优先使用 Mock，关键路径使用真实硬件 |
| Mock 与真实行为不一致 | UT 通过但 E2E 失败 | 定期对比 Mock 行为与真实硬件返回 |
| 单例状态污染 | 用例间相互影响 | TearDown 严格恢复状态，使用 `GlobalMockObject::verify()` |
| ASAN 误报 | 性能下降，CI 失败 | UT 不开 ASAN，FUZZ 单独启用 |
| 覆盖率达标但质量不足 | 仅追求覆盖率 | 增加边界值、异常路径、决策表用例 |
| RPC 测试环境复杂 | 集成测试难复现 | 使用 MockRpc + Future 模拟，关键场景 E2E |
| MTI 调用串行限制 | 并发测试受限 | 通过 Mock 绕过串行限制，真实场景单独验证 |
| `ubse_invoke_call` 桩泄漏到其他用例 | 用例间相互影响，偶发失败 | `TearDown` 严格 `MOCKER(&ubse_invoke_call).reset()` + `GlobalMockObject::verify()` |
| `g_ssuMockLast*` 全局状态污染 | 用例间状态串扰 | SetUp 调用 `ResetSsuMockCapture()` |
| CLI wire 双源同步风险 | CLI `Serialize` 与 daemon `Ssu*Unpack` 字段顺序不一致，UT 通过但 E2E 失败 | 新增 `*MatchesHandlerFieldOrder` 系列用例（5.9.2 E），IT 阶段双源对比 |
| CLI `main()` 入口难 UT | 启动校验、超时定时器覆盖度低 | 转移至 IT 层覆盖（6.2.4），使用 `exec` 启动真实进程 |
| CLI 错误文案改动导致用例失败 | 文案微调引发批量 FAIL | 错误文案集中定义（`cmd_reg.cpp` 顶部），用例通过 `const std::string` 引用，避免硬编码 |
| CLI attach/detach 用例缺口 | FR-CLI-ATTACH/FR-CLI-DETACH 覆盖度不足 | 优先补全 21 个待补用例（5.8.2 E/F） |
| Mock 桩与真实行为不一致 | UT 通过但 E2E 失败 | 定期对比 Mock 桩与真实 daemon 返回，更新 `mock_ssu_*_invoke_call_*` |

---

## 14. 附录

### 14.1 现有测试用例统计

| 测试套件 | 文件 | 用例数 | 状态 |
|----------|------|--------|------|
| TestUbseSsuScheduler | test_ubse_ssu_scheduler.cpp | 24 | 已实现 |
| TestUbseSsuDirectToVmManager | test_ubse_ssu_direct_to_vm_manager.cpp | 45 | 已实现 |
| TestUbseSsuAdapterImpl | test_ubse_ssu_adapter_impl.cpp | 72 | 已实现 |
| TestUbseCliSsuCmdReg | test_ubse_cli_ssu_cmd_reg.cpp | 42 | 已实现 |
| TestUbseCliSsuStruct | test_ubse_cli_ssu_struct.cpp | 8 | 已实现 |
| TestUbseCliSsuValidate | 待新增 | 0 | 待补（14 用例） |
| TestUbseCliSsuMain | 待新增 | 0 | 待补（6 用例，IT 优先） |
| TestUbseCliSsuIpcTemplate | 待新增 | 0 | 待补（7 用例） |
| TestUbseCliSsuPerf | 待新增 | 0 | 待补（PT） |
| TestUbseSsuServiceImp | 待新增 | 0 | 待补 |
| TestUbseSsuDebtLedger | 待新增 | 0 | 待补 |
| TestUbseSsuReservationMgr | 待新增 | 0 | 待补 |
| TestUbseSsuCollector | 待新增 | 0 | 待补 |
| TestUbseSsuRpcProcessor | 待新增 | 0 | 待补 |
| TestUbseSsuHttpHandler | 待新增 | 0 | 待补 |

### 14.2 测试覆盖率目标

| 子模块 | 当前行覆盖率 | 目标行覆盖率 | 目标分支覆盖率 |
|--------|-------------|-------------|---------------|
| UbseSsuScheduler | 待测 | ≥ 90% | ≥ 80% |
| UbseSsuDirectToVmManager | 待测 | ≥ 90% | ≥ 80% |
| UbseSsuAdapterImpl | 待测 | ≥ 90% | ≥ 80% |
| UbseSsuServiceImp | 待测 | ≥ 80% | ≥ 70% |
| UbseSsuDebtLedger | 待测 | ≥ 80% | ≥ 70% |
| UbseSsuCollector | 待测 | ≥ 80% | ≥ 70% |
| UbseSsuRpcProcessor | 待测 | ≥ 80% | ≥ 70% |
| UbseSsuHttpHandler | 待测 | ≥ 80% | ≥ 70% |
| `ubse_cli_ssu_cmd_reg.cpp` | 待测 | ≥ 85% | ≥ 75% |
| `ubse_cli_ssu_struct.cpp` | 待测 | ≥ 90% | ≥ 80% |
| `ubse_cli_ssu_limits.h` | 间接覆盖 | ≥ 95% | ≥ 90% |
| `ubse_cli_ssu_main.cpp` | 待测 | ≥ 60% | ≥ 50% |

### 14.3 关键常量

| 常量 | 值 | 测试用途 |
|------|----|---------|
| MAX_NS_PER_DEV | 128 | 边界值测试（127/128/129） |
| SSU_NAME_MAX_LEN | 48 | name 长度边界测试 |
| TENANT_MAX_LEN | 16 | tenant 长度边界测试 |
| EID_SIZE | 24 | eid 长度校验 |
| MAX_TIMEOUT_SECONDS | 30 | RPC 超时测试 |
| MAX_RETRY_COUNT | 3 | 重试次数测试 |
| COMMON_RETRY_TIME | 5 | MTI 重试次数测试 |
| SLEEP_TIME | 2 | MTI 重试间隔测试（Mock nanosleep） |
| CLEAR_VM_BUS_INST_INTERVAL_SECONDS | 60 | 清理周期测试 |
| 线程池大小 | 18 | 并发能力测试 |
| 队列容量 | 1000 | 队列溢出测试 |
| `SSU_CLI_MAX_NAME_LENGTH` | 48 | CLI 名称边界值测试（47/48/49） |
| `SSU_CLI_MAX_HOST_NQN_LENGTH` | 68 | CLI host_nqn 边界值测试 |
| `SSU_CLI_MAX_SRC_EID_LENGTH` | 16 | CLI src_eid 边界值测试 |
| `SSU_CLI_MAX_DEV_NAME_LENGTH` | 32 | CLI dev_name 边界值测试（31/32/33） |
| `SSU_CLI_DEFAULT_NS_NUM` | 1 | CLI nsNum 默认值测试 |
| `SSU_CLI_MAX_NS_NUM` | 128 | CLI nsNum 边界值测试（127/128/129） |
| `SSU_CLI_MIN_SIZE_BYTES` | 1GiB | CLI size 边界值测试（1G/0G） |
| `SSU_CLI_MAX_NAMESPACES` | 128 | CLI namespace 列表上限测试 |
| `SSU_CLI_MAX_ALLOWED_HOST_NQNS` | 128 | CLI host_nqn 列表上限测试 |
| `SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS` | 1<<16 | CLI allocList 上限测试（FUZZ） |
| `SSU_CLI_WIRE_MAX_NAME_LENGTH` | 48 | CLI wire 字符串长度上限测试 |
| `SSU_CLI_WIRE_MAX_TENANT_LENGTH` | 17 | CLI tenant 长度上限测试 |
| `SSU_CLI_WIRE_MAX_UUID_LENGTH` | 37 | CLI nsUuid 长度上限测试 |
| `SSU_CLI_DETACH_LEVEL` | RAID0 | CLI detach 默认 level 测试 |
| `SSU_CLI_DETACH_CHUNK_SIZE` | CHUNK_SIZE_4K | CLI detach 默认 chunk_size 测试 |

### 14.4 相关文档

- [SSU模块需求分析文档.md](./SSU模块需求分析文档.md) —— 需求与验收标准
- [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) —— 设计与时序图
- [单元测试开发指南.md](../../../test/单元测试开发指南.md) —— UT 框架使用
- [UBSE代码Wiki.md](../../architecture/UBSE代码Wiki.md) —— 项目整体架构
- [AGENTS.md](../../../../AGENTS.md) —— 构建与测试命令
- [SSU模块开发指南.md](./SSU模块开发指南.md) —— 开发规范与排查指南

### 14.5 CLI 端到端追溯矩阵

下表建立 CLI 特性“需求 → 设计 → 代码 → 测试”的完整可追溯链路（设计章节引用见 [SSU模块特性设计文档](./SSU模块特性设计文档.md) 第 14 章）。

| 需求编号 | 需求描述 | 设计决策 | 代码位置 | 测试场景 | TestSuite | 状态 |
|----------|----------|----------|----------|----------|-----------|------|
| FR-CLI-STARTUP-01 | 启动保护 | §14.3 main 入口校验 | `ubse_cli_ssu_main.cpp` | Main_AgcLessThanOne_ReturnsError | TestUbseCliSsuMain | 待补 |
| FR-CLI-STARTUP-04 | 隐式 type 注入 | §14.3 main 入口 | `ubse_cli_ssu_main.cpp` | Main_HelpShortCircuits_ReturnsZero | TestUbseCliSsuMain | 待补 |
| FR-CLI-STARTUP-09 | 超时定时器 | §14.3 main 入口 | `ubse_cli_ssu_main.cpp` | Main_SigalrmHandlerInstalled | TestUbseCliSsuMain | 待补 |
| FR-CLI-CMD-REG | 命令注册 | §14.5 cmd_reg | `ubse_cli_ssu_cmd_reg.cpp:339` | SignUpRegistersDisplayAndCreateSsuCommands | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-DISPLAY-01 | 概览查询 | §14.5 + §14.7 时序图 | `ubse_cli_ssu_cmd_reg.cpp:425` | DisplaySummaryPrintsNameSizeStrategy | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-DISPLAY-02 | 详情查询 | §14.5 + §14.7 时序图 | `ubse_cli_ssu_cmd_reg.cpp:437` | DisplayDetailPrintsDetailTable | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-DISPLAY-03 | 名称校验 | §14.5 + IsValidName | `ubse_cli_ssu_cmd_reg.cpp:615` | DisplayDetailRejectsInvalidName | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-DISPLAY-04 | 概览输出 | §14.5 BuildSummaryTable | `ubse_cli_ssu_cmd_reg.cpp:560` | DisplaySummaryReturnsInfoWhenEmpty | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-DISPLAY-05 | 详情输出 | §14.5 BuildDetailTable | `ubse_cli_ssu_cmd_reg.cpp:550` | DisplayDetailPrintsDetailTable | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-DISPLAY-06 | 表格列宽 | §14.5 BuildSummaryTable | `ubse_cli_ssu_cmd_reg.cpp:562` | DisplaySummaryPrintsNameSizeStrategy | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-01 | 名称校验 | §14.5 IsValidName | `ubse_cli_ssu_cmd_reg.cpp:615` | CreateRejectsInvalidNameCharacters | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-02 | 容量校验 | §14.5 ParseSize | `ubse_cli_ssu_cmd_reg.cpp:631` | CreateRejectsLowercaseOrMissingSizeSuffix | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-03 | LBA 校验 | §14.5 ParseLba | `ubse_cli_ssu_cmd_reg.cpp:688` | CreateRejectsInvalidLba | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-04 | nsNum 校验 | §14.5 ParseNsNum | `ubse_cli_ssu_cmd_reg.cpp:664` | CreateRejectsInvalidNsNum | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-05 | 策略校验 | §14.5 ParseStrategy | `ubse_cli_ssu_cmd_reg.cpp:701` | CreateRejectsInvalidStrategy | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-06 | IPC 调用 | §14.5 + §14.7 时序图 | `ubse_cli_ssu_cmd_reg.cpp:456` | CreateAppliesDefaults | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-CREATE-07 | 输出 | §14.5 BuildDetailOutput | `ubse_cli_ssu_cmd_reg.cpp:550` | CreatePrintsAllocatedDetailTable | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-ATTACH-01 | 通用校验 | §14.5 UbseCliAttachSsuFunc | `ubse_cli_ssu_cmd_reg.cpp:493` | AttachRejectsMissingName | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-ATTACH-02 | 普通挂载 | §14.5 HandleAttachSpace | `ubse_cli_ssu_cmd_reg.cpp:226` | AttachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-ATTACH-03 | 线性聚合 | §14.5 HandleAttachLinear | `ubse_cli_ssu_cmd_reg.cpp:241` | AttachLinearSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-ATTACH-04 | 条带化聚合 | §14.5 HandleAttachStriped | `ubse_cli_ssu_cmd_reg.cpp:264` | AttachStripedSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-ATTACH-05 | IPC 调用 | §14.5 + §14.7 时序图 | `ubse_cli_ssu_cmd_reg.cpp:226-297` | AttachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-ATTACH-06 | 输出 | §14.5 BuildAttachOutput | `ubse_cli_ssu_cmd_reg.cpp` | AttachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-DETACH-01 | 通用校验 | §14.5 UbseCliDetachSsuFunc | `ubse_cli_ssu_cmd_reg.cpp:524` | DetachRejectsMissingName | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-DETACH-02 | 普通卸载 | §14.5 HandleDetachSpace | `ubse_cli_ssu_cmd_reg.cpp:299` | DetachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-DETACH-03 | 聚合卸载 | §14.5 HandleDetachAggregated | `ubse_cli_ssu_cmd_reg.cpp:311` | DetachLinearSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-DETACH-04 | IPC 调用 | §14.5 + §14.7 时序图 | `ubse_cli_ssu_cmd_reg.cpp:299-336` | DetachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-DETACH-05 | 隐藏选项 | §14.5 SSU_CLI_DETACH_* | `ubse_cli_ssu_limits.h:51-55` | DetachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-DETACH-06 | 输出 | §14.5 无响应体 | `ubse_cli_ssu_cmd_reg.cpp:308` | DetachSpaceSuccess | TestUbseCliSsuCmdReg | 待补 |
| FR-CLI-VALIDATE-01 | IsValidName | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:615` | IsValidName_Empty_ReturnsFalse | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-02 | ParseSize | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:631` | ParseSize_LowercaseG_ReturnsFalse | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-03 | ParseNsNum | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:664` | ParseNsNum_Zero_ReturnsFalse | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-04 | ParseLba | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:688` | ParseLba_512B_ReturnsTrue | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-05 | ParseStrategy | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:701` | ParseStrategy_Linear_ReturnsTrue | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-06 | IsValidDevName | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:724` | IsValidDevName_Empty_ReturnsFalse | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-07 | ParseLevel | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:738` | ParseLevel_raid0_ReturnsTrue | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-08 | ParseChunkSize | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:751` | ParseChunkSize_AllCanonicalValues_ReturnsTrue | TestUbseCliSsuValidate | 待补 |
| FR-CLI-VALIDATE-09 | IsValidLengthIfPresent | §14.5 校验子模块 | `ubse_cli_ssu_cmd_reg.cpp:714` | IsValidLengthIfPresent_Absent_ReturnsTrue | TestUbseCliSsuValidate | 待补 |
| FR-CLI-IPC-01 | 通信入口 | §14.6 UDS IPC 接入层 | `ubse_ipc_client.cpp` | InvokeSsuIpc_WithReqWithRsp_Success | TestUbseCliSsuIpcTemplate | 待补 |
| FR-CLI-IPC-02 | 短连接模型 | §14.6 + §14.5 IPC 模板 | `ubse_ipc_client.cpp:78-118` | UdsProtocol_ShortConnection | IT | 待补 |
| FR-CLI-IPC-03 | UDS 路径 | §14.6 UDS 接入层 | `ubse_ipc_client.h:43` | WireContract_DefaultPathIsUbsesock | IT | 待补 |
| FR-CLI-IPC-07 | RAII 资源释放 | §14.5 IPC 模板 + §14.10 | `ubse_cli_buffer_guard.h` | InvokeSsuIpc_ResponseBufferAutoReleased | TestUbseCliSsuIpcTemplate | 待补 |
| FR-CLI-IPC-09 | 模板封装 | §14.5 IPC 模板 | `ubse_cli_ssu_cmd_reg.cpp` | InvokeSsuIpc_*_Success | TestUbseCliSsuIpcTemplate | 待补 |
| FR-CLI-WIRE-01 | 编码工具 | §14.5 wire 子模块 | `ubse_cli_ssu_struct.cpp` | DetailRequestIsBareLengthPrefixedStringWithoutNullTerminator | TestUbseCliSsuStruct | 已实现 |
| FR-CLI-WIRE-02 | 字符串编码 | §14.5 wire 子模块 | `ubse_cli_ssu_struct.cpp` | CreateRequestMatchesHandlerFieldOrderAndEnumWidths | TestUbseCliSsuStruct | 已实现 |
| FR-CLI-WIRE-03 | 反序列化校验 | §14.5 + §14.8 并发控制 | `ubse_cli_ssu_struct.cpp` | RequestsRejectOverlongStringsAndInvalidEnums | TestUbseCliSsuStruct | 已实现 |
| FR-CLI-WIRE-04 | 列表上限 | §14.5 防御性反序列化 | `ubse_cli_ssu_struct.cpp` | ResponsesRejectOversizedLists | TestUbseCliSsuStruct | 已实现 |
| FR-CLI-WIRE-05 | 双源同步 | §14.5 + §14.11 约束 | `ubse_cli_ssu_struct.h:29-30` | AttachSpaceRequestMatchesHandlerFieldOrder | TestUbseCliSsuStruct | 待补 |
| FR-CLI-OUTPUT-01 | 抽象基类 | §14.5 输出格式化 | `ubse_cli_ssu_cmd_reg.cpp` | 摘要表布局校验（间接） | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-OUTPUT-04 | 摘要表 | §14.5 BuildSummaryTable | `ubse_cli_ssu_cmd_reg.cpp:560` | DisplaySummaryPrintsNameSizeStrategy | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-OUTPUT-05 | 详情表 | §14.5 BuildDetailTable | `ubse_cli_ssu_cmd_reg.cpp` | DisplayDetailPrintsDetailTable | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-OUTPUT-06 | 空结果 | §14.5 BuildDetailOutput | `ubse_cli_ssu_cmd_reg.cpp:550` | DisplaySummaryReturnsInfoWhenEmpty | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-OUTPUT-07 | 错误输出 | §14.5 错误文案 | `ubse_cli_ssu_cmd_reg.cpp` | CreateReturnsInternalErrorWithCode | TestUbseCliSsuCmdReg | 已实现 |
| FR-CLI-AUTH-01 | 无显式认证 | §14.7 隐式认证流程 | `ubse_cli_ssu_cmd_reg.cpp` | E2E_Auth_NonAuthorizedUserRejected | E2E | 待补 |
| FR-CLI-AUTH-05 | RBAC 鉴权 | §14.7 + daemon 侧 | daemon | E2E_Auth_NonAuthorizedUserRejected | E2E | 待补 |
| FR-CLI-COVERAGE-01 | 已覆盖 | §14.11.3 避坑指南 | `ubse_cli_ssu_cmd_reg.cpp:339-345` | E2E_Coverage_HelpListsFourCommands | E2E | 待补 |
| NFR-PERF-09 | 单命令响应时间 | §14.10 设计亮点 + 第 7 章 PT | `ubse_cli_ssu_main.cpp` | Perf_DisplaySummary_ResponseTime | TestUbseCliSsuPerf | 待补 |
| NFR-PERF-11 | CLI 启动时间 | §14.10 设计亮点 + 第 7 章 PT | `ubse_cli_ssu_main.cpp` | Perf_CliStartup_ExitTime | TestUbseCliSsuPerf | 待补 |
| NFR-MAINT-07 | 防御性反序列化与双源同步 | §14.5 + §14.8 并发控制 | `ubse_cli_ssu_struct.cpp` | ResponsesRejectOversizedLists | TestUbseCliSsuStruct | 已实现 |
| NFR-MAINT-09 | CLI UT 覆盖 | 全文 | 全部 | 全部 TestSuite | 全部 | 进行中 |
| NFR-SEC-06 | 安全函数 | §14.5 + §14.11 约束 | `ubse_cli_ssu_struct.cpp` | RequestsRejectOverlongStringsAndInvalidEnums | TestUbseCliSsuStruct | 已实现 |

**追溯矩阵汇总**：

| 维度 | 总数 | 已覆盖 | 待补 | 覆盖率 |
|------|------|--------|------|--------|
| FR-CLI-* 功能需求 | 60+ | 30 | 30 | 50% |
| NFR-* 非功能需求（CLI 相关） | 30+ | 5 | 25 | 17% |
| AC-CLI-* 验收标准 | 50+ | 30 | 20 | 60% |
| UT 用例 | 103（规划） | 50 | 53 | 49% |
| IT/E2E 场景 | 25+ | 0 | 25 | 0% |

**说明**：

- FR-CLI-ATTACH/FR-CLI-DETACH 相关用例为待补重点（21 用例，见 5.8.2 E/F），优先级 P1；
- TestUbseCliSsuValidate（14 用例）为待补次重点，直接覆盖 FR-CLI-VALIDATE（见 5.10）；
- IT/E2E 场景在版本发布前补全，覆盖 AC-CLI-* 验收项。
