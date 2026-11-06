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
VIRT_MEM_MAX_NODE_ID_LENGTH = 48
VIRT_MAX_UUID_LENGTH = 37
VIRT_MAX_NAME_LENGTH = 128
VIRT_MAX_STATE_LENGTH = 128
UBS_VA_HOST_NAME_MAX = 64
typedef struct {
    uint64_t pageSize;
    uint64_t hugePageTotal;
    uint64_t hugePageFree;
} numa_page_data;
typedef struct {
    time_t timestamp;
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char host_name[UBS_VA_HOST_NAME_MAX];
    int16_t numa_id;
    int16_t socket_id;             // 该NUMA所绑定CPU的Socket ID
    int16_t is_local;              // 是否为本地NUMA（0：非本地，1：本地）
    uint64_t mem_total;            // 该NUMA节点的总内存，从系统文件采集，单位kB
    uint64_t mem_free;             // 该NUMA节点的空闲内存，从系统文件采集，单位kB
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
    numa_info_t* node_list = NULL;
    uint32_t node_cnt;
    ret = ubs_virt_agent_mem_fragmentation_node_info(&node_list, &node_cnt);
    if (ret != VA_SUCCESS) {
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
    int16_t numaId;   // CPU所属NUMA ID
    int16_t socketId; // CPU所属Socket ID
    bool isLocal;
    uint64_t pageSize; // 虚机在该NUMA上使用的大页大小，单位kB
    int64_t usedMem;
} vm_numa_info_t;

typedef struct {
    char nodeId[VIRT_MAX_NODE_ID_LENGTH]; // 物理节点ID（来自管控配置文件）
    char hostName[UBS_VA_HOST_NAME_MAX];  // 物理节点主机名（来自虚机XML定义）
    char uuid[VIRT_MAX_UUID_LENGTH];       // 虚机UUID（来自虚机XML定义）
    char name[VIRT_MAX_NAME_LENGTH];       // 虚机名称（来自虚机XML定义）
    char state[VIRT_MAX_STATE_LENGTH];     // 虚机状态
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
    vm_domain_info_t* vm_info_list = NULL;
    uint32_t vm_info_cnt;
    ret = ubs_virt_agent_mem_fragmentation_vm_info(&vm_info_list, &vm_info_cnt);
    if (ret != VA_SUCCESS) {
        perror("ubs_virt_agent_mem_fragmentation_vm_info failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 3. ubs_virt_agent_mem_fragmentation_node_info_list

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info_list(node_info_list_s *node_info_list);
```

- 数据结构说明

```c
typedef struct {
    uint64_t pageSize;
    uint64_t hugePageTotal;
    uint64_t hugePageFree;
} numa_page_data;

typedef struct {
    time_t timestamp;
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char host_name[UBS_VA_HOST_NAME_MAX];
    int16_t numa_id;
    int16_t socket_id;  // 该NUMA所绑定CPU的Socket ID
    int16_t is_local;   // 是否为本地NUMA（0：非本地，1：本地）
    uint64_t mem_total; // 该NUMA节点的总内存，从系统文件采集，单位kB
    uint64_t mem_free;  // 该NUMA节点的空闲内存，从系统文件采集，单位kB
    numa_page_data* huge_page_data;
    uint64_t numaPageInfoCount;
} numa_info_t;

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];  // 节点名称
    numa_info_t *numa_infos;  // numa信息
    uint32_t numa_len; // numa数量
    bool is_current; // 是否是查询节点
    bool is_master; // 是否是主节点
    char *pod_id;  // 物理节点所在二层超节点ID（superPodId），NUL 结尾，读取失败时为 nullptr
    uint32_t pod_id_len; // pod_id 长度（不含 NUL 结尾符）
} node_info_s;

typedef struct {
    node_info_s *node_infos; // 节点列表
    uint32_t node_len; // 节点数量
} node_info_list_s;
```

## 描述 DESCRIPTION

查询集群内所有节点的内存信息

## 参数 Parameters

| name           | IN/OUT | description |
|----------------|--------|-------------|
| node_info_list | OUT    | 节点信息列表      |

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
    node_info_list_s node_info_list;
    const virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_info_list(&node_info_list);
    if (ret != 0) {
        return ret;
    }
    /* Do your work here... */
    return 0;
}
```

# 4. ubs_virt_agent_mem_fragmentation_vm_info_list

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_vm_info_list(vm_info_list_s *vm_info_list);
```

- 数据结构说明

```c
typedef struct {
    int16_t numaId;
    int16_t socketId;
    bool isLocal;
    uint64_t pageSize; // 虚机在该NUMA上使用的大页大小，单位kB
    int64_t usedMem;
} vm_numa_info_t;

typedef struct {
    char nodeId[VIRT_MEM_MAX_NODE_ID_LENGTH]; // 物理节点ID
    char hostName[UBS_VA_HOST_NAME_MAX];       // 物理节点主机名
    char uuid[VIRT_MAX_UUID_LENGTH];
    char name[VIRT_MAX_NAME_LENGTH];
    char state[VIRT_MAX_STATE_LENGTH];
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

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];  // 物理节点ID
    vm_domain_info_t *vm_infos;  // 虚拟机信息
    uint32_t vm_len; // 虚拟机数量
    bool is_current; // 是否是查询节点
    bool is_master; // 是否是主节点
    char *pod_id;  // 物理节点所在二层超节点ID（superPodId），NUL 结尾，读取失败时为 nullptr
    uint32_t pod_id_len; // pod_id 长度
} vm_node_info_s;

typedef struct {
    vm_node_info_s *vm_node_infos; // 节点列表
    uint32_t node_len; // 节点数量
} vm_info_list_s;
```

## 描述 DESCRIPTION

查询集群内所有节点的虚拟机信息

## 参数 Parameters

| name          | IN/OUT | description |
|---------------|--------|-------------|
| vm_info_list  | OUT    | 节点虚拟机信息列表  |

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
    vm_info_list_s vm_info_list;
    const virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_vm_info_list(&vm_info_list);
    if (ret != VA_SUCCESS) {
        return ret;
    }
    /* Do your work here... */
    return 0;
}
```
