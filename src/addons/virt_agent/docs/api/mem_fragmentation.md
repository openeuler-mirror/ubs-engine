# 1. ubs_virt_agent_mem_borrow_strategy

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_borrow_strategy(const src_memory_borrow_param* src_param, borrow_strategy_c* borrow_strategy);
```

## 描述 DESCRIPTION

获取内存借用策略

## 参数 Parameters

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| src_param       | IN     | 内存借用信息      |
| borrow_strategy | OUT    | 内存借用决策信息    |

- 数据结构说明

```c
constexpr uint32_t MAX_BORROW_ID_COUNT = 2000;
constexpr uint32_t MAX_BORROW_ID_LENGTH = 128;
constexpr uint32_t MAX_DEST_NUMA_NUM = 1;
constexpr uint32_t MAX_VM_NUM = 300;
constexpr uint32_t MAX_NODE_NUM = 32;
constexpr uint32_t MAX_DEST_PARAM_SIZE = MAX_BORROW_ID_COUNT;
constexpr uint32_t VIRT_MAX_NODE_ID_LENGTH = 48;
constexpr uint32_t MEM_TASK_ID_MAX = 256;
typedef struct {
    char src_nid[VIRT_MAX_NODE_ID_LENGTH];
    int16_t src_socket_id;
    int16_t src_numa_id;
    uint64_t borrow_size;
}src_memory_borrow_param;
typedef struct {
    char host_id[VIRT_MAX_NODE_ID_LENGTH];
    uint16_t socket_id;
    uint16_t numa_nums;
    int numa_ids[MAX_DEST_NUMA_NUM];
    uint64_t mem_sizes[MAX_DEST_NUMA_NUM];
}dst_numa_info_c;
typedef struct {
    char src_host_id[VIRT_MAX_NODE_ID_LENGTH];
    int16_t src_socket_id;
    int16_t src_numa_id;
    uint64_t borrow_size;
    dst_numa_info_c dest_numa_infos[MAX_DEST_PARAM_SIZE];
    uint32_t dest_numa_infos_size;
}borrow_strategy_c;
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
    src_memory_borrow_param src_param = {
            .src_nid = "1",
            .src_socket_id = 2,
            .src_numa_id = 0,
            .borrow_size = 128
    };
    borrow_strategy_c borrow_strategy;
    ret = ubs_virt_agent_mem_borrow_strategy(&src_param, &borrow_strategy);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_borrow_strategy failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 2. ubs_virt_agent_mem_borrow_execute

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_borrow_execute(const borrow_setting_c *borrow_setting, mem_borrow_result_c *result);
```

## 描述 DESCRIPTION

根据借用决策结果执行内存借用。

## 参数 Parameters

| name           | IN/OUT | description                                     |
|----------------|--------|-------------------------------------------------|
| borrow_setting | IN     | 内存借用策略信息，是否异步执行开关。                              |
| result         | OUT    | 借用ID列表，借入的远端NUMA ID列表，借用ID列表大小，借入的远端NUMA ID列表大小 |

- 数据结构说明

```c
typedef struct {
    borrow_strategy_c borrow_strategy;
    bool isAsync;
}borrow_setting_c;
typedef struct {
    char borrow_ids_ptr[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint16_t present_numa_ids_ptr[MAX_BORROW_ID_COUNT];
    uint32_t borrow_ids_size;
    uint32_t present_numa_ids_size;
    char task_id[MEM_TASK_ID_MAX];
} mem_borrow_result_c;
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
    borrow_strategy_c borrow_strategy = {
            .src_host_id = "1",
            .src_socket_id = 1,
            .src_numa_id = 0,
            .borrow_size = 128,
            .dest_numa_infos = {"1", 1, 1, {1}, {128}},
            .dest_numa_infos_size = 1
    };
    borrow_setting_c borrow_setting = {
            borrow_strategy = borrow_strategy,
            isAsync = true
    };
    mem_borrow_result_c result;
    ret = ubs_virt_agent_mem_borrow_execute(&borrow_setting, &result);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_borrow_execute failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 3. ubs_virt_agent_mem_migrate_strategy

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_migrate_strategy(const MemMigrateStrategySrcParam* srcParam,
                                                     MemMigrateStrategy* strategy);
```

## 描述 DESCRIPTION

获取内存迁移策略。

## 参数 Parameters

| name      | IN/OUT | description |
|-----------|--------|-------------|
| src_param | IN     | 内存迁移信息。     |
| strategy  | OUT    | 内存迁移决策信息    |

- 数据结构说明

```c
typedef struct {
    pid_t pid;
    uint16_t ratio;
}MemMigrateStrategyVmInfo;
typedef struct {
    uint64_t borrowSize;
    char borrowInNode[VIRT_MAX_NODE_ID_LENGTH];
    MemMigrateStrategyVmInfo vmInfoList[MAX_VM_NUM];
    uint32_t vmInfoListSize;
}MemMigrateStrategySrcParam;
typedef struct {
    uint16_t destNumaId;
    uint64_t memSize;
    pid_t pid;
}VmMigrateStrategy;
typedef struct {
    uint32_t vmInfoListSize;
    VmMigrateStrategy* vmInfoList;
    uint64_t waitingTime;
}MemMigrateStrategy;
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
    MemMigrateStrategySrcParam srcParam = {
            .borrowSize = 128,
            .borrowInNode = "1",
            .vmInfoList = {{123, 25}},
            .vmInfoListSize = 1
    };
    MemMigrateStrategy strategy;
    ret = ubs_virt_agent_mem_migrate_strategy(&srcParam, &strategy);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_migrate_strategy failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 4. ubs_virt_agent_mem_migrate_execute

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_migrate_execute(const MemMigrateExecuteSrcParam *srcParam);
```

## 描述 DESCRIPTION

根据迁移决策结果执行内存迁移。

## 参数 Parameters

| name           | IN/OUT | description                                     |
|----------------|--------|-------------------------------------------------|
| src_param | IN     | 内存迁移决策信息。                              |

- 数据结构说明

```c
typedef struct {    
    char borrowInNode[VIRT_MAX_NODE_ID_LENGTH];
    char borrowIds[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint32_t borrowIdsCount;
    VmMigrateStrategy vmInfoList[MAX_VM_NUM];
    uint32_t vmInfoListSize;
    uint64_t waitingTime;
}MemMigrateExecuteSrcParam;
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
    MemMigrateExecuteSrcParam srcParam = {
            .borrowInNode = "1",
            .borrowIds = {"1-abc123"},
            .borrowIdsCount = 1,
            .vmInfoList = {{1, 128, 123}},
            .vmInfoListSize = 1,
            .waitingTime = 1000
    };
    ret = ubs_virt_agent_mem_migrate_execute(&srcParam);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_migrate_execute failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 5. ubs_virt_agent_mem_return

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_return(bool isAsync, char **task_id, uint32_t *task_id_len);
```

## 描述 DESCRIPTION

根据参数进行内存归还。

## 参数 Parameters

| name           | IN/OUT | description                                     |
|----------------|--------|-------------------------------------------------|
| isAsync | IN     | 异步执行开关。true：表示接口内部为异步执行内存归还。false：表示接口内部为同步执行内存归还。 |
| task_id         | OUT    | 后台任务id |
|task_id_len | OUT| 后台任务id长度|

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
    bool isAsync = true;
    char *task_id;
    uint32_t task_id_len;
    ret = ubs_virt_agent_mem_return(isAsync, &task_id, &task_id_len);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_return failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 6. ubs_virt_agent_mem_rollback

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_rollback(const RollbackSrcParam *srcParam);
```

## 描述 DESCRIPTION

借用内存回滚。

## 参数 Parameters

| name     | IN/OUT | description                  |
|----------|--------|------------------------------|
| srcParam | IN     | 回滚的入参，包括节点id、节点上的borrowId列表。 |

- 数据结构说明

```c
typedef struct {
    char node_id[VIRT_MAX_NODE_ID_LENGTH];
    char borrow_id_list[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint32_t borrow_id_size;
}RollbackSrcParam;
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
    RollbackSrcParam srcParam = {
            .node_id = "1",
            .borrow_id_list = {"1-abc123"},
            .borrow_id_size = 1
    };
    ret = ubs_virt_agent_mem_rollback(&srcParam);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_rollback failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 7. ubs_virt_agent_mem_fragmentation_node_anti_affinity

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_anti_affinity(const NodeAntiDictionary* dict);
```

- 数据结构说明

```c
struct KeyValuePair {
    char key[VIRT_MAX_NODE_ID_LENGTH];
    char value[MAX_NODE_NUM][VIRT_MAX_NODE_ID_LENGTH];
    size_t value_count;
};
struct NodeAntiDictionary {
    struct KeyValuePair entries[MAX_NODE_NUM];
    size_t entry_count;
};
```

## 描述 DESCRIPTION

设置节点反亲和性关系。

## 参数 Parameters

| name           | IN/OUT | description                                     |
|----------------|--------|-------------------------------------------------|
| dict | IN     | 反亲和信息字典。                              |

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
    NodeAntiDictionary dict = {
            .entries = {
            {"1", {"2"}, 1},
            {"2", {"3"}, 1},
            {"3", {"1"}, 1},
            },
            .entry_count = 3
    };
    ret = ubs_virt_agent_mem_fragmentation_node_anti_affinity(&dict);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_mem_fragmentation_node_anti_affinity failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 8. ubs_virt_agent_sync_task_query

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_sync_task_query(char *task_id, uint32_t task_id_len, async_task_info_c *result);
```

- 数据结构说明

```c
using async_task_status_c = enum {
    ASYNC_TASK_NOT_EXIST = 0,
    ASYNC_TASK_RUNNING   = 1,
    ASYNC_TASK_SUCCESS   = 2,
    ASYNC_TASK_FAILED    = 3
};
```

## 描述 DESCRIPTION

借用归还任务查询。

## 参数 Parameters

| name        | IN/OUT | description |
|-------------|--------|-------------|
| task_id     | IN     | 后台任务id。     |
| task_id_len | IN     | 后台任务id长度。   |
| result      | OUT    | 任务执行结果信息。   |

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
    char task_id[128] = "abc123";
    uint32_t task_id_len = 6;
    async_task_info_c *result
    ret = ubs_virt_agent_sync_task_query(task_id, task_id_len, &result);
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_sync_task_query failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

# 9. ubs_virt_agent_mem_fragmentation_node_info_list

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c++
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info_list(node_info_list_s *node_info_list);
```

- 数据结构说明

```c++
typedef struct {
    uint64_t pageSize{};
    uint64_t hugePageTotal{};
    uint64_t hugePageFree{};
} numa_page_data;

typedef struct {
    time_t timestamp;
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char host_name[UBS_VA_HOST_NAME_MAX];
    int16_t numa_id;
    int16_t socket_id;  // Socket ID mapped to CPUs bound to this NUMA
    int16_t is_local;   // Whether this is a local NUMA (0: non-local, 1: local)
    uint64_t mem_total; // Total memory of this NUMA node (inclusive), collected from system files, in kB
    uint64_t mem_free;  // Free memory on this NUMA node, collected from system files, in kB
    numa_page_data* huge_page_data;
    uint64_t numaPageInfoCount;
} numa_info_t;

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];  // 节点名称
    numa_info_t *numa_infos;  // numa信息
    uint32_t numa_len; // numa数量
    bool is_current; // 是否是查询节点
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

```c++
#include <stdio.h>
#include <ubs_virt_agent_mem_fragmentation.h>

int main(void)
{
    node_info_list_s *node_info_list = null;
    const virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_info_list(node_info_list);
    if (ret != 0) {
        return ret;
    }
    /* Do your work here... */
    return 0;
}
```

# 10. ubs_virt_agent_mem_borrow

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c++
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_borrow(const mem_borrow_param_s *param, const bool is_async, mem_borrow_result_s *result);
```

- 数据结构说明

```c++
typedef struct {
    int16_t numa_id; // numaId
} numa_meta_info_s;

typedef struct {
    char src_nid[VIRT_MEM_MAX_NODE_ID_LENGTH];  // nodeId
    numa_meta_info_s *numa_meta_infos; // numa信息了列表
    uint32_t numa_len; // numa数量
    uint64_t borrow_size; // 借用大小, 单位MB
} mem_borrow_param_s;

typedef struct {
    char borrow_ids_ptr[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH]; // 借用标识符列表
    uint16_t present_numa_ids_ptr[MAX_BORROW_ID_COUNT]; // 借用内存映射numaId列表
    uint32_t borrow_ids_size; // 借用标识符列表长度
    uint32_t present_numa_ids_size; // 映射numaId列表长度
    char task_id[MEM_TASK_ID_MAX]; // 借用任务
} mem_borrow_result_c;

typedef struct {
    mem_borrow_result_c *mem_borrow_result_list; // 内存借用结果列表
    uint16_t mem_borrow_result_list_len; // 借用结果列表长度
} mem_borrow_result_s;
```

## 描述 DESCRIPTION

大规格虚机场景, 按节点借用内存

## 参数 Parameters

| name     | IN/OUT | description    |
|----------|--------|----------------|
| param    | IN     | 借用任务入参         |
| is_async | IN     | 是否异步执行借用任务标识符。 |
| result   | OUT    | 借用结果信息。        |

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

```c++
#include <stdio.h>
#include <ubs_virt_agent_mem_fragmentation.h>

int main(void)
{
    mem_borrow_param_s *param = (mem_borrow_param_s *)malloc(sizeof(mem_borrow_param_s));
    param -> src_nid = "node0";
    param -> numa_len = 4;
    param -> borrow_size = 3145728; // 单位MB, 此处借用3TB内存
    numa_meta_info_s *numa_meta_infos = (numa_meta_info_s *)malloc(sizeof(numa_meta_info_s) * param -> numa_len);
    
    // 根据实际情况填写 or 根据ubs_virt_agent_mem_fragmentation_node_info_list查询结果填写
    // 此处按照如下numa关系配置
    // NUMA0; NUMA1; NUMA2; NUMA3
    for (int i = 0; i < param -> numa_len; ++i) {
        numa_meta_infos[i].numa_id = i;
    }
    mem_borrow_result_s* result = null;
    const virt_agent_ret_t ret = ubs_virt_agent_mem_borrow(param, true, result);
    if (ret != 0) {
        return ret;
    }
    /* Do your work here... */
    return 0;
}
```

# 11. ubs_virt_agent_page_swap_enable

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c++
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_page_swap_enable(const pid_t pid, const page_swap_enable_s *page_swap_enable);
```

- 数据结构说明

```c++
typedef struct {
    uint32_t numa_id; // numa_id
    uint32_t quota; // 限额大小, 单位MB
} numa_quota_s;

typedef struct {
    numa_quota_s *local_numas; // 本地numaId列表
    uint8_t local_numa_len; // 本地numaId列表长度
    numa_quota_s *remote_numas; // 远端映射本地numaId列表
    uint8_t remote_numa_len; // 远端映射本地numaId列表长度
} page_swap_pair_s;

typedef struct {
    page_swap_pair_s *page_swap_pairs; // 本地远端内存映射关系列表
    uint8_t page_swap_pairs_len; // 本地远端内存映射关系列表长度
} page_swap_enable_s;
```

## 描述 DESCRIPTION

大规格虚机场景, 通过配额实现使能冷热页流动

## 参数 Parameters

| name             | IN/OUT | description |
|------------------|--------|-------------|
| pid_t            | IN     | 虚拟机进程ID。    |
| page_swap_enable | IN     | 内存换页使能参数。   |

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

```c++
#include <stdio.h>
#include <ubs_virt_agent_mem_fragmentation.h>

int main(void)
{
    const pid_t pid = 123; // 虚拟机实际进程号
    page_swap_enable_s *page_swap_enable = (page_swap_enable_s *)malloc(sizeof(page_swap_enable) * page_swap_pairs_len);
    page_swap_enable -> page_swap_pairs_len = 2;
    page_swap_enable -> page_swap_pairs = (page_swap_pair_s *)malloc(sizeof(page_swap_pair_s) * page_swap_enable -> page_swap_pairs_len);
    page_swap_enable -> page_swap_pairs[0] -> local_numa_len = 2;
    page_swap_enable -> page_swap_pairs[0] -> local_numas = (numa_quota_s *)malloc(sizeof(numa_quota_s) * page_swap_enable -> local_numa_len);
    page_swap_enable -> page_swap_pairs[0] -> local_numas[0].numa_id = 0;
    page_swap_enable -> page_swap_pairs[0] -> local_numas[0].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[0] -> local_numas[1].numa_id = 1;
    page_swap_enable -> page_swap_pairs[0] -> local_numas[1].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[0] -> remote_numa_len = 2;
    page_swap_enable -> page_swap_pairs[0] -> remote_numas = (numa_quota_s *)malloc(sizeof(numa_quota_s) * page_swap_enable -> remote_numa_len);
    page_swap_enable -> page_swap_pairs[0] -> remote_numas[0].numa_id = 4;
    page_swap_enable -> page_swap_pairs[0] -> remote_numas[0].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[0] -> remote_numas[1].numa_id = 5;
    page_swap_enable -> page_swap_pairs[0] -> remote_numas[1].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[1] -> local_numa_len = 2;
    page_swap_enable -> page_swap_pairs[1] -> local_numas = (numa_quota_s *)malloc(sizeof(numa_quota_s) * page_swap_enable -> local_numa_len);
    page_swap_enable -> page_swap_pairs[1] -> local_numas[0].numa_id = 2;
    page_swap_enable -> page_swap_pairs[1] -> local_numas[0].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[1] -> local_numas[1].numa_id = 3;
    page_swap_enable -> page_swap_pairs[1] -> local_numas[1].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[1] -> remote_numa_len = 2;
    page_swap_enable -> page_swap_pairs[1] -> remote_numas = (numa_quota_s *)malloc(sizeof(numa_quota_s) * page_swap_enable -> remote_numa_len);
    page_swap_enable -> page_swap_pairs[1] -> remote_numas[0].numa_id = 7;
    page_swap_enable -> page_swap_pairs[1] -> remote_numas[0].numa_id = 786432;
    page_swap_enable -> page_swap_pairs[1] -> remote_numas[1].numa_id = 8;
    page_swap_enable -> page_swap_pairs[1] -> remote_numas[1].numa_id = 786432;
    virt_agent_ret_t ret = ubs_virt_agent_page_swap_enable(pid, page_swap_enable);
    if (ret != 0) {
        return ret;
    }
    /* Do your work here... */
    return 0;
}
```