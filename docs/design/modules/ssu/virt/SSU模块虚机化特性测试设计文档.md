# SSU 模块虚机化特性测试设计文档

> 源码路径:`src/controllers/ssu/`(现有模块,VM 侧扩展)、`src/framework/vscok/`(server/transport 扩展)、`src/cli/ssu/`(CLI 扩展)
> 测试路径:`test/UT/framework/vscok/`、`test/UT/controllers/ssu/`、`test/UT/cli/`、`test/IT/controllers/ssu/`
> 文档版本:V1.1
> 编写日期:2026-08-14
> 更新日期:2026-10-28(特性已随主线落地,本文档对照实现全量校对:补全服务层/传输层/推送 handler/CLI 套件的已实现用例清单,修正错误码、常量真值与 Mock 手法描述)
> 关联文档:[SSU模块虚机化特性需求分析文档](./SSU模块虚机化特性需求分析文档.md)、[SSU模块虚机化特性模块设计文档](./SSU模块虚机化特性模块设计文档.md)

---

## 1. 引言

### 1.1 文档目的

本文档从测试视角描述 SSU 模块虚机化特性(vm_mode,ubse@VM 部署形态)的测试策略、测试分层、用例设计方法、测试场景与验收标准，作为开发、测试与持续集成(CI)的依据。

### 1.2 文档范围

- **覆盖**:UT(vsock 服务层/传输层、VM 缓存、VM 服务分支、模式分流、推送 handler、CLI 命令子集)、IT(真实 AF_VSOCK 端到端推送、TLS 1.3 握手、模块装配)、PT(性能测试)、FUZZ(模糊测试)、E2E(端到端测试)
- **不覆盖**:ub-device-manager 组件的测试(独立外部组件，IT 中仅以 mock client 模拟其角色)、host 侧 SSU 基础能力测试(由 [SSU模块测试设计文档](../SSU模块测试设计文档.md) 覆盖，其中 VFE 直通 `UbseSsuDirectToVmManager` 45 例 UT 亦归主文档)
- **落地状态**:UT 与 IT 已随特性主线落地(共 9 个 UT 套件 138 例 + IT 11 例)；PT/FUZZ/E2E 待补

### 1.3 术语与缩略语

| 术语 | 含义 |
|------|------|
| UT | Unit Test，单元测试 |
| IT | Integration Test，集成测试 |
| PT | Performance Test，性能测试 |
| FUZZ | Fuzzing Test，模糊测试 |
| E2E | End-to-End Test，端到端测试 |
| GTest | Google Test，C++ 单元测试框架 |
| MockCpp | 轻量级 C++ Mock 框架 |
| ASAN | AddressSanitizer，内存错误检测 |
| VM | Virtual Machine，虚拟机 |
| vsock | VM Socket，host↔VM 通信通道(AF_VSOCK) |
| ub-device-manager | host 侧独立组件，通过 vsock 向 VM 推送分配/连接信息(本特性外部，IT 中 mock) |
| vsock_sim | IT 用 LD_PRELOAD 仿真层，将 AF_VSOCK 域重定向到 TCP loopback |

### 1.4 参考资料

- [SSU模块虚机化特性需求分析文档](./SSU模块虚机化特性需求分析文档.md)
- [SSU模块虚机化特性模块设计文档](./SSU模块虚机化特性模块设计文档.md)
- [SSU模块测试设计文档](../SSU模块测试设计文档.md)
- [单元测试开发指南](../../../test/单元测试开发指南.md)
- [AGENTS.md](../../../../../AGENTS.md)

---

## 2. 测试策略

### 2.1 测试金字塔

```
        ┌──────────┐
        │   E2E    │  少量,覆盖直通→推送→查询→attach→detach 完整链路(待补)
        └──────────┘
    ┌──────────────────┐
    │   PT / FUZZ      │  中量,关注性能与健壮性(待补)
    └──────────────────┘
┌──────────────────────────┐
│         IT               │  中量,真实 AF_VSOCK 推送/TLS/模块装配(11 例已实现)
└──────────────────────────┘
┌──────────────────────────────────────────┐
│              UT                          │  大量,服务层/传输层/缓存/服务分支/分流/handler/CLI(138 例已实现)
└──────────────────────────────────────────┘
```

### 2.2 分层测试目标

| 测试层 | 目标 | 执行频率 | 责任人 |
|--------|------|----------|--------|
| UT | VM 侧子模块函数级行为验证，行覆盖率 ≥ 80% | 每次提交 | 开发 |
| IT | 真实 AF_VSOCK 推送→缓存→查询→attach→detach、TLS 1.3 握手、模式装配 | 每日构建 | 开发 + 测试 |
| PT | attach ≤ 500ms、查询 ≤ 100ms、缓存加载 ≤ 50ms | 每版本发布前 | 测试 |
| FUZZ | vsock 消息 fuzz、缓存并发 fuzz | 每版本发布前 | 测试 |
| E2E | 完整直通→推送→查询→attach→detach 链路 | 每版本发布前 | 测试 |

### 2.3 测试通过准则

- UT:100% 通过，新增子模块行覆盖率 ≥ 80%，分支覆盖率 ≥ 70%
- IT:100% 通过，关键集成点全覆盖(推送/全量同步/TLS/模式分流)
- PT:所有性能指标满足 NFR-PERF-01/02/03
- FUZZ:24 小时无崩溃，无 ASAN/UBSAN 报错
- E2E:所有 AC-VM-XX / AC-BOUNDARY-XX 验收项全部通过

### 2.4 测试环境

| 项目 | 要求 |
|------|------|
| 操作系统 | openEuler Linux ARM64 |
| 编译器 | GCC 10+，C++17/C11 |
| 测试框架 | GoogleTest + MockCpp |
| 覆盖率工具 | gcov / lcov |
| 内存检测 | ASAN / UBSAN |
| vsock 环境 | IT 需内核 vsock loopback 支持；无该模块的环境(如部分开发机)通过 `vsock_sim` LD_PRELOAD 仿真层重定向到 TCP loopback 执行，产品代码零修改 |
| 硬件 | Mock 设备(UT/IT)，真实 SSU 设备 + 真实 VM(E2E) |

---

## 3. 测试框架与规范

### 3.1 框架选型

| 工具 | 用途 |
|------|------|
| GoogleTest | `TEST_F`/`TEST`/`EXPECT_*`/`ASSERT_*` |
| MockCpp | `MOCKER(accept4)`/`MOCKER_CPP(&ubse::config::UbseGetBool)`/`stubs().will(invoke(...))` |
| 函数指针注入 | 适配器层 UT 绕过 dlopen:`UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo` |
| CMake | `add_ut()` 宏注册 UT 目标;`add_it_scene(ssu_virt)` 注册 IT 场景(编译选项 `-fno-access-control`) |
| build.sh | `bash build.sh ut` / `bash build.sh it` 封装命令 |

### 3.2 测试代码组织

```
test/UT/
├── framework/vscok/                              # vsock 框架 UT(镜像 src/framework/vscok/)
│   ├── CMakeLists.txt
│   ├── test_ubse_vsock_server.cpp                # 服务层 UT(14 例,已实现)
│   └── test_ubse_vsock_transport.cpp             # 传输层 UT(19 例,已实现)
├── controllers/ssu/                              # SSU 模块 UT(镜像 src/controllers/ssu/)
│   ├── test_ubse_ssu_vm_cache.cpp                # VM 缓存 UT(37 例,已实现)
│   ├── test_ubse_ssu_service_imp_vm.cpp          # VM 服务分支 UT(29 例,已实现)
│   ├── test_ubse_ssu_controller_module_vm_mode.cpp     # 模式分流/依赖裁剪 UT(9 例,已实现)
│   ├── test_ubse_ssu_controller_module_vm_handlers.cpp # vsock 推送 handler UT(15 例,已实现)
│   ├── test_ubse_ssu_controller_module_vm_mock.h # vm_mode/vm_handlers 共享配置 mock
│   ├── test_ubse_ssu_direct_to_vm_manager.cpp    # 现有 VFE 直通 UT(45 例,host 侧,见主文档)
│   └── ...(其余 host 模式既有套件,host 回归用)
└── cli/                                          # CLI UT(镜像 src/cli/ssu/)
    ├── test_ubse_cli_ssu_vm.cpp                  # VM 模式 CLI UT(15 例,已实现)
    ├── test_ubse_cli_ssu_cmd_reg.cpp             # 命令注册 UT(121 例,含 VM 子集场景)
    ├── test_ubse_cli_ssu_mock_invoke.cpp         # 共享 IPC mock
    └── test_ubse_cli_ssu_struct.cpp              # wire 结构 UT

test/IT/controllers/ssu/                          # IT(镜像层级)
├── CMakeLists.txt                                # add_it_scene(ssu_virt),-fno-access-control
├── test_ssu_virt_module_it.cpp                   # VM 模式模块装配 IT(4 例,已实现)
├── test_ssu_virt_push_it.{h,cpp}                 # 端到端推送/TLS IT(7 例,已实现)
├── ssu_virt_it_mock_adapter.{h,cpp}              # IT mock 适配器(替代 libssu.so,函数指针注入)
├── ssu_virt_it_vsock_client.h                    # IT vsock client(模拟 ub-device-manager,真实 AF_VSOCK+TLS)
└── vsock_sim/vsock_sim.cpp                       # LD_PRELOAD 仿真层(AF_VSOCK→TCP loopback,EXCLUDE_FROM_ALL)
```

