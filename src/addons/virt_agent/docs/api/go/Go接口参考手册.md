# virt_agent Go SDK 接口参考手册

> 本文档为 virt_agent 对外 Go SDK 接口的完整参考。Go SDK 基于 cgo/dlopen 运行时加载客户端库 `libubs-virt-agent.so`，不产生独立动态库。
> 调用方经 UDS IPC 与 UBSE 服务中的 virt_agent 插件通信，实现内存借用、归还、迁移、查询等能力。

## 文档信息

| 项           | 内容                                                                                        |
| ------------ | ------------------------------------------------------------------------------------------- |
| 文档名称     | virt_agent Go SDK 接口参考手册                                                              |
| 适用组件版本 | UBS VirtAgent 1.0.1                                                                         |
| 依赖库       | `libubs-virt-agent.so`（由 `ubs-engine-virtagent` 主程序包提供，Go SDK 运行时 dlopen 加载） |
| 源代码位置   | `src/addons/virt_agent/sdk/go/`（以 Go module 方式引用）                                    |
| 接口总数     | 16（内存碎片 5 / 容器 6 / 查询 5）                                                          |
| 变更记录     | 1.0.1：初版，整合原分模块 Go 接口文档并补充场景适配说明                                     |

## 1. 概述

### 1.1 文档说明

本手册面向使用 Go 语言集成 virt_agent 能力的开发者，覆盖接口语义、入参、返回值、错误与示例。

- 建议阅读顺序：通用约定 → 目标场景对应模块 → 典型场景编排 → 附录；
- 每个接口首行标注**适用场景**；
- Go SDK 以源码方式分发（module 引用），运行目标机只需安装 `ubs-engine-virtagent` 主程序包。

### 1.2 组件架构

```text
Go 程序（cgo）
  └── memfragmentation / escape / collector 包
        └── dlopen 包（加载 libubs-virt-agent.so）
              └── UDS IPC
                    └── ubse 进程
                          └── virt_agent 插件（libvirtagent.so）
                                └── 依赖 RMRS（mempooling 插件）完成内存借用/迁移
```

- 运行场景由配置项 `virt.sceneType` 决定：`1` 为虚机场景，`0` 为容器场景；
- 虚机场景下按场景配置（`case_conf`）分为**超分**（`overCommitment`）与**内存碎片**（`memFragmentation`）两种业务模式；
- 不同场景注册的接口集合不同，详见 1.4 节“场景适配矩阵”。

### 1.3 快速开始

1. 引用 SDK 源码（module 方式）并加载动态库：
   ```go
   package main
   
   import (
       "fmt"
       "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
       "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
   )
   
   func main() {
       // 必须在使用任何接口前加载动态库
       if err := dlopen.GetLibHandle(); err != nil {
           fmt.Printf("dlopen failed: %v\n", err)
           return
       }
       defer dlopen.CloseLibHandle()
   
       numas, err := memfragmentation.UbsMemFragmentationNodeInfo()
       if err != nil {
           fmt.Printf("query failed: %v\n", err)
           return
       }
       fmt.Printf("numa count: %d\n", len(numas))
   }
   ```
2. 前置条件：目标场景已配置（见[部署说明](../../build_install/部署说明.md)第 4 章）、ubse 服务运行且 virt_agent 插件已加载。

### 1.4 接口清单

**场景适配矩阵**

| 分类                                                | 虚机·超分 | 虚机·碎片 | 容器 |
| --------------------------------------------------- | --------- | --------- | ---- |
| 虚机场景接口 - 内存碎片（`memfragmentation`）       | —         | ✓         | —    |
| 虚机场景接口 - 公共（`memfragmentation` 查询/心跳） | ✓         | ✓         | —    |
| 容器场景接口（`escape` / `collector`）              | —         | —         | ✓    |

> 说明：
>
> 1. 虚机场景（`virt.sceneType=1`）下，内存碎片接口仅在场景配置为 `memFragmentation` 时注册；超分场景由水位告警自动触发借用；
> 2. 容器场景（`virt.sceneType=0`）仅注册容器接口，第 4 章虚机查询接口不可用；
> 3. 未注册场景下的接口调用将失败（IPC 请求无对应服务端 handler）。

**接口清单**

| 接口                              | 所属包           | 适用场景       | 说明                                      |
| --------------------------------- | ---------------- | -------------- | ----------------------------------------- |
| `UbsMemBorrowExecute`             | memfragmentation | 虚机·碎片      | 执行内存借用                              |
| `UbsMemReturn`                    | memfragmentation | 虚机·碎片      | 内存归还                                  |
| `UbsMemRollback`                  | memfragmentation | 虚机·碎片      | 借用内存回滚                              |
| `UbsSyncTaskQuery`                | memfragmentation | 虚机·碎片      | 查询异步任务结果                          |
| `UbsMemBorrow`                    | memfragmentation | 虚机·碎片      | 大规格虚机场景按节点批量借用内存          |
| `UbsMemFragmentationNodeInfo`     | memfragmentation | 虚机·超分/碎片 | 查询本节点 NUMA 信息                      |
| `UbsMemFragmentationVmInfo`       | memfragmentation | 虚机·超分/碎片 | 查询本节点虚机信息                        |
| `UbsMemFragmentationNodeInfoList` | memfragmentation | 虚机·超分/碎片 | 查询集群内所有节点的内存信息              |
| `UbsMemFragmentationVmInfoList`   | memfragmentation | 虚机·超分/碎片 | 查询集群内所有节点的虚机信息              |
| `UbsMemFragmentationHeartbeat`    | memfragmentation | 虚机·超分/碎片 | 检测本节点 ubse 服务是否存活              |
| `UbsInjectWaterLine`              | escape           | 容器           | 注入节点水位线                            |
| `escape.UbsMemBorrow`             | escape           | 容器           | 按水位借用内存                            |
| `UbsMemMigrate`                   | escape           | 容器           | 冷热页交换执行                            |
| `escape.UbsMemReturn`             | escape           | 容器           | 内存归还执行                              |
| `UbsGetContainerMemInfo`          | collector        | 容器           | 查询容器 PID 本地和远端内存               |
| `UbsGetContainerPids`             | collector        | 容器           | 指定容器查询容器内 PID                    |
| `GetLibHandle`                    | dlopen           | 通用           | 加载 SDK 动态库（使用任何接口前必须调用） |
| `LoadFunc`                        | dlopen           | 通用           | 按名称加载库符号（业务接口内部已封装）    |
| `CloseLibHandle`                  | dlopen           | 通用           | 释放动态库句柄                            |

### 1.5 场景说明

虚机场景（`virt.sceneType=1`）包含两种业务模式：

