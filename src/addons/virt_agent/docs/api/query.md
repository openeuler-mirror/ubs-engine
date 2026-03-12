# 1. ubs_virt_agent_mem_fragmentation_node_info

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info(numa_info_t **node_list, uint32_t *node_cnt);
```

## 描述 DESCRIPTION

查询本节点Numa信息。

## 参数 Parameters

| name     | IN/OUT | description |
| -------- |--------|-------------|
| node_list | OUT    | NUMA信息列表    |
|node_cnt| OUT| 列表大小        |

- 数据结构说明
```c
VIRT_MAX_NODE_ID_LENGTH = 48
VIRT_MAX_UUID_LENGTH = 37
VIRT_MAX_NAME_LENGTH = 128
VIRT_MAX_STATE_LENGTH = 128
UBS_VA_HOST_NAME_MAX = 64
typedef struct {
    uint64_t pageSize{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
} numa_page_data;
typedef struct {
    time_t timestamp;
    char node_id[VIRT_MAX_NODE_ID_LENGTH];
    char host_name[UBS_VA_HOST_NAME_MAX];
    int16_t numa_id;
    int16_t socket_id;             // Socket ID mapped to CPUs bound to this NUMA
    int16_t is_local;              // Whether this is a local NUMA (0: non-local, 1: local)
    uint64_t mem_total;            // Total memory of this NUMA node (inclusive), collected from system files, in kB
    uint64_t mem_free;             // Free memory on this NUMA node, collected from system files, in kB
    numa_page_data* huge_page_data;
    uint64_t numaPageInfoCount;
} numa_info_t;
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
#include <ubs_virt_agent_mem_fragmentation.h>

int main(void)
{
    int32_t ret;
    numa_info_t* node_list = null;
    uint32_t node_cnt;
    ret = ubs_virt_agent_mem_fragmentation_node_info(&node_list, &node_cnt);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_fragmentation_node_info failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 2. ubs_virt_agent_mem_fragmentation_vm_info

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_vm_info(vm_domain_info_t **vm_info_list, uint32_t *vm_info_cnt);
```

## 描述 DESCRIPTION

查询本节点虚拟机信息。

## 参数 Parameters

| name         | IN/OUT | description |
|--------------|--------|-------------|
| vm_info_list | OUT    | 虚拟机信息列表。    |
| vm_info_cnt  | OUT    | 列表大小        |

- 数据结构说明
```c
typedef struct {
    char nodeId[VIRT_MAX_NODE_ID_LENGTH]; // Physical node ID (from control-plane configuration file)
    char hostName[UBS_VA_HOST_NAME_MAX];  // Physical node host name (from VM XML definition)
    char uuid[VIRT_MAX_UUID_LENGTH];       // VM UUID (from VM XML definition)
    char name[VIRT_MAX_NAME_LENGTH];       // VM name (from VM XML definition)
    char state[VIRT_MAX_STATE_LENGTH];     // VM state
    int64_t vmCreateTime;
    uint64_t maxMem;
    pid_t pid;
} vm_meta_data_t;
typedef struct {
    time_t timestamp;
    vm_meta_data_t metadata;
    vm_numa_info_t* numaInfo;
    uint64_t numaInfoCount;
} vm_domain_info_t;
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
#include <ubs_virt_agent_mem_fragmentation.h>

int main(void)
{
    int32_t ret;
    vm_domain_info_t* vm_info_list = null;
    uint32_t vm_info_cnt;
    ret = ubs_virt_agent_mem_fragmentation_vm_info(&vm_info_list, &vm_info_cnt);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_fragmentation_vm_info failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```