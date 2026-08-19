# UBS Engine Python SDK API 参考指南

## 前言

**概述**

UBS Engine（UBSE）提供了 UBSE 程序及其相应的 SDK 开发库。开发者可以利用该 SDK 访问 UBSE 提供的服务，从而实现对存储等资源的调度与管理。

UBSE SDK 通过三种形式提供：

- so 动态链接库：C 和 C++ 语言的开发。
- Python 开发库：支持 python 协议栈开发。
- Go SDK 源码库：支持 Go 协议栈通过 import 导入开发。

**介绍**

本文主要介绍 UBS Engine Python SDK 对外提供的 API。Python SDK 通过 IPC 直接与 UBSE 守护进程通信，封装了通信细节，向上层应用暴露一组 Python 函数。SDK 按子系统划分为多个模块，每个模块对应一个章节，当前包含以下模块：

- `ubse.ubs_engine_ssu`：SSU（Storage Service Unit，存储服务单元）子系统，提供存储空间分配、查询、挂载/卸载、访问权限管理及 FE/VFE 设备管理等功能。

后续可能扩展其他子系统的 Python SDK 文档。

**约束条件**

UBS Engine 对外接口的访问入口是 sock 文件 `/run/ubse/ubse.sock`，sock 文件的安全访问基于文件权限控制，其属主为 ubse，mode 为 660，如下：

```shell
srw-rw---- ubse ubse /run/ubse/ubse.sock
```

加入 ubse group 的用户，才能访问 UBSE 的 SDK 接口。所有函数为阻塞式调用，调用失败时抛出异常。

## ubse.ubs\_engine\_ssu

**模块 MODULE**

`ubse.ubs_engine_ssu` (源文件 `src/sdk/python/ubs_engine_ssu.py`)

**依赖**

| 依赖模块 | 用途 |
| -------- | ---- |
| `ubse.ipc.ubs_engine_ipc` | 提供 `invoke_call` 用于发起 IPC 请求 |
| `ubse.models.ubs_engine_model_ssu` | 提供 SSU 数据模型（请求/响应结构体、常量、枚举） |
| `ubse.ffi.ubs_binary_codec` | 提供二进制响应解包能力（`BinaryUnpacker`） |
| `ubse.ipc.ubs_engine_ipc_codes` | 提供 SSU 模块及操作的 IPC 操作码 |
| `ubse.ffi.ubs_engine_binding_ssu` | 提供请求打包/响应解包/参数校验的 FFI 绑定函数 |
| `ubse.ffi.ubs_engine_exceptions` | 提供所有对外暴露的异常类 |

**导入示例**

```python
from ubse.ubs_engine_ssu import (
    ubs_ssu_alloc_info_list,
    ubs_ssu_space_alloc,
    ubs_ssu_space_free,
    ubs_ssu_access_permission_add,
    ubs_ssu_access_permission_remove,
    ubs_ssu_space_attach,
    ubs_ssu_space_detach,
    ubs_ssu_linear_space_attach,
    ubs_ssu_linear_space_detach,
    ubs_ssu_striped_space_attach,
    ubs_ssu_striped_space_detach,
    ubs_ssu_ns_stats_get,
    ubs_ssu_connect_info_get,
    ubs_ssu_fe_device_list,
    ubs_ssu_fe_device_alloc,
    ubs_ssu_fe_device_free,
)

from ubse.models.ubs_engine_model_ssu import (
    UbsSsuAllocSpaceReq,
    UbsSsuSpaceReq,
    UbsSsuLinearSpaceReq,
    UbsSsuStripedSpaceReq,
    UbsUbVfe,
    UbsSsuLbaFormat,
    UbsSsuAllocStrategy,
    UbsSsuRaidLevel,
    UbsSsuChunkSize,
)
```

**异常体系**

本模块所有公开函数在调用失败时均会抛出异常。异常类统一定义在 `ubse.ffi.ubs_engine_exceptions` 模块，所有异常均派生自 `UbsError`（`Exception` 的子类）。