- **超分场景（`overCommitment`）**：内存使用率超过水位线阈值时，自动触发内存借用/归还逃生。该场景的借用由水位告警自动触发，**不提供专属的手动借用接口**，调用方可使用第 4 章中的查询接口；
- **内存碎片场景（`memFragmentation`）**：节点 CPU 充足、内存不足时，借用邻居节点内存创建虚拟机。本模式下注册内存碎片接口，提供借用、迁移、归还、回滚等完整手动操作能力（`memfragmentation` 包）。

第 3 章接口仅适用于内存碎片场景；超分场景的可用接口见第 4 章。

典型调用时序（内存碎片）：

```text
借用执行（UbsMemBorrowExecute，同步/异步）──→ 异步任务查询（UbsSyncTaskQuery）
      └──→ 归还（UbsMemReturn）
             └──→ 异常回滚（UbsMemRollback）

大规格虚机：批量借用（UbsMemBorrow，异步）──→ 任务查询 ──→ 归还
```

## 2. 通用约定

### 2.1 异步任务模型

- `UbsMemBorrowExecute`（`BorrowSetting.IsAsync`）、`memfragmentation.UbsMemReturn`（`isAsync`）、`UbsMemBorrow`（`isAsync`）支持异步执行；
- 异步调用返回 `taskId`，通过 `UbsSyncTaskQuery(taskId)` 轮询；
- `AsyncTaskStatus` 取值：`AsyncTaskNotExist`(0)、`AsyncTaskRunning`(1)、`AsyncTaskSuccess`(2)、`AsyncTaskFailed`(3)；
- 任务失败时 `AsyncTaskInfo.Status` 为 `AsyncTaskFailed`、`ResultCode` 为非 0。

### 2.2 并发与鉴权

**并发与线程安全：**

- 各接口可被多个 goroutine 并发调用；
- 针对同一 `borrowId` 的借用、迁移、归还操作需由调用方串行化。

**鉴权与权限点**（配置见 `/etc/ubse/plugins/auth-virt_agent.conf`）：

| 权限点             | 覆盖接口                                           |
| ------------------ | -------------------------------------------------- |
| `vm.fragmentation` | 内存碎片接口 5 个 + `UbsMemFragmentationHeartbeat` |
| `vm.container`     | 容器接口 6 个                                      |
| `vm.query`         | 查询接口 4 个（不含 Heartbeat）                    |

## 3. 虚机场景—内存碎片接口

### 3.1 接口清单

以下接口适用场景为虚机场景（内存碎片 `memFragmentation`）。

| 接口                  | 所属包           | 说明                             | 同步/异步 | 适用场景  |
| --------------------- | ---------------- | -------------------------------- | --------- | --------- |
| `UbsMemBorrowExecute` | memfragmentation | 执行内存借用（根据借用策略结果） | 同步/异步 | 虚机·碎片 |
| `UbsMemReturn`        | memfragmentation | 内存归还                         | 同步/异步 | 虚机·碎片 |
| `UbsMemRollback`      | memfragmentation | 借用内存回滚                     | 同步      | 虚机·碎片 |
| `UbsSyncTaskQuery`    | memfragmentation | 查询异步任务结果                 | 同步      | 虚机·碎片 |
| `UbsMemBorrow`        | memfragmentation | 大规格虚机场景按节点批量借用内存 | 同步/异步 | 虚机·碎片 |

### 3.2 UbsMemBorrowExecute

> **适用场景**：虚机场景（内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemBorrowExecute(setting BorrowSetting) (MemBorrowResult, error)
```

#### 描述

根据借用决策结果执行内存借用。

#### 入参

| 入参    | 类型            | 必填 | 说明                                 |
| ------- | --------------- | ---- | ------------------------------------ |
| setting | `BorrowSetting` | 是   | 内存借用策略信息，是否异步执行开关。 |

- 类型定义

```go
// DstNumaInfo 借用目标节点 NUMA 信息
type DstNumaInfo struct {
    HostId   string
    SocketId uint16
    NumaIds  []int32
    MemSizes []uint64 // 单位KB，每个NUMA的借用大小建议为所选大页规格（2048KB或1048576KB）的整数倍，未对齐时服务端按页大小向上取整
}

// BorrowStrategy 内存借用策略
type BorrowStrategy struct {
    SrcHostId     string
    SrcSocketId   int16
    SrcNumaId     int16
    BorrowSize    uint64 // 单位KB
    DestNumaInfos []DstNumaInfo
}

// BorrowSetting 内存借用执行入参
type BorrowSetting struct {
    BorrowStrategy BorrowStrategy
    IsAsync        bool
    PageSize       uint64 // 大页大小，单位KB，仅支持2048或1048576，必填
}
```

#### 返回值

成功时返回 `MemBorrowResult` 借用结果；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// MemBorrowResult 内存借用结果
type MemBorrowResult struct {
    BorrowIds      []string // 借用ID列表
    PresentNumaIds []uint16 // 借入的远端NUMA ID列表
    TaskId         string   // 借用任务ID
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                    |
| ----------------------------------------------------- | ---------------------------------------------- |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名      |
| ubs_virt_agent_mem_borrow_execute failed: {code}      | 底层C接口返回非0错误码，{code} 为具体错误码值  |

#### 约束与注意事项

- `DestNumaInfos` 数量上限 100，超出部分截断；单条目标 NUMA 数量上限 1，超出截断。
- 字符串超长截断：节点ID上限 48 字节。
- `PageSize` 为必填项，单位KB，仅支持 2048（2M大页）或 1048576（1G大页）；未设置（值为 0）或其他取值底层返回入参非法错误。
- 对应C接口：`ubs_virt_agent_mem_borrow_execute`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    setting := memfragmentation.BorrowSetting{
        BorrowStrategy: memfragmentation.BorrowStrategy{
            SrcHostId:   "1",
            SrcSocketId: 1,
            SrcNumaId:   0,
            BorrowSize:  1048576,
            DestNumaInfos: []memfragmentation.DstNumaInfo{
                {HostId: "1", SocketId: 1, NumaIds: []int32{1}, MemSizes: []uint64{1048576}},
            },
        },
        IsAsync:  true,
        PageSize: 2048, // 单位KB，仅支持2048或1048576
    }
    result, err := memfragmentation.UbsMemBorrowExecute(setting)
    if err != nil {
        fmt.Printf("UbsMemBorrowExecute failed: %v\n", err)
        return
    }
    fmt.Printf("borrowIds: %v, presentNumaIds: %v, taskId: %s\n",
        result.BorrowIds, result.PresentNumaIds, result.TaskId)
}
```

### 3.3 UbsMemReturn

