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
  sudo dnf install -y ubs-engine
  # 安装客户端运行时库（第三方集成必需）
  sudo dnf install -y ubs-engine-client-libs
  # 安装python 模块（可选，使用UBSE Python API时需要安装）
  sudo dnf install -y python3-ubs-engine
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
  sudo dnf install -y ubs-engine-<version>-<release>.aarch64.rpm
  # 安装客户端运行时库（第三方集成必需）
  sudo dnf install -y ubs-engine-client-libs-<version>-<release>.aarch64.rpm
  # 安装python 模块（可选，使用UBSE Python API时需要安装）
  sudo dnf install -y python3-ubs-engine-<version>-<release>.aarch64.rpm
  ```

## 安装结果

- ubs-engine 主程序安装结果：

  | 路径                                  | 用途          |
  |-------------------------------------| -------------|
  | /usr/bin/ubse, /usr/bin/ubsectl     | 主程序与 CLI  |
  | /usr/lib/systemd/system/ubse.service | systemd 服务  |
  | /etc/ubse/                          | 配置目录      |
  | /var/log/ubse/                      | 日志目录      |
  | /var/lib/ubse/cert/                 | 证书目录      |
  | /var/lib/ubse/data                  | 持久化数据    |
  | /var/lib/ubse/lcne_cert/            | 高安部署证书目录|
  | /var/run/ubse/                      | 运行时 socket |

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
    > 支持配置IP地址范围：192.168.100.100-192.168.100.102
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
