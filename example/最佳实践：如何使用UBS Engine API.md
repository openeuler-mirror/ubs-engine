UBS Engine API 是基于 C++ 语言开发的 ubs engine 管理接口，能够实现资源调度与管理，执行关键业务操作。

本篇内容旨在帮助开发者快速掌握 ubs engine API 的核心功能、适用场景及开发技巧，提供可直接运行的示例代码，并规避常见问题。

## 1. 前置条件

使用 ubs engine API 进行开发，需按如下步骤完成 ubs engine 的开发环境准备与开发包安装。

### 1.1 环境要求

* **操作系统**： Linux 

### 1.2 安装开发包

```bash
# 安装开发包（用于集成开发）
sudo dnf install -y ubs-engine-client-libs-1.0.0-1.aarch64.rpm ubs-engine-client-devel-1.0.0-1.aarch64.rpm
```

### 1.3 安装结果

| 文件/目录                      | 其它说明                                     |
| ------------------------------ | -------------------------------------------- |
| `/usr/include/ubse`            | 目录下头文件（`*.h`）权限：`644`             |
| `/usr/lib64/libubse-client.so` | 软链接，指向`/usr/lib64/libubse-client.so.1` |
| `/usr/lib64/libubse-client.a`  | 二进制静态库                                 |



## 2. 使用 ubs engine API 实现：以 fd 借用为例

fd 形态的远端内存借用为 ubs engine 服务的常见场景，通过创建、查询、删除操作实现对内存资源的调度与管理。

> fd 借用是 OBMM（Open Memory Management Module）提供的一种内存借用机制，通过返回文件描述符（FD）的方式，使应用能够访问借用的内存资源。

### 2.1 实现逻辑概述

fd 借用的实现可以分为以下四个主要步骤：

* 创建 fd 借用：通过 ubs engine API 申请借用内存资源，并通过内存映射获取对应的文件描述符（fd）。

* 查询 fd 借用：通过 ubs engine API 根据借用名称查询对应的借用内存，以便进行后续的读写操作。

* 读写 fd 借用：通过 fd 对应的内存地址，直接进行读写操作。

* 归还 fd 借用：完成操作后，解除内存映射、关闭设备文件并通过 ubs engine API 归还内存资源。

### 2.2 实现代码

文件 [fd_demo.c](fd_demo.c) 给出了 fd 借用的实现代码，用户可参考该代码内容，实现 fd 借用创建、查询、读写、归还流程。

### 2.3 预期输出

```shell
# 预期输出示例
Testing ubse_mem_fd_create...
2025-11-05 10:21:10.062 INFO [ubse_uds_client.cpp:Connect:76] Connection successful
2025-11-05 10:21:10.062 INFO [ubse_ipc_client.cpp:ubse_invoke_call:97] Sending request, module_code=1, op_code=1
2025-11-05 10:21:10.062 INFO [ubse_uds_client.cpp:Send:153] Starting Send operation with timeout: 600000ms
2025-11-05 10:21:10.062 INFO [ubse_uds_client.cpp:Send:182] Waiting for response...
2025-11-05 10:21:10.131 INFO [ubse_uds_client.cpp:Receive:227] Starting Receive operation with remaining time: 600000ms
2025-11-05 10:21:10.132 INFO [ubse_uds_client.cpp:Receive:261] Receive operation completed successfully
2025-11-05 10:21:10.132 INFO [ubse_ipc_client.cpp:ubse_invoke_call:104] Sending request successfully, module_code=1, op_code=1
Memory resource created successfully!
Memory Resource Descriptor:
Resource Name: fd_123
Total Size: 134217728 bytes
Unit Size: 134217728 bytes
Memory IDs: [1]

Testing ubse_mem_fd_get...
2025-11-05 10:21:10.132 INFO [ubse_uds_client.cpp:Connect:76] Connection successful
2025-11-05 10:21:10.132 INFO [ubse_ipc_client.cpp:ubse_invoke_call:97] Sending request, module_code=1, op_code=5
2025-11-05 10:21:10.132 INFO [ubse_uds_client.cpp:Send:153] Starting Send operation with timeout: 600000ms
2025-11-05 10:21:10.132 INFO [ubse_uds_client.cpp:Send:182] Waiting for response...
2025-11-05 10:21:10.132 INFO [ubse_uds_client.cpp:Receive:227] Starting Receive operation with remaining time: 600000ms
2025-11-05 10:21:10.132 INFO [ubse_uds_client.cpp:Receive:261] Receive operation completed successfully
2025-11-05 10:21:10.132 INFO [ubse_ipc_client.cpp:ubse_invoke_call:104] Sending request successfully, module_code=1, op_code=5
Memory resource get successfully!
Memory Resource Descriptor:
Resource Name: fd_123
Total Size: 134217728 bytes
Unit Size: 134217728 bytes
Memory IDs: [1]
Device opened: /dev/obmm_shmdev1, mapped size: 134217728 bytes

Testing write and read...
Written to memory: Hello World!
Read from memory: Hello World!
Device closed.

Testing ubse_mem_fd_delete...
2025-11-05 10:21:10.148 INFO [ubse_uds_client.cpp:Connect:76] Connection successful
2025-11-05 10:21:10.148 INFO [ubse_ipc_client.cpp:ubse_invoke_call:97] Sending request, module_code=1, op_code=7
2025-11-05 10:21:10.148 INFO [ubse_uds_client.cpp:Send:153] Starting Send operation with timeout: 600000ms
2025-11-05 10:21:10.148 INFO [ubse_uds_client.cpp:Send:182] Waiting for response...
2025-11-05 10:21:10.198 INFO [ubse_uds_client.cpp:Receive:227] Starting Receive operation with remaining time: 600000ms
2025-11-05 10:21:10.199 INFO [ubse_uds_client.cpp:Receive:261] Receive operation completed successfully
2025-11-05 10:21:10.199 INFO [ubse_ipc_client.cpp:ubse_invoke_call:104] Sending request successfully, module_code=1, op_code=7
Memory resource delete successfully!


```

