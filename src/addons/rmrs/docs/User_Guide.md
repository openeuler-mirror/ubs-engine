## 1 RMRS 简介
UBS RMRS 的英文全名为 UB Server Core RackMemoryResourceSchedule，是用于集群内存资源调度的组件。本项目为UBS Engine 内rmrs插件。

### 1.1 虚机碎片关键技术方案
#### 1.1.1 整体方案设计
虚机碎片架构分为两层：
- **UBS RMRS（UBS Engine 内）**：各计算节点 Agent 端仅负责数据采集和消息转发；主节点侧作为功能入口，提供碎片内存借用、迁出、归还和回滚接口，负责节点间碎片内存管理。
- **RMRS（OS Turbo 内）**：基于本机资源采集，提供碎片场景虚机迁出/迁回策略，并利用 SMAP 迁移能力执行迁出与迁回。

#### 1.1.2 内存借用与归还
UBS RMRS提供内存借用和归还能力。
1）提供碎片内存借用策略：利用节点拓扑关系，每个节点的剩余碎片资源，考虑numa的连线情况，制订内存资源借用策略，整合集群碎片资源。
2）提供借用内存在虚机创建失败等情况下的回滚能力，归还此次借用内存。以及在虚机迁移或销毁后，节点资源情况发生变化，综合决策借用远端numa是否能够归还，迁回虚机并归还远端numa内存。

#### 1.1.3 虚机内存迁移
RMRS提供内存迁移算法。
当借用远端内存后，RMRS根据虚机需要迁出的内存总量，以及每个虚机可迁出的最大内存约束提供虚机迁移策略，决策各虚机迁移到哪个远端numa上，迁移多少内存。
约束：虚机只能使用一个远端NUMA，使用2M大页。

#### 1.1.4 故障处理方案
支持memID级内存CE故障处理，及节点级BMC下电、os panic、reboot故障处理。
memID级内存CE故障处理，在借入端算法决策需要迁移的虚机进程，迁移虚机并将故障memID内存归还。
节点级故障，借入节点故障尝试归还借用内存。借出节点故障，重新借入内存，迁移虚机，并将故障节点内存归还。

| 故障场景 | 借入侧处理 | 借出侧处理 |
| --- | --- | --- |
| BMC 下电 | 主节点：普通（内存归还） | 主节点：可以通信，正常归还（借入节点 NUMA 数据迁移）- 降级调用 memID 级处理 |
| kernel reboot | 主节点：强制归还 | 主节点：不可以通信，强制归还（借入节点 NUMA 数据迁移）- 降级逻辑同 BMC 下电 |
| os panic | 主节点：强制归还 | 主节点：不可以通信，强制归还（借入节点 NUMA 数据迁移）- 降级逻辑同 BMC 下电 |

### 1.2 虚机超分关键技术方案
#### 1.2.1 整体方案设计
虚机超分场景，MemLink动态回收虚机的空余内存或补充内存。在numa内存触发高水位水线时，触发内存借用，并迁出虚机内存，缓解内存压力。在numa内存触发低水位水线时，归还借用内存。
UBS RMRS提供内存借用、虚机迁移接口，提供虚机迁移算法。OS Turbo提供资源采集能力，供迁出算法决策。

#### 1.2.2 内存借用归还
虚机超分在内存借用时，采用UBS engine默认的水线借用算法执行内存借用。

#### 1.2.3 虚机内存迁出
UBS RMRS提供内存迁移算法。
当借用远端内存后，UBS RMRS根据内存账本设置内存的借用总量（包括发起借用的本地numa，借入的远端numa，借用总量），并根据虚机大小，以及可迁出内存的最大比例提供虚机迁移策略，决策各虚机迁移到哪个远端numa上。
约束：虚机只能使用一个远端NUMA，使用2M大页。

#### 1.2.4 故障处理方案
同1.1.4虚机碎片故障处理方案

### 1.3 容器超分关键技术方案
#### 1.3.1 整体方案设计
容器超分场景，支持容器绑定单numa和不绑定numa的场景，
容器绑定单numa时，触发numa水线告警，发起numa内存借用。容器不绑定numa时，触发节点水线告警，发起节点内存借用。

#### 1.3.2 内存借用归还
容器超分在内存借用时，同虚机超分一样，采用UBS engine默认的水线借用算法执行内存借用。规格约束同虚机超分。

#### 1.3.3 容器内存迁移
UBS RMRS中PageCache管理模块利用资源采集信息，识别存在IO瓶颈的容器。当存在IO瓶颈的容器时，利用SMAP迁出匿名页内存时，也迁出PageCache内存，加速容器IO性能。
PageCache管理模块采集IO信息识别瓶颈，自动调节PageCache与匿名页内存的迁出比例。
约束：SMAP迁出进程只能使用一个远端NUMA，使用4K页。

#### 1.3.4 故障处理方案
同7.4.1虚机碎片故障处理方案

## 2 RMRS 安装与部署
### 2.1 须知
- 需切换为 root 用户执行安装与启动。
- 初始安装需保证环境纯净，否则可能使用环境中的配置导致业务异常。
- 依赖 UBTurbo 服务启动正常，否则无法启动 UBSE。

### 2.2 安装 RMRS
从 `memorypooling` 仓库构建出对应包并安装 `mempooling`。

**方式一：仅有 `libmempooling.so`**
将 `libmempooling.so` 放到 `/usr/lib64`。

**方式二：RPM 包**
```bash
rpm -ivh rmrs-*.aarch64.rpm
```

### 2.3 修改配置文件
**1) `/etc/ubse/plugins/plugin_mempooling.conf`**
```ini
ubse.plugin.name=mempooling
ubse.plugin.pkg=libmempooling.so

# 容器超分场景，是否启用 PageCache 迁移功能
ucache.enable=false
rmrs.ipc.timeout=60

# 内存碎片场景，是否必须同平面（true: 必须同平面，false: 优先同平面）
rmrs.fragment.mustSamePlane=true

# 内存碎片场景，借用是否切分为 4G 粒度（true: 切分，false: 不切分）
rmrs.fragmemt.enableBorrowSplit=true
```

大规格虚机场景下，建议调整为：
```ini
rmrs.fragment.mustSamePlane=false
rmrs.fragmemt.enableBorrowSplit=false
```

**2) `/etc/ubse/ubse_plugin_admission.conf`**
取消 `mempooling=777` 的注释，示例如下：
```ini
# vm plugin. Default value: 205
# vm=205

# mempooling plugin. Default value: 777
mempooling=777

# ucache plugin. Default value: 778
# ucache=778
```

## 3 常用命令
### 3.1 重启 UBSE
```bash
systemctl restart ubse
```

### 3.2 检查 mempooling 日志
```bash
ls -l /var/log/ubse/mempooling_plugin.log
```
输出示例：
```
-rw-r----- 1 ubse ubse 20048 Dec 19 10:04 /var/log/ubse/mempooling_plugin.log
```