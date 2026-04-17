# UBSE URMA Controller SDK 接口说明文档

---

## 概述

UBSE URMA Controller 提供一组用于管理 URMA 设备的接口，支持设备的查询和分配功能。

### 接口列表

| 功能 | 接口 | 描述 |
|------|------|------|
| 设备查询 | `ubs_urma_dev_get` | 查询系统中所有 URMA 设备信息 |
| 设备分配 | `ubs_urma_dev_alloc` | 分配指定的 URMA 设备资源 |
| 设备释放 | `ubs_urma_dev_free` | 释放已分配的 URMA 设备资源 |

---

## 错误码汇总

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

# 1. ubs_urma_dev_get

## 库 LIBRARY

**ubse 库** (`libubse.so`)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_urma.h>

uint32_t ubs_urma_dev_get(ubs_urma_dev_t **urma_devices, uint32_t *urma_cnt);
```

## 描述 DESCRIPTION

查询系统中所有可用的 URMA 设备信息，返回设备列表，包括设备名称、健康状态和硬件资源 ID 等信息。

## 参数 PARAMETERS

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

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过   |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误     |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

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

# 2. ubs_urma_dev_alloc

## 库 LIBRARY

**ubse 库** (`libubse.so`)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_urma.h>

uint32_t ubs_urma_dev_alloc(const char *name, ubs_urma_dev_info_t *dev_info);
```

## 描述 DESCRIPTION

分配指定的 URMA 设备资源，返回设备的路径信息，包括 bonding 路径、EID 和 FE 路径等。

## 参数 PARAMETERS

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

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ENGINE_ERR_NOT_EXIST         | URMA设备不存在       |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过   |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误     |

## 约束 CONSTRAINTS

- `dev_info` 参数需由调用方预先申请内存
- 设备名称长度不能超过 32 字节

## 附注 NOTES

暂无

## 样例 EXAMPLES

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

# 3. ubs_urma_dev_free

## 库 LIBRARY

**ubse 库** (`libubse.so`)

## 摘要 SYNOPSIS

```c
#include <ubs_engine_urma.h>

uint32_t ubs_urma_dev_free(const char *name);
```

## 描述 DESCRIPTION

释放指定的 URMA 设备资源，使其可以被其他进程重新分配使用。

## 参数 PARAMETERS

| name | IN/OUT | 描述                          |
|------|--------|-------------------------------|
| name | IN     | URMA 设备名称，长度不超过 32 字节 |

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description        |
|----------------------------------|--------------------|
| UBS_ERR_NULL_POINTER             | 空指针              |
| UBS_ENGINE_ERR_OUT_OF_RANGE      | name参数超出范围     |
| UBS_ENGINE_ERR_NOT_EXIST         | URMA设备不存在       |
| UBS_ENGINE_ERR_CONNECTION_FAILED | 连接UBSE服务端失败    |
| UBS_ENGINE_ERR_AUTH_FAILED       | UBSE服务端鉴权不通过   |
| UBS_ENGINE_ERR_TIMEOUT           | UBSE服务端处理超时     |
| UBS_ENGINE_ERR_INTERNAL          | UBSE服务端内部错误     |

## 约束 CONSTRAINTS

- 只能释放已分配的设备
- 设备名称长度不能超过 32 字节

## 附注 NOTES

暂无

## 样例 EXAMPLES

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