> **适用场景**：虚机场景（内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemReturn(isAsync bool) (string, error)
```

#### 描述

根据入参进行内存归还。

#### 入参

| 入参    | 类型   | 必填 | 说明                                                                                        |
| ------- | ------ | ---- | ------------------------------------------------------------------------------------------- |
| isAsync | `bool` | 是   | 异步执行开关。true：表示接口内部为异步执行内存归还。false：表示接口内部为同步执行内存归还。 |

#### 返回值

成功时返回后台任务ID `string`（异步执行时可通过 UbsSyncTaskQuery 查询任务状态）；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                    |
| ----------------------------------------------------- | ---------------------------------------------- |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名      |
| ubs_virt_agent_mem_return failed: {code}              | 底层C接口返回非0错误码，{code} 为具体错误码值  |
| invalid task id returned from C                       | 底层返回的后台任务ID为空指针                   |

#### 约束与注意事项

暂无

- 对应C接口：`ubs_virt_agent_mem_return`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    taskId, err := memfragmentation.UbsMemReturn(true)
    if err != nil {
        fmt.Printf("UbsMemReturn failed: %v\n", err)
        return
    }
    fmt.Printf("taskId: %s\n", taskId)
}
```

### 3.4 UbsMemRollback

> **适用场景**：虚机场景（内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemRollback(param RollbackParam) error
```

#### 描述

借用内存回滚。

#### 入参

| 入参  | 类型            | 必填 | 说明                                           |
| ----- | --------------- | ---- | ---------------------------------------------- |
| param | `RollbackParam` | 是   | 回滚的入参，包括节点id、节点上的borrowId列表。 |

- 类型定义

```go
// RollbackParam 借用内存回滚入参
type RollbackParam struct {
    NodeId       string
    BorrowIdList []string
}
```

#### 返回值

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                    |
| ----------------------------------------------------- | ---------------------------------------------- |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名      |
| ubs_virt_agent_mem_rollback failed: {code}            | 底层C接口返回非0错误码，{code} 为具体错误码值  |

#### 约束与注意事项

- `BorrowIdList` 数量上限 100，超出部分截断；借用ID长度上限 128 字节，超长截断。
- 对应C接口：`ubs_virt_agent_mem_rollback`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    param := memfragmentation.RollbackParam{
        NodeId:       "1",
        BorrowIdList: []string{"1-abc123"},
    }
    if err := memfragmentation.UbsMemRollback(param); err != nil {
        fmt.Printf("UbsMemRollback failed: %v\n", err)
        return
    }
}
```

### 3.5 UbsSyncTaskQuery

> **适用场景**：虚机场景（内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsSyncTaskQuery(taskId string) (AsyncTaskInfo, error)
```

#### 描述

借用归还任务查询。

#### 入参

| 入参   | 类型     | 必填 | 说明         |
| ------ | -------- | ---- | ------------ |
| taskId | `string` | 是   | 后台任务id。 |

- 类型定义

```go
// AsyncTaskStatus 异步任务状态
type AsyncTaskStatus int32

const (
    AsyncTaskNotExist AsyncTaskStatus = iota // 任务不存在
    AsyncTaskRunning                          // 任务运行中
    AsyncTaskSuccess                          // 任务成功
    AsyncTaskFailed                           // 任务失败
)
```

#### 返回值

成功时返回 `AsyncTaskInfo` 任务执行结果信息；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// AsyncTaskInfo 异步任务查询结果
type AsyncTaskInfo struct {
    TaskId          string
    Status          AsyncTaskStatus
    ResultCode      uint32
    MemBorrowResult MemBorrowResult // 借用结果，结构同 UbsMemBorrowExecute 返回值
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                               |
| ----------------------------------------------------- | --------------------------------------------------------- |
| invalid task id length: {len}                         | 入参任务ID长度非法（为空或大于等于256），{len} 为实际长度 |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle()            |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名                 |
| ubs_virt_agent_sync_task_query failed: {code}         | 底层C接口返回非0错误码，{code} 为具体错误码值             |

#### 约束与注意事项

- `taskId` 长度须大于 0 且小于 256 字节。
- 对应C接口：`ubs_virt_agent_sync_task_query`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    result, err := memfragmentation.UbsSyncTaskQuery("abc123")
    if err != nil {
        fmt.Printf("UbsSyncTaskQuery failed: %v\n", err)
        return
    }
    fmt.Printf("taskId: %s, status: %d, resultCode: %d\n",
        result.TaskId, result.Status, result.ResultCode)
}
```

### 3.6 UbsMemBorrow

> **适用场景**：虚机场景（内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemBorrow(param MemBorrowParam, isAsync bool) ([]MemBorrowResult, error)
```

#### 描述

大规格虚机场景，按节点借用内存。

#### 入参

| 入参    | 类型             | 必填 | 说明                         |
| ------- | ---------------- | ---- | ---------------------------- |
| param   | `MemBorrowParam` | 是   | 借用任务入参。               |
| isAsync | `bool`           | 是   | 是否异步执行借用任务标识符。 |

- 类型定义

```go
// NumaMetaInfo 借用方 NUMA 元信息
type NumaMetaInfo struct {
    NumaId int16
}

// MemBorrowParam 大规格虚机内存借用入参
type MemBorrowParam struct {
    SrcNid        string
    NumaMetaInfos []NumaMetaInfo
    BorrowSize    uint64 // 借用大小，单位MB
    PageSize      uint64 // 大页大小，单位KB，仅支持2048或1048576，必填
}
```

#### 返回值

成功时返回 `[]MemBorrowResult` 借用结果列表（结构同 UbsMemBorrowExecute 返回值）；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                    |
| ----------------------------------------------------- | ---------------------------------------------- |
| failed to allocate memory for numa_meta_infos         | 为 NUMA 元信息分配C侧内存失败                  |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名      |
| ubs_virt_agent_mem_borrow failed: {code}              | 底层C接口返回非0错误码，{code} 为具体错误码值  |

#### 约束与注意事项

- `NumaMetaInfos` 数量上限 512，超出部分截断。
- `SrcNid` 长度上限 48 字节，超长截断。
- `BorrowSize` 单位MB（大规格虚机路径，服务端内部换算为KB）。
- `PageSize` 为必填项，单位KB，仅支持 2048（2M大页）或 1048576（1G大页）；未设置（值为 0）或其他取值底层返回入参非法错误。
- 对应C接口：`ubs_virt_agent_mem_borrow`
- `NumaMetaInfos` 可根据 UbsMemFragmentationNodeInfoList 查询结果填写。

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    param := memfragmentation.MemBorrowParam{
        SrcNid: "node0",
        NumaMetaInfos: []memfragmentation.NumaMetaInfo{
            {NumaId: 0}, {NumaId: 1}, {NumaId: 2}, {NumaId: 3},
        },
        BorrowSize: 3145728, // 单位MB，此处借用3TB内存（UbsMemBorrow走MB语义，服务端已换算KB）
        PageSize:   2048,    // 单位KB，仅支持2048或1048576
    }
    results, err := memfragmentation.UbsMemBorrow(param, true)
    if err != nil {
        fmt.Printf("UbsMemBorrow failed: %v\n", err)
        return
    }
    for _, result := range results {
        fmt.Printf("borrowIds: %v, presentNumaIds: %v, taskId: %s\n",
            result.BorrowIds, result.PresentNumaIds, result.TaskId)
    }
}
```

