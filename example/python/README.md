# UBSE Python API 最佳实践

UBS Engine Python API 是基于 Python 语言开发的 ubs engine 管理接口，能够实现资源调度与管理，执行关键业务操作。

本篇内容旨在帮助开发者快速掌握 ubs engine Python API 的核心功能、适用场景及开发技巧，提供可直接运行的示例代码，并规避常见问题。

## 1. 前置条件

使用 ubs engine Python API 进行开发，需按如下步骤完成 ubs engine 的开发环境准备与开发包安装。

### 1.1 环境要求

* **操作系统**：Linux
* **Python版本**：Python 3.11

### 1.2 安装开发包

```bash
# 安装开发包（用于集成开发）
sudo dnf install -y ubs-engine-client-libs python3-ubs-engine
```

### 1.3 安装结果

| 文件/目录                      | 其它说明                            |
| ------------------------------ |---------------------------------|
| `/usr/lib/python3.11/site-packages/ubse/`            | 目录下头文件（`*.py`）权限：`644`          |
| `/usr/lib64/libubse-client.so.1` | 软链接，指向`libubse-client.so.xx.xx.xx` |
| `/usr/lib64/libubse-client.so.xx.xx.xx`  | 二进制动态库                          |

### 1.4 导入 Python 模块

在 Python 项目中，需要导入以下模块：

```python
from ubse.ubs_engine_topo import ubs_topo_node_list, ubs_topo_node_local_get, ubs_topo_link_list
from ubse.ubs_engine_mem import ubs_mem_numa_list
from ubse.ubs_engine_log import LogLevel, ubs_engine_log_callback_register
from ubse.ubse_engine import ubs_engine_client_initialize, ubs_engine_client_finalize
```

## 2. 使用 ubs engine Python API 实现：以拓扑查询为例

拓扑查询是 ubs engine 服务的常见场景，通过查询节点信息、连接信息实现对集群资源的调度与管理。

### 2.1 实现逻辑概述

拓扑查询的实现可以分为以下四个主要步骤：

* 注册日志回调：通过 `ubs_engine_log_callback_register` 注册日志回调函数，便于调试和监控。

* 查询拓扑信息：通过 `ubs_topo_node_list` 查询全量节点信息，通过 `ubs_topo_node_local_get` 查询本节点信息，通过 `ubs_topo_link_list` 查询全量连接信息。

### 2.2 实现代码

文件 [test_ubs_engine_topo.py](test_ubs_engine_topo.py) 给出了拓扑查询的实现代码，用户可参考该代码内容，实现拓扑查询流程。

### 2.3 预期输出

```shell
# 预期输出示例
自定义日志处理函数注册成功
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=3, op_code=2
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=3, op_code=2
获取到 2 条 CPU 拓扑信息：
Link : 1-36-1 <-> 2-36-1
Link : 1-236-1 <-> 2-236-1
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=3, op_code=1
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=3, op_code=1
获取到 2 条 节点信息：
Node 2 ('node01'): 2 sockets, IPs: [IPv4:192.168.122.1, IPv4:192.168.1.11, IPv4:192.168.100.101]
Node 1 ('controller'): 2 sockets, IPs: [IPv4:192.168.122.1, IPv4:192.168.1.10, IPv4:192.168.100.100]
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=3, op_code=4
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=3, op_code=4
Node 1 ('controller'): 2 sockets, IPs: [IPv4:192.168.122.1, IPv4:192.168.1.10, IPv4:192.168.100.100]
```

## 3. 使用 ubs engine Python API 实现：以 NUMA 内存查询为例

NUMA 内存查询是 ubs engine 服务的常见场景，通过查询 NUMA 形态的远端内存信息实现对内存资源的管理。

### 3.1 实现逻辑概述

NUMA 内存查询的实现可以分为以下四个主要步骤：

* 查询 NUMA 内存：通过 `ubs_mem_numa_list` 查询本节点所有 NUMA 形态远端内存。

* 处理查询结果：解析返回的内存描述信息，包括借用标识、NUMA ID、借用大小、借出/借入节点信息等。

### 3.2 实现代码

文件 [test_ubs_engine_mem.py](test_ubs_engine_mem.py) 给出了 NUMA 内存查询的实现代码，用户可参考该代码内容，实现内存查询流程。

### 3.3 预期输出

```shell
# 预期输出示例
自定义日志处理函数注册成功
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=1, op_code=20
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=1, op_code=20
获取到 0 条 节点信息：
```

## 4. 运行示例

### 4.1 业务环境准备

**环境要求**

