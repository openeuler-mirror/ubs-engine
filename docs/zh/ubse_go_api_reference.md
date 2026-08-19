# UBS Engine Go SDK API 参考指南

## 前言

**概述**

UBS Engine（UBSE）提供了 UBSE 程序及其相应的 SDK 开发库。开发者可以利用该 SDK 访问 UBSE 提供的服务，从而实现对存储等资源的调度与管理。

UBSE SDK 通过三种形式提供：

- so 动态链接库：C 和 C++ 语言的开发。
- Python 开发库：支持 python 协议栈开发。
- Go SDK 源码库：支持 Go 协议栈通过 import 导入开发。

**介绍**

本文主要介绍 UBS Engine Go SDK 对外提供的 API。Go SDK 通过 IPC 直接与 UBSE 守护进程通信，封装了通信细节，向上层应用暴露一组类型安全的 Go 函数。SDK 按子系统划分为多个 package，每个 package 对应一个章节，当前包含以下 package：

- `ssu`：SSU（Storage Service Unit，存储服务单元）子系统，提供存储空间分配、查询、挂载/卸载、访问权限管理及 FE/VFE 设备管理等功能。

后续可能扩展其他子系统的 Go SDK 文档。

**约束条件**

UBS Engine 对外接口的访问入口是 sock 文件 `/run/ubse/ubse.sock`，sock 文件的安全访问基于文件权限控制，其属主为 ubse，mode 为 660，如下：

```shell
srw-rw---- ubse ubse /run/ubse/ubse.sock
```

加入 ubse group 的用户，才能访问 UBSE 的 SDK 接口。所有 exported 函数均为阻塞式调用，调用前会对入参做基本校验，校验失败时直接返回 `error`，不会发起 IPC 请求；调用失败时返回非 `nil` 的 `error`，成功时返回 `nil`。

## package ssu

**包 PACKAGE**

`ssu` (导入路径 `atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu`，源文件 `src/sdk/go/ssu/ubs_engine_ssu.go`)

**依赖**

| 依赖包 | 说明 |
| ------ | ---- |
| `fmt` | 标准库，用于内部辅助函数打印错误调试信息 |
| `atomgit.com/openeuler/ubs-engine.git/src/sdk/go/errcode` | 提供错误码定义（如 `ErrAlreadyAttached`） |
| `atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ipc` | 提供 `ipc.InvokeCall`，负责与 `ubse` 守护进程通信 |
| `atomgit.com/openeuler/ubs-engine.git/src/sdk/go/pack` | 提供 `pack.NewBinaryPacker`，用于请求/响应的二进制序列化 |

> 说明：本包内的 opCode 常量（如 `UbseSsuAllocSpaceReq`、`UbseModuleCode` 等）以及 `validate*` / `pack*` / `unpack*` 系列辅助函数定义在同 package 的其他源文件中，不属于本文件的导出 API，因此不在本文档中列出。

**导入示例**

```go
import "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
```

**常量**

| 常量名 | 类型 | 值 | 说明 |
| ------ | ---- | -- | ---- |
| `UbsSsuMaxNameLength` | int | 48 | 请求标识最大 48 个字符，含结尾字符 `'\0'` |
| `UbsSsuMaxTenantLength` | int | 17 | 请求方 UPI（租户隔离标识）最大长度，含结尾字符 `'\0'` |
| `UbsSsuMaxNqnLength` | int | 69 | NVMe NQN 最大长度 69 个字符，含结尾字符 `'\0'` |
| `UbsSsuMaxEidLength` | int | 17 | EID 最大长度，含结尾字符 `'\0'` |
| `UbsSsuMaxUuidLength` | int | 37 | UUID 标准长度 37 个字符，含结尾字符 `'\0'` |
| `UbsSsuMaxDevPathLength` | int | 63 | 设备路径最大长度，含结尾字符 `'\0'` |
| `UbsSsuMaxDevNameLength` | int | 33 | 聚合块设备名称最大长度，含结尾字符 `'\0'` |
| `UbsSsuRaid5MinMemberNum` | int | 3 | RAID5 最少成员设备数 |
| `UbsSsuMaxGuidLength` | int | 32 | GUID 最大长度 32 个字符，**不含**结尾字符 `'\0'` |

