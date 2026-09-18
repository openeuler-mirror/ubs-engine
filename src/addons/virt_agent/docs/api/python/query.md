# 1. ubs_node_info_list

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_query import ubs_node_info_list
def ubs_node_info_list() -> NodeNumaInfoT
```

## 描述 DESCRIPTION

查询本节点Numa信息。

## 参数 Parameters

| name | IN/OUT | description |
| ---- |--------|-------------|
| 无   | -      | -            |

- 数据结构说明

```python
@dataclass
class NumaInfoHugePageDataT:
    page_size: int        # 页大小
    huge_page_total: int  # 大页总数
    huge_page_free: int   # 空闲大页数

@dataclass
class NumaInfoT:
    host_id: str                                        # 节点ID（来自控制面配置文件）
    hostname: str                                       # 节点主机名
    numa_id: str                                        # NUMA ID
    socket_id: int                                      # NUMA绑定的CPU所属Socket ID
    is_local: int                                       # 是否为本机NUMA（0: 非本机, 1: 本机）
    mem_total: int                                      # 该NUMA节点内存总量（含保留），从系统文件采集，单位kB
    mem_free: int                                       # 该NUMA节点空闲内存，从系统文件采集，单位kB
    numa_huge_page_info: Dict[int, NumaInfoHugePageDataT]  # 大页信息，key为page_size

@dataclass
class NodeNumaInfoT:
    numa_infos: List[NumaInfoT]  # NUMA信息列表
    host_id: str                 # 节点ID
    hostname: str                # 节点主机名
```

## 返回值 RETURN VALUE

成功时返回 `NodeNumaInfoT` 对象；本节点无NUMA信息时返回 `None`；失败时抛出异常，请见`错误 ERRORS`。

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| UbsVABaseError               | 基本错误类型 |
| UbsVAInvalidParamError       | 参数不合法 |
| UbsVANullPointerError        | 空指针 |
| UbsVAMemAllocError           | 内存分配失败 |
| UbsVAMemCopyError            | 内存拷贝失败 |
| UbsVASerializeError          | 序列化失败 |
| UbsVADeserializeError        | 反序列化失败 |
| ConnectionError              | 本地动态库 libubs-virt-agent.so 加载失败 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_query import ubs_node_info_list

result = ubs_node_info_list()
if result is None:
    print("Node numa info is empty!")
else:
    for numa_info in result.numa_infos:
        print(f"numaId={numa_info.numa_id}, memTotal={numa_info.mem_total}kB, "
              f"memFree={numa_info.mem_free}kB")
```

# 2. ubs_vm_info_list

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_query import ubs_vm_info_list
def ubs_vm_info_list() -> NodeVmInfoT
```

## 描述 DESCRIPTION

查询本节点虚拟机信息。

## 参数 Parameters

| name | IN/OUT | description |
| ---- |--------|-------------|
| 无   | -      | -            |

## 返回值 RETURN VALUE

成功时返回 `NodeVmInfoT` 对象（无虚拟机时 `vm_infos` 为空列表）；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class VmNumaInfoT:
    numa_id: int    # NUMA ID
    socket_id: int  # Socket ID
    is_local: bool  # 是否为本机NUMA
    page_size: int  # 页大小
    used_mem: int   # 已用内存

@dataclass
class VmMetaDataT:
    host_id: str        # 物理节点ID（来自控制面配置文件）
    hostname: str       # 物理节点主机名（来自VM XML定义）
    uuid: str           # 虚拟机UUID（来自VM XML定义）
    name: str           # 虚拟机名称（来自VM XML定义）
    vm_create_time: str # 虚拟机创建时间
    state: str          # 虚拟机状态
    max_mem: str        # 虚拟机最大内存
    pid: int            # 虚拟机进程ID

@dataclass
class VmInfoT:
    timestamp: int                      # 采集时间戳
    metadata: VmMetaDataT               # 虚拟机元数据
    numa_datas: Dict[int, VmNumaInfoT]  # 各NUMA上内存使用信息，key为numa_id

@dataclass
class NodeVmInfoT:
    vm_infos: List[VmInfoT]  # 虚拟机信息列表
```

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| UbsVABaseError               | 基本错误类型 |
| UbsVAInvalidParamError       | 参数不合法 |
| UbsVANullPointerError        | 空指针 |
| UbsVAMemAllocError           | 内存分配失败 |
| UbsVAMemCopyError            | 内存拷贝失败 |
| UbsVASerializeError          | 序列化失败 |
| UbsVADeserializeError        | 反序列化失败 |
| ConnectionError              | 本地动态库 libubs-virt-agent.so 加载失败 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_query import ubs_vm_info_list

result = ubs_vm_info_list()
if not result.vm_infos:
    print("VmInfo is empty!")
else:
    for vm_info in result.vm_infos:
        print(f"uuid={vm_info.metadata.uuid}, name={vm_info.metadata.name}, "
              f"state={vm_info.metadata.state}, pid={vm_info.metadata.pid}")
```
