# SSU 模块需求分析文档

> 源码路径：`src/controllers/ssu/`（主体）、`src/cli/ssu/`（CLI 特性）
> 服务契约：`src/include/plugin_services/ssu/ubse_ssu_service.h`
> 文档版本：V1.4
> 编写日期：2026-07-25

---

## 1. 引言

### 1.1 文档目的

本文档从需求视角描述 UBS-Engine 中 SSU（Storage Subsystem Unit）控制器的功能与非功能要求，明确业务目标、用户场景、约束条件与验收标准，作为设计、开发、测试与验收的依据。

### 1.2 文档范围

涵盖 SSU 控制器的以下需求域：

- 存储资源分配与释放（Namespace 生命周期管理）
- 资源挂载与卸载（含线性聚合、条带化聚合）
- 访问权限控制（Host NQN 授权）
- VFE 直通 VM：需求已随虚机化特性文档维护，FR-FE-DEVICE 现有能力规约见 [SSU模块虚机化特性需求分析文档](virt/SSU模块虚机化特性需求分析文档.md)（V1.3 迁出）
- 跨节点协同（master/agent 架构下的状态一致性）
- 多租户隔离
- 设备视图维护与账本管理
- CLI 运维管理特性（V1.1 并入）：运维通过 `ubsectl-ssu` 对 SSU 资源进行查询/创建/挂载/卸载及输入参数校验，UDS 短连接通信（`ubse_invoke_call`，moduleCode + opCode 接入协议）、wire 序列化、输出格式化与隐式认证（UDS peercred + daemon 侧 RBAC）

### 1.3 术语与缩略语

| 术语 | 全称 | 含义 |
|------|------|------|
| SSU | Storage Subsystem Unit | 存储子系统单元，UBS-Engine 管理的存储资源类型 |
| Namespace | - | NVMe 命名空间，SSU 容量分配的最小逻辑单元 |
| PFE | Physical Function Engine | 物理功能单元，SSU 设备的物理资源 |
| VFE | Virtual Function Engine | 虚拟功能单元，PFE 的虚拟化切片，可直通到 VM |
| EID | Endpoint Identifier | 端点标识，唯一标识一个 SSU 子系统 |
| NQN | NVMe Qualified Name | NVMe 合格名称，标识主机或子系统 |
| LBA | Logical Block Address | 逻辑块地址，常见格式 512B / 4K |
| master/agent | - | 分布式选主架构中的主节点与从节点角色 |
| DebtLedger | Debt Ledger | 债务账本，跨节点协同的事实源 |
| mTLS | mutual TLS | 双向 TLS 身份认证 |
| VIP | Virtual IP | 虚拟 IP，主备切换时跟随漂移 |
| CLI | Command-Line Interface | 命令行界面（CLI 特性，下同） |
| UDS | Unix Domain Socket | Unix 域套接字，CLI 与 daemon 通信通道（CLI） |
| opCode | Operation Code | 操作码，定位 daemon 侧 handler（CLI） |
| moduleCode | Module Code | 模块码，定位 daemon 侧服务模块（SSU=0x000D）（CLI） |
| wire 契约 | Wire Protocol Contract | CLI 与 handler 之间的二进制序列化协议（CLI） |
| peercred | Peer Credentials | UDS 对端进程凭证（uid/gid）（CLI） |
| RBAC | Role-Based Access Control | 基于角色的访问控制（CLI） |

> 注：标注"（CLI）"的术语为 CLI 特性引入；SSU/Namespace/PFE/VFE/EID/NQN/LBA 等通用术语与 CLI 文档重复时以本表为准。

### 1.4 参考资料

- [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) —— 详细设计与时序图(含 CLI 特性设计,第 14 章)
- [SSU模块测试设计文档.md](./SSU模块测试设计文档.md) —— UT/IT/PT/FUZZ/E2E 测试体系(含 CLI 特性测试设计)
- [UBSE代码Wiki.md](../../architecture/UBSE代码Wiki.md)
- [AGENTS.md](../../../../AGENTS.md)
- NVMe Base Specification 2.0
- NVMe over Fabrics Specification 1.1

---

## 2. 业务背景与目标

### 2.1 业务背景

在软件定义计算场景下，计算资源与存储资源需按需组合与分配。传统存储资源管理存在以下痛点：

1. **资源利用率低**：存储资源静态绑定到计算节点，无法在多节点间动态共享
2. **扩展性差**：单节点存储容量受限，无法横向扩展
3. **运维复杂**：手动配置 NVMe namespace、挂载、权限等操作易出错
4. **多租户隔离缺失**：无法有效隔离不同租户的存储资源
5. **虚拟化场景支持不足**：VM 无法直接访问物理存储资源，存在性能损耗

### 2.2 业务目标

SSU 控制器旨在提供**软件定义的存储资源池化管理能力**，实现：

| 目标 | 衡量指标 |
|------|----------|
| 资源池化 | 多个 SSU 物理设备的容量统一调度，按需分配 |
| 动态分配 | 秒级完成 namespace 创建与挂载，无需人工干预 |
| 多租户隔离 | 不同租户的存储资源物理隔离，无越权访问 |
| 虚拟化直通 | VFE 直通 VM，物理资源零损耗访问 |
| 高可用 | master/agent 架构支持故障自动切换，无单点故障 |
| 多接入方式 | 支持 SDK（C/Go/Python）、HTTP RESTful、CLI 多种接入 |
| 状态可追溯 | 完整的账本记录，支持重启后状态恢复 |

### 2.3 用户角色

| 角色 | 描述 | 典型操作 |
|------|------|----------|
| **平台管理员** | 负责存储资源池的整体运维 | 通过 HTTP/CLI 查询分配情况、监控容量 |
| **业务用户** | 通过 SDK 调用存储服务 | 申请/释放存储空间、挂载/卸载到主机 |
| **虚拟机** | 通过 VFE 直通访问存储 | 由 hypervisor 调用 FeDeviceAlloc 直通 VFE |
| **运维系统** | 自动化运维平台 | 调用 RESTful API 进行批量分配与监控 |
| **平台运维**（CLI） | 通过 `ubsectl-ssu` 日常巡检 | `display ssu -t alloc_summary` 查询分配概览（需 SSU 查询权限） |
| **存储管理员**（CLI） | 通过 `ubsectl-ssu` 创建存储空间 | `create ssu -n xx -s 10G`（需 SSU 分配权限） |
| **自动化脚本**（CLI） | 运维脚本嵌入 CLI 调用 | 脚本以授权用户身份执行 `ubsectl-ssu` 命令 |
| **故障排查**（CLI） | 问题定位时查询分配详情 | `display ssu -t alloc_detail -n xx`（需 SSU 查询权限） |

> 注：标注"（CLI）"的角色来自 CLI 特性文档 2.3；"虚机编排挂载"场景与本表"虚拟机/运维系统"角色重叠，其 CLI 操作（`attach ssu`）见 4.11 节 FR-CLI-ATTACH。

---

## 3. 总体需求

### 3.1 系统定位

SSU 控制器是 UBS-Engine 的核心控制器之一，位于控制器层（Controllers），向上对北向接入层（HTTP/SDK/CLI）提供存储服务接口，向下通过适配器插件（Adapter Plugins）操作底层 NVMe SSU 硬件。

### 3.2 运行环境

| 项目 | 要求 |
|------|------|
| 操作系统 | openEuler Linux（ARM64） |
| 硬件平台 | 配备 NVMe SSU 设备的服务器 |
| 依赖库 | libssu.so（SSU 硬件操作库）、libuuid.so.1（UUID 生成，可选） |
| 通信协议 | UDS（本机 SDK）、RPC（跨节点）、HTTP+TLS（北向 RESTful） |
| 进程模型 | 作为 `ubse` daemon 的插件模块运行 |

### 3.3 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│              北向接入层                                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ HTTP+TLS │  │ SDK(UDS) │  │   CLI    │                  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                  │
└───────┼─────────────┼─────────────┼────────────────────────┘
        │             │             │
        ▼             ▼             ▼
┌─────────────────────────────────────────────────────────────┐
│              SSU 控制器（本模块）                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │           UbseSsuServiceImp（单例）                  │    │
│  │  ┌──────────┬──────────┬──────────┬──────────────┐  │    │
│  │  │ Collector│ Scheduler│DebtLedger│DirectToVmMgr │  │    │
│  │  └──────────┴──────────┴──────────┴──────────────┘  │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────┬───────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ SSU Adapter  │  │   MTI 层     │  │  框架服务    │
│ (libssu.so)  │  │(urma/bus)    │  │(HA/Com/Timer)│
└──────────────┘  └──────────────┘  └──────────────┘
        │                 │
        ▼                 ▼
