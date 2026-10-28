# SSU 模块开发指南

> 源码路径:`src/controllers/ssu/`
> 服务契约:`src/include/plugin_services/ssu/ubse_ssu_service.h`
> 配套文档:[SSU模块需求分析文档.md](./SSU模块需求分析文档.md) / [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) / [SSU模块虚机化特性模块设计文档.md](./virt/SSU模块虚机化特性模块设计文档.md) / [SSU模块测试设计文档.md](./SSU模块测试设计文档.md)

---

本指南为 UBS-Engine 项目的 SSU(Storage Service Unit)控制器提供开发规范、问题排查与测试编写的综合指导。SSU 控制器负责存储资源分配/释放/挂载/卸载、NVMe 命名空间管理、FE/VFE 设备管理、责任链调度算法。

## 1. 架构概览

SSU 控制器位于 `src/controllers/ssu/`,采用分层设计:

```
IPC Handler 层 (ipc/handler/*.h)  ← 解包/处理/打包三阶段模板
       │
       ▼
Service 层 (UbseSsuServiceImp)    ← 业务编排,master/agent 协同
       │
       ├── Scheduler (UbseSsuScheduler) ← 责任链调度,分配设备
       ├── Collector  (UbseSsuCollector) ← 定时采集设备列表+预留管理
       └── Adapter    (UbseSsuAdapterImpl) ← dlopen libssu.so 调底层硬件
```

**三条对外通道**:
- **UDS IPC**(主通道):17 个 handler 注册到 `api_server`,经权限校验后分发
- **HTTP 通道**:`RegisterSsuHttpHandlers()` 注册 `POST/DELETE /ubse/v1/ssu/spaces`
- **VM 模式通道**:vm_mode 下通过 vsock 承载虚机侧 IPC(启用条件:配置 `vm_mode=true` 或环境变量 `UBSE_SSU_VM_MODE=1`,环境变量优先);vm_mode 下模块跳过 node/election/com/vip 依赖,CLI 仅注册 display/attach/detach 子集

**master/agent 协同**:master 端执行 `Execute*` 方法处理分配/释放/权限;agent 端执行 attach/detach 操作;`MASTER_ONLINE` 事件触发设备采集与账本重建。`UbseSsuControllerModule` 作为模块入口(`kModuleName = "UbseSsuControllerModule"`),`Initialize` 按 vm_mode 分流初始化。

## 2. 关键文件与类索引

### 2.1 核心服务层

| 文件 | 关键类/函数 | 职责 |
|---|---|---|
| `ubse_ssu_controller_module.h` | `class UbseSsuControllerModule : public UbseModule` | 模块入口,持有 `UbseSsuService` |
| `ubse_ssu_service_imp.h` | `class UbseSsuServiceImp : public UbseSsuService`(单例) | 业务编排,实现所有 override 接口 |
| `ubse_ssu_collector.h` | `class UbseSsuCollector` | 定时设备列表收集+预留管理 |
| `ubse_ssu_scheduler.h` | `class UbseSsuScheduler` | 责任链调度,`Execute(ctx&)` |
| `ubse_ssu_reservation_mgr.h` | `class UbseSsuReservationMgr` | 预扣除/预释放容量管理 |
| `ubse_ssu_direct_to_vm_manager.h` | - | VM 直通管理(设计详见 [SSU模块虚机化特性模块设计文档](./virt/SSU模块虚机化特性模块设计文档.md)) |
| `ubse_ssu_attach_record{,_store}.h` | `UbseSsuAttachRecordStore` | 挂载记录持久化(对账依据) |
| `ubse_ssu_master_online_handler.h` | - | `MASTER_ONLINE` 事件处理(驱动采集与账本重建) |
| `ubse_ssu_mount_ops.h` / `ubse_ssu_reconcile_state.h` | - | 本地挂载操作与对账状态机 |
| `ubse_ssu_rpc_processor.h` | - | RPC 处理器 |
| `ubse_ssu_http_handler.h` | `RegisterSsuHttpHandlers()` | HTTP 路由注册 |
| `ubse_ssu_utils.h` | `GetSsuExecutor()` / `GenerateHostNqn()` | 工具函数 |
| `debt/ubse_ssu_debt_ledger.h` | - | SSU 账本(事实源,复合键 `(name, resourceUuid)`,COW 写入) |

### 2.2 服务基类(插件服务契约)