## 4. 虚机场景—公共接口

### 4.1 接口清单

以下接口在虚机场景（`virt.sceneType=1`）下无条件注册，超分/内存碎片两种业务模式均可用（容器场景不可用）。

| 接口                              | 所属包           | 说明                         | 同步/异步 | 适用场景       |
| --------------------------------- | ---------------- | ---------------------------- | --------- | -------------- |
| `UbsMemFragmentationNodeInfo`     | memfragmentation | 查询本节点 NUMA 信息         | 同步      | 虚机·超分/碎片 |
| `UbsMemFragmentationVmInfo`       | memfragmentation | 查询本节点虚机信息           | 同步      | 虚机·超分/碎片 |
| `UbsMemFragmentationNodeInfoList` | memfragmentation | 查询集群内所有节点的内存信息 | 同步      | 虚机·超分/碎片 |
| `UbsMemFragmentationVmInfoList`   | memfragmentation | 查询集群内所有节点的虚机信息 | 同步      | 虚机·超分/碎片 |
| `UbsMemFragmentationHeartbeat`    | memfragmentation | 检测本节点 ubse 服务是否存活 | 同步      | 虚机·超分/碎片 |

### 4.2 UbsMemFragmentationNodeInfo

> **适用场景**：虚机场景（超分 `overCommitment` / 内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationNodeInfo() ([]NumaInfo, error)
```

#### 描述

查询本节点Numa信息。

#### 入参

无入参。

#### 返回值

成功时返回 `[]NumaInfo` NUMA信息列表；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// NumaPageData NUMA 大页信息
type NumaPageData struct {
    PageSize      uint64 // 页大小
    HugePageTotal uint64 // 大页总数
    HugePageFree  uint64 // 空闲大页数
}

// NumaInfo NUMA 内存碎片化信息
type NumaInfo struct {
    Timestamp    int64
    NodeId       string
    HostName     string
    NumaId       int16
    SocketId     int16  // NUMA绑定的CPU所属Socket ID
    IsLocal      bool   // 是否本地NUMA
    MemTotal     uint64 // 本NUMA总内存，单位kB
    MemFree      uint64 // 本NUMA空闲内存，单位kB
    HugePageData []NumaPageData
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                     | Description                                                           |
| --------------------------------------------------------- | --------------------------------------------------------------------- |
| failed to load symbol: dlopen handle is null              | 动态库未初始化，须先调用 dlopen.GetLibHandle()                        |
| failed to load symbol: failed to load func {funcName}     | 符号查找失败，{funcName} 为未找到的函数名                             |
| ubs_virt_agent_mem_fragmentation_node_info failed: {code} | 底层C接口返回非0错误码，{code} 为具体错误码值                         |
| invalid numa info count returned from C: {count}          | 底层返回的NUMA信息数量非法（大于512），{count} 为底层实际返回的数量值 |

#### 约束与注意事项

- 底层返回的NUMA信息数量上限 512，超出视为非法返回。
- 对应C接口：`ubs_virt_agent_mem_fragmentation_node_info`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    numaInfos, err := memfragmentation.UbsMemFragmentationNodeInfo()
    if err != nil {
        fmt.Printf("UbsMemFragmentationNodeInfo failed: %v\n", err)
        return
    }
    for _, numa := range numaInfos {
        fmt.Printf("nodeId: %s, numaId: %d, memTotal: %d kB, memFree: %d kB\n",
            numa.NodeId, numa.NumaId, numa.MemTotal, numa.MemFree)
        for _, page := range numa.HugePageData {
            fmt.Printf("  pageSize: %d, total: %d, free: %d\n",
                page.PageSize, page.HugePageTotal, page.HugePageFree)
        }
    }
}
```

### 4.3 UbsMemFragmentationVmInfo

> **适用场景**：虚机场景（超分 `overCommitment` / 内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationVmInfo() ([]VmDomainInfo, error)
```

#### 描述

查询本节点虚拟机信息。

#### 入参

无入参。

#### 返回值

成功时返回 `[]VmDomainInfo` 虚拟机信息列表；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// VmNumaInfo 虚拟机 NUMA 内存信息
type VmNumaInfo struct {
    NumaId   int16
    SocketId int16
    IsLocal  bool
    PageSize uint64 // 虚机在该NUMA上使用的大页大小，单位kB
    UsedMem  int64
}

// VmMetaData 虚拟机元数据
type VmMetaData struct {
    NodeId       string // 物理节点ID
    HostName     string // 物理节点主机名
    Uuid         string // 虚拟机UUID
    Name         string // 虚拟机名称
    State        string // 虚拟机状态
    VmCreateTime int64
    MaxMem       uint64
    Pid          int32 // 虚拟机进程ID
}

// VmDomainInfo 虚拟机信息
type VmDomainInfo struct {
    Timestamp int64
    MetaData  VmMetaData
    NumaInfo  []VmNumaInfo
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                   | Description                                                             |
| ------------------------------------------------------- | ----------------------------------------------------------------------- |
| failed to load symbol: dlopen handle is null            | 动态库未初始化，须先调用 dlopen.GetLibHandle()                          |
| failed to load symbol: failed to load func {funcName}   | 符号查找失败，{funcName} 为未找到的函数名                               |
| ubs_virt_agent_mem_fragmentation_vm_info failed: {code} | 底层C接口返回非0错误码，{code} 为具体错误码值                           |
| invalid vm info count returned from C: {count}          | 底层返回的虚拟机信息数量非法（大于300），{count} 为底层实际返回的数量值 |

#### 约束与注意事项

- 底层返回的虚拟机信息数量上限 300，超出视为非法返回。
- 对应C接口：`ubs_virt_agent_mem_fragmentation_vm_info`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    vmInfos, err := memfragmentation.UbsMemFragmentationVmInfo()
    if err != nil {
        fmt.Printf("UbsMemFragmentationVmInfo failed: %v\n", err)
        return
    }
    for _, vm := range vmInfos {
        fmt.Printf("name: %s, uuid: %s, state: %s, pid: %d\n",
            vm.MetaData.Name, vm.MetaData.Uuid, vm.MetaData.State, vm.MetaData.Pid)
        for _, numa := range vm.NumaInfo {
            fmt.Printf("  numaId: %d, usedMem: %d\n", numa.NumaId, numa.UsedMem)
        }
    }
}
```

