# 1. UbsMemFragmentationNodeInfo

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationNodeInfo() ([]NumaInfo, error)
```

## 描述 DESCRIPTION

查询本节点Numa信息。

## 参数 Parameters

无

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `[]NumaInfo` NUMA信息列表；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_fragmentation_node_info failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid numa info count returned from C: \{count\} | 底层返回的NUMA信息数量非法（大于512），\{count\} 为底层实际返回的数量值 |

## 约束 CONSTRAINTS

- 底层返回的NUMA信息数量上限 512，超出视为非法返回。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_fragmentation_node_info`

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

# 2. UbsMemFragmentationVmInfo

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationVmInfo() ([]VmDomainInfo, error)
```

## 描述 DESCRIPTION

查询本节点虚拟机信息。

## 参数 Parameters

无

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `[]VmDomainInfo` 虚拟机信息列表；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_fragmentation_vm_info failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid vm info count returned from C: \{count\} | 底层返回的虚拟机信息数量非法（大于300），\{count\} 为底层实际返回的数量值 |

## 约束 CONSTRAINTS

- 底层返回的虚拟机信息数量上限 300，超出视为非法返回。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_fragmentation_vm_info`

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

# 3. UbsMemFragmentationNodeInfoList

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationNodeInfoList() ([]NodeInfo, error)
```

## 描述 DESCRIPTION

查询集群内所有节点的内存信息。

## 参数 Parameters

无

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `[]NodeInfo` 节点信息列表；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_fragmentation_node_info_list failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid node count returned from C: \{count\} | 底层返回的节点数量非法（大于32），\{count\} 为底层实际返回的数量值 |

## 约束 CONSTRAINTS

- 底层返回的节点数量上限 32，单节点 NUMA 数量上限 512，超出视为非法返回。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_fragmentation_node_info_list`

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

# 4. UbsMemFragmentationVmInfoList

## 库 LIBRARY

Go SDK memfragmentation 包，底层通过 dlopen 运行时加载 virt_agent 库 (libubs-virt-agent.so.1)

## 摘要 SYNOPSIS

```go
import "atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"

func UbsMemFragmentationVmInfoList() ([]VmNodeInfo, error)
```

## 描述 DESCRIPTION

查询集群内所有节点的虚拟机信息。

## 参数 Parameters

无

- 数据结构说明

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

## 返回值 RETURN VALUE

成功时返回 `[]VmNodeInfo` 节点信息列表；失败时返回 `error`，请见`错误 ERRORS`。

## 错误 ERRORS

Go 接口不使用异常，统一通过 `error` 返回错误信息。

| Error                                        | Description |
|----------------------------------------------|-------------|
| failed to load symbol: dlopen handle is null | 动态库未初始化，须先调用 dlopen.GetLibHandle() |
| failed to load symbol: failed to load func \{funcName\} | 符号查找失败，\{funcName\} 为未找到的函数名 |
| ubs_virt_agent_mem_fragmentation_vm_info_list failed: \{code\} | 底层C接口返回非0错误码，\{code\} 为具体错误码值 |
| invalid node count returned from C: \{count\} | 底层返回的节点数量非法（大于32），\{count\} 为底层实际返回的数量值 |

## 约束 CONSTRAINTS

- 底层返回的节点数量上限 32，超出视为非法返回；单节点虚拟机信息数量上限 300，超出部分截断。

## 附注 NOTES

- 对应C接口：`ubs_virt_agent_mem_fragmentation_vm_info_list`

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
