# UB Feature 配置能力设计

## 背景

UB 驱动通过 `/sys/bus/ub/ub_feature` 向用户态暴露当前主机支持的 UB 能力位图。
每个 bit 表示一种 feature 是否支持。UBS-Engine 需要由 `conf` 模块读取该 sysfs
文件，并向内存、URMA 等模块提供稳定的查询接口。

如果 sysfs 文件不存在、不可读或内容非法，UBS-Engine 为兼容旧环境，默认认为所有
已知 UB feature 均支持。

## Feature Bit 定义

feature 位图使用 `uint64_t` 表示。配置模块中定义如下 mask：

```cpp
constexpr uint64_t UB_MEM_BORROW_NC_MASK = 1ULL << 0;
constexpr uint64_t UB_MEM_BORROW_CC_MASK = 1ULL << 1;
constexpr uint64_t UB_MEM_SHARE_NC_MASK = 1ULL << 2;
constexpr uint64_t UB_MEM_SHARE_CC_MASK = 1ULL << 3;

constexpr uint64_t UB_URMA_RTP_ROI_MASK = 1ULL << 16;
constexpr uint64_t UB_URMA_RTP_ROT_MASK = 1ULL << 17;
constexpr uint64_t UB_URMA_RTP_ROL_MASK = 1ULL << 18;
constexpr uint64_t UB_URMA_CTP_ROI_MASK = 1ULL << 19;
constexpr uint64_t UB_URMA_CTP_ROT_MASK = 1ULL << 20;
constexpr uint64_t UB_URMA_CTP_ROL_MASK = 1ULL << 21;
constexpr uint64_t UB_URMA_CTP_UNO_MASK = 1ULL << 22;
constexpr uint64_t UB_URMA_UTP_UNO_MASK = 1ULL << 23;
```

同时定义聚合 mask：

- `UB_URMA_ALL_MASK`：bit 16 到 bit 23 的 OR 值。
- `UB_FEATURE_ALL_MASK`：当前所有已知 feature bit 的 OR 值，作为 sysfs 解析失败时的默认值。

## 解析流程

`UbseConfModule::Initialize()` 先完成现有 `.conf` 配置文件加载，再读取 UB feature 位图。

解析规则：

- 默认 sysfs 路径为 `/sys/bus/ub/ub_feature`。
- 读取第一行内容，并去除首尾空白字符。
- 使用 `std::stoull(value, &pos, 0)` 解析，支持十进制和 `0x` 十六进制格式。
- 只有完整消费去空白后的字符串才认为解析成功。
- 内容为空、非法字符串、数值溢出、文件不存在或打开失败时，记录 `WARN` 日志，并缓存
  `UB_FEATURE_ALL_MASK`。
- 解析成功时，按 sysfs 提供的实际位图缓存。

模块内缓存字段：

```cpp
std::atomic<uint64_t> ubFeature_{UB_FEATURE_ALL_MASK};
```

首个版本只在启动时读取一次 sysfs 值，不支持运行时刷新。

## 查询接口

`UbseConfModule` 提供如下成员接口：

```cpp
bool IsUbFeatureSupported(uint64_t mask) const;
bool IsUrmaSupported() const;
bool IsMemBorrowNcSupported() const;
bool IsMemBorrowCcSupported() const;
bool IsMemShareNcSupported() const;
bool IsMemShareCcSupported() const;
bool IsMemBorrowSupported() const;
bool IsMemShareSupported() const;
bool IsMemSupported() const;
```

接口语义：

- `IsUbFeatureSupported(mask)`：只有 `mask` 中所有 bit 都支持才返回 `true`，
  即 `(ubFeature_ & mask) == mask`。
- `IsUrmaSupported()`：`UB_URMA_ALL_MASK` 中任意 bit 为 1 即认为支持 URMA。
- 内存借用和共享按 NC、CC 模式分别查询。
- 内存借用、共享提供聚合查询接口，任一对应模式支持即返回 `true`。

在 `ubse_conf.h` 和 `ubse_conf.cpp` 中增加 free function 包装接口，保持与现有
`UbseGet*` 配置接口一致的调用风格：

```cpp
bool UbseIsUbFeatureSupported(uint64_t mask);
bool UbseIsUrmaSupported();
bool UbseIsMemBorrowNcSupported();
bool UbseIsMemBorrowCcSupported();
bool UbseIsMemShareNcSupported();
bool UbseIsMemShareCcSupported();
bool UbseIsMemBorrowSupported();
bool UbseIsMemShareSupported();
bool UbseIsMemSupported();
```

如果包装接口无法从 `UbseContext` 获取 `UbseConfModule`，记录错误日志并返回 `false`。

## 内存借用和共享能力生效

