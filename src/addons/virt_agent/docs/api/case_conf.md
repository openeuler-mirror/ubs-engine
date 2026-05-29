# 1. ubs_virt_agent_case_conf_get

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_case_conf.h>
virt_agent_ret_t ubs_virt_agent_case_conf_get(case_conf_info_t *case_conf_info);
```

## 描述 DESCRIPTION

获取当前虚拟化场景配置信息。

## 参数 Parameters

| name           | IN/OUT | description |
|----------------|--------|-------------|
| case_conf_info | OUT    | 场景及超分比例信息   |

- 数据结构说明

```c
typedef struct {
    char cur_case[128];
    char over_commitment_ratio[128];
    char migrate_waterLine[128];
    uint64_t index;
    char host_id[48];
} case_conf_info_t;
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

## 样例 EXAMPLES

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_case_conf.h>

int main(void)
{
    int32_t ret;
    case_conf_info_t case_conf_info;    
    ret = ubs_virt_agent_case_conf_get(&case_conf_info);
    if (ret != VA_SUCESS) {
        perror("get failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 2. ubs_virt_agent_case_conf_set

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_case_conf.h>
virt_agent_ret_t ubs_virt_agent_case_conf_set(const char *param, case_conf_set_info_t *case_conf_set_info);
```

## 描述 DESCRIPTION

设置当前虚拟化场景配置，仅首次设置能真正成功。

## 参数 Parameters

| name     | IN/OUT | description |
| -------- |--------|-------------|
| param |   IN  | 场景和超分比例信息。  |
|case_conf_set_info| OUT | 设置结果|

- 数据结构说明

```c
typedef struct {
    uint32_t ret;
} case_conf_set_info_t;
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

## 样例 EXAMPLES

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_case_conf.h>

int main(void)
{
    int32_t ret;
    char param[128] = "{\"caseType\": \"overCommitment\",\"overCommitment\": 1.25}";
    case_conf_set_info_t case_conf_set_info;    
    ret = ubs_virt_agent_case_conf_set(&param, &case_conf_set_info);
    if (ret != VA_SUCESS) {
        perror("set failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```