| 异常类 | 基类 | 语义说明 |
| ------ | ---- | -------- |
| `UbsErrInvalidArg` | `UbsError` | 无效参数（参数格式/长度/取值校验未通过） |
| `UbsEngineConnectionError` | `UbsError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | `UbsError` | UBSE 服务端鉴权不通过 |
| `UbsEngineTimeoutError` | `UbsError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | `UbsError` | UBSE 服务端内部错误 |
| `UbsEngineOutOfRangeError` | `UbsError` | `name` 等参数超出范围 |
| `UbsEngineNotExistError` | `UbsError` | 借用关系/存储空间/VFE/虚拟机等不存在 |
| `UbsEngineExistedError` | `UbsError` | 实例已存在 |
| `UbsEngineAllocateError` | `UbsError` | 算法分配失败 |
| `UbsEngineAlreadyAttachedError` | `UbsError` | 空间已挂载，重复挂载报错；异常 `data` 属性为已挂载的设备路径列表 |

> 捕获建议：上层调用方可以直接捕获 `UbsError` 以覆盖全部异常场景，也可分别捕获特定子类以进行差异化处理。

**常量**

本模块对外暴露的常量从 `ubse.models.ubs_engine_model_ssu` 导入，与 C 头文件 `ubs_engine_ssu.h` 保持一致。

| 常量名 | 值 | 说明 |
| ------ | -- | ---- |
| `UBS_SSU_MAX_NAME_LENGTH` | 48 | 请求标识最大 48 个字符，含结尾字符 `'\0'` |
| `UBS_SSU_MAX_NQN_LENGTH` | 69 | NVMe NQN 最大长度 69 个字符，含结尾字符 `'\0'` |
| `UBS_SSU_GUID_LENGTH` | 32 | GUID 最大长度 32 个字符，不含结尾字符 `'\0'` |

> 其余长度/容量常量（如 `UBS_SSU_MAX_EID_LENGTH`、`UBS_SSU_MAX_UUID_LENGTH` 等）亦从 `ubse.models.ubs_engine_model_ssu` 导入，详见该模块。

**类型定义**

本模块对外暴露的请求/响应数据类均从 `ubse.models.ubs_engine_model_ssu` 导入，使用 `@dataclass` 定义。

`UbsSsuAllocSpaceReq`：分配存储空间请求参数。

| 字段 | 类型 | 默认值 | 说明 |
| ---- | ---- | ------ | ---- |
| `name` | `str` | `""` | 请求标识，最大 48 个字符 |
| `ns_size` | `int` | `0` | 申请总容量，单位字节 |
| `ns_num` | `int` | `1` | 命名空间数量，等于 1 时 `strategy` 不生效 |
| `lba_format` | `UbsSsuLbaFormat` | `FORMAT_512` | LBA 格式 |
| `strategy` | `UbsSsuAllocStrategy` | `NORMAL` | 分配策略 |
| `tenant` | `str` | `""` | 请求方 tenant（租户隔离标识） |

`UbsSsuSpaceReq`：挂载 / 卸载存储空间请求参数。

| 字段 | 类型 | 默认值 | 说明 |
| ---- | ---- | ------ | ---- |
| `name` | `str` | `""` | 需挂载 / 卸载的存储空间标识，最大 48 个字符 |
| `nqn` | `str` | `""` | Host 的 NVMe Qualified Name |
| `src_eid` | `str` | `""` | 源 EID |

`UbsSsuLinearSpaceReq`：挂载 / 卸载线性编址存储空间请求参数。

| 字段 | 类型 | 默认值 | 说明 |
| ---- | ---- | ------ | ---- |
| `name` | `str` | `""` | 需挂载 / 卸载的存储空间标识 |
| `nqn` | `str` | `""` | Host 的 NVMe Qualified Name |
| `src_eid` | `str` | `""` | 源 EID |
| `dev_name` | `str` | `""` | 聚合后的块设备名称，由外部指定 |

`UbsSsuStripedSpaceReq`：挂载 / 卸载条带化编址存储空间请求参数。

| 字段 | 类型 | 默认值 | 说明 |
| ---- | ---- | ------ | ---- |
| `name` | `str` | `""` | 需挂载 / 卸载的存储空间标识 |
| `nqn` | `str` | `""` | Host 的 NVMe Qualified Name |
| `src_eid` | `str` | `""` | 源 EID |
| `dev_name` | `str` | `""` | 聚合后的块设备名称，由外部指定 |
| `level` | `UbsSsuRaidLevel` | `RAID0` | RAID 级别 |
| `chunk_size` | `UbsSsuChunkSize` | `CHUNK_4K` | chunk 大小，单位 KB |

