# UBSE IPC 序列化与反序列化规则

## 1. UDS 基本信息

### 1.1 UDS 文件路径
- **默认路径**：`/var/run/ubse/ubse.sock`

### 1.2 使用方式
不涉及

### 1.3 权限要求
- **文件权限**：UDS 文件需要有读写权限，通常设置为 `0660`
- **用户权限**：客户端进程需要有访问 UDS 文件的权限，通常需要属于特定的用户组（如 `ubse` 组）
- **目录权限**：UDS 文件所在的目录（如 `/var/run/ubse/`）需要有执行权限，确保进程可以访问该目录

## 2. module_code 和 op_code 清单

### 2.1 模块码 (module_code)
| 模块码 | 十六进制 | 说明 |
|--------|----------|------|
| UBSE_MEM | 0x0001 | UBSE内存基础模块 |
| UBSE_ELECTION | 0x0002 | 选举模块 |
| UBSE_NODE | 0x0003 | 节点模块 |
| UBSE_LONG_LINK_REGISTER | 0x0004 | 向服务端注册监听长连接事件 |
| UBSE_URMA | 0x0005 | URMA模块 |

### 2.2 URMA 操作码 (op_code)
| 操作码 | 十六进制 | 说明 | 对应函数 |
|--------|----------|------|----------|
| UBSE_URMA_QOS_SET | 0x0001 | 设置URMA QoS | UbsSetBandwidth |
| UBSE_URMA_QOS_GET | 0x0002 | 获取URMA QoS | UbsGetBandwidth |
| UBSE_URMA_QOS_RESET | 0x0003 | 重置URMA QoS | UbsResetBandwidth |
| UBSE_URMA_CLI_QOS_GET | 0x0004 | CLI获取URMA QoS | - |
| UBSE_URMA_DEV_GET | 0x0005 | 获取URMA设备信息 | UbsGetVfeDevice, UbsGetSharedDevice |
| UBSE_URMA_DEV_ALLOC | 0x0006 | 分配URMA设备 | UbsAllocateDevice |
| UBSE_URMA_DEV_FREE | 0x0007 | 释放URMA设备 | UbsFreeDevice |
| UBSE_URMA_CLI_DEV_GET | 0x0008 | CLI获取URMA设备信息 | - |
| UBSE_URMA_CLI_DEV_ACTIVATE | 0x0009 | CLI激活URMA设备 | - |

### 2.3 内存操作码 (op_code)
| 操作码 | 十六进制 | 说明 |
|--------|----------|------|
| UBSE_MEM_FD_CREATE | 0x0001 | FD内存创建 |
| UBSE_MEM_FD_WITH_LEND_INFO | 0x0002 | 带出借方信息的FD创建 |
| UBSE_MEM_FD_CREATE_WITH_CANDIDATE | 0x0003 | 带候选节点的FD创建 |
| UBSE_MEM_FD_PERMISSION | 0x0004 | FD权限操作 |
| UBSE_MEM_FD_GET | 0x0005 | FD信息获取 |
| UBSE_MEM_FD_LIST | 0x0006 | FD列表查询 |
| UBSE_MEM_FD_DELETE | 0x0007 | FD删除/归还 |
| UBSE_MEM_NUMA_CREATE | 0x0010 | NUMA内存创建 |
| UBSE_MEM_NUMA_WITH_LEND_INFO | 0x0011 | 带出借方信息的NUMA创建 |
| UBSE_MEM_NUMA_CREATE_WITH_CANDIDATE | 0x0012 | 带候选节点的NUMA创建 |
| UBSE_MEM_NUMA_GET | 0x0013 | NUMA信息获取 |
| UBSE_MEM_NUMA_LIST | 0x0014 | NUMA列表查询 |
| UBSE_MEM_NUMA_DELETE | 0x0015 | NUMA删除/归还 |
| UBSE_MEM_SHM_CREATE | 0x0020 | 共享内存创建 |
| UBSE_MEM_SHM_ATTACH | 0x0021 | 共享内存附加 |
| UBSE_MEM_SHM_GET | 0x0022 | 共享内存信息获取 |
| UBSE_MEM_SHM_LIST | 0x0023 | 共享内存列表查询 |
| UBSE_MEM_SHM_DETACH | 0x0024 | 共享内存分离 |
| UBSE_MEM_SHM_DELETE | 0x0025 | 共享内存删除 |
| UBSE_MEM_SHM_MEMID_STATUS_GET | 0x0026 | 共享内存状态获取 |
| UBSE_MEM_SHM_CREATE_WITH_AFFINITY | 0x0027 | 带亲和的共享内存创建 |
| UBSE_MEM_SHM_LIST_WITH_PREFIX | 0x0028 | 带前缀的共享内存列表查询 |
| UBSE_MEM_SHM_CREATE_WITH_LENDER | 0x0029 | 带指定借出方的共享内存创建 |

### 2.4 节点操作码 (op_code)
| 操作码 | 十六进制 | 说明 |
|--------|----------|------|
| UBSE_NODE_LIST | 0x0001 | 节点列表 |
| UBSE_NODE_CPU_TOPO_LIST | 0x0002 | CPU拓扑列表 |
| UBSE_NODE_NUMA_MEM_GET | 0x0003 | NUMA内存获取 |
| UBSE_NODE_GET | 0x0004 | 节点信息获取 |
| UBSE_NODE_CLI_NODE_INFO | 0x0005 | CLI节点信息 |
| UBSE_NODE_CLI_CPU_TOPO_LIST | 0x0006 | CLI CPU拓扑列表 |
| UBSE_CLUSTER_INFO | 0x0007 | 集群信息 |

