# UB Feature 配置能力需求串讲

## 一句话目标

UBS-Engine 在启动时读取 `/sys/bus/ub/ub_feature`，根据主机实际支持的 UB 能力决定
URMA 通信、内存借用、内存共享等能力是否可用；不支持的能力对外返回明确错误码，后台任务也不再无效运行。

## 背景和问题

UB 驱动已经通过 sysfs 文件向用户态暴露硬件/驱动能力：

```text
/sys/bus/ub/ub_feature
```

该文件是一个 bit 位图，每个 bit 表示一种能力是否支持。当前 UBS-Engine 在多处默认认为
URMA、内存借用、内存共享能力都存在，带来几个问题：

- 不支持 URMA 的环境仍会尝试加载 URMA 相关库、创建 URMA 后台任务、走 URMA 建链。
- 不支持内存借用或共享的环境仍暴露相关接口，调用后可能在后续调度、账本、导入导出流程中失败。
- `cluster.ipList` 被复用成 URMA/TCP 模式判断条件，语义不清晰。
- 能力缺失时没有统一错误码，调用方难以区分“不支持”和“运行失败”。

本需求的核心是让 UBS-Engine 从“默认全能力运行”变成“按环境能力运行”。

## 用户可感知变化

### URMA 通信

默认仍使用 URMA 通信。只有当 `ub_feature` 中没有任何 URMA 相关 bit 时，才切换到 TCP 通信。

TCP 通信依赖配置项：

```text
ubse.rpc.cluster.ipList
```

如果 URMA 不支持，同时 `cluster.ipList` 缺失或为空，通信初始化失败，并打印明确日志：

```text
URMA is unsupported and cluster.ipList is required for TCP communication.
```

### URMA 对外接口

当 URMA 不支持时，URMA 北向接口仍然存在，但直接返回：

```cpp
UBSE_ERR_NOT_SUPPORTED
```

SDK 侧对应：

```cpp
UBS_ERR_NOT_SUPPORTED
```

这样调用方看到的是“能力不支持”，不是“接口不存在”或“daemon 不可达”。

### 内存借用和共享

内存借用、内存共享分别按 NC/CC 能力控制。

- 借用 NC 不支持：NC 借用请求返回不支持。
- 借用 CC 不支持：CC 借用请求返回不支持。
- 共享 NC 不支持：NC 共享请求返回不支持。
- 共享 CC 不支持：CC 共享请求返回不支持。

普通借用请求本身不指定 NC/CC 时，服务端按能力自动选择：

- NC、CC 都支持：保持当前默认行为。
- 仅 NC 支持：走 NC 模式。
- 仅 CC 支持：走 CC 模式。
- NC、CC 都不支持：返回 `UBSE_ERR_NOT_SUPPORTED`。

当前代码中的默认模式需要注意：

- 普通 FD/NUMA/ADDR 借用默认是 CC/cacheable。
- SHM 共享默认是 NC/non-cacheable。
- SHM 带 `UBS_MEM_FLAG_CACHEABLE` 时是 CC/cacheable。

## Feature Bit 范围

内存能力 bit：

```cpp
#define UB_MEM_BORROW_NC_MASK BIT(0)
#define UB_MEM_BORROW_CC_MASK BIT(1)
#define UB_MEM_SHARE_NC_MASK BIT(2)
#define UB_MEM_SHARE_CC_MASK BIT(3)
```

URMA 能力 bit：

```cpp
#define UB_URMA_RTP_ROI_MASK BIT(16)
#define UB_URMA_RTP_ROT_MASK BIT(17)
#define UB_URMA_RTP_ROL_MASK BIT(18)
#define UB_URMA_CTP_ROI_MASK BIT(19)
#define UB_URMA_CTP_ROT_MASK BIT(20)
#define UB_URMA_CTP_ROL_MASK BIT(21)
#define UB_URMA_CTP_UNO_MASK BIT(22)
#define UB_URMA_UTP_UNO_MASK BIT(23)
```

判断原则：

- 单个能力：对应 bit 为 1 表示支持，为 0 表示不支持。
- URMA 聚合能力：bit 16 到 bit 23 中任意一个为 1，即认为 URMA 能力可用。
- 内存借用聚合能力：借用 NC 或借用 CC 任意一个支持，即认为借用能力可用。
- 内存共享聚合能力：共享 NC 或共享 CC 任意一个支持，即认为共享能力可用。

## 模块分工

### conf 模块

conf 模块负责读取和缓存 `ub_feature`。

启动流程：

1. 先加载现有 `.conf` 配置。
2. 再读取 `/sys/bus/ub/ub_feature`。
3. 将解析结果缓存在 `UbseConfModule` 内。
4. 对外提供查询接口，供 URMA、通信、内存模块使用。