内存模块需要根据 `conf` 模块解析出的 feature 位图决定借用、共享接口是否可用。

能力判断规则：

- 内存借用 NC：`UbseIsMemBorrowNcSupported()`。
- 内存借用 CC：`UbseIsMemBorrowCcSupported()`。
- 内存共享 NC：`UbseIsMemShareNcSupported()`。
- 内存共享 CC：`UbseIsMemShareCcSupported()`。

请求模式判断规则：

- 请求中 `cacheableFlag == 0` 时按 NC 能力检查。
- 请求中 `cacheableFlag == 1` 时按 CC 能力检查。
- SDK 的 `UBS_MEM_FLAG_CACHEABLE` 对应显式 CC 模式。
- 普通借用请求不显式指定 NC/CC 模式时，由服务端根据能力自动选择：
  - NC、CC 都支持：使用当前默认借用方式，保持现有行为不变。
  - 仅 NC 支持：按 NC 模式执行。
  - 仅 CC 支持：按 CC 模式执行。
  - NC、CC 都不支持：返回 `UBSE_ERR_NOT_SUPPORTED`。
- 显式指定 NC 或 CC 的请求必须严格检查对应能力；指定模式不支持时不自动切换到另一种模式。

借用接口行为：

- FD、NUMA、ADDR 等借用创建类接口在 `api/ubse_mem_controller_fd_api.cpp`、
  `api/ubse_mem_controller_numa_api.cpp`、`api/ubse_mem_controller_addr_api.cpp`
  的 API 入口处进行能力兜底检查。SDK、CLI 和内部 RPC 最终都会进入这些 API 入口，因此
  能力判断不能只放在 dispatch 层。
- 普通借用请求在 API 入口先检查借用聚合能力；实际 NC/CC 解码模式继续由 decoder 工具按
  能力自动选择。
- 对应模式不支持时，不进入调度、账本、导出、导入流程，直接返回：

```cpp
UBSE_ERR_NOT_SUPPORTED
```

SDK 侧对应：

```cpp
UBS_ERR_NOT_SUPPORTED
```

共享接口行为：

- SHM 创建、指定亲和创建、指定出借节点创建、Attach、Detach、Return 等共享能力相关接口在
  `api/ubse_mem_controller_share_api.cpp` 的 API 入口处进行能力兜底检查。
- SHM 创建根据请求的 `cacheableFlag` 检查对应共享能力；Attach 在找到共享对象后，按共享对象
  记录的 `cacheableFlag` 再检查对应模式能力。
- 对应模式不支持时，不进入调度、账本、导出、导入流程，直接返回 `UBSE_ERR_NOT_SUPPORTED`。
- 查询、列表、删除、Detach 等对外接口也按所属能力组检查；借用或共享能力组关闭时统一返回
  `UBSE_ERR_NOT_SUPPORTED`，避免 feature 关闭后继续暴露相关能力面。

入口分层约定：

- `api/ubse_mem_controller_*_api.cpp` 是能力判断的权威兜底层，保证 SDK、CLI、内部调用和内部
  RPC 路径行为一致。
- `ubse_mem_controller_api_agent.cpp` 不做能力判断，只负责发起请求并等待响应；全关闭场景仍保留
  executor 和内部 RPC，确保请求可以进入 API 层返回明确错误码。
- `ubse_mem_controller_dispatcher.cpp` 只作为 SDK/IPC 入口的快速失败和参数解析层，不作为唯一能力
  拦截点。

需要覆盖的借用类北向 op code：

- `UBSE_MEM_FD_CREATE`
- `UBSE_MEM_FD_WITH_LEND_INFO`
- `UBSE_MEM_FD_CREATE_WITH_CANDIDATE`
- `UBSE_MEM_NUMA_CREATE`
- `UBSE_MEM_NUMA_WITH_LEND_INFO`
- `UBSE_MEM_NUMA_CREATE_WITH_CANDIDATE`
- 现有 ADDR 借用入口及其 CLI 包装入口
- `UBSE_MEM_CLI_FD_CREATE`
- `UBSE_MEM_CLI_NUMA_CREATE`

需要覆盖的共享类北向 op code：

- `UBSE_MEM_SHM_CREATE`
- `UBSE_MEM_SHM_CREATE_WITH_AFFINITY`
- `UBSE_MEM_SHM_CREATE_WITH_LENDER`
- `UBSE_MEM_SHM_ATTACH`
- `UBSE_MEM_CLI_SHM_CREATE`
- `UBSE_MEM_CLI_SHM_ATTACH`

## Mem Controller 禁用行为

mem controller 需要区分“接口注册”和“后台任务运行”。

当以下条件同时满足时，认为当前节点内存借用和共享能力均不可用：

