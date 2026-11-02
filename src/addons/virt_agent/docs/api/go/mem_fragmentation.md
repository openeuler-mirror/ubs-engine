# 1. UbsMemBorrowExecute

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemBorrowExecute(setting BorrowSetting) (MemBorrowResult, error)
```

## 描述 DESCRIPTION

根据借用决策结果执行内存借用。

## 参数 Parameters

| name    | IN/OUT | description               |
|---------|--------|---------------------------|
| setting | IN     | 内存借用策略信息，是否异步执行开关。 |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `MemBorrowResult` 借用结果；失败时返回 `error`，请见`错误 ERRORS`。

- 数据结构说明

```go
// MemBorrowResult 内存借用结果
type MemBorrowResult struct {
    BorrowIds      []string // 借用ID列表
    PresentNumaIds []uint16 // 借入的远端NUMA ID列表
    TaskId         string   // 借用任务ID
}
```

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_borrow_execute failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

- `DestNumaInfos` 数量上限 100，超出部分截断；单条目标 NUMA 数量上限 1，超出截断。
- 字符串超长截断：节点ID上限 48 字节。
- `PageSize` 为必填项，单位KB，仅支持 2048（2M大页）或 1048576（1G大页）；未设置（值为 0）或其他取值底层返回参数非法错误。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_borrow_execute`

## 样例 EXAMPLES

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

# 2. UbsMemReturn

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemReturn(isAsync bool) (string, error)
```

## 描述 DESCRIPTION

根据参数进行内存归还。

## 参数 Parameters

| name    | IN/OUT | description |
|---------|--------|-------------|
| isAsync | IN     | 异步执行开关。true：表示接口内部为异步执行内存归还。false：表示接口内部为同步执行内存归还。 |

## 返回值 RETURN VALUE

成功时返回后台任务ID `string`（异步执行时可通过 UbsSyncTaskQuery 查询任务状态）；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_return failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid task id returned from C              | 底层返回的后台任务ID为空指针 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_return`

## 样例 EXAMPLES

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

# 3. UbsMemRollback

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemRollback(param RollbackParam) error
```

## 描述 DESCRIPTION

借用内存回滚。

## 参数 Parameters

| name  | IN/OUT | description                            |
|-------|--------|----------------------------------------|
| param | IN     | 回滚的入参，包括节点id、节点上的borrowId列表。 |

- 数据结构说明

```go
// RollbackParam 借用内存回滚入参
type RollbackParam struct {
    NodeId       string
    BorrowIdList []string
}
```

## 返回值 RETURN VALUE

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_rollback failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

- `BorrowIdList` 数量上限 100，超出部分截断；借用ID长度上限 128 字节，超长截断。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_rollback`

## 样例 EXAMPLES

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

# 4. UbsSyncTaskQuery

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsSyncTaskQuery(taskId string) (AsyncTaskInfo, error)
```

## 描述 DESCRIPTION

借用归还任务查询。

## 参数 Parameters

| name    | IN/OUT | description  |
|---------|--------|--------------|
| taskId  | IN     | 后台任务id。     |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `AsyncTaskInfo` 任务执行结果信息；失败时返回 `error`，请见`错误 ERRORS`。

- 数据结构说明

```go
// AsyncTaskInfo 异步任务查询结果
type AsyncTaskInfo struct {
    TaskId          string
    Status          AsyncTaskStatus
    ResultCode      uint32
    MemBorrowResult MemBorrowResult // 借用结果，结构同 UbsMemBorrowExecute 返回值
}
```

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| invalid task id length: \{len\}               | 入参任务ID长度非法（为空或大于等于256），\{len\} 为实际长度 |
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_sync_task_query failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

- `taskId` 长度须大于 0 且小于 256 字节。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_sync_task_query`

## 样例 EXAMPLES

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

# 5. UbsMemBorrow

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemBorrow(param MemBorrowParam, isAsync bool) ([]MemBorrowResult, error)
```

## 描述 DESCRIPTION

大规格虚机场景，按节点借用内存。

## 参数 Parameters

| name    | IN/OUT | description                  |
|---------|--------|------------------------------|
| param   | IN     | 借用任务入参。                    |
| isAsync | IN     | 是否异步执行借用任务标识符。            |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `[]MemBorrowResult` 借用结果列表（结构同 UbsMemBorrowExecute 返回值）；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to allocate memory for numa_meta_infos | 为 NUMA 元信息分配C侧内存失败 |
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_borrow failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

- `NumaMetaInfos` 数量上限 512，超出部分截断。
- `SrcNid` 长度上限 48 字节，超长截断。
- `BorrowSize` 单位MB（大规格虚机路径，服务端内部换算为KB）。
- `PageSize` 为必填项，单位KB，仅支持 2048（2M大页）或 1048576（1G大页）；未设置（值为 0）或其他取值底层返回参数非法错误。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_borrow`
- `NumaMetaInfos` 可根据 UbsMemFragmentationNodeInfoList 查询结果填写。

## 样例 EXAMPLES

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

# 6. UbsMemFragmentationHeartbeat

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationHeartbeat() error
```

## 描述 DESCRIPTION

检测本节点ubse服务是否存活。

ubse服务正常运行时，接口在毫秒级返回 `nil`；ubse服务已停止时，接口立即返回 `error`。

## 参数 Parameters

无

## 返回值 RETURN VALUE

成功（ubse服务存活）时返回 `nil`；失败（ubse服务不可用）时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_fragmentation_heartbeat failed: \{code\} | 底层C接口返回非0错误码（ubse服务不可用），\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

- 接口不承诺超时上限，ubse服务假死（进程存活但无响应）场景下调用方会阻塞至IPC内部超时才返回失败。调用方如有实时性要求，须自行控制调用节奏。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_fragmentation_heartbeat`
- 判定依据仅为返回值是否为 `nil`，不验证ubse服务的业务功能是否正常。

## 样例 EXAMPLES

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
