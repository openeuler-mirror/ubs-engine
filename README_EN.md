# UBS-Engine

Software-defined computing, on-demand resource combination and allocation

# ubs_engine

## 1. Introduction

The UB Service Core Engine (UBSE or UBS Engine) provides the UBSE daemon program and its corresponding SDK development library. Developers can use this SDK to access services provided by the UBSE daemon, thereby scheduling and managing resources such as memory and performing key O&M operations.

## 2. Contents Structure

```shell
UBSEngine/
├── 3rdparty                    // Third-party software
├── conf                        // Configuration file
├── doc                         // Document
├── scripts                     // Scripts
├── src                         
│   ├── include                 // Global header file
│   ├── apiserver               // Northbound interface exposure
│   ├── cli                     // CLI code
│   │   ├──ubse_cli_framework   // Auxiliary builder class for command registration and result display
|   │   └──ubse_cert            // Certificate-related
│   │   
│   ├── controllers             // Controllers
│   │   ├── include             // Header file 
│   │   ├── include             // Memory pooling controller
│   │   |    ├── algorithm      // Memory scheduling algorithm
│   │   │    ├── memcontroller  // Memory pooling controller --- file
│   │   │    └── memscheduler   // Memory pooling scheduler --- file
│   │   └── node                // Memory pooling controller --- node collection information
│   │        └── nodecontroller // Memory pooling controller --- file
│   │  
│   ├── framwork                // Software framework
│   │   ├── com                 // Communication component, which connects to hcom, switches threads, and performs callback
│   │   ├── config              // Configuration module
│   │   ├── context             // Context module
│   │   ├── event               // Event center
│   │   ├── ha                  // HA module
│   │   ├── http                // HTTP component
│   │   ├── ipc                 // IPC
│   │   ├── log                 // Log component
│   │   ├── misc                // Miscellaneous: smart pointer, lock, ring queue, and CRC
│   │   ├── plugin_mgr          // Plugin management
│   │   ├── security            // Security component. For the open-source version, only privilege escalation is required. For the commercial version, KMC-based encryption and decryption are supported.
│   │   ├── serde               // Serialization and deserialization
│   │   ├── thread_pool         // Thread pool operation library, which is the system thread pool of Fram
│   │   ├── timer               // Timer
│   │   └── xml                 // XML parsing and processing
│   │   
│   ├── message                 // Message
│   ├── node                    // Data link creation
│   ├── ras                     // Troubleshooting
│   ├── adapter_plugins
│   │   ├── syssentry           // sysSentry interconnection
│   │   ├── mti                 // UBM interconnection
│   │   └── mmi                 // Memory resource interface
│   │   
│   └── sdk                     // SDK module
│       ├── include             // SDK external interface declaration
│       └── example              // SDK example
└── test
    ├── IT
    ├── PT
    └── UT
```

For more information, see the README.md file in each directory.

## 3. Architecture

For details about the UBSE architecture, see [Architecture](./docs/design/architecture.md).

## 4. Function

It provides the UBSE daemon program and its corresponding SDK development library. Developers can use this SDK to access services provided by the UBSE daemon, thereby scheduling and managing resources such as memory and performing key O&M operations. For details about UBSE functions, see the API description.

## 5. Getting Started

* Usage Guide

* Development Guide

* User Manual

* Design Document

<p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">For details, see the product documentation.</p>

## 

### Quick Start

#### 1. Preparations

#### 1.1 Environment Installation

Install the environment based on the hardware, OS, and software requirements for running UBSE.

#### 1.2 Source Code Download

```shell
git clone https://atomgit.com/openeuler/ubs-engine.git
```

#### 2. Project Building

**Recommended**: Build a project on openEuler Linux (ARM64).

```shell
# Perform a Release build (without debug information and with -O2 optimization).
bash build.sh

# Perform a Debug build (with additional debug information).
bash build.sh -D

# Perform a RelWithDebInfo build (with additional debug information and -O2 optimization).
bash build.sh -T RelWithDebInfo

# Perform a MinSizeRel build (without debug information, with minimal binary file size, and with -Os optimization).
bash build.sh -T MinSizeRel
```