`src/include/plugin_services/ssu/ubse_ssu_service.h` 定义了服务契约(命名空间 `ubse::plugin::service::ssu`):

- `class UbseSsuService : public UbseIService` —— 抽象基类,`kServiceName = "UbseSsuService"`
- 关键枚举:
  - `UbseSsuLBAFormat { LBA_FORMAT_512=512, LBA_FORMAT_4K=4096 }`
  - `UbseSsuAggregationRaidLevel { RAID0=0, RAID5=5 }`
  - `UbseSsuChunkSize { CHUNK_SIZE_4K/16K/32K/64K/128K/256K/512K }`(单位 KB)
  - `UbseSsuAllocStrategy { STRIPED=0(分布式,多设备均等), LINEAR=1(顺序,单设备优先) }`
  - `UbseSsuNsState { IDLE, CREATING, CREATED, ATTACHING, ATTACHED, FAULTED=5 }`(`FAULTED`:空间 NS 连续多个采集周期从硬件消失)
- 关键结构体:`UbseSsuAllocIdentityInfo`(userName+uid)、`UbseSsuAllocSpaceReq`(name/nsSize/nsNum/lbaFormat/strategy/tenant)、`UbseSsuNameSpaceInfo`、`UbseSsuAllocResult`、`UbseSsuVfe`、`UbseSsuFe`、`UbseSsuConnectInfo`、`UbseSsuNsStats`、`UbseSsuSpaceReq`、`UbseSsuLinearSpaceReq`(+devName)、`UbseSsuStripedSpaceReq`(+level+chunkSize)
- 纯虚接口(17 个):`AllocSpace/FreeSpace/AttachSpace/DetachSpace/AttachLinearSpace/DetachLinearSpace/AttachStripedSpace/DetachStripedSpace/AddAccessPermission/RemoveAccessPermission/GetNsStats/ListAllocInfo/GetAllocInfoByName/GetConnectInfo/GetFeDeviceList/FeDeviceAlloc/FeDeviceFree`
- 获取服务:`inline std::shared_ptr<UbseSsuService> GetSsuService()` 通过 `UbseServiceRegistry` 查找

### 2.3 调度器(责任链)

`ubse_ssu_scheduler.h`(命名空间 `ubse::ssu::scheduler`):

- `struct UbseSsuAllocRequest { allocSize; nsNum; lbaSize; addressingType; tenant; }`
- `enum class UbseSsuAllocRetCode { OK=0, INVALID_PARAM=1, INSUFFICIENT_SPACE=2, INTERNAL_ERROR=3 }`
- `struct UbseSsuAllocationResult { ret; msg; nsNum; sectorSize; eidNsSizeList; }`
- `struct UbseSsuFilterDev { eid; freeBytes; sectorBytes; nsCount; tenant; }`
- `class UbseSsuAllocationContext` —— 封装 devices(只读)/request/result/selectedDevs
- Filter 实现(继承 `UbseFilter<UbseSsuAllocationContext>`):
  - `PreCheckHandler` —— 预检查
  - `UbseSsuTenantIsolationFilter` —— 租户隔离过滤
  - `UbseSsuTenantQuotaFilter` —— 租户配额过滤
  - `UbseSsuAllocateAlgorithmHandler` —— 默认算法(容量大优先,namespace 数量少次优先)
- `enum class UbseSsuFilterGroupId { PRE_CHECK=0, ALGORITHM=1, POST_CHECK=2 }`
- `class UbseSsuScheduler` —— `Execute(ctx&)` 按 groupId 顺序执行;`AddFilter(groupId, filter)` 添加 filter

### 2.4 IPC Handler 层

`ipc/ubse_ssu_ipc_handler.h`(命名空间 `ubse::ssu::ipc`):

- `class UbseSsuHandler` —— 抽象基类,`Execute(buffer, context)` 模板方法,子类实现三阶段:
  - `virtual UbseResult Unpack() = 0` —— 解包请求
  - `virtual UbseResult Handle() = 0` —— 业务处理
  - `virtual UbseResult Pack(UbseIpcMessage &response) = 0` —— 打包响应
  - `UbseResult PopulateIdentityInfo()` —— 从 context 填充 `identity_`(userName/uid)
- `UbseResult RegisterSdkDispatcher()` —— 注册 SDK dispatcher