┌─────────────────────────────────────────────────────────────┐
│              硬件层（NVMe SSU 设备）                         │
└─────────────────────────────────────────────────────────────┘
```

### 3.4 部署架构

采用 **master/agent** 分布式架构：

| 角色 | 数量 | 职责 |
|------|------|------|
| **master** | 1（活跃） | 维护设备视图、账本、调度算法、HTTP 接口 |
| **standby** | 1（备用） | 同步 master 状态，master 故障时接管 |
| **agent** | N（扩展） | 转发请求到 master，执行本地 attach/detach |

**故障切换**：master 故障时，standby 通过选举提升为新 master，VIP HTTP Server 跟随 VIP 漂移。

---

## 4. 功能需求

### 4.1 存储空间分配（FR-ALLOC）

#### 4.1.1 需求描述

用户通过 SDK/HTTP 申请指定容量的存储空间，系统在多个 SSU 设备上创建 namespace 集合，返回可挂载的设备路径与连接信息。

#### 4.1.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| name | string | 是 | 最大 47 字符（HTTP/CLI 入口校验 47；wire 层缓冲区 `MAX_NAME_LEN=48` 含 `'\0'`），全局唯一 |
| nsSize | uint64 | 是 | 总容量（字节），>0 |
| nsNum | uint32 | 是 | namespace 数量，>0，≤在线设备数；等于 1 时 strategy 不生效 |
| lbaFormat | enum | 是 | 512B（512）或 4K（4096） |
| strategy | enum | 否 | STRIPED（条带化）、LINEAR（线性）或 NORMAL（普通，缺省值，不聚合块设备） |
| tenant | string | 否 | 最大 16 字符（`MAX_TENANT_LEN=17` 含 `'\0'`），租户隔离标识 |

#### 4.1.3 功能规则

**FR-ALLOC-01 分配策略**

- **STRIPED（条带化）**：从 N 个设备各分配等量容量，每个 namespace 大小为 `nsSize / nsNum`。要求 `nsSize` 整除 `nsNum`，且整除结果为 `chunkSize` 的整数倍。CLI 侧要求 `nsNum ≥ 2`（`ERR_STRIPED_NS_NUM_REQUIRED`）。
- **LINEAR（线性）**：按负载均衡算法在 N 个设备间分配总容量，各设备分配量可不等，对齐到 `lbaSize`。
- **NORMAL（普通，缺省）**：只分配 nvme 裸设备，不聚合块设备，支持多节点共享挂载。
- 当 `nsNum == 1` 时，strategy 不生效。

**FR-ALLOC-02 设备选择**

- 仅从 `ONLINE` 状态且已有 namespace 数 < 128 的设备中分配。
- 优先选择剩余空间大的设备，其次选择已有 namespace 数少的设备（兼顾均衡）。
- 条带化分配时，预先过滤掉空间不足容纳单 namespace 的设备。

**FR-ALLOC-03 多租户隔离**

- 设备通过 namespace 的 `customData.tenant` 标记所属租户。
- 分配时仅选择 `dev.tenant == req.tenant` 或 `dev.tenant` 为空的设备。
- 无租户请求（`tenant` 为空）不能占用已被租户隔离的设备。

**FR-ALLOC-04 重复分配拒绝**

- 复合键账本（name + resourceUuid）：同名已存在任一非 `CREATING` 状态条目（CREATED/ATTACHING/ATTACHED 等）时，重复分配一律拒绝，返回 `UBSE_ERR_ALREADY_ALLOCATED`（2000）。
- 已存在 `CREATING` 状态条目（agent RPC 预置的在途分配标记）时放行，复用预置 resourceUuid 继续完成分配；RPC 预置路径通过 per-name 锁保证查重 + 预置入账的原子性，并发同名仅一个成功，其余返回 `UBSE_ERR_ALREADY_ALLOCATED`。

**FR-ALLOC-05 预扣容量**

- 调度成功后立即预扣容量（`reservedBytes`），防止 30s 采集间隔内的并发分配超分。
- namespace 创建失败时回滚预扣容量。
- collector 刷新后若硬件 usedBytes 已反映分配，则清除预留。

**FR-ALLOC-06 失败回滚**

- 任一 namespace 创建失败时，回滚已创建的 namespace。
- 回滚失败时记录 ERROR 日志，需人工介入。

#### 4.1.4 输出结果

```cpp
struct UbseSsuAllocResult {
    std::string name;                                // 请求标识
    std::string resourceUuid;                        // 分配唯一标识，脑裂合并后区分同名多次分配
    UbseSsuAllocStrategy strategy;                   // 分配策略
    std::vector<UbseSsuNameSpaceInfo> nameSpaceList; // namespace 信息列表
};
```

每个 `UbseSsuNameSpaceInfo` 包含：tgtEid、tgtNqn、nsUuid、namespaceId、nsDevPath（`/dev/disk/by-id/nvme-uuid.<hex>`）、nsSize、lbaFormat、allowHostNqnList。

#### 4.1.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-ALLOC-01 | 分配 N 个 namespace 后，每个 namespace 在 `/dev/disk/by-id/` 下可访问 |
| AC-ALLOC-02 | STRIPED 分配时，每个 namespace 容量相等且等于 `nsSize/nsNum` |
| AC-ALLOC-03 | 重复分配同名空间返回 `UBSE_ERR_ALREADY_ALLOCATED`（2000），不产生新 namespace |
| AC-ALLOC-04 | 并发分配同一资源名时，仅一个请求成功 |
| AC-ALLOC-05 | 租户 A 的资源不能在租户 B 的设备上分配 |
| AC-ALLOC-06 | 分配失败时不残留任何 namespace |
| AC-ALLOC-07 | master 重启后，已分配空间仍可通过 GetAllocInfoByName 查询 |

---

### 4.2 存储空间释放（FR-FREE）

#### 4.2.1 需求描述

用户释放已分配的存储空间，系统删除所有关联的 namespace，回收容量。

#### 4.2.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| name | string | 是 | 与分配时的 name 一致 |
| resourceUuid | string | 否 | 分配唯一标识：非空时仅释放该 resourceUuid 对应空间（脑裂合并同名多份场景）；空串时释放该 name 下全部空间 |
| identity | struct | 是 | 调用方身份信息 |

#### 4.2.3 功能规则

**FR-FREE-01 释放流程**

- 根据账本记录的 namespace 列表，逐个调用 `delete_namespace`。
- 删除成功的 namespace 记录预释放容量（`releasedBytes`）。
- 全部删除成功后，从账本移除该条目。

**FR-FREE-02 释放幂等性（非幂等）**

- 释放不存在的空间（含指定的 resourceUuid 在该 name 下不存在）报错 `UBSE_ERR_NO_NEED_FREE`（2002），不再幂等返回成功。
- HTTP 层保留对 `UBSE_ERR_NOT_EXIST` 返回 200 OK 的豁免分支（当前服务层实际返回 `UBSE_ERR_NO_NEED_FREE`，该豁免不会命中）。

**FR-FREE-03 部分失败处理**

- 部分 namespace 删除失败时，账本保留剩余 namespace 信息，便于重试。
- 已删除的 namespace 仍记录预释放容量。

#### 4.2.4 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-FREE-01 | 释放后 namespace 设备路径不再存在 |
| AC-FREE-02 | 释放后容量可被再次分配 |
| AC-FREE-03 | 释放不存在的空间返回 `UBSE_ERR_NO_NEED_FREE`（2002） |
| AC-FREE-04 | 部分 namespace 删除失败后，剩余 namespace 可通过 GetAllocInfoByName 查询 |
| AC-FREE-05 | 释放后再次分配同名空间可成功 |

---

### 4.3 存储空间挂载（FR-ATTACH）

#### 4.3.1 需求描述

将已分配的 namespace 挂载到指定主机，使其可被主机通过 NVMe-oF 协议访问。支持三种挂载模式：

- **基础挂载**：直接挂载 namespace 集合
- **线性聚合挂载**：将多个 namespace 线性拼接为一个逻辑块设备
- **条带化聚合挂载**：将多个 namespace 以条带化方式聚合（RAID0/RAID5）

#### 4.3.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| name | string | 是 | 已分配的空间标识 |
| nqn | string | 是 | Host 的 NVMe Qualified Name |
| srcEid | string | 是 | 源 EID |
| identity | struct | 是 | 调用方身份信息 |
| resourceUuid | string | detach 可选 | 精确卸载该 resourceUuid 对应空间（脑裂合并同名多份场景）；空串按 name 解析（同名多份时拒绝 `UBSE_SSU_ERROR_NAME_DUPLICATED`）；attach 忽略该字段 |
| devName | string | 线性/条带化必填 | 聚合后的块设备名称 |
| level | enum | 条带化必填 | RAID0 或 RAID5 |
| chunkSize | enum | 条带化必填 | 4K/16K/32K/64K/128K/256K/512K |

#### 4.3.3 功能规则

**FR-ATTACH-01 身份验证**

- master 端调用 `VerifyAttachDetachPrecondition` 校验 attach/detach 前置条件（identity、账本 state、条带化参数）；identity 校验先于幂等/state 判断，防止幂等分支向非属主泄露 nsDevPath/devPath。
- 校验 `identity.tenant` 与账本记录的 `allocReq.tenant` 一致。
- 校验 `identity.userName` 与账本记录的 `allocReq.userName` 一致。
- NORMAL 策略多节点共挂场景下按 `requestNodeId`（发起节点）维度判断；`resourceUuid` 非空时按 (name, resourceUuid) 精确定位（跳过多 uuid 歧义拒绝）。
- 校验通过后返回 nsVerifyList（含 defaultNqn/jettyId/guid 等构造 attach 所需字段）。

**FR-ATTACH-02 挂载流程（agent 端）**

- 每个 namespace 执行：
  1. `attach_namespace(tgtEid, namespaceId, srcEid)` —— 将 namespace 挂载到源 EID
  2. `add_namespace_allow_host(tgtEid, namespaceId, nqn)` —— 授权 Host NQN 访问
- 挂载成功后返回 namespace 设备路径列表。

**FR-ATTACH-03 状态机推进**

- attach 开始前，agent 上报 `ATTACHING` 状态，master 修改账本 `CREATED → ATTACHING`。
- attach 完成后，agent 上报 `ATTACHED` 状态，master 修改账本 `ATTACHING → ATTACHED`。
- 容忍 `ATTACHING` 通知丢失：允许 `CREATED → ATTACHED` 直接转换。
- 已挂载（ATTACHED 状态）的空间重复挂载报错 `UBSE_ERR_ALREADY_ATTACHED`（2001），不再幂等返回成功；线性/条带化聚合空间被其他节点挂载时报 `UBSE_SSU_ERROR_ATTACHER_MISMATCH`（1203）。

**FR-ATTACH-04 聚合挂载**

- **线性聚合**：将多个 namespace 设备按顺序拼接为一个逻辑块设备，数据按顺序填充。
- **条带化聚合**：将多个 namespace 设备按 chunkSize 分片轮流写入，支持 RAID0（无校验）和 RAID5（含校验，至少 3 个成员设备）。

#### 4.3.4 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-ATTACH-01 | 挂载后主机可通过 `nvme list` 看到 namespace 设备 |
| AC-ATTACH-02 | 挂载后 Host NQN 可读写 namespace |
| AC-ATTACH-03 | 非授权 Host NQN 无法访问 namespace |
| AC-ATTACH-04 | 线性聚合后，逻辑块设备容量等于各 namespace 容量之和 |
| AC-ATTACH-05 | 条带化聚合后，逻辑块设备容量等于各 namespace 容量之和 |
| AC-ATTACH-06 | RAID5 聚合少于 3 个设备时返回错误 |
| AC-ATTACH-07 | 身份验证失败时返回错误，不执行 attach |

---

### 4.4 存储空间卸载（FR-DETACH）

#### 4.4.1 需求描述

将已挂载的 namespace 从主机卸载，释放设备占用。

#### 4.4.2 功能规则

**FR-DETACH-01 卸载流程**

- 移除 Host NQN 访问权限。
- 调用 `detach_namespace` 解除挂载。
- 卸载前需确保没有进程正在使用该存储空间（由调用方保证）。

**FR-DETACH-02 状态机推进**

- detach 完成后，agent 上报状态，master 修改账本 `ATTACHED → CREATED`，等待重新挂载。
- 已卸载（CREATED 状态）的空间重复卸载报错 `UBSE_ERR_NO_NEED_DETACH`（2003），不再幂等返回成功；线性/条带化聚合空间被其他节点挂载时，挂载或卸载均报 `UBSE_SSU_ERROR_ATTACHER_MISMATCH`（1203）。

#### 4.4.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-DETACH-01 | 卸载后主机 `nvme list` 不再显示该 namespace |
| AC-DETACH-02 | 卸载后再次 attach 可成功 |
| AC-DETACH-03 | 聚合设备卸载后，逻辑块设备不再存在 |

---

### 4.5 访问权限控制（FR-PERM）

#### 4.5.1 需求描述

对已分配的存储空间，按 Host NQN 授予或撤销访问权限，使指定主机可通过 NVMe-oF 协议访问对应 namespace。

#### 4.5.2 功能规则

**FR-PERM-01 添加权限**

- 在 Target 侧将 Host NQN 添加到子系统的允许主机列表。
- 重复添加同一 Host NQN 返回成功（幂等）。

**FR-PERM-02 移除权限**

- 在 Target 侧将 Host NQN 从子系统的允许主机列表移除。
- 移除不存在的 Host NQN 返回成功（幂等）。
- 移除权限前应确保该 Host 已断开与对应 namespace 的连接。

#### 4.5.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-PERM-01 | 添加权限后，Host NQN 可访问 namespace |
| AC-PERM-02 | 移除权限后，Host NQN 无法访问 namespace |
| AC-PERM-03 | 重复添加/移除同一 Host NQN 返回成功 |
| AC-PERM-04 | 非授权 Host NQN 无法访问 namespace |

---

### 4.6 跨节点协同（FR-DISTRIBUTED）

#### 4.6.1 需求描述

在 master/agent 架构下，agent 节点的请求需转发到 master 处理，并保持状态一致性。

#### 4.6.2 功能规则

**FR-DISTRIBUTED-01 角色分发**

- master 节点：执行分配/释放/权限控制等实际业务逻辑。
- agent/standby 节点：通过 RPC 转发请求到 master，使用 Future 等待响应。
- 请求超时（30s）后自动清理 master 端账本，返回超时错误。

**FR-DISTRIBUTED-02 账本一致性**

- 账本（DebtLedger）仅在 master 端维护，作为跨节点协同的事实源。
- master 在真正执行分配前先以 `CREATING` 状态占位，防止并发重复请求。
- 分配成功后修改为 `CREATED`，失败则移除账本条目。
- attach/detach 通过 `STATUS_UPDATE` RPC 上报状态，master 校验状态机合法性后更新账本。

**FR-DISTRIBUTED-03 账本重建**

- master 启动或重启后，从硬件 namespace 列表反推重建账本。
- 按 namespace 的 `customData.name` 分组还原分配记录。
- 根据 namespace 是否已 attach 推断状态（`CREATED` 或 `ATTACHED`）。
- `CREATING` 状态的条目不重建（硬件视图中无对应 namespace）。

**FR-DISTRIBUTED-04 RPC 模式**

- **异步 + Future**：耗时写操作（alloc/free/perm）走异步派发，master 立即 ack，业务结果通过单独 RPC 推回。
- **同步 sync resp**：轻量查询（verify/stats/list/connectInfo）走同步返回，省去 future 等待开销。

**FR-DISTRIBUTED-05 故障切换**

- master 故障时，standby 通过选举提升为新 master。
- HTTP 接口跟随 VIP 漂移到新 master。
- 账本通过 `Rebuild` 从硬件视图恢复。

#### 4.6.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-DIST-01 | agent 节点的 alloc 请求最终在 master 端创建 namespace |
| AC-DIST-02 | agent 请求超时后返回错误，且 master 端无残留 |
| AC-DIST-03 | master 重启后，已分配空间仍可查询与挂载 |
| AC-DIST-04 | 并发 alloc 同名资源（来自不同 agent）时仅一个成功 |
| AC-DIST-05 | master 故障切换后，新 master 可继续提供服务 |
| AC-DIST-06 | 跨节点调用链 traceId 可串联 |

---

### 4.7 设备视图维护（FR-COLLECTOR）

#### 4.7.1 需求描述

周期性采集 SSU 设备列表与 namespace 状态，为调度器提供准确的设备视图。

#### 4.7.2 功能规则

**FR-COLLECTOR-01 采集周期**

- 每 30 秒采集一次设备列表。
- 启动时立即采集一次，避免首次延迟。
- 仅 master 节点启动采集。

**FR-COLLECTOR-02 缓存维护**

- 维护 `EID → DevInfo` 的内存缓存。
- 采集到空列表时保持旧缓存不变（避免短暂硬件异常导致状态丢失）。
- 缓存为空时调度器返回错误，调用方需确保采集已成功完成。

**FR-COLLECTOR-03 容量预留机制**

- 维护两本账：`reservedBytes`（预扣除）和 `releasedBytes`（预释放）。
- `GetDevListWithReservations` 返回的 `usedBytes = hardwareUsed + reserved - released`。
- 采集刷新后若无进行中的预留操作（`pendingOps_ == 0`），清除预留账本。
- 预留操作与采集刷新通过 `pendingOps_` + `shared_lock` 互斥，防止竞态。

#### 4.7.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-COLLECT-01 | 采集后调度器看到的设备容量与硬件实际一致 |
| AC-COLLECT-02 | 采集期间发生的分配不会导致超分 |
| AC-COLLECT-03 | 硬件短暂异常返回空列表时，缓存保持不变 |
| AC-COLLECT-04 | agent 节点不启动采集 |

---

### 4.8 资源调度算法（FR-SCHEDULER）

#### 4.8.1 需求描述

基于责任链模式执行多 namespace 的容量分配算法。

#### 4.8.2 功能规则

**FR-SCHEDULER-01 责任链结构**

按 `PRE_CHECK → ALGORITHM` 顺序执行 filter：
- `PreCheckHandler`：参数校验、设备预处理（过滤+排序）、设备数量校验
- `UbseSsuTenantIsolationFilter`：多租户隔离过滤
- `UbseSsuAllocateAlgorithmHandler`：分配算法执行

**FR-SCHEDULER-02 设备预处理**

- 过滤非 `ONLINE` 状态设备。
- 过滤已有 namespace 数 ≥ 128 的设备。
- 条带化分配时过滤空间不足容纳单 namespace 的设备。
- 按 `freeBytes` 降序 + `nsCount` 升序排序（优先大容量、低负载设备）。

**FR-SCHEDULER-03 条带化分配算法**

- 每个 namespace 分配 `singleNsSize = allocSize / nsNum`。
- 取前 N 个设备（已排序），每个分配 `singleNsSize`。
- 校验前 N 个设备中最小剩余空间 ≥ `singleNsSize`。

**FR-SCHEDULER-04 线性分配算法（负载均衡）**

- 第一轮：按 `freeBytes` 占比权重分配，向下对齐到 `sectorSize`。
- 第二轮：贪心轮询补齐第一轮向下对齐产生的余量。
- 校验剩余未分配容量为 0 且分配设备数等于 `nsNum`。

**FR-SCHEDULER-05 扩展性**

- 支持通过 `AddFilter(groupId, filter)` 在任一 group 插入自定义 filter，无需修改现有代码。

#### 4.8.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-SCHED-01 | 条带化分配后，每个 namespace 容量相等 |
| AC-SCHED-02 | 线性分配后，各设备分配量与剩余空间成比例 |
| AC-SCHED-03 | 设备不足时返回 `INSUFFICIENT_SPACE` 错误 |
| AC-SCHED-04 | 参数非法时返回 `INVALID_PARAM` 错误 |
| AC-SCHED-05 | 租户隔离过滤后，设备数不足时返回错误 |

---

### 4.9 北向接口接入（FR-NORTHBOUND）

#### 4.9.1 需求描述

支持 SDK、HTTP、CLI 三种接入方式。

#### 4.9.2 功能规则

**FR-NORTHBOUND-01 SDK 接入（IPC）**

- 本机 SDK 通过 UDS（Unix Domain Socket）调用。
- 17 个 IPC handler 覆盖全部服务接口（`UBSE_IPC_SSU_*` opCode 0x0004~0x0014）。
- 调用方通过 `UbseRequestContext::clientInfo.uid` 标识身份，系统翻译为 `userName`。
- 权限串 `"ssu"`：SDK 客户端必须持有该权限才能调用。
- 三阶段模板方法：`Unpack → Handle → Pack`。

**FR-NORTHBOUND-02 HTTP 接入（RESTful）**

- 通过 VIP HTTP Server 暴露，只在 master 节点生效。
- mTLS 双向认证：从对端证书 `CN` 提取 `userName`，`OU` 提取 `uid`。
- 路由：
  - `POST /ubse/v1/ssu/spaces` —— 分配存储空间
  - `DELETE /ubse/v1/ssu/spaces/{name}` —— 释放存储空间
- 主备切换时路由跟随 VIP 漂移。

**FR-NORTHBOUND-03 HTTP 错误码映射**

| 业务错误 | HTTP 状态码 |
|----------|-------------|
| 分配成功 | 201 Created |
| 资源已存在（`UBSE_ERR_EXISTED`） | 409 Conflict（映射保留；分配重复现返回 2000，实际按 500 携带错误码返回） |
| 身份认证失败（mTLS 证书无效） | 401 Unauthorized |
| 请求参数错误 | 400 Bad Request |
| 服务不可用 | 503 Service Unavailable |
| 释放不存在的资源 | 服务层返回 `UBSE_ERR_NO_NEED_FREE`（2002），HTTP 映射 500（代码保留对 `UBSE_ERR_NOT_EXIST` 的 200 OK 豁免分支，当前不会命中） |
| 其他失败 | 500 Internal Server Error |

**FR-NORTHBOUND-04 CLI 接入**

- 通过 `ubse_cli_framework` 调用 SDK 接口。

#### 4.9.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-NB-01 | SDK 客户端可调用全部 17 个接口 |
| AC-NB-02 | HTTP 客户端可调用 alloc/free 接口 |
| AC-NB-03 | mTLS 认证失败返回 401 |
| AC-NB-04 | agent 节点的 HTTP 请求失败（VIP 不在 agent） |
| AC-NB-05 | 主备切换后，HTTP 接口在新 master 可用 |

---

### 4.10 查询与监控（FR-QUERY）

#### 4.10.1 需求描述

提供已分配空间、namespace 统计、连接信息的查询能力。

#### 4.10.2 功能规则

**FR-QUERY-01 ListAllocInfo**

- 列出所有已分配空间信息，包括 namespace 列表、容量、LBA 格式、策略等。

**FR-QUERY-02 GetAllocInfoByName**

- 按名称查询已分配空间详细信息。

**FR-QUERY-03 GetNsStats**

- 查询指定空间下各 namespace 的总容量与已用容量。

**FR-QUERY-04 GetConnectInfo**

- 查询指定空间在指定 VFE 上的 NVMe 连接信息（srcEid/tgtEid/tgtNqn/hostNqn/nsUuid/nsId）。
- VFE 参数可选，为空时取 host FE eid。

**FR-QUERY-05 GetFeDeviceList**

- 查询系统中所有 FE 设备信息，包括每个 PFE 下的 VFE 列表及绑定关系。

#### 4.10.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-QUERY-01 | ListAllocInfo 返回的列表包含所有已分配空间 |
| AC-QUERY-02 | GetAllocInfoByName 返回的 namespace 列表与分配时一致 |
| AC-QUERY-03 | GetNsStats 返回的容量与硬件实际一致 |
| AC-QUERY-04 | GetConnectInfo 返回的连接信息可用于 NVMe-oF 连接 |
| AC-QUERY-05 | GetFeDeviceList 返回的 VFE 列表与硬件实际一致 |

---

### 4.11 CLI 运维管理特性

#### 4.11.0 需求描述与命令总览

SSU-CLI（可执行程序 `ubsectl-ssu`，源码 `src/cli/ssu/`）是 UBS-Engine 的命令行运维入口，通过 UDS 短连接（`ubse_invoke_call`）与 `ubse` daemon 通信，完成 SSU 资源的查询与操作。其定位为：**独立可执行程序**（无运行时库依赖）、**一次性执行模型**（每次执行连接 daemon、完成单命令、退出）、**客户端角色**（不持有业务状态，所有业务逻辑由 daemon 完成并统一鉴权）。CLI 不直接调用 SSU 业务逻辑，其 wire 契约与服务端 `Ssu*Pack/Ssu*Unpack` 严格对应。

本节内容并入自原 CLI 特性需求分析文档（代码反推模式），需求编号统一使用 `FR-CLI-*` / `AC-CLI-*` 前缀。

`ubsectl ssu` 命令总览（汇总自 FR-CLI-DISPLAY / FR-CLI-CREATE / FR-CLI-DELETE / FR-CLI-ATTACH / FR-CLI-DETACH / FR-CLI-COVERAGE；opCode 定义于 `src/framework/ipc/include/ubse_ipc_common.h`）：

| 命令 | 说明 | opCode(hex) | 功能域 |
|------|------|-------------|--------|
| `display ssu -t alloc_summary` | 查询分配概览（3 列表） | 0x0004 | FR-CLI-DISPLAY |
| `display ssu -t alloc_detail -n <name>` | 查询指定分配详情（7 列表） | 0x0005 | FR-CLI-DISPLAY |
| `display ssu -t connect_info -n <name>`（VM 模式） | 查询已直通 namespace 连接信息（6 列表） | 0x0007 | FR-CLI-DISPLAY |
| `create ssu -n -s [-l] [-m] [-r]` | 创建存储空间（缺省 Normal；可选 Linear/Striped） | 0x000A | FR-CLI-CREATE |
| `delete ssu -n` | 释放存储空间（无响应体，原覆盖缺口 `free ssu` 已实现） | 0x000D | FR-CLI-COVERAGE |
| `attach ssu -n [-q] [-e]` | 普通挂载 | 0x000B | FR-CLI-ATTACH |
| `attach ssu -t Linear -n -d [-q] [-e]` | 线性聚合挂载 | 0x000E | FR-CLI-ATTACH |
| `attach ssu -t Striped -n -d -l -c` | 条带化聚合挂载 | 0x0010 | FR-CLI-ATTACH |
| `detach ssu -n [-q]` | 普通卸载 | 0x000C | FR-CLI-DETACH |
| `detach ssu -t Linear -n -d` | 线性聚合卸载 | 0x000F | FR-CLI-DETACH |
| `detach ssu -t Striped -n -d` | 条带化聚合卸载 | 0x0011 | FR-CLI-DETACH |
| `add/remove access permission`、`get ns stats`、`get fe device list`、`fe device alloc/free` | 已知命令覆盖缺口（未实现 CLI 入口） | 0x0008/0x0009/0x0006/0x0012/0x0013/0x0014 | FR-CLI-COVERAGE |

> 注：VM 模式（`ubse.conf` `[ubse.ssu] vm_mode=true` 或环境变量 `UBSE_SSU_VM_MODE=1`）下仅注册 display/attach/detach 命令子集（盘由 openstack 分配，不注册 create/delete）；`display ssu -t` 在 VM 模式额外支持 `connect_info`。

#### 4.11.1 程序启动与生命周期（FR-CLI-STARTUP）

##### 4.11.1.1 需求描述

`ubsectl-ssu` 程序应支持从命令行启动，完成参数解析与命令分发，执行完成后退出。

##### 4.11.1.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| argc | int | 是 | 范围 [1, 43] |
| argv | char*[] | 是 | argv[0]=程序名，后续为命令选项 |
| 隐式 type | string | 是 | 程序自动在 argv[1] 注入 `"ssu"` |

##### 4.11.1.3 功能规则

**FR-CLI-STARTUP-01 启动保护**：程序启动时校验 `argc ∈ [1, 43]`、`argv` 非空，防止异常启动

**FR-CLI-STARTUP-02 模块注册**：通过 `UBSE_CLI_REGISTER_MODULE` 宏静态注册 SSU 模块，启动时调用 `UbseCliCallAllModuleSignUp()` 完成注册

**FR-CLI-STARTUP-03 程序上下文**：设置程序名 `ubsectl-ssu`，不显示 type（因 type 隐式注入）

**FR-CLI-STARTUP-04 隐式 type 注入**：在 argv 位置 1 插入 `"ssu"`，使解析器收到的 args 形如 `[cmd, "ssu", -opt, val, ...]`

**FR-CLI-STARTUP-05 帮助短路**：单独 `--help` 直接返回帮助信息，不进行后续解析

**FR-CLI-STARTUP-06 白名单校验**：校验 command/type/option-name 位置的合法性

**FR-CLI-STARTUP-07 命令完整性**：确保 command 和 type 均已提供

**FR-CLI-STARTUP-08 参数解析**：将命令行解析为 `inputOptionMap_`（长选项名 → 值）

**FR-CLI-STARTUP-09 超时定时器**：注册 SIGALRM 处理函数并启动超时定时器，防止单命令卡死

**FR-CLI-STARTUP-10 IPC 日志屏蔽**：CLI 执行期间屏蔽 IPC 日志，避免日志噪声

**FR-CLI-STARTUP-11 命令分发**：回调匹配命令的 `commandFunc`，打印结果

**FR-CLI-STARTUP-12 定时器停止**：命令执行完成后停止超时定时器

##### 4.11.1.4 输出结果

- 启动成功：进入命令分发流程
- 启动失败：返回 `UBSE_ERROR`，打印错误信息

##### 4.11.1.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-STARTUP-01 | `ubsectl-ssu --help` 输出帮助信息，退出码 0 |
| AC-CLI-STARTUP-02 | `ubsectl-ssu` 无参数启动，提示缺少 command/type |
| AC-CLI-STARTUP-03 | argc > 43 时启动失败，返回 UBSE_ERROR |
| AC-CLI-STARTUP-04 | 命令执行超过定时器阈值，SIGALRM 触发并退出 |

#### 4.11.2 命令注册机制（FR-CLI-CMD-REG）

##### 4.11.2.1 需求描述

SSU CLI 应通过静态注册机制，将命令注册到 CLI Framework 的命令注册表：host 模式注册 display/create/delete/attach/detach 5 类命令，VM 模式仅注册 display/attach/detach 3 类命令。

##### 4.11.2.2 输入参数

无（注册在程序启动时自动完成）

##### 4.11.2.3 功能规则

**FR-CLI-CMD-REG-01 模块注册**：通过 `UBSE_CLI_REGISTER_MODULE("CLI_SSU_MODULE", UbseCliRegSsuModule)` 宏静态注册 SSU 模块
**FR-CLI-CMD-REG-02 命令构建**：在 `UbseCliSignUp()` 中按 display/create/delete/attach/detach 顺序构建命令（host 模式 5 个命令；VM 模式仅注册 display/attach/detach 子集，盘由 openstack 分配）
**FR-CLI-CMD-REG-03 命令元信息**：每个命令通过 `UbseCliRegBuilder` 链式构建，设置 command/type/options/func
**FR-CLI-CMD-REG-04 选项定义**：每个命令定义其支持的选项（short/long/description）

##### 4.11.2.4 输出结果

- 注册成功（host 模式）：`UbseCliModuleRegistry` 中存在 SSU 模块的 5 个命令
- 注册成功（VM 模式）：`UbseCliModuleRegistry` 中存在 SSU 模块的 3 个命令（display/attach/detach）
- 注册失败：静态初始化失败，程序无法启动

##### 4.11.2.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-CMD-REG-01 | host 模式程序启动后，`registry` 含 5 个 SSU 命令；VM 模式含 3 个 |
| AC-CLI-CMD-REG-02 | 每个命令的 type 字段为 `"ssu"` |
| AC-CLI-CMD-REG-03 | 每个命令的 options 列表与设计一致 |

#### 4.11.3 display ssu 命令（FR-CLI-DISPLAY）

##### 4.11.3.1 需求描述

提供查询 SSU 分配信息的能力，支持概览（summary）、详情（detail）与连接信息（connect_info，仅 VM 模式）三种模式。

##### 4.11.3.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| -t/--type | string | 是 | 取值 `alloc_summary`、`alloc_detail`（VM 模式额外支持 `connect_info`） |
| -n/--name | string | alloc_detail/connect_info 必填 | 1-47 字符，字母/数字/`.`/`:`/`-`/`_` |

##### 4.11.3.3 功能规则

**FR-CLI-DISPLAY-01 概览查询**：`alloc_summary` 模式调用 `UBSE_IPC_SSU_LIST_ALLOC_INFO`（opCode=0x0004），返回所有分配信息；`alloc_summary` 不接受 `-n`（显式报错 `ERR_SUMMARY_REJECTS_NAME`）

**FR-CLI-DISPLAY-02 详情查询**：`alloc_detail` 模式调用 `UBSE_IPC_SSU_GET_ALLOC_INFO_BY_NAME`（opCode=0x0005），返回指定 name 的分配详情

**FR-CLI-DISPLAY-03 名称校验**：`alloc_detail`/`connect_info` 模式下 `-n` 必填且通过 `IsValidName` 校验（1-47 字符）

**FR-CLI-DISPLAY-04 概览输出**：空列表输出 `INFO_EMPTY` 提示；非空输出 3 列表（name/size/strategy），size 为该分配下所有 namespace `nsSize` 求和的原始字节值（不做 GiB 换算）

**FR-CLI-DISPLAY-05 详情输出**：空列表输出 `INFO_EMPTY` 提示；非空输出 7 列表（ns_uuid/tgt_eid/tgt_nqn/namespace_id/ns_dev_path/ns_size/lba_format），含表头合并行

**FR-CLI-DISPLAY-06 表格列宽**：固定列宽 80（8×10）

**FR-CLI-DISPLAY-07 连接信息查询**（VM 模式，代码反推补充）：`connect_info` 模式调用 `UBSE_IPC_SSU_GET_CONNECT_INFO`（opCode=0x0007），返回 6 列表（src_eid/tgt_eid/tgt_nqn/host_nqn/ns_uuid/ns_id）；空列表输出 `INFO_EMPTY_CONNECT`

##### 4.11.3.4 输出结果

- 概览：`name/size/strategy` 三列表，或 `INFO: No SSU allocation information found.`
- 详情：`Name: <name> Strategy: <Linear|Striped|Normal>` 合并行 + 7 列表，或 INFO_EMPTY
- 连接信息（VM 模式）：6 列表，或 `INFO: No SSU connect information found.`

##### 4.11.3.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-DISPLAY-01 | `display ssu -t alloc_summary` 返回所有分配概览 |
| AC-CLI-DISPLAY-02 | 无分配时，`alloc_summary` 输出 INFO_EMPTY |
| AC-CLI-DISPLAY-03 | `display ssu -t alloc_detail -n <name>` 返回指定分配详情 |
| AC-CLI-DISPLAY-04 | `alloc_detail` 缺少 `-n` 时，返回 ERR_NAME_REQUIRED |
| AC-CLI-DISPLAY-05 | `alloc_detail` 的 `-n` 格式非法时，返回 ERR_INVALID_NAME |
| AC-CLI-DISPLAY-06 | 概览表 size 列聚合所有 namespace 容量，直接展示原始字节值 |

#### 4.11.4 create ssu 命令（FR-CLI-CREATE）

##### 4.11.4.1 需求描述

提供创建 SSU 存储空间（分配 namespace）的能力，支持线性与条带化两种策略。

##### 4.11.4.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| -n/--name | string | 是 | 1-47 字符，字母/数字/`.`/`:`/`-`/`_` |
| -s/--size | string | 是 | 格式 `<正整数>G`，≥1G，大写后缀 |
| -l/--lba | string | 否 | 取值 `512B` 或 `4K`，默认 512B |
| -m/--ns_num | int | 否 | 正整数，范围 [1, 128]，默认 1 |
| -r/--strategy | string | 否 | 仅可显式指定 `Linear` 或 `Striped`，缺省 Normal |

##### 4.11.4.3 功能规则

**FR-CLI-CREATE-01 名称校验**：`-n` 必填，通过 `IsValidName` 校验（1-47 字符，字母/数字/`.`/`:`/`-`/`_`）

**FR-CLI-CREATE-02 容量校验**：`-s` 必填，格式 `<正整数>G` 大写后缀，≥1G，防 stoull 隐式截断与溢出

**FR-CLI-CREATE-03 LBA 校验**：`-l` 可选，精确匹配 `512B` 或 `4K`

**FR-CLI-CREATE-04 namespace 数量校验**：`-m` 可选，正整数，范围 [1, 128]

**FR-CLI-CREATE-05 策略校验**：`-r` 可选，精确匹配 `Linear` 或 `Striped`；缺省 Normal（请求结构默认值），显式 `Striped` 时要求 nsNum≥2，否则返回 ERR_STRIPED_NS_NUM_REQUIRED

**FR-CLI-CREATE-06 IPC 调用**：调用 `UBSE_IPC_SSU_ALLOC_SPACE`（opCode=0x000A），请求体 `UbseCliSsuAllocCreateReq`（lbaFormat 缺省 512B、strategy 缺省 NORMAL），响应体 `UbseCliSsuAllocResult`

**FR-CLI-CREATE-07 输出**：成功输出 7 列详情表；失败输出错误码

##### 4.11.4.4 输出结果

- 成功：`Name: <name> Strategy: <Linear|Striped>` + 7 列表
- 失败：固定错误文案或 `ERROR: Internal error with error code N`

##### 4.11.4.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-CREATE-01 | `create ssu -n test -s 10G` 成功创建，返回详情表 |
| AC-CLI-CREATE-02 | 缺少 `-n` 时，返回 ERR_NAME_REQUIRED |
| AC-CLI-CREATE-03 | 缺少 `-s` 时，返回 ERR_SIZE_REQUIRED |
| AC-CLI-CREATE-04 | `-s 10g`（小写）返回 ERR_INVALID_SIZE |
| AC-CLI-CREATE-05 | `-s 0G` 返回 ERR_INVALID_SIZE（< 1G） |
| AC-CLI-CREATE-06 | `-m 0` 返回 ERR_INVALID_NS_NUM（< 1） |
| AC-CLI-CREATE-07 | `-m 129` 返回 ERR_INVALID_NS_NUM（> 128） |
| AC-CLI-CREATE-08 | `-l 1K` 返回 ERR_INVALID_LBA（非 512B/4K） |
| AC-CLI-CREATE-09 | `-r raid0` 返回 ERR_INVALID_STRATEGY（非 Linear/Striped） |

#### 4.11.5 attach ssu 命令（FR-CLI-ATTACH）

##### 4.11.5.1 需求描述

提供将 SSU namespace 挂载到主机的能力，支持普通空间、线性聚合、条带化聚合三种模式。

##### 4.11.5.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| -n/--name | string | 是 | 1-47 字符 |
| -q/--host_nqn | string | 否 | ≤68 字符 |
| -e/--src_eid | string | 否 | ≤16 字符 |
| -t/--type | string | 否 | 取值 `Linear` 或 `Striped` |
| -d/--dev_name | string | Linear/Striped 必填 | 1-32 字符，字母/数字/`_`/`-`/`.` |
| -l/--level | string | Striped 必填 | 取值 `raid0` 或 `raid5` |
| -c/--chunk_size | string | Striped 必填 | 取值 `4K/16K/32K/64K/128K/256K/512K` |

##### 4.11.5.3 功能规则

**FR-CLI-ATTACH-01 通用校验**：`-n` 必填 + `IsValidName`；`-q`/`-e` 可选但提供时需长度校验

**FR-CLI-ATTACH-02 普通挂载**：未指定 `-t` 时，执行 `HandleAttachSpace`，禁止带 `--dev_name/--level/--chunk_size`

**FR-CLI-ATTACH-03 线性聚合挂载**：`-t Linear` 时，`-d` 必填 + `IsValidDevName`，禁止带 `--level/--chunk_size`

**FR-CLI-ATTACH-04 条带化聚合挂载**：`-t Striped` 时，`-d/-l/-c` 均必填，`-l` 取值 raid0/raid5，`-c` 取值 4K~512K

**FR-CLI-ATTACH-05 IPC 调用**：
  - 普通挂载：`UBSE_IPC_SSU_ATTACH_SPACE`（0x000B）
  - 线性聚合：`UBSE_IPC_SSU_ATTACH_LINEAR_SPACE`（0x000E）
  - 条带化聚合：`UBSE_IPC_SSU_ATTACH_STRIPED_SPACE`（0x0010）
  - 返回 UBSE_ERR_ALREADY_ATTACHED（2001）时视为可接受结果，仍解析响应并输出路径信息（幂等移除后的友好提示）

**FR-CLI-ATTACH-06 输出**：普通挂载输出 `ns_dev_paths: <逗号分隔列表>`；聚合挂载输出 `ns_dev_paths: ...\ndev_path: ...`

##### 4.11.5.4 输出结果

- 成功：plain text 格式的设备路径
- 失败：固定错误文案或错误码

##### 4.11.5.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-ATTACH-01 | `attach ssu -n test` 成功挂载，返回 ns_dev_paths |
| AC-CLI-ATTACH-02 | `attach ssu -t Linear -n test -d dev1` 成功，返回 ns_dev_paths + dev_path |
| AC-CLI-ATTACH-03 | `attach ssu -t Striped -n test -d dev1 -l raid0 -c 4K` 成功 |
| AC-CLI-ATTACH-04 | 未指定 `-t` 但带 `--dev_name`，返回 ERR_ATTACH_AGGREGATION_REQUIRES_TYPE |
| AC-CLI-ATTACH-05 | `-t Linear` 缺少 `-d`，返回 ERR_LINEAR_DEV_NAME_REQUIRED |
| AC-CLI-ATTACH-06 | `-t Striped` 缺少 `-l`，返回 ERR_STRIPED_LEVEL_REQUIRED |
| AC-CLI-ATTACH-07 | `-l raid1` 返回 ERR_INVALID_LEVEL（非 raid0/raid5） |
| AC-CLI-ATTACH-08 | `-c 8K` 返回 ERR_INVALID_CHUNK_SIZE（非 4K~512K） |

#### 4.11.6 detach ssu 命令（FR-CLI-DETACH）

##### 4.11.6.1 需求描述

提供卸载已挂载 SSU namespace 的能力，支持普通空间、线性聚合、条带化聚合三种模式。

##### 4.11.6.2 输入参数

| 参数 | 类型 | 必填 | 约束 |
|------|------|------|------|
| -n/--name | string | 是 | 1-47 字符 |
| -q/--host_nqn | string | 否 | ≤68 字符 |
| -t/--type | string | 否 | 取值 `Linear` 或 `Striped` |
| -d/--dev_name | string | Linear/Striped 必填 | 1-32 字符 |

##### 4.11.6.3 功能规则

**FR-CLI-DETACH-01 通用校验**：`-n` 必填 + `IsValidName`；`-q` 可选但提供时需长度校验

**FR-CLI-DETACH-02 普通卸载**：未指定 `-t` 时，执行 `HandleDetachSpace`，禁止带 `--dev_name`

**FR-CLI-DETACH-03 聚合卸载**：`-t Linear` 或 `Striped` 时，`-d` 必填 + `IsValidDevName`

**FR-CLI-DETACH-04 IPC 调用**：
  - 普通卸载：`UBSE_IPC_SSU_DETACH_SPACE`（0x000C）
  - 线性聚合：`UBSE_IPC_SSU_DETACH_LINEAR_SPACE`（0x000F）
  - 条带化聚合：`UBSE_IPC_SSU_DETACH_STRIPED_SPACE`（0x0011）

**FR-CLI-DETACH-05 隐藏选项**：`srcEid/level/chunkSize` 使用 `SSU_CLI_DETACH_*` 默认值，不对用户暴露

**FR-CLI-DETACH-06 输出**：成功输出空字符串（daemon 返回无响应体）

##### 4.11.6.4 输出结果

- 成功：空字符串（无输出）
- 失败：固定错误文案或错误码

##### 4.11.6.5 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-DETACH-01 | `detach ssu -n test` 成功卸载，无输出 |
| AC-CLI-DETACH-02 | `detach ssu -t Linear -n test -d dev1` 成功卸载 |
| AC-CLI-DETACH-03 | `detach ssu -t Striped -n test -d dev1` 成功卸载 |
| AC-CLI-DETACH-04 | 未指定 `-t` 但带 `--dev_name`，返回 ERR_DETACH_DEV_NAME_REQUIRES_TYPE |
| AC-CLI-DETACH-05 | `-t Linear` 缺少 `-d`，返回 ERR_LINEAR_DEV_NAME_REQUIRED |

#### 4.11.7 输入参数校验（FR-CLI-VALIDATE）

##### 4.11.7.1 需求描述

对所有 CLI 命令的输入参数进行客户端预校验，过滤非法输入，减少无效 IPC 调用。

##### 4.11.7.2 功能规则

**FR-CLI-VALIDATE-01 名称校验**（`IsValidName`）：1-47 字符，仅字母/数字/`.`/`:`/`-`/`_`

**FR-CLI-VALIDATE-02 容量校验**（`ParseSize`）：格式 `<正整数>G` 大写后缀，≥1G，防 stoull 隐式截断与溢出

**FR-CLI-VALIDATE-03 namespace 数量校验**（`ParseNsNum`）：正整数，范围 [1, 128]

**FR-CLI-VALIDATE-04 LBA 校验**（`ParseLba`）：精确匹配 `512B` 或 `4K`

**FR-CLI-VALIDATE-05 策略校验**（`ParseStrategy`）：精确匹配 `Linear` 或 `Striped`

**FR-CLI-VALIDATE-06 设备名校验**（`IsValidDevName`）：1-32 字符，字母/数字/`_`/`-`/`.`

**FR-CLI-VALIDATE-07 RAID 级别校验**（`ParseLevel`）：精确匹配 `raid0` 或 `raid5`

**FR-CLI-VALIDATE-08 chunk 大小校验**（`ParseChunkSize`）：精确匹配 `4K/16K/32K/64K/128K/256K/512K`

**FR-CLI-VALIDATE-09 长度校验**（`IsValidLengthIfPresent`）：可选参数提供时校验最大长度

**FR-CLI-VALIDATE-10 错误反馈**：校验失败返回固定文案（如 `ERR_NAME_REQUIRED`），不进入 IPC 流程

##### 4.11.7.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-VALIDATE-01 | 名称含非法字符（如 `#`），返回 ERR_INVALID_NAME |
| AC-CLI-VALIDATE-02 | 名称超长（49 字符），返回 ERR_INVALID_NAME |
| AC-CLI-VALIDATE-03 | 容量 `10g`（小写）返回 ERR_INVALID_SIZE |
| AC-CLI-VALIDATE-04 | namespace 数 `0` 或 `129` 返回 ERR_INVALID_NS_NUM |

