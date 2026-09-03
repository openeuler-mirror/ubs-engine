# 1. ubs_node_anti_affinity

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_node_anti_affinity
def ubs_node_anti_affinity(node_anti_dict: Union[NodeAntiDictionary, Dict[str, List[str]]]) -> int
```

## 描述 DESCRIPTION

设置节点反亲和性关系。

## 参数 Parameters

| name           | IN/OUT | description         |
|----------------|--------|---------------------|
| node_anti_dict | IN     | 反亲和信息字典。       |

- 数据结构说明

```python
# 推荐直接使用字典形式：{"节点ID": ["反亲和节点ID列表", ...]}
# 示例：{"1": ["2"], "2": ["3"], "3": ["1"]}
node_anti_dict: Dict[str, List[str]]
```

## 返回值 RETURN VALUE

返回 `int` 类型：0 表示成功，非 0 表示失败。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

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
from ubse.ubs_virt_agent_fragmentation import ubs_node_anti_affinity

node_anti_dict = {
    "1": ["2"],
    "2": ["3"],
    "3": ["1"]
}
ret = ubs_node_anti_affinity(node_anti_dict)
if ret != 0:
    print("ubs_node_anti_affinity failed.")
```

# 2. ubs_mem_borrow_strategy

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow_strategy
def ubs_mem_borrow_strategy(param: Dict[str, Any]) -> BorrowStrategyT
```

## 描述 DESCRIPTION

获取内存借用策略。

## 参数 Parameters

| name | IN/OUT | description |
|------|--------|-------------|
| param | IN     | 内存借用信息   |

- 数据结构说明

```python
param = {
    "srcParam": {
        "srcNid": str,       # 源节点ID
        "srcSocketId": int,  # 源Socket ID
        "srcNumaId": int     # 源NUMA ID
    },
    "borrowSize": int        # 借用大小
}
```

## 返回值 RETURN VALUE

成功时返回 `BorrowStrategyT` 对象；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class DstNumaInfoT:
    host_id: str        # 目的节点ID
    socket_id: int      # 目的Socket ID
    numa_nums: int      # NUMA数量
    numa_ids: List[int]     # NUMA ID列表
    mem_sizes: List[int]    # 各NUMA借用内存大小列表

@dataclass
class BorrowStrategyT:
    src_host_id: str                 # 源节点ID
    src_socket_id: int               # 源Socket ID
    src_numa_id: int                 # 源NUMA ID
    borrow_size: int                 # 借用大小
    dest_numa_infos: List[DstNumaInfoT]  # 借用决策信息列表
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
| KeyError                     | param 缺少必填的key |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow_strategy

param = {
    "srcParam": {
        "srcNid": "1",
        "srcSocketId": 36,
        "srcNumaId": 0
    },
    "borrowSize": 1048576
}
result = ubs_mem_borrow_strategy(param)
for dst_info in result.dest_numa_infos:
    print(f"destHostId={dst_info.host_id}, numaIds={dst_info.numa_ids}, "
          f"memSizes={dst_info.mem_sizes}")
```

# 3. ubs_mem_borrow_execute

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow_execute
def ubs_mem_borrow_execute(param: Dict[str, Any], is_async: bool = False) -> Tuple[BorrowExecuteResT, str]
```

## 描述 DESCRIPTION

根据借用决策结果执行内存借用。

## 参数 Parameters

| name    | IN/OUT | description              |
|---------|--------|--------------------------|
| param   | IN     | 内存借用策略信息             |
| is_async | IN    | 是否异步执行开关，默认同步执行 |

- 数据结构说明

```python
param = {
    "srcParam": {
        "srcNid": str,       # 源节点ID
        "srcSocketId": int,  # 源Socket ID
        "srcNumaId": int     # 源NUMA ID
    },
    "borrowSize": int,       # 借用大小
    "destParam": [           # 目的NUMA信息列表
        {
            "destNid": str,      # 目的节点ID
            "destSocketId": int, # 目的Socket ID
            "destNumaNum": int,  # NUMA数量
            "destNumaId": List[int],  # NUMA ID列表
            "memSize": List[int]      # 各NUMA借用内存大小列表
        }
    ]
}
```

## 返回值 RETURN VALUE

成功时返回元组 `(BorrowExecuteResT, str)`，第一个元素为借用结果信息，第二个元素为后台任务id；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class BorrowExecuteResT:
    borrow_ids: List[str]        # 借用ID列表
    present_numa_ids: List[int]  # 借入的远端NUMA ID列表
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
| TypeError / ValueError       | param 字段类型不合法 |
| UbsVirtAgentError            | 借用结果中借用ID列表或NUMA ID列表为空 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

异步执行时返回的task_id可通过 `ubs_task_result_query` 查询后台任务执行结果。

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow_execute

param = {
    "srcParam": {
        "srcNid": "1",
        "srcSocketId": 1,
        "srcNumaId": 0
    },
    "borrowSize": 128,
    "destParam": [
        {
            "destNid": "1",
            "destSocketId": 1,
            "destNumaNum": 1,
            "destNumaId": [1],
            "memSize": [128]
        }
    ]
}
borrow_res, task_id = ubs_mem_borrow_execute(param, is_async=True)
print(f"borrowIds={borrow_res.borrow_ids}, task_id={task_id}")
```

