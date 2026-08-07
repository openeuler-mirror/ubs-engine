# UBSE 配置说明

## 配置原则说明

1. UBS Engine的配置能力通过文件的方式暴露
2. UBS Engine配置文件采用ini文件格式：

    - 由节（section）、键（key）和值（value）构成

    - 配置文件包含多个 section，每个 section 包含多个键值对，section 独自为一行，其名称由中括号包围（ **[** 和 **]**）

    - 每个键值对用等号（**=**）进行连接；左边为键，作为配置项的名称；右边为值，作为配置项内容

3. 配置项设计，确保所有节点相同配置
4. 配置者需要在所有节点修改配置项，并重启各节点UBS Engine进程

UBS Engine配置文件样例如下：

```config
[ubse.log]
# Log level. The value can be DEBUG, INFO (default value), WARN, ERROR, or CRIT. Any invalid value will be reset to INFO.
log.level=INFO
# Size of the log file to be wrapped, in MB. The value range is [2, 20]. Any invalid value will be reset to 20.
log.max.fileSize=20
# Maximum number of log files that can be wrapped. The value range is [1, 200]. Any invalid value will be reset to 20.
log.fileNums=20
# Maximum number of logs that can be cached in the log buffer. If the number of cached logs exceeds the value, the earliest logs are overwritten. Value range: [64,4096]. Any invalid value will be reset to 4096.
log.queue.maxItem=4096
# Syslog switch.The value can be true or false(default value). Any invalid value will be reset to false.
log.sys.open=false
# Syslog type.The value can be kern, user(default value), mail, daemon, auth, syslog, lpr, news, uucp, cron, authpriv, ftp, or local0~7. Any invalid value will be reset to user.
log.sys.type=user

[ubse.election]
# Interval at which the Master node sends heartbeat messages, in milliseconds. The value range is from 1000 to 60000. Any invalid value will be reset to 2000.
heartbeat.timeInterval=2000
# Threshold for the number of lost standby node heartbeats. The value ranges from 3 to 20. Any invalid value will default to 3.
heartbeat.lostThreshold=3

[ubse.vip]
# Whether to enable VIP management. When enabled, the master node binds VIP and announces it to the network.
# The value can be true or false (default value). Any invalid value will be reset to false.
# vip.enable=false
# VIP address with CIDR prefix. Required when vip.enable is true.
# The format is <ip>/<prefix>, e.g. 192.168.100.200/24. The prefix range is [1, 32].
# vip.httpServer.listen.ip=192.168.100.200/24
# HTTP listening port on VIP. The value range is [1024, 65535]. The default value is 10002.
# vip.httpServer.listen.port=10002
# Number of gratuitous ARP packets to send after VIP binding (L2 mode only). The value range is [1, 10]. The default value is 5.
# vip.arpCount=5
# Interval between gratuitous ARP packets in milliseconds (L2 mode only). The value range is [100, 5000]. The default value is 200.
# vip.arpInterval=200

[ubse.ssu]
# Required. Admin Host NVMe Qualified Name, used by Master node to access SSU target for device discovery,
# namespace management (create/delete), and access control (allow host) operations.
#ssu.adminNqn=
```

## log日志配置说明

section取值：[ubse.log]

配置项说明：

| 序号 | 参数              | 说明                                                       | 取值                                                                                                                                                                                                            |
| :--- | :---------------- | :--------------------------------------------------------- |:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1    | log.level         | 日志级别。                                                 | 默认值：INFO <br>取值范围如下：<br>- DEBUG<br>- INFO<br>- WARN<br>- ERROR<br>- CRIT<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                          |
| 2    | log.max.fileSize  | 绕接日志文件的大小。                                       | 默认值：20<br>单位：MB<br>取值范围：[2, 20]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                                                                   |
| 3    | log.fileNums      | 日志文件最大绕接数量。                                     | 默认值：20<br>取值范围：[1, 200]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                                                                           |
| 4    | log.queue.maxItem | 日志缓冲区最大缓存量条数。如果超过该值，则覆盖最初的日志。 | 默认值：4096<br>单位：条<br>取值范围：[64, 4096]<br>参数配置取值范围之外或错误值都会被重置为4096。                                                                                                                                              |
| 5    | log.sys.open      | syslog记录日志开关。                                       | 默认值：false<br>取值范围：[true，false]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                                                                    |
| 6    | log.sys.type      | syslog记录日志类型。                                       | 默认值：user<br>取值范围：<br>kern, user(default value), mail, daemon, auth, syslog, lpr, news, uucp, cron, authpriv, ftp, or local0~7.<br>参数配置取值范围之外或错误值都会被重置为默认值。<br>该配置记录日志规则与syslog的rsyslog.conf一致，部分类型需要用户自配。     |

## ha选举配置说明

section取值：[ubse.election]

配置项说明：

| 序号 | 参数                    | 说明              | 取值                                                               |
|----| ----------------------- |-----------------|------------------------------------------------------------------|
| 1  | heartbeat.timeInterval  | 发送心跳间隔时间，单位毫秒。  | 默认值：2000<br>单位：毫秒<br>取值范围：[1000, 60000]<br>参数配置取值范围之外的值会被重置为默认值。 |
| 2  | heartbeat.lostThreshold | 备节点心跳丢失次数阈值。    | 默认值：3<br>取值范围：[3, 20]<br>参数配置取值范围之外的值会被重置为默认值。                   |
| 3  | election.candidate | 节点是否参与选主。       | 默认值：true<br>取值范围：[true，false]<br>如果取值超过范围，则取默认值true。       |
| 4  | election.wait | 节点是否等待最小节点发起选主。 | 默认值：true<br>取值范围：[true，false]<br>如果取值超过范围，则取默认值true。       |