`UbsUbVfe`：虚拟功能单元（VFE）信息。

| 字段 | 类型 | 默认值 | 说明 |
| ---- | ---- | ------ | ---- |
| `slot_id` | `int` | `0` | Slot ID |
| `chip_id` | `int` | `0` | Chip ID |
| `die_id` | `int` | `0` | Die ID |
| `pfe_id` | `int` | `0` | 所属 PFE ID |
| `vfe_id` | `int` | `0` | VFE ID |
| `vfe_guid` | `str` | `""` | VFE GUID |
| `bind_bus_instance_guid` | `str` | `""` | 绑定的总线实例 GUID |

> 其余响应数据类（`UbsSsuAllocResult`、`UbsSsuNamespaceInfo`、`UbsSsuConnectInfo`、`UbsSsuNsStats`、`UbsUbFe` 等）及枚举类型（`UbsSsuLbaFormat`、`UbsSsuRaidLevel`、`UbsSsuChunkSize`、`UbsSsuAllocStrategy`）详见 `ubse.models.ubs_engine_model_ssu`。

### ubs\_ssu\_alloc\_info\_list

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_alloc_info_list() -> List[UbsSsuAllocResult]
```

**描述 DESCRIPTION**

获取系统中所有已分配的 SSU 存储空间详细信息，包括命名空间列表、容量、LBA 格式和使用类型等。

**参数 PARAMETERS**

无参数。

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `List[UbsSsuAllocResult]` | 已分配空间信息列表 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

无。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```python
from ubse.ubs_engine_ssu import ubs_ssu_alloc_info_list

results = ubs_ssu_alloc_info_list()
for r in results:
    print(f"name={r.name}, ns_cnt={len(r.namespaces)}")
```

### ubs\_ssu\_space\_alloc

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_space_alloc(req: UbsSsuAllocSpaceReq) -> UbsSsuAllocResult
```

**描述 DESCRIPTION**

根据请求参数分配指定数量和大小的命名空间，支持分布式分配（`STRIPED`）、顺序分配（`LINEAR`）和普通分配（`NORMAL`）三种策略。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuAllocSpaceReq` | 是 | 分配请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `UbsSsuAllocResult` | 分配结果，包含已分配的命名空间信息列表 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineExistedError` | 存储空间已分配，重复分配报错 |
| `UbsEngineAllocateError` | 算法分配失败 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `ns_num` 必须大于 0。
- `ns_num` 为 1 时，`strategy` 不能为 `STRIPED`（仅一个命名空间无法条带化）。
- `ns_size` 必须大于 0 且为 1G（`1024*1024*1024`）的整数倍。
- 条带化策略时 `ns_size` 需整除 `ns_num`。
- `lba_format` 必须为 `FORMAT_512`（512）或 `FORMAT_4K`（4096）。
- `strategy` 必须为 `STRIPED`（0）、`LINEAR`（1）或 `NORMAL`（2）。
- `tenant` 允许为空；非空时长度不超过 `UBS_SSU_MAX_TENANT_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- 空间已分配时重复分配将报错，不再幂等返回成功。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```python
from ubse.ubs_engine_ssu import ubs_ssu_space_alloc
from ubse.models.ubs_engine_model_ssu import (
    UbsSsuAllocSpaceReq, UbsSsuLbaFormat, UbsSsuAllocStrategy,
)

req = UbsSsuAllocSpaceReq(
    name="space-001",
    ns_size=1 * 1024 * 1024 * 1024,  # 1 GiB
    ns_num=2,
    lba_format=UbsSsuLbaFormat.FORMAT_4K,
    strategy=UbsSsuAllocStrategy.STRIPED,
)
result = ubs_ssu_space_alloc(req)
print(f"alloc success, name={result.name}, ns_cnt={len(result.namespaces)}")
```

### ubs\_ssu\_space\_free

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_space_free(name: str) -> None
```

**描述 DESCRIPTION**

释放之前通过 `ubs_ssu_space_alloc` 分配的存储空间及其关联的所有命名空间。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `name` | `str` | 是 | 要释放的存储空间标识 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 存储空间不存在或已释放，无需释放报错 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- 释放操作不再幂等，释放不存在的空间将报错。

**附注 NOTES**

暂无。

