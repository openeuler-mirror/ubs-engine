# UBS RMRS

## Introduction

UBS RMRS is an intra-node resource management service plugin designed for virtualization scenarios. As a part of the UBS Engine framework, it provides cluster-level memory resource scheduling services, specifically for scenarios involving VM memory fragmentation, VM memory overcommitment, and container memory overcommitment.

## Contents Structure

```sh
UBS RMRS/
├── 3rdparty                    // Third-party source code library
├── benchmark                   // CLI tool
├── conf                        // Configuration file
├── doc                         // Document
├── scripts                     // Project script
├── src                         
│   ├── include                 // Header file
│   ├── common                 // Common module
│   ├── interface              // External interface
│   ├── mem_fragment           // VM fragment source code
│   ├── over_commit            // VM/Container overcommitment source code
└── test
    ├── UT                     // Third-party test library
        ├── conf           // UT configuration file
        └── testcase           // Test case
```

## Constraints

VM fragmentation/overcommitment scenarios apply only to 2 MB VMs.

## Project Architecture

  - UBS RMRS is an intra-node resource management service plugin designed for virtualization scenarios. As a part of the UBS Engine framework, it provides cluster-level memory resource scheduling services, specifically for scenarios involving VM memory fragmentation, VM memory overcommitment, and container memory overcommitment.

    ![img](https://resource.idp.huawei.com/idpresource/nasshare/editor/image/208584334606/1_zh-cn_image_0000002547490823.png)

    The VM fragmentation architecture consists of two layers:

    The UBS RMRS is deployed in the UBS Engine. The agent on each compute node is only responsible for data collection and message forwarding. The active node serves as the functional entry and provides interfaces for borrowing, migrating out, returning, and rolling back fragmented memory. It is responsible for managing fragmented memory between nodes.

    The RMRS is deployed in the OS Turbo of each compute node. Based on its own resource collection, it provides VM migration-out/migration-back policies in fragmentation scenarios and uses the SMAP migration capability to migrate VMs out and back.

    VM/Container overcommitment scenarios:

    MemLink dynamically reclaims the idle memory of VMs or supplements memory. When NUMA memory reaches the high water mark, memory borrowing is triggered and the VM memory is migrated out to relieve the memory pressure. When NUMA memory reaches the low water mark, the borrowed memory is returned.

    UBS RMRS provides interfaces for memory borrowing, VM migration, and memory return, as well as VM migration algorithms. OS Turbo provides the resource collection capability to support migration algorithm decisions.

# Quick Start

## Prerequisites

- UBS RMRS is a plugin of UBS Engine. After UBS Engine is installed, you need to enable plugin configuration.
- UBS RMRS depends on UB Turbo and its plugins to provide capabilities such as memory migration. Therefore, they must be used together.
- UBS RMRS depends on libvirt to collect node VM information.

## UBS RMRS Compilation

Run the following commands in the root directory:

```bash
git submodule update --init --recursive
dos2unix build.sh
sh build.sh -ub
```

Build artifacts:

`libmempooling.so`, a library file in the `cmake-build-release/lib` directory

# License

This project uses the Mulan open source license. For details, see the License file in the root directory of the project.