# 4. ubs_mem_migrate_strategy

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_migrate_strategy
def ubs_mem_migrate_strategy(param: Dict[str, Any]) -> MemMigrateStrategyT
```

## 描述 DESCRIPTION

获取内存迁移策略。

## 参数 Parameters

| name | IN/OUT | description |
|------|--------|-------------|
| param | IN     | 内存迁移信息。  |

- 数据结构说明

```python
param = {
    "borrowSize": int,     # 借用大小
    "borrowInNode": str,   # 借入节点ID
    "vmInfoList": [        # 虚拟机信息列表
        {
            "pid": int,    # 虚拟机进程ID
            "ratio": int   # 迁移比例
        }
    ]
}
```

## 返回值 RETURN VALUE

成功时返回 `MemMigrateStrategyT` 对象；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class VmMigrateStrategyT:
    dest_numa_id: int  # 目的NUMA ID
    mem_size: int      # 迁移内存大小
    pid: int           # 虚拟机进程ID

@dataclass
class MemMigrateStrategyT:
    vm_info_list: List[VmMigrateStrategyT]  # 虚拟机迁移策略列表
    waiting_time: int                       # 等待时间
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
| ValueError / RuntimeError    | param 字段缺失、超长或数量超限 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_migrate_strategy

param = {
    "borrowSize": 128,
    "borrowInNode": "1",
    "vmInfoList": [{"pid": 123, "ratio": 25}]
}
result = ubs_mem_migrate_strategy(param)
for vm_strategy in result.vm_info_list:
    print(f"pid={vm_strategy.pid}, destNumaId={vm_strategy.dest_numa_id}, "
          f"memSize={vm_strategy.mem_size}")
print(f"waitingTime={result.waiting_time}")
```

# 5. ubs_mem_migrate_execute

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_migrate_execute
def ubs_mem_migrate_execute(param: Dict[str, Any]) -> int
```

## 描述 DESCRIPTION

根据迁移决策结果执行内存迁移。

## 参数 Parameters

| name | IN/OUT | description      |
|------|--------|------------------|
| param | IN     | 内存迁移决策信息。   |

- 数据结构说明

```python
param = {
    "borrowInNode": str,   # 借入节点ID
    "borrowIds": List[str],  # 借用ID列表
    "vmInfoList": [          # 虚拟机迁移策略列表
        {
            "destNumaId": int,  # 目的NUMA ID
            "memSize": int,     # 迁移内存大小
            "pid": int          # 虚拟机进程ID
        }
    ],
    "waitingTime": int     # 等待时间
}
```

## 返回值 RETURN VALUE

返回 `int` 类型：0 表示成功，非 0 表示失败。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

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
| ValueError / RuntimeError    | param 字段缺失、超长或数量超限 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_migrate_execute

param = {
    "borrowInNode": "1",
    "borrowIds": ["1-abc123"],
    "vmInfoList": [{"destNumaId": 1, "memSize": 128, "pid": 123}],
    "waitingTime": 1000
}
ret = ubs_mem_migrate_execute(param)
if ret != 0:
    print("ubs_mem_migrate_execute failed.")
```

# 6. ubs_mem_return

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_return
def ubs_mem_return(is_async: bool = False) -> Tuple[int, str]
```

## 描述 DESCRIPTION

根据参数进行内存归还。

## 参数 Parameters

| name    | IN/OUT | description                                            |
|---------|--------|--------------------------------------------------------|
| is_async | IN     | 异步执行开关，默认同步执行。true：异步执行内存归还。false：同步执行内存归还。 |

## 返回值 RETURN VALUE

返回元组 `(int, str)`：第一个元素为调用结果，0 表示成功，非 0 表示失败；第二个元素为后台任务id，仅 `is_async=True` 且成功时非空，否则为空字符串。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

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
| TypeError                    | is_async 类型不是bool |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

异步执行时返回的task_id可通过 `ubs_task_result_query` 查询后台任务执行结果。

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_return

ret, task_id = ubs_mem_return(is_async=True)
if ret != 0:
    print("ubs_mem_return failed.")
else:
    print(f"task_id={task_id}")
```

