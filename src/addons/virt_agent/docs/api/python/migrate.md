# 1. update_page_flow_and_status

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_vm_migrate import update_page_flow_and_status
def update_page_flow_and_status(params: Dict[str, Any]) -> int
```

## 描述 DESCRIPTION

更新冷热页流动开关和虚拟机状态。

## 参数 Parameters

| name   | IN/OUT | description      |
|--------|--------|------------------|
| params | IN     | 操作类型与虚拟机UUID。 |

- 数据结构说明

```python
params = {
    "opt": str,   # 操作类型，取值："true" / "false" / "none_migrating" / "none_migrate_success" / "none_migrate_failed"
    "uuid": str   # 虚拟机UUID
}
```

## 返回值 RETURN VALUE

返回 `int` 类型：0 表示成功，非 0 表示失败。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

## 错误 ERRORS

| Exception                    | Description |
|------------------------------|-------------|
| UbsVABaseError               | 基本错误类型（对应 VA_ERROR_BASE） |
| UbsVAInvalidParamError       | 参数不合法（对应 VA_ERROR_INVALID_PARAM） |
| UbsVANullPointerError        | 空指针（对应 VA_ERROR_NULL_POINTER） |
| UbsVAMemAllocError           | 内存分配失败（对应 VA_ERROR_MEM_ALLOCATE_FAILED） |
| UbsVAMemCopyError            | 内存拷贝失败（对应 VA_ERROR_MEM_COPY_FAILED） |
| UbsVASerializeError          | 序列化失败（对应 VA_ERROR_SERIALIZE_FAILED） |
| UbsVADeserializeError        | 反序列化失败（对应 VA_ERROR_DESERIALIZE_FAILED） |
| ConnectionError              | 本地动态库 libubs-virt-agent.so 加载失败 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_vm_migrate import update_page_flow_and_status

params = {
    "opt": "true",
    "uuid": "1863ec26-3fda-4f6f-97b2-fcaf9139ac7d"
}
ret = update_page_flow_and_status(params)
if ret != 0:
    print("update_page_flow_and_status failed.")
```

# 2. get_migrate_decision

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_vm_migrate import get_migrate_decision
def get_migrate_decision(uuid: str, vm_memory_mb: int, dest_hostname: str, dest_numa_id: int) -> int
```

## 描述 DESCRIPTION

获取虚拟机迁移策略。

## 参数 Parameters

| name          | IN/OUT | description |
|---------------|--------|-------------|
| uuid          | IN     | 虚拟机UUID。   |
| vm_memory_mb  | IN     | 虚拟机内存大小（MB）。 |
| dest_hostname | IN     | 目标主机名。     |
| dest_numa_id  | IN     | 目标NUMA ID。  |

## 返回值 RETURN VALUE

返回 `int` 类型迁移策略值；任一入参为 `None` 或调用失败时，返回默认策略 `MULTI_COPY_MIGRATE`（0）。

- 返回值枚举说明

```python
class MigrateStrategy(Enum):
    MULTI_COPY_MIGRATE = 0  # 多拷贝迁移（默认策略）
    ONE_COPY_MIGRATE   = 1  # 单拷贝迁移
    HAM_MIGRATE        = 2  # HAM迁移
```

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| ConnectionError              | 本地动态库 libubs-virt-agent.so 加载失败 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 该接口内部不抛出业务异常，调用失败时直接返回默认策略值。
- C接口 `RackStartIpcClientWithTimeout`、`RackSyncSendForHam`、`RackAsyncSendForHam` 为libvirt与virt_agent之间的内部IPC通信接口，Python SDK未封装。

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_vm_migrate import get_migrate_decision

strategy = get_migrate_decision(
    uuid="1863ec26-3fda-4f6f-97b2-fcaf9139ac7d",
    vm_memory_mb=2048,
    dest_hostname="computer02",
    dest_numa_id=0
)
print(f"migrate strategy: {strategy}")
```
