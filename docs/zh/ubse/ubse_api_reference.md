# UBS Engine API 参考指南

## 前言

**概述**

UBS Engine（UBSE）提供了UBSE程序及其相应的SDK开发库。开发者可以利用该SDK访问UBSE提供的服务，从而实现对内存等资源的调度与管理。

UBSE SDK通过三种形式提供：

- so动态链接库：C和C++语言的开发。
- Python开发库：支持python协议栈开发。
- Go SDK源码库：支持Go协议栈通过import导入开发。

**介绍**

本文主要介绍UBS Engine SDK对外提供的API。UBS Engine SDK使用C语言开发，对外提供C语言API接口。

**约束条件**

UBS Engine对外接口的访问入口，是sock文件/run/ubse/ubse.sock，sock文件的安全访问，基于文件权限控制，其属主为ubse，mode为660，如下：

```shell
srw-rw---- ubse ubse /run/ubse/ubse.sock
```

加入ubse group的用户，才能访问UBSE的SDK接口。

## libubse client

### ubs\_engine\_client\_initialize

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine.h>
int32_t ubs_engine_client_initialize(const char *ubs_engine_uds_path);
```

**描述 DESCRIPTION**

创建ubse客户端，完成内部资源申请，记录uds\_path信息

在后续的流程中，根据传入的uds\_path创建与ubse服务端的socket连接，并将业务端请求发送给ubse服务端，并等待服务端返回结果。

**参数 Parameters**

| name                   | IN/OUT | description                                                                                                                                                  |
| ---------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| ubs\_engine\_uds\_path | IN     | ubse服务端的uds文件路径<br>传入空路径，则采用ubse默认地址/var/run/ubse/ubse.sock<br>路径长度限制：遵从linux `sun_path` 的大小是 108 字节 (`#define UNIX_PATH_MAX 108`)，所以路径不能超过 107 个字符（不含结尾的空字符 `\0`） |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                            | Description  |
| -------------------------------- | ------------ |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE | 参数数据长度超108字节 |
| UBS\_ENGINE\_ERR\_RESOURCE       | 资源创建失败       |

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine.h>

int main(void)
{
    int32_t ret;

    char *path = "/var/run/ubse/ubse.sock";
    ret = ubs_engine_client_initialize(path);
    if (UBS_SUCCESS != ret) {
        perror("init failed.\n");
        return -1;
    }
        
    /* Do your work here... */

    /* Do your work here... */

    return 0;
}
```

### ubs\_engine\_client\_finalize

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine.h>
void ubs_engine_client_finalize(void);
```

**描述 DESCRIPTION**

销毁ubse客户端，完成内部资源释放

**参数 PARAMETERS**

无

**返回值 RETURN VALUE**

无

**错误 ERRORS**

无

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序初始化UBSE客户端，并基于客户端完成后续操作，并销毁客户端，释放资源。

```c
#include <stdio.h>
#include <ubs_engine.h>

int main(void)
{
    int32_t ret;

    char *path = "/var/run/ubse/ubse.sock";
    ret = ubs_engine_client_initialize(path);
    if (UBS_SUCCESS != ret) {
        perror("init failed.\n");
        return -1;
    }
        
    /* Do your work here... */

    ubs_engine_client_finalize();

    return 0;
}
```

## libubse log

### ubs_engine_log_callback_register

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_log.h>
void ubs_engine_log_callback_register(ubs_engine_log_handler handler);
```

**描述 DESCRIPTION**

注册日志回调函数。

如果没有注册日志函数，或者传入空指针，则采用标准输出打印日志；如果注册了回调函数，则后续日志通过回调函数返回给调用方处理。

**参数 PARAMETERS**

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

**返回值 RETURN VALUE**

无。

**错误 ERRORS**

无。

**约束 CONSTRAINTS**

传入 `NULL` 时，日志输出回退到标准输出。

回调函数由调用方提供，调用方需要自行保证回调函数内部的线程安全和资源管理安全。

**附注 NOTES**

该接口只负责注册日志处理函数，不负责日志级别过滤。

**样例 EXAMPLES**

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

## libubse mem

### ubs\_mem\_numastat\_get

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t **numa_mems, uint32_t *numa_mem_cnt);
```

**描述 DESCRIPTION**

查询指定节点numa信息，仅返回可用节点的numa信息，当前只支持本地numa内存，后续会增加远端numa。

**参数 PARAMETERS**

| name           | IN/OUT | description                            |
| -------------- | ------ | -------------------------------------- |
| slot\_id       | IN     | 节点标识                                   |
| numa\_mems     | OUT    | 节点numa信息数组，调用方需要使用 `free` 接口主动释放内存     |
| numa\_mem\_cnt | OUT    | 节点numa信息个数，范围 `[0, UBS_TOPO_NUMA_NUM]` |

- 数据结构说明

```c
typedef enum {
    NUMA_LOCAL, // 本地numa
    NUMA_REMOTE // 远端numa, 当前不支持
} ubs_mem_numa_type_t;

typedef struct {
    uint32_t slot_id;              // 节点唯一标识, 采用slotid, 与UBM保持一致
    uint32_t socket_id;            // socket id
    uint32_t numa_id;              // 节点中的numa id
    ubs_mem_numa_type_t numa_type; // numa类型
    uint32_t mem_lend_ratio;       // 池化内存借出比例上限
    uint64_t mem_total;            // 内存总量, 单位字节
    uint64_t mem_free;             // 内存空闲量, 单位字节
    uint32_t huge_pages_2M;        // 2M大页数量
    uint32_t free_huge_pages_2M;   // 2M大页空闲数量
    uint32_t huge_pages_1G;        // 1G大页数量
    uint32_t free_huge_pages_1G;   // 1G大页空闲数量
    uint64_t mem_borrow;           // 借用的内存，单位字节
    uint64_t mem_lend;             // 借出的内存，单位字节
} ubs_mem_numastat_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |
| UBS\_ENGINE\_ERR\_NODE\_NOT\_EXIST   | 查询节点不存在      |
| UBS\_ENGINE\_ERR\_NODE\_FAULT        | 查询节点故障       |

**约束 CONSTRAINTS**

当前只支持本地numa内存

**附注 NOTES**

暂无。

**样例 EXAMPLES**

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    uint32_t node_id = 1;
    uint32_t numa_mem_cnt = 0;
    ubs_mem_numastat_t *numa_mem = NULL;

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numastat_get(node_id, &numa_mem, &numa_mem_cnt);
    if (ret != UBS_SUCCESS) {
        perror("get numastat failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    free(numa_mem);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_create

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_create(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                          ubs_mem_distance_t distance, ubs_mem_fd_desc_t *fd_desc);
```

**描述 DESCRIPTION**

在本节点创建fd形态的远端内存。

**参数 PARAMETERS**

| name     | IN/OUT | description                                                                 |
| -------- | ------ | --------------------------------------------------------------------------- |
| name     | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |
| size     | IN     | 借用大小，单位Byte，取值范围大于等于 `4 * 1024 * 1024`                                      |
| owner    | IN     | 内存资源使用者属主信息，可选参数，`NULL` 不关注该字段                                              |
| mode     | IN     | 内存资源使用者访问权限，可选参数，`0` 不关注该字段                                                 |
| distance | IN     | 内存访问距离                                                                      |
| fd\_desc | OUT    | 内存描述信息                                                                      |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的运行用户的groupid
    pid_t pid; // 属主进程的运行用户的pid, 指定pid时, pid消失后自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;

typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应超过1跳节点, 暂不支持
} ubs_mem_distance_t;