* **操作系统**：openEuler 24.03 LTS 或更高版本
* **CPU架构**：aarch64
* **内存**：≥ 64GB
* **磁盘**：SSD，IOPS 500MB/s
* **芯片互联**：UB
* **网卡**：可选依赖（可选使用TCP辅助UB建链，默认采用UB自举建链）
* **用户权限**：安装与管理需 `root` 权限

> 更多要求详见[部署说明](../../docs/build_install/部署说明.md)。

RPM 包安装（所有集群节点均需安装 `ubs-engine` 主包。）

```bash
# 安装主程序包
sudo dnf install -y ubs-engine

# 安装客户端运行时库（第三方集成必需）
sudo dnf install -y ubs-engine-client-libs

# 安装 Python 模块（第三方集成必需）
sudo dnf install -y python3-ubs-engine
```

启动 ubs engine 服务，各个节点均需启动。

ubs engine 默认开启安全通信，需确保成功导入证书，否则集群状态可能异常，证书导入参考：[ubse_cli_user_guide](../../docs/zh/ubse_cli_user_guide.md#证书管理)，或修改配置文件 `/etc/ubse/ubse.conf`，将 `cert.use` 设置为 `false`，关闭安全通信。

```shell
sudo systemctl start ubse
```

```bash
# 查看业务进程状态
$ ps -ef | grep ubse
--------------------------------------------------------------------------
ubse       64120       1 99 15:14 ?        00:18:18 /usr/bin/ubse
root     2182344 2181443  0 15:52 pts/3    00:00:00 grep --color=auto ubse

```

等待约 1 分钟时间，检查节点启动、集群选主备是否成功。

```shell
sudo ubsectl display cluster
```

```shell
# 预期输出示例，以两节点环境为例
-------------------------------------------------------------------------------------------------------------
  node            role      bonding-eid                               guid
-------------------------------------------------------------------------------------------------------------
  controller(1)   master    4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800010000
                                                                     
  node01(2)       standby   4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801000000
-------------------------------------------------------------------------------------------------------------
```

### 4.2 运行示例程序

#### 4.2.1 配置客户端程序

关于UBSE鉴权机制，详见[UBSE角色访问控制设计](../../docs/design/UBSE角色访问控制设计.md)。
下发配置的用户需有 root 权限，假设使用UBS ENGINE API 接口的用户为 test_user。

1. 将 test_user 加入 ubse 用户组中。

    ```bash
    sudo usermod -aG ubse test_user
    ```

2. 创建客户端自定义权限配置文件。

    ```bash
    sudo touch /etc/ubse/auth_test_api.conf
    ```

3. 编辑客户端自定义权限配置文件

    ```bash
    sudo vi /etc/ubse/auth_test_api.conf
    ```

    按 i 键进入插入模式，将以下内容复制粘贴到文件中。

    ```bash
    # Authentication configuration file
    # Defines base user-role and role-permission mappings with priority flags.

    [auth.user]
    # Section for user-to-role mappings.
    test_user=test

    [auth.role]
    # Section for role-to-permission mappings.
    test=mem.numa,topo
    ```

    完成后，按 Esc 键退出插入模式，输入 :wq 并按回车键保存并退出编辑器。

4. 重启 ubse 服务，使配置生效。

    ```bash
    sudo systemctl restart ubse
    ```

#### 4.2.2 运行客户端程序

将 python 示例程序转移至业务运行环境中，并按照业务要求修改文件权限。

```bash
# 运行拓扑查询示例
python3 ./test_ubs_engine_topo.py

# 运行内存查询示例
python3 ./test_ubs_engine_mem.py
```

### 4.3 注意事项

* **权限要求**：

    * 用户权限：`ubsectl` 命令仅支持 root 、安装时自动创建的 ubse 用户使用；api 接口仅支持 root 、安装时自动创建的 ubse 用户以及加入 ubse 组的自定义用户使用，如为加入 ubse 组的自定义用户使用，需要配置客户端自定义权限配置文件。

* **启动异常分析**

    * 节点查询报错：如果查询节点报错内容为 `ERROR: Failed to obtain cluster information`，则表示当前节点尚未完成启动过程或启动异常。可以通过查询业务进程状态，若进程存在说明节点仍在启动中，请稍作等待再重试。

    * 节点查询主备信息不全：如果查询节点的主节点是自己，而备节点状态为空，则表示当前节点尚未成功加入集群。建议首先检查节点间的通信是否正常。

* **Python 版本**：

    * 使用 UBS Engine Python API 依赖 Python 3.11 版本。