#### 4.11.8 UDS IPC 通信（FR-CLI-IPC）

##### 4.11.8.1 需求描述

CLI 通过 UDS 短连接与 ubse daemon 通信，发送请求并接收响应。

##### 4.11.8.2 功能规则

**FR-CLI-IPC-01 通信入口**：通过 `ubse_invoke_call(moduleCode, opCode, request, response)` 发起 IPC

**FR-CLI-IPC-02 短连接模型**：每次 `ubse_invoke_call` 都重新建立 UDS 连接（connect-send-recv-disconnect）

**FR-CLI-IPC-03 UDS 路径**：默认 `/var/run/ubse/ubse.sock`，可通过 `ubse_socket_path_set` 覆盖

**FR-CLI-IPC-04 请求编码**：`UbseRequestMessage` header 含 `{module_code, op_code, request_data->length}`，body 指向 request_data->buffer

**FR-CLI-IPC-05 同步收发**：`udsClient.Send(request, response)` 同步等待响应，默认超时 1800000ms

**FR-CLI-IPC-06 响应拷贝**：`CopyResponseBody` 用 `malloc` + `memcpy_s` 把 body 复制到出参

**FR-CLI-IPC-07 资源释放**：`UbseCliBufferGuard` RAII 包装响应缓冲，析构时调 `ubse_api_buffer_free`

