# 1. ubs_topo_node_list

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_topo.h>
int32_t ubs_topo_node_list(ubs_topo_node_t **node_list, uint32_t *node_cnt);
```

## 描述 DESCRIPTION

查询全量节点信息

## 参数 PARAMTERS

| name      | IN/OUT | description                                    |
| --------- | ------ | ---------------------------------------------- |
| node_list | OUT    | 节点信息数组<br>调用方需要使用free主动释放内存 |
| node_cnt  | OUT    | 节点信息个数                                   |

- 数据结构说明

```c
typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
```

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                      | Description          |
| -------------------------- | -------------------- |
| UBS_ERR_NULL_POINTER       | 空指针               |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败   |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时   |
| UBS_ENGINE_ERR_INTERNAL   | UBSE服务端内部错误   |

## 约束 CONSTRAINTS

当前只支持CPU节点

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_topo.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
    ubs_topo_node_t *node_list = null;
    uint32_t node_cnt = 0;
    ret = ubs_topo_node_list(&node_list, &node_cnt);
    if (SUCCESS != ret) {
		perror("get nodes failed.\n");
		return -1;
	}
	
    /* Do your work here... */

    free(node_list);
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 2. ubs_topo_node_local_get

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_topo.h>
int32_t ubs_topo_node_local_get(ubs_topo_node_t *node);
```

## 描述 DESCRIPTION

查询本节点信息

## 参数 PARAMTERS

| name | IN/OUT | description |
| ---- | ------ | ----------- |
| node | OUT    | 节点信息    |

- 数据结构说明

```c
typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
```

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过  |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时    |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误    |

## 约束 CONSTRAINTS

当前只支持CPU节点

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_topo.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
    ubs_topo_node_t local_node;
    ret = ubs_topo_node_list(&local_node);
    if (SUCCESS != ret) {
		perror("get local node failed.\n");
		return -1;
	}
	
    /* Do your work here... */

    free(node_list);
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 3. ubs_topo_link_list

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_topo.h>
int32_t ubs_topo_link_list(ubs_topo_link_t **cpu_links, uint32_t *cpu_link_cnt);
```

## 描述 DESCRIPTION

查询所有CPU类型节点的拓扑信息

## 参数 PARAMTERS

| name         | IN/OUT | description                                       |
| ------------ | ------ | ------------------------------------------------- |
| cpu_links    | OUT    | cpu连接信息数组<br>调用方需要使用free主动释放内存 |
| cpu_link_cnt | OUT    | cpu连接信息个数                                   |

- 数据结构说明

```c
typedef struct {
    uint32_t slot_id;                      // 节点id
    uint32_t socket_id;                    // socket id
    uint32_t peer_slot_id;                 // 对端节点id
    uint32_t peer_socket_id;               // 对端socket id
} ubs_topo_link_t;
```

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description          |
|----------------------------------| -------------------- |
| UBS_ERR_NULL_POINTER             | 空指针               |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败   |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时   |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误   |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_topo.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
    ubse_cpu_link_t *cpu_links = null;
    uint32_t cpu_link_cnt = 0;
    ret = ubs_topo_link_list(&cpu_links, &cpu_link_cnt);
    if (SUCCESS != ret) {
		perror("get node cpu list failed.\n");
		return -1;
	}
	
    /* Do your work here... */

    free(cpu_links);
    
	ubs_engine_client_finalize();

	return 0;
}
```