### 4.4 UbsMemFragmentationNodeInfoList

> **适用场景**：虚机场景（超分 `overCommitment` / 内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationNodeInfoList() ([]NodeInfo, error)
```

#### 描述

查询集群内所有节点的内存信息。

#### 入参

无入参。

#### 返回值

成功时返回 `[]NodeInfo` 节点信息列表；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// NumaPageData NUMA 大页信息
type NumaPageData struct {
    PageSize      uint64 // 页大小
    HugePageTotal uint64 // 大页总数
    HugePageFree  uint64 // 空闲大页数
}

// NumaInfo NUMA 内存碎片化信息
type NumaInfo struct {
    Timestamp    int64
    NodeId       string
    HostName     string
    NumaId       int16
    SocketId     int16 // NUMA绑定的CPU所属Socket ID
    IsLocal      bool  // 是否本地NUMA
    MemTotal     uint64 // 本NUMA总内存，单位kB
    MemFree      uint64 // 本NUMA空闲内存，单位kB
    HugePageData []NumaPageData
}

// NodeInfo 集群节点信息（含该节点的 NUMA 内存信息）
type NodeInfo struct {
    NodeId    string
    NumaInfos []NumaInfo
    IsCurrent bool   // 是否是查询节点
    IsMaster  bool   // 是否是主节点
    PodId     string // 物理节点所在二层超节点ID（superPodId），读取失败时为空串
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                          | Description                                                      |
| -------------------------------------------------------------- | ---------------------------------------------------------------- |
| failed to load symbol: dlopen handle is null                   | 动态库未初始化，须先调用 dlopen.GetLibHandle()                   |
| failed to load symbol: failed to load func {funcName}          | 符号查找失败，{funcName} 为未找到的函数名                        |
| ubs_virt_agent_mem_fragmentation_node_info_list failed: {code} | 底层C接口返回非0错误码，{code} 为具体错误码值                    |
| invalid node count returned from C: {count}                    | 底层返回的节点数量非法（大于32），{count} 为底层实际返回的数量值 |

#### 约束与注意事项

- 底层返回的节点数量上限 32，超出返回错误；单节点 NUMA 数量上限 512，超出部分截断。
- 对应C接口：`ubs_virt_agent_mem_fragmentation_node_info_list`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    nodes, err := memfragmentation.UbsMemFragmentationNodeInfoList()
    if err != nil {
        fmt.Printf("UbsMemFragmentationNodeInfoList failed: %v\n", err)
        return
    }
    for _, node := range nodes {
        fmt.Printf("node: %s, isCurrent: %v, isMaster: %v, podId: %s, numaCount: %d\n",
            node.NodeId, node.IsCurrent, node.IsMaster, node.PodId, len(node.NumaInfos))
        for _, numa := range node.NumaInfos {
            fmt.Printf("  numaId: %d, memTotal: %d kB, memFree: %d kB\n",
                numa.NumaId, numa.MemTotal, numa.MemFree)
        }
    }
}
```

### 4.5 UbsMemFragmentationVmInfoList

> **适用场景**：虚机场景（超分 `overCommitment` / 内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationVmInfoList() ([]VmNodeInfo, error)
```

#### 描述

查询集群内所有节点的虚拟机信息。

#### 入参

无入参。

#### 返回值

成功时返回 `[]VmNodeInfo` 节点信息列表；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// VmNumaInfo 虚拟机 NUMA 内存信息
type VmNumaInfo struct {
    NumaId   int16
    SocketId int16
    IsLocal  bool
    PageSize uint64 // 虚机在该NUMA上使用的大页大小，单位kB
    UsedMem  int64
}

// VmMetaData 虚拟机元数据
type VmMetaData struct {
    NodeId       string // 物理节点ID
    HostName     string // 物理节点主机名
    Uuid         string // 虚拟机UUID
    Name         string // 虚拟机名称
    State        string // 虚拟机状态
    VmCreateTime int64
    MaxMem       uint64
    Pid          int32 // 虚拟机进程ID
}

// VmDomainInfo 虚拟机信息
type VmDomainInfo struct {
    Timestamp int64
    MetaData  VmMetaData
    NumaInfo  []VmNumaInfo
}

// VmNodeInfo 集群节点信息（含该节点的虚拟机信息）
type VmNodeInfo struct {
    NodeId    string
    VmInfos   []VmDomainInfo
    IsCurrent bool   // 是否是查询节点
    IsMaster  bool   // 是否是主节点
    PodId     string // 物理节点所在二层超节点ID（superPodId），读取失败时为空串
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                        | Description                                                      |
| ------------------------------------------------------------ | ---------------------------------------------------------------- |
| failed to load symbol: dlopen handle is null                 | 动态库未初始化，须先调用 dlopen.GetLibHandle()                   |
| failed to load symbol: failed to load func {funcName}        | 符号查找失败，{funcName} 为未找到的函数名                        |
| ubs_virt_agent_mem_fragmentation_vm_info_list failed: {code} | 底层C接口返回非0错误码，{code} 为具体错误码值                    |
| invalid node count returned from C: {count}                  | 底层返回的节点数量非法（大于32），{count} 为底层实际返回的数量值 |

#### 约束与注意事项

- 底层返回的节点数量上限 32，超出视为非法返回；单节点虚拟机信息数量上限 300，超出部分截断。
- 对应C接口：`ubs_virt_agent_mem_fragmentation_vm_info_list`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    nodes, err := memfragmentation.UbsMemFragmentationVmInfoList()
    if err != nil {
        fmt.Printf("UbsMemFragmentationVmInfoList failed: %v\n", err)
        return
    }
    for _, node := range nodes {
        fmt.Printf("node: %s, isCurrent: %v, isMaster: %v, podId: %s, vmCount: %d\n",
            node.NodeId, node.IsCurrent, node.IsMaster, node.PodId, len(node.VmInfos))
        for _, vm := range node.VmInfos {
            fmt.Printf("  name: %s, uuid: %s, state: %s, pid: %d\n",
                vm.MetaData.Name, vm.MetaData.Uuid, vm.MetaData.State, vm.MetaData.Pid)
            for _, numa := range vm.NumaInfo {
                fmt.Printf("    numaId: %d, usedMem: %d\n", numa.NumaId, numa.UsedMem)
            }
        }
    }
}
```

### 4.6 UbsMemFragmentationHeartbeat

