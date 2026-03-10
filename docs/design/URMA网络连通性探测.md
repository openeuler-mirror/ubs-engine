# 容器网络连通性探测

## 1. 方案背景

在传统TCP/IP网络架构中，客户普遍依赖ICMP Ping或TCP端口探测来验证主机与容器间的网络连通性。容器化环境下，运维人员需同时监控两类连通性：主机IP连通性和容器IP连通性，以确保分布式应用的正常运行。当容器通过CNI插件获得虚拟IP后，探测系统会基于IP地址矩阵执行周期性连通性检查，形成网络拓扑健康视图。

随着华为灵衢通算超节点的部署，计算节点间通信从TCP/IP协议栈切换至URMA(Unified Remote Memory Access)直通模式。URMA通过旁路内核协议栈实现微秒级远程内存访问，显著提升跨节点数据交换效率，但同时也改变了连通性验证的范式：IP地址不再是通信端点标识，取而代之的是URMA设备的全局唯一标识符EID
(Endpoint Identifier)，主机/容器通过绑定的URMA设备的EID进行URMA通信

由于URM通信完全脱离传统TCP/IP协议栈，传统Ping工具失效，必须通过专用的urma_ping工具基于EID发起端到端探测。为支撑云管系统执行全网连通性扫描，各节点需主动上报两类EID信息：

- 主机EID: 节点自身URMA设备的全局EID标识;

- 容器EID: 容器内直通的URMA设备的全局EID标识

云管采集上述信息后，探测源即可构建完整的“主机-容器”EID映射矩阵，执行跨节点，跨容器的URMA连通性探测，确保超节点网络在微秒级时延要求下仍具备可观测性与故障快速定位能力。

## 2. 方案原理

连通性探测如下：

![image-20260213170948280](images/image-20260213170948280.png)

在上述方案中，需要各层支持：

- 云管系统：支持采集不同节点主机和容器的EID信息，并下发探测任务到探测系统
- 探测系统：支持接收云管下发的探测任务，并将任务下发到对应连通性探测agent，并支持收集探测结果并上报
- 主机节点：
    - 支持采集主机/容器EID信息并上报云管系统
    - 部署连通性探测Agent，响应探测系统下发的探测任务，对目的容器/主机进行连通性探测，并返回探测结果

## 3.UBSE相关能力说明

在容器网络连通性探测方案中，需要UBSE提供主机和容器绑定URMA设备的EID信息查询能力。

### 3.1 主机EID信息查询

针对主机EID查询，需要采用如下命令：

```
$ ubsectl display node
-----------------------------------------------------------------------------------
node            role                   bonding-eid
-----------------------------------------------------------------------------------
compute01(1)    master                 4245:4944:0000:0000:0000:0000:0100:0000
```

输出字段说明：

- node: 节点信息。例：compute01(1)，节点信息由2部分分组成：括号前部分：主机名; 括号内部分：节点的槽位号
- role: 节点在集群中的角色，可选值：[master|standby|agent]
- bonding-eid: 节点bonding配置的eid

### 3.2 URMA设备EID信息查询

针对URMA设备EID查询，需要采用如下命令：

```
$ ubsectl display urma --dev urma_1,urma_2
--------------------------------------------------------------------------------------
urma-name    dev-eid    dev1-name   dev2-name   dev1-eid    dev2-eid     status
--------------------------------------------------------------------------------------
urma_1        eid0       udma1      udma49      eid1      eid2       active
urma_2        eid3       udma2      udma50      eid4      eid5       inactive
```

输出字段说明：

- urma-name：bonding设备的名称
- type：bonding设备的类型
- urma-eid：bonding设备的Eid
- dev1-name: bonding设备绑定的dev1名称
- dev2-name: bonding设备绑定的dev2名称
- dev1-eid: bonding设备绑定的dev1 Eid
- dev2-eid: bonding设备绑定的dev2 Eid
- status: bonding设备状态

## 4. 参考示例

为便于客户快速构建URMA连通性探测能力，本节提供一个EID信息采集参考脚本。该脚本可自动收集节点级与容器级URMA设备的EID信息，并以Prometheus标准格式输出，供云管系统直接采集和消费。

参考示例路径为[urma_eid_collector.py](../example/urma_eid_collector.py)