**FR-CLI-IPC-08 返回值**：返回 `responseMessage.header.statusCode`（daemon 处理结果码），非 0 即错误

**FR-CLI-IPC-09 模板封装**：`InvokeSsuIpc` 重载 4 种场景：有 req 有 rsp、无 req 有 rsp、有 req 无 rsp、原始 buffer

##### 4.11.8.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-IPC-01 | daemon 未启动时，`ubse_invoke_call` 返回连接失败错误码 |
| AC-CLI-IPC-02 | daemon 返回非 0 状态码，CLI 输出 `ERROR: Internal error with error code N` |
| AC-CLI-IPC-03 | 响应缓冲通过 `UbseCliBufferGuard` 自动释放，无内存泄漏 |

#### 4.11.9 wire 序列化与反序列化（FR-CLI-WIRE）

##### 4.11.9.1 需求描述

CLI 与 daemon 之间的请求/响应通过 `UbsePackUtil` 编码为裸 payload（不带 serde 协议头），字段顺序与宽度必须与服务端 `Ssu*Pack/Ssu*Unpack` 严格对应。

##### 4.11.9.2 功能规则

**FR-CLI-WIRE-01 编码工具**：统一使用 `UbsePackUtil` 的 `BuildPayload` 模板：预分配 size → pack → move 到 payload

**FR-CLI-WIRE-02 字符串编码**：`PackString` 同时校验 `StringFits` 与 `UbsePackString(maxLength)`

**FR-CLI-WIRE-03 反序列化校验**：严格校验枚举合法性（`IsValidLbaFormat/IsValidStrategy/IsValidRaidLevel/IsValidChunkSize`）

**FR-CLI-WIRE-04 列表上限**：反序列化时校验列表长度（`SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS`、`SSU_CLI_MAX_DESERIALIZED_HOST_NQNS`、`SSU_CLI_MAX_DESERIALIZED_CONNECT_INFOS`，均为 1<<16），防御不可信响应

**FR-CLI-WIRE-05 双源同步**：CLI 侧 `ubse_cli_ssu_struct.cpp` 的 pack/unpack 与服务端 `ubse_ssu_handler_message.cpp` 的 `Ssu*Pack/Ssu*Unpack` 必须手动保持字段顺序一致

##### 4.11.9.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-WIRE-01 | 请求序列化后的字节流与服务端 `Ssu*Unpack` 期望一致 |
| AC-CLI-WIRE-02 | 响应反序列化失败（字段缺失/枚举非法）时，CLI 返回 ERR_DESERIALIZATION |
| AC-CLI-WIRE-03 | 响应列表超限时，反序列化失败，不导致内存越界 |

#### 4.11.10 输出格式化（FR-CLI-OUTPUT）

##### 4.11.10.1 需求描述

CLI 命令执行结果通过 `UbseCliResultEcho` 抽象输出，支持纯文本与结构化表格两种格式。

##### 4.11.10.2 功能规则

**FR-CLI-OUTPUT-01 抽象基类**：`UbseCliResultEcho` 提供统一的输出接口

**FR-CLI-OUTPUT-02 纯文本**：`UbseCliStringEcho` 用于错误提示、INFO 提示、attach 命令的设备路径输出

**FR-CLI-OUTPUT-03 结构化表格**：`UbseCliVariableCellEcho` 用于 display/create 命令，固定列宽 80

**FR-CLI-OUTPUT-04 摘要表**：3 列（name/size/strategy），size 由该分配下 `nameSpaceList[*].nsSize` 求和后直接展示原始字节值

**FR-CLI-OUTPUT-05 详情表**：7 列（ns_uuid/tgt_eid/tgt_nqn/namespace_id/ns_dev_path/ns_size/lba_format），含表头合并行

**FR-CLI-OUTPUT-06 空结果**：输出 `INFO: No SSU allocation information found.`

**FR-CLI-OUTPUT-07 错误输出**：固定文案（客户端校验错误）；服务端错误中已知错误码（如 1200/1201/1202、2000~2003）输出专属文案，其余输出 `ERROR: Internal error with error code N`

##### 4.11.10.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-OUTPUT-01 | 摘要表列宽固定，表头为 name/size/strategy |
| AC-CLI-OUTPUT-02 | 详情表含表头合并行 `Name: <name> Strategy: <Linear\|Striped>` |
| AC-CLI-OUTPUT-03 | 空结果输出 INFO_EMPTY 提示 |
| AC-CLI-OUTPUT-04 | 服务端错误输出 `ERROR: Internal error with error code N` |

#### 4.11.11 隐式认证（FR-CLI-AUTH）

##### 4.11.11.1 需求描述

CLI 不显式认证，依赖 UDS peercred + daemon 侧 RBAC 完成鉴权。

##### 4.11.11.2 功能规则

