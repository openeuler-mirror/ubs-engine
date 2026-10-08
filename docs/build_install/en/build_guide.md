# Build Guide

> **Supported environment**: This project can be built only on openEuler. openEuler 24.03 LTS SP3 and SP4 are recommended.
> Before you start, verify that your operating system is openEuler by running `cat /etc/openEuler-release`, and install the dependencies listed below for your OS version.
> Other Linux distributions, such as Ubuntu and Debian, are not supported. Their dependencies may not be available through the system package manager, and you must resolve dependency installation yourself. No support is provided for these distributions.

## 1 Preparing the Build Environment: Obtaining the openEuler Image

This project targets AArch64. Building on an AArch64 system is recommended, although x86_64 is also supported. Choose the build method that matches your environment.

### 1.1 Physical Machine or VM (ISO)

| Download | URL |
|---------|------|
| Official downloads (recommended) | <https://www.openeuler.org/en/download/> |
| Archived releases | <https://archives.openeuler.openatom.cn/> |
| Official ISO repository (24.03 LTS SP3) | <https://repo.openeuler.org/openEuler-24.03-LTS-SP3/ISO/aarch64/> |
| Official ISO repository (24.03 LTS SP4) | <https://repo.openeuler.org/openEuler-24.03-LTS-SP4/ISO/aarch64/> |
| Huawei Cloud mirror site | <https://mirrors.huaweicloud.com/openeuler/> |

