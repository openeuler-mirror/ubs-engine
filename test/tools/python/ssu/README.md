# ubse_ssu_test_python - SSU Python SDK 交互式命令行测试工具

通过交互式命令行测试 SSU Python SDK 的存储空间分配、挂载、卸载等功能。

## 1. 前置条件

- **操作系统**：openEuler 24.03 LTS (aarch64)
- **Python**：3.11
- **UBSE 服务**：ubse daemon 已启动，集群状态正常
- **用户权限**：root 或 ubse 用户组成员

## 2. 部署

需要将以下文件拷贝到目标服务器：

- `ubse_ssu_test_python.py` — 测试工具
- `src/sdk/python/` — SSU Python SDK 源码目录（可选）

### 2.1 方式一：测试本地 SDK 源码（推荐）

将测试工具和 SDK 源码放在同一目录下，通过 `UBSE_SDK_DIR` 指定 SDK 路径：

```
/opt/ubse-test/
├── python/                          # SDK 源码 (src/sdk/python/)
│   ├── __init__.py
│   ├── ubs_engine_ssu.py
│   ├── ubs_engine_log.py
│   ├── ubs_engine_mem.py
│   ├── ubs_engine_npu.py
│   ├── ubs_engine_topo.py
│   ├── ubse_engine.py
│   ├── ffi/
│   ├── ipc/
│   └── models/
└── ubse_ssu_test_python.py          # 测试工具
```

```bash
UBSE_SDK_DIR=/opt/ubse-test/python python3 ubse_ssu_test_python.py
```

测试工具和 SDK 也可以放在不同目录，只需通过 `UBSE_SDK_DIR` 指定 SDK 的实际路径：

```
/opt/ubse-test/tools/ubse_ssu_test_python.py          # 测试工具
/opt/ubse-test/sdk/python/                             # SDK 源码
```

```bash
UBSE_SDK_DIR=/opt/ubse-test/sdk/python python3 /opt/ubse-test/tools/ubse_ssu_test_python.py
```

> **原理**：当 `UBSE_SDK_DIR` 指向 SDK 源码目录（目录下没有 `ubse/ipc` 子目录）时，工具会自动在临时目录构建 `ubse` 包结构，将源码通过符号链接映射为 `ubse` 包，优先使用本地 SDK，不影响系统已安装的 `ubse` 包。

### 2.2 方式二：测试系统已安装的 SDK

如果目标服务器已通过 RPM 安装了 `python3-ubs-engine` 包，SDK 位于 `/usr/lib/python3.11/site-packages/ubse/`，可直接测试，无需拷贝 SDK 源码：

```bash
# 不设置 UBSE_SDK_DIR，或指向已安装的 ubse 包的父目录
python3 ubse_ssu_test_python.py
```

此时 `UBSE_SDK_DIR` 默认为测试脚本所在目录，工具检测到该目录下没有 `ubse/ipc` 子目录，会自动在临时目录构建包结构。临时目录的 `ubse/` 中只包含指向测试脚本所在目录的符号链接，不包含 SDK 模块，Python 在临时目录中找不到所需模块后会回退到系统 `site-packages`，从而使用系统已安装的 `ubse` 包。

也可以显式将 `UBSE_SDK_DIR` 指向已安装的 `ubse` 包所在目录（即包含 `ubse/` 子目录的目录）：

```bash
UBSE_SDK_DIR=/usr/lib/python3.11/site-packages python3 ubse_ssu_test_python.py
```

此时工具检测到 `ubse/ipc` 子目录存在，直接使用已安装的 SDK，不构建临时包结构。

## 3. 运行

### 3.1 交互模式

```bash
UBSE_SDK_DIR=/opt/ubse-test/python python3 ubse_ssu_test_python.py
```

### 3.2 单次执行

```bash
UBSE_SDK_DIR=/opt/ubse-test/python python3 ubse_ssu_test_python.py ssu_list_alloc_info
```

### 3.3 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `UBSE_SDK_DIR` | SDK 路径，指向 `src/sdk/python/` 源码目录或已安装 `ubse` 包的父目录 | 测试脚本所在目录 |
| `UBSE_IPC_SOCKET_PATH` | ubse daemon socket 路径 | `/var/run/ubse/ubse.sock` |

## 4. 命令参考

所有参数为有序位置参数，不接受可选参数或省略。

### 4.1 存储空间管理

| 命令 | 参数 | 说明 |
|------|------|------|
| `ssu_list_alloc_info` | 无 | 列出所有已分配存储空间 |
| `ssu_alloc_space` | `<name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>` | 分配存储空间 |
| `ssu_free_space` | `<name>` | 释放存储空间（幂等） |

### 4.2 访问权限

| 命令 | 参数 | 说明 |
|------|------|------|
| `ssu_add_access_permission` | `<name> <nqn>` | 添加 Host 访问权限（幂等） |
| `ssu_remove_access_permission` | `<name> <nqn>` | 移除 Host 访问权限（幂等） |

### 4.3 挂载/卸载

