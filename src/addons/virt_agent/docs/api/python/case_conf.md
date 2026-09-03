# 1. ubs_case_conf_info

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_case_conf import ubs_case_conf_info
def ubs_case_conf_info() -> CaseConfInfoT
```

## 描述 DESCRIPTION

获取当前虚拟化场景配置信息。

## 参数 Parameters

| name | IN/OUT | description |
| ---- |--------|-------------|
| 无   | -      | -            |

## 返回值 RETURN VALUE

成功时返回 `CaseConfInfoT` 对象；失败时抛出异常，请见`错误 ERRORS`。

- 数据结构说明

```python
@dataclass
class CaseConfInfoT:
    case_type: str           # 场景类型
    over_commitment: float   # 超分比例
    migrate_water_line: int  # 迁移水线
    index: int               # 索引
    host_id: str             # 节点ID
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
from ubse.ubs_virt_agent_case_conf import ubs_case_conf_info

case_conf_info = ubs_case_conf_info()
print(f"caseType={case_conf_info.case_type}, "
      f"overCommitment={case_conf_info.over_commitment}, "
      f"migrateWaterLine={case_conf_info.migrate_water_line}")
```

# 2. ubs_case_conf_set

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_case_conf import ubs_case_conf_set
def ubs_case_conf_set(param: Dict[str, Any]) -> int
```

## 描述 DESCRIPTION

设置当前虚拟化场景配置，仅首次设置能真正成功。

## 参数 Parameters

| name  | IN/OUT | description          |
|-------|--------|----------------------|
| param | IN     | 场景和超分比例信息（会被序列化为JSON后传给底层）。 |

- 数据结构说明

```python
param = {
    "caseType": str,        # 场景类型，如 "overCommitment"
    "overCommitment": float # 超分比例，如 1.25
}
```

## 返回值 RETURN VALUE

返回 `int` 类型设置结果：0 表示成功，非 0 表示失败。底层返回已知错误码时抛出异常，请见`错误 ERRORS`。

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
from ubse.ubs_virt_agent_case_conf import ubs_case_conf_set

param = {
    "caseType": "overCommitment",
    "overCommitment": 1.25
}
ret = ubs_case_conf_set(param)
if ret != 0:
    print("ubs_case_conf_set failed.")
```