Download `openEuler-<version>-aarch64-dvd.iso` from the directory for the corresponding version. For more third-party mirror sites, see the [official mirror list](https://www.openeuler.org/en/mirror/list/).

### 1.2 WSL (Windows)

- **Microsoft Store (recommended; easiest option)**: Open Microsoft Store, search for `openEuler`, and install the required version.
- **Download the WSL package from the official repository** (if the Microsoft Store is unavailable):
    - 24.03 LTS SP3: <https://repo.openeuler.org/openEuler-24.03-LTS-SP3/WSL/>
    - 24.03 LTS SP4: <https://repo.openeuler.org/openEuler-24.03-LTS-SP4/WSL/>
- Installation guide: [openEuler WSL Installation](https://www.openeuler.org/en/wiki/install/wsl/)

> [!NOTE]
>
> WSL environments typically use the x86_64 architecture. Because WSL uses the Windows host kernel, building kernel modules that depend on `kernel-devel` may be limited. Perform development and testing involving kernel modules on a physical machine or virtual machine (VM).

### 1.3 Docker Container

1. Pull the official image from Docker Hub.

    ```shell
    docker pull openeuler/openeuler:24.03-lts-sp4
    ```

2. If Docker Hub is inaccessible, use the oepkgs mirror, which provides the same image.

    ```shell
    docker pull hub.oepkgs.net/openeuler/openeuler:24.03-lts-sp4
    ```

- Image repository: <https://hub.docker.com/r/openeuler/openeuler> (Available tags include `24.03-lts-sp3` and `24.03-lts-sp4`.)
- Alternative: Download the container image tarball from the official repository and import it using `docker load`.
    - 24.03 LTS SP3: <https://repo.openeuler.org/openEuler-24.03-LTS-SP3/docker_img/aarch64/>
    - 24.03 LTS SP4: <https://repo.openeuler.org/openEuler-24.03-LTS-SP4/docker_img/aarch64/>

## 2 Obtaining Source Code

```shell
git clone https://atomgit.com/openeuler/ubs-engine.git
```

## 3 Building

### 3.1 Build Dependencies

Build dependencies fall into two categories:

- **Packaging dependencies**: Listed in the `BuildRequires` section of the spec file (`ubs-engine.spec`). Install them all at once by running `dnf builddep -y ubs-engine.spec`.
- **Development and unit test dependencies**: In addition to the packaging dependencies, the GTest and GMock frameworks are required for development and unit testing. Install them by running `yum install -y gtest gtest-devel gmock gmock-devel`. These dependencies are needed only when running `bash build.sh ut`.

The currently required version of `ubs-comm-devel` is available only for openEuler 24.03 LTS SP3 and SP4. Check your build system version by running `cat /etc/openEuler-release`, then follow the corresponding instructions below:

- **For openEuler 24.03 LTS SP3 or SP4** (`openEuler release 24.03 (LTS-SP3)` or `openEuler release 24.03 (LTS-SP4)`), see [3.1.1 Installing Dependencies for openEuler 24.03 LTS SP3/SP4](#311-installing-dependencies-for-openeuler-2403-lts-sp3sp4).
- **For other openEuler versions**, see [3.1.2 Installing Dependencies for Other openEuler Versions](#312-installing-dependencies-for-other-openeuler-versions).

#### 3.1.1 Installing Dependencies for openEuler 24.03 LTS-SP3/SP4

| openEuler Version | Required ubs-comm-devel Version |
|----------------|------------------------|
| 24.03 LTS SP3  | >= 1.0.0-27            |
| 24.03 LTS SP4  | >= 1.0.1-7             |

Before installation, verify that the version of `ubs-comm-devel` available in the repository meets the requirements specified in the table above by running `dnf list ubs-comm-devel`.

- **Option 1: Install all dependencies with DNF**

    ```shell
    # Install the dependencies listed in the spec file's BuildRequires section.
    cd ubs-engine
    dnf builddep -y ubs-engine.spec
    ```

- **Option 2: Install dependencies manually with YUM**

    ```shell
    yum install -y libboundscheck libxml2-devel numactl-libs openssl-devel rapidjson-devel patch libvirt-devel kernel-devel 'cpp-httplib-devel >= 0.40.0''ubs-comm-devel >= <version required by the corresponding SP>' ...
    ```

> If the version in the repository does not meet the requirements for the corresponding SP release, or the package is unavailable, go to [3.1.2 Installing Dependencies for Other openEuler Versions](#312-installing-dependencies-for-other-openeuler-versions).

#### 3.1.2 Installing Dependencies for Other openEuler Versions

> [!IMPORTANT]
>
> When using other openEuler versions, some third-party library dependencies may be unavailable or may not meet the required version requirements. In such cases, build `ubs-comm` from source.

Run the following command to install the dependencies:

```shell
yum install -y libboundscheck libxml2-devel numactl-libs openssl-devel rapidjson-devel patch libvirt-devel cpp-httplib-devel kernel-devel
```

Build `ubs-comm-devel` and `ubs-comm-lib` from source.

1. Install the basic toolchain and the RPM build tools.

    ```shell
    sudo dnf install -y git rpm-build rpmdevtools gcc gcc-c++ make
    mkdir -p /tmp/src
    ```

2. Download the `ubs-comm` source code.

    ```shell
    git clone -b openEuler-24.03-LTS-SP4 https://gitcode.com/src-openeuler/ubs-comm.git /tmp/src/ubs-comm
    ```

3. Initialize the `rpmbuild` directory structure.

    ```shell
    rpmdev-setuptree
    ```

4. Copy the spec file from the `ubs-comm` source tree to the appropriate `rpmbuild` directory.

    ```shell
    cp /tmp/src/ubs-comm/ubs-comm.spec ~/rpmbuild/SPECS/
    cp -rf /tmp/src/ubs-comm/ubs-comm-*.tar.gz ~/rpmbuild/SOURCES/
    ```

5. Install the build dependencies listed in `ubs-comm.spec` under `BuildRequires`.

    ```shell
    sudo dnf builddep -y ~/rpmbuild/SPECS/ubs-comm.spec
    ```

6. Build the `ubs-comm` RPM packages.

    ```shell
    rpmbuild -ba ~/rpmbuild/SPECS/ubs-comm.spec
    ```

7. Install the generated RPM packages.

    * AArch64 architecture

        ```shell
        sudo dnf install -y ~/rpmbuild/RPMS/aarch64/ubs-comm-lib-*.rpm
        sudo dnf install -y ~/rpmbuild/RPMS/aarch64/ubs-comm-devel-*.rpm
        ```

    * x86_64 architecture

        ```shell
        sudo dnf install -y ~/rpmbuild/RPMS/x86_64/ubs-comm-lib-*.rpm
        sudo dnf install -y ~/rpmbuild/RPMS/x86_64/ubs-comm-devel-*.rpm
        ```

### 3.2 Performing Builds

1. Perform a Release build with `-O2` optimization and no debug information.

    ```shell
    bash build.sh
    ```

2. Perform a Debug build with debug information.

    ```shell
    bash build.sh -D
    ```

3. Perform a RelWithDebInfo build with `-O2` optimization and debug information.

    ```shell
    bash build.sh -T RelWithDebInfo
    ```

4. Perform a MinSizeRel build with `-Os` optimization and no debug information to minimize binary size.

    ```shell
    bash build.sh -T MinSizeRel
    ```

### 3.3 Performing Unit Tests

Five standalone UT binaries are supported.

| Binary | Test Scope |
|--------|---------|
| `ubs-engine_ut` | Unit tests for the main project |
| `ubs-engine_ucache_plugin_ut` | Tests for the ucache plugin |
| `ubs-engine_rmrs_plugin_ut` | Tests for the memory pooling plugin |
| `ubs-engine_virtagent_ut` | Tests for the virtualization agent |
| `ubs-engine_process_mem_ut` | Tests for the processmem plugin |

> [!NOTE]
> 
> The `ut` target combines compilation and test execution into a single Ninja target. If a test fails, subsequent compilation or test execution is halted. 
> To avoid this, it is recommended to build in batches using `--skip-run-tests` and run the tests separately.

1. Compile and run all tests.

    ```shell
    bash build.sh ut
    ```

2. Compile all tests without running them. This is recommended as the first step to verify that all code compiles successfully.

    ```shell
    bash build.sh ut --skip-run-tests
    ```

3. Compile a specific test binary.

    ```shell
    bash build.sh ut --skip-run-tests -t ubs-engine_ut
    ```

4. Run selected test cases.

    ```shell
    bash build.sh ut -- --gtest_filter="TestSuite.*"
    ```

## 4 Creating RPM Packages

Build the project and package it into RPM files in the top-level `output/` directory.

```shell
bash build.sh package
```

The generated artifacts are as follows:

```path
└── output                                                            # Packaging output directory
    ├── ubs-engine-<version>-<release>.aarch64.rpm                    # Main package
    ├── ubs-engine-client-libs-<version>-<release>.aarch64.rpm        # Client runtime libraries
    ├── ubs-engine-client-devel-<version>-<release>.aarch64.rpm       # Client development package
    ├── python3-ubs-engine-<version>-<release>.aarch64.rpm            # Python API package
    ├── ubs-engine-rmrs-<version>-<release>.aarch64.rpm               # Memory pooling plugin
    ├── ubs-engine-ucache-<version>-<release>.aarch64.rpm             # ucache plugin
    ├── ubs-engine-virtagent-<version>-<release>.aarch64.rpm          # Virtualization agent plugin
    ├── ubs-engine-processmem-<version>-<release>.aarch64.rpm         # processmem plugin
    └── ubs-engine-debuginfo-<version>-<release>.aarch64.rpm          # Debug information
```

For dependencies between subpackages, see [Dependency Groups](../../build_install/部署说明.md#11-依赖分组说明) in the *Deployment Guide*.