**FR-CLI-AUTH-01 无显式认证**：CLI 不发送 token/证书/用户名密码

**FR-CLI-AUTH-02 UDS 权限**：daemon 侧 UDS socket 权限 0660，仅 owner 与 group 可读写

**FR-CLI-AUTH-03 peercred 提取**：daemon 通过 `SO_PEERCRED` 获取对端 uid，不可伪造

**FR-CLI-AUTH-04 用户名解析**：daemon 通过 `GetUserNameById(uid)` 反查用户名，失败返回 `UBSE_ERR_PERMISSION_DENIED`

**FR-CLI-AUTH-05 RBAC 鉴权**：daemon 通过 `CheckPermission(userName, moduleCode, opCode)` 校验权限

**FR-CLI-AUTH-06 内置用户**：内置用户自动授予全部权限

**FR-CLI-AUTH-07 鉴权失败反馈**：CLI 收到错误码 30（未在 `GetSsuErrorMessage` 单列），走 default 输出 `ERROR: Internal error with error code 30`；32/20/50/1003 等已有专属文案

##### 4.11.11.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-AUTH-01 | 非授权用户执行 CLI，daemon 返回 30，CLI 输出错误码 |
| AC-CLI-AUTH-02 | UDS socket 权限为 0660 |
| AC-CLI-AUTH-03 | 内置用户执行任何命令均成功 |

#### 4.11.12 命令覆盖边界（FR-CLI-COVERAGE）

##### 4.11.12.1 需求描述

明确 CLI 当前覆盖的 SSU 操作范围，以及未覆盖的操作（代码反推发现）。

##### 4.11.12.2 功能规则

**FR-CLI-COVERAGE-01 已覆盖**：display（alloc_summary/alloc_detail/connect_info[仅 VM 模式]）、create、delete、attach（Normal/Linear/Striped）、detach（Normal/Linear/Striped）；host 模式 10 个命令变体，VM 模式 9 个（VM 不注册 create/delete）

**FR-CLI-COVERAGE-02 未覆盖（已知缺口）**：
  - `get ns stats`（namespace 统计，opCode=0x0006）
  - `add access permission`（添加访问权限，opCode=0x0008）
  - `remove access permission`（移除访问权限，opCode=0x0009）
  - `get fe device list`（FE 设备列表，opCode=0x0012）
  - `fe device alloc`（FE 设备分配，opCode=0x0013）
  - `fe device free`（FE 设备释放，opCode=0x0014）

##### 4.11.12.3 验收标准

| 编号 | 验收项 |
|------|--------|
| AC-CLI-COVERAGE-01 | host 模式帮助显示 display/create/delete/attach/detach 5 类命令；VM 模式仅显示 display/attach/detach 3 类 |

---

## 5. 非功能需求

### 5.1 性能需求（NFR-PERF）

| 编号 | 需求项 | 指标 |
|------|--------|------|
| NFR-PERF-01 | 单次 alloc 响应时间 | ≤ 3 秒（10 个 namespace 以内） |
| NFR-PERF-02 | 单次 free 响应时间 | ≤ 3 秒（10 个 namespace 以内） |
| NFR-PERF-03 | 单次 attach 响应时间 | ≤ 5 秒（10 个 namespace 以内） |
| NFR-PERF-04 | 设备采集周期 | 30 秒 |
| NFR-PERF-05 | 跨节点 RPC 超时 | 30 秒 |
| NFR-PERF-06 | 并发处理能力 | 支持至少 18 个并发请求（线程池大小） |
| NFR-PERF-07 | 任务队列容量 | 1000（超过后拒绝新请求） |
| NFR-PERF-08 | 调度算法执行时间 | ≤ 100 毫秒（100 个设备以内） |
| NFR-PERF-09 (CLI) | 单命令响应时间 | daemon 不超时场景 ≤ 3 秒（UDS 短连接 + 同步收发） |
| NFR-PERF-10 (CLI) | 命令超时阈值 | 现状 1800000ms（30 分钟，UDS client 默认）；建议缩短到 30 秒（CLI 场景过长） |
| NFR-PERF-11 (CLI) | CLI 启动到退出时间 | ≤ 100ms（无业务逻辑，仅参数解析 + IPC） |
| NFR-PERF-12 (CLI) | 批量场景性能 | 短连接模型，每次重新建立 UDS；建议支持长连接模式（可选） |

### 5.2 可靠性需求（NFR-REL）

| 编号 | 需求项 | 说明 |
|------|--------|------|
| NFR-REL-01 | 进程重启后状态恢复 | master 重启后通过账本重建恢复状态，已分配空间不丢失 |
| NFR-REL-02 | 主备故障切换 | master 故障后 standby 自动接管，VIP 漂移，服务恢复时间 ≤ 30 秒 |
| NFR-REL-03 | 操作幂等性 | 仅 perm（add/remove access permission）保持幂等（重复添加/移除返回成功）；alloc/free/attach/detach 不再幂等，重复调用分别报 UBSE_ERR_ALREADY_ALLOCATED(2000)/UBSE_ERR_NO_NEED_FREE(2002)/UBSE_ERR_ALREADY_ATTACHED(2001)/UBSE_ERR_NO_NEED_DETACH(2003) |
| NFR-REL-04 | 失败回滚 | namespace 创建失败时回滚已创建的 namespace |
| NFR-REL-05 | 部分失败容错 | 释放时部分 namespace 删除失败，保留账本便于重试 |
| NFR-REL-08 | 空列表保护 | 采集到空设备列表时保持旧缓存，防止短暂异常导致状态丢失 |
| NFR-REL-09 (CLI) | CLI 输入校验 | 客户端预校验过滤非法输入，减少无效 IPC 调用 |
| NFR-REL-10 (CLI) | 防御性反序列化 | 校验枚举合法性、列表上限，防御不可信响应 |
| NFR-REL-11 (CLI) | CLI 资源释放 | `UbseCliBufferGuard` RAII 自动释放响应缓冲 |
| NFR-REL-12 (CLI) | CLI 超时保护 | SIGALRM 定时器 + UDS client 超时双重保护 |
| NFR-REL-13 (CLI) | 错误码透传 | daemon 错误码原样回显；已知错误码（1200/1201/1202、2000~2003 等 10 种）已由 `GetSsuErrorMessage` 映射专属文案，其余回显错误码 |
| NFR-REL-14 (CLI) | wire 契约一致性 | CLI 与 handler 手动同步，有双源风险；建议代码生成或共享头文件 |

> *NFR-REL-06（VFE 直通 MTI 重试）、NFR-REL-07（空 VM bus instance 60 秒周期清理）已随 VFE 直通域迁移至 [虚机化特性需求分析文档](virt/SSU模块虚机化特性需求分析文档.md)。*

### 5.3 安全需求（NFR-SEC）

| 编号 | 需求项 | 说明 |
|------|--------|------|
| NFR-SEC-01 | mTLS 双向认证 | HTTP 接入必须使用 mTLS，从证书 CN/OU 提取身份 |
| NFR-SEC-02 | 身份验证 | attach/detach 前必须调用 `VerifyAttachDetachPrecondition` 校验 tenant/userName/requestNodeId，防止跨租户挂载与路径信息泄露 |
| NFR-SEC-03 | 多租户隔离 | 不同租户的存储资源物理隔离，无越权访问 |
| NFR-SEC-05 | SDK 权限控制 | SDK 客户端必须持有 `"ssu"` 权限串才能调用 |
| NFR-SEC-06 | 安全函数 | 使用 `strncpy_s`/`memset_s`/`snprintf_s` 等安全函数防止缓冲区溢出 |
| NFR-SEC-07 (CLI) | UDS socket 权限 | daemon 侧 UDS socket 权限 0660，仅 owner 与 group 可读写，文件系统层强制 |
| NFR-SEC-08 (CLI) | peercred 身份提取 | daemon 通过 SO_PEERCRED 获取对端 uid，不可伪造 |
| NFR-SEC-09 (CLI) | RBAC 鉴权 | daemon 侧统一鉴权，CLI 请求经 `CheckPermission(userName, moduleCode, opCode)` 校验 |
| NFR-SEC-10 (CLI) | 敏感信息保护 | CLI 错误文案不暴露内部细节 |

> 注：CLI 文档的安全函数要求（`memcpy_s`/`strncpy_s`）与 NFR-SEC-06 内容相同，不重复添加。

> *NFR-SEC-04（VFE 直通 upi 校验）已随 VFE 直通域迁移至 [虚机化特性需求分析文档](virt/SSU模块虚机化特性需求分析文档.md)。*

### 5.4 可用性需求（NFR-AVAIL）

| 编号 | 需求项 | 说明 |
|------|--------|------|
| NFR-AVAIL-01 | 多接入方式 | 支持 SDK/HTTP/CLI 三种接入，覆盖不同场景 |
| NFR-AVAIL-02 | 多语言 SDK | 支持 C/Go/Python SDK |
| NFR-AVAIL-03 | RESTful API | HTTP 接口遵循 RESTful 规范，标准 JSON 格式 |
| NFR-AVAIL-04 | 完整文档 | 每个接口有详细的 doxygen 注释，含参数说明与返回值 |
| NFR-AVAIL-05 (CLI) | 独立可执行 | `ubsectl-ssu` 无运行时库依赖 |
| NFR-AVAIL-06 (CLI) | 帮助信息 | `--help` 输出命令清单 |
| NFR-AVAIL-07 (CLI) | 错误文案可读 | 固定文案；已知错误码已区分文案，其余输出错误码（建议进一步细化） |
| NFR-AVAIL-08 (CLI) | 输出格式 | plain text / ASCII table；建议支持 JSON（可选） |

### 5.5 可维护性需求（NFR-MAINT）

| 编号 | 需求项 | 说明 |
|------|--------|------|
| NFR-MAINT-01 | 结构化日志 | 关键字段（name/eid/nsId/requestId/ret）齐全，支持日志聚合 |
| NFR-MAINT-02 | traceId 透传 | 跨节点调用链通过 traceId 串联，便于问题定位 |
| NFR-MAINT-03 | 资源锁日志 | `UbseLoggingLockGuard` 记录锁等待与持有时间，便于排查死锁 |
| NFR-MAINT-04 | 插件化扩展 | 调度器支持通过 `AddFilter` 插入自定义 filter |
| NFR-MAINT-05 | 单元测试覆盖 | 核心子模块（Scheduler/DebtLedger/ReservationMgr/Collector/DirectToVmManager）有 UT 覆盖 |
| NFR-MAINT-06 (CLI) | CLI 代码结构 | 按职责拆分（main/cmd_reg/struct/limits） |
| NFR-MAINT-07 (CLI) | wire 契约维护 | 双源（CLI + handler）手动同步；建议共享头文件或代码生成 |
| NFR-MAINT-08 (CLI) | 错误文案集中 | 客户端校验文案集中在 `cmd_reg.cpp`，服务端错误文案由 `error.cpp` 的 `GetSsuErrorMessage` 集中映射 |
| NFR-MAINT-09 (CLI) | CLI UT 覆盖 | 现有 `test_ubse_cli_ssu_cmd_reg.cpp`，保持并补全 |

### 5.6 可扩展性需求（NFR-SCALE）

| 编号 | 需求项 | 说明 |
|------|--------|------|
| NFR-SCALE-01 | 多 agent 扩展 | 支持 N 个 agent 节点水平扩展 |
| NFR-SCALE-02 | 设备规模 | 支持单节点 100+ SSU 设备 |
| NFR-SCALE-03 | namespace 规模 | 单设备支持最多 128 个 namespace |
| NFR-SCALE-04 | 调度器扩展 | 责任链支持插入自定义 filter，无需修改现有代码 |
| NFR-SCALE-05 (CLI) | CLI 命令扩展 | 通过 `UBSE_CLI_REGISTER_MODULE` 宏注册新模块命令 |
| NFR-SCALE-06 (CLI) | CLI 选项扩展 | 通过 `UbseCliRegBuilder` 链式添加 |
| NFR-SCALE-07 (CLI) | CLI 响应规模 | 防御性常量 `SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS = 1<<16` |

### 5.7 兼容性需求（NFR-COMPAT）

| 编号 | 需求项 | 说明 |
|------|--------|------|
| NFR-COMPAT-01 | 操作系统兼容 | openEuler Linux ARM64 |
| NFR-COMPAT-02 | UUID 库兼容 | 优先使用 libuuid.so.1，不可用时回退到内置随机数生成 |
| NFR-COMPAT-03 | C++ 标准 | C++17 |
| NFR-COMPAT-04 | C 标准 | C11 |
| NFR-COMPAT-05 (CLI) | 编译器版本 | GCC 10+（C++17 要求同 NFR-COMPAT-03，CLI 与 daemon 均适用） |
| NFR-COMPAT-06 (CLI) | daemon 协议兼容 | wire 契约需与 daemon 版本一致；建议版本协商（可选） |
| NFR-COMPAT-07 (CLI) | CLI Framework 依赖 | 依赖 `ubse_cli_framework` 接口稳定 |

> 注：CLI 文档的操作系统兼容（openEuler ARM64）与 C++17 要求分别与 NFR-COMPAT-01/NFR-COMPAT-03 相同，不重复添加。

---

## 6. 约束条件

### 6.1 业务约束

| 编号 | 约束 |
|------|------|
| CON-BIZ-01 | name 最大 47 字符（`SSU_NAME_MAX_LEN=48` 含 '\0'，有效字符数减 1） |
| CON-BIZ-02 | tenant 最大 16 字符 |
| CON-BIZ-03 | STRIPED 分配时 `allocSize` 必须整除 `nsNum` |
| CON-BIZ-04 | STRIPED 分配时 `singleNsSize = allocSize / nsNum` 必须为 `chunkSize` 的整数倍 |
| CON-BIZ-05 | RAID5 聚合至少需要 3 个成员设备 |
| CON-BIZ-06 | LBA 格式仅支持 512B 和 4K |
| CON-BIZ-07 | 单设备最大 namespace 数为 128 |
| CON-BIZ-08 | SSU 使用 PFE=6，其他 PFE 不归 SSU 管理 |
| CON-BIZ-09 (CLI) | host_nqn 最大 68 字符（`SSU_CLI_MAX_HOST_NQN_LENGTH`） |
| CON-BIZ-10 (CLI) | src_eid 最大 16 字符（`SSU_CLI_MAX_SRC_EID_LENGTH`） |
| CON-BIZ-11 (CLI) | dev_name 最大 32 字符（`SSU_CLI_MAX_DEV_NAME_LENGTH`） |
| CON-BIZ-12 (CLI) | CLI 的 namespace 数量参数范围为 [1, 128]（`SSU_CLI_DEFAULT_NS_NUM=1`、`SSU_CLI_MAX_NS_NUM=128`） |
| CON-BIZ-13 (CLI) | 最小分配容量 1GiB（`SSU_CLI_MIN_SIZE_BYTES`） |
| CON-BIZ-14 (CLI) | 容量单位必须大写 G（`ParseSize` 强制大写后缀） |
| CON-BIZ-15 (CLI) | 策略显式指定仅支持 Linear/Striped（`ParseStrategy` 精确匹配），缺省 Normal（请求结构默认值） |
| CON-BIZ-16 (CLI) | RAID 级别仅支持 raid0/raid5（`ParseLevel` 精确匹配） |
| CON-BIZ-17 (CLI) | chunk_size 仅支持 4K~512K（`ParseChunkSize` 精确匹配） |

