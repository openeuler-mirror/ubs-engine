# 1. ubs_virt_agent_waterline_mem_borrow

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_virt_agent_waterline_mem_borrow(mem_borrow_request_t *memBorrowRequest, char ***borrowIds,
                                            uint32_t *idsSize);
```

## 描述 DESCRIPTION

内存借用执行。

## 参数 Parameters

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
} borrow_param_t;
typedef struct {
    borrow_param_t borrowParam;
    uint64_t borrowSizes[64];
    size_t borrowSizesSize;
    watermark_t waterMark;
} mem_borrow_request_t;
```

## 返回值 RETURN VALUE

int类型：
0表示成功。
非0表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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

# 2. ubs_virt_agent_waterline_mem_migrate

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_virt_agent_waterline_mem_migrate(mem_migrate_request_t *memMigrateRequest);
```

## 描述 DESCRIPTION

内存冷热页交换执行。

## 参数 Parameters

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
    char borrowIds[SDK_NO_64][SDK_NO_128];
    size_t borrowIdsSize;
    container_param_t containerParam[SDK_NO_64];
    size_t containerParamSize;
} mem_migrate_request_t;
```

## 返回值 RETURN VALUE

int类型：
0表示成功。
非0表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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

# 3. ubs_virt_agent_waterline_mem_return

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_virt_agent_waterline_mem_return(return_request_t *returnRequest);
```

## 描述 DESCRIPTION

内存归还执行

## 参数 Parameters

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

## 返回值 RETURN VALUE

int类型：
0表示成功。
非0表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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
        .pids = {12345, 23456},
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

# 4. ubs_container_info_query

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_container_info_query(pid_param* param, pid_mem_info **pidInfos, uint32_t *InfoSize);
```

## 描述 DESCRIPTION

查询容器Pid本地和远端内存

## 参数 Parameters

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

## 返回值 RETURN VALUE

int类型：
0表示成功。
非0表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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
    pid_mem_info *pidInfos = NULL;
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

# 5. ubs_container_inject_waterLine

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

```c
#include <ubs_virt_agent_container.h>
int32_t ubs_container_inject_waterLine(watermark_t* param);
```

## 描述 DESCRIPTION

注入节点水线

## 参数 Parameters

| name            | IN/OUT | description |
|-----------------|--------|-------------|
| param      | IN     | 水线配置信息     |

## 返回值 RETURN VALUE

int类型：
0表示成功。
非0表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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

# 6. ubs_container_get_container_pids

## 库 LIBRARY

virt_agent库 (libubs-virt-agent.so)

## 摘要 SYNOPSIS

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

## 描述 DESCRIPTION

指定容器查询容器内Pid

## 参数 Parameters

| name            | IN/OUT | description  |
|-----------------|--------|--------------|
| containerIdList | IN     | 容器ID信息       |
| param           | OUT    | 容器内pid信息列表   |
| InfoSize        | OUT    | 容器内pid信息列表大小 |

## 返回值 RETURN VALUE

int类型：
0表示成功。
非0表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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
