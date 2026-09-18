# 1. update_page_flow_and_status

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_migrate.h>
int32_t update_page_flow_and_status(const char *opt, const char *uuid);
```

## 描述 DESCRIPTION

更新冷热页流动开关和虚拟机状态。

## 参数 Parameters

| name | IN/OUT | description |
|------|--------|-------------|
| opt  | IN     | 操作类型        |
| uuid | IN     | 虚拟机UUID     |

## 返回值 RETURN VALUE

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| VA_ERROR_BASE                | 基本错误类型      |
| VA_ERROR_INVALID_PARAM       | 参数不合法       |
| VA_ERROR_NULL_POINTER        | 空指针         |
| VA_ERROR_MEM_ALLOCATE_FAILED | 内存分配失败      |
| VA_ERROR_MEM_COPY_FAILED     | 内存拷贝失败      |
| VA_ERROR_SERIALIZE_FAILED    | 序列化失败       |
| VA_ERROR_DESERIALIZE_FAILED  | 反序列化失败      |
| VA_ERROR_TIMEOUT_FAILED      | 超时          |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_mem_migrate.h>

int main(void)
{
    int32_t ret;
    char opt[128] = "true";
    char uuid[128] = "xxxx";
    ret = update_page_flow_and_status(opt, uuid);
    if (ret != VA_SUCCESS) {
        perror("update_page_flow_and_status failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 2. ubs_virt_agent_make_migrate_decision

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_ham_migrate.h>
virt_agent_ret_t ubs_virt_agent_make_migrate_decision(uint32_t vmMemoryMB, const char *uuid, const char *destHostName,
                                                      uint32_t destNumaId, uint32_t *migrateStrategy);
```

## 描述 DESCRIPTION

获取虚拟机迁移策略。

## 参数 Parameters

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| vmMemoryMB      | IN     | 虚拟机内存大小     |
| uuid            | IN     | 虚拟机UUID     |
| destHostName    | IN     | 目标主机名       |
| destNumaId      | IN     | 目标NUMA ID   |
| migrateStrategy | OUT    | 迁移决策结果      |

## 返回值 RETURN VALUE

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| VA_ERROR_BASE                | 基本错误类型      |
| VA_ERROR_INVALID_PARAM       | 参数不合法       |
| VA_ERROR_NULL_POINTER        | 空指针         |
| VA_ERROR_MEM_ALLOCATE_FAILED | 内存分配失败      |
| VA_ERROR_MEM_COPY_FAILED     | 内存拷贝失败      |
| VA_ERROR_SERIALIZE_FAILED    | 序列化失败       |
| VA_ERROR_DESERIALIZE_FAILED  | 反序列化失败      |
| VA_ERROR_TIMEOUT_FAILED      | 超时          |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_ham_migrate.h>

int main(void)
{
    int32_t ret;
    uint32_t vmMemoryMB = 2048;
    char uuid[128] = "1863ec26-3fda-4f6f-97b2-fcaf9139ac7d";
    char destHostName[128] = "computer02";
    uint32_t destNumaId = 0;
    uint32_t migrateStrategy;
    ret = ubs_virt_agent_make_migrate_decision(vmMemoryMB, uuid, destHostName, destNumaId, &migrateStrategy);
    if (ret != VA_SUCCESS) {
        perror("ubs_virt_agent_make_migrate_decision failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 3. RackStartIpcClientWithTimeout

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_ham_migrate.h>
virt_agent_ret_t RackStartIpcClientWithTimeout(uint16_t timeout);
```

## 描述 DESCRIPTION

libvirt与virt_agent通信超时时间设置。

## 参数 Parameters

| name            | IN/OUT | description                |
|-----------------|--------|----------------------------|
| timeout      | IN     | libvirt与virt_agent超时时间，单位秒 |

## 返回值 RETURN VALUE

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| VA_ERROR_BASE                | 基本错误类型      |
| VA_ERROR_INVALID_PARAM       | 参数不合法       |
| VA_ERROR_NULL_POINTER        | 空指针         |
| VA_ERROR_MEM_ALLOCATE_FAILED | 内存分配失败      |
| VA_ERROR_MEM_COPY_FAILED     | 内存拷贝失败      |
| VA_ERROR_SERIALIZE_FAILED    | 序列化失败       |
| VA_ERROR_DESERIALIZE_FAILED  | 反序列化失败      |
| VA_ERROR_TIMEOUT_FAILED      | 超时          |

## 约束 CONSTRAINTS

timeout 必须 > 0 且 <= 1200，单位为秒(s）

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_ham_migrate.h>

int main(void)
{
    int32_t ret;
    uint16_t timeout = 128;
    ret = RackStartIpcClientWithTimeout(timeout);
    if (ret != VA_SUCCESS) {
        perror("RackStartIpcClientWithTimeout failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 4. RackSyncSendForHam

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_ham_migrate.h>
int RackSyncSendForHam(HamComByteBuffer *request, HamComByteBuffer *response);
```

## 描述 DESCRIPTION

获取虚拟机迁移策略。

## 参数 Parameters

| name     | IN/OUT | description                   |
|----------|--------|-------------------------------|
| request  | IN     | libvirt调用virt_agent传参（json）   |
| response | OUT    | virt_agent给libvirt返回的参数（json） |

- 数据结构说明

```c
typedef struct {
    uint8_t *data;
    uint32_t len;
} HamComByteBuffer;
```

## 返回值 RETURN VALUE

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| VA_ERROR_BASE                | 基本错误类型      |
| VA_ERROR_INVALID_PARAM       | 参数不合法       |
| VA_ERROR_NULL_POINTER        | 空指针         |
| VA_ERROR_MEM_ALLOCATE_FAILED | 内存分配失败      |
| VA_ERROR_MEM_COPY_FAILED     | 内存拷贝失败      |
| VA_ERROR_SERIALIZE_FAILED    | 序列化失败       |
| VA_ERROR_DESERIALIZE_FAILED  | 反序列化失败      |
| VA_ERROR_TIMEOUT_FAILED      | 超时          |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

# 5. RackAsyncSendForHam

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_ham_migrate.h>
int RackAsyncSendForHam(HamComByteBuffer *request, HamComCallbackDef *callback);
```

## 描述 DESCRIPTION

libvirt与virt_agent异步调用接口

## 参数 Parameters

| name            | IN/OUT | description                  |
|-----------------|--------|------------------------------|
| request      | IN     | libvirt调用virt_agent传参（json）  |
| callback            | IN     | libvirt调用virt_agent回调函数的函数指针 |

- 数据结构说明

```c
typedef void (*HamComCallbackFunc)(void *ctx, void *recv, uint32_t len, int32_t result);
typedef struct {
    HamComCallbackFunc cb; 
    void *cbCtx;         
} HamComCallbackDef;
```

## 返回值 RETURN VALUE

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                        | Description |
|------------------------------|-------------|
| VA_ERROR_BASE                | 基本错误类型      |
| VA_ERROR_INVALID_PARAM       | 参数不合法       |
| VA_ERROR_NULL_POINTER        | 空指针         |
| VA_ERROR_MEM_ALLOCATE_FAILED | 内存分配失败      |
| VA_ERROR_MEM_COPY_FAILED     | 内存拷贝失败      |
| VA_ERROR_SERIALIZE_FAILED    | 序列化失败       |
| VA_ERROR_DESERIALIZE_FAILED  | 反序列化失败      |
| VA_ERROR_TIMEOUT_FAILED      | 超时          |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无