17 个具体 handler(每个对应一个 SSU 操作),位于 `ipc/handler/`:
- `ubse_alloc_space_handler` / `ubse_free_space_handler`
- `ubse_attach_space_handler` / `ubse_detach_space_handler`
- `ubse_attach_linear_space_handler` / `ubse_detach_linear_space_handler`
- `ubse_attach_striped_space_handler` / `ubse_detach_striped_space_handler`
- `ubse_add_access_permission_handler` / `ubse_remove_access_permission_handler`
- `ubse_get_ns_stats_handler` / `ubse_list_alloc_info_handler` / `ubse_get_alloc_info_by_name_handler` / `ubse_get_connect_info_handler`
- `ubse_fe_device_alloc_handler` / `ubse_fe_device_free_handler` / `ubse_get_fe_device_list_handler`

Handler 示例(`ubse_alloc_space_handler.h`):
```cpp
class UbseAllocSpaceHandler : public UbseSsuHandler {
protected:
    UbseResult Pack(UbseIpcMessage &response) override;
    UbseResult Handle() override;
    UbseResult Unpack() override;
private:
    plugin::service::ssu::UbseSsuAllocSpaceReq req;
    plugin::service::ssu::UbseSsuAllocResult result;
};
```

### 2.5 消息层

`message/` 目录:
- `ubse_ssu_alloc_msg.h` —— 分配消息
- `ubse_ssu_free_msg.h` —— 释放消息
- `ubse_ssu_perm_msg.h` —— 权限消息
- `ubse_ssu_attach_detach_verify_msg.h` —— attach/detach 验证消息(含 `UbseSsuNsVerifyInfo`)
- `ubse_ssu_query_verify_msg.h` —— 查询验证消息
- `ubse_ssu_status_update_msg.h` —— 状态更新消息
- `ubse_ssu_sync_resp_msg.h` —— 同步响应消息

`ipc/message/`:
- `ubse_ssu_handler_message.h` —— handler 消息
- `ubse_ssu_obj_message.h` —— 对象消息

### 2.6 底层适配器(adapter_plugins/ssu)

`class UbseSsuAdapterImpl : public UbseSsuAdapterInterface`(命名空间 `ubse::adapter_plugins::ssu::def`):
- 通过 `UbseDlManager` 运行时 `dlopen("libssu.so")` 加载底层 SSU 库
- 函数指针:`AcquireDevInfoFunc / CreateNamespaceFunc / DeleteNamespaceFunc / AttachNamespaceFunc / DetachNamespaceFunc / AddNamespaceAllowHostFunc / RemoveNamespaceAllowHostFunc / GetNamespaceAllowHostsFunc / FreeAllowHostsMemFunc`
- 数据结构:`DevEidT`(16 字节 EID)、`enum DevStatusT`、`struct DevAddrT`(src/tgt EID + IP + subNqn + jettyId + useUb)、`struct NamespaceBaseAttrT`、`struct DevNamespaceInfoT`、`struct DevInfoT`(最多 64 个 namespace)

## 3. 开发规范

### 3.1 新增 IPC Handler 流程

以新增一个 SSU 操作(如 `UpdateXxx`)为例:

1. **服务基类添加纯虚接口**:在 `src/include/plugin_services/ssu/ubse_ssu_service.h` 的 `UbseSsuService` 类中添加 `virtual uint32_t UpdateXxx(...) = 0;`,补充完整 Doxygen 注释

2. **服务实现类 override**:在 `ubse_ssu_service_imp.h` 的 `UbseSsuServiceImp` 中 `override` 该接口,在 `.cpp` 实现业务逻辑;若需 master 端处理,新增私有 `ExecuteXxx` 方法

3. **新增 IPC Handler 文件**:在 `src/controllers/ssu/ipc/handler/` 新建 `ubse_update_xxx_handler.{h,cpp}`,继承 `UbseSsuHandler`:
   ```cpp
   class UbseUpdateXxxHandler : public UbseSsuHandler {
   protected:
       UbseResult Unpack() override;   // 解包 req
       UbseResult Handle() override;   // 调用 UbseSsuServiceImp::GetInstance().UpdateXxx(req, identity, result)
       UbseResult Pack(UbseIpcMessage &response) override;  // 打包 result
   private:
       plugin::service::ssu::UbseSsuXxxReq req;
       plugin::service::ssu::UbseSsuXxxResult result;
   };
   ```