> 注：CLI 文档的 name 最大长度约束与 CON-BIZ-01 重复、LBA 仅支持 512B/4K 与 CON-BIZ-06 重复，均不重复添加。

### 6.2 技术约束

| 编号 | 约束 |
|------|------|
| CON-TECH-01 | 必须通过 dlopen 加载 libssu.so，不能静态链接 |
| CON-TECH-02 | HTTP 接口必须通过 VIP HTTP Server 暴露，不能直接用普通 HTTP Server |
| CON-TECH-03 | 账本操作只在 master 端进行，agent 端通过 RPC 同步 |
| CON-TECH-04 | collector 仅在 master 端启动 |
| CON-TECH-05 | MTI 调用必须串行执行（单工作者模型） |
| CON-TECH-06 | namespace 的 customData 用于持久化元信息，支持重启恢复 |
| CON-TECH-07 (CLI) | CLI 通过 UDS 短连接通信，`ubse_invoke_call` 每次 connect-send-recv-disconnect |
| CON-TECH-08 (CLI) | UDS 默认路径 `/var/run/ubse/ubse.sock`（`UBSE_UDS_SOCKET_PATH`） |
| CON-TECH-09 (CLI) | wire 协议不带 serde 头，直接 `UbsePackUtil` 裸 payload |
| CON-TECH-10 (CLI) | wire 字段顺序必须与服务端一致（双源同步） |
| CON-TECH-11 (CLI) | SSU 模块码 `UBSE_SSU = 0x000D` |
| CON-TECH-12 (CLI) | CLI 链接选项 `-rdynamic -pie`（供静态注册宏使用） |
| CON-TECH-13 (CLI) | CLI 不显式认证，依赖 UDS peercred + daemon RBAC |
| CON-TECH-14 (CLI) | CLI 输出仅 plain text / ASCII table（CLI Framework 限制，无 JSON 序列化器） |
| CON-TECH-15 (CLI) | UDS client 默认超时 1800000ms |
| CON-TECH-16 (CLI) | CLI 执行期间屏蔽 IPC 日志（`UbseCliDisableIpcLog()`） |

---

## 7. 场景分析

### 7.1 场景一：业务用户申请存储空间

**角色**：业务用户（通过 C SDK）
**前置条件**：SSU 设备已上线，用户已获得 `"ssu"` 权限

**主流程**：
1. 用户调用 `AllocSpace(name="db_data", nsSize=1TB, nsNum=4, lbaFormat=4K, strategy=STRIPED, tenant="tenantA")`
2. 系统校验参数合法性
3. 系统检查账本，确认 `"db_data"` 不存在
4. 系统采集设备视图，过滤出 tenantA 可用且 ONLINE 的设备
5. 系统按 STRIPED 算法选择 4 个设备，每个分配 256GB
6. 系统预扣容量，调用硬件创建 4 个 namespace
7. 系统将分配记录写入账本（state=CREATED）
8. 返回 namespace 设备路径列表

**后置条件**：4 个 namespace 在 `/dev/disk/by-id/` 下可访问，账本中存在 `"db_data"` 条目

**异常分支**：
- 设备不足：返回 `INSUFFICIENT_SPACE` 错误
- name 已存在：返回 `UBSE_ERR_ALREADY_ALLOCATED`（2000，分配不再幂等）
- 创建失败：回滚已创建的 namespace，返回错误

### 7.2 场景二：master 故障切换

**角色**：系统自动
**前置条件**：master 故障，standby 在线

**主流程**：
1. 心跳检测发现 master 故障
2. standby 通过选举提升为新 master
3. VIP 漂移到新 master
4. 新 master 启动 collector 采集设备列表
5. 新 master 调用 `RebuildLedgerFromDevList` 从硬件 namespace 反推重建账本
6. 新 master 注册 HTTP 路由（跟随 VIP）
7. 新 master 恢复服务

**后置条件**：新 master 可继续提供 alloc/free/attach 等服务，已分配空间不丢失

### 7.3 场景三：并发分配同名资源

**角色**：两个 agent 同时发起 alloc 请求
**前置条件**：两个 agent 节点同时收到分配 `"data1"` 的请求

**主流程**：
1. Agent A 发送 ALLOC_REQ 到 master
2. Agent B 发送 ALLOC_REQ 到 master
3. master 收到 Agent A 的请求，账本 Put `"data1"` (state=CREATING)
4. master 收到 Agent B 的请求，账本查重发现 `"data1"` 已存在，返回 `UBSE_ERR_ALREADY_ALLOCATED`（2000）
5. master 异步处理 Agent A 的请求，成功后账本 Modify 为 CREATED
6. master 推送 ALLOC_RESP 给 Agent A（成功）
7. master 推送 ALLOC_RESP 给 Agent B（失败）

**后置条件**：仅 Agent A 成功分配，账本中 `"data1"` 状态为 CREATED

### 7.4 场景四：跨租户隔离

**角色**：租户 A 的用户
**前置条件**：设备 D1 已被租户 A 占用（namespace customData.tenant="A"），设备 D2 未被占用

**主流程**：
1. 租户 A 的用户调用 `AllocSpace(name="data", tenant="A")`
2. 系统过滤设备：D1 (tenant="A", 保留)、D2 (tenant="", 保留)
3. 系统从 D1、D2 中选择设备分配
4. 假设分配到 D2，创建 namespace，customData.tenant="A"

**后置条件**：D2 现在被标记为 tenant="A"，租户 B 的用户无法使用 D2

**异常分支**：
- 租户 B 的用户调用 `AllocSpace(tenant="B")`：D1 和 D2 均被过滤（D1.tenant=A≠B, D2.tenant=A≠B），返回 `INSUFFICIENT_SPACE`

### 7.5 场景五：运维通过 CLI 查询分配概览（CLI 视角，与 7.1 场景对应的命令行操作流程）

**角色**：平台运维
**前置条件**：ubse daemon 已启动，运维用户有查询权限

**主流程**：
1. 运维执行 `ubsectl-ssu display ssu -t alloc_summary`
2. CLI 启动，注入隐式 type `"ssu"`
3. 参数解析，匹配 `display` 命令
4. 调用 `IsValidName`（无 `-n`，跳过）
5. `InvokeSsuIpc` 无 req 有 rsp 模式，opCode=0x0004
6. UDS 连接 daemon，发送请求
7. daemon 鉴权（peercred + RBAC），分发到 SSU handler
8. daemon 返回 `UbseCliSsuAllocListRsp`
9. CLI 反序列化，`BuildSummaryTable` 输出 3 列表
10. CLI 退出

**后置条件**：终端显示分配概览表

**异常分支**：
- daemon 未启动：UDS 连接失败，CLI 输出错误码
- 无分配信息：输出 INFO_EMPTY
- 鉴权失败：daemon 返回 30，CLI 输出 `ERROR: Internal error with error code 30`

### 7.6 场景六：存储管理员通过 CLI 创建空间（CLI 视角，与 7.1 场景对应的命令行操作流程）

**角色**：存储管理员
**前置条件**：daemon 已启动，用户有分配权限

**主流程**：
1. 执行 `ubsectl-ssu create ssu -n test_space -s 10G -m 2 -r Striped`
2. 参数解析，匹配 `create` 命令
3. `IsValidName("test_space")` 通过
4. `ParseSize("10G")` 通过，返回 10GiB
5. `ParseNsNum("2")` 通过
6. `ParseStrategy("Striped")` 通过
7. 序列化 `UbseCliSsuAllocCreateReq`（Striped 且 nsNum=2 满足 ≥2 校验），opCode=0x000A
8. UDS 发送，daemon 分配 namespace
9. daemon 返回 `UbseCliSsuAllocResult`
10. `BuildDetailTable` 输出 7 列表

**异常分支**：
- `-s 10g`（小写）：`ParseSize` 失败，返回 ERR_INVALID_SIZE
- `-m 200`：`ParseNsNum` 失败（>128），返回 ERR_INVALID_NS_NUM
- daemon 分配失败（如容量不足）：返回错误码，CLI 输出 `ERROR: Internal error with error code N`

### 7.7 场景七：虚机编排通过 CLI 挂载条带化聚合空间（CLI 视角）

**角色**：虚机编排系统
**前置条件**：已有分配的 namespace

**主流程**：
1. 执行 `ubsectl-ssu attach ssu -t Striped -n test_space -d dev1 -l raid0 -c 4K -q nqn.2026-01.host`
2. 参数解析，匹配 `attach` 命令
3. `IsValidName`、`IsValidDevName`、`ParseLevel`、`ParseChunkSize`、`IsValidLengthIfPresent` 均通过
4. `HandleAttachStriped` 序列化 `UbseCliSsuAttachStripedReq`，opCode=0x0010
5. UDS 发送，daemon 执行条带化挂载
6. daemon 返回 `UbseCliSsuAttachAggregatedRsp`
7. 输出 `ns_dev_paths: ...\ndev_path: ...`

**异常分支**：
- 缺少 `-l`：`ERR_STRIPED_LEVEL_REQUIRED`
- `-l raid1`：`ERR_INVALID_LEVEL`
- daemon 挂载失败（如 namespace 不存在）：返回错误码

### 7.8 场景八：非授权用户通过 CLI 访问被拒（CLI 视角）

**角色**：普通用户（无 SSU 权限）
**前置条件**：daemon 已启动，用户无 SSU 查询权限

**主流程**：
1. 执行 `ubsectl-ssu display ssu -t alloc_summary`
2. CLI 启动，参数解析通过
3. UDS 连接 daemon
4. daemon 通过 peercred 获取 uid，反查用户名
5. `CheckPermission(userName, UBSE_SSU, 0x0004)` 返回拒绝
6. daemon 返回 `UBSE_ERR_PERMISSION_DENIED`（30）
7. CLI 输出 `ERROR: Internal error with error code 30`

**后置条件**：用户看到错误码 30（但文案不明确，见 12.4 TBD-AUTH-ERROR-MSG）

### 7.9 场景九：CLI 防御性反序列化保护（CLI 视角）

**角色**：运维
**前置条件**：daemon 返回异常响应（如枚举非法）

**主流程**：
1. 执行 `display ssu -t alloc_detail -n test`
2. CLI 发送请求，daemon 返回响应
3. `Deserialize` 校验枚举合法性时发现非法值
4. 返回 ERR_DESERIALIZATION
5. CLI 输出 `ERROR: Deserialization failed in client.`

**异常分支**：
- 列表超限：`SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS` 校验失败，返回错误

---

## 8. 数据模型

### 8.1 核心实体关系

```
┌─────────────┐     1:N     ┌──────────────┐
│  SSU Device │─────────────▶│  Namespace   │
│  (PFE)      │              │              │
└─────┬───────┘              └──────┬───────┘
      │ 1:N                         │ N:1
      ▼                             ▼
┌─────────────┐              ┌──────────────┐
│    VFE      │              │ AllocSpace   │
│             │              │  (账本条目)  │
└──────┬──────┘              └──────────────┘
       │ N:1
       ▼
┌─────────────┐
│ VM BusInst  │
│             │
└─────────────┘
```

### 8.2 关键数据结构

#### 8.2.1 分配请求

```cpp
struct UbseSsuAllocSpaceReq {
    std::string name;                     // 请求标识，有效字符上限 47（缓冲区 48 含 '\0'）
    uint64_t nsSize;                      // 总容量（字节）
    uint32_t nsNum;                       // namespace 数量
    UbseSsuLBAFormat lbaFormat;           // 512B 或 4K
    UbseSsuAllocStrategy strategy;        // STRIPED(0)/LINEAR(1)/NORMAL(2，缺省)
    std::string tenant;                   // 租户隔离标识，最大 16 字符
};
```

#### 8.2.2 账本条目

```cpp
struct UbseSsuLedgerEntry {
    std::string name;                     // 请求标识
    UbseSsuAllocSpaceReq allocReq;        // 原始分配请求
    UbseSsuNsState state;                 // 状态机：IDLE/CREATING/CREATED/ATTACHING/ATTACHED/FAULTED
    UbseSsuAllocResult allocResult;       // 分配结果（含 resourceUuid 与 namespace 列表）
    std::set<std::string> attachNodes;    // NORMAL 策略多节点共挂的节点级对账集合
};
```

#### 8.2.3 FE/VFE 模型

```cpp
struct UbseSsuFe {
    uint8_t slotId, chipId, dieId;
    uint16_t pfeId;                       // SSU 使用 PFE=6
    std::string pfeGuid;
    std::vector<UbseSsuVfe> vfeList;
};

struct UbseSsuVfe {
    uint8_t slotId, chipId, dieId;
    uint16_t pfeId, vfeId;
    std::string vfeGuid;
    std::string bindBusInstanceGuid;      // 绑定的 VM bus instance
};
```

#### 8.2.4 CLI 命令 wire 数据结构（CLI 特有）

CLI 与 daemon 之间的 wire 契约结构（`ubse_cli_ssu_struct.h`），字段顺序必须与服务端 `Ssu*Pack/Ssu*Unpack` 严格一致（见 FR-CLI-WIRE）。

请求结构（均有 `Serialize`）：

```cpp
// 分配详情查询请求
struct UbseCliSsuAllocDetailReq {
    std::string name;
};

// 创建分配请求（nsSize 缺省 1GiB、nsNum 缺省 1、lbaFormat 缺省 512B、strategy 缺省 NORMAL）
struct UbseCliSsuAllocCreateReq {
    std::string name;
    uint64_t nsSize;
    uint32_t nsNum;
    UbseSsuLBAFormat lbaFormat;
    UbseSsuAllocStrategy strategy;
    std::string tenant;
};

// 普通挂载请求
struct UbseCliSsuAttachSpaceReq {
    std::string name;
    std::string hostNqn;
    std::string srcEid;
};

// 线性聚合挂载请求
struct UbseCliSsuAttachLinearReq {
    std::string name;
    std::string hostNqn;
    std::string srcEid;
    std::string devName;
};

// 条带化聚合挂载请求
struct UbseCliSsuAttachStripedReq {
    std::string name;
    std::string hostNqn;
    std::string srcEid;
    std::string devName;
    UbseSsuAggregationRaidLevel level;
    UbseSsuChunkSize chunkSize;
};

// 空间释放请求
struct UbseCliSsuFreeSpaceReq { std::string name; };

// 卸载请求（普通/线性/条带化，srcEid/level/chunkSize 使用隐藏默认值，不对用户暴露）
struct UbseCliSsuDetachSpaceReq { std::string name; std::string hostNqn; std::string srcEid; };
struct UbseCliSsuDetachLinearReq { /* 同 Space + devName */ };
struct UbseCliSsuDetachStripedReq { /* 同 Linear + level + chunkSize(默认值) */ };

// 连接信息查询请求/响应（VM 模式 display -t connect_info 专用）
struct UbseCliSsuConnectInfoReq { std::string name; };
struct UbseCliSsuConnectInfo {
    std::string srcEid; std::string tgtEid; std::string tgtNqn;
    std::string hostNqn; std::string nsUuid; uint32_t nsId;
};
struct UbseCliSsuConnectInfoRsp { std::vector<UbseCliSsuConnectInfo> connectInfoList; };
```

响应结构（均有 `Deserialize`）：