typedef struct {
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;   // socket id
    uint32_t numa_id;     // 节点中的numa id
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;

typedef enum {
    UBSE_NOT_EXIST = 0,         // 借用关系不存在
    UBSE_CREATING = 1,          // 正在创建中
    UBSE_DELETING = 2,          // 正在删除中
    UBSE_EXIST = 3,             // 创建成功
    UBSE_ERR_ONLY_IMPORT = 4,   // 只存在借入
    UBSE_ERR_WAIT_UNEXPORT = 5, // 等待unexport执行，对账会执行，可以手动删除
    UBSE_END = 6                // 类型转换边界值, 不表示任何内存状态
} ubs_mem_stage;

#define UBS_MEM_MAX_NAME_LENGTH 48
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，对应设备路径格式为 /dev/obmm_shmdev<memid>
    uint64_t mem_size;                      // 借用大小
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点, 其中ips字段无效, 需通过topo接口获取
    ubs_topo_node_t import_node;            // 借入节点, 其中ips字段无效, 需通过topo接口获取
    ubs_mem_stage mem_stage;                // 内存状态
} ubs_mem_fd_desc_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description      |
| ------------------------------------ | ---------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针              |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name或者size参数超出范围 |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败      |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过     |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在          |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时      |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误      |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    mode_t mode = 0x700;
    const char *name = "test_mem";
    uint64_t size = 1024ULL * 1024ULL * 1024ULL;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_fd_desc_t fd_desc;

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_create(name, size, &owner, mode, dist, &fd_desc);
    if (ret != UBS_SUCCESS) {
        perror("create fd mem failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_create\_with\_lender

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_create_with_lender(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                      const ubs_mem_lender_t *lender, uint32_t lender_cnt, ubs_mem_fd_desc_t *fd_desc);
```

**描述 DESCRIPTION**

指定借出信息，在本节点创建fd形态的远端内存。

**参数 PARAMETERS**

| name        | IN/OUT | description                                                                 |
| ----------- | ------ | --------------------------------------------------------------------------- |
| name        | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |
| owner       | IN     | 内存资源使用者属主信息，可选参数，`NULL` 不关注该字段                                              |
| mode        | IN     | 内存资源使用者访问权限，可选参数，`0` 不关注该字段                                                 |
| lender      | IN     | 借出信息                                                                        |
| lender\_cnt | IN     | 借出信息个数，最大为 `UBS_MEM_MAX_LENDER_CNT`                                         |
| fd\_desc    | OUT    | 内存描述信息                                                                      |

- 数据结构说明

```c
typedef struct {
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;   // socket id
    uint32_t numa_id;     // 节点中的numa id
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    mode_t mode = 0x700;
    const char *name = "test_mem";
    ubs_mem_fd_desc_t fd_desc;
    ubs_mem_lender_t lender = {
        .lender_size = 128ULL * 1024ULL * 1024ULL,
        .slot_id = 2,
        .socket_id = UINT32_MAX,
        .numa_id = 1,
        .port_id = UINT32_MAX
    };

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_create_with_lender(name, &owner, mode, &lender, 1, &fd_desc);
    if (ret != UBS_SUCCESS) {
        perror("create fd mem with lender failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_create\_with\_candidate

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_create_with_candidate(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                         const uint32_t *slot_ids, uint32_t slot_cnt, ubs_mem_fd_desc_t *fd_desc);
```

**描述 DESCRIPTION**

指定候选借出节点范围，在本节点创建fd形态的远端内存。

**参数 PARAMETERS**

| name      | IN/OUT | description                                                                 |
| --------- | ------ | --------------------------------------------------------------------------- |
| name      | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |
| size      | IN     | 借用大小，单位Byte，取值范围大于等于 `4 * 1024 * 1024`                                      |
| owner     | IN     | 内存资源使用者属主信息，可选参数，`NULL` 不关注该字段                                              |
| mode      | IN     | 内存资源使用者访问权限，可选参数，`0` 不关注该字段                                                 |
| slot\_ids | IN     | 候选借出节点范围，最大为 `UBS_MEM_MAX_SLOT_NUM`                                         |
| slot\_cnt | IN     | 候选借出节点个数                                                                    |
| fd\_desc  | OUT    | 内存描述信息                                                                      |

- 数据结构说明

```c
typedef struct {
    uint32_t slot_id;                                      // 节点唯一标识
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];               // socket id 列表
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM]; // socket 下的 numa id 列表
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];        // IP 地址列表
    char host_name[HOST_NAME_MAX];                         // 主机名
} ubs_topo_node_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    mode_t mode = 0x700;
    const char *name = "test_mem";
    uint64_t size = 1024ULL * 1024ULL * 1024ULL;
    uint32_t slot_ids[2] = {1, 2};
    ubs_mem_fd_desc_t fd_desc;

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_create_with_candidate(name, size, &owner, mode, slot_ids, 2, &fd_desc);
    if (ret != UBS_SUCCESS) {
        perror("create fd mem with candidate failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_permission

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_permission(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode);
```

**描述 DESCRIPTION**

改变本节点fd形态远端内存的permission信息。

- 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同
- 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

**参数 PARAMETERS**

| name  | IN/OUT | description                                                                 |
| ----- | ------ | --------------------------------------------------------------------------- |
| name  | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |
| owner | IN     | 内存资源使用者属主信息，必选参数，不允许为 `NULL`                                                |
| mode  | IN     | 内存资源使用者访问权限，必选参数，不允许为 `0`                                                   |

- 数据结构说明

```c
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的运行用户的groupid
    pid_t pid; // 属主进程的运行用户的pid
} ubs_mem_fd_owner_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_permission("test_mem", &owner, 0x700);
    if (ret != UBS_SUCCESS) {
        perror("set permission failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_get

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_get(const char *name, ubs_mem_fd_desc_t *fd_desc);
```

**描述 DESCRIPTION**

查询本节点fd形态远端内存信息。

**参数 PARAMETERS**

| name     | IN/OUT | description                                                                 |
| -------- | ------ | --------------------------------------------------------------------------- |
| name     | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |
| fd\_desc | OUT    | fd内存信息                                                                      |

- 数据结构说明

```c
#define UBS_MEM_MAX_NAME_LENGTH 48
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，对应设备路径格式为 /dev/obmm_shmdev<memid>
    uint64_t mem_size;                      // 借用大小
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点, 其中ips字段无效, 需通过topo接口获取
    ubs_topo_node_t import_node;            // 借入节点, 其中ips字段无效, 需通过topo接口获取
    ubs_mem_stage mem_stage;                // 内存状态
} ubs_mem_fd_desc_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_fd_desc_t fd_desc;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_get("test_mem", &fd_desc);
    if (ret != UBS_SUCCESS) {
        perror("get fd mem failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_list

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_list(ubs_mem_fd_desc_t **fd_descs, uint32_t *fd_desc_cnt);
```

**描述 DESCRIPTION**

查询本节点所有fd形态远端内存。

**参数 PARAMETERS**

| name          | IN/OUT | description                                      |
| ------------- | ------ | ------------------------------------------------ |
| fd\_descs     | OUT    | fd内存描述信息数组，调用成功后需要使用 `free` 接口主动释放内存             |
| fd\_desc\_cnt | OUT    | fd内存描述信息数组中的元素个数，范围 `[0, UBS_MEM_MAX_DESC_LIST]` |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | 参数超出范围       |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_fd_desc_t *fd_descs = NULL;
    uint32_t fd_desc_cnt = 0;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_list(&fd_descs, &fd_desc_cnt);
    if (ret != UBS_SUCCESS) {
        perror("list fd mem failed");
        ubs_engine_client_finalize();
        return -1;
    }

    free(fd_descs);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_delete

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_delete(const char *name);
```

**描述 DESCRIPTION**

删除本节点指定fd远端内存。

**参数 PARAMETERS**

| name | IN/OUT | description                                                                 |
| ---- | ------ | --------------------------------------------------------------------------- |
| name | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description                                   |
| ------------------------------------ | --------------------------------------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针                                           |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围                                    |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败                                   |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过                                  |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在                                       |
| UBS\_ENGINE\_ERR\_IMPORT\_ABSENT     | IMPORT不在位, 无法删除                               |
| UBS\_ENGINE\_ERR\_CREATING           | 正在创建过程中                                       |
| UBS\_ENGINE\_ERR\_DELETING           | 正在删除过程中                                       |
| UBS\_ENGINE\_ERR\_UNIMPORT\_SUCCESS  | UNIMPORT已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时                                   |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误                                   |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能删除资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

对于异常场景如username、uid改变或者用户被删除，ubse提供cli命令删除内存能力,具体请参考《ubsectl\_mem.md》文档。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_delete("test_mem");
    if (ret != UBS_SUCCESS) {
        perror("delete fd mem failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_create

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_create(const char *name, uint64_t size, ubs_mem_distance_t distance,
                            ubs_mem_numa_desc_t *numa_desc);
```

**描述 DESCRIPTION**

在本节点创建numa形态的远端内存。

**参数 PARAMETERS**

| name       | IN/OUT | description                                                                 |
| ---------- | ------ | --------------------------------------------------------------------------- |
| name       | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name在节点内保持唯一性 |
| size       | IN     | 借用大小，单位Byte，取值范围大于等于 `4 * 1024 * 1024`                                      |
| distance   | IN     | 内存访问跳数                                                                      |
| numa\_desc | OUT    | 借用形成的远端numa信息                                                               |

- 数据结构说明

```c
#define UBS_MEM_MAX_NAME_LENGTH 48
#define UBSE_MAX_USR_INFO_LENGTH 32

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];        // 借用标识
    int64_t numaid;                            // 形成远端numa对应的numaid
    ubs_topo_node_t export_node;               // 借出节点, 其中ips字段无效, 需通过topo接口获取
    ubs_topo_node_t import_node;               // 借入节点, 其中ips字段无效, 需通过topo接口获取
    uint64_t size;                             // 借用大小
    ubs_mem_stage mem_stage;                   // 内存状态
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LENGTH]; // 调用方私有数据，UBSE只负责保存，get时原样返回
} ubs_mem_numa_desc_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description      |
| ------------------------------------ | ---------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针              |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name或者size参数超出范围 |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败      |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过     |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在          |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时      |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误      |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_numa_desc_t numa_desc;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_create("test_numa", 128ULL * 1024ULL * 1024ULL, MEM_DISTANCE_L0, &numa_desc);
    if (ret != UBS_SUCCESS) {
        perror("create numa mem failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_create\_with\_lender

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_create_with_lender(const char *name, const ubs_mem_lender_t *lender, uint32_t lender_cnt,
                                        ubs_mem_numa_desc_t *numa_desc);
```

**描述 DESCRIPTION**

指定借出信息，在本节点创建numa形态的远端内存。

**参数 PARAMETERS**

| name        | IN/OUT | description                         |
| ----------- | ------ | ----------------------------------- |
| name        | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |
| lender      | IN     | 借出信息                                |
| lender\_cnt | IN     | 借出信息个数，最大为 `UBS_MEM_MAX_LENDER_CNT` |
| numa\_desc  | OUT    | 借用形成的远端numa信息                       |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdint.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_numa_desc_t numa_desc;
    ubs_mem_lender_t lender = {
        .lender_size = 128ULL * 1024ULL * 1024ULL,
        .slot_id = 2,
        .socket_id = UINT32_MAX,
        .numa_id = 1,
        .port_id = UINT32_MAX
    };

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_create_with_lender("test_numa", &lender, 1, &numa_desc);
    if (ret != UBS_SUCCESS) {
        perror("create numa with lender failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_create\_with\_candidate

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_create_with_candidate(const char *name, uint64_t size, const uint32_t *slot_ids, uint32_t slot_cnt,
                                           ubs_mem_numa_desc_t *numa_desc);
```

**描述 DESCRIPTION**

指定候选借出节点，在本节点创建numa形态的远端内存。

**参数 PARAMETERS**

| name       | IN/OUT | description                            |
| ---------- | ------ | -------------------------------------- |
| name       | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| size       | IN     | 借用大小，单位Byte，取值范围大于等于 `4 * 1024 * 1024` |
| slot\_ids  | IN     | 候选借出节点范围                               |
| slot\_cnt  | IN     | 候选借出节点个数，最大为 `UBS_MEM_MAX_SLOT_NUM`    |
| numa\_desc | OUT    | 借用形成的远端numa信息                          |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

候选节点与当前借入节点，必须满足直连要求

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    uint32_t slot_ids[2] = {1, 2};
    ubs_mem_numa_desc_t numa_desc;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_create_with_candidate("test_numa", 128ULL * 1024ULL * 1024ULL, slot_ids, 2, &numa_desc);
    if (ret != UBS_SUCCESS) {
        perror("create numa with candidate failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_get

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_get(const char *name, ubs_mem_numa_desc_t *numa_desc);
```

**描述 DESCRIPTION**

查询本节点numa形态远端内存信息。

**参数 PARAMETERS**

| name       | IN/OUT | description   |
| ---------- | ------ | ------------- |
| name       | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |
| numa\_desc | OUT    | 借用形成的远端numa信息 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_numa_desc_t numa_desc;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_get("test_numa", &numa_desc);
    if (ret != UBS_SUCCESS) {
        perror("get numa failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_list

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_list(ubs_mem_numa_desc_t **numa_descs, uint32_t *numa_desc_cnt);
```

**描述 DESCRIPTION**

查询本节点所有numa形态远端内存。

**参数 PARAMETERS**

| name            | IN/OUT | description                            |
| --------------- | ------ | -------------------------------------- |
| numa\_descs     | OUT    | numa内存描述信息数组，调用成功后需要使用 `free` 接口主动释放内存 |
| numa\_desc\_cnt | OUT    | numa内存描述信息数组中的元素个数                     |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | 参数超出范围       |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_numa_desc_t *numa_descs = NULL;
    uint32_t numa_desc_cnt = 0;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_list(&numa_descs, &numa_desc_cnt);
    if (ret != UBS_SUCCESS) {
        perror("list numa mem failed");
        ubs_engine_client_finalize();
        return -1;
    }

    free(numa_descs);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_delete

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_delete(const char *name);
```

**描述 DESCRIPTION**

删除本节点指定numa远端内存。

**参数 PARAMETERS**

| name | IN/OUT | description |
| ---- | ------ | ----------- |
| name | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description                                   |
| ------------------------------------ | --------------------------------------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针                                           |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围                                    |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败                                   |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过                                  |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在                                       |
| UBS\_ENGINE\_ERR\_IMPORT\_ABSENT     | IMPORT不在位, 无法删除                               |
| UBS\_ENGINE\_ERR\_CREATING           | 正在创建过程中                                       |
| UBS\_ENGINE\_ERR\_DELETING           | 正在删除过程中                                       |
| UBS\_ENGINE\_ERR\_UNIMPORT\_SUCCESS  | UNIMPORT已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时                                   |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误                                   |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能删除资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

对于异常场景如username、uid改变或者用户被删除，ubse提供cli命令删除内存能力,具体请参考《ubsectl\_mem.md》文档。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_delete("test_numa");
    if (ret != UBS_SUCCESS) {
        perror("delete numa failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_create

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_create(const char *name, uint64_t size, uint8_t usr_info[32], uint64_t flag,
                           const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider);
```

**描述 DESCRIPTION**

创建共享形态的远端内存。

**参数 PARAMETERS**

| name      | IN/OUT | description                                                                                     |
| --------- | ------ | ----------------------------------------------------------------------------------------------- |
| name      | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name全局保持唯一性                       |
| size      | IN     | 借用大小，单位Byte，取值范围大于等于 `4 * 1024 * 1024`                                                          |
| usr\_info | IN     | 调用方私有数据，UBSE只负责保存，get时原样返回                                                                      |
| flag      | IN     | 额外的内存借用属性，目前支持写接力、自动清理提供方和设置共享内存属性为CacheCoherent（按位组合，每一个二进制位表示一种独立属性）；<br /> **可用标志位定义如下**：<br /> `0x1`: 非写接力 <br />`0x2`: 匿名内存，共享内存没有使用方时，后台对账会自动清理 <br /> `0x4`: 设置共享内存属性为CacheCoherent (默认为NonCacheCoherent)  <br /> **flag使用说明(flag为十进制数)**:  <br /> flag 可以用 `\|` 运算进行赋值,表示开启某个属性，比如：<br /> - 非写接力 + 匿名:`flag= 0x1 \| 0x2 = 3`; <br /> - 匿名+设置共享内存属性为CacheCoherent:`flag = 0x2 \| 0x4 = 6`  <br /> - 非写接力+匿名+设置共享内存属性为CacheCoherent:`flag = 0x1 \| 0x2 \| 0x4 = 7` <br />- 其它属性组合, 使用 `flag \|= 对应标志位` 进行组合即可<br />  **flag其它取值说明**: <br /> 0：默认值，代表三个标志位对应的属性都不选择 |
| region    | IN     | 后续使用共享内存的节点范围，可选参数；`NULL` 表示取集群中全量节点                                                                              |
| provider  | IN     | 资源提供方节点范围，`NULL` 表示不指定                                                                          |

- 数据结构说明

```c
typedef struct {
    uint32_t node_cnt;                        // 实际有效的节点数量
    uint32_t slot_ids[UBS_MEM_MAX_SLOT_NUM];  // 节点ID数组
} ubs_mem_nodes_t;

#define UBS_MEM_MAX_USR_INFO_LEN 32
#define UBS_MEM_FLAG_NO_WR_DELAY 0x1   // 非写接力
#define UBS_MEM_FLAG_SHM_ANONYMOUS 0x2 // 共享内存没有使用方时, 后台对账时自动清理提供方
#define UBS_MEM_FLAG_CACHEABLE 0x4     // 设置cacheableFlag为1
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description      |
| ------------------------------------ | ---------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针              |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name或者size参数超出范围 |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败      |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过     |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在          |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时      |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误      |

**约束 CONSTRAINTS**

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

借出节点的决策原则：指定provider，则从中选择；没有指定provider，则从region中选择；确保所有节点均能使用共享内存

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <string.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    ubs_mem_nodes_t region = {
        .node_cnt = 2,
        .slot_ids = {1, 2}
    };

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_create("demo_shm", 4ULL * 1024ULL * 1024ULL, usr_info, 0, &region, NULL);
    if (ret != UBS_SUCCESS) {
        perror("create shm failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_create\_with\_affinity

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_create_with_affinity(const char *name, uint64_t size, uint32_t affinity_socket_id,
                                         uint8_t usr_info[32], uint64_t flag, const ubs_mem_nodes_t *region,
                                         const ubs_mem_nodes_t *provider);
```

**描述 DESCRIPTION**

创建指定CPU平面的共享形态远端内存。

**参数 PARAMETERS**

| name                 | IN/OUT | description                            |
| -------------------- | ------ | -------------------------------------- |
| name                 | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性 |
| size                 | IN     | 借用大小，单位Byte，取值范围大于等于 `4 * 1024 * 1024` |
| affinity\_socket\_id | IN     | 亲和的cpu socket\_id                      |
| usr\_info            | IN     | 调用方私有数据，UBSE只负责保存，get时原样返回             |
| flag                 | IN     | 额外的内存借用属性，目前支持写接力、自动清理提供方和设置共享内存属性为CacheCoherent（按位组合，每一个二进制位表示一种独立属性）；<br /> **可用标志位定义如下**：<br /> `0x1`: 非写接力 <br />`0x2`: 匿名内存，共享内存没有使用方时，后台对账会自动清理 <br /> `0x4`: 设置共享内存属性为CacheCoherent (默认为NonCacheCoherent)  <br /> **flag使用说明(flag为十进制数)**:  <br /> flag 可以用 `\|` 运算进行赋值,表示开启某个属性，比如：<br /> - 非写接力 + 匿名:`flag= 0x1 \| 0x2 = 3`; <br /> - 匿名+设置共享内存属性为CacheCoherent:`flag = 0x2 \| 0x4 = 6`  <br /> - 非写接力+匿名+设置共享内存属性为CacheCoherent:`flag = 0x1 \| 0x2 \| 0x4 = 7` <br />- 其它属性组合, 使用 `flag \|= 对应标志位` 进行组合即可<br />  **flag其它取值说明**: <br /> 0：默认值，代表三个标志位对应的属性都不选择 |
| region               | IN     | 使用共享内存的节点范围，可选参数；`NULL` 表示取集群中全量节点                       |
| provider             | IN     | 资源提供方节点范围，`NULL` 表示不指定                 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description      |
| ------------------------------------ | ---------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针              |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name或者size参数超出范围 |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败      |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过     |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在          |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时      |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误      |

**附注 NOTES**

`affinity_socket_id` 需要是有效的 CPU socket 标识。

`region` 内的节点必须能够通过公共节点直连。

算法决策借出节点；指定资源提供方时从中选择，没有指定时从 `region` 中选择；确保所有节点均能使用共享内存。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    ubs_mem_nodes_t region = {
        .node_cnt = 2,
        .slot_ids = {1, 2}
    };

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_create_with_affinity("demo_shm", 4ULL * 1024ULL * 1024ULL, 0, usr_info, 0, &region, NULL);
    if (ret != UBS_SUCCESS) {
        perror("create shm with affinity failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_create\_with\_lender

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_create_with_lender(const char *name, uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                       const ubs_mem_nodes_t *region, const ubs_mem_lender_t *lender);
```

**描述 DESCRIPTION**

指定借出方创建共享形态的远端内存。

**参数 PARAMETERS**

| name      | IN/OUT | description                |
| --------- | ------ | -------------------------- |
| name      | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性 |
| usr\_info | IN     | 调用方私有数据，UBSE只负责保存，get时原样返回 |
| flag      | IN     | 额外的内存借用属性，目前支持写接力、自动清理提供方和设置共享内存属性为CacheCoherent（按位组合，每一个二进制位表示一种独立属性）；<br /> **可用标志位定义如下**：<br /> `0x1`: 非写接力 <br />`0x2`: 匿名内存，共享内存没有使用方时，后台对账会自动清理 <br /> `0x4`: 设置共享内存属性为CacheCoherent (默认为NonCacheCoherent)  <br /> **flag使用说明(flag为十进制数)**:  <br /> flag 可以用 `\|` 运算进行赋值,表示开启某个属性，比如：<br /> - 非写接力 + 匿名:`flag= 0x1 \| 0x2 = 3`; <br /> - 匿名+设置共享内存属性为CacheCoherent:`flag = 0x2 \| 0x4 = 6`  <br /> - 非写接力+匿名+设置共享内存属性为CacheCoherent:`flag = 0x1 \| 0x2 \| 0x4 = 7` <br />- 其它属性组合, 使用 `flag \|= 对应标志位` 进行组合即可<br />  **flag其它取值说明**: <br /> 0：默认值，代表三个标志位对应的属性都不选择 |
| region    | IN     | 使用共享内存的节点范围，可选参数；`NULL` 表示取集群中全量节点           |
| lender    | IN     | 指定借出节点数据                   |

- 数据结构说明

```c
#define UBS_MEM_MAX_SLOT_NUM 16

typedef struct {
    uint32_t node_cnt;                        // 实际有效的节点数量
    uint32_t slot_ids[UBS_MEM_MAX_SLOT_NUM];  // 节点ID数组
} ubs_mem_nodes_t;

typedef struct {
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;   // socket id
    uint32_t numa_id;     // 节点中的numa id
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | 参数超出范围       |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_EXISTED            | 借用关系已存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**附注 NOTES**

`region` 内的节点必须能够通过公共节点直连。

借出节点由 `lender` 明确指定，不再从 `region` 自动选择；`lender` 参数需要满足当前硬件拓扑合法性。

`ubs_mem_lender_t` 结构体内部字段支持以下赋值，推荐无效值时填 `UINT32_MAX`：

| lender\_size(必传) | slot\_id(必传) | socket\_id(可选) | numa\_id(可选) | port\_id(可选) | 预期结果                |
| ---------------- | ------------ | -------------- | ------------ | ------------ | ------------------- |
| √                | √            | <br />         | <br />       | <br />       | 根据slot\_id决策        |
| √                | √            | √              | <br />       | <br />       | 根据socket\_id决策      |
| √                | √            | <br />         | √            | <br />       | 根据numa\_id决策        |
| √                | √            | <br />         | <br />       | √            | 不支持该格式              |
| √                | √            | √              | √            | <br />       | 校验合法性, 根据numa\_id决策 |
| √                | √            | <br />         | √            | √            | 根据numa\_id决策        |
| √                | √            | √              | <br />       | √            | 根据socket\_id决策      |
| √                | √            | √              | √            | √            | 校验合法性, 根据numa\_id决策 |

合法性校验：对应关系是否符合硬件环境，校验失败则借用失败。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdint.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    ubs_mem_nodes_t region = {
        .node_cnt = 2,
        .slot_ids = {1, 2}
    };
    ubs_mem_lender_t lender = {
        .slot_id = 2,
        .numa_id = 1,
        .socket_id = UINT32_MAX,
        .port_id = UINT32_MAX,
        .lender_size = 128ULL * 1024ULL * 1024ULL
    };

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_create_with_lender("demo_shm", usr_info, 0, &region, &lender);
    if (ret != UBS_SUCCESS) {
        perror("create shm with lender failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_attach

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_attach(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                           ubs_mem_shm_desc_t **shm_desc);
```

**描述 DESCRIPTION**

导入共享形态的远端内存。

**参数 PARAMETERS**

| name      | IN/OUT | description                                                               |
| --------- | ------ | ------------------------------------------------------------------------- |
| name      | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name全局保持唯一性 |
| owner     | IN     | 内存资源属主信息，可选参数，`NULL` 不关注该字段                                               |
| mode      | IN     | 内存资源访问权限，可选参数，`0` 不关注该字段                                                  |
| shm\_desc | OUT    | 内存描述信息，调用成功后需要使用 `free` 接口主动释放内存                                          |

- 数据结构说明

```c
#define UBS_MEM_MAX_MEMID_NUM 2048
#define UBS_MEM_MAX_NAME_LENGTH 48
#define UBS_MEM_MAX_USR_INFO_LEN 32

typedef struct {
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，对应设备路径格式为 /dev/obmm_shmdev<memid>
    ubs_topo_node_t import_node;            // 借入节点, 其中ips字段无效, 需通过topo接口获取
    ubs_mem_stage mem_stage;                // 内存状态
} ubs_mem_shm_import_desc_t;

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint64_t mem_size;                      // 借用大小
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点, 其中ips字段无效, 需通过topo接口获取
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN]; // 调用方私有数据
    uint32_t import_desc_cnt;               // 导入内存描述符信息的数量
    ubs_mem_stage mem_stage;                // 内存状态
    ubs_mem_shm_import_desc_t *import_desc; // 导入内存描述符信息
} ubs_mem_shm_desc_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_SHM\_NO\_CREATE    | 共享内存未创建      |
| UBS\_ENGINE\_ERR\_CREATING           | 正在创建过程中      |
| UBS\_ENGINE\_ERR\_DELETING           | 正在删除过程中      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能导入资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    ubs_mem_shm_desc_t *shm_desc = NULL;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_attach("demo_shm", &owner, 0666, &shm_desc);
    if (ret != UBS_SUCCESS) {
        perror("attach shm failed");
        ubs_engine_client_finalize();
        return -1;
    }

    free(shm_desc);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_get

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_get(const char *name, ubs_mem_shm_desc_t **shm_desc);
```

**描述 DESCRIPTION**

查询指定共享形态远端内存。

**参数 PARAMETERS**

| name      | IN/OUT | description                                                                 |
| --------- | ------ | --------------------------------------------------------------------------- |
| name      | IN     | 借用标识，name最大长度48字节，含结尾字符 `\0`name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`name全局保持唯一性 |
| shm\_desc | OUT    | 借用形成的远端共享内存信息，调用成功后需要使用 `free` 接口主动释放内存                                     |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_shm_desc_t *shm_desc = NULL;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_get("demo_shm", &shm_desc);
    if (ret != UBS_SUCCESS) {
        perror("get shm failed");
        ubs_engine_client_finalize();
        return -1;
    }

    free(shm_desc);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_list

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_list(ubs_mem_shm_desc_t **shm_descs, uint32_t *shm_desc_cnt);
```

**描述 DESCRIPTION**

查询共享形态远端内存列表。

**参数 PARAMETERS**

| name           | IN/OUT | description                                      |
| -------------- | ------ | ------------------------------------------------ |
| shm\_descs     | OUT    | 共享内存描述信息数组，调用成功后需要使用 `free` 接口主动释放内存             |
| shm\_desc\_cnt | OUT    | 共享内存描述信息数组中的元素个数，范围 `[0, UBS_MEM_MAX_DESC_LIST]` |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | 参数超出范围       |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_shm_desc_t *shm_descs = NULL;
    uint32_t shm_desc_cnt = 0;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_list(&shm_descs, &shm_desc_cnt);
    if (ret != UBS_SUCCESS) {
        perror("list shm failed");
        ubs_engine_client_finalize();
        return -1;
    }

    free(shm_descs);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_list\_with\_prefix

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_list_with_prefix(const char *name_prefix, ubs_mem_shm_desc_t **shm_descs, uint32_t *shm_desc_cnt);
```

**描述 DESCRIPTION**

查询指定借用标识前缀的共享形态远端内存，最多查询到2000条借用内存。

**参数 PARAMETERS**

| name           | IN/OUT | description                                      |
| -------------- | ------ | ------------------------------------------------ |
| name\_prefix   | IN     | 指定借用标识前缀，最大长度48字节，含结尾字符 `\0`<br />仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`|
| shm\_descs     | OUT    | 共享内存描述信息数组，调用成功后需要使用 `free` 接口主动释放内存             |
| shm\_desc\_cnt | OUT    | 共享内存描述信息数组中的元素个数，范围 `[0, UBS_MEM_MAX_DESC_LIST]` |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description        |
| ------------------------------------ | ------------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针                |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name\_prefix参数超出范围 |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败        |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过       |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在            |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时        |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误        |

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_shm_desc_t *shm_descs = NULL;
    uint32_t shm_desc_cnt = 0;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_list_with_prefix("demo", &shm_descs, &shm_desc_cnt);
    if (ret != UBS_SUCCESS) {
        perror("list shm with prefix failed");
        ubs_engine_client_finalize();
        return -1;
    }

    free(shm_descs);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_detach

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_detach(const char *name);
```

**描述 DESCRIPTION**

删除导入共享形态的远端内存。

**参数 PARAMETERS**

| name | IN/OUT | description                                                               |
| ---- | ------ | ------------------------------------------------------------------------- |
| name | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name全局保持唯一性 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description   |
| ------------------------------------ | ------------- |
| UBS\_ERR\_NULL\_POINTER              | 空指针           |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围    |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败   |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过  |
| UBS\_ENGINE\_ERR\_SHM\_NO\_ATTACH    | 共享内存未导入       |
| UBS\_ENGINE\_ERR\_SHM\_ATTACHING     | 正在导入共享内存过程中   |
| UBS\_ENGINE\_ERR\_SHM\_DETACHING     | 正在删除导入共享内存过程中 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时   |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误   |

**约束 CONSTRAINTS**

当前只支持CPU节点

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_detach("demo_shm");
    if (ret != UBS_SUCCESS) {
        perror("detach shm failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_delete

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_delete(const char *name);
```

**描述 DESCRIPTION**

删除指定共享形态远端内存。该接口只设置删除标记，等到所有 attach 都解除之后才删除。

**参数 PARAMETERS**

| name | IN/OUT | description                                                               |
| ---- | ------ | ------------------------------------------------------------------------- |
| name | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name全局保持唯一性 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_CREATING           | 正在创建过程中      |
| UBS\_ENGINE\_ERR\_DELETING           | 正在删除过程中      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

当前只支持CPU节点

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

对于异常场景如username、uid改变或者用户被删除，ubse提供cli命令删除内存能力,具体请参考《ubsectl\_mem.md》文档。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_delete("demo_shm");
    if (ret != UBS_SUCCESS) {
        perror("delete shm failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_fault\_get

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_fault_get(const char *name, ubs_mem_memids_fault_t *fault);
```

**描述 DESCRIPTION**

查询指定共享远端内存的状态。

**参数 PARAMETERS**

| name  | IN/OUT | description                                                               |
| ----- | ------ | ------------------------------------------------------------------------- |
| name  | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_`<br />name全局保持唯一性 |
| fault | OUT    | 内存块的健康状态                                                                  |

- 数据结构说明

```c
typedef enum {
    UB_MEM_ATOMIC_DATA_ERR = 0,
    UB_MEM_READ_DATA_ERR,
    UB_MEM_FLOW_POISON,
    UB_MEM_FLOW_READ_AUTH_POISON,
    UB_MEM_FLOW_READ_AUTH_RESPERR,
    UB_MEM_TIMEOUT_POISON,
    UB_MEM_TIMEOUT_RESPERR,
    UB_MEM_READ_DATA_POISON,
    UB_MEM_READ_DATA_RESPERR,
    MAR_NOPORT_VLD_INT_ERR, // 无物理地址
    MAR_FLUX_INT_ERR,
    MAR_WITHOUT_CXT_ERR,
    RSP_BKPRE_OVER_TIMEOUT_ERR, // 无物理地址
    MAR_NEAR_AUTH_FAIL_ERR,
    MAR_FAR_AUTH_FAIL_ERR,
    MAR_TIMEOUT_ERR,
    MAR_ILLEGAL_ACCESS_ERR,
    REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR,
    MEM_EXPORT_FAULT,
    UB_MEM_HEALTHY = 1000, // 无故障
} ubs_mem_fault_type_t;

#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    uint32_t memid_cnt;                                       // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM];                   // 导出内存块标识信息
    ubs_mem_fault_type_t memid_status[UBS_MEM_MAX_MEMID_NUM]; // 内存块的健康状态，与memids一一对应
} ubs_mem_memids_fault_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE     | name参数超出范围   |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_NOT\_EXIST         | 借用关系不存在      |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

int main(void)
{
    int32_t ret;
    ubs_mem_memids_fault_t fault_info;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_fault_get("demo_shm", &fault_info);
    if (ret != UBS_SUCCESS) {
        perror("get shm fault failed");
        ubs_engine_client_finalize();
        return -1;
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_shm\_fault\_register

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_fault_register(ubs_mem_shm_fault_handler handler);
```

**描述 DESCRIPTION**

客户端订阅共享内存故障事件。

**参数 PARAMETERS**

| name    | IN/OUT | description             |
| ------- | ------ | ----------------------- |
| handler | IN     | 共享内存故障事件响应处理函数，不允许为NULL |

**注册函数返回值 RETURN VALUE**

成功返回 `0`，失败返回非 `0`。

**回调函数类型**

```c
typedef int32_t (*ubs_mem_shm_fault_handler)(const char *name, uint64_t memid, ubs_mem_fault_type_t type);
```

**回调函数参数**

| name  | IN/OUT | description                          |
| ----- | ------ | ------------------------------------ |
| name  | IN     | 发生故障的共享内存借用标识                        |
| memid | IN     | 发生故障的内存块标识信息                         |
| type  | IN     | 内存块故障类型，详见 `ubs_mem_fault_type_t` 枚举 |

```c
typedef enum {
    UB_MEM_ATOMIC_DATA_ERR = 0,
    UB_MEM_READ_DATA_ERR,
    UB_MEM_FLOW_POISON,
    UB_MEM_FLOW_READ_AUTH_POISON,
    UB_MEM_FLOW_READ_AUTH_RESPERR,
    UB_MEM_TIMEOUT_POISON,
    UB_MEM_TIMEOUT_RESPERR,
    UB_MEM_READ_DATA_POISON,
    UB_MEM_READ_DATA_RESPERR,
    MAR_NOPORT_VLD_INT_ERR,
    MAR_FLUX_INT_ERR,
    MAR_WITHOUT_CXT_ERR,
    RSP_BKPRE_OVER_TIMEOUT_ERR,
    MAR_NEAR_AUTH_FAIL_ERR,
    MAR_FAR_AUTH_FAIL_ERR,
    MAR_TIMEOUT_ERR,
    MAR_ILLEGAL_ACCESS_ERR,
    REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR,
    MEM_EXPORT_FAULT,
    UB_MEM_HEALTHY = 1000,
} ubs_mem_fault_type_t;
```

**回调函数返回值 RETURN VALUE**

回调处理成功请返回`0`，失败返回非 `0`。

**附注 NOTES**

回调函数需要由调用方自行保证线程安全。

故障事件按用户级别上报。建议同一用户仅在一个进程注册；多进程注册时，各进程需自行甄别故障内存是否属于本进程借用。

服务端检测断链事件自动清理注册监听，不再提供注销接口。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int32_t shm_fault_handler(const char *name, uint64_t memid, ubs_mem_fault_type_t type)
{
    printf("receive shm fault event, name=%s, memid=%lu, type=%d\n", name, memid, type);
    return 0;
}

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_shm_fault_register(shm_fault_handler);
    if (ret != 0) {
        perror("register shm fault handler failed");
        ubs_engine_client_finalize();
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("shm fault handler registered, waiting for events... (press Ctrl+C to exit)\n");
    while (g_running) {
        pause();
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_fault\_register

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_fault_register(ubs_mem_fd_fault_handler handler);
```

**描述 DESCRIPTION**

客户端订阅Fd内存故障事件。

**参数 PARAMETERS**

| name    | IN/OUT | description              |
| ------- | ------ | ------------------------ |
| handler | IN     | fd内存故障事件响应处理函数，不允许为NULL |

**注册函数返回值 RETURN VALUE**

成功返回 `0`，失败返回非 `0`。

**回调函数类型**

```c
typedef int32_t (*ubs_mem_fd_fault_handler)(const char *name, uint64_t memid, ubs_mem_fault_type_t type);
```

**回调函数参数**

| name  | IN/OUT | description                          |
| ----- | ------ | ------------------------------------ |
| name  | IN     | 发生故障的fd内存借用标识                        |
| memid | IN     | 发生故障的内存块标识信息                         |
| type  | IN     | 内存块故障类型，详见 `ubs_mem_fault_type_t` 枚举 |

```c
typedef enum {
    UB_MEM_ATOMIC_DATA_ERR = 0,
    UB_MEM_READ_DATA_ERR,
    UB_MEM_FLOW_POISON,
    UB_MEM_FLOW_READ_AUTH_POISON,
    UB_MEM_FLOW_READ_AUTH_RESPERR,
    UB_MEM_TIMEOUT_POISON,
    UB_MEM_TIMEOUT_RESPERR,
    UB_MEM_READ_DATA_POISON,
    UB_MEM_READ_DATA_RESPERR,
    MAR_NOPORT_VLD_INT_ERR,
    MAR_FLUX_INT_ERR,
    MAR_WITHOUT_CXT_ERR,
    RSP_BKPRE_OVER_TIMEOUT_ERR,
    MAR_NEAR_AUTH_FAIL_ERR,
    MAR_FAR_AUTH_FAIL_ERR,
    MAR_TIMEOUT_ERR,
    MAR_ILLEGAL_ACCESS_ERR,
    REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR,
    MEM_EXPORT_FAULT,
    UB_MEM_HEALTHY = 1000,
} ubs_mem_fault_type_t;
```

**回调函数返回值 RETURN VALUE**

回调处理成功请返回`0`，失败返回非 `0`。

**附注 NOTES**

回调函数需要由调用方自行保证线程安全。

故障事件按用户级别上报。建议同一用户仅在一个进程注册；多进程注册时，各进程需自行甄别故障内存是否属于本进程借用。

服务端检测断链事件自动清理注册监听，不再提供注销接口。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int32_t fd_fault_handler(const char *name, uint64_t memid, ubs_mem_fault_type_t type)
{
    printf("receive fd fault event, name=%s, memid=%lu, type=%d\n", name, memid, type);
    return 0;
}

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_fd_fault_register(fd_fault_handler);
    if (ret != 0) {
        perror("register fd fault handler failed");
        ubs_engine_client_finalize();
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("fd fault handler registered, waiting for events... (press Ctrl+C to exit)\n");
    while (g_running) {
        pause();
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_numa\_fault\_register

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_fault_register(ubs_mem_numa_fault_handler handler);
```

**描述 DESCRIPTION**

客户端订阅Numa内存故障事件。

**参数 PARAMETERS**

| name    | IN/OUT | description                |
| ------- | ------ | -------------------------- |
| handler | IN     | numa内存故障事件响应处理函数，不允许为NULL |

**注册函数返回值 RETURN VALUE**

成功返回 `0`，失败返回非 `0`。

**回调函数类型**

```c
typedef int32_t (*ubs_mem_numa_fault_handler)(const char *name, uint64_t numaid, ubs_mem_fault_type_t type);
```

**回调函数参数**

| name   | IN/OUT | description                          |
| ------ | ------ | ------------------------------------ |
| name   | IN     | 发生故障的numa内存借用标识                     |
| numaid | IN     | 发生故障的远端numa对应的numaid                |
| type   | IN     | 内存块故障类型，详见 `ubs_mem_fault_type_t` 枚举 |

```c
typedef enum {
    UB_MEM_ATOMIC_DATA_ERR = 0,
    UB_MEM_READ_DATA_ERR,
    UB_MEM_FLOW_POISON,
    UB_MEM_FLOW_READ_AUTH_POISON,
    UB_MEM_FLOW_READ_AUTH_RESPERR,
    UB_MEM_TIMEOUT_POISON,
    UB_MEM_TIMEOUT_RESPERR,
    UB_MEM_READ_DATA_POISON,
    UB_MEM_READ_DATA_RESPERR,
    MAR_NOPORT_VLD_INT_ERR,
    MAR_FLUX_INT_ERR,
    MAR_WITHOUT_CXT_ERR,
    RSP_BKPRE_OVER_TIMEOUT_ERR,
    MAR_NEAR_AUTH_FAIL_ERR,
    MAR_FAR_AUTH_FAIL_ERR,
    MAR_TIMEOUT_ERR,
    MAR_ILLEGAL_ACCESS_ERR,
    REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR,
    MEM_EXPORT_FAULT,
    UB_MEM_HEALTHY = 1000,
} ubs_mem_fault_type_t;
```

**回调函数返回值 RETURN VALUE**

回调处理成功请返回`0`，失败返回非 `0`。

**附注 NOTES**

回调函数需要由调用方自行保证线程安全。

故障事件按用户级别上报。建议同一用户仅在一个进程注册；多进程注册时，各进程需自行甄别故障内存是否属于本进程借用。

已上报故障的numaId对应内存及时删除，该numaId可能被后续借用再次使用。

服务端检测断链事件自动清理注册监听，不再提供注销接口。

**样例 EXAMPLES**

```c
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ubs_engine.h>
#include <ubs_engine_mem.h>

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int32_t numa_fault_handler(const char *name, uint64_t numaid, ubs_mem_fault_type_t type)
{
    printf("receive numa fault event, name=%s, numaid=%lu, type=%d\n", name, numaid, type);
    return 0;
}

int main(void)
{
    int32_t ret;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_mem_numa_fault_register(numa_fault_handler);
    if (ret != 0) {
        perror("register numa fault handler failed");
        ubs_engine_client_finalize();
        return -1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("numa fault handler registered, waiting for events... (press Ctrl+C to exit)\n");
    while (g_running) {
        pause();
    }

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_mem\_fd\_get\_memid\_by\_import

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
int32_t ubs_mem_fd_get_memid_by_import(const char *name, uint64_t import_memid, ubs_mem_export_memid_t *mem_info);
```

**描述 DESCRIPTION**

在导入节点指定资源名和导入memId, 查询fd形态的远端内存memId

**参数 PARAMTERS**

| name                       | IN/OUT | description                                                 |
| -------------------------- | ------ | ----------------------------------------------------------- |
| name                       | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_` |
| import\_memid              | IN     | 导入memId                                                     |
| ubs\_mem\_export\_memid\_t | OUT    | 导出信息的数据结构，包含export\_slot\_id、 export\_memid                 |
| <br />                     | <br /> | <br />                                                      |

- 数据结构说明

```c
 typedef struct {
  uint32_t export_slot_id;                                  // 导出节点的id
  uint64_t export_memid;                                    // 导出内存块标识信息
 } ubs_mem_export_memid_t;
 
```

**返回值 RETURN VALUE**

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                               | Description     |
| ----------------------------------- | --------------- |
| UBS\_ERR\_NULL\_POINTER             | 空指针             |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ERR\_INVALID\_ARG              | 参数无效            |
| UBS_ENGINE_ERR_CONNECTION_FAILED        | 连接UBSE服务端失败     |
| UBS_ENGINE_ERR_AUTH_FAILED              | UBSE服务端鉴权不通过    |
| UBS\_ENGINE\_ERR\_NOT\_EXIST        | 借用关系不存在         |
| UBS\_ENGINE\_ERR\_CREATING          | 资源正在创建中         |
| UBS\_ENGINE\_ERR\_DELETING          | 资源正在删除中         |
| UBS\_ENGINE\_ERR\_EXPORT\_LEDGERING | 导出节点对账中、调用方进行重试 |
| UBS_ENGINE_ERR_TIMEOUT                  | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL                 | UBSE服务端内部错误     |

**附注 NOTES**

无

**样例 EXAMPLES**

```c
#include <stdint.h>
#include <stdio.h>
#include <ubs_engine_mem.h>
static void ubs_mem_fd_get_memid_by_import(void)
{
    
    printf("=== ubs_mem_fd_get_memid_by_import_example ===\n");
    char *name = "fd_name_test";
    uint64_t import_memid = 9;
    ubs_mem_export_memid_t mem_info;
    int ret = ubs_mem_fd_get_memid_by_import(name, import_memid, &mem_info);
    if (ret != 0) {
        printf("ubs_mem_fd_get_memid_by_import failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_fd_get_memid_by_import success, export_slot_id=%u, export_memid=%lu\n",
               mem_info.export_slot_id, mem_info.export_memid);
    }
}
```

### ubs\_mem\_numa\_get\_memid\_by\_import

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
int32_t ubs_mem_numa_get_memid_by_import(const char *name, uint64_t import_memid, ubs_mem_export_memid_t *mem_info);
```

**描述 DESCRIPTION**

在导入节点指定资源名和导入memId, 查询numa形态的远端内存memId

**参数 PARAMTERS**

| name                       | IN/OUT | description                                                 |
| -------------------------- | ------ | ----------------------------------------------------------- |
| name                       | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_` |
| import\_memid              | IN     | 导入memId                                                     |
| ubs\_mem\_export\_memid\_t | OUT    | 导出信息的数据结构，包含export\_slot\_id、 export\_memid                 |

- 数据结构说明

```c
 typedef struct {
  uint32_t export_slot_id;                                  // 导出节点的id
  uint64_t export_memid;                                    // 导出内存块标识信息
 } ubs_mem_export_memid_t;
 
```

**返回值 RETURN VALUE**

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                               | Description     |
| ----------------------------------- | --------------- |
| UBS\_ERR\_NULL\_POINTER             | 空指针             |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ERR\_INVALID\_ARG              | 参数无效            |
| UBS_ENGINE_ERR_CONNECTION_FAILED        | 连接UBSE服务端失败     |
| UBS_ENGINE_ERR_AUTH_FAILED              | UBSE服务端鉴权不通过    |
| UBS\_ENGINE\_ERR\_NOT\_EXIST        | 借用关系不存在         |
| UBS\_ENGINE\_ERR\_CREATING          | 资源正在创建中         |
| UBS\_ENGINE\_ERR\_DELETING          | 资源正在删除中         |
| UBS\_ENGINE\_ERR\_EXPORT\_LEDGERING | 导出节点对账中、调用方进行重试 |
| UBS_ENGINE_ERR_TIMEOUT                  | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL                 | UBSE服务端内部错误     |

**附注 NOTES**

无

**样例 EXAMPLES**

```c
#include <stdint.h>
#include <stdio.h>
#include <ubs_engine_mem.h>
static void ubs_mem_numa_get_memid_by_import(void)
{
    
    printf("=== ubs_mem_numa_get_memid_by_import_example ===\n");
    char *name = "numa_name_test";
    uint64_t import_memid = 9;
    ubs_mem_export_memid_t mem_info;
    int ret = ubs_mem_numa_get_memid_by_import(name, import_memid, &mem_info);
    if (ret != 0) {
        printf("ubs_mem_numa_get_memid_by_import failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_numa_get_memid_by_import success, export_slot_id=%u, export_memid=%lu\n",
               mem_info.export_slot_id, mem_info.export_memid);
    }
}
 
```

### ubs\_mem\_shm\_get\_memid\_by\_import

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
int32_t ubs_mem_shm_get_memid_by_import(const char *name, uint64_t import_memid, ubs_mem_export_memid_t *mem_info);
```

**描述 DESCRIPTION**

在导入节点指定资源名和导入memId, 查询shm形态的远端内存memId

**参数 PARAMTERS**

| name                       | IN/OUT | description                                                 |
| -------------------------- | ------ | ----------------------------------------------------------- |
| name                       | IN     | 借用标识，最大长度48字节，含结尾字符 `\0`<br />name仅可包括大小写字母、数字、`.`、`:`、`-` 以及 `_` |
| import\_memid              | IN     | 导入memId                                                     |
| ubs\_mem\_export\_memid\_t | OUT    | 导出信息的数据结构，包含export\_slot\_id、 export\_memid                 |

- 数据结构说明

```c
 typedef struct {
  uint32_t export_slot_id;                                  // 导出节点的id
  uint64_t export_memid;                                    // 导出内存块标识信息
 } ubs_mem_export_memid_t;
 
```

**返回值 RETURN VALUE**

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                               | Description     |
| ----------------------------------- | --------------- |
| UBS\_ERR\_NULL\_POINTER             | 空指针             |
| UBS\_ERR\_NOT\_SUPPORTED           | 当前服务不支持对应内存特性 |
| UBS\_ERR\_INVALID\_ARG              | 参数无效            |
| UBS_ENGINE_ERR_CONNECTION_FAILED        | 连接UBSE服务端失败     |
| UBS_ENGINE_ERR_AUTH_FAILED              | UBSE服务端鉴权不通过    |
| UBS\_ENGINE\_ERR\_NOT\_EXIST        | 借用关系不存在         |
| UBS\_ENGINE\_ERR\_CREATING          | 资源正在创建中         |
| UBS\_ENGINE\_ERR\_DELETING          | 资源正在删除中         |
| UBS\_ENGINE\_ERR\_EXPORT\_LEDGERING | 导出节点对账中、调用方进行重试 |
| UBS_ENGINE_ERR_TIMEOUT              | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL             | UBSE服务端内部错误     |

**附注 NOTES**

无

**样例 EXAMPLES**

```c
#include <stdint.h>
#include <stdio.h>
#include <ubs_engine_mem.h>
static void ubs_mem_shm_get_memid_by_import(void)
{
    
    printf("=== ubs_mem_shm_get_memid_by_import_example ===\n");
    char *name = "shm_name_test";
    uint64_t import_memid = 9;
    ubs_mem_export_memid_t mem_info;
    int ret = ubs_mem_shm_get_memid_by_import(name, import_memid, &mem_info);
    if (ret != 0) {
        printf("ubs_mem_shm_get_memid_by_import failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_get_memid_by_import success, export_slot_id=%u, export_memid=%lu\n",
               mem_info.export_slot_id, mem_info.export_memid);
    }
}
```

## libubse topo

### ubs\_topo\_node\_list

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_topo.h>
int32_t ubs_topo_node_list(ubs_topo_node_t **node_list, uint32_t *node_cnt);
```

**描述 DESCRIPTION**

查询全量节点信息。

**参数 PARAMETERS**

| name       | IN/OUT | description                            |
| ---------- | ------ | -------------------------------------- |
| node\_list | OUT    | 节点信息数组，调用方需要使用 `free` 主动释放内存           |
| node\_cnt  | OUT    | 节点信息个数，范围 `[0, UBS_TOPO_MAX_NODE_NUM]` |

- 数据结构说明

```c
#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 4

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;                                      // 节点唯一标识
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];               // socket id 列表
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM]; // socket 下的 numa id 列表
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];        // IP 地址列表
    char host_name[HOST_NAME_MAX];                         // 主机名
} ubs_topo_node_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

当前只支持 CPU 节点。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

以下程序初始化 UBSE 客户端，并查询全量节点信息。

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_topo.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_topo_node_t *node_list = NULL;
    uint32_t node_cnt = 0;

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_topo_node_list(&node_list, &node_cnt);
    if (ret != UBS_SUCCESS) {
        perror("get nodes failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    free(node_list);
    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_topo\_node\_local\_get

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_topo.h>
int32_t ubs_topo_node_local_get(ubs_topo_node_t *node);
```

**描述 DESCRIPTION**

查询本节点信息。

**参数 PARAMETERS**

| name | IN/OUT | description |
| ---- | ------ | ----------- |
| node | OUT    | 节点信息        |

- 数据结构说明

```c
#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 4

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;                                      // 节点唯一标识
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];               // socket id 列表
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM]; // socket 下的 numa id 列表
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];        // IP 地址列表
    char host_name[HOST_NAME_MAX];                         // 主机名
} ubs_topo_node_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

当前只支持 CPU 节点。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

以下程序初始化 UBSE 客户端，并查询本节点信息。

```c
#include <stdio.h>
#include <ubs_engine.h>
#include <ubs_engine_topo.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_topo_node_t local_node;

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_topo_node_local_get(&local_node);
    if (ret != UBS_SUCCESS) {
        perror("get local node failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    ubs_engine_client_finalize();
    return 0;
}
```

### ubs\_topo\_link\_list

**库 LIBRARY**

ubse库 (/usr/lib64/libubse-client.so)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_topo.h>
int32_t ubs_topo_link_list(ubs_topo_link_t **cpu_links, uint32_t *cpu_link_cnt);
```

**描述 DESCRIPTION**

查询所有 CPU 类型节点的组网拓扑信息，粒度为硬件连线。

**参数 PARAMETERS**

| name           | IN/OUT | description                                    |
| -------------- | ------ | ---------------------------------------------- |
| cpu\_links     | OUT    | CPU 连接信息数组，调用方需要使用 `free` 主动释放内存               |
| cpu\_link\_cnt | OUT    | CPU 连接信息个数，范围 `[0, UBS_TOPO_MAX_CPU_LINK_NUM]` |

- 数据结构说明

```c
typedef struct {
    uint32_t slot_id;        // 节点id
    uint32_t socket_id;      // socket id, 0xFFFFFFFF表示无效值
    uint32_t port_id;        // 端口id
    uint32_t peer_slot_id;   // 对端节点id
    uint32_t peer_socket_id; // 对端socket id, 0xFFFFFFFF表示无效值
    uint32_t peer_port_id;   // 对端端口id
} ubs_topo_link_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

**错误 ERRORS**

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

**约束 CONSTRAINTS**

暂无。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

以下程序初始化 UBSE 客户端，并查询 CPU 拓扑连线信息。

```c
#include <stdio.h>
#include <stdlib.h>
#include <ubs_engine.h>
#include <ubs_engine_topo.h>

int main(void)
{
    int32_t ret;
    const char *path = "/var/run/ubse/ubse.sock";
    ubs_topo_link_t *cpu_links = NULL;
    uint32_t cpu_link_cnt = 0;

    ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        perror("init failed");
        return -1;
    }

    ret = ubs_topo_link_list(&cpu_links, &cpu_link_cnt);
    if (ret != UBS_SUCCESS) {
        perror("get node cpu list failed");
        ubs_engine_client_finalize();
        return -1;
    }

    /* Do your work here... */

    free(cpu_links);
    ubs_engine_client_finalize();
    return 0;
}
```

## UBSE URMA Controller SDK 接口说明文档

---

**概述**

UBSE URMA Controller 提供一组用于管理 URMA 设备的接口，支持设备的查询和分配功能。

**接口列表**

| 功能 | 接口 | 描述 |
|------|------|------|
| 设备查询 | `ubs_urma_dev_get` | 查询系统中所有 URMA 设备信息 |
| 设备分配 | `ubs_urma_dev_alloc` | 分配指定的 URMA 设备资源 |
| 设备释放 | `ubs_urma_dev_free` | 释放已分配的 URMA 设备资源 |

---

**错误码汇总**

| 错误码                            | 值    | 描述                |
|-----------------------------------|-------|---------------------|
| `UBS_SUCCESS`                     | 0     | 操作成功             |
| `UBS_ERR_NULL_POINTER`            | 3     | 空指针               |
| `UBS_ERR_NOT_SUPPORTED`           | 41    | 不支持的操作          |
| `UBS_ENGINE_ERR_OUT_OF_RANGE`      | 1000  | 参数超出范围          |
| `UBS_ENGINE_ERR_CONNECTION_FAILED` | 1002  | 连接 UBSE 服务端失败   |
| `UBS_ENGINE_ERR_AUTH_FAILED`       | 1003  | UBSE 服务端鉴权不通过  |
| `UBS_ENGINE_ERR_TIMEOUT`           | 1004  | UBSE 服务端处理超时    |
| `UBS_ENGINE_ERR_INTERNAL`          | 1005  | UBSE 服务端内部错误    |
| `UBS_ENGINE_ERR_NOT_EXIST`         | 1007  | URMA 设备不存在       |

---

<br>
<hr style="border: 2px solid #4a90d9; margin-top: 30px; margin-bottom: 30px;">

### ubs_urma_dev_get

**库 LIBRARY**

**ubse 库** (`libubse.so`)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_urma.h>

uint32_t ubs_urma_dev_get(ubs_urma_dev_t **urma_devices, uint32_t *urma_cnt);
```

**描述 DESCRIPTION**

查询系统中所有可用的 URMA 设备信息，返回设备列表，包括设备名称、健康状态和硬件资源 ID 等信息。

**参数 PARAMETERS**

| name          | IN/OUT | 描述                                         |
|---------------|--------|----------------------------------------------|
| urma_devices  | OUT    | URMA 设备信息数组指针，调用成功后需用 free() 释放 |
| urma_cnt      | OUT    | 返回的 URMA 设备数量                           |

- 数据结构说明

```c
#define UBS_URMA_NAME_MAX 32 // URMA设备名称最大长度（含结束符）

typedef struct {
    char name[UBS_URMA_NAME_MAX]; // 设备名称
    uint32_t healthy;             // 端口可用状态：0-可用，1-不可用
    uint64_t hw_res_id;           // 硬件资源 ID
} ubs_urma_dev_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ERR_NOT_SUPPORTED          | 当前服务不支持URMA特性 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过   |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误     |

**约束 CONSTRAINTS**

暂无

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序初始化UBSE客户端，查询URMA设备信息。

```c
#include <stdio.h>
#include <stdlib.h>
#include <libubse.h>
#include <ubs_engine_urma.h>

int main(void)
{
    int32_t ret;
    uint32_t urma_cnt = 0;
    ubs_urma_dev_t *urma_devices = NULL;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (UBS_SUCCESS != ret) {
        perror("init failed.\n");
        return -1;
    }

    ret = ubs_urma_dev_get(&urma_devices, &urma_cnt);
    if (UBS_SUCCESS != ret) {
        perror("get urma devices failed.\n");
        ubs_engine_client_finalize();
        return -1;
    }

    for (uint32_t i = 0; i < urma_cnt; i++) {
        printf("URMA device: %s, healthy: %u\n", urma_devices[i].name, urma_devices[i].healthy);
    }

    free(urma_devices);
    ubs_engine_client_finalize();

    return 0;
}
```

---

<br>
<hr style="border: 2px solid #4a90d9; margin-top: 30px; margin-bottom: 30px;">

### ubs_urma_dev_alloc

**库 LIBRARY**

**ubse 库** (`libubse.so`)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_urma.h>

uint32_t ubs_urma_dev_alloc(const char *name, ubs_urma_dev_info_t *dev_info);
```

**描述 DESCRIPTION**

分配指定的 URMA 设备资源，返回设备的路径信息，包括 bonding 路径、EID 和 FE 路径等。

**参数 PARAMETERS**

| name     | IN/OUT | 描述                                         |
|----------|--------|----------------------------------------------|
| name     | IN     | URMA 设备名称，长度不超过 32 字节              |
| dev_info | IN/OUT | 分配的 URMA 设备路径信息，调用方需预先申请内存  |

- 数据结构说明

```c
#define UBS_URMA_NAME_MAX        32  // URMA设备名称最大长度（含结束符）
#define UBS_MAX_URMA_PATH_LENGTH 64  // URMA路径最大长度（含结束符）
#define UBS_FE_PATH_NUM          2   // FE路径数量

typedef struct {
    char bonding_path[UBS_MAX_URMA_PATH_LENGTH];                // Bonding 路径
    char bonding_eid[UBS_MAX_URMA_PATH_LENGTH];                 // Bonding EID
    char fe_path[UBS_FE_PATH_NUM][UBS_MAX_URMA_PATH_LENGTH];   // FE 路径数组
} ubs_urma_dev_info_t;
```

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ERR_NOT_SUPPORTED          | 当前服务不支持URMA特性 |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ENGINE_ERR_NOT_EXIST         | URMA设备不存在       |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过   |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误     |

**约束 CONSTRAINTS**

- `dev_info` 参数需由调用方预先申请内存
- 设备名称长度不能超过 32 字节

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序初始化UBSE客户端，分配指定的URMA设备。

```c
#include <stdio.h>
#include <string.h>
#include <libubse.h>
#include <ubs_engine_urma.h>

int main(void)
{
    int32_t ret;
    ubs_urma_dev_info_t dev_info;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (UBS_SUCCESS != ret) {
        perror("init failed.\n");
        return -1;
    }

    memset(&dev_info, 0, sizeof(dev_info));
    ret = ubs_urma_dev_alloc("urma0", &dev_info);
    if (UBS_SUCCESS != ret) {
        perror("alloc urma device failed.\n");
        ubs_engine_client_finalize();
        return -1;
    }

    printf("bonding_path: %s\n", dev_info.bonding_path);
    printf("bonding_eid: %s\n", dev_info.bonding_eid);

    ubs_engine_client_finalize();

    return 0;
}
```

---

<br>
<hr style="border: 2px solid #4a90d9; margin-top: 30px; margin-bottom: 30px;">

### ubs_urma_dev_free

**库 LIBRARY**

**ubse 库** (`libubse.so`)

**摘要 SYNOPSIS**

```c
#include <ubs_engine_urma.h>

uint32_t ubs_urma_dev_free(const char *name);
```

**描述 DESCRIPTION**

释放指定的 URMA 设备资源，使其可以被其他进程重新分配使用。

**参数 PARAMETERS**

| name | IN/OUT | 描述                          |
|------|--------|-------------------------------|
| name | IN     | URMA 设备名称，长度不超过 32 字节 |

**返回值 RETURN VALUE**

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

**错误 ERRORS**

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ERR_NOT_SUPPORTED          | 当前服务不支持URMA特性 |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ENGINE_ERR_NOT_EXIST         | URMA设备不存在       |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过   |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误     |

**约束 CONSTRAINTS**

- 只能释放已分配的设备
- 设备名称长度不能超过 32 字节

**附注 NOTES**

暂无

**样例 EXAMPLES**

以下程序初始化UBSE客户端，分配并释放URMA设备。

```c
#include <stdio.h>
#include <string.h>
#include <libubse.h>
#include <ubs_engine_urma.h>

int main(void)
{
    int32_t ret;
    ubs_urma_dev_info_t dev_info;

    ret = ubs_engine_client_initialize("/var/run/ubse/ubse.sock");
    if (UBS_SUCCESS != ret) {
        perror("init failed.\n");
        return -1;
    }

    memset(&dev_info, 0, sizeof(dev_info));
    ret = ubs_urma_dev_alloc("urma0", &dev_info);
    if (UBS_SUCCESS != ret) {
        perror("alloc urma device failed.\n");
        ubs_engine_client_finalize();
        return -1;
    }

    printf("bonding_path: %s\n", dev_info.bonding_path);
    printf("bonding_eid: %s\n", dev_info.bonding_eid);

    ret = ubs_urma_dev_free("urma0");
    if (UBS_SUCCESS != ret) {
        perror("free urma device failed.\n");
    }

    ubs_engine_client_finalize();

    return 0;
}
```

## UBSE 内存控制器接口说明文档

**概述**

UBSE 内存控制器提供了一组用于管理内存借用关系的接口。这些接口支持不同类型的内存借用，包括基于 NUMA 的内存借用和基于地址的内存借用。文档中详细描述了每个接口的功能、参数和返回值。

---

### 枚举类型

#### `UbseMemStage`

表示借用关系的阶段。

| 枚举值 | 描述 |
|--------|------|
| `UBSE_NOT_EXIST` | 借用关系不存在 |
| `UBSE_CREATING` | 正在创建中 |
| `UBSE_DELETING` | 正在删除中 |
| `UBSE_EXIST` | 创建成功 |
| `UBSE_ERR_ONLY_IMPORT` | 只存在借入 |
| `UBSE_ERR_WAIT_UNEXPORT` | 等待 unexport 执行 |

#### `UbseMemBorrowType`

表示借用类型。

| 枚举值 | 描述 |
|--------|------|
| `FD_BORROW` | 文件描述符借用 |
| `NUMA_BORROW` | NUMA 借用 |
| `ADDR_BORROW` | 地址借用 |
| `SHM_BORROW` | 共享内存借用 |
| `SHM_ATTACH` | 共享内存附加 |

#### `UbseMemDistance`

表示CPU连线类型。

| 枚举值 | 描述 |
|--------|------|
| `MEM_DISTANCE_L0` | L0 对应直接 CPU 连线节点 |
| `MEM_DISTANCE_L1` | L1 对应通过 1 跳节点（暂不支持） |
| `MEM_DISTANCE_L2` | L2 对应超过 1 跳节点（暂不支持） |

---

### 结构体

#### `UbseMemResult`

表示借用结果。

| 字段 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string` | 借用标识 |
| `importNodeId` | `std::string` | 借入节点 ID |
| `realSize` | `uint64_t` | 实际大小 |
| `stage` | `UbseMemStage` | 借用阶段 |

---

#### `UbseNumaMemoryDebtInfo`

表示 NUMA 内存借用账本信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string` | 资源名称标识 |
| `borrowNodeId` | `std::string` | 借入节点 ID |
| `borrowSocketIdList` | `std::vector<int>` | 借入 socket ID 列表 |
| `usrInfo` | `uint8_t[UBSE_MAX_USR_INFO_LEN]` | 调用方私有数据 |
| `borrowMemId` | `std::vector<uint64_t>` | 借入内存 ID 列表 |
| `lentNodeId` | `std::string` | 借出节点 ID |
| `lentSocketIdList` | `std::vector<int>` | 借出 socket ID 列表 |
| `lentNumaIdList` | `std::vector<int16_t>` | 借出 NUMA ID 列表 |
| `lentNumaSizeList` | `std::vector<uint64_t>` | 借出 NUMA 大小列表 |
| `lentMemId` | `std::vector<uint64_t>` | 借出内存 ID 列表 |
| `size` | `uint64_t` | 总借用内存大小 |
| `remoteNumaId` | `int64_t` | 远端 NUMA ID |
| `uid` | `uid_t` | 发起借用方运行用户的 UID |
| `username` | `std::string` | 发起借用方运行用户的名称 |

---

#### `UbseNumaMemoryImportDebtInfo`

表示 NUMA 内存导入账本信息。

| 字段 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| `name` | `std::string` | - | 资源名称标识 |
| `borrowNodeId` | `std::string` | - | 借入节点 ID |
| `borrowSocketIdList` | `std::vector<int>` | - | 借入 socket ID 列表 |
| `size` | `uint64_t` | - | 总借用内存大小（字节） |
| `usrInfo` | `uint8_t[UBSE_MAX_USR_INFO_LEN]` | - | 调用方私有数据 |
| `remoteNumaId` | `int64_t` | `-1` | 远端 NUMA ID |

---

#### `UbseMemBorrower`

表示借用方信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `nodeId` | `std::string` | 节点 ID |
| `affinitySocketId` | `int` | 可选，亲和 socket ID，-1 表示无效 |
| `uid` | `uid_t` | 发起借用方运行用户的 UID |
| `username` | `std::string` | 发起借用方运行用户的名称 |

---

#### `UbseMemNumaLender`

表示 NUMA 借出方信息。

| 字段 | 类型 | 描述                                      |
|------|------|-----------------------------------------|
| `slotId` | `uint32_t` | 节点唯一标识                                  |
| `socketId` | `uint32_t` | socket ID                               |
| `numaId` | `uint64_t` | NUMA ID                                 |
| `size` | `uint64_t` | 借出内存大小<br/>单位Byte, 取值范围大于等于4\*1024*1024 |

---

#### `UbseTopoIpAddress`

表示拓扑 IP 地址。

| 字段 | 类型 | 描述 |
|------|------|------|
| `af` | `int32_t` | 地址族 |
| `ipv4` | `struct in_addr` | IPv4 地址 |
| `ipv6` | `struct in6_addr` | IPv6 地址 |

---

#### `UbseTopoNode`

表示拓扑节点。

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotId` | `uint32_t` | 节点唯一标识 |
| `socketIdList` | `std::vector<int16_t>` | socket ID 列表 |
| `numaIdList` | `std::vector<int32_t>` | NUMA ID 列表 |
| `ips` | `std::vector<UbseTopoIpAddress>` | IP 地址列表 |
| `hostName` | `std::string` | 主机名 |

---

#### `UbseMemNumaDesc`

表示 NUMA 借用形成的远端 NUMA 信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string` | 借用标识 |
| `numaId` | `int64_t` | 远端 NUMA ID |
| `exportNode` | `UbseTopoNode` | 借出节点 |
| `importNode` | `UbseTopoNode` | 借入节点 |
| `size` | `uint64_t` | 借用大小 |
| `usrInfo` | `uint8_t[UBSE_MAX_USR_INFO_LEN]` | 调用方私有数据 |

---

#### `UbseNodeNumaInfo`

表示节点 NUMA 信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `nodeId` | `std::string` | 节点 ID |
| `hostName` | `std::string` | 主机名 |
| `numaId` | `uint32_t` | NUMA ID |
| `socketId` | `uint32_t` | socket ID |
| `mReservedMemRatio` | `uint64_t` | 预留内存比例 |
| `memTotal` | `uint64_t` | 总内存量 |
| `memFree` | `uint64_t` | 空闲内存量 |
| `nrHugepages` | `uint64_t` | 2M 大页数量 |
| `freeHugepages` | `uint64_t` | 2M 大页空闲数量 |
| `usedHugepages` | `uint64_t` | 2M 大页已用数量 |
| `timestamp` | `uint64_t` | 数据采集的时间戳 |
| `memLent` | `uint64_t` | 借出内存量 |
| `memShared` | `uint64_t` | 共享内存量 |

---

#### `UbseMemNumaCreateOpt`

表示 NUMA 借用的可选项。

| 字段 | 类型 | 描述                                      |
|------|------|-----------------------------------------|
| `size` | `uint64_t` | 借出内存大小<br/>单位Byte, 取值范围大于等于4\*1024*1024 |
| `distance` | `UbseMemDistance` | 内存距离                                    |
| `highWatermark` | `size_t` | 内存使用量百分比                                |
| `usrInfo` | `uint8_t[UBSE_MAX_USR_INFO_LEN]` | 调用方私有数据                                 |

---

#### `UbseMemNumaCandidateOpt`

表示指定候选借出节点的 NUMA 借用可选项。继承自 `UbseMemNumaCreateOpt`。

**自有字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotIds` | `std::vector<std::string>` | 候选借出节点范围 |

---

#### `UbseMemAddrBorrowLocAndSizeByPid`

表示基于 PID 的地址借用位置和大小。

| 字段 | 类型 | 默认值 | 描述                                    |
|------|------|--------|---------------------------------------|
| `addr` | `uint64_t` | `0` | 借用的进程虚拟地址                             |
| `size` | `uint64_t` | `0` | 该段地址借入大小，单位Byte |

---

#### `UbseMemProcessLender`

表示基于进程的地址借出方信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotId` | `uint32_t` | 节点唯一标识 |
| `socketId` | `int` | 内存申请借出方节点 socket 信息，-1 表示无效 |
| `pid` | `uint64_t` | 借出进程 PID |
| `vaLists` | `std::vector<UbseMemAddrBorrowLocAndSizeByPid>` | 借用地址段 |

---

#### `UbseMemAddrDesc`

表示基于地址借用形成的远端 NUMA 信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string` | 借用标识 |
| `numaId` | `int64_t` | 远端 NUMA ID |
| `lender` | `UbseMemProcessLender` | 借出节点 |
| `importNode` | `UbseTopoNode` | 借入节点 |
| `size` | `uint64_t` | 借用大小 |

---

### 函数

#### `UbseQueryResult`

查询借用标识对应的操作结果。

```cpp
uint32_t UbseQueryResult(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType = UbseMemBorrowType::NUMA_BORROW);
```

**参数:**

- `name`: 借用标识
- `result`: 借用结果
- `borrowType`: 借用类型，默认为 `NUMA_BORROW`

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseGetNumaMemDebtInfoWithNode`

返回和传入节点相关的账本信息。

```cpp
UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string &nodeId, std::vector<UbseNumaMemoryDebtInfo> &debtInfos);
```

**参数:**

- `nodeId`: 节点 ID
- `debtInfos`: 借用账本对象集合

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseGetNumaMemDebtInfo`

返回当前集群拓扑中的所有对账完成节点的账本信息。

```cpp
UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo> &debtInfos);
```

**参数:**

- `debtInfos`: 借用账本对象集合

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseGetNumaMemImportDebtInfoWithLocalNode`

返回当前节点导入账本信息。本地节点账本已经从 OBMM 恢复完成则返回成功，否则返回部分成功。

```cpp
UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos);
```

**参数:**

- `debtInfos`: 借用账本对象集合

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseGetAllNodeNumaInfo`

返回所有采集到的节点相关的 NUMA 信息。

```cpp
UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList);
```

**参数:**

- `numaNodeInfoList`: 节点 NUMA 对象集合

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseGetNodeNumaInfoByNodeId`

返回所有采集到的节点指定节点相关的 NUMA 信息。

```cpp
UbseResult UbseGetNodeNumaInfoByNodeId(const std::string &nodeId, std::vector<UbseNodeNumaInfo> &numaNodeInfoList);
```

**参数:**

- `nodeId`: 节点 ID
- `numaNodeInfoList`: 节点 NUMA 对象集合

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemNumaCreateWithLender`

指定借出信息，提供给插件使用 NUMA 借用。

```cpp
UbseResult UbseMemNumaCreateWithLender(const std::string &name, const UbseMemBorrower &borrower, const std::vector<UbseMemNumaLender> &lenders, uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc &desc);
```

**参数:**

- `name`: 借用标识
- `borrower`: 借用方信息
- `lenders`: 借出方信息
- `usrInfo`: 调用方私有数据
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemNumaCreate`

提供给插件使用 NUMA 借用。

```cpp
UbseResult UbseMemNumaCreate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemNumaCreateOpt &opt, UbseMemNumaDesc &desc);
```

**参数:**

- `name`: 借用标识
- `borrower`: 借用方信息
- `opt`: 借出方信息
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemNumaCreateWithCandidate`

指定候选借出节点，提供给插件使用 NUMA 借用。

```cpp
UbseResult UbseMemNumaCreateWithCandidate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemNumaCandidateOpt &opt, UbseMemNumaDesc &desc);
```

**参数:**

- `name`: 借用标识
- `borrower`: 借用方信息
- `opt`: 可选项
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemNumaDelete`

删除指定 NUMA 远端内存。

```cpp
UbseResult UbseMemNumaDelete(const std::string &name, const UbseMemBorrower &borrower);
```

**参数:**

- `name`: 借用标识
- `borrower`: 借用方信息

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemAddrCreate`

提供给插件使用地址借用。

```cpp
UbseResult UbseMemAddrCreate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemProcessLender &lender, uint32_t flag, UbseMemAddrDesc &desc);
```

**参数:**

- `name`: 借用标识
- `borrower`: 借用方信息
- `lender`: 借出方信息
- `flag`: 额外的内存借用属性
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemAddrDelete`

删除地址借用。

```cpp
UbseResult UbseMemAddrDelete(const std::string &name, const UbseMemBorrower &borrower);
```

**参数:**

- `name`: 借用标识
- `borrower`: 借用方信息

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseMemDebtCircleCheck`

检查借用是否成环。

```cpp
UbseResult UbseMemDebtCircleCheck(const std::string &srcNodeId, const std::string &dstNodeId, bool &isCircle);
```

**参数:**

- `srcNodeId`: 借入 NodeId
- `dstNodeId`: 借出 NodeId
- `isCircle`: 是否成环

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

#### `UbseGetAddrMemDebtInfoWithNode`

返回和传入节点相关的地址借用账本信息。传入节点对账完成则返回成功，传入节点未对账完成，如果剩余节点全部对账完成返回成功，否则返回部分成功。

```cpp
UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string &nodeId, std::vector<UbseMemAddrDesc> &debtInfos);
```

**参数:**

- `nodeId`: 节点 ID
- `debtInfos`: 地址借用账本对象集合

**返回值:**

- `UBSE_OK`: 成功
- 其他: 失败，详见 `ubse_error.h`

---

### 常量

#### `UBSE_MAX_USR_INFO_LEN`

调用方私有数据的最大长度。

```cpp
static constexpr uint32_t UBSE_MAX_USR_INFO_LEN = 32;
```

---

#### `UBSE_MEM_FLAG_NO_WR_DELAY`

非写接力模式标志。

```cpp
#define UBSE_MEM_FLAG_NO_WR_DELAY 0x1
```

---

### 总结

UBSE 内存控制器提供了一组丰富的接口，用于管理内存借用关系。通过这些接口，可以方便地进行 NUMA 和地址借用，同时支持查询和删除操作。
