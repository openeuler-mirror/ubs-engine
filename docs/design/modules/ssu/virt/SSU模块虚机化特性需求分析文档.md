# SSU 模块虚机化特性需求分析文档

> **文档版本**:v1.2
> **工作模式**:混合模式(现有 SSU 模块代码反推 + 新增虚机化特性正向需求)
> **模块路径**:`src/controllers/ssu/`(现有模块,新增特性)+ `src/framework/vscok/`(扩展)
> **特性定位**:本特性为 SSU 模块的虚机化场景扩展,**不是新增模块**,所有代码归入现有 SSU 模块目录
> **关联文档**:[SSU模块需求分析文档](../SSU模块需求分析文档.md)、[SSU模块特性设计文档](../SSU模块特性设计文档.md)、[UBSE技术架构设计文档](../../../architecture/UBSE技术架构设计文档.md)
> **变更记录**:v1.2(2026-10-27)接收自模块需求分析文档迁出的 VFE 直通配套内容:NFR-REL-06(空实例清理周期)、RISK-13/RISK-14、host 侧 IF-16/IF-17 接口规约、fe device CLI 命令缺口、FR-FE-DEVICE-04 清理常量

---

## 1. 引言

### 1.1 文档目的

本文档从需求视角描述 SSU 模块在虚拟化场景下的特性扩展(ubse@VM 部署形态),作为 SSU 模块虚机化特性开发、测试与验收的依据。文档基于现有 SSU 模块代码反推 + 新增虚机化场景需求正向,采用混合模式编写。

### 1.2 特性定位(重要)

本特性**不是新增独立模块**,而是对现有 SSU 模块的能力扩展:

- **代码归入** `src/controllers/ssu/`,不新建 `src/controllers/ssu_virt/`
- **模块入口复用** 现有 `UbseSsuControllerModule`,通过 `vm_mode` 配置开关在 Initialize/Start 阶段分流
- **服务契约复用** 现有 `UbseSsuService`(`src/include/plugin_services/ssu/ubse_ssu_service.h`),不新增契约
- **服务实现扩展** 现有 `UbseSsuServiceImp`,在 VM 模式下走本地缓存/本地适配器分支
- **vsock 通信(VM 侧)** 扩展现有 `src/framework/vscok/`,新增 `UbseVsockServer` 接收 host 侧 ub-device-manager 推送
- **CLI 扩展** 现有 `src/cli/ssu/`,在 VM 模式下注册子集命令

### 1.3 组件边界(关键)

本特性涉及 host 侧两个独立组件,职责严格划分,不互相侵入:

| 组件 | 部署位置 | 职责 | 是否本特性交付 |
|------|----------|------|----------------|
| **ubse@host** | host 物理机 | SSU 资源核心管理(alloc/free/FeDeviceAlloc/权限/账本/选举等),**不与 VM 内 ubse 直接交互** | ❌ 现有组件,本特性不修改 |
| **ub-device-manager** | host 物理机 | host 侧独立组件,负责通过 vsock 与 VM 内 ubse 通信,推送分配信息与连接信息 | ❌ 独立外部组件,不在本特性范围 |
| **ubse@VM** | 虚机内 | VM 内轻量化 ubse,启动 vsock server 接收 ub-device-manager 推送,提供查询/attach/detach/CLI 能力 | ✅ 本特性核心交付 |

**关键事实**:
- host 侧 ubse **不**与 VM 内 ubse 直接交互,不持有 vsock client,不感知 VM 内 ubse 的存在
- 与 VM 内 ubse 交互的是 host 上的 **ub-device-manager** 组件(独立进程)
- ub-device-manager 通过现有 SDK/IPC 接口从 ubse@host 查询分配信息与连接信息,再通过自己的 vsock client 推送到 VM
- 本特性仅交付 VM 侧 ubse 的能力(以及为支持 VM 模式对现有 SSU 模块入口的分流扩展)

### 1.4 文档范围

- **覆盖**:ubse@VM 部署模式、VM 内 vsock server、VM 内查询/attach/detach 能力、CLI 接口、设备直通后的 VM 内消费链路,含自模块文档迁入的 VFE 直通现有能力规约(4.8)
- **不覆盖**:SSU 基础分配/释放/挂载能力(已在 [SSU模块需求分析文档](../SSU模块需求分析文档.md) 中描述)、openstack 侧盘分配编排、virt_agent 的 VM 生命周期管理、**ub-device-manager 组件的设计与实现**(独立外部组件)
- **复用**:现有 `UbseSsuService` 契约、`UbseSsuDirectToVmManager` 的 VFE 直通能力、`UbseVsockClient` 的 vsock 通信基础、`UbseModule` 模块化机制

### 1.5 术语与缩略语

| 术语 | 含义 |
|------|------|
| ubse@host | 部署在 host 侧的 ubse 守护进程(现有形态,负责 SSU 核心管理) |
| ub-device-manager | 部署在 host 侧的独立组件,负责通过 vsock 与 VM 内 ubse 通信,推送分配/连接信息 |
| ubse@VM | 部署在虚机内的轻量化 ubse 守护进程(本特性新增) |
| vm_mode | SSU 模块的部署模式配置开关(host 模式 / VM 模式) |
| VFE | Virtual Function Element,SSU 设备的虚拟功能单元,可直通到 VM |
| BusInstance | MTI 总线实例,标识一个 VM 的设备挂载点 |
| vsock | VM socket,Linux 提供的 host↔VM 高效通信通道(AF_VSOCK) |
| openstack | 云编排平台,负责 VM 创建与盘的预分配 |
| 设备直通 | 将 SSU 物理设备的 VFE 透传到虚机内,VM 可直接访问 |
| 分配信息 | SSU namespace 的分配结果(name/size/nqn/devPath 等) |
| 连接信息 | VM 内访问 SSU namespace 所需的 NVMe 连接参数(nqn/eid/connection id 等) |

### 1.6 参考资料

- [SSU模块需求分析文档](../SSU模块需求分析文档.md)
- [SSU模块特性设计文档](../SSU模块特性设计文档.md)
- [UBSE技术架构设计文档](../../../architecture/UBSE技术架构设计文档.md)
- [高安部署说明.md](../../../../build_install/高安部署说明.md)
- [目录规范文档](../../../../conventions/目录规范文档.md)

### 1.7 编号体系

| 前缀 | 含义 | 示例 |
|------|------|------|
| FR-VM-XXX | 虚机化特性功能需求(VM 侧) | FR-VM-DEPLOY |
| FR-BOUNDARY-XXX | 组件边界划分需求 | FR-BOUNDARY-01 |
| NFR-XXX-XX | 非功能需求 | NFR-REL-01 |
| AC-XXX-XX | 验收标准 | AC-VM-QUERY-01 |
| CON-XXX | 约束条件 | CON-OPENSTACK |
| TBD-XXX | 待澄清项 | TBD-VSOCK-PROTO |
| ASSUMPTION-XXX | 假设项 | ASSUMPTION-01 |
| FR-FE-DEVICE | VFE 直通现有能力需求(迁移自模块文档,保持原编号) | FR-FE-DEVICE-01 |
| AC-FE-XX | VFE 直通现有能力验收标准(迁移自模块文档,保持原编号) | AC-FE-01 |

---

## 2. 业务背景与目标

### 2.1 业务背景

SSU 模块当前主要面向 host 侧场景:ubse@host 通过 `libssu.so` 操作 NVMe 设备,通过 `UbseSsuDirectToVmManager` + MTI BusInstance 将 VFE 直通到 VM。但 VM 内部无法直接管理已直通的 SSU 设备,存在以下业务痛点:

1. **VM 内不可见**:VM 内应用无法查询已直通的 SSU namespace 分配信息(name/size/devPath),必须通过 host 侧 SDK 间接查询,链路长、延迟高
2. **VM 内不可控**:VM 内应用无法在虚机内部完成 namespace 的 attach/detach,所有操作必须绕行 host,无法满足虚机自治运维诉求
3. **直通编排割裂**:host 侧虽能将 VFE 直通到 VM,但"直通后如何通知 VM 内部使用、VM 内如何获取连接信息"缺乏端到端机制
4. **云编排割裂**:openstack 完成盘分配后,分配结果与连接信息无法直接传递到 VM 内的 ubse,导致 VM 内 ubse 无法感知已分配的资源

为解决上述痛点,引入 ubse@VM 部署形态。架构上采用职责分离:
- **ubse@host** 继续专注 SSU 核心管理,不感知 VM
- **ub-device-manager** 作为 host 侧独立组件,负责把 ubse@host 的分配/连接信息推送到 VM
- **ubse@VM** 在虚机内部接收推送,提供查询/attach/detach 能力

### 2.2 业务目标

