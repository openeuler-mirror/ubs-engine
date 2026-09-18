# 1. UbsMemBorrow

## 库 LIBRARY

Go SDK escape 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsMemBorrow(memBorrowRequest MemBorrowRequest) ([]string, error)
```

## 描述 DESCRIPTION

内存借用执行。

## 参数 Parameters

| name             | IN/OUT | description |
|------------------|--------|-------------|
| memBorrowRequest | IN     | 内存借用请求体 |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回借用ID列表 `[]string`；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                    | Description |
|------------------------------------------|-------------|
| dlopen handle is null                    | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| dlopen failed: \{err\}                   | 动态库加载失败，\{err\} 为 dlerror 返回的底层错误描述 |
| failed to load func \{funcName\}         | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_waterline_mem_borrow failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_waterline_mem_borrow`

## 样例 EXAMPLES

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

# 2. UbsMemMigrate

## 库 LIBRARY

Go SDK escape 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsMemMigrate(req MemMigrateRequest) error
```

## 描述 DESCRIPTION

内存冷热页交换执行。

## 参数 Parameters

| name | IN/OUT | description         |
|------|--------|---------------------|
| req  | IN     | 内存冷热页交换请求体。 |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| dlopen handle is null                        | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| dlopen failed: \{err\}                       | 动态库加载失败，\{err\} 为 dlerror 返回的底层错误描述 |
| failed to load func \{funcName\}             | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_waterline_mem_migrate failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

- ContainerParam 不可为 nil（为 nil 时空指针会传入底层C接口，行为未定义）

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_waterline_mem_migrate`

## 样例 EXAMPLES

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

# 3. UbsMemReturn

## 库 LIBRARY

Go SDK escape 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsMemReturn(req MemReturnRequest) error
```

## 描述 DESCRIPTION

内存归还执行。

## 参数 Parameters

| name | IN/OUT | description     |
|------|--------|-----------------|
| req  | IN     | 内存归还请求体。   |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| dlopen handle is null                        | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| dlopen failed: \{err\}                       | 动态库加载失败，\{err\} 为 dlerror 返回的底层错误描述 |
| failed to load func \{funcName\}             | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_waterline_mem_return failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_waterline_mem_return`

## 样例 EXAMPLES

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

# 4. UbsGetContainerMemInfo

## 库 LIBRARY

Go SDK collector 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/collector"

func UbsGetContainerMemInfo(param PidParam) ([]ContainerMemInfo, error)
```

## 描述 DESCRIPTION

查询容器Pid本地和远端内存。

## 参数 Parameters

| name  | IN/OUT | description          |
|-------|--------|----------------------|
| param | IN     | 待查询的pid信息。       |

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `[]ContainerMemInfo` pid对应的内存信息列表（进程存在但无内存使用时为空列表）；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                    | Description |
|------------------------------------------|-------------|
| dlopen handle is null                    | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| dlopen failed: \{err\}                   | 动态库加载失败，\{err\} 为 dlerror 返回的底层错误描述 |
| failed to load symbol \{funcName\}       | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_container_info_query failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid container count returned from C: \{count\} | 底层返回的内存信息数量非法（小于0或大于2048），\{count\} 为底层实际返回的数量值 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 对应C接口：`ubs_container_info_query`
- 与 RMRS 接口对齐：进程存在但没有分配内存时，空列表表示当前无内存使用，属正常返回。

## 样例 EXAMPLES

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

# 5. UbsGetContainerPids

## 库 LIBRARY

Go SDK collector 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/collector"

func UbsGetContainerPids(containerIds []string) ([]ContainerPidInfo, error)
```

## 描述 DESCRIPTION

指定容器查询容器内Pid。

## 参数 Parameters

| name         | IN/OUT | description  |
|--------------|--------|--------------|
| containerIds | IN     | 容器ID列表。     |

## 返回值 RETURN VALUE

成功时返回 `[]ContainerPidInfo` 容器内pid信息列表；失败时返回 `error`，请见`错误 ERRORS`。

- 数据结构说明

```go
// ContainerPidInfo storage the list of PIDs under the corresponding container ID
type ContainerPidInfo struct {
    Pids        []uint64
    ContainerId string
}
```

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                          | Description |
|------------------------------------------------|-------------|
| dlopen handle is null                          | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| dlopen failed: \{err\}                         | 动态库加载失败，\{err\} 为 dlerror 返回的底层错误描述 |
| failed to load symbol \{funcName\}             | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_container_get_container_pids failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid pid count returned from C: \{count\}   | 底层返回的pid信息数量非法（小于等于0或大于64），\{count\} 为底层实际返回的数量值 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 对应C接口：`ubs_container_get_container_pids`

## 样例 EXAMPLES

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

# 6. UbsInjectWaterLine

## 库 LIBRARY

Go SDK escape 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/escape"

func UbsInjectWaterLine(w WaterMark) error
```

## 描述 DESCRIPTION

注入节点水线。

## 参数 Parameters

| name | IN/OUT | description |
|------|--------|-------------|
| w    | IN     | 水线配置信息。   |

- 数据结构说明

```go
// WaterMark waterMark value for escape
type WaterMark struct {
    HighWaterMark uint16  // 高水线
    LowWaterMark  uint16  // 低水线
}
```

## 返回值 RETURN VALUE

成功时返回 `nil`；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| dlopen handle is null                        | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| dlopen failed: \{err\}                       | 动态库加载失败，\{err\} 为 dlerror 返回的底层错误描述 |
| failed to load func \{funcName\}             | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_container_inject_waterLine failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 对应C接口：`ubs_container_inject_waterLine`

## 样例 EXAMPLES

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