### 4.1 脚本部署
在k8s集群的每个物理节点部署该文件，并执行。

### 4.2 前置条件检查
软件运行依赖ubsectl和dmidecode命令可以正常执行，并且以root权限运行
```
# 必需命令验证
which ubsectl dmidecode > dev/null || echo "ERROR:ubsectl or dmidecode not found"
```

### 4.3 脚本使用
```
# 基本用法，默认情况下采集结果输出到 /var/log/ub_dev_eid.prom
/usr/local/bin/urma_eid_collector.py # 或者 python3 /usr/local/bin/urma_eid_collector.py

# 脚本支持指定输出路径
/usr/local/bin/urma_eid_collector.py -o /custom/path/ub_dev_eid.prom

# 查看帮助
/usr/local/bin/urma_eid_collector.py -h

# 输出打屏信息
/usr/local/bin/urma_eid_collector.py -v
```

### 4.4 脚本输出结果说明
采集脚本会生成一个满足Prometheus格式的文件
```
# HELP urma_eid_type URMA device EID information
# TYPE urma_eid_type gauge
urma_eid_type{poduid="",container="",host_uuid="01920156-2022-xxxx-xxxx-xxxxxxxxxxxx",ub_dev="",dev_eid="8424:1234:0000:0000:0000:0000:0000:0000",dev1_eid="",dev2_eid="",ip_addr="xx.xx.xx.xx"} 0
urma_eid_type{poduid="96eead6a-0dab-xxxx-xxxx-xxxxxxxxxxxx",container="urmacontainer",host_uuid="01920156-2022-xxxx-xxxx-xxxxxxxxxxxx",ub_dev="",dev_eid="0000:1234:0000:0000:0000:0000:3200:0000",dev1_eid="0000:1234:0000:0000:0000:0000:3200:1000",dev2_eid="0000:1234:0000:0000:0000:0000:3200:2000",ip_addr="xx.xx.xx.xx"} 1
```

- poduid: K8s Pod的唯一标识（容器场景填充，主机场景为空字符串）
- container: 容器名称（容器场景填充，主机场景为空字符串）
- host_uuid: 主机系统UUID（所有指标相同）
- ub_dev: URMA设备名（主机场景为空字符串，容器场景为实际设备名，如bonding_dev_1）
- dev_eid: URMA设备EID信息
- dev1-eid: URMAbonding设备绑定的dev1 Eid（容器场景填充，主机场景为空字符串）
- dev2-eid: URMAbonding设备绑定的dev2 Eid（容器场景填充，主机场景为空字符串）
- ip_addr: IP信息（主机场景为kubelet监听IP，容器场景为POD的IP）
- urma_eid_type: EID类型，0表示主机EID，1表示容器EID

### 4.5 脚本修改指南

#### 4.5.1 脚本结构概览

`urma_eid_collector.py` 主要模块：

1. **LabelRegistry 标签注册中心**
   - 装饰器风格的标签管理机制
   - 统一处理 host 和 container 场景的标签

2. **Host 信息采集**
   - 获取节点 UUID、URMA EID 和 IP 地址

3. **容器信息采集**
   - 解析 Kubernetes checkpoint 文件获取 container 与 URMA 设备映射
   - 获取容器 IP（严格按 `(container_name, pod_uid)` 匹配）

4. **设备信息采集与处理**
   - 查询 URMA 设备 EID
   - 可根据需要扩展采集额外字段或新增指标

5. **Prometheus metrics 生成**
   - `build_labels(mapping)` 构建动态 label 字典
   - `format_labels(labels)` 转换为 Prometheus 文本格式
   - `generate_prom_file()` 写入输出文件

6. **主流程**
   - 按顺序执行 Host → Container → Device → Prometheus 文件生成
   - 保证每条 metric 标签完整、IP 信息正确


#### 4.5.2 扩展指南

**新增指标 label 的方法**：

使用 `@registry.label("label_name")` 装饰器注册新标签，函数签名统一为 `(collector, mapping)`：

```python
@registry.label("namespace")
def _label_namespace(collector: 'UrmaEidCollector', mapping: Optional[Dict]) -> str:
    """新增 namespace 标签"""
    if mapping:
        return mapping.get("namespace", "default")
    return ""
```