| 目标 | 说明 | 归属组件 |
|------|------|----------|
| 虚机内部署 ubse | 在虚机内部署轻量化 ubse,作为 VM 内 SSU 资源管理代理 | ubse@VM(本特性) |
| VM 内 vsock server | VM 模式下 SSU 模块启动 vsock server,接收 ub-device-manager 推送 | ubse@VM(本特性) |
| VM 内查询/attach/detach | VM 内 ubse 提供查询空间分配信息、attach、detach 三类能力 | ubse@VM(本特性) |
| VM 内 CLI 接口 | VM 内 ubse 提供 CLI 命令,支持运维人员在 VM 内直接操作 | ubse@VM(本特性) |
| 设备直通编排 | 复用现有 FeDeviceAlloc 能力,直通后由 ub-device-manager 推送 | ubse@host(现有) + ub-device-manager(外部) |
| host→VM 信息推送 | ub-device-manager 通过 vsock 向 VM 内 ubse 推送分配信息与连接信息 | ub-device-manager(外部,不在本特性范围) |

### 2.3 在系统中的位置

```
┌─────────────────────────────────────────────────────────────────────┐
│                       Host 物理机                                    │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  ubse@host(现有 SSU 模块,host 模式,本特性不修改)          │   │
│  │  ├── UbseSsuControllerModule(vm_mode=false)                  │   │
│  │  │   ├── AllocSpace/FreeSpace/AttachSpace/DetachSpace        │   │
│  │  │   ├── FeDeviceAlloc/Free(现有 VFE 直通)                  │   │
│  │  │   └── (不与 VM 内 ubse 直接交互)                          │   │
│  │  └── 对外暴露 SDK/IPC/HTTP 接口(现有,ub-device-manager 消费)│   │
│  └────────────────────┬─────────────────────────────────────────┘   │
│                       │ SDK/UDS IPC(现有契约)                       │
│  ┌────────────────────▼─────────────────────────────────────────┐   │
│  │  ub-device-manager(独立外部组件,不在本特性范围)             │   │
│  │  ├── 通过 SDK/IPC 查询 ubse@host 的分配/连接信息             │   │
│  │  ├── 自带 vsock client,连接 VM 内 ubse@VM                   │   │
│  │  └── 在 FeDeviceAlloc 后触发推送(由 ub-device-manager 自行感知)│  │
│  └────────────────────┬─────────────────────────────────────────┘   │
│                       │ vsock(AF_VSOCK, TLS 1.3)                     │
│  ┌────────────────────▼─────────────────────────────────────────┐   │
│  │  openstack(外部):VM 创建 + 盘预分配                         │   │
│  └────────────────────────────────────────────────────────────────┘   │
│  ┌────────────────────────────────────────────────────────────────┐   │
│  │  虚机(VM)                                                     │   │
│  │  ┌──────────────────────────────────────────────────────────┐ │   │
│  │  │  ubse@VM(本特性交付,同一 SSU 模块,VM 模式)             │ │   │
│  │  │  ├── UbseSsuControllerModule(vm_mode=true)                │ │   │
│  │  │  │   ├── UbseVsockServer(新增:接收 ub-device-manager 推送)│ │   │
│  │  │  │   ├── UbseSsuVmCache(新增:本地缓存)                  │ │   │
│  │  │  │   ├── Query 分配信息(走本地缓存)                      │ │   │
│  │  │  │   ├── Attach/Detach(操作已直通设备)                   │ │   │
│  │  │  │   └── (不启用 alloc/free/FeDevice/election/com/vip)    │ │   │
│  │  │  └── CLI(查询/attach/detach 命令)                       │ │   │
│  │  └──────────────────────────────────────────────────────────┘ │   │
│  │  ┌──────────────────────────────────────────────────────────┐ │   │
│  │  │  已直通的 SSU VFE 设备(via FeDeviceAlloc@ubse@host)     │ │   │
│  │  └──────────────────────────────────────────────────────────┘ │   │
│  └────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

**关键点**:
1. host 与 VM 部署的是**同一份 SSU 模块代码**,通过 `vm_mode` 配置开关决定运行形态,不是两个独立模块
2. ubse@host 与 ubse@VM 之间**没有直接通信通道**,两者解耦
3. 信息流:host 侧 ubse 的分配/连接信息 → ub-device-manager 查询并推送 → VM 内 ubse 缓存 → VM 内应用查询/attach

---

## 3. 总体需求

### 3.1 总体功能需求

| 需求编号 | 需求名称 | 优先级 | 说明 |
|----------|----------|--------|------|
| FR-VM-DEPLOY | VM 模式部署 | Must | SSU 模块支持 vm_mode 配置,VM 模式下裁剪模块依赖,不启用集群选举/RPC/VIP/alloc/free |
| FR-VM-VSOCK-SERVER | VM 内 vsock server | Must | VM 模式下 SSU 模块启动 vsock server,接收 ub-device-manager 推送 |
| FR-VM-QUERY | VM 内查询分配信息 | Must | 查询已推送到 VM 的 SSU 分配信息(走本地缓存) |
| FR-VM-ATTACH | VM 内 attach | Must | 在 VM 内对已直通的 namespace 执行 attach |
| FR-VM-DETACH | VM 内 detach | Must | 在 VM 内对已直通的 namespace 执行 detach |
| FR-VM-CLI | VM 内 CLI 接口 | Must | VM 模式下提供 query/attach/detach CLI 命令子集 |
| FR-VM-LIFECYCLE | VM 生命周期联动 | Should | VM 重启后启动 vsock server,等待 ub-device-manager 主动重连并推送全量分配信息(VM 侧被动接收,不主动拉取) |
| FR-VM-CACHE-PERSIST | VM 缓存本地持久化 | Should | VM 模式下 `UbseSsuVmCache` 将分配信息/连接信息持久化到本地文件,作为重启加速缓存;`attachedNames` 不持久化 |
| FR-BOUNDARY-01 | host 侧 ubse 不感知 VM | Must | ubse@host 不引入对 VM 内 ubse 的任何依赖,不持有 vsock client |
| FR-BOUNDARY-02 | 推送由 ub-device-manager 负责 | Must | host→VM 的信息推送由独立的 ub-device-manager 组件完成,不在 SSU 模块范围内 |
| FR-DEVICE-PASSTHROUGH | 设备直通编排(现有) | Must | 复用现有 FeDeviceAlloc(ubse@host),直通后由 ub-device-manager 推送(本特性不实现推送) |

### 3.2 总体非功能需求

| 需求编号 | 需求类别 | 说明 |
|----------|----------|------|
| NFR-REL-01 | 可靠性 | vsock 连接断开后,VM 内 server 等待 ub-device-manager 重连(ub-device-manager 侧重连策略) |
| NFR-REL-02 | 可靠性 | VM 重启后,ubse@VM 启动 vsock server 等待 ub-device-manager 主动重连并推送全量分配信息(VM 侧不主动发起连接) |
| NFR-REL-03 | 可靠性 | VM 内 ubse 进程异常退出后,systemd 自动拉起 |
| NFR-REL-04 | 可靠性 | 缓存文件掉电损坏时,ubse@VM 启动删除损坏文件并等待 ub-device-manager 全量同步,不阻塞启动 |
| NFR-PERF-03 | 性能 | 缓存文件加载耗时 ≤ 50ms(数据量典型 ≤ 100 条 namespace) |
| NFR-SEC-03 | 安全性 | 缓存文件权限 0600(root 仅读),路径 `/var/lib/ubse/ssu_vm_cache.json`,包含 nqn/devPath 等敏感信息 |
| NFR-PERF-01 | 性能 | VM 内 attach 响应时间 ≤ 500ms |
| NFR-PERF-02 | 性能 | VM 内查询响应时间 ≤ 100ms |
| NFR-SEC-01 | 安全性 | vsock 通道启用 TLS 1.3 双向认证(VM 侧 server 强制校验 ub-device-manager 身份) |
| NFR-SEC-02 | 安全性 | VM 内 ubse 操作需身份验证(复用现有 identity 机制) |
| NFR-OBS-01 | 可观测性 | VM 内 vsock 接收与缓存更新均有审计日志 |
| NFR-OBS-02 | 可观测性 | vsock 连接状态可监控(connected/disconnected/waiting_reconnect) |
| NFR-COMPAT-01 | 兼容性 | 特性开启后不影响 host 模式现有能力(vm_mode=false 时行为不变,host 侧代码零修改) |
| NFR-COMPAT-02 | 兼容性 | VM 模式复用现有 UbseModule 框架与 UbseSsuService 契约 |

### 3.3 总体约束

| 约束编号 | 约束内容 |
|----------|----------|
| CON-OPENSTACK | 盘由 openstack 预分配,VM 内 ubse 不做 alloc/free |
| CON-VSOCK | ub-device-manager↔VM 通信使用 vsock(AF_VSOCK),非 TCP |
| CON-VM-DEPLOY | ubse 部署在虚机内,作为独立进程运行 |
| CON-LIGHTWEIGHT | VM 模式下 ubse 轻量化,不启用 election/com/vip/mem/urma |
| CON-EXISTING-PASSTHROUGH | 设备直通复用现有 FeDeviceAlloc(ubse@host),不重复实现 |
| CON-CERT | vsock TLS 证书体系复用现有 lcne_cert 机制,路径可配置 |
| CON-SAME-MODULE | host 与 VM 部署同一 SSU 模块代码,通过 vm_mode 配置分流,不新增独立模块 |
| CON-UDS-PATH | VM 模式下 ubse 复用 host 模式现有 UDS 路径 `/var/run/ubse/ubse.sock`,不新增独立路径(VM 与 host 部署在不同文件系统命名空间,路径不冲突) |
| CON-HOST-NO-VM-AWARE | ubse@host 不引入对 VM 内 ubse 的依赖,不持有 vsock client,不感知 VM 内 ubse 的存在 |
| CON-PUSH-EXTERNAL | host→VM 的信息推送由 ub-device-manager 负责,SSU 模块不实现推送侧 |
| CON-CACHE-FILE | VM 缓存文件路径 `/var/lib/ubse/ssu_vm_cache.json`,权限 0600;采用原子写(tmp+fsync+rename) |
| CON-CACHE-SCOPE | 仅持久化分配信息与连接信息;`attachedNames` 不持久化(VM 重启后 namespace 必然处于 detached 状态) |
| CON-CACHE-AUTHORITY | 文件非权威源,仅作重启加速;ub-device-manager 全量同步推送后必须覆盖文件 |

---

## 4. 功能需求

### 4.1 VM 模式部署(FR-VM-DEPLOY)

**需求描述**:SSU 模块支持通过配置开关 `vm_mode` 切换部署形态,VM 模式下裁剪模块依赖,仅启用必要子模块。

**功能要求**:
- 在 `ubse.conf [ubse.ssu]` 段新增配置项 `vm_mode`(默认 false)
- `UbseSsuControllerModule::Initialize` 根据 `vm_mode` 分流:
  - `vm_mode=false`(host 模式,默认):现有行为不变,本特性不修改 host 模式任何代码
  - `vm_mode=true`(VM 模式):裁剪依赖,不启动 election/com/vip/collector,启用 vsock server + 本地缓存
- VM 模式下启用的子模块:log、config、context、event、ipc、security、ssu(VM 形态)、vsock_server(新增)
- VM 模式下禁用的子模块:election、com、vip、mem、urma、node、ras、addons、adapter_plugins(除必要)
- VM 模式下 ubse 复用 host 模式现有 UDS 路径 `/var/run/ubse/ubse.sock`(VM 与 host 处于不同文件系统命名空间,路径天然隔离,无需新增独立路径)
- VM 模式下 `UbseGetRole` 返回固定角色(非 master),不参与选举

**验收标准**:
- AC-VM-DEPLOY-01:`vm_mode=true` 时,SSU 模块启动后不加载 election/com/vip
- AC-VM-DEPLOY-02:`vm_mode=true` 时,ubse 监听 `/var/run/ubse/ubse.sock`(复用现有路径)
- AC-VM-DEPLOY-03:`vm_mode=false` 时,SSU 模块行为与特性启用前完全一致(host 侧代码零修改)
- AC-VM-DEPLOY-04:`vm_mode=true` 时,`UbseGetRole` 返回非 master

### 4.2 VM 内 vsock server(FR-VM-VSOCK-SERVER)

**需求描述**:VM 模式下 SSU 模块启动 vsock server,监听 host 侧 ub-device-manager 的 vsock 连接,接收其推送的分配信息与连接信息。

**功能要求**:
- 在 `src/framework/vscok/` 新增 `UbseVsockServer` 类(对称设计,参考现有 `UbseVsockClient`)
- 监听 AF_VSOCK,端口可配置(默认 6174 或新增端口)
- 启用 TLS 1.3 双向认证,证书路径可配置
- 支持长连接(区别于现有 client 的短连接模式),ub-device-manager 连接后保持
- 支持消息接收与响应(基于现有 `MsgHeader` 协议头扩展)
- 接收消息类型(由 ub-device-manager 发送,VM 侧仅接收):
  - `MSG_TYPE_PUSH_ALLOC_INFO`:ub-device-manager 推送分配信息
  - `MSG_TYPE_PUSH_CONNECT_INFO`:ub-device-manager 推送连接信息
  - `MSG_TYPE_QUERY_VM_STATUS`:ub-device-manager 主动查询 VM 内状态(可选)
  - `MSG_TYPE_LIFECYCLE_NOTIFY`:生命周期通知(如 VM 迁移)
  - `MSG_TYPE_FULL_SYNC_RESP`:ub-device-manager 主动全量同步(VM 重启后)
- 收到推送后,更新 VM 内本地缓存(`UbseSsuVmCache`),并响应 ACK
- `UbseSsuControllerModule::Start` 在 VM 模式下启动 vsock server
- VM 侧不主动向 host 发起连接,仅作为 server 等待 ub-device-manager 连接

**验收标准**:
- AC-VM-VSOCK-01:`vm_mode=true` 时,SSU 模块启动后 vsock server 监听指定端口
- AC-VM-VSOCK-02:ub-device-manager 可通过 vsock 连接 VM 内 server,TLS 握手成功
- AC-VM-VSOCK-03:长连接保持,断开后等待 ub-device-manager 侧重连
- AC-VM-VSOCK-04:接收推送后,VM 内本地缓存更新且响应 ACK

### 4.3 VM 内查询分配信息(FR-VM-QUERY)

**需求描述**:VM 模式下 SSU 模块提供查询已直通 SSU namespace 分配信息的能力,数据来源于 ub-device-manager 推送的本地缓存。

**功能要求**:
- 复用 `UbseSsuService` 契约的查询接口(`ListAllocInfo`/`GetAllocInfoByName`/`GetNsStats`/`GetConnectInfo`)
- `UbseSsuServiceImp` 在 VM 模式下,查询走本地缓存(不发起 RPC,不查 host)
- 新增 `UbseSsuVmCache` 子模块(归入 `src/controllers/ssu/`),缓存数据结构:`vector<UbseSsuAllocResult>` + `vector<UbseSsuConnectInfo>`
- 查询接口需身份验证(复用 `UbseSsuAllocIdentityInfo`)
- 缓存为只读,VM 内不可修改分配信息
- 缓存持久化策略详见 4.11

**验收标准**:
- AC-VM-QUERY-01:`ListAllocInfo` 返回 ub-device-manager 已推送到 VM 的所有分配信息
- AC-VM-QUERY-02:`GetAllocInfoByName(name)` 返回指定 name 的分配信息,不存在返回错误码
- AC-VM-QUERY-03:`GetConnectInfo(name, vfe)` 返回指定 namespace 的连接信息
- AC-VM-QUERY-04:未收到推送时,查询返回空列表(不报错)
- AC-VM-QUERY-05:VM 重启后、ub-device-manager 重连前,`ListAllocInfo` 返回上次落盘的分配信息(可能过期但不阻塞查询)

### 4.4 VM 内 attach(FR-VM-ATTACH)

**需求描述**:VM 模式下 SSU 模块提供对已直通 namespace 的 attach 能力,在 VM 内部完成 NVMe namespace 的挂载。

**功能要求**:
- 复用 `UbseSsuService` 契约的 attach 接口(`AttachSpace`/`AttachLinearSpace`/`AttachStripedSpace`)
- `UbseSsuServiceImp` 在 VM 模式下,attach 通过本地 SSU 适配器(VM 内 `libssu.so` 或 virtio 后端)操作已直通的设备
- attach 前需校验 namespace 是否已由 ub-device-manager 推送(本地缓存命中)
- attach 幂等性:重复 attach 同一 namespace 返回成功
- attach 成功后返回 VM 内的设备路径(如 `/dev/nvme0n1`)

**验收标准**:
- AC-VM-ATTACH-01:对已直通的 namespace 执行 attach,返回 VM 内设备路径
- AC-VM-ATTACH-02:重复 attach 同一 namespace,返回成功(幂等)
- AC-VM-ATTACH-03:attach 未在本地缓存中的 namespace,返回错误码 `UBSE_SSU_NS_NOT_FOUND`
- AC-VM-ATTACH-04:attach 线性/条带化聚合空间,返回聚合块设备路径

### 4.5 VM 内 detach(FR-VM-DETACH)

**需求描述**:VM 模式下 SSU 模块提供对已 attach namespace 的 detach 能力。

**功能要求**:
- 复用 `UbseSsuService` 契约的 detach 接口(`DetachSpace`/`DetachLinearSpace`/`DetachStripedSpace`)
- detach 幂等性:重复 detach 返回成功
- detach 后更新本地状态(标记 namespace 为已卸载)

**验收标准**:
- AC-VM-DETACH-01:对已 attach 的 namespace 执行 detach,卸载成功
- AC-VM-DETACH-02:重复 detach,返回成功(幂等)
- AC-VM-DETACH-03:detach 未 attach 的 namespace,返回成功(幂等容错)

### 4.6 VM 内 CLI 接口(FR-VM-CLI)

**需求描述**:VM 模式下 SSU 模块提供 CLI 命令子集,支持运维人员在 VM 内直接查询与操作。

**功能要求**:
- 扩展现有 `src/cli/ssu/ubse_cli_ssu_cmd_reg.{h,cpp}`,根据 `vm_mode` 注册不同命令集:
  - VM 模式注册:`display ssu`(alloc_summary/alloc_detail/connect_info)、`attach ssu`、`detach ssu`
  - VM 模式不注册:`create ssu`、`free ssu`(盘由 openstack 分配)
- CLI 通过 UDS 连接 VM 内 ubse(`/var/run/ubse/ubse.sock`,复用 host 模式路径)
- host 模式下 CLI 命令集不变(向后兼容,host 侧代码零修改)

**验收标准**:
- AC-VM-CLI-01:`vm_mode=true` 时,`display ssu -t alloc_summary` 返回所有已推送的分配信息
- AC-VM-CLI-02:`vm_mode=true` 时,`display ssu -t alloc_detail -n <name>` 返回指定 namespace 详情
- AC-VM-CLI-03:`vm_mode=true` 时,`attach ssu -n <name>` 成功 attach 并返回设备路径
- AC-VM-CLI-04:`vm_mode=true` 时,`detach ssu -n <name>` 成功 detach
- AC-VM-CLI-05:`vm_mode=true` 时,不提供 `create`/`free` 命令
- AC-VM-CLI-06:`vm_mode=false` 时,CLI 命令集与特性启用前一致

### 4.7 组件边界划分(FR-BOUNDARY-01, FR-BOUNDARY-02)

**需求描述**:明确 SSU 模块与 ub-device-manager 组件的职责边界,确保 host 侧 ubse 不引入对 VM 的依赖。

**功能要求**:
- ubse@host 不引入任何与 VM 内 ubse 通信的代码:
  - 不新增 vsock client 类
  - 不新增推送执行器类
  - 不注册推送回调到 `UbseSsuServiceImp`
  - 不感知 VM 内 ubse 的存在与状态
- host→VM 的信息推送完全由独立的 ub-device-manager 组件负责:
  - ub-device-manager 通过现有 SDK/IPC 接口查询 ubse@host 的分配/连接信息
  - ub-device-manager 自带 vsock client,连接 VM 内 ubse@VM 的 vsock server
  - 推送触发时机、推送内容、失败重试策略均由 ub-device-manager 自行决定
- VM 内 ubse 的 vsock server 协议对 ub-device-manager 公开(契约对齐)
- 本特性交付的 SSU 模块代码:
  - VM 侧:新增 vsock server、本地缓存、VM 模式分流
  - host 侧:**零修改**(仅通过 vm_mode=false 走现有分支)

**验收标准**:
- AC-BOUNDARY-01:代码审查确认 ubse@host 不持有 vsock client,不注册推送回调
- AC-BOUNDARY-02:代码审查确认 SSU 模块内无 `UbseSsuVirtHostPusher`、`UbseVsockHostClient` 类
- AC-BOUNDARY-03:`vm_mode=false` 时,SSU 模块的二进制大小、启动行为、内存占用与特性启用前一致
- AC-BOUNDARY-04:ub-device-manager 可通过现有 SDK 接口获取分配/连接信息(无新增接口要求)

### 4.8 VFE 直通(现有能力规约,FR-FE-DEVICE)

> 本节自模块级需求分析文档迁入;FR-DEVICE-PASSTHROUGH 编排依赖本节规约的 FeDeviceAlloc/Free 能力。

**需求描述**:将 SSU 物理功能单元(PFE=6)下的虚拟功能单元(VFE)注册到 VM bus instance,使虚拟机可直接访问物理存储资源,实现零损耗直通。

**输入参数**:

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| upi | uint32 | 是 | 租户隔离标识 |
| vfe | struct | 是 | VFE 信息(slotId/chipId/dieId/pfeId/vfeId/vfeGuid) |
| busInstanceGuid | string | 否 | 目标 VM bus instance GUID,为空则新建 |

**功能规则**:

**FR-FE-DEVICE-01 FeDeviceAlloc 流程**

- 查询 PFE 列表,定位指定 VFE。
- 检查 VFE 是否已被占用:
  - 已被占用且目标 bus instance 相同 → 幂等成功返回。
  - 已被占用但目标 bus instance 不同 → 抢占式反注册原 bus instance(支持 VFE 在 VM 间迁移)。
- 若调用方指定了 `busInstanceGuid`:
  - 复用该 bus instance,并强制反注册其上的其他 VFE(独占语义)。
- 若未指定:
  - 新建 VM bus instance。
- 将 VFE 注册到目标 bus instance。
- 所有 MTI 调用支持 5 次重试,每次间隔 2 秒。

**FR-FE-DEVICE-02 FeDeviceFree 流程**

- 校验 bus instance 的 upi 与请求一致(防跨 VM 误操作)。
- 反注册 VFE。
- 若 bus instance 上无其他 VFE,销毁空实例;否则保留并告警。

**FR-FE-DEVICE-03 并发控制**

- 同一时刻只允许一个 FeDeviceAlloc/FeDeviceFree/后台清理任务运行(单工作者模型)。
- 状态机:`AVAILABLE → RUNNING_ALLOC/RUNNING_FREE/CLEARING_BG → AVAILABLE`。

**FR-FE-DEVICE-04 空实例清理**

- 周期性(60s,常量 `CLEAR_VM_BUS_INST_INTERVAL_SECONDS = 60`)清理空的 VM bus instance,回收 VM 异常退出后的残留资源。
- 清理任务与 alloc/free 互斥:运行中则跳过本次清理。

**验收标准**:

| 编号 | 验收项 |
|------|--------|
| AC-FE-01 | VFE 直通后,VM 内可看到对应的 NVMe 设备 |
| AC-FE-02 | 同一 VFE 重复直通到同一 bus instance 返回成功(幂等) |
| AC-FE-03 | VFE 从 VM A 迁移到 VM B 后,VM A 不再可见,VM B 可见 |
| AC-FE-04 | upi 不一致时返回错误 |
| AC-FE-05 | bus instance 上其他 VFE 被强制反注册后,原 VM 不再可见 |
| AC-FE-06 | 空的 bus instance 在 60s 内被自动清理 |
| AC-FE-07 | 并发 FeDeviceAlloc 时串行执行,无 MTI 调用冲突 |

### 4.9 设备直通编排(FR-DEVICE-PASSTHROUGH)

**需求描述**:复用现有 `UbseSsuDirectToVmManager.FeDeviceAlloc`(ubse@host)完成 SSU 设备到 VM 的直通,直通后的信息推送由 ub-device-manager 自行编排。

**功能要求**:
- 设备直通调用现有 `FeDeviceAlloc(upi, vfe, busInstanceGuid)` 接口(ubse@host,不修改)
- 直通成功后,ubse@host 不触发任何推送(推送由 ub-device-manager 自行感知或通过其他机制)
- 编排顺序:
  1. openstack 分配盘 → ubse@host 的 AllocSpace(可选,现有能力)
  2. ubse@host 的 FeDeviceAlloc 直通 VFE 到 VM(现有能力)
  3. ub-device-manager 感知直通完成(机制由 ub-device-manager 自行设计,不在本特性范围)
  4. ub-device-manager 通过 SDK 查询 ubse@host 的分配/连接信息
  5. ub-device-manager 通过 vsock 推送到 VM 内 ubse@VM
  6. VM 内 ubse 更新本地缓存

**验收标准**:
- AC-DEVICE-PASSTHROUGH-01:`FeDeviceAlloc` 现有行为不变,本特性不修改其代码
- AC-DEVICE-PASSTHROUGH-02:VM 内 ubse 收到 ub-device-manager 推送后,可执行 attach
- AC-DEVICE-PASSTHROUGH-03:本特性交付的 SSU 模块代码不包含任何推送触发逻辑

### 4.10 VM 生命周期联动(FR-VM-LIFECYCLE)

**需求描述**:VM 销毁/重启/迁移时,VM 内 ubse 与 ub-device-manager 协同完成状态恢复。

**功能要求**:
- VM 重启后,ubse@VM 启动 vsock server,等待 ub-device-manager 重连
- ub-device-manager 重连成功后,主动全量同步分配信息到 VM(由 ub-device-manager 触发)
- VM 内 ubse 收到全量同步后,重建 `UbseSsuVmCache`
- VM 销毁时,vsock 连接断开,VM 内 ubse 进程退出(随 VM 关机)
- VM 迁移时(配合 virt_agent),迁移前 VM 内 ubse detach 所有 namespace(可选),迁移后由 ub-device-manager 重新推送

**验收标准**:
- AC-VM-LIFECYCLE-01:VM 重启后,ubse@VM 启动并等待 ub-device-manager 重连
- AC-VM-LIFECYCLE-02:ub-device-manager 重连并全量同步后,VM 内缓存重建,查询数据一致
- AC-VM-LIFECYCLE-03:VM 迁移后,ub-device-manager 重新推送,新 VM 内 ubse 可正常查询/attach

### 4.11 VM 缓存本地持久化(FR-VM-CACHE-PERSIST)

**需求描述**:VM 模式下 `UbseSsuVmCache` 将分配信息与连接信息持久化到本地文件,作为 VM 重启后的加速缓存,使 ubse@VM 在 ub-device-manager 重连前即可提供查询服务。

**功能要求**:
- 持久化对象:**仅** `allocInfoMap_`(分配信息)与 `connectInfoMap_`(连接信息)
- **不持久化** `attachedNames_`(VM 重启后 namespace 必然处于 detached 状态,文件状态会失真)
- 文件路径:`/var/lib/ubse/ssu_vm_cache.json`(可配置),权限 `0600`
- 文件格式:JSON,含 `version`、`lastSyncTs`、`allocList`、`connectList` 四个字段
- 写入时机:
  - ub-device-manager 推送分配/连接信息后,内存更新成功 → 异步触发文件落盘
  - 全量同步(`MSG_TYPE_FULL_SYNC_RESP`)完成后,覆盖写文件
- 加载时机:`UbseSsuControllerModule::Initialize`(VM 模式分支)启动时加载
- 原子写策略:写 `ssu_vm_cache.json.tmp` → `fsync` → `rename` 为 `ssu_vm_cache.json`,避免掉电损坏
- 损坏恢复:加载时 JSON 解析失败或字段缺失,删除损坏文件,记录告警日志,等 ub-device-manager 全量同步
- 权威性:文件**非权威源**,仅作重启加速;ub-device-manager 全量同步推送后必须以推送数据覆盖文件
- 过期容忍:文件可能过期(host 侧已 free 某 namespace 但 VM 文件仍保留),ub-device-manager 全量同步后会覆盖修正;过期期间查询返回的"幽灵 namespace"在 attach 时会因设备不存在而失败,不影响系统一致性

**验收标准**:
- AC-VM-CACHE-PERSIST-01:ub-device-manager 推送分配信息后,`/var/lib/ubse/ssu_vm_cache.json` 文件内容与内存一致
- AC-VM-CACHE-PERSIST-02:VM 重启后、ub-device-manager 重连前,`ListAllocInfo` 返回文件中的分配信息(不阻塞)
- AC-VM-CACHE-PERSIST-03:VM 重启后,`attachedNames` 为空(未持久化),需重新 attach
- AC-VM-CACHE-PERSIST-04:手动破坏缓存文件(截断/乱码),ubse@VM 启动删除文件并继续启动,不崩溃
- AC-VM-CACHE-PERSIST-05:ub-device-manager 全量同步后,文件被覆盖写,内容与推送一致
- AC-VM-CACHE-PERSIST-06:缓存文件权限为 0600,属主 root

---

## 5. 非功能需求

### 5.1 可靠性(NFR-REL)

| 需求编号 | 需求描述 | 验收方法 |
|----------|----------|----------|
| NFR-REL-01 | vsock 连接断开后,VM 内 server 等待 ub-device-manager 重连(不主动重连) | 模拟断连,观察 server 监听状态 |
| NFR-REL-02 | VM 重启后,ubse@VM 启动并等待 ub-device-manager 全量同步 | VM 重启后查询,数据一致 |
| NFR-REL-03 | VM 内 ubse 进程异常退出后,systemd 自动拉起 | 杀进程,观察 systemd 拉起 |
| NFR-REL-04 | vsock 通信采用 TLS 1.3,证书过期前 30 天告警 | 证书即将过期时检查告警 |
| NFR-REL-05 | VFE 直通相关 MTI 调用支持 5 次重试,间隔 2 秒(自模块文档迁入,原模块文档 NFR-REL-06) | MTI 调用失败后检查重试次数与间隔 |
| NFR-REL-06 | 周期性(60s,常量 `CLEAR_VM_BUS_INST_INTERVAL_SECONDS`)清理空的 VM bus instance,回收 VM 异常退出后的残留资源(自模块文档迁入,原模块文档 NFR-REL-07) | 观察 VM 异常退出后空 bus instance 在 60s 周期内被清理 |

### 5.2 性能(NFR-PERF)

| 需求编号 | 需求描述 | 验收方法 |
|----------|----------|----------|
| NFR-PERF-01 | VM 内 attach 响应时间 ≤ 500ms | VM 内执行 attach,测量响应时间 |
| NFR-PERF-02 | VM 内查询响应时间 ≤ 100ms | VM 内执行查询,测量响应时间 |
| NFR-PERF-03 | VM 内 vsock 接收吞吐 ≥ 1000 条/秒 | 批量推送测试(由 ub-device-manager 测端主导) |

### 5.3 安全性(NFR-SEC)

| 需求编号 | 需求描述 | 验收方法 |
|----------|----------|----------|
| NFR-SEC-01 | vsock 通道启用 TLS 1.3 双向认证(VM 侧 server 强制校验) | 抓包验证 TLS 握手 |
| NFR-SEC-02 | VM 内 ubse 操作需身份验证 | 无身份调用返回鉴权失败 |
| NFR-SEC-03 | VM 侧 server 验证 ub-device-manager 身份(证书 CN/OU) | 证书不匹配时连接拒绝 |
| NFR-SEC-04 | FeDeviceFree 时校验 bus instance 的 upi 与请求一致,防跨 VM 误操作(自模块文档迁入,原模块文档 NFR-SEC-04) | upi 不一致的 FeDeviceFree 请求返回错误 |

### 5.4 可观测性(NFR-OBS)

| 需求编号 | 需求描述 | 验收方法 |
|----------|----------|----------|
| NFR-OBS-01 | VM 内 vsock 接收与缓存更新均有审计日志 | 检查审计日志记录 |
| NFR-OBS-02 | vsock 连接状态可监控(VM 侧 server 视角) | 查询连接状态接口 |
| NFR-OBS-03 | 缓存为空、全量同步完成等关键事件有告警/通知 | 检查告警 |

### 5.5 兼容性(NFR-COMPAT)

| 需求编号 | 需求描述 | 验收方法 |
|----------|----------|----------|
| NFR-COMPAT-01 | 特性启用后不影响 host 模式现有能力(`vm_mode=false` 时行为不变,host 侧代码零修改) | 启用特性后,host 模式 SSU 功能全部正常 |
| NFR-COMPAT-02 | VM 模式复用现有 UbseModule 框架与 UbseSsuService 契约 | 代码审查确认复用,无新增契约 |
| NFR-COMPAT-03 | 支持 openEuler Linux ARM64(主目标平台) | ARM64 环境验证 |

---

## 6. 约束条件

| 约束编号 | 约束内容 | 依据 |
|----------|----------|------|
| CON-OPENSTACK | 盘由 openstack 预分配,VM 内 ubse 不做 alloc/free | 业务约束:云编排由 openstack 负责 |
| CON-VSOCK | ub-device-manager↔VM 通信使用 vsock(AF_VSOCK) | 技术约束:vsock 是 host↔VM 高效通道 |
| CON-VM-DEPLOY | ubse 部署在虚机内,作为独立进程运行 | 业务约束:VM 内自治运维 |
| CON-LIGHTWEIGHT | VM 模式下 ubse 轻量化,不启用 election/com/vip/mem/urma | 技术约束:VM 内不需要集群能力 |
| CON-EXISTING-PASSTHROUGH | 设备直通复用现有 FeDeviceAlloc(ubse@host) | 技术约束:避免重复造轮子 |
| CON-CERT | vsock TLS 证书体系复用现有 lcne_cert 机制 | 技术约束:证书体系统一管理 |
| CON-SAME-MODULE | host 与 VM 部署同一 SSU 模块代码,通过 vm_mode 配置分流 | 架构约束:保持模块内聚 |
| CON-UDS-PATH | VM 模式下 ubse 复用 host 模式现有 UDS 路径 `/var/run/ubse/ubse.sock` | 部署约束:VM 与 host 文件系统命名空间隔离,路径不冲突,保持 SDK/CLI 默认连接路径一致 |
| CON-ROLE | VM 模式下 ubse 不参与选举,角色固定为非 master | 技术约束:VM 内无主备概念 |
| CON-HOST-NO-VM-AWARE | ubse@host 不引入对 VM 内 ubse 的依赖,不持有 vsock client | 架构约束:组件职责严格分离 |
| CON-PUSH-EXTERNAL | host→VM 的信息推送由 ub-device-manager 负责,SSU 模块不实现推送侧 | 架构约束:推送逻辑外置到独立组件 |
| CON-CACHE-FILE | 缓存文件 `/var/lib/ubse/ssu_vm_cache.json`,权限 0600,原子写 | 部署约束:含敏感信息,需文件权限保护 |
| CON-CACHE-SCOPE | 仅持久化分配/连接信息,`attachedNames` 不持久化 | 语义约束:VM 重启后 attach 状态必然失效 |
| CON-CACHE-AUTHORITY | 文件非权威源,ub-device-manager 全量同步后覆盖 | 一致性约束:推送数据为权威源 |

---

## 7. 场景分析

### 7.1 场景一:首次部署与设备直通

**触发条件**:openstack 创建 VM,需要将 SSU 设备直通到 VM。

**前置条件**:
- host 侧 ubse@host 运行中,SSU 模块 `vm_mode=false`(本特性不修改)
- host 侧 ub-device-manager 已部署并运行(独立组件)
- openstack 已完成 VM 创建
- VM 内已部署 ubse@VM,`vm_mode=true`,vsock server 已启动

**主流程**:
1. openstack 调用 ubse@host 的 `AllocSpace`(或盘已预分配,跳过)
2. openstack 调用 ubse@host 的 `FeDeviceAlloc(upi, vfe, busInstanceGuid)` 直通 VFE 到 VM
3. ub-device-manager 感知直通完成(机制由 ub-device-manager 自行设计,不在本特性范围)
4. ub-device-manager 通过 SDK/IPC 查询 ubse@host 的分配信息与连接信息
5. ub-device-manager 通过 vsock 向 VM 内 ubse@VM 推送分配信息与连接信息
6. VM 内 `UbseVsockServer` 接收推送,更新 `UbseSsuVmCache`,响应 ACK
7. VM 内应用通过 CLI 或 SDK 调用 `attach ssu -n <name>` 挂载 namespace
8. VM 内应用使用已挂载的 SSU 设备

**异常分支**:
- 步骤 2 FeDeviceAlloc 失败:ubse@host 返回错误码,ub-device-manager 不推送
- 步骤 5 推送失败:由 ub-device-manager 侧重试策略处理(不在本特性范围)
- 步骤 7 attach 失败:VM 内 ubse 返回错误码,应用重试

**本特性交付范围**:步骤 6-7(VM 侧接收与 attach),其余步骤为外部组件协作

### 7.2 场景二:VFE 直通占用/抢占/独占(host 侧现有能力,自模块文档迁入)

**触发条件**:hypervisor 通过 SDK 调用 ubse@host 的 `FeDeviceAlloc`,将 VFE 直通到指定 VM(场景一步骤 2 的语义细节)。

**主流程**:
1. hypervisor 调用 `FeDeviceAlloc(upi=100, vfe={slotId=0,chipId=0,dieId=0,pfeId=6,vfeId=1}, busInstanceGuid="")`
2. 系统查询 PFE 列表,定位 VFE
3. 系统检查 VFE 未被占用
4. 系统新建 VM bus instance,返回 GUID
5. 系统将 VFE 注册到 bus instance
6. 返回 busInstanceGuid(后续推送链路见场景一)

**语义细节**(对应 4.8 现有能力规约):
- **占用与抢占**:VFE 已被占用且目标 bus instance 不同时,抢占式反注册原 bus instance(支持 VFE 在 VM 间迁移,迁移后原 VM 不再可见)
- **独占**:调用方指定 `busInstanceGuid` 时,复用该实例并强制反注册其上其他 VFE
- **幂等**:同一 VFE 重复直通到同一 bus instance,直接返回成功
- **upi 校验**:`FeDeviceFree` 校验 bus instance 的 upi 与请求一致,防跨 VM 误操作

**后置条件**:VM 通过 bus instance 看到 VFE,可作为 NVMe 子系统使用

**异常分支**:
- MTI 调用失败:5 次重试(间隔 2 秒)后仍失败返回错误
- 并发请求:排队等待 `DirectToVmManager` 状态机 AVAILABLE(单工作者模型)

### 7.3 场景三:VM 内查询分配信息

**触发条件**:VM 内应用需要查询已直通的 SSU namespace 信息。

**主流程**:
1. VM 内应用通过 SDK 调用 `ubs_ssu_alloc_info_list()`
2. SDK 通过 UDS 连接 VM 内 ubse(`/var/run/ubse/ubse.sock`,复用 host 模式路径)
3. VM 内 `UbseSsuServiceImp.ListAllocInfo`(VM 模式分支)从 `UbseSsuVmCache` 返回
4. SDK 返回分配信息列表

**性能要求**:≤ 100ms

### 7.4 场景四:VM 内 attach/detach

**触发条件**:VM 内应用需要挂载或卸载 SSU namespace。

**主流程(attach)**:
1. VM 内应用通过 CLI `attach ssu -n <name>` 或 SDK 调用
2. VM 内 `UbseSsuServiceImp.AttachSpace`(VM 模式分支)校验 namespace 在 `UbseSsuVmCache` 中
3. VM 内 ubse 调用本地 SSU 适配器执行 attach
4. 返回 VM 内设备路径(如 `/dev/nvme0n1`)

**主流程(detach)**:
1. VM 内应用通过 CLI `detach ssu -n <name>` 或 SDK 调用
2. VM 内 ubse 调用本地 SSU 适配器执行 detach
3. 更新本地状态

### 7.5 场景五:VM 重启恢复

**触发条件**:VM 异常重启或计划内重启。

**主流程**:
1. VM 重启,vsock 连接断开
2. ub-device-manager 检测到断连(由 ub-device-manager 自行处理,不在本特性范围)
3. VM 内 ubse@VM 随 VM 启动,`vm_mode=true`,vsock server 监听
4. ub-device-manager 重连 VM 内 server
5. ub-device-manager 主动向 VM 推送全量分配信息(`MSG_TYPE_FULL_SYNC_RESP`)
6. VM 内 `UbseSsuVmCache` 重建
7. VM 内应用可正常查询/attach

**本特性交付范围**:步骤 3、6、7(VM 侧启动、缓存重建、查询恢复)

### 7.6 场景六:VM 销毁清理

**触发条件**:openstack 销毁 VM。

**主流程**:
1. VM 销毁,vsock 连接断开
2. VM 内 ubse 进程随 VM 关机退出
3. ub-device-manager 检测到断连,记录告警(由 ub-device-manager 处理)
4. ub-device-manager(可选)调用 ubse@host 的 `FeDeviceFree` 清理该 VM 的 VFE 绑定

**本特性交付范围**:无(VM 侧仅随 VM 关机退出)

### 7.7 场景七:VM 热迁移

**触发条件**:virt_agent 触发 VM 热迁移。

**主流程**:
1. virt_agent 发起 VM 热迁移
2. 迁移前:VM 内 ubse detach 所有 namespace(可选)
3. 迁移中:VM 状态迁移到目标 host
4. 迁移后:目标 host 侧 ub-device-manager 重新推送分配/连接信息到新 VM
5. 新 VM 内 ubse 重建本地缓存
6. VM 内应用重新 attach(若迁移前 detach)

**本特性交付范围**:步骤 5-6(新 VM 内缓存重建与 attach)

---

## 8. 数据模型

### 8.1 新增数据结构

```cpp
// vsock 消息类型枚举(扩展 MsgHeader.type)
// 注意:这些消息由 ub-device-manager 发送,VM 侧仅接收与响应
enum class UbseVsockMsgType : uint32_t {
    REQ_TYPE_SIGN = 0x0010,                    // 签名请求(现有,保留)
    REQ_TYPE_VERIFY_AND_SIGN = 0x0012,         // 验签+重签(现有,保留)
    MSG_TYPE_PUSH_ALLOC_INFO = 0x0100,         // ub-device-manager 推送分配信息(VM 接收)
    MSG_TYPE_PUSH_CONNECT_INFO = 0x0101,       // ub-device-manager 推送连接信息(VM 接收)
    MSG_TYPE_QUERY_VM_STATUS = 0x0102,         // ub-device-manager 查询 VM 内状态(VM 接收,可选)
    MSG_TYPE_LIFECYCLE_NOTIFY = 0x0103,        // 生命周期通知(VM 接收)
    MSG_TYPE_FULL_SYNC_RESP = 0x0105,          // ub-device-manager 全量同步(VM 接收)
    MSG_TYPE_ACK = 0x01FF,                     // 通用 ACK(VM 响应)
};