# 7. ubs_mem_rollback

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_rollback
def ubs_mem_rollback(node_id: str, borrow_id_list: List[str]) -> int
```

## 描述 DESCRIPTION

借用内存回滚。

## 参数 Parameters

| name           | IN/OUT | description                  |
|----------------|--------|------------------------------|
| node_id        | IN     | 节点ID。                       |
| borrow_id_list | IN     | 节点上的borrowId列表。           |

## 返回值 RETURN VALUE

返回 `int` 类型：0 表示成功，非 0 表示失败。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

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
| TypeError / ValueError       | 参数类型不合法、超长或数量超限 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_rollback

ret = ubs_mem_rollback("1", ["1-abc123"])
if ret != 0:
    print("ubs_mem_rollback failed.")
```

# 8. ubs_task_result_query

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_task_result_query
def ubs_task_result_query(task_id: str) -> Tuple[int, TaskInfoT]
```

## 描述 DESCRIPTION

借用归还任务查询。

## 参数 Parameters

| name    | IN/OUT | description                                    |
|---------|--------|------------------------------------------------|
| task_id | IN     | 后台任务id，来自ubs_mem_borrow_execute或ubs_mem_return接口的返回值。 |

## 返回值 RETURN VALUE

返回元组 `(int, TaskInfoT)`：第一个元素为调用结果，0 表示成功，非 0 表示失败；第二个元素为任务执行结果信息。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class BorrowExecuteResT:
    borrow_ids: List[str]        # 借用ID列表
    present_numa_ids: List[int]  # 借入的远端NUMA ID列表

@dataclass
class TaskInfoT:
    task_id: str                        # 后台任务id
    status: int                         # 任务状态
    result_code: int                    # 任务结果码
    mem_borrow_result: BorrowExecuteResT  # 内存借用结果

# 任务状态取值：
ASYNC_TASK_NOT_EXIST = 0  # 任务不存在
ASYNC_TASK_RUNNING   = 1  # 任务运行中
ASYNC_TASK_SUCCESS   = 2  # 任务成功
ASYNC_TASK_FAILED    = 3  # 任务失败
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
| TypeError                    | task_id 类型不是str |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_task_result_query

ret, task_info = ubs_task_result_query("abc123")
if ret != 0:
    print("ubs_task_result_query failed.")
else:
    print(f"status={task_info.status}, resultCode={task_info.result_code}")
```

# 9. ubs_mem_fragmentation_node_info_list

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_fragmentation_node_info_list
def ubs_mem_fragmentation_node_info_list() -> List[NodeInfoT]
```

## 描述 DESCRIPTION

查询集群内所有节点的内存信息。

## 参数 Parameters

| name | IN/OUT | description |
| ---- |--------|-------------|
| 无   | -      | -            |

## 返回值 RETURN VALUE

成功时返回 `List[NodeInfoT]` 节点信息列表（无数据时为空列表）；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class NumaInfoHugePageDataT:
    page_size: int        # 页大小
    huge_page_total: int  # 大页总数
    huge_page_free: int   # 空闲大页数

@dataclass
class NumaInfoT:
    host_id: str                                        # 节点ID
    hostname: str                                       # 节点主机名
    numa_id: str                                        # NUMA ID
    socket_id: int                                      # NUMA绑定的CPU所属Socket ID
    is_local: int                                       # 是否为本机NUMA（0: 非本机, 1: 本机）
    mem_total: int                                      # 该NUMA节点内存总量（含保留），从系统文件采集，单位kB
    mem_free: int                                       # 该NUMA节点空闲内存，从系统文件采集，单位kB
    numa_huge_page_info: Dict[int, NumaInfoHugePageDataT]  # 大页信息，key为page_size

@dataclass
class NodeInfoT:
    node_id: str                # 节点ID
    numa_infos: List[NumaInfoT] # NUMA信息列表
    is_current: bool            # 是否是查询节点
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
from ubse.ubs_virt_agent_fragmentation import ubs_mem_fragmentation_node_info_list

node_info_list = ubs_mem_fragmentation_node_info_list()
for node_info in node_info_list:
    print(f"nodeId={node_info.node_id}, isCurrent={node_info.is_current}, "
          f"numaCount={len(node_info.numa_infos)}")
```