**类型定义**

枚举类型：

```go
// UbsSsuLbaFormat LBA 格式
type UbsSsuLbaFormat uint32
const (
    Fmt512 UbsSsuLbaFormat = 512  // 512B
    Fmt4K  UbsSsuLbaFormat = 4096 // 4K
)

// UbsSsuAggregationRaidLevel 聚合 RAID 级别
type UbsSsuAggregationRaidLevel uint8
const (
    Raid0 UbsSsuAggregationRaidLevel = 0 // RAID0 条带化
    Raid5 UbsSsuAggregationRaidLevel = 5 // RAID5 条带化带校验
)

// UbsSsuChunkSize 条带化 chunk 大小(KB)
type UbsSsuChunkSize uint32
const (
    Size4K   UbsSsuChunkSize = 4
    Size16K  UbsSsuChunkSize = 16
    Size32K  UbsSsuChunkSize = 32
    Size64K  UbsSsuChunkSize = 64
    Size128K UbsSsuChunkSize = 128
    Size256K UbsSsuChunkSize = 256
    Size512K UbsSsuChunkSize = 512
)

// UbsSsuAllocStrategy 分配策略
type UbsSsuAllocStrategy uint8
const (
    Striped UbsSsuAllocStrategy = 0 // 分布式策略，尽量从多个设备均等分配，适用于条带化编址使用场景
    Linear  UbsSsuAllocStrategy = 1 // 顺序策略，尽量从单个设备分配，适用于线性编址使用场景
    Normal  UbsSsuAllocStrategy = 2 // 普通策略，挂载时只挂载nvme裸设备，不聚合块设备，该值为推荐值
)
```

结构体类型：

```go
// UbsSsuNamespaceInfo 命名空间信息
type UbsSsuNamespaceInfo struct {
    TgtEid           string
    TgtNqn           string
    NsUuid           string
    NamespaceId      uint32
    NsDevPath        string
    NsSize           uint64
    LbaFormat        UbsSsuLbaFormat
    AllowHostNqnList []string
}

// UbsSsuAllocResult 分配存储空间结果
type UbsSsuAllocResult struct {
    Name       string
    Strategy   UbsSsuAllocStrategy
    Namespaces []UbsSsuNamespaceInfo
}

// UbsSsuAllocSpaceReq 分配存储空间请求参数
type UbsSsuAllocSpaceReq struct {
    Name      string
    NsSize    uint64
    NsNum     uint32
    LbaFormat UbsSsuLbaFormat
    Strategy  UbsSsuAllocStrategy
    Tenant    string
}

// UbsSsuSpaceReq 挂载|卸载存储空间请求参数
type UbsSsuSpaceReq struct {
    Name   string
    Nqn    string
    SrcEid string
}

// UbsSsuLinearSpaceReq 挂载|卸载线性编址存储空间请求参数
type UbsSsuLinearSpaceReq struct {
    Name    string
    Nqn     string
    SrcEid  string
    DevName string
}

// UbsSsuStripedSpaceReq 挂载|卸载条带化编址存储空间请求参数
type UbsSsuStripedSpaceReq struct {
    Name      string
    Nqn       string
    SrcEid    string
    DevName   string
    Level     UbsSsuAggregationRaidLevel
    ChunkSize UbsSsuChunkSize
}

// UbsUbVfe 虚拟功能单元(VFE)信息
type UbsUbVfe struct {
    SlotId              uint8
    ChipId              uint8
    DieId               uint8
    PfeId               uint16
    VfeId               uint16
    VfeGuid             string
    BindBusInstanceGuid string
}

// UbsSsuFe 功能单元(FE)信息
type UbsSsuFe struct {
    SlotId  uint8
    ChipId  uint8
    DieId   uint8
    PfeId   uint16
    PfeGuid string
    VfeList []UbsUbVfe
}

// UbsSsuConnectInfo 存储空间连接信息
type UbsSsuConnectInfo struct {
    SrcEid  string
    TgtEid  string
    TgtNqn  string
    HostNqn string
    NsUuid  string
    NsId    uint32
}

// UbsSsuNsStats 存储空间状态
type UbsSsuNsStats struct {
    NsUuid    string
    NsId      uint32
    TotalSize uint64
    UsedSize  uint64
}
```