```cpp
!UbseIsMemBorrowNcSupported() &&
!UbseIsMemBorrowCcSupported() &&
!UbseIsMemShareNcSupported() &&
!UbseIsMemShareCcSupported()
```

此时 mem controller 只保留北向接口注册，用于返回明确的不支持错误，不运行后台任务。

`UbseMemControllerModule::Initialize()` 行为：

- 始终保证内存北向 IPC handler 最终可注册，避免调用方收到 no handler。
- 借用和共享均不支持时：
  - 仍创建 `ubseMemController` executor，用于承载内部 RPC handler 到 API 层的失败响应路径。
  - 不注册 node controller 本地状态、集群状态通知回调。
  - 不订阅 `UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE`。
  - 不注册 `handleCheckTimer`。
  - 不运行 decoder handle 周期检查。
  - 返回 `UBSE_OK`，不阻塞主进程启动。
- 任一借用或共享能力支持时，保持现有后台任务初始化流程。

`UbseMemControllerModule::Start()` 行为：

- 借用和共享均不支持时：
  - 只注册 SDK/CLI dispatcher 和北向 IPC handler。
  - 注册 mem 内部 RPC handler，使 agent、CLI 和内部 RPC 请求仍可进入
    `api/ubse_mem_controller_*_api.cpp` 统一返回 `UBSE_ERR_NOT_SUPPORTED`。
  - 不初始化 scheduler。
  - 不初始化 fault manager。
  - 只初始化 agent 响应 handler 和超时配置，不启动 agent 后台任务。
  - 返回 `UBSE_OK`。
- 任一借用或共享能力支持时，保持现有 `Init()`、`RegMemControllerHandler()`、
  `RegisterSdkDispatcher()`、fault manager、agent 初始化流程。

`UbseMemControllerModule::Stop()` 行为：

- 借用和共享均不支持时，只执行必要的 handler/dispatcher 级清理；不注销未注册的 timer/event，
  不注销未注册的 timer/event。
- 任一借用或共享能力支持时，保持现有清理流程。

建议模块内缓存：

```cpp
bool memFeatureEnabled_{true};
```

## 通信模式选择

UBS-Engine 根据 `IsUrmaSupported()` 决定跨节点通信建链方式。

- `IsUrmaSupported() == true`：默认使用 URMA/UBC 通信。
- `IsUrmaSupported() == false`：降级使用 TCP 通信。

`ubse.rpc.cluster.ipList` 不再作为 URMA/TCP 模式开关。是否进入 TCP 模式只由 UB
feature 中的 URMA 能力决定。

`cluster.ipList` 的新语义：

- URMA/UBC 模式下不依赖 `cluster.ipList`。
- TCP 模式下，`cluster.ipList` 是必需配置项，用于提供集群节点 IP 列表。
- 当 URMA 不支持且 `cluster.ipList` 缺失、为空或解析失败时，TCP 通信初始化或建链失败，
  不再回退到 URMA。

通信模块改造点：

- 替换当前基于 `ubse.rpc.cluster.ipList` 是否存在来判断 UB/URMA 通信是否启用的逻辑。
- `UbseRpcServer::Start()` 通过 `UbseIsUrmaSupported()` 选择协议：
  - 支持 URMA：`UbseProtocol::UBC`。
  - 不支持 URMA：`UbseProtocol::TCP`。
- `ConnectWithOption()` 继续将通信模式传给 `UbseComRpcConnect()`：
  - URMA 模式调用 `CreateUbChannel()`。
  - TCP 模式调用 `CreateChannel()`。
- `queryCb` 和本地地址选择逻辑同步按通信模式分支：
  - URMA 模式按 nodeId 查询 bonding EID。
  - TCP 模式按 nodeId 查询 IP，依赖 `cluster.ipList` 或由其建立的 nodeId/IP 映射。

当 TCP 模式缺少必要配置时，日志需要明确提示：

```text
URMA is unsupported and cluster.ipList is required for TCP communication.
```

## URMA Controller 禁用行为

当 `IsUrmaSupported() == false` 时，URMA 控制器只保留北向接口注册，用于向调用方返回明确的
不支持错误，不运行 URMA 后台任务。

`UbseUrmaControllerModule::Initialize()` 行为：

- 始终注册 URMA 北向 IPC handler，避免调用方收到 no handler。
- URMA 不支持时：
  - 不创建 `UrmaExecutor`。
  - 不注册 URMA 内部 RPC handler。
  - 不订阅 `UBSE_EVENT_NODE_JOIN`。
  - 不订阅 `UBSE_EVENT_NODE_TOPO_LINK_CHANGE`。
  - 不触发 bonding 恢复、URMA 信息更新、QoS 同步等后台任务。
  - 返回 `UBSE_OK`，不阻塞主进程启动。