4. **在 `RegisterSdkDispatcher()` 中注册**:添加新 handler 的注册逻辑

5. **新增消息序列化**(如需):在 `message/` 目录新增 `ubse_ssu_xxx_msg.h`,定义请求/响应结构与 `Serialize/Deserialize`

6. **CMakeLists 更新**:在 `src/controllers/ssu/CMakeLists.txt` 添加新源文件

7. **SDK 三端对齐**:
   - C SDK:`src/sdk/c/include/ubs_engine_ssu.h` 添加 `ubs_ssu_xxx_update(...)` 声明,`src/sdk/c/ubs_engine_ssu.cpp` 实现,`libubse_helper` 添加 pack/unpack,`ssu/ubs_ssu_validate.{h,cpp}` 添加校验
   - Go SDK:`src/sdk/go/ssu/ubs_engine_ssu.go` 添加函数(通过 `ipc.InvokeCall(UbseModuleCode, OpCode, request)`),`ubs_engine_ssu_pack.go` 添加打包,`ubs_engine_ssu_validation.go` 添加校验
   - Python SDK:`src/sdk/python/ubs_engine_ssu.py` + `ffi/ubs_engine_binding_ssu.py` + `ipc/ubs_engine_ipc_codes.py`(op 码) + `models/ubs_engine_model_ssu.py`

8. **CLI(可选)**:若需 CLI 命令,在 `src/cli/ssu/` 新增命令注册,`ubse_cli_ssu_struct.h` 定义 wire 契约结构(字段顺序/宽度严格对齐 handler),`ubse_cli_ssu_limits.h` 定义默认值

### 3.2 调度算法扩展(Filter)

新增自定义调度 filter:

1. 在 `ubse_ssu_scheduler.h` 新增 filter 类,继承 `UbseFilter<UbseSsuAllocationContext>`:
   ```cpp
   class UbseSsuXxxFilter : public UbseFilter<UbseSsuAllocationContext> {
   public:
       bool Handle(UbseSsuAllocationContext &ctx) override;
   };
   ```
2. 在 `.cpp` 实现 `Handle`,操作 `ctx.selectedDevs`(过滤设备)或 `ctx.result`(设置结果);返回 `true` 继续链,`false` 终止
3. 选择合适的 `UbseSsuFilterGroupId`(`PRE_CHECK`/`ALGORITHM`/`POST_CHECK`),在 `UbseSsuScheduler` 构造函数中通过 `AddFilter(groupId, filter)` 注册,或在运行时动态添加

**默认算法逻辑**(`UbseSsuAllocateAlgorithmHandler`):容量大优先为第一优先级,namespace 数量少次优先。修改默认算法需谨慎,影响所有分配请求。

### 3.3 命名约定

- 文件名:`ubse_ssu_<module>.{h,cpp}`(小写下划线),handler 文件 `ubse_<verb>_<noun>_handler.{h,cpp}`
- 类名:`UbseSsu<Name>`(PascalCase),handler 类 `Ubse<Verb><Noun>Handler`
- 命名空间:
  - 服务层 `ubse::ssu::service`
  - 调度器 `ubse::ssu::scheduler`
  - IPC `ubse::ssu::ipc`
  - HTTP `ubse::ssu::http_handler`
  - 控制器模块 `ubse::ssu::controller`
  - 消息 `ubse::ssu::message`
  - 适配器 `ubse::adapter_plugins::ssu::def`
  - 服务契约 `ubse::plugin::service::ssu`
- 错误码:沿用 `UbseResult`/`uint32_t`,0 为成功,非零为失败

### 3.4 预留管理(Collector)

`UbseSsuCollector` 的预留机制用于防止并发分配/释放导致超分或不足分:

- **分配前**:`OnReserveBegin()` → `AddReserveSpace(eidBytesList)` → 调底层 `create_namespace` → 成功则 `OnReserveEnd()`,失败则 `ReleaseReservation(eidBytesList)` 回滚
- **释放前**:`OnReserveBegin()` → `AddReleasedSpace(eidBytesList)` → 调底层 `delete_namespace` → `OnReserveEnd()`
- **关键约束**:`OnReserveBegin` 和 `OnReserveEnd` 必须成对调用;`CollectDeviceList` 仅在 `pendingOps_==0`(无进行中操作)时清空预留,防止误清

### 3.5 HTTP 路由注册