> IT 运行方式:`run_ssu_virt_it_local` 目标以 `LD_PRELOAD=vsock_sim` 运行；有内核 vsock loopback 模块的环境直接原生执行(push IT 检测 `IsVsockLoopbackAvailable()`，不可用时 `GTEST_SKIP`)。

### 3.3 命名规范

| 类型 | 规范 | 实际示例 |
|------|------|----------|
| TestSuite | `Test<模块名><子模块>` | `TestUbseSsuVmCache`、`TestUbseVsockTransport`、`TestUbseSsuVmHandlers` |
| TestCase | `<方法>_<场景>_<期望>` | `LoadFromDisk_JsonParseError_DeletesFile`、`AttachSpace_VmMode_MidwayFail_RollsBackAttachedNs` |
| 文件 | `test_<被测文件名>.cpp` | `test_ubse_ssu_vm_cache.cpp` |
| 命名空间 | `ubse::<模块>::ut` | `ubse::vsock::ut`、`ubse::ssu::vm_cache::ut`、`ubse::ssu::service::ut`、`ubse::ssu::controller::ut`、`ubse::ut::cli` |

### 3.4 用例注释规范

每个 `TEST_F` 上方必须含三段注释(与项目 UT 开发指南对齐):

```cpp
/*
 * 用例描述:
 * <一句话描述被测行为>
 * 测试步骤:
 * 1、<步骤1>
 * 2、<步骤2>
 * 预期结果:
 * 1、<断言1>
 * 2、<断言2>
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_JsonParseError_DeletesFile) { ... }
```

### 3.5 SetUp/TearDown 规范

- `SetUp()`:构造 Mock 对象、初始化被测对象(如 `SetVmMode(true)` + `SetVmCache(&vmCache_)`)
- `TearDown()`:`GlobalMockObject::verify()` 校验 Mock 期望；恢复 ServiceImp 单例状态(`SetVmCache(nullptr)`/`SetVmMode(false)`)；清理临时文件
- 特殊防护:适配器 UT/IT 的 TearDown 需 `UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr`，防止 `~UbseDlManager` 对句柄 0x1 执行 `dlclose` 段错误
- 缓存 UT:SetUp 用 `mkstemp("/tmp/ssu_vm_cache_ut_XXXXXX")` 生成临时路径，TearDown `Clear()` 并 unlink 临时文件与 `.tmp`

### 3.6 测试数据构造

各测试文件通过工厂方法/helper 集中构造(详见第 10 章)，代表性签名:

```cpp
// test_ubse_ssu_vm_cache.cpp(夹具 protected 静态方法)
static UbseSsuAllocResult MakeAllocResult(const std::string& name, uint64_t size, uint32_t nsCount = 1);
static UbseSsuConnectInfo MakeConnectInfo(const std::string& name, const std::string& tgtEid);
static std::vector<UbseSsuConnectInfo> MakeConnectList(const std::string& name, size_t n);

// test_ubse_ssu_controller_module_vm_handlers.cpp(序列化 helper)
UbseSsuAllocResult MakeAllocResult(const std::string& name);
std::string SerializeAlloc(const UbseSsuAllocResult& ar);
std::string SerializeConnectPush(const std::string& name, const std::vector<UbseSsuConnectInfo>& list);
std::string SerializeFullSync(const std::vector<UbseSsuAllocResult>& allocList,
                              const std::vector<UbseSsuConnectInfo>& connectList);

// test_ubse_cli_ssu_vm.cpp
void WriteConfFile(const std::string& path, const std::string& content);  // 写临时 ubse.conf
void SetVmModeEnv(bool enabled);   // 复用 test_ubse_cli_ssu_cmd_reg.h,设/清 UBSE_SSU_VM_MODE 环境变量
```

---

## 4. 测试场景设计

### 4.1 功能场景覆盖矩阵

| 需求编号 | 测试模块 | TestSuite | 用例数 | 覆盖率目标 |
|----------|----------|-----------|--------|-----------|
| FR-VM-DEPLOY | 模式分流/依赖裁剪 | TestUbseSsuControllerModuleVmMode | 9(已实现) | ≥ 85% |
| FR-VM-VSOCK-SERVER | vsock 服务层 | TestUbseVsockServer | 14(已实现) | ≥ 85% |
| FR-VM-VSOCK-SERVER | vsock 传输层 | TestUbseVsockTransport | 19(已实现) | ≥ 85% |
| FR-VM-VSOCK-SERVER | 推送 handler | TestUbseSsuVmHandlers | 15(已实现) | ≥ 85% |
| FR-VM-QUERY | VM 缓存 + 服务分支 | TestUbseSsuVmCache + TestUbseSsuServiceImpVm | 37 + 29(已实现,查询子集) | ≥ 85% |
| FR-VM-ATTACH | 服务分支 | TestUbseSsuServiceImpVm | 29(已实现,attach 子集) | ≥ 85% |
| FR-VM-DETACH | 服务分支 | TestUbseSsuServiceImpVm | 29(已实现,detach 子集) | ≥ 85% |
| FR-VM-CLI | CLI 命令子集 | TestUbseCliSsuVm + TestUbseCliSsuCmdReg | 15 + 121(已实现,含 VM 场景) | ≥ 80% |
| FR-VM-LIFECYCLE | 模式分流 + 缓存 + handler | TestUbseSsuControllerModuleVmMode + TestUbseSsuVmCache + TestUbseSsuVmHandlers | 跨套件(已实现) | ≥ 80% |
| FR-VM-CACHE-PERSIST | 缓存持久化 | TestUbseSsuVmCache | 37(已实现,持久化子集 16 例) | ≥ 85% |
| FR-BOUNDARY-01/02 | host 模式向后兼容 | TestUbseSsuControllerModuleVmMode + host 套件回归 | 9 + 全量回归(已实现) | ≥ 80% |
| FR-DEVICE-PASSTHROUGH | VFE 直通(host 侧,零修改) | [SSU模块测试设计文档](../SSU模块测试设计文档.md) §5(DirectToVmManager 45 例) | 45(已实现,归主文档) | ≥ 85% |
| NFR-PERF-01/02/03 | 性能 | PT(待补) | — | — |
| NFR-REL-01~04 | 可靠性 | TestUbseVsockTransport + TestUbseSsuVmCache(断连/损坏恢复子集) | 跨套件(已实现) | ≥ 80% |
| NFR-SEC-01 | TLS 1.3 | TestSsuVirtTls(IT) + TestUbseVsockTransport(配置分支) | 1 + 4(已实现) | — |
| NFR-SEC-02/03 | 身份验证/文件权限 | TestUbseSsuServiceImpVm + TestUbseSsuVmCache | 跨套件(已实现) | ≥ 85% |
| NFR-COMPAT-01 | host 模式兼容 | host 套件全量回归 | 全量回归(已实现) | — |

### 4.2 测试用例设计方法

| 方法 | 适用场景 | 示例 |
|------|----------|------|
| 等价类划分 | 参数取值域 | `vm_mode`=true/false 两种等价类;`IsVmMode` 环境变量"1"/非"1"/未设置 |
| 边界值分析 | 边界条件 | name 长度 0/47/48;`MAX_PUSH_CONNECT_INFO_NUM` 4095/4096;全量同步 8192 |
| 状态迁移 | 状态机 | vsock 连接 WAITING_CONNECT→CONNECTED→DISCONNECTED→WAITING_CONNECT |
| 决策表 | 多条件组合 | vm_mode × 接口 × 预期行为;`enable_tls` × certDir 有效性 |
| 错误猜测 | 异常路径 | 缓存文件损坏/缺字段/版本不匹配、反序列化中途失败、临时文件残留、open 失败 |
| 因果图 | 因果关系 | 推送成功 → 缓存更新 → 查询命中 → attach 可见 |
| 往返一致 | 序列化对等 | `SerializeFullSync` 字节流与 handler 反序列化字段顺序一致;CLI `ParseSsuVmModeConf` 解析规则与 daemon 对齐 |

---

## 5. 单元测试设计

### 5.1 UbseSsuVmCache 测试(TestUbseSsuVmCache,37 例已实现)

#### 5.1.1 测试目标