## 3. 使用 ubs engine API 实现：以 numa 借用为例

numa 形态的远端内存借用为 ubs engine 服务的常见场景，通过创建、查询、删除操作实现对内存资源的调度与管理。

> numa（non-uniform memory access，非统一内存访问）是一种计算机内存架构设计，主要用于多处理器（多核）系统。其核心特点是：内存的访问时间取决于处理器（CPU）与内存之间的物理位置关系。具体来说，处理器访问本地内存（即同一NUMA节点内的内存）的速度更快，而访问其他节点的远程内存时延迟更高、带宽更低。

### 3.1 实现逻辑概述
numa 借用的实现可以分为以下四个主要步骤：

* 创建 numa 借用：通过 ubs engine API 申请借用内存资源，并通过指定 numa 节点分配内存。

* 查询 numa 借用：通过 ubs engine API 根据借用名称查询对应的借用内存，以便进行后续的读写操作。

* 读写 numa 借用：通过 numa 对应的内存地址，直接进行读写操作。

* 归还 numa 借用：完成操作后，释放指定 numa 节点的内存并通过 ubs engine API 归还内存资源。

### 3.2 numa.h 依赖安装

在实现过程中，可能会遇到编译器报错 “fatal error: numa.h: 没有那个文件或目录”。安装 `numactl` 库可以解决缺少`numa.h`文件的问题。

```bash
# 安装numactl库
# CentOS/ Euler: 
sudo dnf install numactl

# Ubuntu: 
sudo apt install numactl
```

安装完成后，可以通过 `numactl -H` 检查 numactl 是否安装成功。同时，可通过`find / -name "numa.h"`检查头文件位置。

### 3.3 实现代码

文件 [numa_demo.c](numa_demo.c) 给出了 numa 借用的实现代码，用户可参考该代码内容，实现 numa 借用创建、查询、读写、归还流程。

### 3.4 预期输出

```shell
# 预期输出示例
Testing ubse_mem_numa_create...
Memory resource created size:134217728!
2025-11-05 10:21:25.528 INFO [ubse_uds_client.cpp:Connect:76] Connection successful
2025-11-05 10:21:25.528 INFO [ubse_ipc_client.cpp:ubse_invoke_call:97] Sending request, module_code=1, op_code=16
2025-11-05 10:21:25.528 INFO [ubse_uds_client.cpp:Send:153] Starting Send operation with timeout: 600000ms
2025-11-05 10:21:25.528 INFO [ubse_uds_client.cpp:Send:182] Waiting for response...
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Receive:227] Starting Receive operation with remaining time: 600000ms
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Receive:261] Receive operation completed successfully
2025-11-05 10:21:25.897 INFO [ubse_ipc_client.cpp:ubse_invoke_call:104] Sending request successfully, module_code=1, op_code=16
Memory resource created successfully!
Memory Resource Descriptor:
Resource Name: numa_123
Numaid: 3 

Testing ubse_mem_fd_get...
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Connect:76] Connection successful
2025-11-05 10:21:25.897 INFO [ubse_ipc_client.cpp:ubse_invoke_call:97] Sending request, module_code=1, op_code=19
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Send:153] Starting Send operation with timeout: 600000ms
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Send:182] Waiting for response...
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Receive:227] Starting Receive operation with remaining time: 600000ms
2025-11-05 10:21:25.897 INFO [ubse_uds_client.cpp:Receive:261] Receive operation completed successfully
2025-11-05 10:21:25.897 INFO [ubse_ipc_client.cpp:ubse_invoke_call:104] Sending request successfully, module_code=1, op_code=19
Memory resource get successfully!
Memory Resource Descriptor:
Resource Name: numa_123
Numaid: 3 
Memory allocated on NUMA 3

Testing write and read ...
Written to memory: Hello World!
Read from memory: Hello World!

Testing ubse_mem_numa_delete...
2025-11-05 10:21:25.898 INFO [ubse_uds_client.cpp:Connect:76] Connection successful
2025-11-05 10:21:25.898 INFO [ubse_ipc_client.cpp:ubse_invoke_call:97] Sending request, module_code=1, op_code=21
2025-11-05 10:21:25.898 INFO [ubse_uds_client.cpp:Send:153] Starting Send operation with timeout: 600000ms
2025-11-05 10:21:25.898 INFO [ubse_uds_client.cpp:Send:182] Waiting for response...
2025-11-05 10:21:26.907 INFO [ubse_uds_client.cpp:Receive:227] Starting Receive operation with remaining time: 600000ms
2025-11-05 10:21:26.907 INFO [ubse_uds_client.cpp:Receive:261] Receive operation completed successfully
2025-11-05 10:21:26.907 INFO [ubse_ipc_client.cpp:ubse_invoke_call:104] Sending request successfully, module_code=1, op_code=21
Memory resource delete successfully!
```