For details, see [Build Guide](./docs/build_install/Build_Guide.md).

#### 3. Project Packaging

```shell
# Build a project, package it into an RPM file, and output the file to the top-level output/ directory of the project.
# To add files to an RPM package, see [How to Add Files to an RPM Package in the RackManager Project](http://3ms.huawei.com/km/blogs/details/17822996?l=en).
bash build.sh package
```

#### 4. Project Development

#### 4.1 Common Issues

1. Unable to understand build types or find build products

    <p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">For details about the differences between build types, see the previous section "Project Building".</p>

    * Release (default build type used for production non-debug packages, where code is optimized and debug information is excluded)

    * Debug (used for debug packages, where code is not optimized and debug information is included)

    * RelWithDebInfo (used for production debug packages, where code is optimized and debug information is included)

    * MinSizeRel (used for production packages with the minimum binary size and without debug information)

    <p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">"RelWithDebInfo" is used to debug optimized code when debugging fails due to differences between release and debug packages.
    "MinSizeRel" is mainly used in special scenarios, such as when a minimum production package is required.</p>

2. The test code is all highlighted in red because header files cannot be found.

    <p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">By default, the project build does not include test code, facilitating package building. It is recommended that developers manually enable the BUILD_TESTS option during development.</p>

    ![image-20251013163754583](docs/images/image-20251013163754583.png)

#### 4.2 Developer Testing

Developer testing includes IT and UT. The source code is stored in the **test** directory.

```shell
# Execute UT only.
bash build.sh ut
# Execute only some test cases.
bash build.sh ut -- --gtest_filter="TestRackHttpClient.*:TestRackHttpReq.*"
# Execute only one group of test cases.
bash build.sh ut -- --gtest_filter="TestRackHttpClient.*"
# Execute only one test case.
bash build.sh ut -- --gtest_filter="ClientSendSuccessfully"
```

For details, see [Developer UT Guide](./docs/test/UT_Development_Guide.md).

<p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;"><span style="font-size: 14pt;font-weight: bold"><b>Coverage Report</b></span></p>

<p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">The prerequisite for generating a coverage report is that the unit test pass rate is 100%. If there are failed test cases, the coverage report cannot be generated.</p>

> <p style="margin-top: 0px; font-family: SimSun; font-size: 16px;">For module unit tests, the pipeline is not directly connected, which facilitates quick coverage of module tests. Therefore, the restriction on the coverage report is lifted. Even if there are failed test cases, a coverage report will be generated.</p>

```shell
# Generate a coverage report in the cmake-build-debug/coverage directory.
bash build.sh ut -C

# A coverage report can also be generated for module tests in the same directory.
bash build.sh ubse_http_ut -C
```

<p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">To view the coverage report, add the <code>-H</code> parameter to start the HTTP server in the background. After the server is started, the URL of the coverage report is printed. You can open this URL in a browser to access the coverage report.</p>

> <p style="margin-top: 0px; font-family: SimSun; font-size: 16px;">Note:</p>
> <p style="margin-top: 0px; font-family: SimSun; font-size: 16px;">The custom CMake command incorrectly determines the end status of background commands. As a result, when the HTTP server is started, the CMake command cannot exit. In this case, you can manually press Ctrl+C to exit. The server will still run in the background.</p>

<p style="text-align: left; margin-top: 0px; font-family: SimSun; font-size: 16px;">Additional information: The HTTP server is created only once on each machine and will not be created repeatedly. You do not need to worry about port occupation.</p>

```shell
# Start the HTTP server in the background to print the URL of the coverage report.
bash build.sh ut -C -H
```

### 

Tips:

```shell
# Build a test only but not execute it.
bash build.sh ut --skip-run-tests
```

#### 5. License

This project uses the Mulan open source license. For details, see the [License](./LICENSE) directory.