### ubs\_ssu\_access\_permission\_add

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_access_permission_add(name: str, nqn: str) -> None
```

**描述 DESCRIPTION**

为指定的 Host 授予对已分配存储空间的访问权限，在 Target 侧将 Host NQN 添加到子系统的允许主机列表中。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `name` | `str` | 是 | 存储空间标识 |
| `nqn` | `str` | 是 | Host 的 NVMe Qualified Name |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `nqn` 不能为空，长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。

**附注 NOTES**

重复添加同一 Host 的访问权限是否成功取决于底层适配器实现（适配器不幂等时重复添加可能报错），调用方不应依赖幂等性保证进行重试。

### ubs\_ssu\_access\_permission\_remove

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_access_permission_remove(name: str, nqn: str) -> None
```

**描述 DESCRIPTION**

撤销指定 Host 对已分配存储空间的访问权限，在 Target 侧将 Host NQN 从子系统的允许主机列表中移除。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `name` | `str` | 是 | 存储空间标识 |
| `nqn` | `str` | 是 | Host 的 NVMe Qualified Name |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `nqn` 不能为空，长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。

**附注 NOTES**

- 命名空间已被删除（不在设备缓存中）时，移除操作幂等跳过。
- 重复移除访问权限是否成功取决于底层适配器实现。

### ubs\_ssu\_space\_attach

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_space_attach(req: UbsSsuSpaceReq) -> List[str]
```

**描述 DESCRIPTION**

将指定的存储空间挂载到系统，使其可被主机访问。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuSpaceReq` | 是 | 挂载请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `List[str]` | 挂载后的命名空间设备路径列表 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineAlreadyAttachedError` | 空间已挂载，重复挂载报错；异常 `data` 属性为已挂载的设备路径列表 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.nqn` 允许为空；非空时长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。
- `req.src_eid` 允许为空；非空时长度不超过 `UBS_SSU_MAX_EID_LENGTH - 1` 个字符。

**附注 NOTES**

暂无。

**样例 EXAMPLES**

```python
from ubse.ubs_engine_ssu import ubs_ssu_space_attach
from ubse.models.ubs_engine_model_ssu import UbsSsuSpaceReq

req = UbsSsuSpaceReq(
    name="space-001",
    nqn="nqn.2024-01.com.huawei:host1",
    src_eid="eid-source-01",
)
dev_paths = ubs_ssu_space_attach(req)
for p in dev_paths:
    print(f"dev_path: {p}")
```

### ubs\_ssu\_space\_detach

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_space_detach(req: UbsSsuSpaceReq) -> None
```

**描述 DESCRIPTION**

将指定的存储空间从系统卸载，释放设备占用。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuSpaceReq` | 是 | 卸载请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 空间已卸载或未挂载，无需卸载报错 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.nqn` 允许为空；非空时长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。
- `req.src_eid` 允许为空；非空时长度不超过 `UBS_SSU_MAX_EID_LENGTH - 1` 个字符。
- 卸载前需确保没有进程正在使用该存储空间。

**附注 NOTES**

已卸载的空间重复卸载将报错，不再幂等返回成功。

### ubs\_ssu\_linear\_space\_attach

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_linear_space_attach(req: UbsSsuLinearSpaceReq) -> Tuple[List[str], str]
```

**描述 DESCRIPTION**

将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuLinearSpaceReq` | 是 | 挂载请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `Tuple[List[str], str]` | 元组：`(命名空间设备路径列表, 聚合后的设备路径)` |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 存储空间不存在 |
| `UbsEngineAlreadyAttachedError` | 空间已挂载，重复挂载报错；异常 `data` 属性为 `(设备路径列表, 聚合设备路径)` |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.nqn` 允许为空；非空时长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。
- `req.src_eid` 允许为空；非空时长度不超过 `UBS_SSU_MAX_EID_LENGTH - 1` 个字符。
- `req.dev_name` 不能为空，长度不超过 `UBS_SSU_MAX_DEV_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。
- 线性编址模式下，数据按顺序填充各成员设备。

**附注 NOTES**

已挂载的空间重复挂载将报错，不再幂等返回成功。

### ubs\_ssu\_linear\_space\_detach

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_linear_space_detach(req: UbsSsuLinearSpaceReq) -> None
```

**描述 DESCRIPTION**

