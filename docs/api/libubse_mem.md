# 1. ubs_mem_numastat_get

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t **numa_mems, uint32_t *numa_mem_cnt);
```

## 描述 DESCRIPTION

查询指定节点numa信息，仅返回可用节点的numa信息，当前只支持本地numa内存，后续会增加远端numa

## 参数 PARAMTERS

| name         | IN/OUT | description                                            |
| ------------ | ------ | ------------------------------------------------------ |
| slot_id      | IN     | 节点标识                                               |
| numa_mems    | OUT    | 节点numa信息数组<br>调用方需要使用free接口主动释放内存 |
| numa_mem_cnt | OUT    | 节点numa信息个数                                       |

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

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description  |
|----------------------------------|--------------|
| UBS_ERR_NULL_POINTER             | 空指针          |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时  |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误  |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败  |
| UBS_ENGINE_ERR_NODE_NOT_EXIST    | 查询节点不存在      |
| UBS_ENGINE_ERR_NODE_FAULT        | 查询节点故障       |

## 约束 CONSTRAINTS

当前只支持本地numa内存

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
    uint32_t node_id = 1;
    uint32_t numa_mem_cnt = 0;
    ubs_mem_numastat_t *numa_mem = null;
    ret = ubs_mem_numastat_get(node_id, &numa_mem, &numa_mem_cnt);
    if (SUCCESS != ret) {
		perror("create fd mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 2. ubs_mem_fd_create

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_create(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                          ubs_mem_distance_t distance, ubs_mem_fd_desc_t *fd_desc);
```

## 描述 DESCRIPTION

在本节点创建fd形态的远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                    |
| -------- | ------ |------------------------------------------------|
| name     | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| size     | IN     | 借用大小<br>单位Byte, 取值范围大于等于4\*1024*1024           |
| owner    | IN     | 内存资源使用者属主信息, 可选参数, Null不关注该字段                  |
| mode     | IN     | 内存资源使用者访问权限, 可选参数, 0则不关注该字段                    |
| distance | IN     | 内存访问距离                                         |
| fd_desc  | OUT    | 内存描述信息                                         |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubs自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;

typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
} ubs_mem_distance_t;