> **适用场景**：虚机场景（超分 `overCommitment` / 内存碎片 `memFragmentation`）
> **所属包**：memfragmentation

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationHeartbeat() error
```

#### 描述

检测本节点ubse服务是否存活。

ubse服务正常运行时，接口在毫秒级返回 `nil`；ubse服务已停止时，接口立即返回 `error`。

#### 入参

无入参。

#### 返回值

成功（ubse服务存活）时返回 `nil`；失败（ubse服务不可用）时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                     | Description                                                     |
| --------------------------------------------------------- | --------------------------------------------------------------- |
| failed to load symbol: dlopen handle is null              | 动态库未初始化，须先调用 dlopen.GetLibHandle()                  |
| failed to load symbol: failed to load func {funcName}     | 符号查找失败，{funcName} 为未找到的函数名                       |
| ubs_virt_agent_mem_fragmentation_heartbeat failed: {code} | 底层C接口返回非0错误码（ubse服务不可用），{code} 为具体错误码值 |

#### 约束与注意事项

- 接口不承诺超时上限，ubse服务假死（进程存活但无响应）场景下调用方会阻塞至IPC内部超时才返回失败。调用方如有实时性要求，须自行控制调用节奏。
- 对应C接口：`ubs_virt_agent_mem_fragmentation_heartbeat`
- 判定依据仅为返回值是否为 `nil`，不验证ubse服务的业务功能是否正常。

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    if err := memfragmentation.UbsMemFragmentationHeartbeat(); err != nil {
        fmt.Printf("ubse service is not alive: %v\n", err)
        return
    }
    fmt.Printf("ubse service is alive\n")
}
```

## 5. 容器场景接口

### 5.1 接口清单

以下接口适用容器场景（`virt.sceneType=0`）。

| 接口                     | 所属包    | 说明                        | 同步/异步 | 适用场景 |
| ------------------------ | --------- | --------------------------- | --------- | -------- |
| `UbsInjectWaterLine`     | escape    | 注入节点水位线              | 同步      | 容器     |
| `escape.UbsMemBorrow`    | escape    | 按水位借用内存              | 同步      | 容器     |
| `UbsMemMigrate`          | escape    | 冷热页交换执行              | 同步      | 容器     |
| `escape.UbsMemReturn`    | escape    | 内存归还执行                | 同步      | 容器     |
| `UbsGetContainerMemInfo` | collector | 查询容器 PID 本地和远端内存 | 同步      | 容器     |
| `UbsGetContainerPids`    | collector | 指定容器查询容器内 PID      | 同步      | 容器     |

### 5.2 模块概述与调用时序

> **适用场景**：容器场景（`virt.sceneType=0`）。容器场景仅注册本模块接口，不注册第 4 章虚机查询接口及迁移、场景配置接口。

典型调用时序：

```text
注入水位（UbsInjectWaterLine）
   └──→ 容器内存/PID 查询（UbsGetContainerMemInfo / UbsGetContainerPids）
          └──→ 借用（escape.UbsMemBorrow）
                 └──→ 冷热页迁移（UbsMemMigrate，可选）
                        └──→ 归还（escape.UbsMemReturn）
```

> 注意：容器场景的 `escape.UbsMemBorrow` 与虚机碎片场景的 `memfragmentation.UbsMemBorrow` 同名但属于不同包、不同场景，请勿混淆。

### 5.3 UbsInjectWaterLine

> **适用场景**：容器场景
> **所属包**：escape

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsInjectWaterLine(w WaterMark) error
```

#### 描述

注入节点水线。

#### 入参

| 入参 | 类型        | 必填 | 说明           |
| ---- | ----------- | ---- | -------------- |
| w    | `WaterMark` | 是   | 水线配置信息。 |

- 类型定义

```go
// WaterMark waterMark value for escape
type WaterMark struct {
    HighWaterMark uint16  // 高水线
    LowWaterMark  uint16  // 低水线
}
```

#### 返回值

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                    |
| ----------------------------------------------------- | ---------------------------------------------- |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名      |
| ubs_container_inject_waterLine failed: {code}         | 底层C接口返回非0错误码，{code} 为具体错误码值  |

#### 约束与注意事项

暂无

- 对应C接口：`ubs_container_inject_waterLine`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    w := escape.WaterMark{HighWaterMark: 90, LowWaterMark: 80}
    if err := escape.UbsInjectWaterLine(w); err != nil {
        fmt.Printf("UbsInjectWaterLine failed: %v\n", err)
        return
    }
}
```

### 5.4 escape.UbsMemBorrow

> **适用场景**：容器场景
> **所属包**：escape

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsMemBorrow(memBorrowRequest MemBorrowRequest) ([]string, error)
```

#### 描述

内存借用执行。

#### 入参

| 入参             | 类型               | 必填 | 说明           |
| ---------------- | ------------------ | ---- | -------------- |
| memBorrowRequest | `MemBorrowRequest` | 是   | 内存借用请求体 |

- 类型定义

```go
// SrcLocation escape node's socketId and numaId
type SrcLocation struct {
    SocketId int
    NumaId   int
}

// BorrowParam escape node info
type BorrowParam struct {
    SrcNid       string
    SrcLocations []SrcLocation
}

// WaterMark waterMark value for escape
type WaterMark struct {
    HighWaterMark uint16
    LowWaterMark  uint16
}

// MemBorrowRequest request param to borrow mem for sdk
type MemBorrowRequest struct {
    BorrowParam BorrowParam
    BorrowSizes []uint64
    WaterMark   WaterMark
}
```

#### 返回值

成功时返回借用ID列表 `[]string`；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                              | Description                                    |
| -------------------------------------------------- | ---------------------------------------------- |
| [UbsMemBorrow] LoadFunc error: {err}               | 动态库未初始化或符号加载失败，{err} 为具体原因 |
| [UbsMemBorrow] function pointer cast failed        | 函数指针类型转换失败                           |
| ubs_virt_agent_waterline_mem_borrow failed: {code} | 底层C接口返回非0错误码，{code} 为具体错误码值  |

#### 约束与注意事项

- `SrcLocations` 数量上限 16，超出部分截断。
- `BorrowIds` 数量上限 64，超出部分截断。
- `ContainerParam` 数量上限 16，超出部分截断。
- `SrcNid` 长度上限 15 字节，超长截断。
- 对应C接口：`ubs_virt_agent_waterline_mem_borrow`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    req := escape.MemBorrowRequest{
        BorrowParam: escape.BorrowParam{
            SrcNid: "1",
            SrcLocations: []escape.SrcLocation{
                {SocketId: 1, NumaId: 1},
                {SocketId: 2, NumaId: 2},
            },
        },
        BorrowSizes: []uint64{128, 128},
        WaterMark:   escape.WaterMark{HighWaterMark: 85, LowWaterMark: 80},
    }
    borrowIds, err := escape.UbsMemBorrow(req)
    if err != nil {
        fmt.Printf("UbsMemBorrow failed: %v\n", err)
        return
    }
    fmt.Printf("borrowIds: %v\n", borrowIds)
}
```

