The UBS Engine C API is a UBS Engine management interface developed based on the C++ language. It is used to schedule and manage resources and perform key service operations.

This document aims to help developers quickly master the core functions, application scenarios, and development skills of the UBS Engine C API, provide sample code that can be directly executed, and avoid common issues.

## 1. Prerequisites

To use the UBS Engine C API for development, prepare the UBS Engine development environment and install the development package by following the steps below.

### 1.1 Environment Requirements

* **OS**: Linux

### 1.2 Development Package Installation

```bash
# Installing the development package (for integrated development)
sudo dnf install -y ubs-engine-client-libs ubs-engine-client-devel
```

### 1.3 Installation Result

| File/Directory                     | Other Information                                    |
| ------------------------------ | -------------------------------------------- |
| `/usr/include/ubse`            | Permission on the header file (`*.h`) in the directory: `644`            |
| `/usr/lib64/libubse-client.so` | Soft link, pointing to `/usr/lib64/libubse-client.so.1`|
| `/usr/lib64/libubse-client.so.1` | Soft link, pointing to `libubse-client.so.xx.xx.xx`|
| `/usr/lib64/libubse-client.so.xx.xx.xx`  | Binary dynamic library                         |
| `/usr/lib64/libubse-client.a`  | Binary static library                                |

## 2. Using the UBS Engine C API: FD Borrowing as an Example

Remote memory borrowing in file descriptor (FD) form is a common scenario of the UBS Engine service. Memory resources can be scheduled and managed through creation, query, and deletion.

> FD borrowing is a memory borrowing mechanism provided by the Open Memory Management Module (OBMM). It allows applications to access borrowed memory resources by returning file descriptors.

### 2.1 Implementation Logic Overview

The implementation of FD borrowing can be divided into the following four main steps:

* Creating an FD borrowing: Use the UBS Engine C API to request memory resources, and obtain the corresponding file descriptor (FD) through memory mapping.

* Querying an FD borrowing: Use the UBS Engine C API to query the borrowed memory based on the borrowing name for subsequent read and write operations.

* Reading and writing an FD borrowing: Directly perform read and write operations using the memory address corresponding to the FD.

* Returning an FD borrowing: After the operations are complete, unmap the memory, close the device file, and return the memory resources through the UBS Engine API.

### 2.2 Implementation Code

The [fd_demo.c](fd_demo.c) file provides the implementation code for FD borrowing. You can refer to this code to implement the creation, query, read/write, and return processes of FD borrowing.

### 2.3 Expected Output

```shell
# Example of the expected output
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

## 3. Using the UBS Engine C API: NUMA Borrowing as an Example

Remote memory borrowing in non-uniform memory access (NUMA) form is a common scenario of the UBS Engine service. Memory resources can be scheduled and managed through creation, query, and deletion.

> NUMA is a computer memory architecture design mainly used in multi-processor (multi-core) systems. The core feature of NUMA is that the memory access time depends on the physical location relationship between the processor (CPU) and the memory. Specifically, a processor accesses a local memory (that is, a memory in a same NUMA node) at a higher speed, and accesses a remote memory of another node at a higher latency and a lower bandwidth.

### 3.1 Implementation Logic Overview

The implementation of NUMA borrowing can be divided into the following four main steps:

* Creating an NUMA borrowing: Use the UBS Engine API to request memory resources, and specify a NUMA node for allocating memory.

* Querying an NUMA borrowing: Use the UBS Engine API to query the borrowed memory based on the borrowing name for subsequent read and write operations.

* Reading and writing an NUMA borrowing: Directly perform read and write operations using the memory address corresponding to the NUMA.

* Returning an NUMA borrowing: After the operations are complete, release the memory of the specified NUMA node and return the memory resources through the UBS Engine API.

### 3.2 Installing the numa.h Dependency

During the implementation, the compiler may report the error message "fatal error: numa.h: The file or directory does not exist." You can install the `numactl-devel` library to solve the problem that the `numa.h` file is missing.

```bash
# Install the numactl-devel library.
# CentOS/ Euler: 
sudo dnf install numactl-devel

# Ubuntu: 
sudo apt install numactl-devel
```

After the installation is complete, run the `rpm -qi numactl-devel` command to check whether the numactl-devel library is successfully installed. In addition, you can run the `find / -name "numa.h"` command to check the location of the header file.

### 3.3 Implementation Code

The [numa_demo.c](numa_demo.c) file provides the implementation code for NUMA borrowing. You can refer to this code to implement the creation, query, read/write, and return processes of NUMA borrowing.

### 3.4 Expected Output

```shell
# Example of the expected output
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

## 4. Using the UBS Engine C API: Borrowing Detail Query as an Example

### 4.1 Using the Command Line Tool ubsectl to Query the Borrowing Details

```shell
sudo ubsectl display memory -t borrow_detail
```

### 4.2 Expected Output

```shell
# Example of the expected output
# The borrowing details are empty.
INFO: The borrow detail information is empty.

# Memory borrowing exists.
-----------------------------------------------------------------------------------------
  name       type   borrow_node     lend_node   lend_numa   lend_size   status   handle
-----------------------------------------------------------------------------------------
  numa_123   numa   controller(1)   node01(2)   1(236)      256         done     3
-----------------------------------------------------------------------------------------
```

