# 1. ubs_mem_numastat_get

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t **numa_mems, uint32_t *numa_mem_cnt);
```

## 描述 DESCRIPTION

查询指定节点numa信息，当前只支持本地numa内存，后续会增加远端numa

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
    uint32_t slot_id;            // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;          // socket id
    uint32_t numa_id;            // 节点中的numa id
    ubs_mem_numa_type_t numa_type;  // numa类型
    uint32_t mem_lend_ratio;     // 池化内存借出比例上限
    uint64_t mem_total;          // 内存总量, 单位字节
    uint64_t mem_free;           // 内存空闲量, 单位字节
    uint32_t huge_pages_2M;      // 2M大页数量
    uint32_t free_huge_pages_2M; // 2M大页空闲数量
} ubs_mem_numastat_t;
```

## 返回值 RETURN VALUE

返回`UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description          |
|----------------------------------|----------------------|
| UBS_ERR_NULL_POINTER             | 空指针                |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | node_id长度超108字节   |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败      |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过    |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时      |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误      |

## 约束 CONSTRAINTS

当前只支持本地numa内存

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <libubse_mem.h>

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

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_fd_create(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                           ubs_mem_distance_t distance, ubs_mem_fd_desc_t *fd_desc);
```

## 描述 DESCRIPTION

在本节点创建fd形态的远端内存

## 参数 PARAMTERS

| name     | IN/OUT | description                                                  |
| -------- | ------ | ------------------------------------------------------------ |
| name     | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name在节点内保持唯一性 |
| size     | IN     | 借用大小<br>单位Byte, 取值范围[`128*1024*1024`, `256*1024*1024*1024`] |
| owner    | IN     | 内存资源使用者属主信息, 可选参数, Null不关注该字段           |
| mode     | IN     | 内存资源使用者访问权限, 可选参数, 0则不关注该字段            |
| distance | IN     | 内存访问距离                                                 |
| fd_desc  | OUT    | 内存描述信息                                                 |

- 数据结构说明

```c
// 使用方进程信息
typedef struct {
    uid_t uid; // 属主进程的运行用户的uid
    gid_t gid; // 属主进程的的运行用户的groupid
    pid_t pid; // 属主进程的的运行用户的pid, 指定pid，pid消失时, ubse自动释放借用内存(暂不提供)
} ubs_mem_fd_owner_t;

typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
} ubs_mem_distance_t;

typedef struct {
    uint32_t slot_id;     // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socket_id;   // socket id
    uint64_t numa_id;     // 节点中的numa id
    uint64_t lender_size; // 借出内存大小, 单位Byte, 取值范围[128*1024*1024, 256*1024*1024*1024]
} ubs_mem_lender_t;

// 内存借用状态枚举
#define UBS_MEM_DEV_NAME_PREFIX "obmm_shmdev"
#define UBS_MEM_DEV_PATH "/dev/" UBS_MEM_DEV_NAME_PREFIX
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];        // 借用标识
    uint32_t memid_cnt;                        // 导出的内存块数量
    uint64_t memids[UBS_MEM_MAX_MEMID_NUM];    // 内存块标识信息，FD的文件形成规则：UBS_MEM_DEV_PATH + memid
    uint64_t mem_size;                         // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                          // 芯片表项拆分粒度, 单位Byte
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

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <libubse_mem.h>

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

# 3. ubs_mem_fd_get()

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_fd_get(const char *name, ubs_mem_fd_desc_t *fd_desc);
```

## 描述 DESCRIPTION

查询本节点fd形态远端内存信息

## 参数 PARAMTERS

| name               | IN/OUT | description                                                        |
| ------------------ | ------ | ------------------------------------------------------------------ |
| name               | IN     | 借用标识<br>name最大长度48字节, 含结尾字符\0<br>name在节点内保持唯一性 |
| fd_desc            | OUT    | fd内存信息                                                          |

- 数据结构说明

