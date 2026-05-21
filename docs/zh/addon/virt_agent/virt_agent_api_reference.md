# virt_agent API 参考

## 虚拟化

### ubs_virt_agent_case_conf_get

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_case_conf.h>
virt_agent_ret_t ubs_virt_agent_case_conf_get(case_conf_info_t *case_conf_info);
```

**描述 DESCRIPTION**

获取当前虚拟化场景配置信息。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_case_conf_set

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_case_conf.h>
virt_agent_ret_t ubs_virt_agent_case_conf_set(const char *param, case_conf_set_info_t *case_conf_set_info);
```

**描述 DESCRIPTION**

设置当前虚拟化场景配置，仅首次设置能真正成功。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

## 容器

### ubs_virt_agent_waterline_mem_borrow

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_virt_agent_waterline_mem_borrow(mem_borrow_request_t *memBorrowRequest, char ***borrowIds,
                                            uint32_t *idsSize);
```

**描述 DESCRIPTION**

内存借用执行。

**参数 Parameters**

| name             | IN/OUT | description |
|------------------|--------|-------------|
| memBorrowRequest | IN     | 内存借用请求体     |
| borrowIds        | OUT    | 借用ID列表      |
| idsSize          | OUT    | 借用ID列表大小    |

- 数据结构说明

```c
typedef struct {
    uint16_t highWaterMark;
    uint16_t lowWaterMark;
} watermark_t;
typedef struct {
    int socketId;
    int numaId;
} src_location_t;
typedef struct {
    char srcNid[16];
    src_location_t srcLocations[16];
    size_t srcLocationsSize;
} borrow_param_t
typedef struct {
    borrow_param_t borrowParam;
    uint64_t borrowSizes[64];
    size_t borrowSizesSize;
    watermark_t waterMark;
} mem_borrow_request_t;
```

**返回值 RETURN VALUE**

int类型：
0表示成功。
非0表示失败。

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_container.h>

int main(void)
{
    int32_t ret;
    borrow_param_t tmpBorrowParam {
        .srcNid = "1",
        .srcLocations = {{1, 1}, {2, 2}},
        .srcLocationsSize = 2
    };
    mem_borrow_request_t memBorrowRequest{
        .borrowParam = tmpBorrowParam,
        .borrowSizes = {128, 128},
        .borrowSizesSize = 2,
        .waterMark = {85, 80}
    };
    char **borrowIds = nullptr;
    uint32_t idsSize;
    ret = ubs_virt_agent_waterline_mem_borrow(&memBorrowRequest, &borrowIds, &idsSize);
    if (ret != 0) {
        perror("ubs_virt_agent_waterline_mem_borrow failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### ubs_virt_agent_waterline_mem_migrate

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_virt_agent_waterline_mem_migrate(mem_migrate_request_t *memMigrateRequest);
```

**描述 DESCRIPTION**

内存冷热页交换执行。

**参数 Parameters**

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| memMigrateRequest      | IN     | 内存冷热页交换请求体     |

- 数据结构说明

```c
typedef struct {
    pid_t pid;
    int ratio;
} container_param_t;
typedef struct {
    borrow_param_t borrowParam;
    uint64_t borrowSizes[64];
    size_t borrowSizesSize;
    watermark_t waterMark;
} mem_borrow_request_t;
```

**返回值 RETURN VALUE**

int类型：
0表示成功。
非0表示失败。

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_container.h>