验证 `UbseSsuVmCache` 的分配/连接信息缓存读写、attach 状态管理、并发安全、持久化加载/落盘(含权限与原子写)，覆盖需求 FR-VM-QUERY、FR-VM-CACHE-PERSIST、FR-VM-LIFECYCLE。

#### 5.1.2 测试场景

**A. 分配信息缓存(UpdateAllocInfo / GetAllocInfo / ListAllocInfo)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| UpdateAllocInfo_Single_Success | 推送单条分配信息 | 缓存更新成功 |
| UpdateAllocInfo_Duplicate_Overwrite | 重复推送同名 namespace | 覆盖更新，无异常 |
| GetAllocInfo_Exists_ReturnsResult | 查询已存在的 namespace | 返回正确分配信息 |
| GetAllocInfo_NotExists_ReturnsError | 查询不存在的 namespace | 返回 `UBSE_SSU_ERROR_NS_NOT_FOUND` |
| ListAllocInfo_Empty_ReturnsEmpty | 空缓存查询 | 返回空列表 |
| ListAllocInfo_Multiple_ReturnsAll | 多条分配信息查询 | 返回全部 |

**B. 连接信息缓存(UpdateConnectInfo / GetConnectInfo)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| UpdateConnectInfo_Single_Success | 推送单条连接信息 | 缓存更新成功 |
| UpdateConnectInfo_Duplicate_Overwrite | 重复推送同名连接信息 | 覆盖更新 |
| GetConnectInfo_Exists_ReturnsList | 查询已存在的连接信息 | 返回正确列表 |
| GetConnectInfo_NotExists_ReturnsError | 查询不存在的 namespace | 返回错误码 |

**C. Attach/Detach 状态管理(MarkAttached / MarkDetached / IsAttached)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| MarkAttached_NotAttached_Success | 标记未 attach 的 namespace | 返回 true,IsAttached 返回 true |
| MarkAttached_AlreadyAttached_Idempotent | 重复标记已 attach | 返回 true(幂等) |
| MarkDetached_Attached_Success | 标记已 attach 为 detach | 返回 true,IsAttached 返回 false |
| MarkDetached_NotAttached_Idempotent | detach 未 attach 的 namespace | 返回 true(幂等) |
| IsAttached_NotInCache_ReturnsFalse | 查询不在缓存中的 namespace | 返回 false |

**D. 持久化加载(LoadFromDisk)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| LoadFromDisk_FileNotExists_ReturnsFalse | 缓存文件不存在 | 返回 false,缓存为空 |
| LoadFromDisk_ValidFile_ReturnsTrue | 合法缓存文件 | 返回 true,缓存正确填充 |
| LoadFromDisk_JsonParseError_DeletesFile | JSON 解析失败(损坏) | 删除损坏文件，返回 false,记录告警 |
| LoadFromDisk_VersionMismatch_DeletesFile | 文件 version 不匹配 | 删除文件，返回 false |
| LoadFromDisk_MissingRequiredField_PreservesMemoryCache | 缺必填字段 | 返回 false,不破坏内存缓存 |
| LoadFromDisk_MissingNestedField_DeletesFile | 嵌套字段缺失 | 删除文件，返回 false |
| LoadFromDisk_InvalidStrategy_DeletesFile | strategy 枚举非法 | 删除文件，返回 false |
| LoadFromDisk_ValidFile_ClearsAttachedState | 加载合法文件 | attachedNames 清空(不持久化语义) |
| LoadFromDisk_EmptyFile_ReturnsFalse | 空文件 | 返回 false |

**E. 持久化落盘(FlushToDisk)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| FlushToDisk_Normal_Success | 正常落盘 | 文件内容与内存一致 |
| FlushToDisk_EmptyCache_WriteEmptyFile | 空缓存落盘 | 文件写入空列表 |
| FlushToDisk_WritesDocumentedSchema | 落盘 schema | 文件含 version/lastSyncTs/allocList/connectList 四字段 |
| FlushToDisk_AttachedNamesNotInFile | 落盘验证 | 文件不含 attachedNames |
| FlushToDisk_SetsOwnerReadWriteOnlyPermission | 文件权限 | 权限 0600(NFR-SEC-03) |
| FlushToDisk_ExistingTmpFileResetsPermission | 残留 `.tmp` 文件 | 写入前重置权限为 0600 |
| FlushToDisk_OpenFailure_ReturnsFalse | 文件 open 失败 | 返回 false,不崩溃 |
| FlushToDisk_MissingParentDir_AutoCreated | 父目录缺失 | 自动创建目录后落盘成功 |

**F. 全量与并发(Clear / FullSync / 并发)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Clear_AfterPopulated_AllMapsEmpty | 清空已填充缓存 | 全部缓存容器为空 |
| Clear_DuringRead_QueryReturnsEmpty | 清空期间查询 | 返回空(短暂窗口) |
| FullSync_PopulateFromEmpty_Success | 全量同步重建 | 数据原子替换正确填充 |
| UpdateAllocInfo_ConcurrentReadWrite_NoRace | 并发读写 | 无数据竞争(`shared_mutex`) |
| FlushToDisk_Concurrent_Serialized | 并发落盘 | 串行化，文件内容一致 |

#### 5.1.3 测试夹具

```cpp
class TestUbseSsuVmCache : public testing::Test {
public:
    void SetUp() override;    // mkstemp 临时路径 + SetCacheFilePath
    void TearDown() override; // Clear() + unlink 临时文件与 .tmp
protected:
    static UbseSsuAllocResult MakeAllocResult(const std::string& name, uint64_t size, uint32_t nsCount = 1);
    static UbseSsuConnectInfo MakeConnectInfo(const std::string& name, const std::string& tgtEid);
    UbseSsuVmCache cache_;
};
```

#### 5.1.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| (无) | - | `UbseSsuVmCache` 为纯内存数据结构 + 文件 IO,无外部依赖;文件用 `/tmp` 临时路径 |

---

### 5.2 UbseVsockServer 测试(TestUbseVsockServer,14 例已实现)

#### 5.2.1 测试目标

验证 `ubse::vsock::UbseVsockServer` 服务层的配置校验、handler 注册、请求路由、响应发送与启动/停止，覆盖需求 FR-VM-VSOCK-SERVER。服务层不感知传输细节:真实 AF_VSOCK 连接与 TLS 握手由 IT 层覆盖，UT 中 TLS 仅走配置校验分支。

#### 5.2.2 测试场景

**A. 配置校验(UbseVsockConfig::IsValid)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Config_DefaultIsValid | 默认配置(port=6174,tls=true,certDir 默认) | 校验通过 |
| Config_ZeroPort_IsInvalid | port=0 | 校验失败 |
| Config_TlsEnabledEmptyCertDir_IsInvalid | tls=true 且 certDir 为空 | 校验失败 |
| Config_TlsDisabledEmptyCertDir_IsValid | tls=false 且 certDir 为空 | 校验通过 |

**B. handler 注册(RegisterHandler)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| RegisterHandler_Valid_ReturnsOk | 注册合法 handler | 成功 |
| RegisterHandler_Duplicate_ReturnsExisted | 重复注册同一 opCode | 返回已存在 |
| RegisterHandler_NullHandler_ReturnsInvalidArg | 空 handler | 返回 `UBSE_ERR_INVALID_ARG` |
| RegisterHandler_ConcurrentDifferentHandlers_AllRegistered | 并发注册不同 opCode | 全部注册成功 |

**C. 请求路由与响应(HandleRequest / SendResponse / Start / Stop)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| HandleRequest_RoutesToRegisteredHandler | 请求路由到已注册 handler | handler 被调用,收到正确 body |
| HandleRequest_UnregisteredOp_HandlerNotCalled | 未注册的 opCode | handler 不被调用 |
| SendResponse_NotStarted_ReturnsError | 未 Start 时发响应 | 返回错误 |
| Start_InvalidPort_ReturnsInvalidArg | 非法端口 Start | 返回 `UBSE_ERR_INVALID_ARG` |
| Stop_NotStarted_NoCrash | 未启动时 Stop | 无异常 |
| GetConnectionStatus_InitialState_ReturnsWaitingConnect | 初始状态 | 返回 WAITING_CONNECT |

#### 5.2.3 测试夹具

```cpp
// namespace ubse::vsock::ut
class TestUbseVsockServer : public testing::Test {
public:
    void SetUp() override;   // config{port=6174, enableTls=false} + new UbseVsockServer
    void TearDown() override; // Stop()
};
// 匿名 helper:MakeRequestBytes(moduleCode, opCode, body, clientRequestId) 拼
// UbseRequestHeader+body 字节流;常量 TEST_MODULE_CODE=0x0007、TEST_OP_CODE=0x0100
```

#### 5.2.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| (无) | - | 服务层 UT 不做 socket 桩,纯逻辑路由测试;socketpair 伪造 fd 的手法见 5.3 传输层 |