```c
typedef struct {
    char name[UBSE_MAX_MEM_RESOUCE_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                              // 导出的内存块数量
    uint64_t memids[UBSE_MAX_MEMID_NUM];             // 内存块标识信息
    uint64_t mem_size;                               // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                                // 芯片表项拆分粒度, 单位Byte
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

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_mem.h>

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

# 4. ubs_mem_fd_list

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
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
typedef struct {
    char name[UBSE_MAX_MEM_RESOUCE_NAME_LENGTH];     // 借用标识
    uint32_t memid_cnt;                              // 导出的内存块数量
    uint64_t memids[UBSE_MAX_MEMID_NUM];             // 内存块标识信息
    uint64_t mem_size;                               // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
    size_t unit_size;                                // 芯片表项拆分粒度, 单位Byte

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

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_mem.h>

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

# 5. ubs_mem_fd_delete

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_fd_delete(const char *name);
```

## 描述 DESCRIPTION

删除本节点指定fd远端内存

## 参数 PARAMTERS

| name | IN/OUT | description                                                  |
| ---- | ------ | ------------------------------------------------------------ |
| name | IN     | 借用标识, name最大长度48字节, 含结尾字符\0; name在节点内保持唯一性 |

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

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <unistd.h>
#include <libubse_mem.h>

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

# 6. ubs_mem_numa_create

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_numa_create(const char *name, uint64_t size, ubs_mem_distance_t distance,
                             ubs_mem_numa_desc_t *numa_desc);
```

## 描述 DESCRIPTION

创建numa形态的远端内存

## 参数 PARAMTERS

| name      | IN/OUT | description                                                  |
| --------- | ------ | ------------------------------------------------------------ |
| name      | IN     | 借用标识, name最大长度48字节, 含结尾字符\0; name节点内保持唯一性 |
| size      | IN     | 借用大小, 单位Byte, 取值范围[`1024*1024`, `1024*1024*1024*100`] |
| distance  | IN     | 内存访问跳数                                                 |
| numa_desc | OUT    | 借用形成的远端numa信息                                       |

- 数据结构说明

```c
typedef enum {
    MEM_DISTANCE_L0, // L0对应直连节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
} ubs_mem_distance_t;

typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];   // 借用标识
    int64_t numaid;                       // 形成远端numa对应的numaid
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

调用者的标识处理原则：能够获取username，则使用username；无法获取username，则使用uid；

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_mem.h>

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

# 7. ubs_mem_numa_get

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_numa_get(const char *name, ubs_mem_numa_desc_t *numa_desc);
```

## 描述 DESCRIPTION

查询本节点numa形态远端内存信息

## 参数 PARAMTERS

| name                 | IN/OUT | description                                                  |
| -------------------- | ------ | ------------------------------------------------------------ |
| name                 | IN     | 借用标识, name最大长度48字节, 含结尾字符\0; name节点内保持唯一性 |
| numa_desc            | OUT    | 借用形成的远端numa信息                                         |

- 数据结构说明

```c
typedef struct {
    char name[UBS_MEM_MAX_NAME_LENGTH];  // 借用标识
    int64_t numaid;                      // 形成远端numa对应的numaid
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
#include <libubse_mem.h>

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

# 8. ubs_mem_numa_list

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
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
    char name[UBS_MEM_MAX_NAME_LENGTH];  // 借用标识
    int64_t numaid;                      // 形成远端numa对应的numaid
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

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_mem.h>

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

# 9. ubs_mem_numa_delete

## 库 LIBRARY

ubse库 (libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <libubse_mem.h>
int32_t ubs_mem_numa_delete(const char *name);
```

## 描述 DESCRIPTION

删除指定numa远端内存

## 参数 PARAMTERS

| name | IN/OUT | description                                                  |
| ---- | ------ | ------------------------------------------------------------ |
| name | IN     | 借用标识, name最大长度48字节, 含结尾字符\0; name节点内保持唯一性 |

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

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <libubse_mem.h>

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