**错误处理**

所有函数在调用失败时返回非 `nil` 的 `error`；成功时返回 `nil`。错误来源包括：参数校验失败、IPC 通信失败、守护进程返回的错误等。其中 `UbsSsuAttachSpace` / `UbsSsuAttachLinearSpace` / `UbsSsuAttachStripedSpace` 在空间已挂载（`errcode.ErrAlreadyAttached`）时，会同时返回已挂载的设备路径和原始错误，调用方可通过 `errors.Is(err, errcode.ErrAlreadyAttached)` 判断并使用返回的路径。

### UbsSsuListAllocInfo

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuListAllocInfo() ([]UbsSsuAllocResult, error)
```

**描述 DESCRIPTION**

获取系统中所有已分配的 SSU 存储空间详细信息，包括命名空间列表、容量、LBA 格式和使用类型等。

**参数 PARAMETERS**

无参数。

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]UbsSsuAllocResult` | 已分配空间信息列表 |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

无。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```go
package main

import (
    "fmt"
    "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
)

func main() {
    results, err := ssu.UbsSsuListAllocInfo()
    if err != nil {
        fmt.Printf("list failed: %v\n", err)
        return
    }
    for _, r := range results {
        fmt.Printf("name=%s, ns_cnt=%d\n", r.Name, len(r.Namespaces))
    }
}
```

### UbsSsuAllocSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuAllocSpace(req UbsSsuAllocSpaceReq) (UbsSsuAllocResult, error)
```

**描述 DESCRIPTION**

根据请求参数分配指定数量和大小的命名空间，支持分布式分配（`Striped`）、顺序分配（`Linear`）和普通分配（`Normal`）三种策略。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuAllocSpaceReq` | 分配请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `UbsSsuAllocResult` | 分配结果 |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 已存在错误 | 存储空间已分配，重复分配报错 |
| 分配失败错误 | 算法分配失败 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `NsNum` 必须大于 0。
- `NsNum` 为 1 时，`Strategy` 不能为 `Striped`（仅一个命名空间无法条带化）。
- `NsSize` 必须大于 0 且为 1G（`1024*1024*1024`）的整数倍。
- 条带化策略时 `NsSize` 需整除 `NsNum`。
- `LbaFormat` 必须为 `Fmt512`（512）或 `Fmt4K`（4096）。
- `Strategy` 必须为 `Striped`（0）、`Linear`（1）或 `Normal`（2）。
- `Tenant` 允许为空；非空时长度不超过 `UbsSsuMaxTenantLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```go
package main

import (
    "fmt"
    "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
)

func main() {
    req := ssu.UbsSsuAllocSpaceReq{
        Name:      "space-001",
        NsSize:    1 << 30, // 1 GiB
        NsNum:     2,
        LbaFormat: ssu.Fmt4K,
        Strategy:  ssu.Striped,
    }
    result, err := ssu.UbsSsuAllocSpace(req)
    if err != nil {
        fmt.Printf("alloc failed: %v\n", err)
        return
    }
    fmt.Printf("alloc success, name=%s, ns_cnt=%d\n", result.Name, len(result.Namespaces))
}
```

### UbsSsuFreeSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuFreeSpace(name string) error
```

**描述 DESCRIPTION**

释放之前通过 `UbsSsuAllocSpace` 分配的存储空间及其关联的所有命名空间。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `name` | `string` | 要释放的存储空间标识 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `name` 为空或超长 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 存储空间不存在或已释放，无需释放报错 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- 释放操作不再幂等，释放不存在的空间将报错。

**附注 NOTES**

暂无。

### UbsSsuAddAccessPermission

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuAddAccessPermission(name string, nqn string) error
```

**描述 DESCRIPTION**

为指定的 Host 授予对已分配存储空间的访问权限，在 Target 侧将 Host NQN 添加到子系统的允许主机列表中。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `name` | `string` | 存储空间标识 |
| `nqn` | `string` | Host 的 NVMe Qualified Name |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `name` 或 `nqn` 为空或超长 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `nqn` 不能为空，长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。

**附注 NOTES**

重复添加同一 Host 的访问权限是否成功取决于底层适配器实现（适配器不幂等时重复添加可能报错），调用方不应依赖幂等性保证进行重试。

### UbsSsuRemoveAccessPermission

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuRemoveAccessPermission(name string, nqn string) error
```

