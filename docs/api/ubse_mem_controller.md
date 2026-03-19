# UBSE 内存控制器接口说明文档

## 概述
UBSE 内存控制器提供了一组用于管理内存借用关系的接口。这些接口支持不同类型的内存借用，包括基于 NUMA 的内存借用和基于地址的内存借用。文档中详细描述了每个接口的功能、参数和返回值。

---

## 枚举类型

### `UbseErrorCode`
表示操作的错误代码。

| 枚举值 | 描述 |
|--------|------|
| `SUCCESS` | 操作成功 |
| `UBS_ERR_INVALID_ARG` | 无效参数 |
| `UBS_ERR_ALGO_FAIL` | 算法报错 |
| `UBS_ERR_EXIST` | 借用标识已存在 |
| `UBS_ERR_CREATING` | 借用标识正在创建 |
| `UBS_ERR_DELETING` | 借用标识正在删除 |
| `UBS_ERR_TIMEOUT` | 操作超时 |
| `UBS_ERR_INTERNAL` | 内部错误 |
| `UBS_ERR_NOT_EXIST` | 借用标识不存在 |
| `UBS_ERR_UNIMPORT_FAIL` | unimport 失败 |
| `UBS_ERR_WAIT_UNEXPORT` | 等待 unexport 执行 |
| `UBS_ERR_LINKINFO` | 链路信息错误 |
| `UBS_ERR_ATTACHED` | 已附加 |
| `UBS_ERR_AUTH` | 权限校验失败 |
| `UBS_ERR_ATTACH_NO_NODE` | 附加请求中无 nodeId |
| `UBS_ERR_DETACH_NO_NODE` | 分离请求中无 nodeId |
| `UBSE_ERR_ATTACH_USING` | 资源存在借用关系，无法删除 |
| `UBS_ERR_COMP_ERROR` | 确定性迁移模式参数错误 |
| `UBS_ERR_INVALID_AFFINITY_PARAMS` | 指定 CPU 平面创建参数错误 |
| `UBS_ERR_GET_INFO_FAIL` | 从内部获取数据失败 |
| `UBSE_PAR_SUCCESS` | 部分成功 |

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

### `UbseMemDistance`
表示CPU连线类型。

| 枚举值 | 描述 |
|--------|------|
| `MEM_DISTANCE_L0` | L0 对应直接 CPU 连线节点 |
| `MEM_DISTANCE_L1` | L1 对应通过 1 跳节点 |
| `MEM_DISTANCE_L2` | L2 对应超过 1 跳节点 |

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