读取失败时，为兼容旧环境，默认认为所有已知 feature 都支持。

### com/election 模块

通信模式不再由 `cluster.ipList` 是否存在决定，而是由 `UbseIsUrmaSupported()` 决定。

- URMA 支持：使用 UBC/URMA 协议。
- URMA 不支持：使用 TCP 协议。

TCP 模式下，`cluster.ipList` 是必需配置项，只负责提供 IP 列表。

### URMA controller

URMA 支持时，保持现有流程：

- 创建 `UrmaExecutor`。
- 注册 URMA 内部 RPC。
- 订阅节点加入和拓扑变化事件。
- 运行 URMA 后台任务。

URMA 不支持时：

- 仍注册北向 IPC handler。
- 不创建 `UrmaExecutor`。
- 不注册内部 RPC。
- 不订阅 URMA 相关事件。
- 对外接口返回 `UBSE_ERR_NOT_SUPPORTED`。

### URMA UVS

URMA 不支持时不加载 `libtpsa.so`，避免 TCP 场景被 URMA 依赖库影响。

### mem controller

只要借用或共享任一能力可用，mem controller 保持现有后台流程。

借用和共享四个 bit 全部不支持时：

- 仍注册北向 IPC handler。
- 不创建 `ubseMemController` executor。
- 不注册 node controller 回调。
- 不注册后台 timer。
- 不初始化 scheduler、内部 RPC、fault manager、agent。
- 对外内存相关接口返回 `UBSE_ERR_NOT_SUPPORTED`。

## 错误码约定

能力不支持统一返回：

```cpp
UBSE_ERR_NOT_SUPPORTED
```

SDK 侧统一映射为：

```cpp
UBS_ERR_NOT_SUPPORTED
```

这个错误码只表达“当前环境不支持该能力”，不表达参数错误、资源不足、通信失败或内部异常。

## 兼容性策略

为了不影响没有新 sysfs 文件的旧环境，本需求采用保守兼容策略：

- `/sys/bus/ub/ub_feature` 不存在：默认所有已知 feature 支持。
- 文件不可读：默认所有已知 feature 支持。
- 内容为空或非法：默认所有已知 feature 支持。
- 数值超过 `uint64_t`：默认所有已知 feature 支持。

默认全支持只覆盖代码中已知的 feature bit，不表示 64 个 bit 全部支持。

## 验收场景

### conf 解析

- `0x0`：URMA、内存借用、内存共享均不支持。
- `0x3`：仅内存借用 NC/CC 支持。
- `0xc`：仅内存共享 NC/CC 支持。
- `0x10000`：URMA 支持。
- 文件不存在或内容非法：默认全支持。

### 通信建链

- URMA 支持时，通信协议选择 UBC。
- URMA 不支持且 `cluster.ipList` 合法时，通信协议选择 TCP。
- URMA 不支持且 `cluster.ipList` 缺失时，启动或建链失败并打印明确日志。

### URMA 能力关闭

- 不加载 `libtpsa.so`。
- 不创建 `UrmaExecutor`。
- 不注册 URMA 内部 RPC。
- URMA 北向接口返回 `UBSE_ERR_NOT_SUPPORTED`。

### 内存能力关闭

- 借用能力全关闭时，FD/NUMA/ADDR 借用创建类接口返回 `UBSE_ERR_NOT_SUPPORTED`。
- 共享能力全关闭时，SHM 创建、Attach、查询、删除等共享接口返回 `UBSE_ERR_NOT_SUPPORTED`。
- 借用和共享全关闭时，mem controller 后台 executor、timer、scheduler、RPC、fault manager、agent 均不运行。
- 普通借用未指定 NC/CC 时，只支持单一模式的环境自动选择唯一可用模式。

## 风险和注意事项

- 如果生产环境缺少 `/sys/bus/ub/ub_feature`，系统会按兼容策略默认全支持，行为与历史版本一致。
- TCP 模式强依赖 `cluster.ipList`，部署不完整时会更早失败，这是预期行为。
- 北向接口保留注册，调用方会收到“不支持”错误码，不应再把该场景理解为 daemon 不可达。
- feature 值只在启动时读取一次，运行时修改 sysfs 不会立即生效，需要重启 UBS-Engine。

## 串讲结论

本需求把 UB 能力从“隐式假设”变成“显式配置”，让 UBS-Engine 能在不同硬件/驱动能力环境下自动选择合适路径：

- 支持 URMA 时继续走高性能 URMA 通信。
- 不支持 URMA 时明确降级 TCP，并要求配置 IP 列表。
- 不支持内存借用或共享时，对外返回统一错误码。
- 能力全关闭时，相关后台任务不再运行，减少无效资源占用和误报。

整体方案保持旧环境兼容，同时为后续新增 UB feature bit 预留统一查询入口。
