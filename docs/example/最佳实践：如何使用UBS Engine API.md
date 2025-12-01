## 介绍
UBS Engine API 是基于 **C++语言** 开发的 **ubs_engine 管理接口**，能够实现资源调度与管理，执行关键运维操作。

本篇内容旨在帮助开发者快速掌握 ubs_engine API 的核心功能、适用场景及开发技巧，提供可直接运行的示例代码，并规避常见问题。

## 1. 前置条件

使用 API 前需完成 ubs_engine 的环境准备与安装启动，步骤如下：

### 1.1 业务运行环境安装

ubs_engine 运行的硬件要求、操作系统和软件要求详见[部署说明](../build_install/部署说明.md)。

### 1.2 ubs_engine 业务部署与启动

1. 获取 ubs_engine 最新版本的 rpm 包，并安装到系统。

```shell
rpm -ivh ubs-engine-1.0.0-1.aarch64.rpm
rpm -ivh ubs-engine-client-libs-1.0.0-1.aarch64.rpm
rpm -ivh ubs-engine-client-devel-1.0.0-1.aarch64.rpm 
```

```shell
# 预期输出示例
[root@controller manager]# rpm -ivh ubs-engine-1.0.0-1.aarch64.rpm
Verifying...                          ################################# [100%]
Preparing...                          ################################# [100%]
Updating / installing...
   1:ubs-engine-1.0.0-1               ################################# [100%]
Created symlink /etc/systemd/system/multi-user.target.wants/ubse.service → /usr/lib/systemd/system/ubse.service.
Checking and deleting semaphores...
delete ubse semaphores finished
[root@controller manager]# rpm -ivh ubs-engine-client-libs-1.0.0-1.aarch64.rpm
Verifying...                          ################################# [100%]
Preparing...                          ################################# [100%]
Updating / installing...
   1:ubs-engine-client-libs-1.0.0-1   ################################# [100%]
[root@controller manager]# rpm -ivh ubs-engine-client-devel-1.0.0-1.aarch64.rpm 
Verifying...                          ################################# [100%]
Preparing...                          ################################# [100%]
Updating / installing...
   1:ubs-engine-client-devel-1.0.0-1  ################################# [100%]

```

2. rpm 包安装完成后，各个节点均启动 ubs_engine 服务。

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

3. 等待约 1 分钟时间，检查节点启动、集群选主备是否成功。

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

若集群内不同节点的查询主备结果一致，则 **ubs_engine 业务启动已完成**

> 若集群内不同节点的查询主备结果不一致，则可参考 4.3 节中的情况说明进行检查

## 2. 使用 ubs_engine API 实现：以 numa 借用为例

numa 形态的远端内存借用为 ubs_engine 服务的常见场景，通过创建、查询、删除操作实现对内存资源的调度与管理。

> numa（non-uniform memory access，非统一内存访问）是一种计算机内存架构设计，主要用于多处理器（多核）系统。其核心特点是：内存的访问时间取决
> 于处理器（CPU）与内存之间的物理位置关系。具体来说，处理器访问本地内存（即同一NUMA节点内的内存）的速度更快，而访问其他节点的远程内存时延迟更高、
> 带宽更低。

### 2.1 使用动态库 libubse 实现 numa 借用创建、查询、归还

1. 基于 libubse 实现 numa 借用创建、查询、归还 ,  将下面的`example.c`代码放在`src/sdk/sample/`目录下。

```c
#include <stdio.h>
#include <libubse.h>

void print_mem_numa_desc(const ubse_mem_numa_desc_t *numa_desc)
{
    printf("Memory Resource Descriptor:\n");
    printf("+----------------------+------------+------------+\n");
    printf("| Name                 | numaid     | Status     |\n");
    printf("+----------------------+------------+------------+\n");
    printf("| %-20s | %-10ld | %-10s |\n",
           numa_desc->name,
           numa_desc->numaid,
           numa_desc->state == MEM_STATE_NORMAL ? "success" :
           numa_desc->state == MEM_STATE_ERROR ? "fault" : "restoring");
    printf("+----------------------+------------+------------+\n");
}

int main()
{
    // 测试创建numa内存
    ubse_mem_numa_create_example();

    // 测试查询numa内存
    ubse_mem_numa_get_example();

    // 测试删除numa内存
    ubse_mem_numa_delete_example();
    
    return 0;
}
```