### `UbseMemBorrower`
表示借用方信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `nodeId` | `std::string` | 节点 ID |
| `affinitySocketId` | `int` | 可选，亲和 socket ID |
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
表示指定候选借出节点的 NUMA 借用可选项。

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotIds` | `std::vector<std::string>` | 候选借出节点范围 |

---

### `UbseMemAddrBorrowLocAndSizeByPid`
表示基于 PID 的地址借用位置和大小。

| 字段 | 类型 | 描述                                    |
|------|------|---------------------------------------|
| `addr` | `uint64_t` | 借用的进程虚拟地址                             |
| `size` | `uint64_t` | 借入大小<br/>单位Byte, 取值范围大于等于4\*1024*1024 |

---

### `UbseMemProcessLender`
表示基于进程的地址借出方信息。

| 字段 | 类型 | 描述 |
|------|------|------|
| `slotId` | `uint32_t` | 节点唯一标识 |
| `socketId` | `int` | socket ID |
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
- `0`: 查询成功
- `1`: 查询失败

---

### `UbseGetNumaMemDebtInfoWithNode`
返回和传入节点相关的账本信息。

```cpp
UbseErrorCode UbseGetNumaMemDebtInfoWithNode(const std::string &nodeId, std::vector<UbseNumaMemoryDebtInfo> &debtInfos);
```

**参数:**
- `nodeId`: 节点 ID
- `debtInfos`: 借用账本对象集合

**返回值:**
- `UbseErrorCode::SUCCESS`: 成功
- `UbseErrorCode::UBS_ERR_INVALID_ARG`: 无效参数
- `UbseErrorCode::UBSE_PAR_SUCCESS`: 部分成功
- `UbseErrorCode::UBS_ERR_INTERNAL`: 内部错误

---

### `UbseGetNumaMemDebtInfo`
返回当前集群拓扑中的所有对账完成节点的账本信息。

```cpp
UbseErrorCode UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo> &debtInfos);
```

**参数:**
- `debtInfos`: 借用账本对象集合

**返回值:**
- `UbseErrorCode::SUCCESS`: 成功
- `UbseErrorCode::UBS_ERR_INVALID_ARG`: 无效参数
- `UbseErrorCode::UBSE_PAR_SUCCESS`: 部分成功
- `UbseErrorCode::UBS_ERR_INTERNAL`: 内部错误

---

### `UbseGetAllNodeNumaInfo`
返回所有采集到的节点相关的 NUMA 信息。

```cpp
UbseErrorCode UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList);
```

**参数:**
- `numaNodeInfoList`: 节点 NUMA 对象集合

**返回值:**
- `UbseErrorCode::SUCCESS`: 成功
- `UbseErrorCode::UBS_ERR_INTERNAL`: 获取节点信息失败

---

### `UbseGetNodeNumaInfoByNodeId`
返回所有采集到的节点指定节点相关的 NUMA 信息。

```cpp
UbseErrorCode UbseGetNodeNumaInfoByNodeId(const std::string &nodeId, std::vector<UbseNodeNumaInfo> &numaNodeInfoList);
```

**参数:**
- `nodeId`: 节点 ID
- `numaNodeInfoList`: 节点 NUMA 对象集合

**返回值:**
- `UbseErrorCode::UBS_ERR_INVALID_ARG`: 传入值非法或指定节点不存在
- `UbseErrorCode::UBS_ERR_INTERNAL`: 获取节点信息失败

---

### `UbseMemNumaCreateWithLender`
指定借出信息，提供给插件使用 NUMA 借用。

```cpp
UbseErrorCode UbseMemNumaCreateWithLender(const std::string &name, const UbseMemBorrower &borrower, const std::vector<UbseMemNumaLender> &lenders, uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc &desc);
```

**参数:**
- `name`: 借用标识
- `borrower`: 借用方信息
- `lenders`: 借出方信息
- `usrInfo`: 调用方私有数据
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**
- `UbseErrorCode`: 操作结果

---

### `UbseMemNumaCreate`
提供给插件使用 NUMA 借用。

```cpp
UbseErrorCode UbseMemNumaCreate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemNumaCreateOpt &opt, UbseMemNumaDesc &desc);
```

**参数:**
- `name`: 借用标识
- `borrower`: 借用方信息
- `opt`: 借出方信息
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**
- `UbseErrorCode`: 操作结果

---

### `UbseMemNumaCreateWithCandidate`
指定候选借出节点，提供给插件使用 NUMA 借用。

```cpp
UbseErrorCode UbseMemNumaCreateWithCandidate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemNumaCandidateOpt &opt, UbseMemNumaDesc &desc);
```

**参数:**
- `name`: 借用标识
- `borrower`: 借用方信息
- `opt`: 可选项
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**
- `UbseErrorCode::SUCCESS`: 成功
- 其他: 失败

---

### `UbseMemNumaDelete`
删除指定 NUMA 远端内存。

```cpp
UbseErrorCode UbseMemNumaDelete(const std::string &name, const UbseMemBorrower &borrower);
```

**参数:**
- `name`: 借用标识
- `borrower`: 借用方信息

**返回值:**
- `UbseErrorCode`: 操作结果

---

### `UbseMemAddrCreate`
提供给插件使用地址借用。

```cpp
UbseErrorCode UbseMemAddrCreate(const std::string &name, const UbseMemBorrower &borrower, const UbseMemProcessLender &lender, uint32_t flag, UbseMemAddrDesc &desc);
```

**参数:**
- `name`: 借用标识
- `borrower`: 借用方信息
- `lender`: 借出方信息
- `flag`: 额外的内存借用属性
- `desc`: 借用形成的远端 NUMA 信息

**返回值:**
- `UbseErrorCode`: 操作结果

---

### `UbseMemAddrDelete`
删除地址借用。

```cpp
UbseErrorCode UbseMemAddrDelete(const std::string &name, const UbseMemBorrower &borrower);
```

**参数:**
- `name`: 借用标识
- `borrower`: 借用方信息

**返回值:**
- `UbseErrorCode`: 操作结果

---

### `UbseMemDebtCircleCheck`
检查借用是否成环。

```cpp
UbseErrorCode UbseMemDebtCircleCheck(const std::string &srcNodeId, const std::string &dstNodeId, bool &isCircle);
```

**参数:**
- `srcNodeId`: 借入 NodeId
- `dstNodeId`: 借出 NodeId
- `isCircle`: 是否成环

**返回值:**
- `UbseErrorCode`: 操作结果

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