## 4. 使用 ubs engine API 实现：以借用账本查询为例

### 4.1 使用命令行工具 ubsectl 实现借用账本查询

```shell
ubsectl display memory -t borrow_detail
```

### 4.2 预期输出
```shell
# 预期输出示例
# 借用账本为空
INFO: The borrow detail information is empty.

# 存在内存借用
----------------------------------------------------------------------------------
  name       type   borrow_node     borrow_size   lend_node   lend_size   status   
----------------------------------------------------------------------------------
  numa_123   numa   controller(1)   256           node01(2)   256         done     
----------------------------------------------------------------------------------
```

## 5. 运行示例

### 5.1 业务环境准备

**环境要求**

* **操作系统**： openEuler 24.03 LTS 或更高版本
* **CPU架构**： aarch64
* **内存**： ≥ 64GB
* **磁盘**： SSD，IOPS 500MB/s
* **芯片互联**： UB
* **网卡**：可选依赖（可选使用TCP辅助UB建链，默认采用UB自举建链）
* **用户权限**： 安装与管理需 `root` 权限
> 更多要求详见[部署说明](docs/build_install/部署说明.md)。

RPM 包安装 （所有集群节点均需安装 `ubs-engine` 主包。）

```bash
# 安装主程序包
sudo dnf install -y ubs-engine-1.0.0-1.aarch64.rpm

# 安装客户端运行时库（第三方集成必需）
sudo dnf install -y ubs-engine-client-libs-1.0.0-1.aarch64.rpm
```
启动 ubs engine 服务，各个节点均需启动。

```shell
systemctl start ubse
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
sudo -u ubse ubsectl display cluster
```

```shell
# 预期输出示例，以两节点环境为例
---------------------------------------------------------------------
  node            role      bonding-eid                               
---------------------------------------------------------------------
  controller(1)   master    4245:4944:0000:0000:0000:0000:0100:0000   
                                                                     
  node01(2)       standby   4245:4944:0000:0000:0000:0000:0200:0000     
---------------------------------------------------------------------
```


### 5.2 构建第 2、3 节的示例 demo 
> 确保你的 CMake 版本至少为 3.11。

在example目录下创建一个用于构建的目录，通常命名为`build`：

```bash
mkdir build
```

进入构建目录并运行CMake：

```bash
cd build
cmake ..
```

编译项目

```bash
make
```

编译完成后，可执行文件`numa_demo`和`fd_demo`将生成在`build`目录中。需手动将可执行文件转移至业务运行环境，并按照业务要求修改文件权限。

### 5.3 运行示例程序

```bash
# 运行示例程序：fd 借用
./fd_demo fd_123

# 运行示例程序：numa 借用
./numa_demo numa_123

# 运行示例程序：借用账本查询
ubsectl display memory -t borrow_detail
```

### 5.4 注意事项

* **权限要求**：

    * 用户权限：`ubsectl` 命令仅支持安装时自动创建的 ubse 用户以及加入 ubse 组的自定义用户，不支持 root 用户直接调用。

    * 使用 root 用户：若需使用 root 用户执行 `ubsectl` 命令，应使用以下格式：

```bash
sudo -u ubse ubsectl [-h | --help] COMMAND TYPE [-h | --help][OPTIONS]
```

* **启动异常分析**

    * 节点查询报错：如果查询节点报错内容为 `ERROR: Failed to obtain cluster information`，则表示当前节点尚未完成启动过程或启动异常。可以通过查询业务进程状态，若进程存在说明节点仍在启动中，请稍作等待再重试。

    * 节点查询主备信息不全：如果查询节点的主节点是自己，而备节点状态为空，则表示当前节点尚未成功加入集群。建议首先检查节点间的通信是否正常。


