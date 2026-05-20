The UBS Engine Python API is a UBS Engine management interface developed based on the Python language. It is used to schedule and manage resources and perform key service operations.

This document aims to help developers quickly master the core functions, application scenarios, and development skills of the UBS Engine Python API, provide sample code that can be directly executed, and avoid common issues.

## 1. Prerequisites

To use the UBS Engine Python API for development, prepare the UBS Engine development environment and install the development package by following the steps below.

### 1.1 Environment Requirements

* **OS**: Linux
* **Python version**: Python 3.11

### 1.2 Development Package Installation

```bash
# Install the development package (for integrated development).
sudo dnf install -y ubs-engine-client-libs python3-ubs-engine
```

### 1.3 Installation Result

| File/Directory                     | Other Information                           |
| ------------------------------ |---------------------------------|
| `/usr/lib/python3.11/site-packages/ubse/`            | Permission on the header file (`*.py`) in the directory: `644`         |
| `/usr/lib64/libubse-client.so.1` | Soft link, pointing to `libubse-client.so.1.0.0`|
| `/usr/lib64/libubse-client.so.1.0.0`  | Binary dynamic library                         |

### 1.4 Importing Python Modules

In a Python project, you need to import the following modules:

```python
from ubse.ubs_engine_topo import ubs_topo_node_list, ubs_topo_node_local_get, ubs_topo_link_list
from ubse.ubs_engine_mem import ubs_mem_numa_list
from ubse.ubs_engine_log import LogLevel, ubs_engine_log_callback_register
from ubse.ubse_engine import ubs_engine_client_initialize, ubs_engine_client_finalize
```

## 2. Using the UBS Engine Python API: Topology Query as an Example

Topology query is a common scenario of the UBS Engine service. Cluster resources can be scheduled and managed by querying node and connection information.

### 2.1 Implementation Logic Overview

The implementation of topology query can be divided into the following four main steps:

* Register log callback: Use `ubs_engine_log_callback_register` to register the log callback function for debugging and monitoring.

* Query topology information: Use `ubs_topo_node_list` to query information about all nodes, `ubs_topo_node_local_get` to query information about the local node, and `ubs_topo_link_list` to query information about all connections.

### 2.2 Implementation Code

The [test_ubs_engine_topo.py](test_ubs_engine_topo.py) file provides the implementation code for topology query. You can refer to this code to implement the topology query process.

### 2.3 Expected Output

```shell
# Example of the expected output
The custom log processing function is successfully registered.
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=3, op_code=2
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=3, op_code=2
Information about two CPU topology links is obtained.
Link : 1-36-1 <-> 2-36-1
Link : 1-236-1 <-> 2-236-1
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=3, op_code=1
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=3, op_code=1
Information about two nodes is obtained.
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

## 3. Using the UBS Engine Python API: NUMA Memory Query as an Example

NUMA memory query is another common scenario of the UBS Engine service. Memory resources can be managed by querying the remote memory information in NUMA topology.

### 3.1 Implementation Logic Overview

The implementation of NUMA memory query can be divided into the following four main steps:

* Query NUMA memory: Use `ubs_mem_numa_list` to query all remote NUMA memory on the local node.

* Process the query result: Parse the returned memory description, including the borrow identifier, NUMA ID, borrow size, and lending/borrowing node information.

### 3.2 Implementation Code

The [test_ubs_engine_mem.py](test_ubs_engine_mem.py) file provides the implementation code for NUMA memory query. You can refer to this code to implement the memory query process.

### 3.3 Expected Output

```shell
# Example of the expected output
The custom log processing function is successfully registered.
[simple_log_handler] INFO: [ubse_uds_client.cpp:Connect:90] Connection successful
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:98] Sending request, module_code=1, op_code=20
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:178] Starting Send operation with timeout: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Send:212] Waiting for response...
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:271] Starting Receive operation with remaining time: 1800000ms
[simple_log_handler] INFO: [ubse_uds_client.cpp:Receive:321] Receive operation completed successfully
[simple_log_handler] INFO: [ubse_ipc_client.cpp:ubse_invoke_call:105] Sending request successfully, module_code=1, op_code=20
No node information is obtained.
```

## 4. Running Example

### 4.1 Preparing the Service Environment

**Environment requirements**

* **OS**: openEuler 24.03 LTS or later
* **CPU architecture**: AArch64
* **Memory**: ≥ 64 GB
* **Disk**: SSD, throughput 500 MB/s (IOPS)
* **Chip interconnection**: UB
* **NIC**: optional dependency (TCP can be used to assist UB in link establishment. By default, UB bootstrap is used for link establishment.)
* **User permission**: The `root` permission is required for installation and management.

> For more requirements, see [Deployment Description](../../docs/build_install/Deployment_Description.md).

Install the RPM package. (The `ubs-engine` main package must be installed on all cluster nodes.)

```bash
# Install the main program package.
sudo dnf install -y ubs-engine

# Install the client runtime library (mandatory for third-party integration).
sudo dnf install -y ubs-engine-client-libs

# Install the Python module (mandatory for third-party integration).
sudo dnf install -y python3-ubs-engine
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

### 4.2 Running the sample program

#### 4.2.1 Configuring the Client Program

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
    test=mem.numa,topo
    ```

    Press **Esc** to exit insert mode, type **:wq**, and press **Enter** to save and exit the editor.

4. Restart the UBSE service for the configuration to take effect.

    ```bash
    sudo systemctl restart ubse
    ```

#### 4.2.2 Running the Client Program

Transfer the Python sample program to the service runtime environment and modify the file permission based on service requirements.

```bash
# Run the topology query example.
python3 ./test_ubs_engine_topo.py

# Run the memory query example.
python3 ./test_ubs_engine_mem.py
```

### 4.3 Precautions

* **Required permissions**

    * User permissions: The `ubsectl` command can be used only by the root user and the UBSE user automatically created during installation. The API can be used only by the root user, the UBSE user automatically created during installation, and custom users added to the UBSE group. If the API is used by a custom user added to the UBSE group, a custom permission configuration file for the client needs to be configured.

* **Startup exception analysis**

    * Node query error: If the error message is `ERROR: Failed to obtain cluster information`, the current node has not been started or the startup is abnormal. You can query the service process status. If the process exists, the node is still being started. Try again later.

    * Incomplete active/standby node information: If the queried node is the active node and the standby node status is empty, the current node has not been added to the cluster. You are advised to check whether the communication between nodes is normal.

* **Python version**

    * The UBS Engine Python API depends on Python 3.11.