---

### 5.3 UbseVsockTransport 测试(TestUbseVsockTransport,19 例已实现)

#### 5.3.1 测试目标

验证 `ubse::vsock::UbseVsockTransport` 传输层的粘包/半包编解码、会话管理、响应写入、线程池提交与连接状态迁移，覆盖需求 FR-VM-VSOCK-SERVER、NFR-REL-01。UT 以 `socketpair(AF_UNIX)` 端点伪造已 accept 的 fd,配合测试构建选项 `-fno-access-control` 直接注入私有成员(`running_`/`sessions_`/`handshakeExecutor_`/`threadPool_`),不做真实 TLS 握手(仅测证书目录缺失错误路径)。

#### 5.3.2 测试场景

**A. 生命周期(Start / Stop / 事件循环)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Start_TlsCertDirMissing_ReturnsInternal | TLS 开启但证书目录不存在 | 返回 `UBSE_ERR_INTERNAL` |
| Stop_QueuedTlsHandshake_ClosesSocket | Stop 时排队中的 TLS 握手 | socket 被关闭(以 fcntl==EBADF 断言) |
| EventLoop_FatalError_SelfStopsAndStopCleansUp | 事件循环致命错误 | 自行停止，Stop 清理无泄漏 |

**B. 编解码(TryDecodeMessage,粘包/半包)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| TryDecodeMessage_FullPacket_SubmitsToPool | 完整报文 | 提交线程池 |
| TryDecodeMessage_HalfHeader_Waits | 半个协议头 | 等待更多数据 |
| TryDecodeMessage_BodyIncomplete_Waits | body 不完整 | 等待更多数据 |
| TryDecodeMessage_OversizedBodyLen_ClosesConnection | 超长 body len(恶意 size) | 关闭连接,防内存放大 |
| TryDecodeMessage_ZeroBodyLen_CreatesRequest | body len=0 | 创建空 body 请求 |
| TryDecodeMessage_MultipleMessages_AllDecoded | 粘包多消息 | 全部解码 |
| HandleAccept_TlsQueueFull_ClosesSocket | TLS 握手队列满 | 关闭连接 |

**C. 响应发送(SendResponse / HandleWrite)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| SendResponse_SessionExists_AppendsWriteBuffer | 会话存在 | 响应追加写缓冲 |
| SendResponse_UnknownRequestId_ReturnsError | 未知 clientRequestId | 返回错误 |
| SendResponse_SessionGone_ReturnsErrorAndCleansMap | 会话已销毁 | 返回错误并清理映射 |
| HandleWrite_PeerClosed_ClosesConnection | 对端关闭时写 | 关闭连接 |

**D. 读取与线程池(HandleRead / SubmitToThreadPool)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| SubmitToThreadPool_QueueFull_WritesErrorResponse | 线程池队列满 | 写错误响应 |
| HandleRead_PendingWrite_RetriedAfterRead | 读事件期间有待写数据 | 读取后重试写入 |
| HandleRead_PeerCloseAfterFullMessage_MessageSubmitted | 读完整消息后对端关闭 | 消息仍提交处理 |

**E. 连接状态迁移(NFR-REL-01)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| CloseConnection_LastSession_TransitionsToWaitingConnect | 最后一个会话关闭 | 状态迁回 WAITING_CONNECT |
| CloseConnection_NotLastSession_KeepsConnected | 非最后会话关闭 | 保持 CONNECTED |

#### 5.3.3 测试夹具

```cpp
// namespace ubse::vsock::ut
class TestUbseVsockTransport : public testing::Test {
public:
    void SetUp() override; // config{enableTls=false}
};
// 手法:socketpair(AF_UNIX) 伪造 fd;MOCKER(accept4).stubs().will(returnValue(fds[0]))
//   .then(invoke(AcceptWouldBlock))(AcceptWouldBlock 设 errno=EAGAIN 返回 -1);
// -fno-access-control 注入 transport.running_/sessions_/handshakeExecutor_/threadPool_
```

#### 5.3.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `accept4`(libc) | `MOCKER(accept4)` | 模拟 accept 返回 socketpair 端点/阻塞(EAGAIN) |
| SSL 上下文 | 不 mock,走错误路径 | 证书目录缺失(`/nonexistent_vsock_cert_dir_for_ut`)返回 `UBSE_ERR_INTERNAL`;真实握手由 IT 覆盖 |

---

### 5.4 UbseSsuServiceImp VM 模式测试(TestUbseSsuServiceImpVm,29 例已实现)

#### 5.4.1 测试目标

验证 `UbseSsuServiceImp` 在 VM 模式(`vmMode_=true`)下的查询/attach/detach 分支(含聚合与回滚)、身份校验，以及不支持接口的正确拒绝，覆盖需求 FR-VM-QUERY、FR-VM-ATTACH、FR-VM-DETACH、NFR-SEC-02。

#### 5.4.2 测试场景

**A. 查询接口(VM 模式走缓存)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ListAllocInfo_VmMode_ReturnsCacheData | VM 模式查询 | 返回缓存中的分配信息 |
| ListAllocInfo_VmMode_EmptyCache_ReturnsEmpty | 空缓存查询 | 返回空列表(AC-VM-QUERY-04) |
| GetAllocInfoByName_VmMode_Exists_ReturnsResult | 按名查询已存在 | 从缓存返回正确结果 |
| GetAllocInfoByName_VmMode_NotExists_ReturnsError | 查询不存在 | 返回 `UBSE_SSU_ERROR_NS_NOT_FOUND` |
| GetAllocInfoByName_VmMode_IdentityMismatch_ReturnsDenied | 身份不匹配 | 返回 `UBSE_ERR_ACCESS_DENIED`(NFR-SEC-02) |
| GetConnectInfo_VmMode_ReturnsCacheData | 连接信息查询 | 从缓存返回 |
| GetConnectInfo_VmMode_IdentityMismatch_ReturnsDenied | 身份不匹配 | 返回 `UBSE_ERR_ACCESS_DENIED` |
| GetNsStats_VmMode_ReturnsAdapterResult | NS 统计 | 走本地适配器 |
| GetNsStats_VmMode_NotInCache_ReturnsNotFound | 统计不在缓存的 ns | 返回 `UBSE_SSU_ERROR_SPACE_NOT_FOUND`(注意与 attach 的 NS_NOT_FOUND 不同) |

**B. Attach 接口(VM 模式走本地适配器)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| AttachSpace_VmMode_NotInCache_ReturnsNotFound | attach 缓存中不存在的 namespace | 返回 `UBSE_SSU_ERROR_NS_NOT_FOUND`(AC-VM-ATTACH-03) |
| AttachSpace_VmMode_Success_ReturnsDevPath | attach 成功 | 返回设备路径,MarkAttached 调用 |
| AttachSpace_VmMode_AlreadyAttached_Idempotent | 重复 attach | 返回成功(幂等,AC-VM-ATTACH-02) |
| AttachSpace_VmMode_MidwayFail_RollsBackAttachedNs | 多 ns attach 中途失败 | 逆序 Detach 回滚已 attach 的 ns |
| AttachLinearSpace_VmMode_Success_PassesOptions | 线性聚合 attach | 返回聚合设备路径,参数透传 |
| AttachStripedSpace_VmMode_Success_PassesOptions | 条带化聚合 attach | 返回聚合设备路径,参数透传 |
| AttachStripedSpace_VmMode_RAID0_PassesChunkSize | RAID0 chunkSize | chunkSize 正确透传 |
| AttachStripedSpace_VmMode_AlreadyAttached_Idempotent_NoRecreate | 重复条带 attach | 幂等,不重建块设备 |

**C. Detach 接口(VM 模式走本地适配器)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| DetachSpace_VmMode_Attached_Success | detach 已 attach | 返回成功,MarkDetached 调用 |
| DetachSpace_VmMode_NotAttached_Idempotent | detach 未 attach | 返回成功(幂等,AC-VM-DETACH-03) |
| DetachLinearSpace_VmMode_Success | 线性聚合 detach | 块设备删除 + VmDetachSpace |
| DetachLinearSpace_VmMode_DeleteBlockDeviceFail_NoNsDetach | 块设备删除失败 | ns 不被 detach(状态一致) |
| DetachStripedSpace_VmMode_Success | 条带化聚合 detach | 返回成功 |

**D. 不支持接口(VM 模式拒绝)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| AllocSpace_VmMode_ReturnsNotSupported | VM 模式 alloc | 返回 `UBSE_ERR_NOT_SUPPORTED` |
| FreeSpace_VmMode_ReturnsNotSupported | VM 模式 free | 返回 `UBSE_ERR_NOT_SUPPORTED` |
| AddAccessPermission_VmMode_ReturnsNotSupported | 添加权限 | 返回 `UBSE_ERR_NOT_SUPPORTED` |
| RemoveAccessPermission_VmMode_ReturnsNotSupported | 移除权限 | 返回 `UBSE_ERR_NOT_SUPPORTED` |
| GetFeDeviceList_VmMode_ReturnsNotSupported | 获取 FE 列表 | 返回 `UBSE_ERR_NOT_SUPPORTED` |
| FeDeviceAlloc_VmMode_ReturnsNotSupported | FeDeviceAlloc | 返回 `UBSE_ERR_NOT_SUPPORTED` |

