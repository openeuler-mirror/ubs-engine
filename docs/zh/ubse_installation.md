# UBS-Engine (UBSE) 安装指南

## 环境要求

|部件|版本|
|:---|:---|
|操作系统|openEuler 24.03 LTS 或更高版本|
|CPU架构|aarch64|
|内存|64GB及以上|
|磁盘|SSD，IOPS 500MB/s|
|芯片互联|UB|
|网卡|可选依赖（可选使用TCP辅助UB建链，默认采用UB自举建链）|
|用户权限|安装与管理需 <code>root</code> 权限|

## 节点规划

- 所有集群节点均需安装 <code>ubs-engine</code> 主包。

- 建议提前规划节点 IP 地址列表（用于 TCP 模式）。

## 安装注意事项

  - 非高安场景下，UBSE与UBM使用UDS通信，需要将ubse用户加到ubm_nuds用户组中，该用户组由UBM服务创建，如果UBM服务未在UBSE前安装，ubse用户可能无法加入ubm_nuds用户组，导致UBSE服务与UBM服务通信异常。待UBM服务完成安装后，需手动将ubse用户加入ubm_nuds用户组。
  - UBSE需要调用ubturbo接口，ubturbo接口有权限校验，需要将ubse用户加到ubturbo用户组中，该用户组由ubturbo服务创建，如果ubturbo服务未安装，ubse用户可能无法加入ubturbo用户组，导致UBSE服务调用ubturbo接口异常。待ubturbo安装完成后，需手动将ubse用户加入ubturbo用户组。

## 执行安装

- 在线安装

  > [!NOTE]说明
  >
  > 在线安装过程中，所需依赖会自动进行安装。

  ```bash
  # 注：需要系统配置了openEuler release 24.03 (LTS-SP3)镜像源
  # 安装主程序包
  # 智算场景，执行如下命令：
  sudo env ENABLE_AI=true dnf install -y ubs-engine
  # 通算场景，执行如下命令：
  sudo dnf install -y ubs-engine
  # 安装客户端运行时库（第三方集成必需）
  sudo dnf install -y ubs-engine-client-libs
  # 安装python 模块（可选，使用UBSE Python API时需要安装）
  sudo dnf install -y python3-ubs-engine
  # 安装插件子包（按需，通常由主包自动依赖拉取）
  sudo dnf install -y ubs-engine-rmrs ubs-engine-ucache ubs-engine-virtagent ubs-engine-processmem ubs-engine-ssu
  # 安装 SSU 客户端运行时库（第三方使用 SSU C SDK 集成时必需）
  sudo dnf install -y ubs-engine-ssu-client-libs
  # 安装 SSU 客户端开发包（编译链接 SSU C SDK 时必需）
  sudo dnf install -y ubs-engine-ssu-client-devel
  # 安装 SSU Python 模块（可选，使用 UBSE SSU Python API 时需要安装）
  sudo dnf install -y python3-ubs-engine-ssu
  ```