在 `ubse_ssu_http_handler.h` 的 `RegisterSsuHttpHandlers()` 中,通过 `UbseHttpModule::RegHttpService(method, url, func)` 注册路由:
```cpp
// 现有路由
POST   /ubse/v1/ssu/spaces        -> AllocSpace
DELETE /ubse/v1/ssu/spaces/{name} -> FreeSpace
```
handler 函数签名:`uint32_t(const UbseHttpRequest&, UbseHttpResponse&)`。

## 4. 问题排查

### 4.1 常见问题与排查路径

| 现象 | 可能原因 | 排查路径 |
|---|---|---|
| `AllocSpace` 返回 `INSUFFICIENT_SPACE` | 设备可用空间不足;预留未释放导致虚占;租户隔离/配额过滤了所有设备 | 1. 检查 `GetCachedDevList()` 设备 freeBytes;2. 查 `UbseSsuReservationMgr` 是否有残留预留;3. 确认请求 tenant 与设备 tenant 匹配 |
| 重复分配返回 `ALREADY_ALLOCATED(2000)` | 同名条目已存在且状态非 `CREATING` | 1. `GetAllocInfoByName` 确认存量;2. 确非本节点在途请求则换 name 或先释放 |
| 释放返回 `NO_NEED_FREE(2002)` / 卸载返回 `NO_NEED_DETACH(2003)` | 账本无可释放/卸载条目(非幂等成功语义) | 1. 确认 name/resourceUuid 正确;2. 查账本当前状态 |
| 挂载返回 `ALREADY_ATTACHED(2001)` | 该节点已在 attachNodes 中(半成功语义,返回 2001 但设备已可用) | 1. 视为成功处理;2. `GetConnectInfo` 获取连接信息 |
| `AllocSpace` 返回 `INVALID_PARAM` | nsSize 不满足对齐(nsNum 整除且为 chunkSize 整数倍);name 超 47 字符 | 检查 `UbseSsuAllocSpaceReq` 字段;条带化策略需 nsSize 整除 nsNum 且整除后为 chunkSize 整数倍 |
| `AttachSpace` 失败 | identity 验证失败;namespace 不存在;底层 `attach_namespace` 返回错误 | 1. 检查 `VerifyAttachDetachIdentity` 返回;2. 确认 namespaceId 存在;3. 查底层 libssu 错误码 |
| 账本不一致 | master 重启未调用 `RebuildLedgerFromDevList`;并发操作预留未成对 | 1. 重启后确认调用了 `RebuildLedgerFromDevList()`;2. 检查 `OnReserveBegin/End` 配对;3. 对比 `GetCachedDevList` 与账本 |
| `libssu.so` 加载失败 | 适配器 dlopen 失败;函数符号缺失 | 1. 确认 `libssu.so` 在库搜索路径;2. 用 `ldd` / `nm -D` 检查符号;3. 查 `UbseDlManager` 日志 |
| FE 设备分配失败 | VFE 已被占用;busInstanceGuid 无效 | 1. 查 `GetFeDeviceList` VFE 状态;2. 确认 busInstanceGuid 非空且格式正确 |
| RPC 跨节点同步失败 | com 通道断开;agent 节点离线 | 1. 检查 `UbseComModule` 链路状态;2. 确认 agent 节点在线;3. 查 `ubse_ssu_rpc_processor` 日志 |

### 4.2 关键日志关键字

排查时优先搜索以下关键字(配合 `UBSE_LOG_ERROR/UBSE_LOG_WARN`):
- `AllocSpace` / `FreeSpace` / `AttachSpace` / `DetachSpace` —— 操作入口
- `ExecuteAlloc` / `ExecuteFree` / `ExecuteScheduler` —— master 端执行
- `UbseSsuScheduler` / `Execute` / `Filter` —— 调度链
- `UbseSsuCollector` / `CollectDeviceList` / `reservation` —— 设备采集与预留
- `UbseSsuAdapterImpl` / `dlopen` / `libssu` —— 适配器加载
- `VerifyAttachDetachIdentity` —— identity 验证
- `RebuildLedgerFromDevList` —— 账本重建

### 4.3 日志增强

若默认日志不足以定位,可在关键路径临时增加日志(发布前移除):
- `UbseSsuScheduler::Execute` 前后打印 `ctx.request` 与 `ctx.result`
- `UbseSsuCollector::GetDevListWithReservations` 打印预留叠加前后差异
- 每个 Filter 的 `Handle` 入口打印 `ctx.selectedDevs.size()`