**E. host 模式行为不变(NFR-COMPAT-01)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ListAllocInfo_HostMode_NotAffected | host 模式查询 | 行为与特性启用前一致 |

#### 5.4.3 测试夹具

```cpp
// namespace ubse::ssu::service::ut,复用 host 模式基础设施
class TestUbseSsuServiceImpVm : public UbseSsuServiceImpTestBase {
public:
    void SetUp() override;
    // 基类 SetUp → SetupAdapterFuncs() → ResetControllableMockState()
    //   → service_.SetVmMode(true) + service_.SetVmCache(&vmCache_)
    void TearDown() override;
    // SetVmCache(nullptr)/SetVmMode(false)/vmCache_.Clear()
    //   + UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr(防 dlclose(0x1) 段错误)
protected:
    static UbseSsuAllocResult MakeVmAllocResult(const std::string& name, uint32_t nsCount = 1);
    static UbseSsuConnectInfo MakeVmConnectInfo(const std::string& name, const std::string& tgtEid);
};
```

#### 5.4.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `UbseSsuAdapterImpl`(函数指针注入) | `acquireDevInfo_` 等函数指针 | 绕过 dlopen 模拟 VM 内适配器(`SetupAdapterDevQueryMock`) |
| `ubse::config::UbseGetStr` | `MOCKER_CPP` | 模拟配置读取 |
| 适配器 customData | uid=100/userName="test_user" | 身份校验(NFR-SEC-02)注入 |

---

### 5.5 UbseSsuControllerModule vm_mode 分流测试(TestUbseSsuControllerModuleVmMode,9 例已实现)

#### 5.5.1 测试目标

验证 `UbseSsuControllerModule` 在 `vm_mode` 配置下的依赖裁剪(插件注册期)与初始化分流,覆盖需求 FR-VM-DEPLOY、FR-BOUNDARY-01/02。本套件不验证 vsock server 与缓存的具体实现(分别由 5.2/5.3/5.1 覆盖)。

#### 5.5.2 测试场景

**A. 依赖裁剪(GetSsuModuleDependencies,插件注册期)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ModuleDependencies_VmMode_AreEmpty | VM 模式依赖集 | 为空(不加载 node/election/com/vip 等,AC-VM-DEPLOY-01) |
| ModuleDependencies_HostMode_PreserveExistingDependencies | host 模式依赖集 | 与现有行为一致 |
| ModuleDependencies_ConfigReadFails_PreserveExistingDependencies | 配置读取失败 | 回落 host 模式依赖集 |

**B. 初始化与分流(Initialize)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Init_VmMode_SetsVmModeTrue | VM 模式初始化 | ServiceImp `SetVmMode(true)` 被调用 |
| Init_VmMode_DefaultConfig_Succeeds | 默认配置(端口 6174/tls 默认) | 初始化成功 |
| Init_VmMode_HandlerRegistrationFails_RollsBackServiceState | handler 注册失败 | 回滚 Service 状态 |
| Init_HostMode_BehaviorUnchanged | host 模式初始化 | 现有行为不变 |

**C. vsock 配置与清理**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| BuildVsockConfig_UsesSingleRequestWorker | vsock 配置构建 | 请求工作线程按单线程构建 |
| UnInitialize_VmMode_ClearsServiceState | 反初始化 | Service 状态清理 |

#### 5.5.3 测试夹具

```cpp
// namespace ubse::ssu::controller::ut,共享 mock 见 test_ubse_ssu_controller_module_vm_mock.h
class TestUbseSsuControllerModuleVmMode : public testing::Test {
public:
    void SetUp() override;    // 复位 g_* 全局态 + SetupVmConfigMocks()
    void TearDown() override; // GlobalMockObject::verify() + 恢复 ServiceImp 单例
protected:
    void SetConfigVmMode(bool enabled); // VM 模式下 enable_tls 同步置 true
};
```

#### 5.5.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `ubse::config::UbseGetBool/GetStr/GetUInt` | `MOCKER_CPP`(经 `SetupVmConfigMocks()`) | `vm_mode`/`enable_tls`/`vsockServerPort` 配置模拟(inline 全局态 `g_vmModeForTest`/`g_enableTlsForTest`/`g_vmModeConfigRetForTest` 控制) |

---

### 5.6 vsock 推送 handler 测试(TestUbseSsuVmHandlers,15 例已实现)

#### 5.6.1 测试目标

验证 `UbseSsuControllerModule` 的三个 vsock 推送 handler(HandlePushAllocInfo/HandlePushConnectInfo/HandleFullSync)对合法/非法载荷的处理与缓存更新语义，覆盖需求 FR-VM-VSOCK-SERVER、FR-VM-LIFECYCLE、NFR-REL-02。超限或反序列化失败统一不更新缓存(框架回 `UBSE_ERR_INVALID_ARG` 错误响应)，防恶意 size 字段内存放大。

#### 5.6.2 测试场景

**A. 单条分配信息推送(HandlePushAllocInfo,opCode 0x0100)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| HandlePushAllocInfo_ValidPayload_UpdatesCache | 合法载荷 | 缓存更新(AC-VM-VSOCK-04) |
| HandlePushAllocInfo_EmptyPayload_NotUpdate | 空 payload | 不更新 |
| HandlePushAllocInfo_NameEmpty_NotUpdate | name 为空 | 不更新 |
| HandlePushAllocInfo_DeserializeFailed_NotUpdate | 反序列化失败 | 不更新 |

**B. 连接信息推送(HandlePushConnectInfo,opCode 0x0101)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| HandlePushConnectInfo_ValidPayload_UpdatesCache | 合法载荷 | 缓存更新 |
| HandlePushConnectInfo_CountExceedsLimit_NotUpdate | 条数超 `MAX_PUSH_CONNECT_INFO_NUM`(4096) | 不更新,防内存放大 |
| HandlePushConnectInfo_EmptyPayload_NotUpdate | 空 payload | 不更新 |
| HandlePushConnectInfo_NameEmpty_NotUpdate | name 为空 | 不更新 |
| HandlePushConnectInfo_DeserializeMidFail_NotUpdate | 中途反序列化失败 | 不更新 |

**C. 全量同步(HandleFullSync,opCode 0x0105)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| HandleFullSync_ValidPayload_ClearsAndRebuilds | 合法全量同步 | 清空缓存原子重建(NFR-REL-02) |
| HandleFullSync_EmptyPayload_NotClear | 空 payload | 不清空 |
| HandleFullSync_AllocCountExceedsLimit_NotClear | alloc 条数超 8192 | 不清空 |
| HandleFullSync_DeserializeMidFail_NotClear | 中途反序列化失败 | 不清空 |
| HandleFullSync_AllocNameEmpty_NotClear | alloc name 为空 | 不清空 |
| HandleFullSync_ConnectNameEmpty_NotClear | connect name 为空 | 不清空 |

#### 5.6.3 测试夹具

```cpp
// namespace ubse::ssu::controller::ut
class TestUbseSsuVmHandlers : public testing::Test {
public:
    void SetUp() override;    // g_vmModeForTest=true + SetupVmConfigMocks() + module_.Initialize()
    void TearDown() override; // UnInitialize + GlobalMockObject::verify() + 恢复 ServiceImp
};
// 私有 handler 经 CallHandlePushAllocInfo/CallHandlePushConnectInfo/CallHandleFullSync
// 封装调用(-fno-access-control);以 module_.vmCache_ 状态验证分支
// (server 未 Start 时 SendResponse 返回 UNREACHABLE,不断言响应)
```

#### 5.6.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `ubse::config::UbseGetBool/GetStr/GetUInt` | `MOCKER_CPP`(经 `SetupVmConfigMocks()`) | 配置模拟 |
| 序列化 helper | `SerializeAlloc`/`SerializeConnectPush`/`SerializeFullSync` | 按 UbseSerialization 协议格式构造载荷 |

---

### 5.7 CLI VM 模式测试(TestUbseCliSsuVm,15 例已实现)

#### 5.7.1 测试目标

验证 `ubse_cli_ssu_cmd_reg` 的 `IsVmMode` 判定(环境变量 `UBSE_SSU_VM_MODE` 优先,配置文件 `/etc/ubse/ubse.conf` 回落)与 VM 模式命令注册子集(display/attach/detach,含 connect_info 类型)，覆盖需求 FR-VM-CLI。