## rpc通信配置说明

section取值：[ubse.rpc]

配置项说明：

| 序号 | 参数            | 说明                     | 取值                                                                                                                                                               | 备注                                                                                                                                                                                                                                                                                  |
|----| --------------- |------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1  | request.timeout | rpc通信接口的超时时间。          | 默认值：60<br>单位：秒<br>取值范围：[0,65535]<br>如果取值超过范围，则取默认值60。                                                                                                            | -                                                                                                                                                                                                                                                                                   |
| 2  | cluster.rootList | CLOS组网根节点IP列表。         | 默认值：用#注释<br>连续IP地址以“-”连接表示IP范围，非连续IP地址以“,”分隔。<br>参考值：192.168.100.100-192.168.100.102<br>上述参考值等同于配置192.168.100.100,192.168.100.101,192.168.100.102。               | - 1D_FULL_MESH组网模式下，cluster.ipList配置开启时，使用tcp通信；cluster.ipList配置注释时使用urma通信。<br> - CLOS组网模式下，cluster.rootList配置开启时，使用tcp通信，根节点列表同时也作为主节点候选列表；cluster.rootList配置注释、cluster.ipList配置开启时，使用tcp通信；cluster.rootList、cluster.ipList两项配置均注释时，使用urma通信。    |
| 3  | cluster.ipList  | rpc使用tcp协议时的集群各节点IP地址。 | 默认值：用#注释<br>连续IP地址以“-”连接表示IP范围，非连续IP地址以“,”分隔。<br>参考值：192.168.100.100-192.168.100.103<br>上述参考值等同于配置192.168.100.100,192.168.100.101,192.168.100.102,192.168.100.103。 | - 1D_FULL_MESH组网模式下，cluster.ipList配置开启时，使用tcp通信；cluster.ipList配置注释时使用urma通信。<br> - CLOS组网模式下，cluster.rootList配置开启时，使用tcp通信，根节点列表同时也作为主节点候选列表；cluster.rootList配置注释、cluster.ipList配置开启时，使用tcp通信；cluster.rootList、cluster.ipList两项配置均注释时，使用urma通信。 |
| 4  | cluster.pod.capability | 逻辑分组容量。                | 默认值：用#注释<br>取值范围：[2,4096]<br>如果取值超过范围，则取默认值8。<br>该配置仅在CLOS组网模式下生效。                                                                                               | -                                                                                                                                                                                                                                                                                   |
| 5  | cert.use        | rpc安全证书能力开关。           | 默认值：true（开启证书能力）<br>取值范围：[true，false]<br>如果取值超过范围，则取默认值true。                                                                                                     | -                                                                                                                                                                                                                                                                                   |

## UBFM对接配置说明

section取值：[ubse.ubfm]

配置项说明：

| 序号 | 参数             | 说明                                 | 取值                                                                                               | 备注                                                                                                                                               |
| ---- | ---------------- |------------------------------------|-----------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| 1    | ubse.server.port | UBSE暴露给UBM的通信端口，在该端口开启HTTPS服务。     | 默认值：用#注释<br>取值范围：[1024, 65535]<br>如果取值超过范围，则取值为8082。                                    | UBM和UBSE共部署在host主机时，ubm.server.cid用#注释，UBSE与UBM使用UDS通信，此配置项不生效；高安部署模式下，ubm.server.cid配置开启，UBSE与UBM使用HTTPS通信，此配置项生效。                              |
| 2    | ubm.server.port | UBM暴露的通信端口，UBSE通过该端口与UBM进行HTTPS交互。 | 默认值：用#注释<br>取值范围：[1024, 65535]<br>如果取值超过范围，则取值为8799。                                    | UBM和UBSE共部署在host主机时，ubm.server.cid用#注释，UBSE与UBM使用UDS通信，此配置项不生效；高安部署模式下，ubm.server.cid配置开启，UBSE与UBM使用HTTPS通信，此配置项生效。                                                                  |
| 3    | ubm.server.cid | 高安场景UBM所在机密虚机的地址。                  | 默认值：用#注释<br>打开此配置时（取消注释），与UBM使用HTTPS通信，此配置注释时与UBM使用UDS通信。<br>取值范围：[0, 2^32-1] <br>如果取值超过范围，则不使能高安模式。 | - UBM和UBSE共部署在host主机时，该配置项关闭，两者之间使用UDS通信；在高安部署模式下，该配置开启，两者之间使用HTTPS通信。<br>- UBSE与UBM所在机密虚机使用vsock通信，参数配置取值应该与虚拟机启动使用的cid一致，配置错误的cid，会导致与虚拟机通信失败。 |
| 4    | ubm.server.hostname | ubm主机标识，要和ubm侧的服务端证书中主题备用名称（SAN）或通用名称（CN）字段包含的主机标识其中之一一致。如果不一致Ubse启动发生异常，功能不可用。| 默认值：用#注释<br>取值范围：有效的DNS主机名，配置非法值会被重置为默认值localhost|仅高安场景生效。<br>校验失败，Ubse启动发生异常，功能不可用,UBSE与UBM使用HTTPS通信，此配置项生效，需要确保生效的主机名能被DNS解析到ip地址127.0.0.1上