| 命令 | 参数 | 说明 |
|------|------|------|
| `ssu_attach_space` | `<name> <nqn> <src_eid>` | 挂载存储空间 |
| `ssu_detach_space` | `<name> <nqn> <src_eid>` | 卸载存储空间 |
| `ssu_attach_linear_space` | `<name> <nqn> <src_eid> <dev_name>` | 挂载线性编址存储空间 |
| `ssu_detach_linear_space` | `<name> <nqn> <src_eid> <dev_name>` | 卸载线性编址存储空间 |
| `ssu_attach_striped_space` | `<name> <nqn> <src_eid> <dev_name> <level> <chunk_size>` | 挂载条带化编址存储空间 |
| `ssu_detach_striped_space` | `<name> <nqn> <src_eid> <dev_name> <level> <chunk_size>` | 卸载条带化编址存储空间 |

### 4.4 查询

| 命令 | 参数 | 说明 |
|------|------|------|
| `ssu_get_ns_stats` | `<name>` | 查询命名空间容量统计 |
| `ssu_get_connect_info` | `<name> <vfe_present> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>` | 查询 NVMe 连接信息 |
| `ssu_get_fe_device_list` | 无 | 查询 FE 设备列表 |

### 4.5 FE 设备管理

| 命令 | 参数 | 说明 |
|------|------|------|
| `ssu_fe_device_alloc` | `<upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>` | 将 VFE 绑定到虚拟机 |
| `ssu_fe_device_free` | `<upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>` | 释放 VFE 设备 |

| `quit` | 无 | 退出 |

## 5. 参数说明

### 5.1 枚举值

填写 SDK 底层十进制数值：

| 参数 | 值 | 说明 |
|------|----|------|
| `lba_format` | `4096` / `512` | LBA 格式 |
| `strategy` | `0` / `1` | 分配策略：0=分布式(striped)，1=顺序(linear) |
| `level` | `0` / `5` | RAID 级别：0=RAID0，5=RAID5 |
| `chunk_size` | `4` / `16` / `32` / `64` / `128` / `256` / `512` | 条带 chunk 大小(KB) |

### 5.2 特殊值

- 空字符串输入 `""`
- 数值零输入 `0`
- `ns_size` 只接受 uint64 十进制数值（如 `1073741824` 表示 1GiB），不接受 `1G` 等单位
- `vfe_present=0` 时向 SDK 传 nil，但仍须写全后续占位参数

### 5.3 GUID 字段

`vfe_guid`、`bind_bus_instance_guid`、`bus_instance_guid` 为 32 字符十六进制字符串，如 `00112233445566778899aabbccddeeff`。`bus_instance_guid` 可为空字符串 `""`。

## 6. 典型使用流程

### 6.1 分配并挂载存储空间

```
ubse_ssu_test_python> ssu_alloc_space myspace 81920 2 4096 0 ""
ubse_ssu_test_python> ssu_add_access_permission myspace nqn.2024-01.example:host-a
ubse_ssu_test_python> ssu_attach_space myspace nqn.2024-01.example:host-a 0000000000000001
```

### 6.2 查询状态

```
ubse_ssu_test_python> ssu_list_alloc_info
ubse_ssu_test_python> ssu_get_ns_stats myspace
ubse_ssu_test_python> ssu_get_connect_info myspace 0 0 0 0 0 0 "" ""
```

### 6.3 卸载并释放

```
ubse_ssu_test_python> ssu_detach_space myspace nqn.2024-01.example:host-a 0000000000000001
ubse_ssu_test_python> ssu_remove_access_permission myspace nqn.2024-01.example:host-a
ubse_ssu_test_python> ssu_free_space myspace
```

### 6.4 条带化编址

```
ubse_ssu_test_python> ssu_alloc_space myspace 1073741824 2 4096 0 ""
ubse_ssu_test_python> ssu_add_access_permission myspace nqn.2024-01.example:host-a
ubse_ssu_test_python> ssu_attach_striped_space myspace nqn.2024-01.example:host-a 0000000000000001 mydev 0 64
```

### 6.5 FE 设备管理

```
ubse_ssu_test_python> ssu_get_fe_device_list
ubse_ssu_test_python> ssu_fe_device_alloc 1 1 0 0 10 20 00112233445566778899aabbccddeeff 11112222333344445555666677778888 ""
ubse_ssu_test_python> ssu_fe_device_free 1 1 0 0 10 20 00112233445566778899aabbccddeeff 11112222333344445555666677778888
```

## 7. 错误说明

| 异常类型 | 含义 |
|----------|------|
| `UbsErrInvalidArg` | 参数校验错误 |
| `UbsEngineConnectionError` | 连接 ubse daemon 失败 |
| `UbsEngineAuthError` | 鉴权不通过 |
| `UbsEngineTimeoutError` | 服务端处理超时 |
| `UbsEngineInternalError` | 服务端内部错误（错误码 ≥10000 为内部错误，对外统一返回） |
| `UbsEngineNotExistError` | 存储空间不存在 |
| `UbsEngineAllocateError` | 分配算法失败 |
| `UbsEngineExistedError` | 实例已存在 |

## 8. 注意事项

- 释放和权限操作具有幂等性，对不存在的资源操作应返回成功
- 卸载存储空间前需确保没有进程正在使用
- RAID5 至少需要 3 个成员设备
- 当 `ns_num=1` 时 `strategy` 参数不生效
