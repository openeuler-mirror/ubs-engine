# 1. ubs\_topo\_node\_list

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_topo.h>
int32_t ubs_topo_node_list(ubs_topo_node_t **node_list, uint32_t *node_cnt);
```

## 描述 DESCRIPTION

查询全量节点信息。

## 参数 PARAMETERS

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

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

## 错误 ERRORS

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

## 约束 CONSTRAINTS

当前只支持 CPU 节点。

## 附注 NOTES

暂无。

## 样例 EXAMPLES

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

# 2. ubs\_topo\_node\_local\_get

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_topo.h>
int32_t ubs_topo_node_local_get(ubs_topo_node_t *node);
```

## 描述 DESCRIPTION

查询本节点信息。

## 参数 PARAMETERS

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

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

## 错误 ERRORS

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

## 约束 CONSTRAINTS

当前只支持 CPU 节点。

## 附注 NOTES

暂无。

## 样例 EXAMPLES

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

# 3. ubs\_topo\_link\_list

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_topo.h>
int32_t ubs_topo_link_list(ubs_topo_link_t **cpu_links, uint32_t *cpu_link_cnt);
```

## 描述 DESCRIPTION

查询所有 CPU 类型节点的组网拓扑信息，粒度为硬件连线。

## 参数 PARAMETERS

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

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见 `错误 ERRORS`。

## 错误 ERRORS

| Error                                | Description  |
| ------------------------------------ | ------------ |
| UBS\_ERR\_NULL\_POINTER              | 空指针          |
| UBS\_ENGINE\_ERR\_CONNECTION\_FAILED | 连接UBSE服务端失败  |
| UBS\_ENGINE\_ERR\_AUTH\_FAILED       | UBSE服务端鉴权不通过 |
| UBS\_ENGINE\_ERR\_TIMEOUT            | UBSE服务端处理超时  |
| UBS\_ENGINE\_ERR\_INTERNAL           | UBSE服务端内部错误  |

## 约束 CONSTRAINTS

暂无。

## 附注 NOTES

暂无。

## 样例 EXAMPLES

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