## 内存池化配置说明

section取值：[ubse.memory]

配置项说明：

| 序号 | 参数               | 说明                  | 取值                                                                         | 备注                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
|----|--------------------|---------------------|----------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1  | obmm.memory.block.size   | 芯片表项内存拆分粒度大小。       | 默认值：用#注释<br>单位：MB<br>取值范围：[4, 4096]<br>参数取值必须是2的整数倍。                       | - 管理员可按需修改配置文件，增加该配置，当该配置被定义时以配置文件为准。<br>- 校验规则：<br>1.配置值超范围，取默认值。<br>2.各节点不一致，借用失败。<br>- 当ubse.conf文件不显式配置时，取值规则如下：<br>1.mempool_allocator=buddy_highmem时，obmm.memory.block.size取值为128。<br>2.mempool_allocator=hugetlb_pmd时，obmm.memory.block.size取值为128。<br>3.mempool_allocator=hugetlb_pud时，obmm.memory.block.size取值为1024。 <br>- 当用户借用内存大小不与blocksize对齐时，导出内存会向上取整。<br>- 预上线场景（ubse.preonline.enable配置项为true）建议配置为UB Memory decoder表中entry粒度的整数倍，entry粒度默认为2G。 |
| 2  | isLender           | 当前节点是否可以作为内存借出节点。   | 默认值：true<br>取值范围：[true，false]<br>如果取值超过范围，则取默认值true。                       | —                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 3  | ubse.preonline.enable           | 是否开启预上线能力。          | 默认值：false<br>取值范围：[true，false]<br>如果取值超过范围，则取默认值false。                     | - 依赖环境启用预上线能力，需在`/boot/efi/EFI/openEuler/grub.cfg`文件的numa_remote配置项中配置preonline。<br>- 所有节点的配置需保持一致。                                                                                                                                                                                                                                                                                                                                                               |
| 4  | ubse.preonline.size           | 预上线的内存大小。           | 默认值：4096<br>单位：MB<br>取值范围：[128, 262144]<br>参数取值必须是128的整数倍，取值超过范围则取默认值4096。 | - 依赖环境启用预上线能力，需在`/boot/efi/EFI/openEuler/grub.cfg`文件的numa_remote配置项中配置preonline。<br>- 建议配置为UB Memory decoder表中entry粒度的整数倍，entry粒度默认为2G。<br>- 所有节点的配置需保持一致。                                                                                                                                                                                                                                                                                                        |
| 5  | api.timeout           | API接口调用的超时时间。       | 默认值：1800<br>单位：秒<br>取值范围：[1,3600]<br>如果取值超过范围，则取默认值1800。                   | - 下层执行超过这个时间会给上层API接口调用返回超时，超时后续借用的数据由上层接口管理是否清理。<br>- 所有节点的配置需保持一致。                                                                                                                                                                                                                                                                                                                                                                                              |
| 6  | lender.balance     | 算法是否使用借出 NUMA 均衡模式。 | 默认值：false<br>取值范围：[true，false]<br>如果取值超过范围，则取默认值false。                     | - **true**：资源充足时，不同的借用请求节点尽量优先分散到不同节点的借出numa或者相同节点的不同借出numa上，相同借用请求节点优先复用其历史使用的借出numa。<br>- **false**：默认决策方式，资源充足时，不同的借用请求节点也可能由相同的借出节点的借出numa借出（借出numa优先选择最大剩余内存的numa）。                                                                                                                                                                                                                                                                                          |
| 7  | group           | 同一个组里的节点可以相互借用。     | 默认值：用#注释（所有节点在同一个组里）                                                       | - 同一个组里可以相互借用，以分号为组与组之间的标识。<br>- 不配置时/配置为空时，默认所有节点在同一个组里。<br>- 必须保证集群中所有节点hostname唯一。<br>- 配置的节点数量需要和集群静态规划节点数量一致。<br>- 一个节点不允许在多个group。<br>- 所有节点的配置需保持一致。                                                                                                                                                                                                                                                                                                       |
| 8  | provider           | 指定节点专用于借出。          | 默认值：用#注释（所有节点均可借出）                                                         | - 不配置时/配置为空时，默认所有节点均可借出。<br>- 必须保证集群中所有节点hostname唯一。<br>- 配置的节点数量必须小于集群静态规划节点数量。<br>- 所有节点的配置需保持一致。                                                                                                                                                                                                                                                                                                                                                               |
| 9  | radius.lender      | 借出节点的内存借用半径。        | 默认值：用#注释（不限制）<br>取值范围：[0, 65535]<br>如果取值超过范围，则取默认值65535（不限制）。        | - 控制借出节点允许借用内存的半径范围。<br>- 所有节点的配置需保持一致。<br>- 仅适用于fd和numa借用场景。                                                                                                                                                                                                                                                            |
| 10 | radius.borrow      | 借入节点的内存借用半径。        | 默认值：用#注释（不限制）<br>取值范围：[0, 65535]<br>如果取值超过范围，则取默认值65535（不限制）。        | - 控制借入节点请求内存的半径范围。<br>- 所有节点的配置需保持一致。<br>- 仅适用于fd和numa借用场景。                                                                                                                                                                                                                                                            |
| 11 | obmm.memory.offline.timeout | obmm_unimport操作超时时间。       | 默认值：用#注释<br>单位：秒<br>取值范围：[10, 1800]                                        | - 当显式配置该配置项且值在范围内时，obmm_unimport超时时间固定为该值。<br>- 当不配置、配置值非法或超出范围时，超时时间由内部决策。 |

