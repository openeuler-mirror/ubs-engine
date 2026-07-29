# topo查询接口支持SuperPodId

## 1. 背景与动机

SuperPodId（超节点ID）是SMBIOS中新增的字段，位于CLOS组网类型的下一个byte，用于标识节点所属的超节点。当前topo查询接口（`ubs_topo_node_list`、`ubs_topo_node_local_get`）返回的`ubs_topo_node_t`结构体中不包含该信息，上层应用无法获取SuperPodId。

## 2. 需求描述

- 在SDK侧topo接口的`ubs_topo_node_t`结构体中增加`super_pod_id`字段（2字节，`uint16_t`）
- 后端通过SMBIOS解析获取SuperPodId，并通过IPC响应传递给SDK
- **兼容性要求**：新SDK连接旧版本后端时，`super_pod_id`默认为0，不影响其他字段解析；旧版本SDK连接新后端时，能正常工作

## 3. 设计方案

### 3.1 核心设计原则

**SuperPodId是全局值**，而非per-node值。所有节点的SuperPodId相同，来源于SMBIOS。因此：

- `UbseNode`结构体中**不增加**`superPodId`字段
- SuperPodId在IPC数据中的位置：**追加在所有node数据之后**，而非嵌入每个node内部

### 3.2 IPC数据格式

#### 单节点查询（`ubs_topo_node_local_get`）

```
┌──────────────────┐  ┌──────────────┐
│  node_base_data  │  │ superPodId   │
│  (原有字段)       │  │ (uint16_t)   │
└──────────────────┘  └──────────────┘
```

#### 节点列表查询（`ubs_topo_node_list`）

```
┌────────┐  ┌────────────┐  ┌────────────┐       ┌──────────────┐
│ count  │  │ node1_data │  │ node2_data │  ...  │ superPodId   │
│(uint32)│  │ (原有字段)  │  │ (原有字段)  │       │ (uint16_t)   │
└────────┘  └────────────┘  └────────────┘       └──────────────┘
```

**关键**：superPodId放在所有node数据之后，而非嵌入每个node内部。这样旧SDK解包每个node时不会产生偏移错位。

### 3.3 兼容性分析

| 场景 | 行为 | 结果 |
|------|------|------|
| 新SDK + 新后端 | 正常解包所有数据，包含尾部superPodId | super_pod_id正确获取 |
| 新SDK + 旧后端 | 旧后端IPC响应不含尾部superPodId，解包失败时默认为0 | super_pod_id=0，INFO日志提示 |
| 旧SDK + 新后端 | 旧SDK不读取尾部2字节，node数据按原有长度解包无偏移错位 | 正常工作，尾部2字节被忽略 |
| 旧SDK + 旧后端 | 无变化 | 正常工作 |

## 4. 详细修改

### 4.1 SMBIOS解析层

| 文件 | 修改 |
|------|------|
| `src/adapter_plugins/mti/smbios/ubse_smbios_def.h` | `UbseSmbios`类增加`GetSuperPodId(uint16_t&)`接口声明 |
| `src/adapter_plugins/mti/smbios/ubse_smbios_def.cpp` | 实现SuperPodId解析，字段位于CLOS组网类型下一个byte |

### 4.2 IPC打包层（服务端）

| 文件 | 修改 |
|------|------|
| `src/controllers/node/ubse_node_api_convert.cpp` | `UbseNodePack`：在node数据之后追加`UbsePackUint16(superPodId)`，superPodId从`UbseSmbios::GetInstance().GetSuperPodId()`获取 |
| | `UbseNodeListPack`：在所有node数据之后追加`UbsePackUint16(superPodId)` |
| | `UbseNodeCalcSize`/`UbseNodeListCalcSize`：不含superPodId（由Pack函数单独追加） |

### 4.3 IPC解包层（SDK侧）

| 文件 | 修改 |
|------|------|
| `src/sdk/c/libubse_helper.cpp` | `ubse_node_unpack`：在node数据之后尝试解包superPodId，失败则默认0 |
| | `ubse_node_list_unpack`：在所有node数据之后尝试解包superPodId，成功则设置到所有节点，失败则默认0 |
| | 新增`unpack_uint16`辅助函数 |

### 4.4 SDK公共头文件

| 文件 | 修改 |
|------|------|
| `src/sdk/c/include/ubs_engine_topo.h` | `ubs_topo_node_t`结构体末尾增加`uint16_t super_pod_id`字段 |

### 4.5 Python SDK

| 文件 | 修改 |
|------|------|
| `src/sdk/python/ffi/ubs_engine_types.py` | `UbsTopoNodeT`增加`("super_pod_id", ctypes.c_uint16)`字段 |

### 4.6 Go SDK

| 文件 | 修改 |
|------|------|
| `src/sdk/go/topo/ubs_engine_topo.go` | `UbsTopoNode`增加`SuperPodId uint16`字段，`convertTopoNode`中读取 |

### 4.7 API文档

| 文件 | 修改 |
|------|------|
| `docs/zh/ubse_api_reference.md` | `ubs_topo_node_t`结构体增加`super_pod_id`字段说明；`ubs_topo_node_list`和`ubs_topo_node_local_get`附注中说明兼容性 |

## 5. UT测试

### 5.1 SDK UT（`test/UT/sdk/test_ubs_engine_topo.cpp`）

| 测试用例 | 验证内容 |
|---------|---------|
| `UbsTopoNodeListWhenSuccess_SuperPodId` | node list解包后所有节点的super_pod_id一致（全局值） |
| `UbsTopoNodeListWhenOldBackend_NoSuperPodId` | 旧后端数据（截断尾部2字节）解包成功，super_pod_id默认为0 |
| `UbsTopoNodeLocalGet_SuperPodId` | 单节点解包后super_pod_id正确填充 |
| `UbsTopoNodeLocalGet_OldBackend_NoSuperPodId` | 旧后端单节点数据解包成功，super_pod_id默认为0 |

### 5.2 Node Controller UT（`test/UT/node_controller/test_ubse_node_api_convert.cpp`）

| 测试用例 | 验证内容 |
|---------|---------|
| `UbseNodePack_ContainsSuperPodId` | UbseNodePack输出大小包含sizeof(uint16_t)的superPodId |
| `UbseNodeListPack_Success` | UbseNodeListPack成功，输出大小包含superPodId |
| `UbseNodeListPack_EmptyList` | 空node list打包成功（superPodId仍被打包为0） |