## 5. Running Example

### 5.1 Preparing the Service Environment

**Environment requirements**

* **OS**: openEuler 24.03 LTS or later
* **CPU architecture**: AArch64
* **Memory**: ≥ 64 GB
* **Disk**: SSD, IOPS 500 MB/s
* **Chip interconnection**: UB
* **NIC**: optional dependency (TCP can be used to assist UB in link establishment. By default, UB bootstrap is used for link establishment.)
* **User permission**: The root permission is required for installation and management.

> For more requirements, see [Deployment Description](../../docs/build_install/Deployment_Description.md).

Install the RPM package. (The `ubs-engine` main package must be installed on all cluster nodes.)

```bash
# Install the main program package.
sudo dnf install -y ubs-engine

# Install the client runtime library (mandatory for third-party integration).
sudo dnf install -y ubs-engine-client-libs
```

Start the UBS Engine service on each node.

By default, secure communication is enabled for UBS Engine. Ensure that the certificate is successfully imported. Otherwise, the cluster status may be abnormal. For details about how to import the certificate, see [ubsectl_cert](../../docs/cli/ubsectl_cert.md). Alternatively, you can modify the `/etc/ubse/ubse.conf` configuration file and set `cert.use` to `false` to disable secure communication.

```shell
sudo systemctl start ubse
```

```bash
# Check the service process status.
$ ps -ef | grep ubse
--------------------------------------------------------------------------
ubse       64120       1 99 15:14 ?        00:18:18 /usr/bin/ubse
root     2182344 2181443  0 15:52 pts/3    00:00:00 grep --color=auto ubse

```

Wait for about 1 minute and check whether the nodes are started and active and standby clusters are successfully selected.

```shell
sudo ubsectl display cluster
```

```shell
# Expected output example (two-node environment)
-------------------------------------------------------------------------------------------------------------
  node            role      bonding-eid                               guid
-------------------------------------------------------------------------------------------------------------
  controller(1)   master    4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800010000
                                                                     
  node01(2)       standby   4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801000000
-------------------------------------------------------------------------------------------------------------
```

### 5.2 Building the Demo in Sections 2 and 3

> Ensure that your CMake version is 3.11 or later.

Create a directory for building in the example directory. The directory is usually named `build`.

```bash
mkdir build
```

Go to the build directory and run CMake:

```bash
cd build
cmake ..
```

Compile the project.

```bash
make
```

After the compilation is complete, the executable files `numa_demo` and `fd_demo` can be found in the `build` directory. You need to manually transfer the executable files to the service runtime environment and modify the file permission based on service requirements.

### 5.3 Running the Sample Program

#### 5.3.1 Configuring the Client Program

For details about the UBSE authentication mechanism, see [UBSE Role-based Access Control Design](../../docs/design/UBSE_Role-based_Access_Control_Design.md).
The user who delivers the configuration must have the root permission. Assume that the user who uses the UBSE API is test_user.

1. Add test_user to the UBSE user group.

    ```bash
    sudo usermod -aG ubse test_user
    ```

2. Create a custom permission configuration file for the client.

    ```bash
    sudo touch /etc/ubse/auth_test_api.conf
    ```

3. Edit the custom permission configuration file for the client.

    ```bash
    sudo vi /etc/ubse/auth_test_api.conf
    ```

    Press the **i** key to enter insert mode and copy the following content into the file:

    ```bash
    # Authentication configuration file
    # Defines base user-role and role-permission mappings with priority flags.

    [auth.user]
    # Section for user-to-role mappings.
    test_user=test

    [auth.role]
    # Section for role-to-permission mappings.
    test=mem.numa,mem.fd,mem.shm,mem.stat,topo
    ```

    Press **Esc** to exit insert mode, type **:wq**, and press **Enter** to save and exit the editor.

4. Restart the UBSE service for the configuration to take effect.

    ```bash
    sudo systemctl restart ubse
    ```

#### 5.3.2 Running the Client Program

```bash
# Run the sample program: FD borrowing.
./fd_demo fd_123

# Run the sample program: NUMA borrowing.
./numa_demo numa_123

# Run the sample program: borrowing detail query.
sudo ubsectl display memory -t borrow_detail
```

### 5.4 Precautions

* **Required permissions**

    * User permissions: The `ubsectl` command can be used only by the root user and the UBSE user automatically created during installation. The API can be used only by the root user, the UBSE user automatically created during installation, and custom users added to the UBSE group. If the API is used by a custom user added to the UBSE group, a custom permission configuration file for the client needs to be configured.

* **Startup exception analysis**

    * Node query error: If the error message is `ERROR: Failed to obtain cluster information`, the current node has not been started or the startup is abnormal. You can query the service process status. If the process exists, the node is still being started. Try again later.

    * Incomplete active/standby node information: If the queried node is the active node and the standby node status is empty, the current node has not been added to the cluster. You are advised to check whether the communication between nodes is normal.