#### 5.7.2 测试场景

**A. VM 模式命令注册**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| RegCmd_VmMode_IncludesDisplay | VM 模式注册 | `display ssu` 注册(含 connect_info 类型) |
| RegCmd_VmMode_IncludesAttach | VM 模式注册 | `attach ssu` 注册 |
| RegCmd_VmMode_IncludesDetach | VM 模式注册 | `detach ssu` 注册 |
| RegCmd_VmMode_ExcludesCreate | VM 模式注册 | `create ssu` 不注册(AC-VM-CLI-05) |
| RegCmd_VmMode_ExcludesDelete | VM 模式注册 | `delete ssu` 不注册 |
| RegCmd_VmMode_CommandCountIs3 | VM 模式命令数 | 恰为 3 条 |

**B. host 模式向后兼容(AC-VM-CLI-06)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| RegCmd_HostMode_IncludesCreate | host 模式注册 | `create ssu` 注册 |
| RegCmd_HostMode_IncludesDelete | host 模式注册 | `delete ssu` 注册 |
| RegCmd_HostMode_CommandCountIs5 | host 模式命令数 | 恰为 5 条(display/attach/detach/create/delete) |

**C. vm_mode 判定(IsVmMode,环境变量优先)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| VmModeConf_True_ReturnsVmMode | 配置文件 vm_mode=true | 返回 VM 模式 |
| VmModeConf_False_ReturnsHostMode | 配置文件 vm_mode=false | 返回 host 模式 |
| VmModeConf_MissingFile_FallsBackToHost | 配置文件不存在 | 回落 host 模式 |
| VmModeEnv_OverridesConf | 环境变量与配置同时存在 | 环境变量优先 |

**D. 配置解析与 daemon 对齐**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ParseSsuVmModeConf_ValueSemantics_MatchesDaemon | 值语义 | 与 daemon 侧解析一致(仅字面量 "true" 视为 VM 模式) |
| ParseSsuVmModeConf_ParserRules_MatchDaemon | 解析规则 | trim/注释跳过/同键后值覆盖规则与 daemon UbseConfigManager 对齐 |

#### 5.7.3 测试夹具

```cpp
// namespace ubse::ut::cli,复用 test_ubse_cli_ssu_cmd_reg.h 基类
class TestUbseCliSsuVm : public TestUbseCliSsuCmdReg {
public:
    void SetUp() override;    // UnsetVmModeEnv() + RemoveConfFile()
    void TearDown() override; // 同 SetUp 清理
protected:
    static void WriteConfFile(const std::string& path, const std::string& content);
    static bool HasCommand(const std::vector<...>& cmds, const std::string& command);
};
// 被测入口:UbseCliRegSsuModule::UbseCliSignUp();不验证 ubse_invoke_call IPC 路径
```

#### 5.7.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| 环境变量 `UBSE_SSU_VM_MODE` | `SetVmModeEnv(bool)` | IsVmMode 环境变量优先级验证 |
| 临时配置文件 | `WriteConfFile` | IsVmMode 配置文件回落验证 |

---

## 6. 集成测试设计

### 6.1 测试目标

验证真实 AF_VSOCK 通道上的推送→缓存更新→查询→attach→detach 端到端链路、TLS 1.3 握手，以及 VM/host 模式装配与 IPC handler 注册子集。共 11 例已实现。

### 6.2 测试场景

#### 6.2.1 VM 模式模块装配(TestSsuVirtModuleMode,4 例已实现,`test_ssu_virt_module_it.cpp`)

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ModuleInit_Start_VmMode | VM 模式完整初始化→启动 | 所有 VM 组件就绪(缓存/vsock/handler) |
| ModuleInit_Start_HostMode | host 模式完整初始化→启动 | host 组件就绪,无 VM 组件 |
| RegisterIpc_VmMode_Subset | VM 模式 IPC 注册 | 仅查询/attach/detach handler 子集注册(10/17) |
| RegisterIpc_HostMode_FullSet | host 模式 IPC 注册 | 全量 handler 注册 |

> 夹具成员:apiServer_/taskExecutorModule_/module_/cacheFilePath_;helper `RegisteredSsuOpCodes` 收集 ApiServer 预注册队列中的 SSU opCode 集合;配置经 `SetupModuleConfigMocks()` 注入，缓存文件用 `/tmp/ssu_virt_module_it_<pid>.json`。

#### 6.2.2 vsock 端到端推送(TestSsuVirtPush,6 例已实现,`test_ssu_virt_push_it.cpp`)

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| PushAllocInfo_FullCycle | 推送分配信息→接收→查询 | 全链路通过,ACK 正常 |
| PushConnectInfo_FullCycle | 推送连接信息→接收→查询 | 全链路通过 |
| PushFullSync_AfterCacheClear | 全量同步后缓存重建 | 缓存数据与推送一致 |
| PushMultipleMsgs_Sequential | 连续推送多条消息 | 全部 ACK,缓存逐条更新 |
| PushDuringAttach_NoConflict | 推送与 attach 并发 | 无冲突，attach 成功后推送覆盖 |
| PushInvalidPayload_Nack_CacheUnchanged | 非法载荷推送 | 错误响应，缓存不变 |

> IT 环境说明:真实 `UbseVsockServer`(TLS 关、threadPoolSize=1、QueueSize=256)，端口 `IT_VSOCK_PORT_BASE + pid%512*IT_MAX_PORT_ATTEMPTS + attempt` 冲突自动后移;对端 `ItVsockPushClient`(`ssu_virt_it_vsock_client.h`)以真实 AF_VSOCK socket + 线路协议编解码模拟 ub-device-manager;适配器由 `ssu_virt_it_mock_adapter` 函数指针注入(uid=2000/userName="it_user")；vmFlushExecutor_ 单线程("itSsuVmFlush")验证异步落盘。无内核 vsock loopback 模块的环境通过 `vsock_sim` LD_PRELOAD 重定向到 TCP loopback。

#### 6.2.3 TLS 握手(TestSsuVirtTls,1 例已实现,`test_ssu_virt_push_it.cpp` 裸 TEST)

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| TlsHandshake_PushAllocInfo_Ack | TLS 1.3 双向认证(用例自签 trust.pem/client.pem/client_key.pem) | 握手成功，推送→ACK 正常(NFR-SEC-01) |

#### 6.2.4 host 模式向后兼容集成

host 模式功能由既有 host 套件全量回归覆盖(`test_ubse_ssu_service_imp_*.cpp`、`test_ubse_ssu_scheduler.cpp` 54 例、DirectToVmManager 45 例等);`ModuleInit_Start_HostMode`/`RegisterIpc_HostMode_FullSet` 已覆盖装配层兼容性。

---

## 7. 性能测试设计

### 7.1 测试目标

验证满足 NFR-PERF-01(VM 内 attach ≤ 500ms)、NFR-PERF-02(VM 内查询 ≤ 100ms)、NFR-PERF-03(缓存文件加载 ≤ 50ms，典型 ≤ 100 条 namespace)。另按需求文档 §5.2 验证 vsock 接收吞吐 ≥ 1000 条/秒(该项由 ub-device-manager 侧批量推送配合测得)。PT 场景待补。

### 7.2 测试场景

| 用例名 | 场景 | 期望指标 |
|--------|------|----------|
| PerfQuery_Normal | 100 条缓存数据查询 | 响应时间 ≤ 100ms(NFR-PERF-02) |
| PerfQuery_Concurrent_10Threads | 10 线程并发查询 | 全部 ≤ 100ms |
| PerfAttach_Normal | 单 namespace attach | 响应时间 ≤ 500ms(NFR-PERF-01) |
| PerfCacheLoad_100Entries | 100 条 namespace 缓存文件加载 | ≤ 50ms(NFR-PERF-03) |
| PerfCacheFlush_100Entries | 100 条 namespace 缓存落盘 | ≤ 100ms |
| PerfVsockReceive_1000Msgs | 1000 条推送消息接收 | 吞吐 ≥ 1000 条/秒 |

### 7.3 测试方法

- 使用 `std::chrono` 测量响应时间，预热后执行 100 次取 P50/P95/P99
- 使用 `std::thread` 模拟并发
- 关闭日志输出避免 IO 影响
- 缓存加载/落盘计时以 `LoadFromDisk`/`FlushToDisk` 为边界(含 JSON 解析与 map 填充)

---

## 8. 模糊测试设计

### 8.1 测试目标

通过随机/异常输入发现崩溃、内存错误、未定义行为。FUZZ 场景待补。

### 8.2 测试场景

