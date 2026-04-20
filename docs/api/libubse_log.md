# 1. ubs_engine_log_callback_register

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_log.h>
void ubs_engine_log_callback_register(ubs_engine_log_handler handler);
```

## 描述 DESCRIPTION

注册日志回调函数。

如果没有注册日志函数，或者传入空指针，则采用标准输出打印日志；如果注册了回调函数，则后续日志通过回调函数返回给调用方处理。

## 参数 PARAMETERS

| name | IN/OUT | description |
| --- | --- | --- |
| handler | IN | 日志回调函数 |

- 数据结构说明

```c
typedef void (*ubs_engine_log_handler)(uint32_t level, const char *message);
```

字段说明：

| name | description |
| --- | --- |
| level | 日志级别，类型为 `uint32_t` |
| message | 日志消息内容，字符串以 `\0` 结尾 |

日志级别取值如下：

| level | enum | description |
| --- | --- | --- |
| `0` | `DEBUG` | 调试日志 |
| `1` | `INFO` | 普通信息日志 |
| `2` | `WARN` | 告警日志 |
| `3` | `ERROR` | 错误日志 |

## 返回值 RETURN VALUE

无。

## 错误 ERRORS

无。

## 约束 CONSTRAINTS

传入 `NULL` 时，日志输出回退到标准输出。

回调函数由调用方提供，调用方需要自行保证回调函数内部的线程安全和资源管理安全。

## 附注 NOTES

该接口只负责注册日志处理函数，不负责日志级别过滤。

## 样例 EXAMPLES

以下程序注册一个简单的日志回调函数，将日志打印到标准输出。

```c
#include <stdio.h>
#include <ubs_engine_log.h>

static void simple_log_handler(uint32_t level, const char *message)
{
    printf("[level=%u] %s\n", level, message ? message : "(null)");
}

int main(void)
{
    ubs_engine_log_callback_register(simple_log_handler);

    /* Do your work here... */

    return 0;
}
```

如果希望恢复为标准输出方式，也可以传入空指针：

```c
#include <ubs_engine_log.h>

int main(void)
{
    ubs_engine_log_callback_register(NULL);
    return 0;
}
```