**描述 DESCRIPTION**

撤销指定 Host 对已分配存储空间的访问权限，在 Target 侧将 Host NQN 从子系统的允许主机列表中移除。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `name` | `string` | 存储空间标识 |
| `nqn` | `string` | Host 的 NVMe Qualified Name |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `name` 或 `nqn` 为空或超长 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `nqn` 不能为空，长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。

**附注 NOTES**

- 命名空间已被删除（不在设备缓存中）时，移除操作幂等跳过。
- 重复移除访问权限是否成功取决于底层适配器实现。

### UbsSsuAttachSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuAttachSpace(req UbsSsuSpaceReq) ([]string, error)
```

**描述 DESCRIPTION**

将指定的存储空间挂载到系统，使其可被主机访问。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuSpaceReq` | 挂载请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]string` | 命名空间设备路径列表 |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 已挂载错误 | 空间已挂载（`errcode.ErrAlreadyAttached`）；此时第 1 个返回值为已挂载的设备路径列表，第 2 个返回值为原始错误 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.Nqn` 允许为空；非空时长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。
- `req.SrcEid` 允许为空；非空时长度不超过 `UbsSsuMaxEidLength - 1` 个字符。

**附注 NOTES**

当返回 `errcode.ErrAlreadyAttached` 时，第 1 个返回值（`[]string`）为已挂载的设备路径列表，调用方可直接使用。

**样例 EXAMPLES**

```go
package main

import (
    "errors"
    "fmt"
    "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/errcode"
    "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
)

func main() {
    req := ssu.UbsSsuSpaceReq{
        Name:   "space-001",
        Nqn:    "nqn.2024-01.com.huawei:host1",
        SrcEid: "eid-source-01",
    }
    paths, err := ssu.UbsSsuAttachSpace(req)
    if err != nil {
        if errors.Is(err, errcode.ErrAlreadyAttached) {
            fmt.Printf("already attached, paths: %v\n", paths)
        } else {
            fmt.Printf("attach failed: %v\n", err)
        }
        return
    }
    for _, p := range paths {
        fmt.Printf("dev_path: %s\n", p)
    }
}
```

### UbsSsuDetachSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuDetachSpace(req UbsSsuSpaceReq) error
```

**描述 DESCRIPTION**

将指定的存储空间从系统卸载，释放设备占用。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuSpaceReq` | 卸载请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 空间已卸载或未挂载，无需卸载报错 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.Nqn` 允许为空；非空时长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。
- `req.SrcEid` 允许为空；非空时长度不超过 `UbsSsuMaxEidLength - 1` 个字符。
- 卸载前需确保没有进程正在使用该存储空间。

**附注 NOTES**

已卸载的空间重复卸载将报错，不再幂等返回成功。

### UbsSsuAttachLinearSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuAttachLinearSpace(req UbsSsuLinearSpaceReq) ([]string, string, error)
```

**描述 DESCRIPTION**

将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuLinearSpaceReq` | 挂载请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]string` | 命名空间设备路径列表 |
| 第 2 个 | `string` | 聚合后的设备路径 |
| 第 3 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 存储空间不存在 |
| 已挂载错误 | 空间已挂载（`errcode.ErrAlreadyAttached`）；此时前两个返回值为已挂载的路径信息 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.Nqn` 允许为空；非空时长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。
- `req.SrcEid` 允许为空；非空时长度不超过 `UbsSsuMaxEidLength - 1` 个字符。
- `req.DevName` 不能为空，长度不超过 `UbsSsuMaxDevNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。
- 线性编址模式下，数据按顺序填充各成员设备。

**附注 NOTES**

当返回 `errcode.ErrAlreadyAttached` 时，前两个返回值为已挂载的设备路径列表和聚合设备路径，调用方可直接使用。

### UbsSsuDetachLinearSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuDetachLinearSpace(req UbsSsuLinearSpaceReq) error
```

**描述 DESCRIPTION**

将线性聚合的块设备卸载并释放。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuLinearSpaceReq` | 卸载请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 存储空间不存在或已卸载，无需卸载报错 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.Nqn` 允许为空；非空时长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。
- `req.SrcEid` 允许为空；非空时长度不超过 `UbsSsuMaxEidLength - 1` 个字符。
- `req.DevName` 不能为空，长度不超过 `UbsSsuMaxDevNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。

**附注 NOTES**

已卸载的空间重复卸载将报错，不再幂等返回成功。

### UbsSsuAttachStripedSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuAttachStripedSpace(req UbsSsuStripedSpaceReq) ([]string, string, error)
```

**描述 DESCRIPTION**

将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载，支持 RAID0 和 RAID5 两种级别。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuStripedSpaceReq` | 条带化挂载请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]string` | 命名空间设备路径列表 |
| 第 2 个 | `string` | 聚合后的设备路径 |
| 第 3 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 已挂载错误 | 空间已挂载（`errcode.ErrAlreadyAttached`）；此时前两个返回值为已挂载的路径信息 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.Nqn` 允许为空；非空时长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。
- `req.SrcEid` 允许为空；非空时长度不超过 `UbsSsuMaxEidLength - 1` 个字符。
- `req.DevName` 不能为空，长度不超过 `UbsSsuMaxDevNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。
- `req.Level` 必须为 `Raid0`（0）或 `Raid5`（5）。
- `req.ChunkSize` 必须为 `Size4K`（4）、`Size16K`（16）、`Size32K`（32）、`Size64K`（64）、`Size128K`（128）、`Size256K`（256）或 `Size512K`（512）。
- RAID5 至少需要 `UbsSsuRaid5MinMemberNum`（3）个成员设备。

**附注 NOTES**

当返回 `errcode.ErrAlreadyAttached` 时，前两个返回值为已挂载的设备路径列表和聚合设备路径，调用方可直接使用。

### UbsSsuDetachStripedSpace

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuDetachStripedSpace(req UbsSsuStripedSpaceReq) error
```

**描述 DESCRIPTION**

将条带化聚合的块设备卸载并释放。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `req` | `UbsSsuStripedSpaceReq` | 卸载请求参数 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `req` 字段不合法 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 存储空间不存在或已卸载，无需卸载报错 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.Name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.Nqn` 允许为空；非空时长度不超过 `UbsSsuMaxNqnLength - 1` 个字符。
- `req.SrcEid` 允许为空；非空时长度不超过 `UbsSsuMaxEidLength - 1` 个字符。
- `req.DevName` 不能为空，长度不超过 `UbsSsuMaxDevNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。
- 卸载操作不校验 `Level` 和 `ChunkSize`。

**附注 NOTES**

已卸载的空间重复卸载将报错，不再幂等返回成功。

### UbsSsuGetNsStats

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuGetNsStats(name string) ([]UbsSsuNsStats, error)
```

**描述 DESCRIPTION**

查询指定存储空间下各命名空间的容量使用情况，包括总容量和已用容量。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `name` | `string` | 存储空间标识 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]UbsSsuNsStats` | 命名空间统计信息列表 |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `name` 为空或超长 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 存储空间不存在 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。

**附注 NOTES**

暂无。

### UbsSsuGetConnectInfo

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuGetConnectInfo(name string, vfe *UbsUbVfe) ([]UbsSsuConnectInfo, error)
```

**描述 DESCRIPTION**

查询指定存储空间在指定 VFE 上的 NVMe 连接信息，包括子系统 NQN、Host NQN、命名空间 ID 等。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `name` | `string` | 存储空间标识 |
| `vfe` | `*UbsUbVfe` | VFE 信息指针，可选参数；指定 vfe 时连接信息里的 `src_eid` 为指定 vfe 的 eid，否则为 host 侧分配给 ssu 的 fe 的 eid |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]UbsSsuConnectInfo` | 连接信息列表 |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `name` 为空或超长 |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | 存储空间或 VFE 不存在 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UbsSsuMaxNameLength - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `vfe` 可为 `nil`，表示不限定 VFE，此时连接信息里的 `src_eid` 为 host 侧分配给 ssu 的 fe 的 eid；指定 vfe 时 `src_eid` 为指定 vfe 的 eid。