| 用例名 | 输入域 | 期望结果 |
|--------|--------|----------|
| FuzzVmCache_UpdateAllocInfo | 随机 name(长度 0~48)、size(0~UINT64_MAX) | 不崩溃，不内存泄漏 |
| FuzzVmCache_LoadFromDisk | 随机/截断/乱码/字段缺失/枚举越界 JSON 文件 | 不崩溃，正确删除文件或保留内存缓存 |
| FuzzVsockTransport_TryDecodeMessage | 随机 body len(含超长值)、随机 payload 长度(0~64KB) | 不崩溃，超长关闭连接，防内存放大 |
| FuzzVmHandlers_PushPayload | 随机 type、违反契约的二进制载荷、count 字段超限 | 不更新缓存，回错误响应(`UBSE_ERR_INVALID_ARG`) |
| FuzzCache_ConcurrentReadWrite | 随机读写并发 | 无数据竞争，无崩溃 |

### 8.3 测试方法

- 编译时启用 ASAN/UBSAN
- 使用随机输入生成器(基于 `std::mt19937`)
- 持续运行 24 小时，监控崩溃与告警

---

## 9. 端到端测试设计

### 9.1 测试目标

验证完整业务链路，覆盖所有 AC-VM-XX 验收项。E2E 场景待补(依赖联调环境)。

### 9.2 测试环境

- 真实 VM 环境(openEuler Linux ARM64)
- host 侧 ubse@host + ub-device-manager(或 mock)
- VM 内 ubse@VM(vm_mode=true)

### 9.3 测试场景

#### 9.3.1 首次部署与设备直通(AC-VM-DEPLOY-01/02, AC-VM-VSOCK-01/02, AC-VM-ATTACH-01)

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_FirstDeploy_FeDeviceAlloc | openstack → ubse@host FeDeviceAlloc → ub-device-manager 推送 → ubse@VM 接收 → attach | AC-VM-DEPLOY-01, AC-VM-VSOCK-02, AC-VM-ATTACH-01 |
| E2E_QueryAfterPush | 推送完成 → VM 内 CLI `display ssu -t alloc_summary` | AC-VM-QUERY-01 |
| E2E_AttachDetach | VM 内 attach → 使用设备 → detach | AC-VM-ATTACH-01, AC-VM-DETACH-01 |

#### 9.3.2 VM 重启恢复(AC-VM-LIFECYCLE-01/02, AC-VM-CACHE-PERSIST-03)

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_VmRestart_CacheRecovery | VM 重启 → ubse@VM 启动 → ub-device-manager 重连 → 全量同步 → 查询 | AC-VM-LIFECYCLE-01/02 |
| E2E_VmRestart_AttachedReset | VM 重启后 attachedNames 为空 | AC-VM-CACHE-PERSIST-03 |

#### 9.3.3 host 模式向后兼容(AC-VM-DEPLOY-03, AC-BOUNDARY-03)

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_HostMode_AllocSpace | host 模式 AllocSpace 正常 | AC-VM-DEPLOY-03 |
| E2E_HostMode_FeDeviceAlloc | host 模式 FeDeviceAlloc 正常 | AC-BOUNDARY-03 |

---

## 10. 测试数据管理

### 10.1 测试数据分类

| 类型 | 说明 | 示例 |
|------|------|------|
| 输入数据 | 接口参数 | name、size、nqn、tgtEid |
| Mock 数据 | 模拟外部依赖返回 | 分配结果、连接信息、适配器 ns 列表、customData(uid/userName) |
| 期望数据 | 断言期望值 | ret=UBSE_OK、devPath、缓存条数 |
| 状态数据 | 被测对象内部状态 | 缓存 map、attachedNames、vsock 连接状态、`g_*` 全局 mock 态 |

### 10.2 测试数据工厂

| 工厂/helper | 所在文件 | 用途 |
|-------------|----------|------|
| `MakeAllocResult(name, size, nsCount)` | test_ubse_ssu_vm_cache.cpp | 构造分配结果 |
| `MakeConnectInfo(name, tgtEid)` / `MakeConnectList(name, n)` | test_ubse_ssu_vm_cache.cpp | 构造连接信息 |
| `WriteCacheFile` / `ReadCacheFile` | test_ubse_ssu_vm_cache.cpp | 直接构造/读取磁盘缓存文件(损坏场景) |
| `MakeVmAllocResult` / `MakeVmConnectInfo` | test_ubse_ssu_service_imp_vm.cpp | VM 服务分支夹具工厂(tgtEid 固定 16 字符) |
| `PushAdapterNs(eid, subNqn, nsId, uuidByte, uid, userName)` | test_ubse_ssu_service_imp_vm.cpp | 注入适配器可见 ns(身份校验用) |
| `SerializeAlloc` / `SerializeConnectPush` / `SerializeFullSync` | test_ubse_ssu_controller_module_vm_handlers.cpp | 按线路协议构造推送载荷 |
| `MakeRequestBytes(moduleCode, opCode, body, clientRequestId)` | vscok 两个 UT | 构造 UbseRequestHeader+body 字节流 |
| `RegisterAdapterNs` / `MakeItConnectInfo` | ssu_virt_it_mock_adapter | IT 适配器注入(uid=2000/userName="it_user") |

### 10.3 测试数据隔离

- 每个用例独立构造数据，避免共享状态污染；`g_*` 全局 mock 态在 SetUp 复位
- `UbseSsuVmCache` 在 SetUp 中创建新实例并指向 `/tmp` 临时文件，TearDown 中 `Clear()` 并 unlink
- `UbseSsuServiceImp` 单例在 TearDown 中恢复初始状态(`SetVmCache(nullptr)`/`SetVmMode(false)`)，并清空 `dlManager_.handle_` 防 dlclose 段错误
- Mock 期望在 TearDown 中通过 `GlobalMockObject::verify()` 校验并清理
- IT 端口冲突自动后移(`IT_VSOCK_PORT_BASE + pid%512*IT_MAX_PORT_ATTEMPTS + attempt`)，缓存文件带 pid 后缀

---

## 11. 测试执行与 CI 集成

### 11.1 本地执行

```shell
# 执行所有 UT
bash build.sh ut

# 执行 VM 特性相关 UT
bash build.sh ut -- --gtest_filter="TestUbseSsuVmCache.*"
bash build.sh ut -- --gtest_filter="TestUbseVsockServer.*"
bash build.sh ut -- --gtest_filter="TestUbseVsockTransport.*"
bash build.sh ut -- --gtest_filter="TestUbseSsuServiceImpVm.*"
bash build.sh ut -- --gtest_filter="TestUbseSsuControllerModuleVmMode.*"
bash build.sh ut -- --gtest_filter="TestUbseSsuVmHandlers.*"
bash build.sh ut -- --gtest_filter="TestUbseCliSsuVm.*"

# 执行 IT(需 vsock loopback 内核模块;无该模块时经 vsock_sim LD_PRELOAD)
bash build.sh it -- --gtest_filter="TestSsuVirtModuleMode.*:TestSsuVirtPush.*:TestSsuVirtTls.*"

# 生成覆盖率报告
bash build.sh ut -C

# 覆盖率报告 + HTTP 服务查看
bash build.sh ut -C -H
```

### 11.2 CI 流水线

| 阶段 | 触发 | 动作 |
|------|------|------|
| Pre-Commit | git commit | clang-format + clang-tidy |
| CI-Build | push / PR | `bash build.sh` 编译 |
| CI-UT | push / PR | `bash build.sh ut`，新增子模块覆盖率 ≥ 80% |
| CI-IT | 每日 | vsock 端到端 IT(loopback 或 vsock_sim) |
| CI-PT | 每版本 | 性能基线对比 |
| CI-FUZZ | 每版本 | 24 小时模糊测试 |
| CI-E2E | 每版本 | 真实 VM 环境 E2E |

### 11.3 覆盖率度量

| 度量项 | 目标 | 工具 |
|--------|------|------|
| 行覆盖率 | ≥ 80% | gcov / lcov |
| 分支覆盖率 | ≥ 70% | gcov / lcov |
| 函数覆盖率 | ≥ 90% | gcov / lcov |
| UbseSsuVmCache | ≥ 85% | 核心子模块 |
| UbseVsockServer / UbseVsockTransport | ≥ 85% | 核心子模块 |

### 11.4 测试报告

- UT 报告:JUnit XML 格式
- 覆盖率报告:`coverage/index.html`
- CI 集成:通过 `gtest_discover_tests` 注册到 CTest

---

## 12. 缺陷管理

### 12.1 缺陷分级

| 级别 | 定义 | 处理时限 |
|------|------|----------|
| P0 | 阻塞主流程，数据丢失，host 模式行为被破坏 | 立即修复 |
| P1 | VM 模式核心功能不可用(查询/attach/detach/推送接收) | 1 个工作日 |
| P2 | VM 模式次要功能不可用(CLI 显示、日志、缓存落盘延迟) | 3 个工作日 |
| P3 | 体验问题(注释、命名) | 版本发布前 |

### 12.2 缺陷跟踪