将线性聚合的块设备卸载并释放。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuLinearSpaceReq` | 是 | 卸载请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 存储空间不存在或已卸载，无需卸载报错 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.nqn` 允许为空；非空时长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。
- `req.src_eid` 允许为空；非空时长度不超过 `UBS_SSU_MAX_EID_LENGTH - 1` 个字符。
- `req.dev_name` 不能为空，长度不超过 `UBS_SSU_MAX_DEV_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。

**附注 NOTES**

已卸载的空间重复卸载将报错，不再幂等返回成功。

### ubs\_ssu\_striped\_space\_attach

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_striped_space_attach(req: UbsSsuStripedSpaceReq) -> Tuple[List[str], str]
```

**描述 DESCRIPTION**

将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载，支持 RAID0 和 RAID5 两种级别。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuStripedSpaceReq` | 是 | 条带化挂载请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `Tuple[List[str], str]` | 元组：`(命名空间设备路径列表, 聚合后的设备路径)` |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineAlreadyAttachedError` | 空间已挂载，重复挂载报错；异常 `data` 属性为 `(设备路径列表, 聚合设备路径)` |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.nqn` 允许为空；非空时长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。
- `req.src_eid` 允许为空；非空时长度不超过 `UBS_SSU_MAX_EID_LENGTH - 1` 个字符。
- `req.dev_name` 不能为空，长度不超过 `UBS_SSU_MAX_DEV_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。
- `req.level` 必须为 `RAID0`（0）或 `RAID5`（5）。
- `req.chunk_size` 必须为 `CHUNK_4K`（4）、`CHUNK_16K`（16）、`CHUNK_32K`（32）、`CHUNK_64K`（64）、`CHUNK_128K`（128）、`CHUNK_256K`（256）或 `CHUNK_512K`（512）。
- RAID5 至少需要 `UBS_SSU_RAID5_MIN_MEMBER_NUM`（3）个成员设备。

**附注 NOTES**

已挂载的空间重复挂载将报错，不再幂等返回成功。

### ubs\_ssu\_striped\_space\_detach

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_striped_space_detach(req: UbsSsuStripedSpaceReq) -> None
```

**描述 DESCRIPTION**

将条带化聚合的块设备卸载并释放。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `req` | `UbsSsuStripedSpaceReq` | 是 | 卸载请求参数 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 存储空间不存在或已卸载，无需卸载报错 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `req.name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `req.nqn` 允许为空；非空时长度不超过 `UBS_SSU_MAX_NQN_LENGTH - 1` 个字符。
- `req.src_eid` 允许为空；非空时长度不超过 `UBS_SSU_MAX_EID_LENGTH - 1` 个字符。
- `req.dev_name` 不能为空，长度不超过 `UBS_SSU_MAX_DEV_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_/.-]`。
- 卸载操作不校验 `level` 和 `chunk_size`。

**附注 NOTES**

已卸载的空间重复卸载将报错，不再幂等返回成功。

### ubs\_ssu\_ns\_stats\_get

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_ns_stats_get(name: str) -> List[UbsSsuNsStats]
```

**描述 DESCRIPTION**

查询指定存储空间下各命名空间的容量使用情况，包括总容量和已用容量。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `name` | `str` | 是 | 存储空间标识 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `List[UbsSsuNsStats]` | 命名空间统计信息列表 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 存储空间不存在 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。

**附注 NOTES**

暂无。

### ubs\_ssu\_connect\_info\_get

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_connect_info_get(name: str, vfe: Optional[UbsUbVfe] = None) -> List[UbsSsuConnectInfo]
```

**描述 DESCRIPTION**

查询指定存储空间在指定 VFE 上的 NVMe 连接信息，包括子系统 NQN、Host NQN、命名空间 ID 等。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `name` | `str` | 是 | 存储空间标识 |
| `vfe` | `Optional[UbsUbVfe]` | 否 | VFE 信息；传 `None` 时使用 host 侧分配给 ssu 的 fe 的 eid |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `List[UbsSsuConnectInfo]` | 连接信息列表 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | 存储空间或 VFE 不存在 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `name` 不能为空，长度不超过 `UBS_SSU_MAX_NAME_LENGTH - 1` 个字符，仅允许字符 `[a-zA-Z0-9_-.:]`。
- `vfe` 可为 `None`，表示不限定 VFE，此时连接信息里的 `src_eid` 为 host 侧分配给 ssu 的 fe 的 eid；指定 vfe 时 `src_eid` 为指定 vfe 的 eid。