```cpp
// 普通挂载响应
struct UbseCliSsuAttachSpaceRsp {
    std::vector<std::string> nsDevPaths;
};

// 聚合挂载响应
struct UbseCliSsuAttachAggregatedRsp {
    std::vector<std::string> nsDevPaths;
    std::string devPath;
};

// namespace 信息
struct UbseCliSsuNameSpaceInfo {
    std::string nsUuid;
    std::string tgtEid;
    std::string tgtNqn;
    uint32_t namespaceId;
    std::string nsDevPath;
    uint64_t nsSize;
    UbseSsuLBAFormat lbaFormat;
    std::vector<std::string> allowHostNqnList;
};

// 分配结果
struct UbseCliSsuAllocResult {
    std::string name;
    UbseSsuAllocStrategy strategy;
    std::vector<UbseCliSsuNameSpaceInfo> nameSpaceList;
};

// 分配列表响应
struct UbseCliSsuAllocListRsp {
    std::vector<UbseCliSsuAllocResult> allocations;
};
```

### 8.3 状态机

#### 8.3.1 Namespace 状态机

```
IDLE ──▶ CREATING ──▶ CREATED ──▶ ATTACHING ──▶ ATTACHED
                          │            │             │
                          │            ▼             │
                          └──────◀─────┘             │
                          ▲                        │
                          └─────────────────────────┘
                              (detach 成功)

容错转换：CREATED ──▶ ATTACHED (容忍 ATTACHING 通知丢失)

故障维度（collector 周期增量对账维护，独立于主状态流）：
任意状态 ──▶ FAULTED   (空间 NS 连续 3 个采集周期从硬件消失，防抖判定，约 90s)
FAULTED ──▶ 原状态     (硬件重现，collector 对账回正后自动解除)

FAULTED 空间的行为约束：
- attach/detach/状态上报一律拒绝，返回 UBSE_SSU_ERROR_SPACE_FAULTED(1204)
- free 为人工止损出口（硬件缺失时 NS 不可见视为已释放，允许 free）
```

#### 8.3.2 DirectToVmManager 状态机

```
AVAILABLE ──▶ RUNNING_ALLOC ──▶ AVAILABLE
          ──▶ RUNNING_FREE  ──▶ AVAILABLE
          ──▶ CLEARING_BG   ──▶ AVAILABLE
```

---

## 9. 接口清单

### 9.1 业务接口（UbseSsuService）

| 编号 | 接口 | 输入 | 输出 | 幂等 |
|------|------|------|------|------|
| IF-01 | AllocSpace | req, identity | result（含 resourceUuid） | 否（重复分配报 2000） |
| IF-02 | FreeSpace | name, resourceUuid, identity | - | 否（不存在报 2002；HTTP 释放不存在豁免返回 200） |
| IF-03 | AttachSpace | req | nsDevPaths | 否（重复挂载报 2001） |
| IF-04 | DetachSpace | req | - | 否（未挂载报 2003） |
| IF-05 | AttachLinearSpace | req | nsDevPaths, devPath | 否（重复挂载报 2001；ATTACHER_MISMATCH 报 1203） |
| IF-06 | DetachLinearSpace | req | - | 否（未挂载报 2003；ATTACHER_MISMATCH 报 1203） |
| IF-07 | AttachStripedSpace | req | nsDevPaths, devPath | 否（重复挂载报 2001；ATTACHER_MISMATCH 报 1203） |
| IF-08 | DetachStripedSpace | req | - | 否（未挂载报 2003；ATTACHER_MISMATCH 报 1203） |
| IF-09 | AddAccessPermission | name, nqn, identity | - | 是（重复添加返回成功） |
| IF-10 | RemoveAccessPermission | name, nqn, identity | - | 是（不存在返回成功） |
| IF-11 | ListAllocInfo | identity | result[] | - |
| IF-12 | GetAllocInfoByName | name, identity | result | - |
| IF-13 | GetNsStats | name, identity | statsList[] | - |
| IF-14 | GetConnectInfo | name, vfe, identity | connectInfoList[] | - |
| IF-15 | GetFeDeviceList | - | feList[] | - |
| IF-18 | VerifyAttachDetachPrecondition | name, identity, requestNodeId | nsVerifyList[] | - |

> *IF-16/IF-17（FeDeviceAlloc/FeDeviceFree）已随 VFE 直通域迁移至 [虚机化特性需求分析文档](virt/SSU模块虚机化特性需求分析文档.md) §9.2。*

### 9.2 HTTP 路由

| 方法 | URL | 功能 |
|------|-----|------|
| POST | `/ubse/v1/ssu/spaces` | 分配存储空间 |
| DELETE | `/ubse/v1/ssu/spaces/{name}` | 释放存储空间 |

### 9.3 CLI 命令接口

CLI 命令清单（`ubsectl-ssu`，通过 UDS 调用 daemon opCode）：

| 命令 | opCode(hex) | 请求结构 | 响应结构 | 优先级 |
|------|-------------|----------|----------|--------|
| display ssu -t alloc_summary | UBSE_IPC_SSU_LIST_ALLOC_INFO (0x0004) | 无 | UbseCliSsuAllocListRsp | Must |
| display ssu -t alloc_detail -n | UBSE_IPC_SSU_GET_ALLOC_INFO_BY_NAME (0x0005) | UbseCliSsuAllocDetailReq | UbseCliSsuAllocResult | Must |
| display ssu -t connect_info -n（仅 VM 模式） | UBSE_IPC_SSU_GET_CONNECT_INFO (0x0007) | UbseCliSsuConnectInfoReq | UbseCliSsuConnectInfoRsp | Must（VM） |
| create ssu -n -s（仅 host 模式） | UBSE_IPC_SSU_ALLOC_SPACE (0x000A) | UbseCliSsuAllocCreateReq | UbseCliSsuAllocResult | Must（host） |
| delete ssu -n（仅 host 模式） | UBSE_IPC_SSU_FREE_SPACE (0x000D) | UbseCliSsuFreeSpaceReq | 无 | Must（host） |
| attach ssu -n | UBSE_IPC_SSU_ATTACH_SPACE (0x000B) | UbseCliSsuAttachSpaceReq | UbseCliSsuAttachSpaceRsp | Must |
| attach ssu -t Linear -n -d | UBSE_IPC_SSU_ATTACH_LINEAR_SPACE (0x000E) | UbseCliSsuAttachLinearReq | UbseCliSsuAttachAggregatedRsp | Must |
| attach ssu -t Striped -n -d -l -c | UBSE_IPC_SSU_ATTACH_STRIPED_SPACE (0x0010) | UbseCliSsuAttachStripedReq | UbseCliSsuAttachAggregatedRsp | Must |
| detach ssu -n | UBSE_IPC_SSU_DETACH_SPACE (0x000C) | UbseCliSsuDetachSpaceReq | 无 | Must |
| detach ssu -t Linear -n -d | UBSE_IPC_SSU_DETACH_LINEAR_SPACE (0x000F) | UbseCliSsuDetachLinearReq | 无 | Must |
| detach ssu -t Striped -n -d | UBSE_IPC_SSU_DETACH_STRIPED_SPACE (0x0011) | UbseCliSsuDetachStripedReq | 无 | Must |

未覆盖的 SSU 服务接口（已知命令覆盖缺口，对应 FR-CLI-COVERAGE-02）：

| 服务端接口 | opCode | 缺失 CLI 命令 | 建议 |
|-----------|--------|---------------|------|
| GetNsStats | 0x0006 | `display ssu -t ns_stats` | 建议补充 |
| AddAccessPermission | 0x0008 | `add access` | 建议补充 |
| RemoveAccessPermission | 0x0009 | `remove access` | 建议补充 |

> *`display fe`(0x0012)/`create fe`(0x0013)/`free fe`(0x0014) 三条 fe device 命令缺口已随 VFE 直通域迁移至 [虚机化特性需求分析文档](virt/SSU模块虚机化特性需求分析文档.md) §9.3。*

### 9.4 CLI Framework 内部接口

| 接口 | 用途 |
|------|------|
| `UbseCliModuleRegistry::GetInstance()` | 获取模块注册表单例 |
| `UbseCliCallAllModuleSignUp()` | 触发所有模块注册 |
| `UbseCliRegBuilder` | 链式构建命令元信息 |
| `UbseCliValidateStartupConditions` | 启动保护校验 |
| `UbseCliBuildArgs` | 构建 args（注入隐式 type） |
| `UbseCliValidateArgsByWhitelist` | 白名单校验 |
| `UbseCliArgsParse` | 参数解析 |
| `UbseCliStartTimeoutTimer` | 启动超时定时器 |
| `UbseCliExecuteMatchedCommand` | 命令分发 |
| `ubse_invoke_call` | UDS IPC 调用 |
| `UbseCliBufferGuard` | 响应缓冲 RAII |
| `UbseCliStringPromptReply` | 构造纯文本输出 |
| `UbseCliVariableCelReply` | 构造表格输出 |

---

## 10. 风险与对策

| 编号 | 风险 | 影响 | 对策 |
|------|------|------|------|
| RISK-01 | 30s 采集间隔内并发分配导致超分 | 容量分配错误 | 预扣/预释放机制 + pendingOps_ 并发协议 |
| RISK-02 | master 故障导致账本丢失 | 已分配空间无法查询 | 硬件 namespace 持久化 customData，支持账本重建 |
| RISK-03 | CREATING 状态崩溃后残留脏记录 | 阻塞后续同名分配 | Rebuild 不重建 CREATING 条目（硬件视图无对应 namespace） |
| RISK-04 | ATTACHING 通知丢失导致状态不一致 | 账本状态停留在 CREATED | 容忍 CREATED → ATTACHED 直接转换 |
| RISK-07 | 并发 alloc 同名资源 | 重复创建 namespace | 账本 Put 拒绝覆盖 + CREATING 占位 |
| RISK-08 | libuuid 不可用 | UUID 生成失败 | 回退到线程本地 mt19937_64 随机数 |
| RISK-09 | 跨节点 RPC 超时 | agent 请求挂起 | 30s 超时 + 自动清理 master 端账本 |
| RISK-11 | wire 契约双源同步（CLI） | CLI 与 handler 字段顺序不一致，导致解析失败 | 建议共享头文件或代码生成（见 TBD-WIRE-GEN） |
| RISK-12 | 命令超时阈值过长（30 分钟）（CLI） | daemon 卡死时 CLI 长时间无响应 | 建议缩短到 30 秒（CLI 场景） |
| RISK-13 | 错误码不区分内外部（CLI） | 鉴权错误（30）仍走 default 文案，运维难定位 | 已部分实现：`GetSsuErrorMessage` 已区分 10 种错误码文案；建议进一步为 30 单列文案（见 TBD-AUTH-ERROR-MSG） |
| RISK-14 | 输出格式单一（CLI） | 无 JSON，不利于脚本化集成 | 建议增加 JSON 输出选项（可选） |
| RISK-15 | 短连接性能开销（CLI） | 批量场景下重复建连 | 建议支持长连接模式（可选） |
| RISK-16 | 命令覆盖缺口（CLI） | ns stats/perm/fe device 操作无 CLI 入口，运维不便（free ssu、connect info 已落地） | 建议补充缺失命令（见 FR-CLI-COVERAGE-02） |
| RISK-17 | 防御性反序列化失效（CLI） | 不可信响应导致内存越界 | 保持枚举与列表上限校验 |
| RISK-18 | UDS socket 权限配置错误（CLI） | 非授权用户可访问 | 部署时检查 socket 权限 0660 |

> *RISK-05（VM 异常退出致 bus instance 泄漏）、RISK-06（MTI 调用瞬时失败影响 VFE 直通）、RISK-10（DirectToVm 单工作者模型瓶颈）已随 VFE 直通域迁移至 [虚机化特性需求分析文档](virt/SSU模块虚机化特性需求分析文档.md)（分别对应其 RISK-13/RISK-12/RISK-14）。*

---

## 11. 验收标准汇总

### 11.1 功能验收

| 类别 | 验收项数量 | 关键验收项 |
|------|-----------|-----------|
| 分配（FR-ALLOC） | 7 | AC-ALLOC-01 ~ AC-ALLOC-07 |
| 释放（FR-FREE） | 5 | AC-FREE-01 ~ AC-FREE-05 |
| 挂载（FR-ATTACH） | 7 | AC-ATTACH-01 ~ AC-ATTACH-07 |
| 卸载（FR-DETACH） | 3 | AC-DETACH-01 ~ AC-DETACH-03 |
| 权限（FR-PERM） | 4 | AC-PERM-01 ~ AC-PERM-04 |
| 跨节点（FR-DISTRIBUTED） | 6 | AC-DIST-01 ~ AC-DIST-06 |
| 采集（FR-COLLECTOR） | 4 | AC-COLLECT-01 ~ AC-COLLECT-04 |
| 调度（FR-SCHEDULER） | 5 | AC-SCHED-01 ~ AC-SCHED-05 |
| 北向（FR-NORTHBOUND） | 5 | AC-NB-01 ~ AC-NB-05 |
| 查询（FR-QUERY） | 5 | AC-QUERY-01 ~ AC-QUERY-05 |
| CLI 启动与生命周期（FR-CLI-STARTUP） | 4 | AC-CLI-STARTUP-01 ~ AC-CLI-STARTUP-04 |
| CLI 命令注册（FR-CLI-CMD-REG） | 3 | AC-CLI-CMD-REG-01 ~ AC-CLI-CMD-REG-03 |
| CLI display 命令（FR-CLI-DISPLAY） | 6 | AC-CLI-DISPLAY-01 ~ AC-CLI-DISPLAY-06 |
| CLI create 命令（FR-CLI-CREATE） | 9 | AC-CLI-CREATE-01 ~ AC-CLI-CREATE-09 |
| CLI attach 命令（FR-CLI-ATTACH） | 8 | AC-CLI-ATTACH-01 ~ AC-CLI-ATTACH-08 |
| CLI detach 命令（FR-CLI-DETACH） | 5 | AC-CLI-DETACH-01 ~ AC-CLI-DETACH-05 |
| CLI 输入校验（FR-CLI-VALIDATE） | 4 | AC-CLI-VALIDATE-01 ~ AC-CLI-VALIDATE-04 |
| CLI IPC 通信（FR-CLI-IPC） | 3 | AC-CLI-IPC-01 ~ AC-CLI-IPC-03 |
| CLI wire 序列化（FR-CLI-WIRE） | 3 | AC-CLI-WIRE-01 ~ AC-CLI-WIRE-03 |
| CLI 输出格式化（FR-CLI-OUTPUT） | 4 | AC-CLI-OUTPUT-01 ~ AC-CLI-OUTPUT-04 |
| CLI 隐式认证（FR-CLI-AUTH） | 3 | AC-CLI-AUTH-01 ~ AC-CLI-AUTH-03 |
| CLI 命令覆盖边界（FR-CLI-COVERAGE） | 1 | AC-CLI-COVERAGE-01 |
| **合计** | **104** | 模块主体 51 项 + CLI 特性 53 项 |

### 11.2 非功能验收

| 类别 | 关键指标 |
|------|----------|
| 性能 | alloc ≤ 3s，attach ≤ 5s，调度 ≤ 100ms |
| 可靠性 | 重启后状态恢复，故障切换 ≤ 30s |
| 安全 | mTLS 认证，多租户隔离，upi 校验 |
| 可扩展 | 支持 N 个 agent，100+ 设备 |
| CLI 性能 | 单命令 ≤ 3 秒，CLI 启动到退出 ≤ 100ms |
| CLI 可靠性 | 输入校验、防御性反序列化、RAII 资源释放、SIGALRM 超时保护 |
| CLI 安全 | UDS socket 0660、peercred、daemon 侧 RBAC |
| CLI 可用性 | 独立可执行、`--help` 支持 |
| CLI 兼容性 | openEuler ARM64、GCC 10+/C++17、wire 契约与 daemon 版本一致 |