- 录入缺陷管理系统(如 Gitee Issue)
- 包含:标题、复现步骤、期望/实际结果、环境、日志、截图
- 关联对应的 AC-VM-XX 或 NFR-XX

---

## 13. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| UT 中无法创建真实 AF_VSOCK/TLS 连接 | 服务层/传输层覆盖受限 | 服务层纯逻辑测试；传输层 socketpair 伪造 fd + `-fno-access-control` 私有成员注入;真实连接下沉到 IT |
| 私有成员注入依赖布局与实现细节 | 重构后 UT 脆弱断裂 | 注入点集中注释;重构时同步评审 vscok/缓存/service 三处 UT 的注入语句 |
| Mock 适配器与真实 VM 内适配器行为不一致 | UT/IT 通过但 E2E 失败 | 定期对比 Mock 与真实返回；E2E 用真实设备验证 |
| host 模式单例状态污染 | VM 模式 UT 影响 host 模式 UT | TearDown 严格恢复 `UbseSsuServiceImp` 单例与 `g_*` 全局 mock 态 |
| `~UbseDlManager` 对空/非法句柄 dlclose 段错误 | 适配器相关 UT 崩溃 | TearDown 统一置空 `dlManager_.handle_` |
| IT 依赖内核 vsock loopback 模块 | 部分环境 IT 跳过 | `vsock_sim` LD_PRELOAD 仿真层重定向 TCP loopback；CI 环境预装 vsock_loopback |
| IT 端口冲突(并发跑 IT) | 偶发失败 | 端口按 pid 派生 + 冲突自动后移 |
| 缓存文件 IO 在 UT 中依赖真实文件系统 | UT 不稳定 | `mkstemp` 临时目录 + TearDown 清理(含 `.tmp`) |
| 多线程并发测试不稳定 | 偶发失败 | 使用原子计数/条件同步，增加超时容忍 |
| 覆盖率达标但质量不足 | 仅追求覆盖率 | 增加边界值(4096/8192 超限)、异常路径(中途反序列化失败/回滚)、并发用例 |

---

## 14. 附录

### 14.1 现有测试用例统计

| 测试套件 | 文件 | 用例数 | 状态 |
|----------|------|--------|------|
| TestUbseVsockServer | test/UT/framework/vscok/test_ubse_vsock_server.cpp | 14 | 已实现 |
| TestUbseVsockTransport | test/UT/framework/vscok/test_ubse_vsock_transport.cpp | 19 | 已实现 |
| TestUbseSsuVmCache | test/UT/controllers/ssu/test_ubse_ssu_vm_cache.cpp | 37 | 已实现 |
| TestUbseSsuServiceImpVm | test/UT/controllers/ssu/test_ubse_ssu_service_imp_vm.cpp | 29 | 已实现 |
| TestUbseSsuControllerModuleVmMode | test/UT/controllers/ssu/test_ubse_ssu_controller_module_vm_mode.cpp | 9 | 已实现 |
| TestUbseSsuVmHandlers | test/UT/controllers/ssu/test_ubse_ssu_controller_module_vm_handlers.cpp | 15 | 已实现 |
| TestUbseCliSsuVm | test/UT/cli/test_ubse_cli_ssu_vm.cpp | 15 | 已实现 |
| TestUbseCliSsuCmdReg(含 VM 场景) | test/UT/cli/test_ubse_cli_ssu_cmd_reg.cpp | 121 | 已实现 |
| TestSsuVirtModuleMode(IT) | test/IT/controllers/ssu/test_ssu_virt_module_it.cpp | 4 | 已实现 |
| TestSsuVirtPush + TestSsuVirtTls(IT) | test/IT/controllers/ssu/test_ssu_virt_push_it.cpp | 7 | 已实现 |
| (host 侧既有套件,回归参照) | test_ubse_ssu_scheduler.cpp / test_ubse_ssu_direct_to_vm_manager.cpp 等 | 54 / 45 | 已实现(host 归主文档) |
| TestUbseSsuPerf / Fuzz 套件 | 待新增 | 0 | 待补(PT/FUZZ) |

**小计**:VM 特性 UT 138 例 + IT 11 例已实现;PT/FUZZ/E2E 待补。

### 14.2 测试覆盖率目标

| 子模块 | 当前行覆盖率 | 目标行覆盖率 | 目标分支覆盖率 |
|--------|-------------|-------------|---------------|
| UbseSsuVmCache | 待测(`bash build.sh ut -C`) | ≥ 85% | ≥ 75% |
| UbseVsockServer / UbseVsockTransport | 待测 | ≥ 85% | ≥ 75% |
| UbseSsuServiceImp(VM 分支) | 待测 | ≥ 80% | ≥ 70% |
| UbseSsuControllerModule(VM 分支/handler) | 待测 | ≥ 80% | ≥ 70% |
| CLI(VM 命令集) | 待测 | ≥ 80% | ≥ 70% |

### 14.3 关键常量

| 常量 | 值 | 来源 | 测试用途 |
|------|----|------|---------|
| `UBSE_VSOCK_DEFAULT_PORT` | 6174 | src/framework/vscok/ubse_vsock_message.h | 端口默认值/0 回落测试 |
| `UbseVsockConfig` 默认 | port=6174/enableTls=true/certDir=/var/lib/ubse/lcne_cert//threadPoolSize=8/queueSize=16 | src/framework/vscok/ubse_vsock_transport.h | Config::IsValid 校验矩阵 |
| `UBSE_SSU_DEFAULT_CACHE_FILE` | /var/lib/ubse/ssu_vm_cache.json | src/controllers/ssu/ubse_ssu_controller_module.h | 缓存文件路径(固定不可配置) |
| `MAX_PUSH_CONNECT_INFO_NUM` | 4096 | ubse_ssu_controller_module.cpp | 连接信息推送条数上限(4095/4096 边界) |
| `MAX_FULL_SYNC_ALLOC_NUM` | 8192 | ubse_ssu_controller_module.cpp | 全量同步 alloc 条数上限 |
| `MAX_FULL_SYNC_CONNECT_NUM` | 8192 | ubse_ssu_controller_module.cpp | 全量同步 connect 条数上限 |
| `MAX_NAME_LEN` / `MAX_NQN_LEN` / `MAX_DEV_NAME_LEN` | 48 / 69 / 33 | src/controllers/ssu/ipc/message/ubse_ssu_obj_message.h | 线协议长度上限(含 '\0') |
| `SSU_CLI_MAX_NAME_LENGTH` 等 | 47 / 68 / 32 / 62 / 16 / 36 | src/cli/ssu/ubse_cli_ssu_limits.h | CLI 侧上限(线协议值-1) |
| `UBSE_SSU_CONF_VM_MODE_KEY` 等 | vm_mode / vsockServerPort / enable_tls | ubse_ssu_controller_module.h | 配置键(mock 匹配) |
| `UBSE_SSU_VM_MODE_ENV` | UBSE_SSU_VM_MODE | src/cli/ssu/ubse_cli_ssu_cmd_reg.h | CLI 环境变量判定(值恰为 "1") |
| `UBSE_ERR_NOT_SUPPORTED` | 41 | src/include/ubse_error.h | VM 模式不支持接口错误码 |
| `UBSE_ERR_ACCESS_DENIED` | 32 | src/include/ubse_error.h | 身份不匹配错误码(NFR-SEC-02) |
| `UBSE_SSU_ERROR_NS_NOT_FOUND` | 1315 | src/include/ubse_error.h | attach/查询未命中错误码(注意:无 `UBSE_SSU_NS_NOT_FOUND` 符号) |
| `UBSE_SSU_ERROR_SPACE_NOT_FOUND` | 1200 | src/include/ubse_error.h | GetNsStats 缓存未命中错误码 |
| `UBSE_ERR_INTERNAL` | 1005 | src/include/ubse_error.h | 传输层 TLS 证书目录缺失错误码 |
| `SSU_FE_ID` | 6 | ubse_ssu_direct_to_vm_manager.cpp | VFE 直通 PFE 编号(host 侧,见主文档) |
| `COMMON_RETRY_TIME` / `SLEEP_TIME` | 5 / 2 秒 | ubse_ssu_direct_to_vm_manager.cpp | MTI 重试次数与间隔 |
| `CLEAR_VM_BUS_INST_INTERVAL_SECONDS` | 60 | ubse_ssu_direct_to_vm_manager.cpp | 空 bus instance 清理周期 |

### 14.4 相关文档

- [SSU模块虚机化特性需求分析文档](./SSU模块虚机化特性需求分析文档.md)
- [SSU模块虚机化特性模块设计文档](./SSU模块虚机化特性模块设计文档.md)
- [SSU模块测试设计文档](../SSU模块测试设计文档.md)
- [SSU模块需求分析文档](../SSU模块需求分析文档.md)
- [单元测试开发指南](../../../test/单元测试开发指南.md)
- [AGENTS.md](../../../../../AGENTS.md)
