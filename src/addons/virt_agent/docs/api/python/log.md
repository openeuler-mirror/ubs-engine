# 1. ubs_virt_agent_log_callback_register

## 库 LIBRARY

python3-ubs-engine 包（ubse 模块），底层调用 virt_agent 库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```python
from ubse.ubs_virt_agent_log import ubs_virt_agent_log_callback_register
from ubse.ffi.ubs_virt_agent_log import LogLevel
def ubs_virt_agent_log_callback_register(log_handler: Callable[[LogLevel, str], None]) -> None
```

## 描述 DESCRIPTION

注册日志处理函数，用于接收底层 virt_agent 库的日志回调。

## 参数 Parameters

| name        | IN/OUT | description            |
|-------------|--------|------------------------|
| log_handler | IN     | 日志处理函数，接收日志级别和日志内容两个参数。 |

- 数据结构说明

```python
class LogLevel(IntEnum):
    DEBUG = 0  # 调试日志
    INFO = 1   # 信息日志
    WARN = 2   # 警告日志
    ERROR = 3  # 错误日志
    CRIT = 4   # 严重错误日志
```

## 返回值 RETURN VALUE

无返回值。

## 错误 ERRORS

| Error          | Description |
|-----------------|-------------|
| ConnectionError | 本地动态库 libubs-virt-agent.so 加载失败 |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

- 日志处理函数签名要求：`def log_handler(level: LogLevel, message: str) -> None`。
- 该接口为Python SDK特有接口，C接口文档中无对应条目。

## 样例 EXAMPLES

以下程序为使用示例。

```python
from ubse.ubs_virt_agent_log import ubs_virt_agent_log_callback_register
from ubse.ffi.ubs_virt_agent_log import LogLevel


def log_handler(level: LogLevel, message: str) -> None:
    print(f"[{level.name}] {message}")


ubs_virt_agent_log_callback_register(log_handler)
```