// 推送分配信息消息体(ub-device-manager → VM)
struct UbseVsockPushAllocInfoMsg {
    uint32_t upi;                              // 租户标识
    UbseSsuAllocResult allocResult;            // 分配结果(name/size/nqn/devPath/nsGuid)
};

// 推送连接信息消息体(ub-device-manager → VM)
struct UbseVsockPushConnectInfoMsg {
    uint32_t upi;                              // 租户标识
    std::string name;                          // namespace 名称
    std::vector<UbseSsuConnectInfo> connectInfoList;  // 连接信息列表
};

// 生命周期通知消息体(ub-device-manager → VM)
struct UbseVsockLifecycleNotifyMsg {
    uint32_t upi;                              // 租户标识
    uint8_t eventType;                         // 0=迁移开始 1=迁移结束 2=VM 销毁
    std::string busInstanceGuid;               // VM 标识
};

// SSU 模块虚机化特性配置(扩展 ubse.conf [ubse.ssu])
struct UbseSsuVirtConfig {
    bool vmMode{false};                        // 是否 VM 模式
    uint32_t vsockServerPort{6174};            // vsock server 端口(VM 模式)
    std::string vsockCertDir{"/var/lib/ubse/lcne_cert/"};  // vsock 证书目录
    std::string udsPath{"/var/run/ubse/ubse.sock"};         // UDS 路径(VM/host 模式复用,命名空间隔离)
    std::string cacheFilePath{"/var/lib/ubse/ssu_vm_cache.json"};  // 缓存文件路径(VM 模式)
    bool cachePersistEnabled{true};            // 是否启用缓存持久化(VM 模式,默认开)
};
```

### 8.2 复用现有数据结构

| 数据结构 | 来源 | 复用方式 |
|----------|------|----------|
| `UbseSsuAllocResult` | `ubse_ssu_service.h` | VM 内缓存分配信息 |
| `UbseSsuConnectInfo` | `ubse_ssu_service.h` | VM 内缓存连接信息 |
| `UbseSsuVfe` | `ubse_ssu_service.h` | 设备直通标识 |
| `UbseSsuSpaceReq` | `ubse_ssu_service.h` | VM 内 attach/detach 请求 |
| `UbseSsuAllocIdentityInfo` | `ubse_ssu_service.h` | 身份验证 |
| `MsgHeader` | `ubse_vsock_client.h` | vsock 通信协议头 |

### 8.3 VM 内本地缓存模型

```cpp
// VM 模式下 SSU 模块的本地缓存(只读,由 ub-device-manager 推送更新)
struct UbseSsuVmCache {
    std::shared_mutex rwLock;
    // 分配信息缓存:name → AllocResult
    std::unordered_map<std::string, UbseSsuAllocResult> allocInfoMap;
    // 连接信息缓存:name → ConnectInfo 列表
    std::unordered_map<std::string, std::vector<UbseSsuConnectInfo>> connectInfoMap;
    // 已 attach 的 namespace 集合
    std::unordered_set<std::string> attachedNames;
};
```

---

## 9. 接口清单

### 9.1 SSU 模块新增内部接口(VM 模式)

| 接口 | 归属 | 功能 |
|------|------|------|
| `UbseVsockServer.Start(port, certDir)` | `src/framework/vscok/` | 启动 vsock server,等待 ub-device-manager 连接 |
| `UbseVsockServer.Stop()` | `src/framework/vscok/` | 停止 server |
| `UbseVsockServer.GetConnectionStatus()` | `src/framework/vscok/` | 查询当前连接状态(VM 侧视角) |
| `UbseSsuVmCache.UpdateAllocInfo(allocResult)` | `src/controllers/ssu/` | 更新分配信息缓存 |
| `UbseSsuVmCache.UpdateConnectInfo(name, list)` | `src/controllers/ssu/` | 更新连接信息缓存 |
| `UbseSsuVmCache.GetAllocInfo(name)` | `src/controllers/ssu/` | 查询分配信息 |
| `UbseSsuVmCache.MarkAttached(name)` / `MarkDetached(name)` | `src/controllers/ssu/` | 标记 attach/detach 状态 |

### 9.2 复用现有服务契约(VM 模式下走本地分支)

VM 模式下 `UbseSsuServiceImp` 实现以下接口(走本地缓存或本地适配器),不支持的接口返回 `UBSE_SSU_NOT_SUPPORTED`:

| 接口 | VM 模式实现 | 说明 |
|------|-------------|------|
| `ListAllocInfo` | ✅ 走 `UbseSsuVmCache` | 返回 ub-device-manager 推送的分配信息 |
| `GetAllocInfoByName` | ✅ 走 `UbseSsuVmCache` | 按名查询 |
| `GetNsStats` | ✅ 走本地适配器 | 查询 namespace 统计 |
| `GetConnectInfo` | ✅ 走 `UbseSsuVmCache` | 返回连接信息 |
| `AttachSpace` / `AttachLinearSpace` / `AttachStripedSpace` | ✅ 走本地适配器 | VM 内 attach |
| `DetachSpace` / `DetachLinearSpace` / `DetachStripedSpace` | ✅ 走本地适配器 | VM 内 detach |
| `AllocSpace` / `FreeSpace` | ❌ NOT_SUPPORTED | 盘由 openstack 分配 |
| `AddAccessPermission` / `RemoveAccessPermission` | ❌ NOT_SUPPORTED | 权限由 host 管理 |
| `GetFeDeviceList` / `FeDeviceAlloc` / `FeDeviceFree` | ❌ NOT_SUPPORTED | VFE 由 host 管理 |

host 侧(ubse@host)通过内部接口提供 VFE 直通能力(自模块文档迁入,原模块文档 IF-16/IF-17):

| 接口名 | 功能 | 输入 | 输出 | 优先级 |
|--------|------|------|------|--------|
| IF-16 `FeDeviceAlloc` | 将 VFE 注册到 VM bus instance | upi、vfe(slotId/chipId/dieId/pfeId/vfeId/vfeGuid)、busInstanceGuid(可选) | busInstanceGuid | Must |
| IF-17 `FeDeviceFree` | 将 VFE 从 VM bus instance 反注册 | upi、vfe | - | Must |

- IF-16 幂等语义:VFE 已注册到**相同** bus instance 时幂等成功;注册到**不同** bus instance 时抢占式反注册原实例(支持 VM 间迁移);指定 `busInstanceGuid` 时独占(强制反注册其上其他 VFE)。
- IF-17 前置校验:bus instance 的 upi 必须与请求一致(防跨 VM 误操作);反注册后空实例销毁。

### 9.3 CLI 命令清单(VM 模式)

| 命令 | 参数 | 功能 |
|------|------|------|
| `display ssu -t alloc_summary` | 无 | 查询所有分配信息摘要 |
| `display ssu -t alloc_detail -n <name>` | `-n` | 查询单个 namespace 详情 |
| `display ssu -t connect_info -n <name>` | `-n` | 查询连接信息 |
| `attach ssu -n <name> [-t Linear\|Striped]` | `-n`,可选 `-t` | attach namespace |
| `detach ssu -n <name> [-t Linear\|Striped]` | `-n`,可选 `-t` | detach namespace |

> host 模式下 CLI 命令集不变(`create ssu`、`display ssu`、`attach ssu`、`detach ssu` 均可用)。

> 已知缺口(自模块文档迁入):host 模式暂缺 `display fe`(0x0012)/`create fe`(0x0013)/`free fe`(0x0014) 三条 fe device 命令,VFE 直通操作目前仅能通过内部接口(IF-16/IF-17)完成,建议后续补充对应 CLI 命令。

### 9.4 与 ub-device-manager 的契约(vsock 协议)

VM 侧 vsock server 对 ub-device-manager 公开以下契约(ub-device-manager 需遵循):

| 项 | 说明 |
|----|------|
| 传输层 | AF_VSOCK + TLS 1.3 双向认证 |
| 监听端口 | 可配置(默认 6174),由 VM 侧 `ubse.conf [ubse.ssu] vsockServerPort` 指定 |
| 协议头 | 复用 `MsgHeader{id, version, type, len, data[0]}` |
| 消息类型 | 见 8.1 `UbseVsockMsgType` 枚举 |
| 消息体 | 见 8.1 各消息体结构(二进制序列化,格式待澄清 TBD-VSOCK-PROTO) |
| 响应 | VM 侧收到消息后响应 `MSG_TYPE_ACK`,ACK 携带原始消息 id 与错误码 |
| 连接模式 | 长连接,ub-device-manager 连接后保持,断开后由 ub-device-manager 侧重连 |

---

## 10. 风险与对策

| 风险编号 | 风险描述 | 影响 | 概率 | 对策 |
|----------|----------|------|------|------|
| RISK-01 | vsock 连接不稳定,频繁断连 | VM 内缓存过期 | 中 | VM 侧 server 保持监听,等待 ub-device-manager 重连 + 全量同步机制(由 ub-device-manager 主导) |
| RISK-02 | VM 内 ubse 与 ub-device-manager 状态不一致 | VM 内查询到过期数据 | 中 | ub-device-manager 定期全量同步 + 版本号校验(由 ub-device-manager 主导) |
| RISK-03 | vsock TLS 证书过期 | 通信中断 | 低 | 证书过期前 30 天告警,支持热更新 |
| RISK-04 | VM 内 ubse 进程异常退出 | VM 内无法管理 SSU | 中 | systemd 拉起 + 健康检查 |
| RISK-05 | ub-device-manager 推送吞吐不足 | 大规模 VM 场景推送延迟 | 低 | 由 ub-device-manager 侧优化(批量推送 + 异步化,不在本特性范围) |
| RISK-06 | VM 迁移后 VFE 绑定关系丢失 | 迁移后 VM 无法访问 SSU | 中 | 迁移后 ub-device-manager 重新推送 + ubse@host 重新 FeDeviceAlloc |
| RISK-07 | vm_mode 分流逻辑不彻底 | 模式串扰,启动失败 | 中 | 严格的模式判定 + 启动校验 |
| RISK-08 | openstack 与 ubse@host 编排冲突 | 重复分配或直通失败 | 低 | 明确职责边界,openstack 只负责 VM 创建,ubse@host 负责 SSU 编排 |
| RISK-09 | host 与 VM 部署同一份代码但行为差异大 | 维护成本高 | 中 | 通过配置开关分流,共享核心逻辑,差异点集中管理 |
| RISK-10 | ub-device-manager 与 VM 侧 vsock 协议不匹配 | 推送失败或解析错误 | 中 | vsock 协议契约公开,版本兼容性由 ub-device-manager 与 VM 侧共同保证 |
| RISK-11 | ub-device-manager 查询 ubse@host 接口与现有 SDK 契约不匹配 | 无法获取分配信息 | 低 | ubse@host 现有 SDK 接口稳定,ub-device-manager 复用现有契约 |
| RISK-12 | MTI 调用瞬时失败(自模块文档迁入,原模块文档 RISK-06) | VFE 直通失败 | 中 | 5 次重试,间隔 2 秒(见 4.8 FR-FE-DEVICE-01) |
| RISK-13 | VM 异常退出导致空 bus instance 残留(自模块文档迁入,原模块文档 RISK-05) | 资源配额耗尽,后续直通失败 | 中 | 60 秒周期清理空 bus instance(见 4.8 FR-FE-DEVICE-04) |
| RISK-14 | DirectToVm 单工作者模型成为瓶颈(自模块文档迁入,原模块文档 RISK-10) | 高并发 VFE 直通时排队等待 | 低 | 状态机+条件变量实现,业务逻辑无锁,仅状态切换持锁;如成为瓶颈可扩展为多工作者+分片状态机 |

---

## 11. 验收标准汇总

### 11.1 功能验收

| 验收编号 | 验收项 | 验收方法 |
|----------|--------|----------|
| AC-VM-DEPLOY-01 | `vm_mode=true` 时模块清单正确 | 启动 VM 内 ubse,检查日志模块加载清单 |
| AC-VM-DEPLOY-02 | `vm_mode=true` 时复用 host 模式 UDS 路径 | 检查 `/var/run/ubse/ubse.sock` 存在(VM 内文件系统命名空间) |
| AC-VM-DEPLOY-03 | `vm_mode=false` 时行为不变(host 侧代码零修改) | host 模式 SSU 功能全部正常,二进制大小一致 |
| AC-VM-DEPLOY-04 | `vm_mode=true` 时不参与选举 | `UbseGetRole` 返回非 master |
| AC-VM-VSOCK-01~04 | vsock server 验证 | 启动/ub-device-manager 连接/长连接保持/缓存更新 |
| AC-VM-QUERY-01~05 | VM 内查询验证 | 推送后查询,数据一致;重启后未重连前返回文件缓存 |
| AC-VM-ATTACH-01~04 | VM 内 attach 验证 | attach/幂等/未缓存报错/聚合 |
| AC-VM-DETACH-01~03 | VM 内 detach 验证 | detach/幂等/容错 |
| AC-VM-CLI-01~06 | CLI 命令验证 | 各 CLI 命令 + host 模式向后兼容 |
| AC-VM-CACHE-PERSIST-01~06 | 缓存持久化验证 | 推送后落盘/重启加载/attachedNames 不持久化/损坏恢复/全量覆盖/权限 0600 |
| AC-BOUNDARY-01~04 | 组件边界验证 | 代码审查确认 host 侧无 VM 感知代码,无推送类 |
| AC-DEVICE-PASSTHROUGH-01~03 | 设备直通编排验证 | FeDeviceAlloc 现有行为不变,本特性不含推送触发逻辑 |
| AC-FE-01~07 | VFE 直通现有能力验证(自模块文档迁入) | 直通可见/幂等/抢占迁移/upi 校验/独占反注册/空实例清理/并发串行 |
| AC-VM-LIFECYCLE-01~03 | 生命周期联动验证 | VM 重启/销毁/迁移后的 VM 侧行为 |

### 11.2 非功能验收

| 验收编号 | 验收项 | 验收方法 |
|----------|--------|----------|
| AC-REL-01 | vsock 断连后 VM 侧 server 保持监听 | 模拟断连,检查 server 状态 |
| AC-REL-02 | VM 重启后全量同步 | VM 重启,查询数据一致 |
| AC-REL-03 | VM 内 ubse 进程 systemd 拉起 | 杀进程,观察拉起 |
| AC-PERF-01 | attach 响应 ≤ 500ms | 计时 |
| AC-PERF-02 | 查询响应 ≤ 100ms | 计时 |
| AC-SEC-01 | TLS 1.3 双向认证 | 抓包验证 |
| AC-SEC-02 | 身份验证 | 无身份调用拒绝 |
| AC-OBS-01 | 审计日志 | 检查日志 |
| AC-OBS-02 | 连接状态监控 | 查询状态 |
| AC-COMPAT-01 | host 模式向后兼容 | `vm_mode=false` 时所有现有功能正常,代码零修改 |

---

## 12. 附录

### 12.1 待澄清项(TBD)

| TBD 编号 | 描述 | 影响范围 |
|----------|------|----------|
| TBD-VSOCK-PROTO | vsock 消息序列化格式(JSON/二进制) | 通信层(ub-device-manager 与 VM 侧契约) |
| TBD-VM-SSU-ADAPTER | VM 内 SSU 适配器后端(libssu.so/virtio/PCIe) | VM 内 attach 实现 |
| TBD-DEVICE-MGR-PERCEPTION | ub-device-manager 如何感知 FeDeviceAlloc 完成(轮询/事件/回调) | ub-device-manager 设计(不在本特性范围) |
| TBD-VFE-CLEANUP-ON-VM-DESTROY | VM 销毁时是否由 ub-device-manager 触发 FeDeviceFree | 生命周期联动(ub-device-manager 侧) |
| TBD-VM-MIGRATE-COORD | VM 热迁移与 virt_agent/ub-device-manager 的协作细节 | 迁移场景 |

### 12.2 假设项(ASSUMPTION)

| ASSUMPTION 编号 | 描述 |
|-----------------|------|
| ASSUMPTION-01 | 假设 openstack 已完成 VM 创建与盘预分配,ubse@host 不参与 VM 创建 |
| ASSUMPTION-02 | 假设 VM 内 ubse 与 host 侧 ubse 是不同进程实例(同一份代码不同配置),不共享内存 |
| ASSUMPTION-03 | 假设 VM 内已安装必要的依赖(libssl、libxml2 等) |
| ASSUMPTION-04 | 假设 vsock CID 由 hypervisor 分配,ubse@VM 不主动管理 |
| ASSUMPTION-05 | 假设 VM 内 ubse 仅管理本 VM 直通的 SSU 设备,不跨 VM |
| ASSUMPTION-06 | 假设 ub-device-manager 已部署并运行,本特性不交付 ub-device-manager |
| ASSUMPTION-07 | 假设 ub-device-manager 通过现有 SDK/IPC 接口查询 ubse@host,无需 ubse@host 新增接口 |
| ASSUMPTION-08 | 假设 vsock 协议契约由 ub-device-manager 与 VM 侧共同遵守,版本兼容性由双方保证 |

### 12.3 MoSCoW 优先级汇总

| 优先级 | 需求项 |
|--------|--------|
| Must | FR-VM-DEPLOY、FR-VM-VSOCK-SERVER、FR-VM-QUERY、FR-VM-ATTACH、FR-VM-DETACH、FR-VM-CLI、FR-BOUNDARY-01、FR-BOUNDARY-02、FR-DEVICE-PASSTHROUGH |
| Should | FR-VM-LIFECYCLE、FR-VM-CACHE-PERSIST |
| Won't | host 侧 vsock client(由 ub-device-manager 负责)、host 侧推送执行器(由 ub-device-manager 负责)、VM 内 alloc/free(由 openstack 负责)、VM 内 FeDeviceAlloc(由 ubse@host 负责)、VM 主动向 ub-device-manager 请求全量同步(违反 server/client 角色,ubse@VM 仅作 server 被动接收) |

### 12.4 相关文档

- [SSU模块需求分析文档](../SSU模块需求分析文档.md)
- [SSU模块特性设计文档](../SSU模块特性设计文档.md)
- [SSU模块测试设计文档](../SSU模块测试设计文档.md)
- [SSU模块虚机化特性模块设计文档](./SSU模块虚机化特性模块设计文档.md)
- [UBSE技术架构设计文档](../../../architecture/UBSE技术架构设计文档.md)
- [高安部署说明.md](../../../../build_install/高安部署说明.md)
- [目录规范文档](../../../../conventions/目录规范文档.md)
- [编码规范文档](../../../../conventions/编码规范文档.md)