## URMA配置说明

section取值：[ubse.urma]

配置项说明：

| 序号 | 参数      | 说明                                                   | 取值                                                         | 备注 |
|------|-----------|--------------------------------------------------------|--------------------------------------------------------------|------|
| 1    | topo_mode | UBSE在CLOS组网下向URMA UVS下发拓扑时使用的建链模式。1D-FULLMESH组网下该配置不生效。 | 默认值：用#注释<br>取值范围：<br>- non-cross<br>- hccs-cross<br>参数配置取值范围之外或错误值都会被重置为 non-cross。| 仅CLOS组网使用。 |

## VIP配置说明

section取值：[ubse.vip]

VIP（Virtual IP）管理能力用于在主备切换场景下，将对外服务的虚拟 IP 绑定到当前主节点，保证外部访问地址不变。VIP 模块依赖选主能力，仅主节点绑定 VIP 并对外提供 HTTPS 服务，备节点/Agent 节点不绑定。VIP HTTP 服务使用 TCP+TLS 证书通信，开启前需在 `/var/lib/ubse/vip_server_cert/` 目录下部署证书文件（`server.pem`、`trust.pem`、`ca.crl`、`server_key.pem`、`key_pwd.txt`）。物理网卡名称由 K8s 辅助容器写入 `/var/run/ubse/ubse_iface`，UBSE 启动时读取。

> [!NOTE]权限要求
>
> VIP 模块通过 fork+exec 执行 `ip addr add/del`（IP 绑定/解绑，需 `CAP_NET_ADMIN`）和 `arping`（发送免费 ARP，需 `CAP_NET_RAW`）命令完成网络操作。
>
> 默认安装的 `/usr/lib/systemd/system/ubse.service` 中已包含 `CAP_NET_ADMIN`，但未包含 `CAP_NET_RAW`。验证 VIP 功能前需手动编辑该文件，在 `CapabilityBoundingSet` 和 `AmbientCapabilities` 两项中均添加 `CAP_NET_RAW`：
>
>     ```ini
>     CapabilityBoundingSet=... CAP_NET_ADMIN CAP_NET_RAW ...
>     AmbientCapabilities=... CAP_NET_ADMIN CAP_NET_RAW ...
>     ```
>
> 修改后执行 `sudo systemctl daemon-reload && sudo systemctl restart ubse` 使配置生效。

配置项说明：

| 序号 | 参数                       | 说明                                                  | 取值                                                                                                                                                               | 备注                                                                                                                                                                                                                                                                                       |
|------|----------------------------|-------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1    | vip.enable                 | 是否开启 VIP 管理能力。开启后主节点绑定 VIP 并对外通告。 | 默认值：false<br>取值范围：[true，false]<br>如果取值超过范围则取默认值 false。                                                                                                  | -                                                                                                                                                                                                                                                                                          |
| 2    | vip.httpServer.listen.ip   | VIP 地址及 CIDR 前缀。开启 VIP 时该项必填。           | 默认值：用#注释<br>格式：`<ip>/<prefix>`，例如 `192.168.100.200/24`<br>prefix 取值范围：[1, 32]<br>非法 IP 或 prefix 超范围时启动失败。                                              | - 地址必须为合法 IPv4 地址。<br>- prefix 表示子网掩码长度。                                                                                                                                                                                                                                  |
| 3    | vip.httpServer.listen.port | VIP HTTP 服务的监听端口。                             | 默认值：10002<br>取值范围：[1024, 65535]<br>如果取值超过范围，则取默认值 10002。                                                                                                  | -                                                                                                                                                                                                                                                                                          |
| 4    | vip.arpCount               | VIP 绑定后发送的免费 ARP 报文数量（仅 L2 模式生效）。  | 默认值：5<br>取值范围：[1, 10]<br>如果取值超过范围，则取默认值 5。                                                                                                                | - 用于在主备切换后加速网络设备刷新 ARP 缓存。                                                                                                                                                                                                                                                |
| 5    | vip.arpInterval            | 发送免费 ARP 报文的时间间隔，单位毫秒（仅 L2 模式生效）。 | 默认值：200<br>单位：毫秒<br>取值范围：[100, 5000]<br>如果取值超过范围，则取默认值 200。                                                                                          | -                                                                                                                                                                                                                                                                                          |

## SSU配置说明

section取值：[ubse.ssu]

SSU（Shared Storage Unit）插件用于 UB 存储资源的池化管理，支持命名空间分配/释放、访问权限控制、存储空间挂载/卸载，以及线性编址和条带化（RAID0/RAID5）聚合等能力。该配置段仅在安装 `ubs-engine-ssu` 插件子包后生效。

配置项说明：

| 序号 | 参数         | 说明                                                                                                                                                          | 取值                                                                                               | 备注                                                                                                                                                                                                                                                                                                                                                                                              |
|------|--------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1    | ssu.adminNqn | 管理 Host 的 NVMe Qualified Name（NQN）。主节点通过该 NQN 访问 SSU target，完成设备发现、命名空间创建/删除以及访问控制（allow host）等管理操作。 | 默认值：用#注释<br>取值范围：有效的 NVMe NQN 字符串，最大长度 69 个字符（含结尾字符'\0'）。 | - 使用 SSU 插件时必填，未配置或配置非法将导致 SSU 管理操作失败。<br>- 该 NQN 需与 SSU target 侧已注册的管理 Host NQN 一致。<br>- 所有节点的配置需保持一致。                                                                                                                                                                                                                                          |

