# UBSE 内存控制器接口说明文档

## 概述
UBSE 内存控制器提供了一组用于管理内存借用关系的接口。这些接口支持不同类型的内存借用，包括基于 NUMA 的内存借用和基于地址的内存借用。文档中详细描述了每个接口的功能、参数和返回值。

---

## 枚举类型

### `UbseMemStage`
表示借用关系的阶段。

| 枚举值 | 描述 |
|--------|------|
| `UBSE_NOT_EXIST` | 借用关系不存在 |
| `UBSE_CREATING` | 正在创建中 |
| `UBSE_DELETING` | 正在删除中 |
| `UBSE_EXIST` | 创建成功 |
| `UBSE_ERR_ONLY_IMPORT` | 只存在借入 |
| `UBSE_ERR_WAIT_UNEXPORT` | 等待 unexport 执行 |

### `UbseMemBorrowType`
表示借用类型。

| 枚举值 | 描述 |
|--------|------|
| `FD_BORROW` | 文件描述符借用 |
| `NUMA_BORROW` | NUMA 借用 |
| `ADDR_BORROW` | 地址借用 |
| `SHM_BORROW` | 共享内存借用 |
| `SHM_ATTACH` | 共享内存附加 |

### `UbseMemDistance`
表示CPU连线类型。

| 枚举值 | 描述 |
|--------|------|
| `MEM_DISTANCE_L0` | L0 对应直接 CPU 连线节点 |
| `MEM_DISTANCE_L1` | L1 对应通过 1 跳节点（暂不支持） |
| `MEM_DISTANCE_L2` | L2 对应超过 1 跳节点（暂不支持） |

---

## 结构体

### `UbseMemResult`
表示借用结果。

| 字段 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string` | 借用标识 |
| `importNodeId` | `std::string` | 借入节点 ID |
| `realSize` | `uint64_t` | 实际大小 |
| `stage` | `UbseMemStage` | 借用阶段 |

---

### `UbseNumaMemoryDebtInfo`
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

### `UbseNumaMemoryImportDebtInfo`
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

### `UbseMemBorrower`
表示借用方信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `nodeId` | `std::string` | 节点 ID |
| `affinitySocketId` | `int` | 可选，亲和 socket ID，-1 表示无效 |
| `uid` | `uid_t` | 发起借用方运行用户的 UID |
| `username` | `std::string` | 发起借用方运行用户的名称 |

---

### `UbseMemNumaLender`
表示 NUMA 借出方信息。

| 字段 | 类型 | 描述                                      |
|------|------|-----------------------------------------|
| `slotId` | `uint32_t` | 节点唯一标识                                  |
| `socketId` | `uint32_t` | socket ID                               |
| `numaId` | `uint64_t` | NUMA ID                                 |
| `size` | `uint64_t` | 借出内存大小<br/>单位Byte, 取值范围大于等于4\*1024*1024 |

---

### `UbseTopoIpAddress`
表示拓扑 IP 地址。

| 字段 | 类型 | 描述 |
|------|------|------|
| `af` | `int32_t` | 地址族 |
| `ipv4` | `struct in_addr` | IPv4 地址 |
| `ipv6` | `struct in6_addr` | IPv6 地址 |

---

### `UbseTopoNode`
表示拓扑节点。

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotId` | `uint32_t` | 节点唯一标识 |
| `socketIdList` | `std::vector<int16_t>` | socket ID 列表 |
| `numaIdList` | `std::vector<int32_t>` | NUMA ID 列表 |
| `ips` | `std::vector<UbseTopoIpAddress>` | IP 地址列表 |
| `hostName` | `std::string` | 主机名 |

---

### `UbseMemNumaDesc`
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

### `UbseNodeNumaInfo`
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

### `UbseMemNumaCreateOpt`
表示 NUMA 借用的可选项。

| 字段 | 类型 | 描述                                      |
|------|------|-----------------------------------------|
| `size` | `uint64_t` | 借出内存大小<br/>单位Byte, 取值范围大于等于4\*1024*1024 |
| `distance` | `UbseMemDistance` | 内存距离                                    |
| `highWatermark` | `size_t` | 内存使用量百分比                                |
| `usrInfo` | `uint8_t[UBSE_MAX_USR_INFO_LEN]` | 调用方私有数据                                 |

---

### `UbseMemNumaCandidateOpt`
表示指定候选借出节点的 NUMA 借用可选项。继承自 `UbseMemNumaCreateOpt`。

**自有字段：**

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotIds` | `std::vector<std::string>` | 候选借出节点范围 |

---

### `UbseMemAddrBorrowLocAndSizeByPid`
表示基于 PID 的地址借用位置和大小。

| 字段 | 类型 | 默认值 | 描述                                    |
|------|------|--------|---------------------------------------|
| `addr` | `uint64_t` | `0` | 借用的进程虚拟地址                             |
| `size` | `uint64_t` | `0` | 该段地址借入大小，单位Byte |

---

### `UbseMemProcessLender`
表示基于进程的地址借出方信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotId` | `uint32_t` | 节点唯一标识 |
| `socketId` | `int` | 内存申请借出方节点 socket 信息，-1 表示无效 |
| `pid` | `uint64_t` | 借出进程 PID |
| `vaLists` | `std::vector<UbseMemAddrBorrowLocAndSizeByPid>` | 借用地址段 |

---

### `UbseMemAddrDesc`
表示基于地址借用形成的远端 NUMA 信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `name` | `std::string` | 借用标识 |
| `numaId` | `int64_t` | 远端 NUMA ID |
| `lender` | `UbseMemProcessLender` | 借出节点 |
| `importNode` | `UbseTopoNode` | 借入节点 |
| `size` | `uint64_t` | 借用大小 |

---

## 函数

### `UbseQueryResult`
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

### `UbseGetNumaMemDebtInfoWithNode`
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

### `UbseGetNumaMemDebtInfo`
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

### `UbseGetNumaMemImportDebtInfoWithLocalNode`
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

### `UbseGetAllNodeNumaInfo`
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

### `UbseGetNodeNumaInfoByNodeId`
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

### `UbseMemNumaCreateWithLender`
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

### `UbseMemNumaCreate`
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

### `UbseMemNumaCreateWithCandidate`
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

### `UbseMemNumaDelete`
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

### `UbseMemAddrCreate`
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

### `UbseMemAddrDelete`
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

### `UbseMemDebtCircleCheck`
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

### `UbseGetAddrMemDebtInfoWithNode`
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

## 常量

### `UBSE_MAX_USR_INFO_LEN`
调用方私有数据的最大长度。

```cpp
static constexpr uint32_t UBSE_MAX_USR_INFO_LEN = 32;
```

---

### `UBSE_MEM_FLAG_NO_WR_DELAY`
非写接力模式标志。

```cpp
#define UBSE_MEM_FLAG_NO_WR_DELAY 0x1
```

---

## 总结
UBSE 内存控制器提供了一组丰富的接口，用于管理内存借用关系。通过这些接口，可以方便地进行 NUMA 和地址借用，同时支持查询和删除操作。