**标签函数参数说明**：
- `collector`: UrmaEidCollector 实例，可访问 `host_uuid`、`host_eid`、`device_eid_map` 等属性
- `mapping`: 容器映射字典，包含 `pod_uid`、`container_name`、`device_id`、`ip_addr` 等字段
  - `mapping is None`: 表示 host 场景
  - `mapping is not None`: 表示 container 场景

**扩展示例**：

```python
@registry.label("cluster_name")
def _label_cluster(collector: 'UrmaEidCollector', mapping: Optional[Dict]) -> str:
    return collector.cluster_name or ""

@registry.label("pod_name")
def _label_pod_name(collector: 'UrmaEidCollector', mapping: Optional[Dict]) -> str:
    return mapping.get("pod_name", "") if mapping else ""
```

**保持兼容性**：
- 原有指标和逻辑不受影响
- Host metric 对新增 label 默认返回空字符串
- IP 和 container 映射逻辑保持不变  
  
## 5. 主机和容器连通性探测说明
在完成EID信息采集后，云管系统将采集各节点生成的Prometheus指标文件，构建全网探测任务矩阵，并下发至各探测源执行urma_ping探测。本节介绍探测任务执行流程、结果解析方法及健康评估标准。

### 5.1 探测任务生成与下发
云管系统通过以下步骤生成探测任务：
1. EID信息采集：从各节点采集的ub_dev_eid.prom文件中获取两类EID，获取主机/容器EID,以及和IP地址关联关系
```
# 主机EID (指标值=0)
urma_eid_type{...,dev_eid="8424:4944:0000:0000:0000:0000:0000:0000",dev1_eid="",dev2_eid="",ip_addr="xx.xx.xx.xx"} 0
urma_eid_type{...,dev_eid="0000:0000:0100:0000:0000:0000:0000:1100",dev1_eid="0000:0000:0100:0000:0000:0000:4DA0:1100",dev2_eid="0000:0000:0100:0000:0000:0000:0000:081D",ip_addr="xx.xx.xx.xx"} 1
```
2. 规划生成探测任务：云管系统构建探测矩阵，包括容器级探测和主机级探测

3. 下发探测任务：云管将探测任务下发至源节点探测Agent

### 5.2 探测Agent使用urma_ping进行探测
探测Agent使用urma_ping命令对目的容器/主机进行探测，参考示例如下
```
urma_ping -c 5 -s 4096 -i 0.5 -s 4096 -w 15 -W 2 0000:0000:0100:0000:0000:0000:0000:1100
```
其中urma_ping的目的EID为探测Agent需要探测的目的主机/容器的EID，探测命令携带关键参数作用如下：

| 参数 | 说明                            |
|----|-------------------------------|
| -c | 探测包数量，太少统计意义不足，太多影响业务         |
| -i | 发包间隔，避免突发流量冲击（单位：秒）           |
| -w | 总超时时间，防止命令卡死（单位：秒）            |
| -W | 单个探测包超时时间，避免单包无响应导致整体超时（单位：秒） |
| -s | 包大小，建议配置为典型业务包，默认4字节 （单位：字节）  |

详细参数可以通过 urma_ping --help 了解
通过urma_ping进行探测的典型结果如下：
```
$ urma_ping -c 5 -s 4096 -i 0.5 -s 4096 -w 15 -W 2 0000:0000:0100:0000:0000:0000:0000:1100

URMA_PING 0000:0000:0100:0000:0000:0000:0000:1100 4096 bytes of data.
4096 bytes from 0000:0000:0100:0000:0000:0000:0000:1100: seq=1 time=1.020ms
4096 bytes from 0000:0000:0100:0000:0000:0000:0000:1100: seq=2 time=0.920ms
4096 bytes from 0000:0000:0100:0000:0000:0000:0000:1100: seq=3 time=0.910ms
4096 bytes from 0000:0000:0100:0000:0000:0000:0000:1100: seq=4 time=0.900ms
4096 bytes from 0000:0000:0100:0000:0000:0000:0000:1100: seq=5 time=0.820ms

--- ping statistics ---
5 packets transmitted,5 received, 0.0% packet loss, time 5100ms
rtt min/avg/max/mdev = 0.890/0.928/1.020/0.047 ms
```
建议客户基于urma_ping返回的探测丢包情况、时延和抖动情况合理评估连通性结果，并返回目的节点连通性结果