2. 为本节点创建 numa 形态的远端内存

```c
void ubse_mem_numa_create_example()
{
    // 1. 准备调用参数
    const char *mem_name = "test_mem";
    const uint64_t mem_size = 256 * 1024 * 1024;    // 256MB
    ubse_mem_distance_t distance = MEM_DISTANCE_L0; // 直连节点
    ubse_mem_numa_desc_t numa_desc = {0};           // 结果存储结构体

    // 2. 调用创建函数
    ubs_error_t result = ubse_mem_numa_create(mem_name, mem_size, distance, &numa_desc);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource created successfully!\n");
        print_mem_numa_desc(&numa_desc);
    } else {
        // 错误处理
        printf("Error creating memory resource: %s\n", ubs_error_string(result));
    }
}
```

```shell
# 预期输出示例
2025-09-26 11:39:40.852 INFO [ubse_uds_client.cpp:Connect:80] Connection successful
2025-09-26 11:39:40.853 INFO [ubse_ipc_client.cpp:ubse_invoke_call:96] Sending request, module_code=1, op_code=16
2025-09-26 11:39:40.853 INFO [ubse_uds_client.cpp:Send:136] Starting Send operation with timeout: 600000ms
2025-09-26 11:39:40.853 INFO [ubse_uds_client.cpp:Send:165] Waiting for response...
2025-09-26 11:39:41.300 INFO [ubse_uds_client.cpp:Receive:172] Starting Receive operation with remaining time: 600000ms
2025-09-26 11:39:41.300 INFO [ubse_uds_client.cpp:Receive:206] Receive operation completed successfully
2025-09-26 11:39:41.300 INFO [ubse_ipc_client.cpp:ubse_invoke_call:103] Sending request successfully, module_code=1, op_code=16
Memory resource created successfully!
Memory Resource Descriptor:
+----------------------+------------+------------+
| Name                 | numaid     | Status     |
+----------------------+------------+------------+
| test_mem             | 2          | success    |
+----------------------+------------+------------+

```

3. 查询本节点 numa 形态远端内存信息

```c
void ubse_mem_numa_get_example()
{
    // 1. 准备调用参数
    const char *mem_name = "test_mem";
    ubse_mem_numa_desc_t numa_desc = {0}; // 结果存储结构体

    // 2. 调用查询函数
    ubs_error_t result = ubse_mem_numa_get(mem_name, &numa_desc);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource get successfully!\n");
        print_mem_numa_desc(&numa_desc);
    } else {
        // 错误处理
        printf("Error get memory resource: %s\n", ubs_error_string(result));
    }
}

```

```shell
# 预期输出示例
2025-09-26 11:39:41.300 INFO [ubse_uds_client.cpp:Connect:80] Connection successful
2025-09-26 11:39:41.300 INFO [ubse_ipc_client.cpp:ubse_invoke_call:96] Sending request, module_code=1, op_code=19
2025-09-26 11:39:41.300 INFO [ubse_uds_client.cpp:Send:136] Starting Send operation with timeout: 600000ms
2025-09-26 11:39:41.300 INFO [ubse_uds_client.cpp:Send:165] Waiting for response...
2025-09-26 11:39:41.301 INFO [ubse_uds_client.cpp:Receive:172] Starting Receive operation with remaining time: 600000ms
2025-09-26 11:39:41.301 INFO [ubse_uds_client.cpp:Receive:206] Receive operation completed successfully
2025-09-26 11:39:41.301 INFO [ubse_ipc_client.cpp:ubse_invoke_call:103] Sending request successfully, module_code=1, op_code=19
Memory resource get successfully!
Memory Resource Descriptor:
+----------------------+------------+------------+
| Name                 | numaid     | Status     |
+----------------------+------------+------------+
| test_mem             | 2          | success    |
+----------------------+------------+------------+

```