### 5.5 UbsMemMigrate

> **适用场景**：容器场景
> **所属包**：escape

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsMemMigrate(req MemMigrateRequest) error
```

#### 描述

内存冷热页交换执行。

#### 入参

| 入参 | 类型                | 必填 | 说明                   |
| ---- | ------------------- | ---- | ---------------------- |
| req  | `MemMigrateRequest` | 是   | 内存冷热页交换请求体。 |

- 类型定义

```go
// SrcLocation escape node's socketId and numaId
type SrcLocation struct {
    SocketId int
    NumaId   int
}

// BorrowParam escape node info
type BorrowParam struct {
    SrcNid       string
    SrcLocations []SrcLocation
}

// ContainerParam mem ratio of pid
type ContainerParam struct {
    Pid   uint64
    Ratio int
}

// MemMigrateRequest request param to migrate mem for sdk
type MemMigrateRequest struct {
    BorrowParam    BorrowParam
    BorrowIds      []string
    ContainerParam []ContainerParam
}
```

#### 返回值

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                               | Description                                    |
| --------------------------------------------------- | ---------------------------------------------- |
| [UbsMemMigrate] LoadFunc error: {err}               | 动态库未初始化或符号加载失败，{err} 为具体原因 |
| ubs_virt_agent_waterline_mem_migrate failed: {code} | 底层C接口返回非0错误码，{code} 为具体错误码值  |

#### 约束与注意事项

- `SrcLocations` 数量上限 16，超出部分截断。
- `BorrowIds` 数量上限 64，超出部分截断。
- `ContainerParam` 数量上限 16，超出部分截断。
- `ContainerParam` 不可为 nil（为 nil 时空指针会传入底层C接口，行为未定义）。
- 该接口不校验函数指针是否为空。
- 对应C接口：`ubs_virt_agent_waterline_mem_migrate`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    req := escape.MemMigrateRequest{
        BorrowParam: escape.BorrowParam{
            SrcNid: "1",
            SrcLocations: []escape.SrcLocation{
                {SocketId: 1, NumaId: 1},
                {SocketId: 2, NumaId: 2},
            },
        },
        BorrowIds: []string{"1-abc123"},
        ContainerParam: []escape.ContainerParam{
            {Pid: 12345, Ratio: 25},
        },
    }
    if err := escape.UbsMemMigrate(req); err != nil {
        fmt.Printf("UbsMemMigrate failed: %v\n", err)
        return
    }
}
```

### 5.6 UbsMemReturn

> **适用场景**：容器场景
> **所属包**：escape

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsMemReturn(req MemReturnRequest) error
```

#### 描述

内存归还执行。

#### 入参

| 入参 | 类型               | 必填 | 说明             |
| ---- | ------------------ | ---- | ---------------- |
| req  | `MemReturnRequest` | 是   | 内存归还请求体。 |

- 类型定义

```go
// SrcLocation escape node's socketId and numaId
type SrcLocation struct {
    SocketId int
    NumaId   int
}

// BorrowParam escape node info
type BorrowParam struct {
    SrcNid       string
    SrcLocations []SrcLocation
}

// MemReturnRequest request param to return mem for sdk
type MemReturnRequest struct {
    BorrowParam BorrowParam
    BorrowIds   []string
    Pids        []uint64
}
```

#### 返回值

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                              | Description                                   |
| ------------------------------------------------------------------ | --------------------------------------------- |
| convertReturnRequestToC returned nil                               | 请求参数转换为C结构失败（内存分配失败）       |
| failed to load symbol 'ubs_virt_agent_waterline_mem_return': {err} | 符号加载失败，{err} 为具体原因                |
| loaded nil function pointer for mem_return                         | 加载的函数指针为空                            |
| invalid function pointer for mem_return                            | 函数指针类型无效                              |
| ubs_virt_agent_waterline_mem_return failed: {code}                 | 底层C接口返回非0错误码，{code} 为具体错误码值 |

#### 约束与注意事项

- `SrcLocations` 数量上限 16，超出部分截断。
- `BorrowIds` 数量上限 16，超出部分截断。
- `Pids` 数量上限 64，超出部分截断。
- 借用 ID 长度上限 127 字节，超长截断。
- 对应C接口：`ubs_virt_agent_waterline_mem_return`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    req := escape.MemReturnRequest{
        BorrowParam: escape.BorrowParam{
            SrcNid: "1",
            SrcLocations: []escape.SrcLocation{
                {SocketId: 1, NumaId: 1},
                {SocketId: 2, NumaId: 2},
            },
        },
        BorrowIds: []string{"1-abc123"},
        Pids:      []uint64{12345, 23456},
    }
    if err := escape.UbsMemReturn(req); err != nil {
        fmt.Printf("UbsMemReturn failed: %v\n", err)
        return
    }
}
```

### 5.7 UbsGetContainerMemInfo

> **适用场景**：容器场景
> **所属包**：collector

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/collector"

func UbsGetContainerMemInfo(param PidParam) ([]ContainerMemInfo, error)
```

#### 描述

查询容器Pid本地和远端内存。

#### 入参

| 入参  | 类型       | 必填 | 说明              |
| ----- | ---------- | ---- | ----------------- |
| param | `PidParam` | 是   | 待查询的pid信息。 |

- 类型定义

```go
// PidParam Information about the PID passed by the SDK
type PidParam struct {
    SrcNid string
    Pids   []uint64
}

// ContainerMemInfo Container scenario is used to store the remote and local memory used by the collected PIDs.
type ContainerMemInfo struct {
    Pid           uint64    // 进程ID
    NumaIds       []uint16  // 本地NUMA ID列表
    LocalMemSize  uint64    // 本地已用内存
    RemoteMemSize uint64    // 远端已用内存
}
```

#### 返回值

成功时返回 `[]ContainerMemInfo` pid对应的内存信息列表（进程存在但无内存使用时为空列表）；失败时返回 `error`，请见`错误 ERRORS`。

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                                     |
| ----------------------------------------------------- | --------------------------------------------------------------- |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle()                  |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名                       |
| ubs_container_info_query failed: {code}               | 底层C接口返回非0错误码，{code} 为具体错误码值                   |
| invalid container count returned from C: {count}      | 底层返回的内存信息数量非法（小于0或大于2048），{count} 为实际值 |

#### 约束与注意事项

- `Pids` 数量上限 2048，超出部分截断。
- `SrcNid` 长度上限 15 字节，超长截断。
- 与 RMRS 接口对齐：进程存在但没有分配内存时，空列表表示当前无内存使用，属正常返回。
- 对应C接口：`ubs_container_info_query`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/collector"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    param := collector.PidParam{
        SrcNid: "1",
        Pids:   []uint64{123},
    }
    memInfos, err := collector.UbsGetContainerMemInfo(param)
    if err != nil {
        fmt.Printf("UbsGetContainerMemInfo failed: %v\n", err)
        return
    }
    for _, info := range memInfos {
        fmt.Printf("pid=%d, localMem=%d, remoteMem=%d, numaIds=%v\n",
            info.Pid, info.LocalMemSize, info.RemoteMemSize, info.NumaIds)
    }
}
```