**附注 NOTES**

暂无。

### UbsSsuGetFeDeviceList

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuGetFeDeviceList() ([]UbsSsuFe, error)
```

**描述 DESCRIPTION**

查询系统中所有 FE 设备信息，包括每个 PFE 下的 VFE 列表。

**参数 PARAMETERS**

无参数。

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `[]UbsSsuFe` | FE 设备信息列表 |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

无。

**附注 NOTES**

暂无。

### UbsSsuFeDeviceAlloc

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuFeDeviceAlloc(upi uint32, vfe *UbsUbVfe, busInstanceGuid string) (string, error)
```

**描述 DESCRIPTION**

将指定的虚拟功能单元绑定到目标虚拟机，使虚拟机可通过该 VFE 访问存储资源。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `upi` | `uint32` | 租户隔离标识 |
| `vfe` | `*UbsUbVfe` | 要绑定的 VFE 信息 |
| `busInstanceGuid` | `string` | 总线实例 GUID，标识目标虚拟机，**可传空**；非空时长度须为 `UbsSsuMaxGuidLength`。为空表示由 ubse 内部创建 vm busInstance 并绑定到该 VFE，非空表示绑定到指定虚拟机 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `string` | 绑定后的总线实例 GUID（内部创建场景下为新创建的 GUID） |
| 第 2 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `vfe` 为 `nil` 或 `busInstanceGuid` 非空时长度不等于 `UbsSsuMaxGuidLength` |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | VFE 或虚拟机不存在 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `vfe` 不能为 `nil`。
- `busInstanceGuid` 允许为空字符串（表示由 ubse 内部创建 vm busInstance）；非空时长度须为 `UbsSsuMaxGuidLength`（32）。

**附注 NOTES**

- 与 C 端的差异：C 端 `bus_instance_guid` 为 IN/OUT 复用参数，不允许传 NULL，改以全 0 缓冲区表达"内部创建"语义；Go 端 `busInstanceGuid` 仅作为 IN 参数，实际 GUID 通过返回值传出。
- 协议层等价：Go 空字符串与 C 端全 0 缓冲区均打包为 `UbsSsuMaxGuidLength` 字节全 0。

**样例 EXAMPLES**

```go
package main

import (
    "fmt"
    "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
)

func main() {
    vfe := &ssu.UbsUbVfe{
        SlotId: 0,
        ChipId: 0,
        PfeId:  1,
        VfeId:  1,
        // VfeGuid 需填充实际值
    }
    // 传空字符串表示由 ubse 内部创建 vm busInstance
    guid, err := ssu.UbsSsuFeDeviceAlloc(1, vfe, "")
    if err != nil {
        fmt.Printf("alloc failed: %v\n", err)
        return
    }
    fmt.Printf("bind success, guid=%s\n", guid)
}
```

### UbsSsuFeDeviceFree

**包 PACKAGE**

`ssu`

**摘要 SYNOPSIS**

```go
func UbsSsuFeDeviceFree(upi uint32, vfe *UbsUbVfe) error
```

**描述 DESCRIPTION**

将已分配的虚拟功能单元从目标虚拟机释放，回收 VFE 设备资源。

**参数 PARAMETERS**

| name | 类型 | 说明 |
| ---- | ---- | ---- |
| `upi` | `uint32` | 租户隔离标识 |
| `vfe` | `*UbsUbVfe` | 要释放的 VFE 信息 |

**返回值 RETURN VALUE**

| 返回值 | 类型 | 说明 |
| ------ | ---- | ---- |
| 第 1 个 | `error` | 错误信息；成功返回 `nil` |

**错误 ERRORS**

| Error | Description |
| ----- | ----------- |
| 参数校验错误 | `vfe` 为 `nil` |
| IPC 通信错误 | 连接 UBSE 服务端失败 |
| 鉴权错误 | UBSE 服务端鉴权不通过 |
| 不存在错误 | VFE 或虚拟机不存在 |
| 超时错误 | UBSE 服务端处理超时 |
| 内部错误 | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `vfe` 不能为 `nil`。
- `vfe.BindBusInstanceGuid` 必须为 32 个字符（`UbsSsuMaxGuidLength`）。

**附注 NOTES**

暂无。