- 离线安装

  > [!WARNING]说明
  >
  > 离线安装需要提前安装所需依赖。
  > ubs-engine运行依赖信息记录在spec文件（[ubs-engine.spec](https://atomgit.com/openeuler/ubs-engine/blob/master/ubs-engine.spec)）中。
  > 运行依赖所需系统库，通常由包管理器自动安装。

  ```bash
  # 通过rpm包安装运行包
  # 安装主程序包
  # 智算场景，执行如下命令：
  sudo env ENABLE_AI=true dnf install -y ubs-engine-<version>-<release>.aarch64.rpm
  # 通算场景，执行如下命令：
  sudo dnf install -y ubs-engine-<version>-<release>.aarch64.rpm
  # 安装客户端运行时库（第三方集成必需）
  sudo dnf install -y ubs-engine-client-libs-<version>-<release>.aarch64.rpm
  # 安装python 模块（可选，使用UBSE Python API时需要安装）
  sudo dnf install -y python3-ubs-engine-<version>-<release>.aarch64.rpm
  # 安装插件子包（按需，需与主包同版本）
  sudo dnf install -y ubs-engine-rmrs-<version>-<release>.aarch64.rpm \
                      ubs-engine-ucache-<version>-<release>.aarch64.rpm \
                      ubs-engine-virtagent-<version>-<release>.aarch64.rpm \
                      ubs-engine-processmem-<version>-<release>.aarch64.rpm \
                      ubs-engine-ssu-<version>-<release>.aarch64.rpm
  # 安装 SSU 客户端运行时库（第三方使用 SSU C SDK 集成时必需，依赖 ubs-engine-client-libs）
  sudo dnf install -y ubs-engine-ssu-client-libs-<version>-<release>.aarch64.rpm
  # 安装 SSU 客户端开发包（编译链接 SSU C SDK 时必需，依赖 ubs-engine-client-devel）
  sudo dnf install -y ubs-engine-ssu-client-devel-<version>-<release>.aarch64.rpm
  # 安装 SSU Python 模块（可选，使用 UBSE SSU Python API 时需要安装，依赖 python3-ubs-engine）
  sudo dnf install -y python3-ubs-engine-ssu-<version>-<release>.aarch64.rpm
  ```

## 安装结果

 ubs-engine 主程序安装结果：

  | 路径                                  | 用途          |
  |-------------------------------------| -------------|
  | /usr/bin/ubse, /usr/bin/ubsectl     | 主程序与 CLI  |
  | /usr/lib/systemd/system/ubse.service | systemd 服务  |
  | /etc/ubse/                          | 配置目录      |
  | /var/log/ubse/                      | 日志目录      |
  | /var/lib/ubse/cert/                 | 证书目录      |
  | /var/lib/ubse/data                  | 持久化数据    |
  | /var/lib/ubse/lcne_cert/            | 高安部署证书目录|
  | /var/lib/ubse/vip_server_cert/      | VIP模块证书目录|
  | /var/run/ubse/                      | 运行时 socket |
  | /lib/modules/ubse/bandbridge.ko              | NPU直通虚机和LCNE进行带外通信 |
  | /lib/modules/$(uname -r)/extra/bandbridge.ko | 软链接，指向/lib/modules/ubse/bandbridge.ko             |

- ubs-engine 客户端运行库安装结果：

  | 文件                                 | 其它说明                                          |
  | ------------------------------------ | ------------------------------------------------- |
  | `/usr/lib64/libubse-client.so.1.0.0` | 二进制动态库实体                                  |
  | `/usr/lib64/libubse-client.so.1`     | 软链接，指向 `/usr/lib64/libubse-client.so.1.0.0` |

- ubs-engine Python API 包安装结果：

| 文件/目录                      | 其它说明                                 |
| ------------------------------ | ---------------------------------------- |
| `/usr/lib/python3.11/site-packages/ubse` | 内部文件（`*.py`）权限：`644`         |
| `/usr/lib/python3.11/site-packages/ubse-xx.xx.xx-py3.11.egg-info` | 内部文件权限：`644`，Python包相关信息    |

- ubs-engine ssu 插件安装结果：

  | 文件/目录                                  | 其它说明          |
  | ------------------------------------------ | ----------------- |
  | `/usr/lib64/ubse_plugin/libssu_plugin.so` | ssu 插件动态库    |
  | `/usr/bin/ubsectl-ssu`                     | ssu 插件 CLI 工具 |

- ubs-engine-ssu-client-libs 安装结果：

  | 文件                                     | 其它说明                                                |
  | ---------------------------------------- | ------------------------------------------------------- |
  | `/usr/lib64/libubse-ssu-client.so.1.0.1` | 二进制动态库实体                                        |
  | `/usr/lib64/libubse-ssu-client.so.1`     | 软链接，指向 `/usr/lib64/libubse-ssu-client.so.1.0.1` |

  > [!NOTE]说明
  > 该包运行时依赖 `ubs-engine-client-libs`（提供 `libubse-client.so.1`），安装时会自动拉取。

- ubs-engine-ssu-client-devel 安装结果：

  | 文件/目录                                | 其它说明                                                        |
  | ---------------------------------------- | --------------------------------------------------------------- |
  | `/usr/include/ubse/ubs_engine_ssu.h`     | SSU SDK 头文件                                                  |
  | `/usr/lib64/libubse-ssu-client.so`       | 软链接，指向 `/usr/lib64/libubse-ssu-client.so.1`，供链接器使用 |
  | `/usr/lib64/libubse-ssu-client.a`        | 二进制静态库                                                    |

  > [!NOTE]说明
  > 该包依赖 `ubs-engine-client-devel`（提供 `libubse-client.so`、`ubs_error.h` 等公共头文件），安装时会自动拉取。

- python3-ubs-engine-ssu 安装结果：

  | 文件/目录                                                            | 其它说明                                  |
  | -------------------------------------------------------------------- | ----------------------------------------- |
  | `/usr/lib/python3.x/site-packages/ubse/ubs_engine_ssu.py`            | SSU Python SDK 主模块                      |
  | `/usr/lib/python3.x/site-packages/ubse/ffi/ubs_engine_binding_ssu.py` | SSU Python FFI 绑定                        |
  | `/usr/lib/python3.x/site-packages/ubse/models/ubs_engine_model_ssu.py`| SSU Python 数据模型                        |
  | `/usr/lib/python3.x/site-packages/ubse_ssu-xx.xx.xx-py3.x.egg-info`  | Python 包元数据                            |

  > [!NOTE]说明
  > 该包为 `noarch` 架构，依赖 `python3-ubs-engine`（提供 `ubse.ipc`、`ubse.ffi` 等公共命名空间），安装时会自动拉取。
  > SSU Python 模块通过 PEP 420 隐式命名空间扩展 `ubse` 包，安装主 `python3-ubs-engine` 后即可使用 `from ubse.ubs_engine_ssu import ...`。

## （可选）修改配置

1. 编辑配置文件：

    ```bash
    sudo vi /etc/ubse/ubse.conf
    ```

2. 修改以下配置项(默认无此配置项,打开此配置时，使用tcp通信，否则默认使用urma通信)：

    ```ini
    [ubse.rpc]
    cluster.ipList=192.168.100.100-192.168.100.102
    ```

    > [!NOTE]说明
    > 支持配置IP地址范围, 例如：192.168.100.100-192.168.100.102
    > 默认使用urma通信时，需已安装urma。
    > 未配置 `cluster.ipList` 且未开启 URMA 特性时，UBSE 无可用建链方式，服务启动失败。

    ubs engine支持两种通信模式，可根据硬件和网络环境选择：

    | 通信方式   | URMA(默认)             | TCP                    |
    | ---------- | ---------------------- | ---------------------- |
    | 性能       | 高性能、低延迟、零拷贝 | 标准 TCP，性能适中     |
    | 硬件要求   | 需支持 URMA 的智能网卡 | 普通以太网卡即可       |
    | 配置复杂度 | 免配置，自动发现节点   | 需手动配置 IP 列表     |
    | 使用场景   | 高性能计算、金融交易   | 普通数据中心、开发测试 |
  
3. 启动ubs engine服务

    ```bash
    sudo systemctl start ubse
    sudo systemctl enable ubse
    ```

## （可选）安装URMA

默认使用urma通信时需确保部署环境中已安装URMA驱动和运行时库，并开启URMA特性；如果未配置 `cluster.ipList` 且未开启 URMA 特性，服务将无法启动。

```python
# 示例：安装 URMA 运行时包（具体包名根据发行版可能不同）
sudo yum install -y umdk-urma-lib    # OpenEuler
sudo yum install -y umdk-urma-kmod
```

安装后重启ubs engine服务

    ```bash
    sudo systemctl restart ubse
    ```

## （可选）配置VIP

VIP（Virtual IP）管理能力用于在主备切换场景下，将对外服务的虚拟 IP 绑定到当前主节点，保证外部访问地址不变。VIP 模块默认关闭，开启前需在 `/var/lib/ubse/vip_server_cert/` 目录下部署证书文件。

1. 编辑配置文件并开启 VIP：

    ```bash
    sudo vi /etc/ubse/ubse.conf
    ```

    ```ini
    [ubse.vip]
    vip.enable=true
    vip.httpServer.listen.ip=192.168.100.200/24
    vip.httpServer.listen.port=10002
    ```

2. 重启 ubs engine 服务使配置生效：

    ```bash
    sudo systemctl restart ubse
    ```

> [!NOTE]说明
> 各配置项的完整取值范围、证书文件清单及网卡获取方式等详细说明，请参考 [UBSE配置说明 — VIP配置说明](./ubse_configration_instructions.md#vip配置说明)。

## （可选）安装Bash Completion脚本库

使用ubsectl工具进行命令补全时依赖该脚本库。

> [!NOTE] 须知
> 
>- 安装完成后，当前终端窗口不会立即生效。
>- 新建终端窗口将自动加载Bash Completion，无需额外操作。

**使用DNF源安装Bash Completion脚本库**

1. 安装Bash Completion，根据终端交互信息提示完成安装。

    ```shell
    dnf install bash-completion
    ```

2. 后续如需卸载Bash Completion，可执行以下命令并根据终端交互信息完成卸载。

    ```shell
    dnf remove bash-completion
    ```

**使用RPM包安装Bash Completion脚本库**

1. 获取[Bash Completion脚本库](https://easysoftware.openeuler.org/zh/search?name=bash-completion&tab=rpmpkg&key=all)。
2. 安装Bash Completion，根据终端交互信息提示完成安装。

    ```shell
    rpm -ivh bash-completion-version.rpm
    ```

3. 后续如需卸载Bash Completion，可执行以下命令并根据终端交互信息完成卸载。

    ```shell
    rpm -e bash-completion
    ```

## （可选）安装hikptool工具

使用ubse内存借用功能，考虑链路亚健康时，需要安装hikptool工具。

以下命令需要root或sudo权限执行。

1. 源码下载：git clone [https://atomgit.com/openeuler/hikptool.git](https://atomgit.com/openeuler/hikptool.git) 。下载至/usr/bin/目录下。

2. 编译安装：

    ```bash
    cd hikptool
    cmake -S . -B build
    cmake --build build -j"$(nproc)"
    ```

    编译最终产物包括二进制程序hikptool和动态库文件libhikptdev.so.1.x.x。

3. 动态库配置：

    ```shell
    cd build
    export LD_LIBRARY_PATH=./libhikptdev/src/rciep:$LD_LIBRARY_PATH
    ```

    该 export 需在 build/ 目录下执行

4. 用户态的hikptool，加权限后，可直接./执行。(ubse会自动调用，此处只作为hikptool工具调用说明)

    ```shell
    chmod -R 755 /usr/bin/hikptool 
    ./hikptool sub_health
    ```

5. 亚健康检测结果存在当前目录下的detection.json文件中。(ubse自动调用时，检测结果在/var/log/ubse/detection.json)

6. 如需进行链路亚健康的内存借用决策，则需要在ubse.conf里设置subHealthPenaltyEnabled=true，开启ubse对hikptool工具的调用。设置该配置前，请确认 hikptool 已安装至 /usr/bin/ 目录且具备执行权限（见上文第 1-4 步）

7. 工具详细使用手册参考：[https://atomgit.com/openeuler/hikptool/tree/master/ub/subhealth](https://atomgit.com/openeuler/hikptool/tree/master/ub/subhealth)。

## 验证部署

### 检查服务状态

```bash
systemctl is-active ubse  # 应输出 "active"
```

### 查看日志

- 方式一

  ```bash
  journalctl -u ubse -f
  ```

- 方式二

  ```bash
  /var/log/ubse/xxx
  ```

### 使用CLI工具

```bash
sudo /usr/bin/ubsectl --help
```