---

## 12. 附录

### 12.1 错误码定义

| 错误码 | 含义 |
|--------|------|
| UBSE_OK | 成功 |
| UBSE_ERROR | 通用错误 |
| UBSE_ERROR_NULLPTR | 空指针 |
| UBSE_ERR_TIMED_OUT | 超时 |
| UBSE_ERR_EXISTED | 资源已存在 |
| UBSE_ERR_NOT_EXIST | 资源不存在 |
| UBSE_ERR_ALLOCATE | 分配失败 |
| UBSE_ERROR_DESERIALIZE_FAILED | 反序列化失败 |
| UBSE_ERR_ALREADY_ALLOCATED (2000) | 空间已分配，重复分配报错（分配不再幂等） |
| UBSE_ERR_ALREADY_ATTACHED (2001) | 空间已挂载，重复挂载报错（挂载不再幂等） |
| UBSE_ERR_NO_NEED_FREE (2002) | 空间不存在或已释放，无需释放报错（释放不再幂等） |
| UBSE_ERR_NO_NEED_DETACH (2003) | 空间已卸载或未挂载，无需卸载报错（卸载不再幂等） |
| UBSE_SSU_ERROR_SPACE_NOT_FOUND (1200) | SSU 空间不存在 |
| UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE (1201) | 空间已挂载，释放前需先卸载 |
| UBSE_SSU_ERROR_STRATEGY_MISMATCH (1202) | 分配策略与挂载/卸载策略不匹配 |
| UBSE_SSU_ERROR_ATTACHER_MISMATCH (1203) | 独占策略空间已被其他节点挂载，当前节点无权操作 |
| UBSE_SSU_ERROR_SPACE_FAULTED (1204) | 空间所在设备故障，attach/detach 被拦截，可 free 止损 |
| UBSE_SSU_ERROR_NAME_DUPLICATED (1208) | 同名多个 resourceUuid（疑似脑裂残留），操作被拒绝 |
| UbseSsuAllocRetCode::INVALID_PARAM | 调度参数非法 |
| UbseSsuAllocRetCode::INSUFFICIENT_SPACE | 可分配空间不足 |
| UbseSsuAllocRetCode::INTERNAL_ERROR | 调度内部错误 |

#### 12.1.1 CLI 客户端固定错误文案（CLI，`ubse_cli_ssu_cmd_reg.cpp` 与 `ubse_cli_ssu_error.cpp`）

客户端校验/提示文案（`ubse_cli_ssu_cmd_reg.cpp`）：

| 错误常量 | 文案 | 触发场景 |
|----------|------|----------|
| ERR_NAME_REQUIRED | `ERROR: The option -n or --name is required.` | 缺少 `-n` |
| ERR_SIZE_REQUIRED | `ERROR: The option -s or --size is required.` | 缺少 `-s` |
| ERR_INVALID_NAME | `ERROR: Invalid name. The value must be 1-47 characters and contain only letters, digits, '.', ':', '-' or '_'.` | 名称格式非法 |
| ERR_INVALID_SIZE | `ERROR: Invalid size. The value must be an integer number of GiB with uppercase G suffix, minimum 1G.` | 容量格式非法 |
| ERR_INVALID_LBA | `ERROR: Invalid lba. The value must be 512B or 4K.` | LBA 非法 |
| ERR_INVALID_STRATEGY | `ERROR: Invalid strategy. The value must be Linear or Striped.` | 策略非法 |
| ERR_INVALID_NS_NUM | `InvalidNsNumError()`（动态拼接区间） | namespace 数非法 |
| ERR_STRIPED_NS_NUM_REQUIRED | `ERROR: Striped strategy requires ns_num to be at least 2.` | Striped 但 nsNum<2 |
| ERR_INVALID_HOST_NQN | `ERROR: Invalid host_nqn. The value must be 1-68 characters.` | host_nqn 非法 |
| ERR_INVALID_SRC_EID | `ERROR: Invalid src_eid. The value must be 1-16 characters.` | src_eid 非法 |
| ERR_INVALID_ATTACH_TYPE | `ERROR: Invalid type. The value must be Linear or Striped.` | attach/detach 的 -t 非法 |
| ERR_INVALID_DISPLAY_TYPE_HOST | `ERROR: Invalid type. The value must be alloc_summary or alloc_detail.` | display -t 非法（host 模式） |
| ERR_INVALID_DISPLAY_TYPE_VM | `ERROR: Invalid type. The value must be alloc_summary, alloc_detail or connect_info.` | display -t 非法（VM 模式） |
| ERR_SUMMARY_REJECTS_NAME | `ERROR: The option -n or --name is only valid when --type is alloc_detail.` | alloc_summary 带 -n |
| ERR_INVALID_DEV_NAME | `ERROR: Invalid dev_name. The value must be 1-32 characters and contain only letters, digits, '_', '-' or '.'.` | dev_name 非法 |
| ERR_INVALID_LEVEL | `ERROR: Invalid level. The value must be raid0 or raid5.` | RAID 级别非法 |
| ERR_INVALID_CHUNK_SIZE | `ERROR: Invalid chunk_size. The value must be 4K, 16K, 32K, 64K, 128K, 256K or 512K.` | chunk_size 非法 |
| ERR_ATTACH_AGGREGATION_REQUIRES_TYPE | `ERROR: The option --dev_name, --level or --chunk_size requires --type Linear or Striped.` | attach 缺 type |
| ERR_DETACH_DEV_NAME_REQUIRES_TYPE | `ERROR: The option --dev_name requires --type Linear or Striped.` | detach 缺 type |
| ERR_STRIPED_ONLY_OPTIONS | `ERROR: The option --level or --chunk_size is only valid when --type is Striped.` | 非条带化带 level/chunk_size |
| ERR_LINEAR_DEV_NAME_REQUIRED | `ERROR: The option -d or --dev_name is required when --type is Linear.` | Linear 缺 dev_name |
| ERR_STRIPED_DEV_NAME_REQUIRED | `ERROR: The option -d or --dev_name is required when --type is Striped.` | Striped 缺 dev_name |
| ERR_STRIPED_LEVEL_REQUIRED | `ERROR: The option -l or --level is required when --type is Striped.` | Striped 缺 level |
| ERR_STRIPED_CHUNK_SIZE_REQUIRED | `ERROR: The option -c or --chunk_size is required when --type is Striped.` | Striped 缺 chunk_size |
| ERR_SERIALIZATION | `ERROR: Serialization failed in client.` | 请求序列化失败 |
| ERR_DESERIALIZATION | `ERROR: Deserialization failed in client.` | 响应反序列化失败 |
| INFO_EMPTY | `INFO: No SSU allocation information found.` | 分配查询结果为空 |
| INFO_EMPTY_CONNECT | `INFO: No SSU connect information found.` | 连接信息查询结果为空 |

服务端错误码文案映射（`ubse_cli_ssu_error.cpp` 的 `GetSsuErrorMessage`）：

| 错误码 | 文案 |
|--------|------|
| UBSE_ERR_INVALID_ARG | `ERROR: The SSU request contains invalid arguments.` |
| UBSE_ERR_ACCESS_DENIED (32) | `ERROR: Access to the SSU allocation is denied.` |
| UBSE_ERR_TIMED_OUT | `ERROR: The SSU operation timed out.` |
| UBSE_ERR_ALREADY_ALLOCATED (2000) | `ERROR: The SSU allocation already exists.` |
| UBSE_ERR_ALREADY_ATTACHED (2001) | `ERROR: The SSU allocation is already attached.` |
| UBSE_ERR_NO_NEED_FREE (2002) | `ERROR: The SSU allocation does not exist or has already been deleted.` |
| UBSE_ERR_NO_NEED_DETACH (2003) | `ERROR: The SSU allocation is already detached or has not been attached.` |
| UBSE_SSU_ERROR_SPACE_NOT_FOUND (1200) | `INFO: The SSU allocation does not exist.` |
| UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE (1201) | `ERROR: The SSU allocation is attached. Detach it before deleting it.` |
| UBSE_SSU_ERROR_STRATEGY_MISMATCH (1202) | `ERROR: The SSU allocation strategy does not match the attach or detach type.` |
| 其余错误码（default） | `ERROR: Internal error with error code N.`（含鉴权 30） |

#### 12.1.2 daemon 返回错误码透传（CLI）

| 错误码 | 含义 |
|--------|------|
| 30 | UBSE_ERR_PERMISSION_DENIED（鉴权失败） |
| 50 | UBSE_ERR_DAEMON_UNREACHABLE（daemon 未就绪） |
| 1003 | UBSE_ERR_AUTH_FAILED（认证失败） |
| 20 | UBSE_ERR_IPC_CONNECTION_FAILED（IPC 连接失败） |
| ≥10000 | 内部错误码 |

> 注：错误码 0（UBSE_OK）与 12.1 上表重复，不重复列出。2000~2003 与 1200/1201/1202 已由 `GetSsuErrorMessage` 映射专属文案（见 12.1.1），30/50/1003/20 走 default 回显错误码。

### 12.2 关键常量

| 常量 | 值 | 说明 |
|------|----|-----|
| DEVICE_COLLECT_INTERVAL_SECONDS | 30 | 设备采集周期 |
| MAX_NS_PER_DEV | 128 | 单设备最大 namespace 数 |
| MAX_TIMEOUT_SECONDS | 30 | RPC future 等待超时 |
| MAX_RETRY_COUNT | 3 | RPC 超时后重试次数 |
| SSU_FE_ID | 6 | SSU 使用的 PFE ID |
| COMMON_RETRY_TIME | 5 | MTI 调用重试次数 |
| SLEEP_TIME | 2 | MTI 调用重试间隔（秒） |
| 线程池大小 | 18 | SSU 控制器任务执行器 |
| 队列容量 | 1000 | SSU 控制器任务队列 |
| SSU_CLI_BYTES_PER_GIB | 1GiB | GiB 字节数（CLI） |
| SSU_CLI_MAX_NAME_LENGTH | 47 | name 最大有效长度，缓冲区 48 含 '\0'（CLI） |
| SSU_CLI_MAX_TENANT_LENGTH | 16 | tenant 最大有效长度，缓冲区 17 含 '\0'（CLI，代码反推补充） |
| SSU_CLI_MAX_HOST_NQN_LENGTH | 68 | host_nqn 最大有效长度，缓冲区 69 含 '\0'（CLI） |
| SSU_CLI_MAX_SRC_EID_LENGTH | 16 | src_eid 最大有效长度，缓冲区 17 含 '\0'（CLI） |
| SSU_CLI_MAX_DEV_NAME_LENGTH | 32 | dev_name 最大有效长度，缓冲区 33 含 '\0'（CLI） |
| SSU_CLI_DEFAULT_NS_NUM | 1 | namespace 默认数量（CLI） |
| SSU_CLI_MAX_NS_NUM | 128 | namespace 最大数量（CLI） |
| SSU_CLI_MIN_SIZE_BYTES | 1GiB | 最小分配容量（CLI） |
| SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS | 1<<16 | 反序列化分配列表上限，防御性（CLI） |
| SSU_CLI_MAX_DESERIALIZED_HOST_NQNS | 1<<16 | 反序列化 hostNqn 列表上限，防御性（CLI，代码反推补充） |
| SSU_CLI_MAX_DESERIALIZED_CONNECT_INFOS | 1<<16 | 反序列化连接信息列表上限，防御性（CLI，代码反推补充） |
| SSU_CLI_DETACH_SRC_EID | "" | detach 默认 srcEid（CLI） |
| SSU_CLI_DETACH_LEVEL | RAID0 | detach 默认 level（CLI） |
| SSU_CLI_DETACH_CHUNK_SIZE | CHUNK_SIZE_4K | detach 默认 chunkSize（CLI） |

### 12.3 相关文档

- [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) —— 详细设计与时序图(含 CLI 特性设计,第 14 章)
- [SSU模块虚机化特性需求分析文档.md](./virt/SSU模块虚机化特性需求分析文档.md) —— VFE 直通域（FR-FE-DEVICE）现有能力规约及相关 NFR/RISK/场景自 V1.3 起迁移至该文档
- [SSU模块测试设计文档.md](./SSU模块测试设计文档.md) —— UT/IT/PT/FUZZ/E2E 测试体系(含 CLI 特性测试设计)
- [UBSE代码Wiki.md](../../architecture/UBSE代码Wiki.md) —— 项目整体架构
- [AGENTS.md](../../../../AGENTS.md) —— 构建与测试指南
- [SSU模块开发指南.md](./SSU模块开发指南.md) —— 开发规范与排查指南

> 注：原 CLI 特性需求分析文档已并入本文档，其详细设计参考指向 [SSU模块特性设计文档.md](./SSU模块特性设计文档.md) 第 14 章与 [SSU模块测试设计文档.md](./SSU模块测试设计文档.md)。

### 12.4 待澄清项（TBD，并入自 CLI 特性文档）

| TBD 编号 | 描述 | 影响范围 |
|----------|------|----------|
| TBD-WIRE-GEN | wire 契约是否改用代码生成或共享头文件，消除双源同步风险 | 通信层 |
| TBD-AUTH-ERROR-MSG | 鉴权错误（30）与内部错误（≥10000）是否区分文案 | 错误处理 |
| TBD-JSON-OUTPUT | 是否增加 JSON 输出格式选项 | 输出格式 |
| TBD-LONG-LINK | 是否支持长连接模式（批量场景优化） | IPC 通信 |
| TBD-CMD-GAP | 是否补充 `get ns stats`/`add access`/`remove access`/fe device 等缺失命令（`delete ssu` 与 `display ssu -t connect_info` 已落地） | 命令覆盖 |
| TBD-TIMEOUT | 命令超时阈值是否缩短到 30 秒（CLI 场景） | 超时控制 |

### 12.5 假设项（ASSUMPTION，并入自 CLI 特性文档）

| ASSUMPTION 编号 | 描述 |
|-----------------|------|
| ASSUMPTION-01 | 假设 ubse daemon 已启动且监听 UDS `/var/run/ubse/ubse.sock` |
| ASSUMPTION-02 | 假设执行用户有 UDS socket 访问权限（属主或属组） |
| ASSUMPTION-03 | 假设 CLI 与 daemon 版本一致（wire 契约兼容） |
| ASSUMPTION-04 | 假设 daemon 侧 SSU handler 已实现所有 opCode 的 Pack/Unpack |

### 12.6 MoSCoW 优先级汇总（CLI 特性需求）

| 优先级 | 需求项 |
|--------|--------|
| Must | FR-CLI-STARTUP、FR-CLI-CMD-REG、FR-CLI-DISPLAY、FR-CLI-CREATE、FR-CLI-ATTACH、FR-CLI-DETACH、FR-CLI-VALIDATE、FR-CLI-IPC、FR-CLI-WIRE、FR-CLI-OUTPUT、FR-CLI-AUTH、FR-CLI-COVERAGE |
| Should | NFR-PERF-09（CLI 单命令响应时间）、NFR-REL-09（CLI 输入校验）、NFR-SEC-07（CLI UDS 权限） |
| Could | TBD-JSON-OUTPUT（JSON 输出）、TBD-LONG-LINK（长连接）、TBD-CMD-GAP（补充命令） |
| Won't | CLI 直接调用 SSU 业务逻辑（必须经 daemon）、显式认证（依赖 UDS peercred） |