**附注 NOTES**

暂无。

### ubs\_ssu\_fe\_device\_list

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_fe_device_list() -> List[UbsUbFe]
```

**描述 DESCRIPTION**

查询系统中所有 FE 设备信息，包括每个 PFE 下的 VFE 列表。

**参数 PARAMETERS**

无参数。

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `List[UbsUbFe]` | FE 设备信息列表 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

无。

**附注 NOTES**

暂无。

### ubs\_ssu\_fe\_device\_alloc

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_fe_device_alloc(upi: int, vfe: UbsUbVfe, guid: str) -> str
```

**描述 DESCRIPTION**

将指定的虚拟功能单元绑定到目标虚拟机，使虚拟机可通过该 VFE 访问存储资源。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `upi` | `int` | 是 | 租户隔离标识 |
| `vfe` | `UbsUbVfe` | 是 | 要绑定的 VFE 信息 |
| `guid` | `str` | 是 | 总线实例 GUID，标识目标虚拟机，**可传空**（空字符串）；非空时长度须为 `UBS_SSU_GUID_LENGTH`。为空表示由 ubse 内部创建 vm busInstance 并绑定到该 VFE，非空表示绑定到指定虚拟机 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `str` | 绑定后的总线实例 GUID 字符串（内部创建场景下为新创建的 GUID） |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | VFE 或虚拟机不存在 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `vfe` 不能为 `None`，且 `vfe.vfe_guid` 和 `vfe.bind_bus_instance_guid` 长度须为 `UBS_SSU_GUID_LENGTH`（32）。
- `guid` 允许为空字符串（表示由 ubse 内部创建 vm busInstance）；非空时长度须为 `UBS_SSU_GUID_LENGTH`（32）。

**附注 NOTES**

- 与 C 端的差异：C 端 `bus_instance_guid` 为 IN/OUT 复用参数，不允许传 NULL，改以全 0 缓冲区表达"内部创建"语义；Python 端 `guid` 仅作为 IN 参数，实际 GUID 通过返回值传出。
- 协议层等价：Python 空字符串与 C 端全 0 缓冲区均打包为 `UBS_SSU_GUID_LENGTH` 字节全 0。

**样例 EXAMPLES**

```python
from ubse.ubs_engine_ssu import ubs_ssu_fe_device_alloc
from ubse.models.ubs_engine_model_ssu import UbsUbVfe

vfe = UbsUbVfe(slot_id=0, chip_id=0, pfe_id=1, vfe_id=1, vfe_guid="...")
# 传空字符串表示由 ubse 内部创建 vm busInstance
guid = ubs_ssu_fe_device_alloc(upi=1, vfe=vfe, guid="")
print(f"bind success, guid={guid}")
```

### ubs\_ssu\_fe\_device\_free

**模块 MODULE**

`ubse.ubs_engine_ssu`

**摘要 SYNOPSIS**

```python
def ubs_ssu_fe_device_free(upi: int, vfe: UbsUbVfe) -> None
```

**描述 DESCRIPTION**

将已分配的虚拟功能单元从目标虚拟机释放，回收 VFE 设备资源。

**参数 PARAMETERS**

| name | 类型 | 是否必填 | 说明 |
| ---- | ---- | -------- | ---- |
| `upi` | `int` | 是 | 租户隔离标识 |
| `vfe` | `UbsUbVfe` | 是 | 要释放的 VFE 信息 |

**返回值 RETURN VALUE**

| 返回类型 | 说明 |
| -------- | ---- |
| `None` | 无返回值 |

**异常 RAISES**

| 异常类 | 说明 |
| ------ | ---- |
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 UBSE 服务端失败 |
| `UbsEngineAuthError` | UBSE 服务端鉴权不通过 |
| `UbsEngineNotExistError` | VFE 或虚拟机不存在 |
| `UbsEngineTimeoutError` | UBSE 服务端处理超时 |
| `UbsEngineInternalError` | UBSE 服务端内部错误 |

**约束 CONSTRAINTS**

- `vfe` 不能为 `None`。
- `vfe.bind_bus_instance_guid` 必须为 32 个字符（`UBS_SSU_GUID_LENGTH`）。

**附注 NOTES**

暂无。