### 5.8 UbsGetContainerPids

> **适用场景**：容器场景
> **所属包**：collector

#### 函数原型

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/collector"

func UbsGetContainerPids(containerIds []string) ([]ContainerPidInfo, error)
```

#### 描述

指定容器查询容器内Pid。

#### 入参

| 入参         | 类型       | 必填 | 说明         |
| ------------ | ---------- | ---- | ------------ |
| containerIds | `[]string` | 是   | 容器ID列表。 |

#### 返回值

成功时返回 `[]ContainerPidInfo` 容器内pid信息列表；失败时返回 `error`，请见`错误 ERRORS`。

- 类型定义

```go
// ContainerPidInfo storage the list of PIDs under the corresponding container ID
type ContainerPidInfo struct {
    Pids        []uint64
    ContainerId string
}
```

#### 错误码

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                                 | Description                                                |
| ----------------------------------------------------- | ---------------------------------------------------------- |
| failed to load symbol: dlopen handle is null          | 动态库未初始化，须先调用 dlopen.GetLibHandle()             |
| failed to load symbol: failed to load func {funcName} | 符号查找失败，{funcName} 为未找到的函数名                  |
| ubs_container_get_container_pids failed: {code}       | 底层C接口返回非0错误码，{code} 为具体错误码值              |
| invalid pid count returned from C: {count}            | 底层返回的 PID 数量非法（小于0或大于64），{count} 为实际值 |

#### 约束与注意事项

- `containerIds` 数量上限 64，超出部分截断。
- 单个容器 ID 长度上限 127 字节，超长截断。
- 对应C接口：`ubs_container_get_container_pids`

#### 示例

以下程序为使用示例。

```go
package main

import (
    "fmt"

    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/collector"
    "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
)

func main() {
    if err := dlopen.GetLibHandle(); err != nil {
        fmt.Printf("dlopen failed: %v\n", err)
        return
    }
    defer dlopen.CloseLibHandle()

    pidInfos, err := collector.UbsGetContainerPids([]string{"123", "456"})
    if err != nil {
        fmt.Printf("UbsGetContainerPids failed: %v\n", err)
        return
    }
    for _, info := range pidInfos {
        fmt.Printf("containerId=%s, pids=%v\n", info.ContainerId, info.Pids)
    }
}
```

## 6. 典型场景编排

### 6.1 虚机·内存碎片整理

```text
借用执行：UbsMemBorrowExecute（同步/异步）
   └──→ 异步任务查询：UbsSyncTaskQuery
          └──→ 归还：UbsMemReturn
                 └──→ 异常回滚：UbsMemRollback
```

### 6.2 虚机·大规格虚机创建/删除

```text
查询集群资源：UbsMemFragmentationNodeInfoList
   └──→ 批量借用：UbsMemBorrow（异步）
          └──→ 任务查询：UbsSyncTaskQuery
                 └──→ 归还：UbsMemReturn（异步）──→ 任务查询
```

> 说明：Go SDK 暂未封装冷热页流动使能接口（`page_swap_enable`）。大规格虚机创建后如需按配额使能冷热页流动，请使用 C 接口 `ubs_virt_agent_page_swap_enable`（Python 侧为 `ubs_page_swap_enable`）。

详见[大规格虚机最佳实践](../../example/base_case_for_big_vm/best_case_for_big_vm.md)。

### 6.3 容器·超分逃生

```text
注入水位：UbsInjectWaterLine
   └──→ 查询容器内存/PID：UbsGetContainerMemInfo / UbsGetContainerPids
          └──→ 借用：escape.UbsMemBorrow
                 └──→ 冷热页迁移（可选）：UbsMemMigrate
                        └──→ 归还：escape.UbsMemReturn
```

## 附录 A 类型索引

| 类型                                                                                                                     | 所属包             | 说明                       |
| ------------------------------------------------------------------------------------------------------------------------ | ------------------ | -------------------------- |
| `BorrowStrategy`、`DstNumaInfo`、`BorrowSetting`、`MemBorrowResult`                                                      | `memfragmentation` | 内存碎片借用策略/执行/结果 |
| `NumaMetaInfo`、`MemBorrowParam`                                                                                         | `memfragmentation` | 大规格虚机批量借用入参     |
| `RollbackParam`、`AsyncTaskInfo`、`AsyncTaskStatus`                                                                      | `memfragmentation` | 回滚入参与异步任务         |
| `NumaInfo`、`NumaPageData`、`VmDomainInfo`、`VmNumaInfo`、`VmMetaData`、`NodeInfo`、`VmNodeInfo`                         | `memfragmentation` | 查询结果类型               |
| `WaterMark`、`BorrowParam`、`SrcLocation`、`MemBorrowRequest`、`MemMigrateRequest`、`MemReturnRequest`、`ContainerParam` | `escape`           | 容器场景请求与水位类型     |
| `PidParam`、`ContainerMemInfo`、`ContainerPidInfo`                                                                       | `collector`        | 容器查询类型               |

> 基础设施接口（`GetLibHandle`、`LoadFunc`、`CloseLibHandle`）见 [1.4 接口清单](#14-接口清单)。

## 附录 B 错误处理与返回码

各接口通过 `error` 返回错误，具体错误项见各接口“错误码”小节；底层 C 返回码定义参见 C 接口参考手册 2.1 节。

## 附录 C 权限点

见 2.2 并发与鉴权。

## 附录 D 示例代码索引

以下路径相对于 `src/addons/virt_agent/`。

| 位置                                               | 说明                               |
| -------------------------------------------------- | ---------------------------------- |
| `sdk/go/sample/query_verify/main.go`               | 查询接口验证示例（含 dlopen 加载） |
| `sdk/go/sample/memfrag_verify/main.go`             | 内存碎片借用验证示例               |
| `sdk/go/memfragmentation/memfragmentation_test.go` | 接口单测示例                       |

## 附录 E 版本与兼容性

| 版本  | 说明                                                 |
| ----- | ---------------------------------------------------- |
| 1.0.1 | 初版；接口总数 16；依赖 `libubs-virt-agent.so` 1.0.1 |