## 5. 测试编写

### 5.1 测试组织

SSU 相关测试位于:
- `test/UT/controllers/ssu/` —— 控制器 UT(调度器、direct_to_vm_manager)
- `test/UT/adapter_plugins/ssu/` —— 适配器 UT
- `test/UT/cli/test_ubse_cli_ssu_*.cpp` —— CLI UT(cmd_reg / mock_invoke / struct / vm 四组,`TestUbseCliSsu*` 套名,含 vm_mode 注册子集用例)
- `test/mock_libs/ssu/` —— mock libssu 动态库

测试框架:GoogleTest + GoogleMock + mockcpp。UT 编译选项 `-fno-access-control`(打破私有成员访问)、`-rdynamic`(允许动态库访问全局符号)。

### 5.2 mock libssu

`test/mock_libs/ssu/mock_libssu.cpp` 实现了 `libssu.so` 的 mock,通过 JSON 配置驱动:

- **配置文件**:`mock_ssu_config.json`(路径由 `MOCK_SSU_CONFIG_PATH` 环境变量或 `mock_ssu_config_path.h.in` 模板指定),定义设备列表(`devices` 数组),每设备含 `eid/devPath/tnvmcap/unvmcap/subNqn/state` 及 `namespaces` 数组
- **mock 函数**(extern "C"):`acquire_dev_info / create_namespace / delete_namespace / attach_namespace / detach_namespace / add_namespace_allow_host / remove_namespace_allow_host / get_namespace_allow_hosts / free_allow_hosts_mem`
- **故障注入**:设置环境变量 `MOCK_SSU_FAIL`(含操作名如 `acquire,create,attach`),对应操作返回 -1
- **状态持久化**:每次 create/delete/attach/detach 后 `SaveJsonConfig()` 回写 JSON,跨用例保持状态
- **重置**:`mock_ssu_reset()` 清空 `g_devices` 与 `g_configLoaded`(测试间隔离)

### 5.3 调度器 UT 示例

`test/UT/controllers/ssu/test_ubse_ssu_scheduler.h`:

```cpp
class TestUbseSsuScheduler : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
protected:
    // 构造在线 SSU 设备
    static UbseSsuDevInfo MakeDev(const std::string &eid, uint64_t totalBytes, uint64_t usedBytes,
                                  uint32_t nsCount = 0, UbseSsuState state = UbseSsuState::ONLINE);
    // 构造指定 tenant 的设备
    static UbseSsuDevInfo MakeDevWithTenant(const std::string &eid, uint64_t totalBytes, uint64_t usedBytes,
                                            const std::string &tenant, uint32_t nsCount = 1,
                                            UbseSsuState state = UbseSsuState::ONLINE);
};

// 自定义 filter 测试
class CustomFilter : public UbseFilter<UbseSsuAllocationContext> {
public:
    bool Handle(UbseSsuAllocationContext &ctx) override {
        ctx.result.msg += " | CUSTOM";
        return true;
    }
};
```

编写调度器测试要点:
- 用 `MakeDev` / `MakeDevWithTenant` 构造设备列表
- 用 `UbseSsuAllocationContext(devs, req)` 构造上下文
- 调 `UbseSsuScheduler::Execute(ctx)` 后断言 `ctx.result.ret` / `ctx.result.nsNum` / `ctx.selectedDevs`
- 测试自定义 filter:用 `AddFilter(UbseSsuFilterGroupId::ALGORITHM, std::make_shared<CustomFilter>())` 添加

### 5.4 测试命令

```shell
# 构建 SSU 相关 UT
bash build.sh ut -- --gtest_filter="*Ssu*"

# 仅调度器 UT
bash build.sh ut -- --gtest_filter="TestUbseSsuScheduler.*"

# CLI SSU UT(测试套名 TestUbseCliSsu*,注意大小写)
bash build.sh ut -- --gtest_filter="*CliSsu*"

# 适配器 UT
bash build.sh ut -- --gtest_filter="*SsuAdapter*"

# 覆盖率
bash build.sh ut -C -- --gtest_filter="*Ssu*"
```

### 5.5 新增测试规范