### SSU测试环境配置

测试 SSU 相关功能时，除上述 `[ubse.ssu]` 配置段外，还需要修改插件准入配置、systemd 服务环境变量及设备权限。以下配置仅用于测试环境，**禁止用于生产环境**。

**1. 修改 UBSE 主配置文件**

编辑 `/etc/ubse/ubse.conf`，配置 SSU 管理 NQN、CLOS 组网根节点列表，并关闭 TLS 证书认证（测试环境无证书时）：

```bash
sudo vi /etc/ubse/ubse.conf
```

```ini
[ubse.ssu]
ssu.adminNqn=nqn.2026-07.com.example:e2e

[ubse.rpc]
cluster.rootList=192.168.100.100-192.168.100.101
cert.use=false
```

> [!NOTE]说明
>
> - `ssu.adminNqn` 的值需与 SSU target 侧已注册的管理 Host NQN 保持一致。
> - `cert.use=false` 关闭 TLS 证书认证后通信不加密，存在安全风险，仅限可信测试环境使用。
> - `cluster.rootList` 为 CLOS 组网模式下的根节点 IP 列表，同时也是主节点候选列表。

**2. 开启 SSU 插件准入**

编辑 `/etc/ubse/ubse_plugin_admission.conf`，取消 `ssu_plugin` 的注释并设置插件号：

```bash
sudo vi /etc/ubse/ubse_plugin_admission.conf
```

```ini
ssu_plugin=780
```

**3. 修改 systemd 服务环境变量与设备权限**

SSU 插件通过环境变量 `SSU_NVME_SERVER_IP_LIST` 获取 SSU NVMe target 的 IP:PORT/EID 列表，用于设备发现与建链。同时需要给 `ubse` 用户授予 `/dev/` 和 `/dev/disk/by-id/` 的读写执行权限，以便访问聚合块设备。

编辑 `/usr/lib/systemd/system/ubse.service`，在 `[Service]` 段中添加环境变量与 `ExecStartPre` 钩子：

```bash
sudo vi /usr/lib/systemd/system/ubse.service
```

```ini
[Service]
Environment=SSU_NVME_SERVER_IP_LIST=192.168.100.100:18080/0123456789ABCDEF,192.168.100.101:18082/1123456789ABCDEF
Environment=SSU_SRC_EID=0123456789ABCDEF
ExecStartPre=/bin/sh -c "/usr/bin/setfacl -m u:ubse:rwx /dev/disk/by-id/  &>/dev/null || echo 'Unable to set /dev/disk/by-id/ acl'"
ExecStartPre=/bin/sh -c "/usr/bin/setfacl -m u:ubse:rwx /dev/  &>/dev/null || echo 'Unable to set /dev/ acl'"
```

> [!NOTE]说明
>
> - `SSU_NVME_SERVER_IP_LIST` 格式为 `IP:PORT/EID`，多条以逗号分隔。其中 `PORT` 对应 SSU target 的监听端口，`EID` 对应 target 的 EID（16 字节十六进制字符串）。
> - `SSU_SRC_EID` 为本节点（源端）的 EID，用于 SSU 建链时标识自身；取值为 16 字节十六进制字符串，需与 SSU target 侧为本节点登记的 EID 一致。未配置时源端 EID 置零，可能导致 target 侧鉴权失败。
> - `ExecStartPre` 中的 `setfacl` 命令需要系统已安装 `acl` 包；若未安装，可执行 `sudo dnf install -y acl`。
> - 修改 systemd 服务文件后，需执行 `sudo systemctl daemon-reload` 重新加载单元配置。

**4. 重启 UBSE 服务使配置生效**

```bash
sudo systemctl daemon-reload
sudo systemctl restart ubse
```

## 权限点配置

### 权限点说明

基于安全考虑，在UBSE中引入“轻量化角色访问控制（RBAC）能力”，达成对单一外部用户进行权限最小化控制。

```mermaid
graph LR
A(用户/User)-->B(角色/Role)
B(角色/Role)-->C(目标对象/Object)
```

**用户/User**：表示调用UBSE的接口的客户端，因为UBSE暴露的UDS接口，由UBSE通过OS接口自动获取username或者uid（客户端是容器应用的情况下可能没有username）；单用户/User能且只能绑定一个角色/Role

**角色/Role**：表示用户/User所绑定的角色，每个角色/Role可以绑定多个目标对象/Object

**目标对象/Object**：表示能够被操作的资源对象，表示可以对相应对象完成所有操作（简化系统复杂度，不定义基于对象的动作行为）；资源对象的取值范围，由UBSE开发过程确定，在包ubs-engine-client-devel-\<version>-\<release>.aarch64.rpm中可以获取

### 配置说明

客户端自定义配置UBSE接口的权限点，当与默认权限点配置重复时，采用用户自定义权限点配置覆盖默认权限点配置。

**命名规则**

- 文件命名：建议采用 auth\_\{module\}.conf 的命名。
- 内容section：必须使用\[auth.user\]和\[auth.role\]命名，否则不会生效。
- user、role的定义：禁止与其他自定义文件内容重复，重复会相互覆盖，禁止尝试修改内置权限点的内容（配置不会生效）。