- URMA 支持时，保持现有初始化流程。

`UbseUrmaControllerModule::Stop()` 需要根据初始化时缓存的 URMA 支持状态分支：

- URMA 不支持时，不执行事件退订、定时器注销、`UrmaExecutor` 删除、URMA normal link 断链等清理动作。
- URMA 支持时，保持现有清理流程。

建议模块内缓存：

```cpp
bool urmaSupported_{true};
```

## URMA UVS 模块行为

`UbseUrmaUvsModule::Initialize()` 也需要根据 `UbseIsUrmaSupported()` 分支：

- URMA 不支持时：
  - 不 `dlopen("libtpsa.so")`。
  - 不查找 `uvs_*` symbol。
  - 记录 INFO 日志并返回 `UBSE_OK`。
- URMA 支持时，保持现有 `dlopen` 和 symbol 查找逻辑。

这样在不支持 URMA 的环境中，即使系统未安装 `libtpsa.so`，也不会产生无意义的初始化失败或告警。

## URMA 北向接口错误码

所有对外暴露的 URMA IPC handler 在入口统一检查 `UbseIsUrmaSupported()`。

URMA 不支持时直接返回：

```cpp
UBSE_ERR_NOT_SUPPORTED
```

SDK 侧对应错误码为：

```cpp
UBS_ERR_NOT_SUPPORTED
```

需要覆盖的 URMA 北向接口包括：

- `UbseUrmaDevGet`
- `UbseUrmaCliDevGet`
- `UbseUrmaCliDevActivate`
- `UbseUrmaDevAlloc`
- `UbseUrmaDevFree`
- `UbseUrmaBandWidthSet`
- `UbseUrmaBandWidthGet`
- `UbseUrmaBandWidthCliGet`
- `UbseUrmaBandWidthReset`

## 测试设计

在 `test/UT/config` 下增加或扩展单元测试。

测试场景：

- `0x3`：内存借用 NC、CC 支持；内存共享和 URMA 不支持。
- `0xc`：内存共享 NC、CC 支持；内存借用和 URMA 不支持。
- `0x10000`：URMA 支持，因为至少一个 URMA bit 被置位。
- 组合 mask 查询要求传入的所有 bit 都被置位才返回 `true`。
- 文件不存在时，默认所有已知 feature 支持。
- 内容为空、非法字符串、超过 `uint64_t` 范围时，默认所有已知 feature 支持。
- 无法获取 `UbseConfModule` 时，free function 包装接口返回 `false`。
- URMA 支持时，通信模块选择 `UbseProtocol::UBC`，URMA controller 正常创建 executor、
  注册内部 RPC、订阅事件。
- URMA 不支持且 `cluster.ipList` 合法时，通信模块选择 `UbseProtocol::TCP` 并走 TCP 建链。
- URMA 不支持且 `cluster.ipList` 缺失或非法时，TCP 通信初始化或建链失败。
- URMA 不支持时，URMA controller 不创建 `UrmaExecutor`，不注册内部 RPC，不订阅事件。
- URMA 不支持时，URMA UVS 模块不 `dlopen("libtpsa.so")`。
- URMA 不支持时，所有 URMA 北向 IPC handler 返回 `UBSE_ERR_NOT_SUPPORTED`。
- 内存借用 NC/CC 分别关闭时，对应 `cacheableFlag` 的 FD、NUMA、ADDR 借用创建接口返回
  `UBSE_ERR_NOT_SUPPORTED`。
- 普通借用未指定 NC/CC 时，NC、CC 都支持则保持默认行为；只支持 NC 或 CC 时自动选择唯一可用模式；
  二者都不支持时返回 `UBSE_ERR_NOT_SUPPORTED`。
- 内存共享 NC/CC 分别关闭时，对应 `cacheableFlag` 的 SHM 创建、Attach 接口返回
  `UBSE_ERR_NOT_SUPPORTED`。
- 内存借用和共享四个 bit 均关闭时，mem controller 不创建后台 executor，不注册后台 timer/event，
  不初始化 scheduler、内部 RPC、fault manager、agent 后台流程。

单元测试不能依赖真实的 `/sys/bus/ub/ub_feature`。读取路径需要设计成可测试入口，
测试中通过临时文件路径或 mock 文件读取入口验证行为。

## 兼容性说明

- `/sys/bus/ub/ub_feature` 对 UBS-Engine 只读，服务不写入该文件。
- 解析后的 feature 值不写入 `UbseConfigManager::configMap_`。它属于运行环境能力数据，
  不是普通 `.conf` 配置项。
- “默认全支持”只包含当前代码中定义的已知 feature bit，不表示 `uint64_t` 的全部 64 位
  都支持。