## 3. 序列化规则 (SerializeRequestMessage)

### 3.1 总长度计算
```c++
const uint32_t totalLength = sizeof(bool) + sizeof(UbseRequestHeader) + requestMessage.header.bodyLen;
```

### 3.2 字段顺序与格式
| 字段 | 大小 | 类型 | 说明 |
|------|------|------|------|
| isResp | 1字节 | bool | 固定为false (0) |
| moduleCode | 2字节 | uint16_t | 模块代码，小端序 |
| opCode | 2字节 | uint16_t | 操作代码，小端序 |
| bodyLen | 4字节 | uint32_t | 请求体长度，小端序 |
| clientRequestId | 8字节 | uint64_t | 客户端请求ID，小端序 |
| body | bodyLen字节 | uint8_t[] | 请求体数据 |

### 3.3 序列化步骤
1. 计算总长度
2. 分配缓冲区
3. 写入isResp标志（值为false）
4. 写入请求头（UbseRequestHeader结构）
5. 写入请求体（如果bodyLen > 0）

### 3.4 错误处理
- 内存分配失败：返回UBSE_ERROR_SERIALIZE_FAILED
- 内存复制失败：返回UBSE_ERROR_SERIALIZE_FAILED

## 4. 反序列化规则 (Receive)

### 4.1 接收步骤
1. **接收isResp标志**：
   - 大小：1字节
   - 类型：bool
   - 说明：标识是否为响应消息（值为true）

2. **接收响应头**（UbseResponseHeader结构）：
   - 总大小：16字节
   - 字段：
     | 字段 | 大小 | 类型 | 说明 |
     |------|------|------|------|
     | statusCode | 4字节 | uint32_t | 操作状态码（0 = 成功，非0 = 错误码），小端序 |
     | bodyLen | 4字节 | uint32_t | 响应体长度，小端序 |
     | clientRequestId | 8字节 | uint64_t | 客户端请求ID，小端序 |

3. **接收响应体**：
   - 大小：bodyLen字节
   - 类型：uint8_t[]
   - 说明：仅当bodyLen > 0时接收

### 4.2 错误处理
- 接收isResp标志失败：返回UBSE_IPC_ERROR_RECV_FAILED
- 接收响应头失败：返回UBSE_IPC_ERROR_RECV_FAILED
- 响应体长度超过最大消息大小（10M）：返回UBSE_IPC_ERROR_RECV_FAILED
- 内存分配失败：返回UBSE_IPC_ERROR_RECV_FAILED
- 接收响应体失败：返回UBSE_IPC_ERROR_RECV_FAILED
- 超时：返回UBSE_ERR_TIMED_OUT

### 4.3 内存管理
- 响应体内存分配：使用new[]
- 响应体内存释放：使用delete[]
- 接收失败时需要释放已分配的内存

## 5. 数据结构定义

### 5.1 UbseRequestHeader
```c++
struct UbseRequestHeader {
    uint16_t moduleCode;      // 模块代码
    uint16_t opCode;          // 操作代码
    uint32_t bodyLen;         // 请求体长度
    uint64_t clientRequestId; // 客户端请求ID
};
```

### 5.2 UbseResponseHeader
```c++
struct UbseResponseHeader {
    uint32_t statusCode;      // 操作状态码（0 = 成功，非0 = 错误码）
    uint32_t bodyLen;         // 响应体长度
    uint64_t clientRequestId; // 客户端请求ID
};
```

## 6. 注意事项
- 所有字段使用小端序
- 所有字段连续存储，无填充
- 消息最大长度为10M（UBSE_MESSAGE_SIZE）
- 序列化和反序列化过程中需要处理内存分配和释放
- 接收过程中需要处理超时情况

## 7. 字符串解析规则 (unpackString)

### 7.1 字段顺序与格式
| 字段 | 大小 | 类型 | 说明 |
|------|------|------|------|
| strLen | 4字节 | uint32_t | 字符串长度，小端序 |
| str | strLen字节 | uint8_t[] | 字符串数据 |

### 7.2 解析步骤
1. 读取字符串长度（4字节）
2. 检查字符串长度是否合法（不超过最大长度，不超过剩余数据长度）
3. 读取字符串数据（strLen字节）
4. 移动数据指针到字符串结束位置

### 7.3 错误处理
- 数据长度不足：返回错误
- 字符串长度超过最大长度：返回错误
- 字符串长度超过剩余数据长度：返回错误

### 7.4 关键解析Response示例代码
```go
// unpackString unpacks a string from the response.
func unpackString(response []byte, maxLen uint32) (string, []byte, error) {
	if len(response) < 4 {
		return "", response, fmt.Errorf("invalid string length")
	}
	strLen := binary.LittleEndian.Uint32(response[0:])
	response = response[4:]

	if strLen > maxLen || int(strLen) > len(response) {
		return "", response, fmt.Errorf("invalid string length")
	}

	str := string(response[0:strLen])
	response = response[strLen:]

	return str, response, nil
}
```