1. **UT 文件命名**:`test_ubse_ssu_<module>.{h,cpp}`,置于 `test/UT/controllers/ssu/`(控制器)或 `test/UT/adapter_plugins/ssu/`(适配器)
2. **测试类**:继承 `testing::Test`,`SetUp/TearDown` 管理资源;`namespace ubse::ssu::xxx::ut`
3. **mock**:用 mockcpp mock C++ 函数/方法;底层 `libssu.so` 用 `test/mock_libs/ssu/mock_libssu.cpp`(JSON 驱动 + `MOCK_SSU_FAIL` 故障注入)
4. **CMakeLists**:在对应 `test/UT/.../CMakeLists.txt` 通过 `add_ut(module)` 宏注册
5. **断言**:用 `EXPECT_EQ` / `ASSERT_EQ` / `EXPECT_TRUE`;断言错误码、字段值、列表大小
6. **隔离**:每个用例 `SetUp` 调 `mock_ssu_reset()` 重置 mock 状态;跨用例状态由 `SaveJsonConfig` 持久化,需注意清理

## 6. 关键约束

- **name 长度**:存储空间标识有效上限 47 字符(`SSU_CLI_MAX_NAME_LENGTH = 48 - 1`,不含结尾 `'\0'`)
- **namespace 上限**:每控制器最多 64 个 namespace(`MAX_NAMESPACES_PER_CTRL`)
- **EID 长度**:16 字节(`EID_SIZE`)
- **nsSize 对齐**:条带化策略时,nsSize 需整除 nsNum,且整除后为 chunkSize 的整数倍
- **RAID5 约束**:至少需要 3 个成员设备
- **nsNum=1**:strategy 参数不生效
- **非幂等语义**:`AllocSpace` 重复分配返回 `ALREADY_ALLOCATED(2000)`;`FreeSpace` 无可释放返回 `NO_NEED_FREE(2002)`(均非成功);`AddAccessPermission` / `RemoveAccessPermission` 幂等(重复操作返回成功)
- **预留成对**:`OnReserveBegin` / `OnReserveEnd` 必须成对调用,否则预留泄漏导致超分/不足分
- **master/agent**:分配/释放/权限在 master 端 `Execute*`;attach/detach 在 agent 端;跨节点通过 `ubse_ssu_rpc_processor` 同步
- **vm_mode 分流**:配置 `vm_mode=true` 或环境变量 `UBSE_SSU_VM_MODE=1`(优先)启用;vm_mode 下跳过 node/election/com/vip 依赖,经 vsock 承载虚机侧 IPC
- **dlopen 时机**:`UbseSsuAdapterImpl` 运行时 `dlopen("libssu.so")`,需确保库在搜索路径(`LD_LIBRARY_PATH` 或 RPATH)
- **identity 验证**:attach/detach 前必须调用 `VerifyAttachDetachIdentity` 验证 userName/uid 对 name 的所有权

## 7. 参考路径

- 控制器根目录:`src/controllers/ssu/`
- 服务契约:`src/include/plugin_services/ssu/ubse_ssu_service.h`
- 适配器:`src/adapter_plugins/ssu/`
- C SDK:`src/sdk/c/include/ubs_engine_ssu.h` + `src/sdk/c/ubs_engine_ssu.cpp` + `src/sdk/c/ssu/ubs_ssu_validate.{h,cpp}`
- Go SDK:`src/sdk/go/ssu/`
- Python SDK:`src/sdk/python/ubs_engine_ssu.py` + `ffi/` + `ipc/` + `models/`
- CLI:`src/cli/ssu/`
- 测试:`test/UT/controllers/ssu/` + `test/UT/adapter_plugins/ssu/` + `test/mock_libs/ssu/`
- 配置:`conf/ubse.conf` 的 `[ubse.ssu]` 段

## 8. 相关文档

- [SSU模块需求分析文档.md](./SSU模块需求分析文档.md) —— 需求视角(做什么)
- [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) —— 设计视角(怎么做)
- [SSU模块虚机化特性模块设计文档.md](./virt/SSU模块虚机化特性模块设计文档.md) —— VFE 直通域设计
- [SSU模块测试设计文档.md](./SSU模块测试设计文档.md) —— 测试视角(怎么验)
- [编码规范文档.md](../../../conventions/编码规范文档.md) —— 编码规范约束
- [目录规范文档.md](../../../conventions/目录规范文档.md) —— 目录归档约束