# 10. ubs_mem_borrow

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow
from ubse.models.ubs_virt_agent_model import BorrowParamT, NumaMetaInfoT
def ubs_mem_borrow(param: BorrowParamT, is_async: bool = False) -> List[MemBorrowResultT]
```

## 描述 DESCRIPTION

大规格虚机场景，按节点借用内存。

## 参数 Parameters

| name     | IN/OUT | description                  |
|----------|--------|------------------------------|
| param    | IN     | 借用任务入参。                  |
| is_async | IN     | 是否异步执行借用任务，默认同步执行。 |

- 数据结构说明

```python
@dataclass
class NumaMetaInfoT:
    numa_id: int  # NUMA ID

@dataclass
class BorrowParamT:
    node_id: str                    # 源节点ID
    numa_meta_infos: List[NumaMetaInfoT]  # NUMA元信息列表
    borrow_size: int                # 借用大小，单位MB
```

## 返回值 RETURN VALUE

成功时返回 `List[MemBorrowResultT]` 借用结果列表（无数据时为空列表）；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class MemBorrowResultT:
    borrow_ids: List[str]        # 借用ID列表
    present_numa_ids: List[int]  # 借用内存映射NUMA ID列表
    task_id: str                 # 借用任务id（异步模式下有效）
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
| TypeError                    | 参数类型不合法 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

异步执行时结果中的task_id可通过 `ubs_task_result_query` 查询后台任务执行结果。

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_mem_borrow
from ubse.models.ubs_virt_agent_model import BorrowParamT, NumaMetaInfoT

# 此处按照NUMA0; NUMA1; NUMA2; NUMA3关系配置
numa_meta_infos = [NumaMetaInfoT(numa_id=i) for i in range(4)]
param = BorrowParamT(
    node_id="node0",
    numa_meta_infos=numa_meta_infos,
    borrow_size=3145728  # 单位MB，此处借用3TB内存
)
result = ubs_mem_borrow(param, is_async=True)
for res in result:
    print(f"taskId={res.task_id}, borrowIds={res.borrow_ids}")
```

# 11. ubs_page_swap_enable

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_fragmentation import ubs_page_swap_enable
from ubse.models.ubs_virt_agent_model import PageSwapPairT, NumaQuotaT
def ubs_page_swap_enable(pid: int, page_swap_enable: List[PageSwapPairT]) -> int
```

## 描述 DESCRIPTION

大规格虚机场景，通过配额实现使能冷热页流动。

## 参数 Parameters

| name             | IN/OUT | description          |
|------------------|--------|----------------------|
| pid              | IN     | 虚拟机进程ID。          |
| page_swap_enable | IN     | 本地远端内存映射关系列表。 |

- 数据结构说明

```python
@dataclass
class NumaQuotaT:
    numa_id: int  # NUMA ID
    quota: int    # 限额大小，单位MB

@dataclass
class PageSwapPairT:
    local_numa_quotas: List[NumaQuotaT]    # 本地NUMA配额列表
    remote_numa_quotas: List[NumaQuotaT]   # 远端映射本地NUMA配额列表
```

## 返回值 RETURN VALUE

返回 `int` 类型：0 表示成功，非 0 表示失败。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

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
| TypeError                    | 参数类型不合法 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_fragmentation import ubs_page_swap_enable
from ubse.models.ubs_virt_agent_model import PageSwapPairT, NumaQuotaT

pid = 123  # 虚拟机实际进程号
page_swap_enable = [
    PageSwapPairT(
        local_numa_quotas=[NumaQuotaT(numa_id=0, quota=786432),
                           NumaQuotaT(numa_id=1, quota=786432)],
        remote_numa_quotas=[NumaQuotaT(numa_id=4, quota=786432),
                            NumaQuotaT(numa_id=5, quota=786432)]
    ),
    PageSwapPairT(
        local_numa_quotas=[NumaQuotaT(numa_id=2, quota=786432),
                           NumaQuotaT(numa_id=3, quota=786432)],
        remote_numa_quotas=[NumaQuotaT(numa_id=7, quota=786432),
                            NumaQuotaT(numa_id=8, quota=786432)]
    )
]
ret = ubs_page_swap_enable(pid, page_swap_enable)
if ret != 0:
    print("ubs_page_swap_enable failed.")
```