4. 删除指定 numa 形态的远端内存

```c
void ubse_mem_numa_delete_example()
{
    // 1. 准备调用参数
    const char *mem_name = "test_mem";

    // 2. 调用删除函数
    ubs_error_t result = ubse_mem_numa_delete(mem_name);
    // 3. 处理结果
    if (result == UBS_SUCCESS) {
        printf("Memory resource delete successfully!\n");
    } else {
        // 错误处理
        printf("Error delete memory resource: %s\n", ubs_error_string(result));
    }
}
```

```shell
# 预期输出示例
2025-09-26 11:26:01.379 INFO [ubse_uds_client.cpp:Connect:80] Connection successful
2025-09-26 11:26:01.379 INFO [ubse_ipc_client.cpp:ubse_invoke_call:96] Sending request, module_code=1, op_code=21
2025-09-26 11:26:01.379 INFO [ubse_uds_client.cpp:Send:136] Starting Send operation with timeout: 600000ms
2025-09-26 11:26:01.379 INFO [ubse_uds_client.cpp:Send:165] Waiting for response...
2025-09-26 11:26:01.828 INFO [ubse_uds_client.cpp:Receive:172] Starting Receive operation with remaining time: 600000ms
2025-09-26 11:26:01.828 INFO [ubse_uds_client.cpp:Receive:206] Receive operation completed successfully
2025-09-26 11:26:01.828 INFO [ubse_ipc_client.cpp:ubse_invoke_call:103] Sending request successfully, module_code=1, op_code=21
Memory resource delete successfully!
```

## 3. 使用 ubs_engine API 实现：以借用账本查询为例

### 3.1 使用命令行工具 ubsectl 实现借用账本查询

```shell
ubsectl display memory -t borrow_detail
```

```shell
# 预期输出示例

# 借用账本为空
INFO: The borrow detail information is empty.

# 存在内存借用
---------------------------------------------------------------------------------------
name                type   borrow_node   borrow_size   lend_node    lend_size   status   
---------------------------------------------------------------------------------------
numa_Func_demo_123  numa   controller(1) 256           node01(2)    256         done  
---------------------------------------------------------------------------------------

```

## 4. 注意事项

1. **权限要求**：

- 用户权限：`ubsectl` 命令仅支持安装时自动创建的 ubse 用户以及加入 ubse 组的自定义用户，不支持 root 用户直接调用。

- 使用 root 用户：若需使用 root 用户执行`ubsectl`命令，应使用以下格式：

```bash
sudo -u ubse ubsectl [-h | --help] COMMAND TYPE [-h | --help][OPTIONS]
```

2. **构建第二节中示例并执行**：

修改`src/sdk/sample/CMakeList.txt`，添加下列内容。

```plaintext
add_executable(example EXCLUDE_FROM_ALL example.c)
target_link_libraries(example PRIVATE ubse_sdk)
```

构建`example.c`，生成 example 二进制文件，在`cmake-build-release/bin`目录下。

```shell
bash build.sh example
```

使用 root 用户：若需使用 root 用户执行，需修改 example 文件权限为 777.

```shell
chmod 777 cmake-build-release/bin/example
```

3. **启动异常分析**

- **节点查询报错**：如果查询节点报错内容为`ERROR: Failed to obtain cluster information`，则表示当前节点尚未完成启动过程或启动异常。可以通过查询业务进程状态，若进程存在说明节点仍在启动中，请稍作等待再重试。
- **节点查询主备信息不全**：如果查询节点的主节点是自己，而备节点状态为空，则表示当前节点尚未成功加入集群。建议首先检查节点间的通信是否正常。