**表 1**  内置权限点说明 <a id="table9"></a>

|用户角色|说明|
|--|--|
|UBSE内置管理员用户|ubse（UBSE进程运行用户）、root用户。|
|UBSE内置管理员角色admin|用于支持管理员用户授权。|
|UBSE内置all对象|表示所有对象。|
|管理员角色admin|具备all对象的操控权限。|

>[!NOTE]说明
>内置ubse/root用户、管理员角色admin，均不能被覆盖。

**参数说明**

参考[表2](#table9)配置自定义权限点。

**表 2**  auth\_test.conf配置说明 <a id="table9"></a>

|序号|所属配置节|参数|说明|取值|配置节点|
|--|--|--|--|--|--|
|1|[auth.user]|user|客户端需要自定义配置的用户名禁止与其他自定义文件内容重复，重复会相互覆盖；禁止修改内置用户root,ubse；客户端用户不允许配置为admin角色|客户端需要给该用户配置的角色。|所有节点|
|2|[auth.role]|default_ubsm|客户端需要自定义配置的角色禁止与其他自定义文件内容重复，重复会相互覆盖；禁止修改内置角色admin；自定义角色不允许引用all对象|客户端需要给该角色配置的权限点。|所有节点|

**配置示例**

示例如下：

```shell
[auth.user]
userA = roleA
userB = roleA
[auth.role]
roleA = objectA,objectB
```

### 默认配置

`ubse\_auth\_default.conf`文件用于配置UBSE接口的默认权限点定义，可以被客户端自定义权限点文件的配置覆盖。

**配置说明**

参考[表1](#table8)修改/etc/ubse/ubse\_auth\_default.conf。

**表 1**  ubse\_auth\_default.conf配置说明 <a id="table8"></a>

|序号|所属配置节|参数|说明|取值| 配置节点              |应用场景|
|--|--|--|--|--|-------------------|--|
|1|[auth.user.default]|ubsmd|ubsmd用户|默认值：default_ubsm| 所有节点              |通算大数据场景|
|2|ubs-scheduler|openstack插件用户|默认值：default_op|所有节点| 通算虚拟化场景           |
|3|matrixplugin|k8s插件用户|默认值：default_k8s|所有节点| 通算虚拟化场景           |
|4|[auth.role.default]|default_ubsm|ubsm场景角色|默认值：mem.fd,mem.numa,mem.shm,mem.stat,topo| 所有节点              |通算大数据场景|
|5|default_op|openstack场景角色|默认值：topo,npu|所有节点| 通算虚拟化场景及智算NPU直通虚机 |
|6|default_k8s|k8s场景角色|默认值：mem.numa,topo|所有节点| 通算虚拟化场景           |

**默认配置**

```shell
[auth.user.default]
# Section for default user-to-role mappings.
ubsmd = default_ubsm
ubs-scheduler = default_op
matrixplugin = default_k8s
[auth.role.default]
# Section for default role-to-permission mappings.
default_ubsm = mem.fd,mem.numa,mem.shm,mem.stat,topo
default_op = topo,npu
default_k8s = mem.numa,topo
```

**权限点说明**

|权限点|关联接口|说明|适用场景|
|--|--|--|--|
|mem.fd|ubs\_mem\_fd\_create等|fd形态远端内存借用|通算大数据场景|
|mem.numa|ubs\_mem\_numa\_create等|numa形态远端内存借用|通算大数据/虚拟化场景|
|mem.shm|ubs\_mem\_shm\_create等|共享形态远端内存借用|通算大数据场景|
|mem.stat|ubs\_mem\_numastat\_get|内存统计查询|通算大数据场景|
|topo|ubs\_topo\_node\_list等|拓扑信息查询|所有场景|
|urma|ubs\_urma\_dev\_get等|URMA设备管理|URMA通信场景|
|npu|ubs\_npu\_device\_list\_query, ubs\_npu\_device\_alloc, ubs\_npu\_device\_free, ubs\_npu\_device\_list\_free, ubs\_uba\_tid\_size\_query|NPU设备查询/使能/去使能，允许调用NPU查询接口获取设备拓扑信息，以及NPU使能/去使能接口分配和释放UB设备资源|NPU直通虚机场景，openstack插件需查询NPU设备拓扑并按虚机需求分配/释放NPU资源|

## 场景化插件配置

### plugin\_virt\_agent.conf

**参数说明**

参考[表1](#table6)修改/etc/ubse/plugins/plugin\_virt\_agent.conf。

> [!NOTE]说明
> 
>- 虚拟化场景，vm插件依赖OpenStackPlugin组件，需完成OpenStack部署。
>- 除ubse.plugin.name、ubse.plugin.pkg以及openstack相关参数外，其他参数配置取值范围之外或错误值都会被重置为默认值。

**表 1**  plugin\_virt\_agent.conf配置说明 <a id="table6"></a>

|序号|参数|说明|取值|配置节点|应用场景|
|--|--|--|--|--|--|
|1|ubse.plugin.name|VM模块插件名。|固定值为virt\_agent|所有节点|通算虚拟化场景|
|2|ubse.plugin.pkg|VM模块依赖的so文件名称。|固定值为libvirtagent.so|所有节点|通算虚拟化场景|
|3|ubse.plugin.vm.export.interval|VM模块数据采集周期。|默认值： 2单位： s取值范围：[1, 60]仿真UB环境，取值建议不少于10参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|4|borrow.watermark|决策模块第二逃生水线占比配置，高于会触发内存借用。|默认值：92单位：%取值范围：[70,95]且high.watermark <= borrow.watermark - 5参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|5|high.watermark|高水线百分比，高触发借用或迁移，低触发归还，且用于决策模块。|默认值：85单位：%取值范围：[65~90]且low.watermark +5 <= high.watermark <= borrow.watermark - 5参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景。|
|6|low.watermark|低水线百分比，高触发借用或迁移，低触发归还，且用于决策模块。|默认值：80单位：%取值范围：[60~80]low.watermark +5 <= high.watermark参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景。|
|7|borrow.maxMemPerBorrowSize|单块借用内存的借用量上限。|默认值： 4096单位：MB取值范围：{1024,2048,3072,4096}且borrow.minMemPerBorrowSize <= borrow.maxMemPerBorrowSize参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|8|borrow.minMemPerBorrowSize|单块借用内存的借用量下限。|默认值： 1024单位：MB取值范围：{1024,2048,3072,4096}参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|9|borrow.maxPerTotalMemBorrowSize|单次决策最多内存借用上限。|默认值： 16384单位：MB取值范围：[4096, 20480]参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|10|borrow.oomBorrowMemSize|oom事件期望借用内存大小。|默认值：1024单位：MB取值范围：[1024,4096]参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|11|escape.algorithm.dir|自定义逃生算法so文件的绝对路径，配置中应包含so文件名称。由配置方保证加载so文件的安全可靠性。|默认值：/usr/lib64/libstrategy.so|所有节点|通算虚拟化场景|
|12|mig.ham.maxTimeout|单个VM确定性热迁移的任务的超时时间。|默认值：60单位：s取值范围：[10, 10800]UB仿真环境建议配成10800。参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|13|mig.migrateOneCopyMemoryBound|UB代际下虚拟机热迁移决策OneCopy迁移策略的虚拟机内存阈值。|默认值： 4096单位：MB取值范围：[256,4096]UB仿真环境建议配成256。当目的节点为当前节点的邻居节点，虚拟机使用2M大页，节点间内存借用没有成环，且虚拟机内存规格大于mig.migrateOneCopyMemoryBound配置项的值时，为确定性热迁移。参数配置取值范围之外或错误值都会被重置为默认值。|所有节点|通算虚拟化场景|
|14|virt.pageType|虚拟化场景页面类型配置|默认值：1取值范围：01用于设置冷内存迁出时的页面单位，标准设置为0，2M设置为1。|所有节点|通算虚拟化场景|
|15|mig.isEnableHamMigrate|是否支持确定性热迁移开关|默认值： false取值范围：falsetrue开启确定性热迁移，使用true。|所有节点|通算虚拟化场景|
|16|virt.sceneType|虚拟化场景类型应用配置|默认值：1取值范围：01容器场景设置为0，虚拟机场景设置为1。|所有节点|通算虚拟化场景|

**配置示例**

示例如下：

```shell
ubse.plugin.name=virt_agent
ubse.plugin.pkg=libvirtagent.so
borrow.watermark=92
high.watermark=85
low.watermark=80
borrow.maxMemPerBorrowSize=4096
borrow.minMemPerBorrowSize=1024
borrow.maxPerTotalMemBorrowSize=16384
mig.ham.maxTimeout=60

mig.isEnableHamMigrate=false

virt.pageType=1
virt.sceneType=1
```

### plugin\_mempooling.conf

**参数说明**

参考[表1](#table7)修改/etc/ubse/plugins/plugin\_mempooling.conf。

**表 1**  plugin\_mempooling.conf配置说明 <a id="table7"></a>

|序号|参数|说明|取值|配置节点|应用场景|
|--|--|--|--|--|--|
|1|ubse.plugin.name|mempooling模块插件名。|固定值为mempooling|所有节点|通算虚拟化场景|
|2|ubse.plugin.pkg|mempooling模块依赖的so文件名称。|固定值为libmempooling.so|所有节点|通算虚拟化场景|
|3|ucache.enable|容器超分场景，是否启用pagecache功能|默认值：false单位： bool|所有节点|容器超分场景|
|4|rmrs.ipc.timeout|与osturbo进行进程通信的超时时间|默认值：60单位：s|所有节点|通算虚拟化场景|
|5|rmrs.fragment.mustSamePlane|内存碎片场景，内存借用、迁出策略、迁出执行、故障处理，是否必须同平面(true:必须同平面，false:优先同平面)|默认值：true单位：bool配置范围：false/true|所有节点|通算虚拟化场景|
|6|rmrs.fragment.enableBorrowSplit|内存碎片场景，借用是否切分为 4G 粒度（true: 切分，false: 不切分）|默认值：true单位：bool配置范围：false/true|所有节点|通算虚拟化场景|
|7|overcommit.enableMultiNuma|容器超分场景，是否为虚机多numa场景（true: 虚机多numa场景, false: 单numa场景）|默认值：false单位： bool配置范围：false/true|所有节点|容器超分场景|

**配置示例**

示例如下：

```shell
ubse.plugin.name=mempooling
ubse.plugin.pkg=libmempooling.so
ucache.enable=false
rmrs.ipc.timeout=60
rmrs.fragment.mustSamePlane=true
rmrs.fragment.enableBorrowSplit=true
overcommit.enableMultiNuma=false
```

### plugin\_ucache.conf

**参数说明**

参考[表1 plugin\_ucache.conf配置说明](#table7)修改/etc/ubse/plugins/plugin\_ucache.conf。

**表 1**  plugin\_ucache.conf配置说明 <a id="table7"></a>

|序号|参数|说明| 取值                                                 |配置节点|应用场景|
|--|--|--|----------------------------------------------------|--|--|
|1|ubse.plugin.name|UCache模块插件名。| 固定值为ucache                                         |所有节点|通算大数据场景|
|2|ubse.plugin.pkg|UCache模块依赖的so文件名称。| 固定值为libucache_plugin.so                            |所有节点|通算大数据场景|
|3|ucache.export.interval|UCache模块数据采集周期。| 默认值： 10单位： s取值范围：[1, 60]仿真UB环境，取值建议不少于10。          |所有节点|通算大数据场景|
|4|strategy.borrow_size|以16进制表示，单次借用或归还动作操作的最小内存大小。参数不在配置范围内，插件初始化失败。| 默认值：1073741824单位：字节配置范围：[134217728,4294967296]     |所有节点|通算大数据场景|
|5|strategy.maxActionSize|借用策略动作集动作的最大数量，整数型。参数不在配置范围内，插件初始化失败。| 默认值：10配置范围：>=0                                     |所有节点|通算大数据场景|
|6|strategy.balanceThreshold|借用策略中平衡阈值，节点的平衡状态如果大于平衡阈值将会发生借用调度来平衡系统，浮点型。参数不在配置范围内，插件初始化失败。| 默认值：1.5配置范围：>=1                                    |所有节点|通算大数据场景|
|7|strategy.scarcityThreshold|发起借用动作时的最小内存稀缺度，浮点型。参数不在配置范围内，插件初始化失败。| 默认值：0.01配置范围：>=0                                   |所有节点|通算大数据场景|
|8|bottleneck.threshold|应用瓶颈识别的阈值，来判断PageCache in和IO读写带宽，整数型。参数不在配置范围内，则采用默认值。| 默认值：10240单位：KB/s配置范围：[1,10*1024*1024]              |所有节点|通算大数据场景|
|9|bottleneck.short.size|应用瓶颈识别的短窗口长度，整数型。参数不在配置范围内，则采用默认值。| 默认值：180单位：秒配置范围：[30,1200]                          |所有节点|通算大数据场景|
|10|bottleneck.short.threshold|应用瓶颈识别的短窗口阈值，整数型。参数不在配置范围内，则采用默认值。| 默认值：70单位：百分比配置范围：[0,100]                           |所有节点|通算大数据场景|
|11|bottleneck.long.size|应用瓶颈识别的长窗口长度，整数型。参数不在配置范围内，则采用默认值。| 默认值：600单位：秒配置范围：[90,3600]                          |所有节点|通算大数据场景|
|12|bottleneck.long.threshold|应用瓶颈识别的长窗口阈值，整数型。参数不在配置范围内，则采用默认值。| 默认值：30单位：百分比配置范围：[0,100]                           |所有节点|通算大数据场景|
|13|strategy.maxReliableTimes|借用策略数据最大可靠周期数，每个周期的时间由master.masterInterval 决定，整数型。参数不在配置范围内，则采用默认值。| 默认值：3配置范围：[1,10]                                   |所有节点|通算大数据场景|
|14|master.masterInterval|master主流程间隔时间，整数型。参数不在配置范围内，插件初始化失败。| 默认值：3单位：秒配置范围：[1,15]                               |所有节点|通算大数据场景|
|15|master.maxBorrowSize|最大借用内存量，整数型。参数不在配置范围内，插件初始化失败。| 默认值：1073741824单位：字节配置范围：strategy.borrow_size的正整数倍。 |所有节点|通算大数据场景|

**配置示例**

示例如下：

```shell
ubse.plugin.name=ucache
ubse.plugin.pkg=libucache_plugin.so
# export interval
ucache.export.interval=10
# Borrowing strategy unit
strategy.borrow_size=1073741824
# The maximum number of output actions of the borrowing strategy
strategy.maxActionSize=10
# Borrowing strategy balance threshold
strategy.balanceThreshold=1.5
# Borrowing strategy minimum borrowing scarcity
strategy.scarcityThreshold=0.01
# Bottleneck - threshold (type: uint32, unit: kb/s, default: 10240 kb/s)
#    min: 1
#    max: 10*1024*1024 (nvme disk read limit)
bottleneck.threshold=10240
# Bottleneck - short window size (type: uint32, unit: second, default: 180)
#    min: 30. The window has at least 3 points, calculated based on a 10-second period, and at least 30 seconds.
#    max: 1200. according to the puncture experience, the PageCache volume reaches its peak in about 20 minutes.
bottleneck.short.size=180
# Bottleneck - short window threshold (type: uint32, unit: percent, default: 70%)
#    min: 0
#    max: 100
bottleneck.short.threshold=70
# Bottleneck - long window size (type: uint32, unit: second, default: 600)
#    min: 90
#    max: 3600
bottleneck.long.size=600
# Bottleneck - long window threshold (type: uint32, unit: percent, default: 30%)
#    min: 0
#    max: 100
bottleneck.long.threshold=30
# Maximum reliable period of borrowing strategy data
strategy.maxReliableTimes=3
# Master main process interval time (s)
master.masterInterval=10
# Maximum borrowed memory, Byte
master.maxBorrowSize=1073741824
```