typedef struct {
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与UBM保持一致
    uint32_t socket_id;   // socket id
    uint64_t numa_id;     // 节点中的numa id
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;

// 内存借用状态枚举
#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                      // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点
    ubs_topo_node_t import_node;            // 借入节点
} ubs_mem_fd_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description            |
| -------------------------------- | ---------------------- |
| UBS_ERR_NULL_POINTER             | 空指针                  |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
   ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    mode_t mode = 0x700;
    char *name = "test_mem";
    uint64_t size = 1024 * 1024 * 1024;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_fd_desc_t fd_desc;
    ret = ubs_mem_fd_create(name, size, &owner, mode, dist, &fd_desc);
    if (SUCCESS != ret) {
		perror("create fd mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 3. ubs_mem_fd_create_with_lender

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_create_with_lender(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                      const ubs_mem_lender_t *lender, uint32_t lender_cnt, ubs_mem_fd_desc_t *fd_desc);
```

## 描述 DESCRIPTION

指定借出信息，在本节点创建fd形态的远端内存

## 参数 PARAMTERS

| name    | IN/OUT | description                                                  |
| ------- | ------ | ------------------------------------------------------------ |
| name    | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| owner   | IN     | 内存资源使用者属主信息, 可选参数, Null不关注该字段           |
| mode    | IN     | 内存资源使用者访问权限, 可选参数, 0则不关注该字段            |
| lender  | IN     | 借出信息                                                     |
| fd_desc | OUT    | 内存描述信息                                                 |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubs自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;

typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
} ubs_mem_distance_t;

typedef struct {
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与UBM保持一致
    uint32_t socket_id;   // socket id
    uint64_t numa_id;     // 节点中的numa id
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;

// 内存借用状态枚举
#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                      // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点
    ubs_topo_node_t import_node;            // 借入节点
} ubs_mem_fd_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description              |
| -------------------------------- | ------------------------ |
| UBS_ERR_NULL_POINTER             | 空指针                   |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
   ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    mode_t mode = 0x700;
    char *name = "test_mem";
    uint64_t size = 1024 * 1024 * 1024;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_fd_desc_t fd_desc;
    ubs_mem_lender_t lender{};
    ret = ubs_mem_fd_create_with_lender(name, size, &owner, mode,lender, 1, &fd_desc);
    if (SUCCESS != ret) {
		perror("create fd mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 

# 4.ubs_mem_fd_create_with_candidate

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_create_with_candidate(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                         const uint32_t *slot_ids, uint32_t slot_cnt, ubs_mem_fd_desc_t *fd_desc);
```

## 描述 DESCRIPTION

指定候选借出节点范围，在本节点创建fd形态的远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                    |
| -------- | ------ |------------------------------------------------|
| name     | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| size     | IN     | 借用大小<br>单位Byte, 取值范围大于等于4\*1024*1024           |
| owner    | IN     | 内存资源使用者属主信息, 可选参数, Null不关注该字段                  |
| mode     | IN     | 内存资源使用者访问权限, 可选参数, 0则不关注该字段                    |
| slot_ids | IN     | 候选借出节点范围                                       |
| slot_cnt | IN     | 候选借出节点个数                                       |
| fd_desc  | OUT    | 内存描述信息                                         |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubs自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;

typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
} ubs_mem_distance_t;

typedef struct {
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与UBM保持一致
    uint32_t socket_id;   // socket id
    uint64_t numa_id;     // 节点中的numa id
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;

#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;

// 内存借用状态枚举
#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                      // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点
    ubs_topo_node_t import_node;            // 借入节点
} ubs_mem_fd_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description              |
| -------------------------------- | ------------------------ |
| UBS_ERR_NULL_POINTER             | 空指针                   |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
   ubs_mem_fd_owner_t owner = {
        .uid = getuid(),
        .gid = getgid(),
        .pid = getpid()
    };
    mode_t mode = 0x700;
    char *name = "test_mem";
    uint64_t size = 1024 * 1024 * 1024;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_fd_desc_t fd_desc;
    ubs_mem_lender_t lender{};
    ret = ubs_mem_fd_create_with_candidate(name, size, &owner, mode,NULL, 0, &fd_desc);
    if (SUCCESS != ret) {
		perror("create fd mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 

# 5.ubs_mem_fd_permission

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_permission(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode);
```

## 描述 DESCRIPTION

改变本节点fd形态远端内存的permission信息

- 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同
- 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

## 参数 PARAMTERS

| name  | IN/OUT | description                                                  |
| ----- | ------ | ------------------------------------------------------------ |
| name  | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| owner | INOUT  | 内存资源使用者属主信息，必选参数, 不允许为Null               |
| mode  | IN     | 内存资源使用者访问权限，必选参数, 为0则不生效                |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubs自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description              |
| -------------------------------- | ------------------------ |
| UBS_ERR_NULL_POINTER             | 空指针                   |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由参数owner和mode标识

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c

```

# 

# 6.ubs_mem_fd_get()

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_get(const char *name, ubs_mem_fd_desc_t *fd_desc);
```

## 描述 DESCRIPTION

查询本节点fd形态远端内存信息

## 参数 PARAMTERS

| name               | IN/OUT | description                                                        |
| ------------------ | ------ | ------------------------------------------------------------------ |
| name               | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| fd_desc            | OUT    | fd内存信息                                                          |

- 数据结构说明

```c

#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;

// 内存借用状态枚举
#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                      // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点
    ubs_topo_node_t import_node;            // 借入节点
} ubs_mem_fd_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description         |
| -------------------------------- | ------------------- |
| UBS_ERR_NULL_POINTER             | 空指针               |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围      |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过  |
| UBS_ENGINE_ERR_NOT_EXIST         | 借用关系不存在        |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时    |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误    |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}

    char *name = "test_mem";
    ubs_mem_fd_desc_t fd_desc;
    ret = ubs_mem_fd_get(name, &fd_desc);
    if (SUCCESS != ret) {
		perror("get fd mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 7. ubs_mem_fd_list

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_list(ubs_mem_fd_desc_t **fd_descs, uint32_t *fd_desc_cnt);
```

## 描述 DESCRIPTION

查询本节点所有fd形态远端内存

## 参数 PARAMTERS

| name        | IN/OUT | description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| fd_descs    | OUT    | fd内存描述信息数组<br>调用成功时, 调用方需要使用free接口主动释放内存 |
| fd_desc_cnt | OUT    | fd内存描述信息数组中的元素个数                               |

- 数据结构说明

```c

#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;

// 内存借用状态枚举
#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
#define UBS_MEM_MAX_MEMID_NUM 2048

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                     // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM]; // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                      // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                       // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;            // 借出节点
    ubs_topo_node_t import_node;            // 借入节点
} ubs_mem_fd_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description         |
| -------------------------------- | ------------------- |
| UBS_ERR_NULL_POINTER             | 空指针               |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name前缀参数超出范围  |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过  |
| UBS_ENGINE_ERR_NOT_EXIST         | 借用关系不存在        |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时    |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误    |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}

    ubs_mem_fd_desc_t *fd_desc_list;
    uint32_t fd_desc_cnt = 0;
    ret = ubs_mem_fd_list(&fd_desc_list, &fd_desc_cnt);
    if (SUCCESS != ret) {
		perror("get fd mem list failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
    free(fd_desc_list);
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 8. ubs_mem_fd_delete

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_fd_delete(const char *name);
```

## 描述 DESCRIPTION

删除本节点指定fd远端内存

## 参数 PARAMTERS

| name | IN/OUT | description                                                  |
| ---- | ------ | ------------------------------------------------------------ |
| name | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |

- 数据结构说明

```c
无
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description                                                     |
| -------------------------------- | --------------------------------------------------------------- |
| UBS_ERR_NULL_POINTER             | 空指针                                                           |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围                                                  |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败                                                |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过                                              |
| UBS_ENGINE_ERR_NOT_EXIST         | 借用关系不存在                                                    |
| UBS_ENGINE_ERR_IMPORT_ABSENT     | IMPORT不在位, 无法删除                                            |
| UBS_ENGINE_ERR_CREATING          | 正在创建过程中                                                    |
| UBS_ENGINE_ERR_DELETING          | 正在删除过程中                                                    |
| UBS_ENGINE_ERR_UNIMPORT_SUCCESS  | UNIMPORT已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收 |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时                                                |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误                                                |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能删除资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

对于异常场景如username、uid改变或者用户被删除，ubse提供cli命令删除内存能力,具体请参考《ubsectl_mem.md》文档。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
    char *name = "test_mem";
    ret = ubs_mem_fd_delete(name);
    if (SUCCESS != ret) {
		perror("delete fd mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 9. ubs_mem_numa_create

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_create(const char *name, uint64_t size, ubs_mem_distance_t distance,
                            ubs_mem_numa_desc_t *numa_desc);
```

## 描述 DESCRIPTION

创建numa形态的远端内存

## 参数 PARAMTERS

| name      | IN/OUT | description                               |
| --------- | ------ |-------------------------------------------|
| name      | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |
| size      | IN     | 借用大小, 单位Byte, 取值范围大于等于大于等于4\*1024*1024    |
| distance  | IN     | 内存访问跳数                                    |
| numa_desc | OUT    | 借用形成的远端numa信息                             |

- 数据结构说明

```c
#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
#define UBS_MEM_MAX_NAME_LENGTH 48
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    int64_t numaid;                     // 形成远端numa对应的numaid
    ubs_topo_node_t export_node;        // 借出节点
    ubs_topo_node_t import_node;        // 借入节点
    uint64_t size;                      // 借用大小
} ubs_mem_numa_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description            |
| -------------------------------- | ---------------------- |
| UBS_ERR_NULL_POINTER             | 空指针                  |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
    char *name = "test_mem";
    uint64_t size = 1024 * 1024 * 1024;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_numa_desc_t numa_desc;
    ret = ubs_mem_numa_create(name, size, dist, &numa_desc);
    if (SUCCESS != ret) {
		perror("create numa mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 10. ubs_mem_numa_create_with_lender

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_create_with_lender(const char *name, const ubs_mem_lender_t *lender, uint32_t lender_cnt,
                                        ubs_mem_numa_desc_t *numa_desc);
```

## 描述 DESCRIPTION

指定借出信息，在本节点创建numa形态的远端内存，该资源的管理权限属于调用者

- 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid
- 指定port_id时从指定链路借用，否则从指定numa借用

## 参数 PARAMTERS

| name      | IN/OUT | description                                 |
| --------- | ------ |---------------------------------------------|
| name      | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |
| lender    | IN     | 借出信息                                        |
| numa_desc | OUT    | 借用形成的远端numa信息                               |

- 数据结构说明

```c
typedef struct {
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与UBM保持一致
    uint32_t socket_id;   // socket id
    uint64_t numa_id;     // 节点中的numa id
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;


#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
#define UBS_MEM_MAX_NAME_LENGTH 48
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    int64_t numaid;                     // 形成远端numa对应的numaid
    ubs_topo_node_t export_node;        // 借出节点
    ubs_topo_node_t import_node;        // 借入节点
    uint64_t size;                      // 借用大小
} ubs_mem_numa_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description              |
| -------------------------------- | ------------------------ |
| UBS_ERR_NULL_POINTER             | 空指针                   |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
    ubs_mem_lender_t lender = {
        .lender_numa = {
            .socket = {
                .node = {
                    .type = NODE_CPU,                   
                    .nodeid = "Node1"               
                },
                .socketid = 0                             
            },
            .numaid = 1                   
        },
        .lender_size = 1024 * 1024 * 128  
    };
    char *name = "test_mem";
    uint64_t size = 1024 * 1024 * 1024;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_numa_desc_t numa_desc;
    ret = ubs_mem_numa_create_with_lender(name, &lender, &numa_desc);
    if (SUCCESS != ret) {
		perror("create numa mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```



# 11.ubs_mem_numa_create_with_candidate

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_create_with_candidate(const char *name, uint64_t size, const uint32_t *slot_ids, uint32_t slot_cnt,
                                           ubs_mem_numa_desc_t *numa_desc);
```

## 描述 DESCRIPTION

指定候选借出节点，在本节点创建numa形态的远端内存，该资源的管理权限属于调用者

- 调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid
- 候选节点与当前借入节点，必须满足直连要求

## 参数 PARAMTERS

| name     | IN/OUT | description                                |
| -------- | ------ |--------------------------------------------|
| name     | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |
| size     | IN     | 借用大小, 单位Byte, 取值范围大于等于4\*1024*1024         |
| slot_ids | IN     | 候选借出节点范围                                   |
| slot_cnt | IN     | 候选借出节点个数                                   |
| fd_desc  | OUT    | 内存描述信息                                     |

- 数据结构说明

```c
#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
#define UBS_MEM_MAX_NAME_LENGTH 48
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    int64_t numaid;                     // 形成远端numa对应的numaid
    ubs_topo_node_t export_node;        // 借出节点
    ubs_topo_node_t import_node;        // 借入节点
    uint64_t size;                      // 借用大小
} ubs_mem_numa_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description              |
| -------------------------------- | ------------------------ |
| UBS_ERR_NULL_POINTER             | 空指针                   |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_EXISTED           | 借用关系已存在           |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
    ubs_mem_lender_t lender = {
        .lender_numa = {
            .socket = {
                .node = {
                    .type = NODE_CPU,                   
                    .nodeid = "Node1"               
                },
                .socketid = 0                             
            },
            .numaid = 1                   
        },
        .lender_size = 1024 * 1024 * 128  
    };
    char *name = "test_mem";
    uint64_t size = 1024 * 1024 * 1024;
    ubs_mem_distance_t dist = MEM_DISTANCE_L0;
    ubs_mem_numa_desc_t numa_desc;
    ret = ubs_mem_numa_create_with_candidate(name, &lender,nullptr,0, &numa_desc);
    if (SUCCESS != ret) {
		perror("create numa mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```



# 12.ubs_mem_numa_get

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_get(const char *name, ubs_mem_numa_desc_t *numa_desc);
```

## 描述 DESCRIPTION

查询本节点numa形态远端内存信息

## 参数 PARAMTERS

| name                 | IN/OUT | description                                                  |
| -------------------- | ------ | ------------------------------------------------------------ |
| name                 | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |
| numa_desc            | OUT    | 借用形成的远端numa信息                                         |

- 数据结构说明

```c

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
#define UBS_MEM_MAX_NAME_LENGTH 48
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    int64_t numaid;                     // 形成远端numa对应的numaid
    ubs_topo_node_t export_node;        // 借出节点
    ubs_topo_node_t import_node;        // 借入节点
    uint64_t size;                      // 借用大小
} ubs_mem_numa_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description         |
| -------------------------------- | ------------------- |
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败   |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ENGINE_ERR_NOT_EXIST         | 借用关系不存在       |
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
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}

    char *name = "test_mem";
    ubs_mem_numa_desc_t numa_desc;
    ret = ubs_mem_numa_get(name, &numa_desc);
    if (SUCCESS != ret) {
		perror("get numa mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 13. ubs_mem_numa_list

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_list(ubs_mem_numa_desc_t **numa_descs, uint32_t *numa_desc_cnt);
```

## 描述 DESCRIPTION

查询本节点所有numa形态远端内存

## 参数 PARAMTERS

| name          | IN/OUT | description                                                |
| ------------- | ------ | ---------------------------------------------------------- |
| numa_descs    | OUT    | numa内存描述信息数组<br>调用方需要使用free接口主动释放内存 |
| numa_desc_cnt | OUT    | numa内存描述信息数组中的元素个数                           |

- 数据结构说明

```c

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;
#define UBS_MEM_MAX_NAME_LENGTH 48
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    int64_t numaid;                     // 形成远端numa对应的numaid
    ubs_topo_node_t export_node;        // 借出节点
    ubs_topo_node_t import_node;        // 借入节点
    uint64_t size;                      // 借用大小
} ubs_mem_numa_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description          |
| -------------------------------- | -------------------- |
| UBS_ERR_NULL_POINTER             | 空指针               |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name前缀参数超出范围  |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过  |
| UBS_ENGINE_ERR_NOT_EXIST         | 借用关系不存在        |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时    |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误    |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}

    ubs_mem_numa_desc_t *numa_desc_list;
    uint32_t numa_desc_cnt = 0;
    ret = ubs_mem_numa_list(&numa_desc_list, &numa_desc_cnt);
    if (SUCCESS != ret) {
		perror("get numa mem list failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
    free(numa_desc_list);
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 14. ubs_mem_numa_delete

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_numa_delete(const char *name);
```

## 描述 DESCRIPTION

删除指定numa远端内存

## 参数 PARAMTERS

| name | IN/OUT | description                                                  |
| ---- | ------ | ------------------------------------------------------------ |
| name | IN     | 借用标识, name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name节点内保持唯一性 |

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description                                                     |
| -------------------------------- | --------------------------------------------------------------- |
| UBS_ERR_NULL_POINTER             | 空指针                                                           |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围                                                  |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败                                                |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过                                              |
| UBS_ENGINE_ERR_NOT_EXIST         | 借用关系不存在                                                    |
| UBS_ENGINE_ERR_IMPORT_ABSENT     | IMPORT不在位, 无法删除                                            |
| UBS_ENGINE_ERR_CREATING          | 正在创建过程中                                                    |
| UBS_ENGINE_ERR_DELETING          | 正在删除过程中                                                    |
| UBS_ENGINE_ERR_UNIMPORT_SUCCESS  | UNIMPORT已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收 |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时                                                |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误                                                |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能删除资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

对于异常场景如username、uid改变或者用户被删除，ubse提供cli命令删除内存能力,具体请参考《ubsectl_mem.md》文档。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}

    char *name = "test_mem";
    ret = ubs_mem_numa_delete(name);
    if (SUCCESS != ret) {
		perror("delete numa mem failed.\n");
		return -1;
	}
	
    /* Do your work here... */
    
	ubs_engine_client_finalize();

	return 0;
}
```

# 15.ubs_mem_shm_create

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_create(const char *name, uint64_t size, uint8_t usr_info[32], uint64_t flag,
                           const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider);
```

## 描述 DESCRIPTION

创建共享形态的远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
|----------|--------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| name     | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| size     | IN     | 借用大小<br>单位Byte, 取值范围大于等于4\*1024*1024                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| usr_info | IN     | 调用方私有数据(当前有NC/CC信息), UBSE只负责保存，get时原样返回                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| flag     | IN     | 额外的内存借用属性，目前支持写接力、自动清理提供方和设置共享内存属性为CacheCoherent （按位组合，每一个二进制位表示一种独立属性）；<br /> **可用标志位定义如下**：<br /> `0x1`: 非写接力 <br />`0x2`: 匿名内存，共享内存没有使用方时，后台对账会自动清理 <br /> `0x4`: 设置共享内存属性为CacheCoherent (默认为NonCacheCoherent)  <br /> **flag使用说明(flag为十进制数)**:  <br /> flag 可以用 `\|` 运算进行赋值,表示开启某个属性，比如：  <br /> - 非写接力 + 匿名:`flag= 0x1 \| 0x2 = 3`; <br /> - 匿名+设置共享内存属性为CacheCoherent:`flag = 0x2 \| 0x4 = 6`  <br /> - 非写接力+匿名+设置共享内存属性为CacheCoherent:`flag = 0x1 \| 0x2 \| 0x4 = 7` <br />- 其它属性组合, 使用 `flag \|= 对应标志位` 进行组合即可<br />  **flag其它取值说明**: <br /> 0：默认值，代表三个标志位对应的属性都不选择 |
| region   | IN     | 使用共享内存的节点范围, region内的节点必须能通过公共节点直连, 必选参数                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| provier  | IN     | 资源提供方节点范围, null表示不指定, 非null表示指定                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |

- 数据结构说明

```c
typedef struct {
    uint32_t node_cnt;                    // 节点数
    uint32_t slot_ids[UBS_ENGINE_MAX_SLOT_NUM]; // 节点id列表
} ubs_mem_nodes_t;

#define UBS_MEM_MAX_USR_INFO_LEN 32
#define UBS_MEM_FLAG_NO_WR_DELAY 0x1   // 非写接力
#define UBS_MEM_FLAG_SHM_ANONYMOUS 0x2 // 共享内存没有使用方时, 后台对账时自动清理提供方
#define UBS_MEM_FLAG_CACHEABLE 0x4     // 设置共享内存属性为CacheCoherent
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description              |
| ------------------------- | ------------------------ |
| UBS_ERR_NULL_POINTER      | 空指针                   |
| UBS_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败       |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ERR_EXISTED           | 借用关系已存在           |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时       |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误       |

## 约束 CONSTRAINTS

资源使用权限分离与管理权限分离

使用者由操作系统的numa内存管理模块管理

资源的管理权限由ubse系统自动授予，权限属于该接口的调用者（使用OS的username/uid标识）

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

借出节点的决策原则：指定provider，则从中选择；没有指定provider，则从region中选择；确保所有节点均能使用共享内存

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

int main(void)
{
	int32_t ret;

    char *path = "/var/run/ubse/ubse.sock"
	ret = ubs_engine_client_initialize(path);
	if (SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}

    ubs_mem_nodes_t region = {
        .slot_ids = {1,2,3},
        .node_cnt = 3  
    };
    ubs_mem_nodes_t provier = {
        .slot_ids = {2},
        .node_cnt = 1  
    };

    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    uint64_t flag = UBS_MEM_FLAG_NO_WR_DELAY | UBS_MEM_FLAG_SHM_ANONYMOUS;
    uint64_t size = 1024 * 1024 * 1024;
    char *name = "test_mem";
    ret = ubs_mem_shm_create(name, size, usr_info, flag, &region, &provider);
    if (SUCCESS != ret) {
		perror("create shm mem failed.\n");
		return -1;
	}

    /* Do your work here... */

	ubs_engine_client_finalize();

	return 0;
}
```

# 16.ubs_mem_shm_attach

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_attach(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                           ubs_mem_shm_desc_t **shm_desc);
```

## 描述 DESCRIPTION

导入共享形态的远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                                  |
| -------- | ------ | ------------------------------------------------------------ |
| name     | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性 |
| owner    | IN     | 内存资源属主信息, 可选参数, Null不关注该字段                 |
| mode     | IN     | 内存资源的访问权限, 可选参数, 0则不关注该字段                |
| shm_desc | OUT    | 借用形成的远端fd信息,, 调用方需要使用free接口主动释放内存    |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubse自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;
#define UBS_TOPO_SOCKET_NUM 2
#define UBS_TOPO_IPADDR_NUM 50
#define UBS_TOPO_NUMA_NUM 2

typedef struct {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
} ubs_topo_ip_address_t;

typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBS_TOPO_SOCKET_NUM];
    uint32_t numa_ids[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubs_topo_node_t;

#define UBS_MEM_MAX_MEMID_NUM 2048
#define UBS_MEM_MAX_NAME_LENGTH 48
#define UBS_MEM_MAX_USR_INFO_LEN 32

typedef struct {
    uint32_t memid_cnt;  // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM];  // 内存块标识信息，FD的文件形成规则：UBSE_MEM_DEV_PATH + memid
    ubs_topo_node_t import_node;  // 借入节点
} ubs_mem_shm_import_desc_t;
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];  // 借用标识
    uint64_t mem_size;  // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;  // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;  // 借出节点
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN];
    uint32_t import_desc_cnt;
    ubs_mem_shm_import_desc_t* import_desc;
} ubs_mem_shm_desc_t;

```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description          |
| ------------------------- | -------------------- |
| UBS_ERR_NULL_POINTER      | 空指针               |
| UBS_ERR_OUT_OF_RANGE      | name超出范围         |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败   |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ERR_SHM_NO_CREATE     | 共享内存未创建       |
| UBS_ERR_CREATING          | 正在创建过程中       |
| UBS_ERR_DELETING          | 正在删除过程中       |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时   |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误   |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能导入资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
static void ubse_mem_share_attach_example(void)
{
    g_owner.uid = getuid();
    g_owner.gid = getgid();
    g_owner.pid = getpid();
    ubse_mem_shm_desc_t* shm_descs = NULL;

    printf("=== ubse_mem_share_attach_example ===\n");
    int ret = ubse_mem_shm_attach(g_shm_name, &g_owner, 0666, &shm_descs);
    if (ret != 0) {
        printf("ubse_mem_shm_attach failed, ret=%d\n", ret);
    } else {
        printf("ubse_mem_shm_attach success, name=%s\n", g_shm_name);
    }
}
```

# 17.ubs_mem_shm_get

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubse_mem_shm_get(const char* name, ubse_mem_shm_desc_t** shm_desc);
```

## 描述 DESCRIPTION

查询指定共享形态远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                                  |
| -------- | ------ | ------------------------------------------------------------ |
| name     | IN     | 借用标识前缀<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性 |
| shm_desc | OUT    | 借用形成的远端fd信息,, 调用方需要使用free接口主动释放内存    |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubse自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;
typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBSE_SOCKET_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubse_node_t;
typedef struct {
    uint32_t memid_cnt;  // 导出的内存块数量
    uint64_t memids[UBSE_MAX_MEMID_NUM];  // 内存块标识信息，FD的文件形成规则：UBSE_MEM_DEV_PATH + memid
    ubse_node_t import_node;  // 借入节点
} ubse_mem_shm_import_desc_t;
typedef struct {
    char name[UBSE_MAX_MEM_RESOURCE_NAME_LENGTH];  // 借用标识
    uint64_t mem_size;  // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;  // 芯片表项拆分粒度, 单位Byte
    ubse_node_t export_node;  // 借出节点
    uint8_t usr_info[UBSE_MAX_USR_INFO_LEN];
    uint32_t import_desc_cnt;
    ubse_mem_shm_import_desc_t* import_desc;
} ubse_mem_shm_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description          |
| ------------------------- | -------------------- |
| UBS_ERR_NULL_POINTER      | 空指针               |
| UBS_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败   |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ERR_NOT_EXIST         | 借用关系不存在       |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时   |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误   |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
static void ubse_mem_share_get_example(void)
{
    ubs_mem_shm_desc_t* shm_descs = NULL;
    printf("=== ubs_mem_share_get_example ===\n");
    const int ret = ubs_mem_shm_get("shm_mem", &shm_descs);
    if (ret != 0) {
        printf("ubs_mem_shm_get failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_get success\n");
        printf("name=%s, size=%lu, import_desc_cnt=%u\n", shm_descs->name, shm_descs->mem_size,
               shm_descs->import_desc_cnt);
        for (int i = 0; i < shm_descs->import_desc_cnt; i++) {
            printf("[%u] shm_descs=%u\n", i, shm_descs->import_desc[i].memid_cnt);
            printf("memid: ");
            for (int j = 0; j < shm_descs->import_desc[i].memid_cnt; j++) {
                printf("%lu ", shm_descs->import_desc[i].memids[j]);
            }
            printf("\n");
            printf("importNode: slot_id: %u, host_name: %s, socket_id: %p",
                   shm_descs->import_desc[i].import_node.slot_id, shm_descs->import_desc[i].import_node.host_name,
                   shm_descs->import_desc[i].import_node.socket_id);
            printf("\nmemid_node: ");
        }
        free(shm_descs);
    }
}
```

# 18. ubs_mem_shm_list

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_list(ubs_mem_shm_desc_t **shm_descs, uint32_t *shm_desc_cnt);
```

## 描述 DESCRIPTION

查询所有共享形态远端内存

## 参数 PARAMTERS

| name         | IN/OUT | description                                                  |
| ------------ | ------ | ------------------------------------------------------------ |
| shm_descs    | OUT    | 内存描述信息数组 <br />调用成功时, 调用方需要使用free接口主动释放内存 |
| shm_desc_cnt | OUT    | 内存描述信息数组中的元素个数                                 |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubse自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;
typedef struct {
    uint32_t slot_id;
    uint32_t socket_id[UBSE_SOCKET_NUM];
    char host_name[HOST_NAME_MAX]; // 主机名
} ubse_node_t;
typedef struct {
    uint32_t memid_cnt;  // 导出的内存块数量
    uint64_t memids[UBSE_MAX_MEMID_NUM];  // 内存块标识信息，FD的文件形成规则：UBSE_MEM_DEV_PATH + memid
    ubse_node_t import_node;  // 借入节点
} ubse_mem_shm_import_desc_t;
typedef struct {
    char name[UBSE_MAX_MEM_RESOURCE_NAME_LENGTH];  // 借用标识
    uint64_t mem_size;  // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;  // 芯片表项拆分粒度, 单位Byte
    ubse_node_t export_node;  // 借出节点
    uint8_t usr_info[UBSE_MAX_USR_INFO_LEN];
    uint32_t import_desc_cnt;
    ubse_mem_shm_import_desc_t* import_desc;
} ubse_mem_shm_desc_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description             |
| ------------------------- | ----------------------- |
| UBS_ERR_NULL_POINTER      | 空指针                  |
| UBS_ERR_OUT_OF_RANGE      | name_prefix参数超出范围 |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败      |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过    |
| UBS_ERR_NOT_EXIST         | 借用关系不存在          |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时      |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误      |

## 约束 CONSTRAINTS

ubse自动识别该接口调用方是否为资源管理者，只有管理者能查看资源

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
static void ubse_mem_share_list_example(void)
{
  ubs_mem_shm_desc_t* shm_descs = NULL;
    uint32_t shm_desc_cnt = 0;

    printf("=== ubs_mem_share_list_example ===\n");
    const int ret = ubs_mem_shm_list(&shm_descs, &shm_desc_cnt);
    if (ret != 0) {
        printf("ubs_mem_shm_list failed, ret=%d\n", ret);
        return;
    }
    printf("ubs_mem_shm_list success, cnt=%u\n", shm_desc_cnt);
    for (uint32_t i = 0; i < shm_desc_cnt; i++) {
        printf("name=%s, size=%lu, import_desc_cnt=%u\n", shm_descs[i].name, shm_descs[i].mem_size,
               shm_descs[i].import_desc_cnt);
        for (int j = 0; j < shm_descs[i].import_desc_cnt; j++) {
            printf("[%u] shm_descs=%u\n", i, shm_descs[i].import_desc[j].memid_cnt);
            printf("memid: ");
            for (int k = 0; k < shm_descs[i].import_desc[j].memid_cnt; k++) {
                printf("%lu ", shm_descs[i].import_desc[j].memids[k]);
            }
            printf("\n");
            printf("importNode: slot_id: %u, host_name: %s, socket_id: %p",
                   shm_descs[i].import_desc[j].import_node.slot_id,
                   shm_descs[i].import_desc[j].import_node.host_name,
                   shm_descs[i].import_desc[j].import_node.socket_id);
            printf("\nmemid_node: ");
        }
    }
    free(shm_descs);
}
```

# 19. ubs_mem_shm_detach

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_detach(const char *name);
```

## 描述 DESCRIPTION

删除导入共享形态的远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；

## 参数 PARAMTERS

| name | IN/OUT | description                                                  |
| ---- | ------ | ------------------------------------------------------------ |
| name | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description                |
| ------------------------- | -------------------------- |
| UBS_ERR_NULL_POINTER      | 空指针                     |
| UBS_ERR_OUT_OF_RANGE      | name参数超出范围           |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败         |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过       |
| UBS_ERR_SHM_NO_ATTACH     | 共享内存未导入             |
| UBS_ERR_SHM_ATTACHING     | 正在导入共享内存过程中     |
| UBS_ERR_SHM_DETACHING     | 正在删除导入共享内存过程中 |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时         |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误         |

## 约束 CONSTRAINTS

当前只支持CPU节点

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>

static void ubs_mem_share_detach_example(void)
{
    printf("=== ubs_mem_share_detach_example ===\n");
    int ret = ubs_mem_shm_detach(g_shm_name);
    if (ret != 0) {
        printf("ubs_mem_shm_detach failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_detach success, name=%s\n", g_shm_name);
    }
}

```

# 20.ubs_mem_shm_delete

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_mem.h>
int32_t ubs_mem_shm_delete(const char *name);
```

## 描述 DESCRIPTION

删除指定共享形态远端内存; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；

## 参数 PARAMTERS

| name | IN/OUT | description                                                  |
| ---- | ------ | ------------------------------------------------------------ |
| name | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name在节点内保持唯一性 |

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description          |
| ------------------------- | -------------------- |
| UBS_ERR_NULL_POINTER      | 空指针               |
| UBS_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败   |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ERR_NOT_EXIST         | 借用关系不存在       |
| UBS_ERR_CREATING          | 正在创建过程中       |
| UBS_ERR_DELETING          | 正在删除过程中       |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时   |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误   |

## 约束 CONSTRAINTS

当前只支持CPU节点

资源管理者确定时机：创建资源时UBSE自动确定和记录管理者（使用OS的username/uid标识）

管理者身份识别原则：能够获取username，则使用username；无法获取username，则使用uid

不同节点的用户名需要相同，否则节点间无法进行内存管理动作

对于异常场景如username、uid改变或者用户被删除，ubse提供cli命令删除内存能力,具体请参考《ubsectl_mem.md》文档。

## 附注 NOTES



## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <ubs_engine_mem.h>
static void ubs_mem_share_delete_example(void)
{
    printf("=== ubs_mem_share_delete_example ===\n");
    int ret = ubs_mem_shm_delete(g_shm_name);
    if (ret != 0) {
        printf("ubs_mem_shm_delete failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_delete success, name=%s\n", g_shm_name);
    }
}
```

# 21.ubs_mem_shm_create_with_affinity

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
int32_t ubs_mem_shm_create_with_affinity(const char *name, uint64_t size, uint32_t affinity_socket_id, uint8_t usr_info[32], uint64_t flag,
                           const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider);
```

## 描述 DESCRIPTION

在本节点创建指定cpu平面的共享numa形态的远端内存

## 参数 PARAMTERS

| name               | IN/OUT | description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
|--------------------|--------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| name               | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| size               | IN     | 借用大小<br>单位Byte, 取值范围大于等于4\*1024*1024                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |
| affinity_socket_id | IN     | 亲和的cpu socket_id                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| usr_info           | IN     | 调用方私有数据(当前有NC/CC信息), UBSE只负责保存，get时原样返回                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| flag               | IN     | 额外的内存借用属性，目前支持写接力、自动清理提供方和设置共享内存属性为CacheCoherent （按位组合，每一个二进制位表示一种独立属性）；<br /> **可用标志位定义如下**：<br /> `0x1`: 非写接力 <br />`0x2`: 匿名内存，共享内存没有使用方时，后台对账会自动清理 <br /> `0x4`: 设置共享内存属性为CacheCoherent (默认为NonCacheCoherent)  <br /> **flag使用说明(flag为十进制数)**:  <br /> flag 可以用 `\|` 运算进行赋值,表示开启某个属性，比如：  <br /> - 非写接力 + 匿名:`flag= 0x1 \| 0x2 = 3`; <br /> - 匿名+设置共享内存属性为CacheCoherent:`flag = 0x2 \| 0x4 = 6`  <br /> - 非写接力+匿名+设置共享内存属性为CacheCoherent:`flag = 0x1 \| 0x2 \| 0x4 = 7` <br />- 其它属性组合, 使用 `flag \|= 对应标志位` 进行组合即可<br />  **flag其它取值说明**: <br /> 0：默认值，代表三个标志位对应的属性都不选择 |
| region             | IN     | 使用共享内存的节点范围, region内的节点必须能通过公共节点直连, 必选参数                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| provier            | IN     | 资源提供方节点范围, null表示不指定, 非null表示指定                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |

- 数据结构说明
```c
typedef struct {
    uint32_t node_cnt;                    // 节点数
    uint32_t slot_ids[UBS_ENGINE_MAX_SLOT_NUM]; // 节点id列表
} ubs_mem_nodes_t;

#define UBS_MEM_MAX_USR_INFO_LEN 32
#define UBS_MEM_FLAG_NO_WR_DELAY 0x1   // 非写接力
#define UBS_MEM_FLAG_SHM_ANONYMOUS 0x2 // 共享内存没有使用方时, 后台对账时自动清理提供方
#define UBS_MEM_FLAG_CACHEABLE 0x4     // 设置共享内存属性为CacheCoherent
```
## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description      |
|---------------------------|------------------|
| UBS_ERR_NULL_POINTER      | 空指针              |
| UBS_ERR_OUT_OF_RANGE      | name或者size参数超出范围 |
|
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败      |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ERR_EXISTED           | 借用关系已存在          |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时      |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误      |
|UBSE_ERR_SHM_AFFINITY_PARAMS_ERROR| socket-id不存在|

## 附注 NOTES
无
## 样例 EXAMPLES
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // getuid, getgid, getpid
#include "ubs_engine_mem.h"
static const char *g_shm_name = "demo_shm";
static const uint64_t g_shm_size = 4 * 1024 * 1024; // 4MB
static ubs_mem_fd_owner_t g_owner;
static ubs_mem_shm_desc_t g_shm_desc;
static void ubs_mem_share_create_with_affinity_example(void)
{
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    ubs_mem_nodes_t region;
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    ubs_mem_nodes_t *provider = NULL;
    printf("=== ubs_mem_share_create_example ===\n");
    uint32_t affinity_socket_id = 36; // 这个需要查询CPU拓扑,找到app所在的cpu亲和的socket-id
    uint64_t flag = UBS_MEM_FLAG_NO_WR_DELAY | UBS_MEM_FLAG_SHM_ANONYMOUS;
    printf("socket-id=%d\n", affinity_socket_id);
    int ret =
        ubs_mem_shm_create_with_affinity(g_shm_name, g_shm_size, affinity_socket_id, usr_info, flag, &region, provider);
    if (ret != 0) {
        printf("ubs_mem_shm_create_with_affinity failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_create_with_affinity success, name=%s\n", g_shm_name);
    }
}

int main(void)
{
    ubs_mem_share_create_with_affinity_example();
}
```

# 22.ubs_mem_shm_list_with_prefix

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
int32_t ubs_mem_shm_list_with_prefix(const char *name_prefix, ubs_mem_shm_desc_t **shm_descs, uint32_t *shm_desc_cnt);
```

## 描述 DESCRIPTION

查询指定借用标识前缀的共享形态远端内存; 最多查询到2000条借用内存

## 参数 PARAMTERS

| name         | IN/OUT | description                            |
|--------------|--------|----------------------------------------|
| name_prefix  | IN     | name_prefix                            |
| shm_descs    | OUT    | 共享内存描述信息数组, 调用成功时, 调用方需要使用free接口主动释放内存 |
| shm_desc_cnt | OUT    | 共享内存描述信息数组中的元素个数, 范围 [0, UBS_MEM_MAX_DESC_LIST]         |

- 数据结构说明
```c
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH]; // 借用标识
    uint64_t mem_size;                  // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                   // 芯片表项拆分粒度, 单位Byte
    ubs_topo_node_t export_node;        // 借出节点, 其中ips字段无效, 需通过topo接口获取
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN];
    uint32_t import_desc_cnt;               // 导入内存描述符信息的数量, 范围 [0, UBS_TOPO_MAX_NODE_NUM]
    ubs_mem_stage mem_stage;                // 内存状态
    ubs_mem_shm_import_desc_t *import_desc; // 导入内存描述符信息
} ubs_mem_shm_desc_t;

#define UBS_MEM_MAX_DESC_LIST 2000     // 设置共享内存属性为CacheCoherent
```
## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description      |
|---------------------------|------------------|
| UBS_ERR_NULL_POINTER      | 空指针              |
| UBS_ERR_OUT_OF_RANGE      | name参数超出范围 |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败      |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过     |
| UBS_ENGINE_ERR_NOT_EXIST  | 借用关系不存在          |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时      |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误      |

## 附注 NOTES
无
## 样例 EXAMPLES
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ubs_engine_mem.h"

static void ubs_mem_shm_list_with_prefix_example(void)
{
    const char *name_prefix = "demo";
    ubs_mem_shm_desc_t *shm_descs = nullptr;
    uint32_t shm_desc_cnt = 0;
    int ret =
        ubs_mem_shm_list_with_prefix(name_prefix, &shm_descs, &shm_descs);
    if (ret != 0) {
        printf("ubs_mem_shm_list_with_prefix failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_list_with_prefix success, name=%s\n", name_prefix);
    }
}

int main(void)
{
    ubs_mem_shm_list_with_prefix_example();
}
```

# 23.ubs_mem_shm_fault_get

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
int32_t ubs_mem_shm_fault_get(const char *name, ubs_mem_memids_fault_t *fault);
```

## 描述 DESCRIPTION

查询指定共享远端内存的状态; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同

## 参数 PARAMTERS

| name       | IN/OUT | description                                                             |
|------------|--------|-------------------------------------------------------------------------|
| name       | IN     | 借用标识, 最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name全局保持唯一性 |
| shm_status | OUT    | 内存块的健康状态                                                                |

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
    UB_MEM_HEALTHY = 1000, // 无故障
} ubs_mem_fault_type_t;

typedef struct {
    uint32_t memid_cnt;                                       // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM];                   // 导出内存块标识信息
    ubs_mem_fault_type_t memid_status[UBS_MEM_MAX_MEMID_NUM]; // 内存块的健康状态，与memids一一对应
} ubs_mem_memids_fault_t;
```
## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description |
|---------------------------|-------------|
| UBS_ERR_NULL_POINTER      | 空指针         |
| UBS_ERR_OUT_OF_RANGE      | name参数超出范围  |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败 |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ENGINE_ERR_NOT_EXIST  | 借用关系不存在     |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时 |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误 |

## 附注 NOTES
无
## 样例 EXAMPLES
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ubs_engine_mem.h"

static void ubs_mem_shm_fault_get_example(void)
{
    const char *name = "demo_shm";
    ubs_mem_memids_fault_t fault_info;
    int ret = ubs_mem_shm_fault_get(name, &fault_info);
    if (ret != 0) {
        printf("ubs_mem_shm_fault_get failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_fault_get success, name=%s\n", name);
    }
}

int main(void)
{
    ubs_mem_shm_fault_get_example();
}
```

# 24.ubs_mem_shm_fault_register

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
int32_t ubs_mem_shm_fault_register(ubs_mem_shm_fault_handler handler);
```

## 描述 DESCRIPTION

查询指定共享远端内存的状态; 调用该接口能操控的资源：创建资源时的标识与本次调用方的标识相同

## 参数 PARAMTERS

| name    | IN/OUT | description          |
|---------|--------|----------------------|
| handler | IN     | 共享内存UB Event事件响应处理函数 |

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
    UB_MEM_HEALTHY = 1000, // 无故障
} ubs_mem_fault_type_t;

typedef int32_t (*ubs_mem_shm_fault_handler)(const char *name, uint64_t memid, ubs_mem_fault_type_t type);
```
## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                     | Description |
|---------------------------|-------------|
| UBS_ERR_NULL_POINTER      | 空指针         |
| UBS_ERR_CONNECTION_FAILED | 连接UBSE服务端失败 |
| UBS_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过 |
| UBS_ERR_TIMEOUT           | UBSE服务端处理超时 |
| UBS_ERR_INTERNEL          | UBSE服务端内部错误 |

## 附注 NOTES
无
## 样例 EXAMPLES
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ubs_engine_mem.h"

int32_t ubs_mem_shm_fault_handler_demo(const char *name, uint64_t memid, ubs_mem_fault_type_t type)
{
    printf("receive fault event, name=%s, mem id=%d, fault type=%d\n", name, memid, type);
}

static void ubs_mem_shm_fault_register_example(void)
{
    int ret = ubs_mem_shm_fault_register(ubs_mem_shm_fault_handler_demo);
    if (ret != 0) {
        printf("ubs_mem_shm_fault_register failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_fault_register success\n");
    }
}

int main(void)
{
    ubs_mem_shm_fault_register_example();
}
```

# 25.ubs_mem_shm_create_with_lender

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
int32_t ubs_mem_shm_create_with_lender(const char *name, uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                       const ubs_mem_nodes_t *region, const ubs_mem_lender_t *lender);
```

## 描述 DESCRIPTION

在本节点创建指定借出方的共享numa形态的远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
|----------|--------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| name     | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name仅可包括大小写字母、数字、"."、":"、"-"以及"_"<br>name全局保持唯一性                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| usr_info | IN     | 调用方私有数据(当前有NC/CC信息), UBSE只负责保存，get时原样返回                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| flag     | IN     | 额外的内存借用属性，目前支持写接力、自动清理提供方和设置共享内存属性为CacheCoherent （按位组合，每一个二进制位表示一种独立属性）；<br /> **可用标志位定义如下**：<br /> `0x1`: 非写接力 <br />`0x2`: 匿名内存，共享内存没有使用方时，后台对账会自动清理 <br /> `0x4`: 设置共享内存属性为CacheCoherent (默认为NonCacheCoherent)  <br /> **flag使用说明(flag为十进制数)**:  <br /> flag 可以用 `\|` 运算进行赋值,表示开启某个属性，比如：  <br /> - 非写接力 + 匿名:`flag= 0x1 \| 0x2 = 3`; <br /> - 匿名+设置共享内存属性为CacheCoherent:`flag = 0x2 \| 0x4 = 6`  <br /> - 非写接力+匿名+设置共享内存属性为CacheCoherent:`flag = 0x1 \| 0x2 \| 0x4 = 7` <br />- 其它属性组合, 使用 `flag \|= 对应标志位` 进行组合即可<br />  **flag其它取值说明**: <br /> 0：默认值，代表三个标志位对应的属性都不选择 |
| region   | IN     | 使用共享内存的节点范围, 为nullptr则为当前集群所有节点,指定时,region内的节点必须能通过公共节点直连                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| lender   | IN     | 指定借出节点数据,必填参数,支持赋值方式见附注                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |

- 数据结构说明

```c
typedef struct {
    uint32_t node_cnt;                    // 节点数
    uint32_t slot_ids[UBS_ENGINE_MAX_SLOT_NUM]; // 节点id列表
} ubs_mem_nodes_t;

typedef struct {
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;   // socket id
    uint32_t numa_id;     // 节点中的numa id
    uint32_t port_id;     // 指定链路借用
} ubs_mem_lender_t;

#define UBS_MEM_MAX_USR_INFO_LEN 32
#define UBS_MEM_FLAG_NO_WR_DELAY 0x1   // 非写接力
#define UBS_MEM_FLAG_SHM_ANONYMOUS 0x2 // 共享内存没有使用方时, 后台对账时自动清理提供方
#define UBS_MEM_FLAG_CACHEABLE 0x4     // 设置共享内存属性为CacheCoherent
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                              | Description      |
|------------------------------------|------------------|
| UBS_ERR_NULL_POINTER               | 空指针              |
| UBS_ERR_OUT_OF_RANGE               | name或者size参数超出范围 |
| UBS_ERR_CONNECTION_FAILED          | 连接UBSE服务端失败      |
| UBS_ERR_AUTH_FAILED                | UBSE服务端鉴权不通过     |
| UBS_ERR_EXISTED                    | 借用关系已存在          |
| UBS_ERR_TIMEOUT                    | UBSE服务端处理超时      |
| UBS_ERR_INTERNEL                   | UBSE服务端内部错误      |
| UBSE_ERR_SHM_AFFINITY_PARAMS_ERROR | socket-id不存在     |

## 附注 NOTES
ubs_mem_lender_t结构体内部字段支持以下赋值,无效值时填UINT32_MAX

| lender_size(必传) | slot_id(必传) | socket_id(可选) | numa_id(可选) | port_id(可选) | 预期结果             |
|-----------------|-------------|---------------|-------------|-------------|------------------|
| √               | √           |               |             |             | 根据slot_id决策      |
| √               | √           | √             |             |             | 根据socket_id决策    |
| √               | √           |               | √           |             | 根据numa_id决策      |
| √               | √           |               |             | √           | 不支持该格式           |
| √               | √           | √             | √           |             | 校验合法性,根据numa_id决策 |
| √               | √           |               | √           | √           | 根据numa_id决策      |
| √               | √           | √             |             | √           | 根据socket_id决策    |
| √               | √           | √             | √           | √           | 校验合法性,根据numa_id决策 |

合法性校验：对应关系是否符合硬件环境，校验失败借用失败

## 样例 EXAMPLES

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // getuid, getgid, getpid
#include "ubs_engine_mem.h"
static const char *g_shm_name = "demo_shm";
static const uint64_t g_shm_size = 4 * 1024 * 1024; // 4MB
static ubs_mem_fd_owner_t g_owner;
static ubs_mem_shm_desc_t g_shm_desc;
static void ubs_mem_share_create_with_lender_example(void)
{
    uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region;
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    ubs_mem_lender_t lender;
    lender.slot_id = 2;
    lender.numa_id = 1;
    lender.socket_id = UINT32_MAX;
    lender.port_id = UINT32_MAX;
    lender.lender_size = 128 * 1024 * 1024;

    printf("=== ubs_mem_share_create_with_lender_example ===\n");
    int ret = ubs_mem_shm_create_with_lender(g_shm_name, usr_info, 0, &region, &lender);
    if (ret != 0) {
        printf("ubs_mem_shm_create_with_lender failed, ret=%d\n", ret);
    } else {
        printf("ubs_mem_shm_create_with_lender success, name=%s\n", g_shm_name);
    }
}

int main(void)
{
    ubs_mem_share_create_with_lender_example();
}
```