int main(void)
{
    int32_t ret;
    borrow_param_t tmpBorrowParam {
        .srcNid = "1",
        .srcLocations = {{1, 1}, {2, 2}},
        .srcLocationsSize = 2
    };
    mem_migrate_request_t memMigrateRequest {
        .borrowParam = tmpBorrowParam,
        .borrowIds = {"1-abc123"},
        .borrowIdsSize = 1,
        .containerParam = {{12345, 25}},
        .containerParamSize = 1
    };
    ret = ubs_virt_agent_waterline_mem_migrate(&memBorrowRequest);
    if (ret != 0) {
        perror("ubs_virt_agent_waterline_mem_migrate failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### ubs_virt_agent_waterline_mem_return

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_virt_agent_waterline_mem_return(return_request_t *returnRequest);
```

**描述 DESCRIPTION**

内存归还执行

**参数 Parameters**

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| returnRequest      | IN     | 内存归还请求体     |

- 数据结构说明

```c
typedef struct {
    borrow_param_t borrowParam;
    char borrowIds[64][128];
    size_t borrowIdsSize;
    pid_t pids[64];
    size_t pidsSize;
} return_request_t;
```

**返回值 RETURN VALUE**

int类型：
0表示成功。
非0表示失败。

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_container.h>

int main(void)
{
    int32_t ret;
    borrow_param_t tmpBorrowParam {
        .srcNid = "1",
        .srcLocations = {{1, 1}, {2, 2}},
        .srcLocationsSize = 2
    };
    return_request_t returnRequest {
        .borrowParam = tmpBorrowParam,
        .borrowIds = {"1-abc123"},
        .borrowIdsSize = 1,
        .pids = {{12345, 23456}},
        .pidsSize = 2
    };
    char **borrowIds = nullptr;
    uint32_t idsSize;
    ret = ubs_virt_agent_waterline_mem_return(&returnRequest);
    if (ret != 0) {
        perror("ubs_virt_agent_waterline_mem_return failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### ubs_container_info_query

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_container_info_query(pid_param* param, pid_mem_info **pidInfos, uint32_t *InfoSize);
```

**描述 DESCRIPTION**

查询容器Pid本地和远端内存

**参数 Parameters**

| name     | IN/OUT | description                  |
|----------|--------|------------------------------|
| param  | IN     | 待查询的pid信息   |
| pidInfos | OUT    | pid对应的内存信息列表 |
| InfoSize | OUT    | pid对应的内存信息列表大小 |

- 数据结构说明

```c
typedef struct {
    char srcNid[16];
    pid_t pids[2048];
    size_t pids_size;
} pid_param;
typedef struct {
    pid_t pid;
    uint64_t localUsedMem;
    uint16_t localNumaIds[64];
    size_t localNumaCount;
    uint64_t remoteUsedMem;
} pid_mem_info;
```

**返回值 RETURN VALUE**

int类型：
0表示成功。
非0表示失败。

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_container.h>

int main(void)
{
    int32_t ret;
    pid_param param{
        .srcNid = "1",
        .pids = {123},
        .pids_size = 1
    };
    pid_mem_info *pidInfos = null;
    uint32_t InfoSize;
    ret = ubs_container_info_query(&param, &pidInfos, &InfoSize);
    if (ret != 0) {
        perror("ubs_container_info_query failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### ubs_container_inject_waterLine

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_container_inject_waterLine(watermark_t* param);
```

**描述 DESCRIPTION**

注入节点水线

**参数 Parameters**

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| param      | IN     | 水线配置信息     |

**返回值 RETURN VALUE**

int类型：
0表示成功。
非0表示失败。

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_container.h>

int main(void)
{
    int32_t ret;
    watermark_t param {
        .highWaterMark = 90,
        .lowWaterMark = 80
    };
    ret = ubs_container_inject_waterLine(&param);
    if (ret != 0) {
        perror("ubs_container_inject_waterLine failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### ubs_container_get_container_pids

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_container_get_container_pids(container_id_list *containerIdList, container_pid_info **param,
                                         uint32_t *InfoSize);
```

- 数据结构说明

```c
typedef struct {
    char containerId[128][128];
    size_t containerIdSize;
} container_id_list;
typedef struct {
    pid_t pids[2048];
    size_t pidsCount;
    char* containerId;
} container_pid_info;
```

**描述 DESCRIPTION**

指定容器查询容器内Pid

**参数 Parameters**

| name            | IN/OUT | description  |
|-----------------|--------|--------------|
| containerIdList | IN     | 容器ID信息       |
| param           | IN     | 容器内pid信息列表   |
| InfoSize        | IN     | 容器内pid信息列表大小 |

**返回值 RETURN VALUE**

int类型：
0表示成功。
非0表示失败。

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_container.h>

int main(void)
{
    int32_t ret;
    container_id_list containerIdList = {
            .containerId = {"123", "456"},
            .containerIdSize = 2
    };
    container_pid_info *param = null;
    uint32_t InfoSize;
    ret = ubs_container_get_container_pids(&containerIdList, &param, &InfoSize);
    if (ret != 0) {
        perror("get_container_pids failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

## 内存借用

### ubs_virt_agent_mem_borrow_strategy

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_borrow_strategy(const src_memory_borrow_param* src_param, borrow_strategy_c* borrow_strategy);
```

**描述 DESCRIPTION**

获取内存借用策略

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_borrow_execute

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_borrow_execute(const borrow_setting_c *borrow_setting, mem_borrow_result_c *result);
```

**描述 DESCRIPTION**

根据借用决策结果执行内存借用。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_migrate_strategy

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_migrate_strategy(const MemMigrateStrategySrcParam* srcParam,
                                                     MemMigrateStrategy* strategy);
```

**描述 DESCRIPTION**

获取内存迁移策略。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_migrate_execute

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_migrate_execute(const MemMigrateExecuteSrcParam *srcParam);
```

**描述 DESCRIPTION**

根据迁移决策结果执行内存迁移。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_return

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_return(bool isAsync, char **task_id, uint32_t *task_id_len);
```

**描述 DESCRIPTION**

根据参数进行内存归还。

**参数 Parameters**

| name           | IN/OUT | description                                     |
|----------------|--------|-------------------------------------------------|
| isAsync | IN     | 异步执行开关。true：表示接口内部为异步执行内存归还。false：表示接口内部为同步执行内存归还。 |
| task_id         | OUT    | 后台任务id |
|task_id_len | OUT| 后台任务id长度|

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_rollback

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_rollback(const RollbackSrcParam *srcParam);
```

**描述 DESCRIPTION**

借用内存回滚。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_fragmentation_node_anti_affinity

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

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

**描述 DESCRIPTION**

设置节点反亲和性关系。

**参数 Parameters**

| name           | IN/OUT | description                                     |
|----------------|--------|-------------------------------------------------|
| dict | IN     | 反亲和信息字典。                              |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_sync_task_query

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

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

**描述 DESCRIPTION**

借用归还任务查询。

**参数 Parameters**

| name        | IN/OUT | description |
|-------------|--------|-------------|
| task_id     | IN     | 后台任务id。     |
| task_id_len | IN     | 后台任务id长度。   |
| result      | OUT    | 任务执行结果信息。   |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_fragmentation_node_info_list

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

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

**描述 DESCRIPTION**

查询集群内所有节点的内存信息

**参数 Parameters**

| name           | IN/OUT | description |
|----------------|--------|-------------|
| node_info_list | OUT    | 节点信息列表      |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_borrow

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c++
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_borrow(const mem_borrow_param_s *param, const bool is_async, mem_borrow_result_s *result);
```

- 数据结构说明

```c++
typedef struct {
    int16_t socket_id;  // socketId
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

**描述 DESCRIPTION**

大规格虚机场景, 按节点借用内存

**参数 Parameters**

| name     | IN/OUT | description    |
|----------|--------|----------------|
| param    | IN     | 借用任务入参         |
| is_async | IN     | 是否异步执行借用任务标识符。 |
| result   | OUT    | 借用结果信息。        |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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
    // NUMA0 - socketId: 0; NUMA1 - socketId: 0; NUMA2 - socketId: 1; NUMA3 - socketId: 1
    for (int i = 0; i < param -> numa_len; ++i) {
        numa_meta_infos[i].socket_id = i < 2 ? 0 : 1;
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

### ubs_virt_agent_page_swap_enable

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

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

**描述 DESCRIPTION**

大规格虚机场景, 通过配额实现使能冷热页流动

**参数 Parameters**

| name             | IN/OUT | description |
|------------------|--------|-------------|
| pid_t            | IN     | 虚拟机进程ID。    |
| page_swap_enable | IN     | 内存换页使能参数。   |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

## 迁移

### update_page_flow_and_status

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_migrate.h>
int32_t update_page_flow_and_status(const char *opt, const char *uuid);
```

**描述 DESCRIPTION**

更新冷热页流动开关和虚拟机状态。

**参数 Parameters**

| name | IN/OUT | description |
|------|--------|-------------|
| opt  | IN     | 操作类型        |
| uuid | IN     | 虚拟机UUID     |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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
    if (ret != VA_SUCESS) {
        perror("update_page_flow_and_status failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### ubs_virt_agent_make_migrate_decision

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_ham_migrate.h>
virt_agent_ret_t ubs_virt_agent_make_migrate_decision(uint32_t vmMemoryMB, const char *uuid, const char *destHostName,
                                                      uint32_t destNumaId, uint32_t *migrateStrategy);
```

**描述 DESCRIPTION**

获取虚拟机迁移策略。

**参数 Parameters**

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| vmMemoryMB      | OUT    | 虚拟机内存大小     |
| uuid            | OUT    | 虚拟机UUID     |
| destHostName    | OUT    | 目标主机名       |
| destNumaId      | OUT    | 目标NUMA ID   |
| migrateStrategy | OUT    | 迁移决策结果      |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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
    if (ret != VA_SUCESS) {
        perror("ubs_virt_agent_make_migrate_decision failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### RackStartIpcClientWithTimeout

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_ham_migrate.h>
virt_agent_ret_t RackStartIpcClientWithTimeout(uint16_t timeout);
```

**描述 DESCRIPTION**

libvirt与virt_agent通信超时时间设置。

**参数 Parameters**

| name            | IN/OUT | description                |
|-----------------|--------|----------------------------|
| timeout      | IN     | libvirt与virt_agent超时时间，单位秒 |

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序为使用示例。

```c
#include <stdio.h>
#include <ubs_virt_agent_ham_migrate.h>

int main(void)
{
    int32_t ret;
    uint16_t timeout = 128;
    ret = RackStartIpcClientWithTimeout(timeout);
    if (ret != VA_SUCESS) {
        perror("RackStartIpcClientWithTimeout failed.\n");
        return -1;
    }
    /* Do your work here... */
    return 0;
}
```

### RackSyncSendForHam

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_ham_migrate.h>
int RackSyncSendForHam(HamComByteBuffer *request, HamComByteBuffer *response);
```

**描述 DESCRIPTION**

获取虚拟机迁移策略。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

### RackAsyncSendForHam

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_ham_migrate.h>
int RackAsyncSendForHam(HamComByteBuffer *request, HamComCallbackDef *callback);
```

**描述 DESCRIPTION**

libvirt与virt_agent异步调用接口

**参数 Parameters**

| name            | IN/OUT | description                  |
|-----------------|--------|------------------------------|
| request      | IN     | libvirt调用virt_agent传参（json）  |
| callback            | OUT    | libvirt调用virt_agent回调函数的函数指针 |

- 数据结构说明

```c
typedef void (*HamComCallbackFunc)(void *ctx, void *recv, uint32_t len, int32_t result);
typedef struct {
    HamComCallbackFunc cb; 
    void *cbCtx;         
} HamComCallbackDef;
```

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

## 节点

### ubs_virt_agent_mem_fragmentation_node_info

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info(numa_info_t **node_list, uint32_t *node_cnt);
```

**描述 DESCRIPTION**

查询本节点Numa信息。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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

### ubs_virt_agent_mem_fragmentation_vm_info

**库 LIBRARY**

virt_agent库 (libubs-virt-agent.so)

**摘要 SYNOPSIS**

```c
#include <ubs_virt_agent_mem_fragmentation.h>
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_vm_info(vm_domain_info_t **vm_info_list, uint32_t *vm_info_cnt);
```

**描述 DESCRIPTION**

查询本节点虚拟机信息。

**参数 Parameters**

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

**返回值 RETURN VALUE**

返回 `VA_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

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

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

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
