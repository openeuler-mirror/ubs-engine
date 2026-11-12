**状态 (Status):** Draft

**作者 (Authors):** @davidhwang

**创建日期 (Created):** 2026-07-23

**更新日期 (Updated):** 2026-07-23

**相关 Issue/PR:** #TBD

---

# 1. 概述

## 1.1 简介

本提案将 RMRS 超分场景的故障处理逻辑从"以借用记录（numa/memid）为驱动"重构为"以业务 PID 为驱动"的四阶段流水线架构（采集→任务形成→借用决策→并行执行），实现分层化、结构化、支持并行的故障处理能力，同时通过状态持久化保证故障叠加场景下的可恢复性与不扩大故障原则。

## 1.2 动机

当前超分故障处理存在以下痛点：

1. **入口粒度不统一**：容器走 memid 级（`ProcessBorrowOutNodeFaultByMemId`）、虚机走 numa 级（`ProcessBorrowOutNodeFaultMultiNuma`），逻辑分叉多，维护成本高
2. **缺乏全局决策**：每条借用记录独立借用，无法在节点级聚合借用大小，导致借用碎片化
3. **无并行能力**：全串行处理，多借入节点逐一 RPC，故障恢复耗时长
4. **容错不足**：借用成功但迁移失败时，已借内存泄漏（无持久化断点），违反"不扩大故障"原则
5. **无贪心选择**：可借用内存不足时直接失败，无法最大化利用有限资源释放故障 numa

**用户价值**：借出节点故障后，借入节点上的业务（虚机/容器）能更快、更可靠地完成内存迁移恢复，减少业务中断时间。

**不做的后果**：现有架构下，故障处理失败后重试仍会重复借用导致内存泄漏；可借用内存不足时完全无法处理，只能等待人工介入。

## 1.3 目标

**目标**：
- 以业务 PID 粒度为统一入口，归一化容器与虚机的超分故障处理流程
- 引入节点级聚合借用决策 + 贪心选择，最大化利用可借用资源
- 集群内，节点级并行执行，缩短故障恢复时间
- 节点内，内存借用聚合执行，内存迁移业务级并行执行，故障内存归还并行处理，缩短故障恢复时间
- 通过状态持久化实现断点恢复，保证可恢复性与不扩大故障

**非目标（边界）**：
- 不处理多借出节点同时故障的场景
- 不修改原有 ByMemId/MultiNuma 路径的任何逻辑
- 不修改碎片场景（mem_fragment）的任何代码

# 2. 用例分析

## 2.1 覆盖场景

| 业务类型 | 迁移对象 | NUMA绑定 | 同Socket约束 | 远端NUMA数 |
|----------|----------|----------|-------------|-----------|
| 容器超分-绑定numa | 容器内一组PID | BIND_SINGLE | **有** | 单远端 |
| 容器超分-非绑定numa | 容器内一组PID | BIND_MULTIPLE | **无** | 单远端 |
| 虚机超分-单远端 | 虚机PID(单个) | BIND_SINGLE | **有** | 单远端 |
| 虚机超分-多远端 | 虚机PID(单个) | BIND_SINGLE | **有** | 多远端 |

## 2.2 远端NUMA呈现机制

**关键机制**: 同一借出节点的同一平面（socket）生成的远端内存，在借入节点呈现为**同一个 numaId**。

- 虚机"单本地多远端"中的多个远端 numa 实际来自**不同借出节点**
- 当前仅支持单借出节点故障 → 单个 PID 在单个故障借出节点上**只涉及一个远端 numa**
- 任务形成时无需考虑单 PID 跨多个故障 numa 的聚合问题

## 2.3 同 Socket 约束

| 场景 | 约束 | 原因 |
|------|------|------|
| 容器 BIND_SINGLE（单本地单远端） | 借用numa与借出numa必须同socket | NUMA亲和性 |
| 容器 BIND_MULTIPLE（多本地单远端） | **无同socket限制** | 多本地numa可跨socket |
| 虚机（单/多远端） | 同socket | 虚机NUMA亲和性 |

## 2.4 可借用范围

- **无同socket约束**: 整个集群所有可借用节点（排除已故障、不可达、已借入节点（借用不能成环））的所有可用numa
- **有同socket约束**: 整个集群所有可借用节点（排除同上）中与故障numa同socket的可用numa

## 2.5 故障叠加场景

| 叠加场景 | 受影响操作 | 预期行为 |
|----------|-----------|----------|
| 借出故障 + 借入ubse故障 | RPC通信、借用、归还 | 快速失败→下次重试 |
| 借出故障 + 借入ubturbo故障 | SMAP迁移控制 | 借用成功→迁移失败→保留numa→下次RESUME |
| 借出故障 + 多借入节点部分故障 | 部分节点RPC | 各节点独立并行→部分成功 |

## 2.6 DFX要求

- **可靠性**: 任何步骤失败后下次重试从断点继续；ubse/ubturbo恢复后下次处理应成功
- **可维护性**: 四阶段分层清晰，新路径独立开发，配置开关切换
- **可测试性**: 各阶段可独立UT；故障叠加场景可模拟
- **兼容性**: 新增RPC Opcode，不修改旧Opcode；配置默认关闭

# 3. 方案设计

## 3.1 总体方案

### 3.1.1 核心思想

```
旧模型: 账本记录 → 按numa/memid分组 → 逐组借用+迁移+归还
新模型: 账本记录 → 定位受影响PID → PID粒度形成task → 聚合决策借用 → 并行执行迁移
```

**关键洞察**：
- 每个业务 PID 在故障 numa 上的占用 = **天然迁移大小**
- 聚合所有 PID 在故障 numa 上的总占用 = **天然借用大小**
- 可借用内存不足时，贪心选择可处理的 task 组合

### 3.1.2 四阶段流水线

```
┌─────────────────────────────────────────────────────────────────────┐
│          HandleOverCommitNodeFault (统一入口)                         │
│   前置: DetermineNodeTypeOverCommit → 非BORROW_OUT直接返回OK         │
│   配置: pid_fault_handle_enabled == true 时进入新路径                 │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│  Phase 1: 采集 (Collect)                                            │
│  1a. 查账本 → 获取受影响借入节点 + 故障numa列表                       │
│  1b. 加载持久化历史状态（断点恢复）                                    │
│  1c. RPC到借入节点 → 查询故障numa上的PID列表及内存分布                 │
│  1d. 场景识别 + 同socket约束标记                                      │
│  1e. 借入节点可达性探测（快速失败）                                    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│  Phase 2: 任务形成 (Task Formation)                                  │
│  2a. 每个PID(容器为PID组)在故障numa上的占用 → 形成一个MigrationTask    │
│  2b. 故障numa无使用 → 标记为DirectReturn（直接归还，不借用）           │
│  2c. 按借入节点聚合task → 计算单节点总借用需求                         │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│  Phase 3: 借用决策 (Borrow Decision)                                 │
│  3a. 查询集群可借用内存（排除故障/不可达/已借入节点）                   │
│  3b. 可借用 >= 总需求 → 全部task可处理                                │
│  3c. 可借用 < 总需求 → 贪心(按占用从大到小)选择task子集                │
│  3d. 生成执行计划列表 (NEW/RESUME/DEFER/DIRECT_RETURN)                │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│  Phase 4: 执行 (Execute, 按借入节点并行)                              │
│  4a. std::async + RPC下发执行计划到借入节点                            │
│  4b. 借入节点本地: 禁用冷热流动→借用→迁移→归还→恢复冷热流动            │
│  4c. 每步即时持久化状态                                               │
│  4d. 支持部分成功，失败部分等下次重试                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.1.3 执行位置划分

| 角色 | 职责 |
|------|------|
| **RMRS（借出节点）** | 全局决策：查账本、聚合task、贪心选择、生成执行计划 |
| **借入节点RMRS** | 本地执行：接收RPC → 借用 → 迁移 → 归还 |

### 3.1.4 配置切换

```cpp
MpResult OverCommitFaultNodeModule::ProcessBorrowOutNodeFault(const std::string& nodeId)
{
    if (MpConfiguration::GetInstance().GetPidFaultHandleEnabled()) {
        return ProcessBorrowOutNodeFaultByPid(nodeId);  // 新路径
    }
    // 原有路径: 保持不变
    if (VIRTUAL_SCENE && MultiNuma) {
        return ProcessBorrowOutNodeFaultMultiNuma(nodeId);
    } else {
        return ProcessBorrowOutNodeFaultByMemId(nodeId);
    }
}
```

## 3.2 技术选型

### 被否决方案

| 方案 | 描述 | 否决原因 |
|------|------|----------|
| **V3策略模式包装** | 保留numa/memid入口，用策略模式封装 | 无法引入全局借用决策和贪心选择，仅是代码组织优化 |
| **复用碎片FaultNodeModule基类** | 继承碎片三阶段流水线 | 超分与碎片数据来源差异大（账本vs借用记录），超分有PID级处理需求 |
| **仅并行化不重构** | 保持if/else，内部加std::async | 无法解决缺乏全局决策和容错不足的根本问题 |

### 选择PID粒度的理由

1. PID占用是天然迁移大小 → 无需额外计算
2. 聚合PID占用是天然借用大小 → 节点级决策自然形成
3. 容器/虚机统一为"一组PID在故障numa上的占用" → 消除场景分叉
4. 贪心选择以task为单位 → 资源不足时最大化释放故障numa

## 3.3 功能与性能设计

### 3.3.1 Phase 1: 采集

**账本获取**:
- 调用 `UbseGetNumaMemDebtInfoWithNode(faultNodeId, debtInfos)`
- 可信度校验: `UBSE_OK`完全可信；`PAR_SUCCESS`使用可用部分；其他重试30次
- 提取: 受影响借入节点集合、故障numa集合、按借入节点分组的借用记录

**PID信息查询（RPC到借入节点）**:
- 新增RPC: `OPCODE_OVER_COMMIT_FAULT_PID_QUERY`
- 虚机场景: 使用 `Exporter::GetVmInfoImmediately` 获取全量信息
- 容器场景: 从 cgroup 获取 containerId→PIDs 映射，再查询numa内存分布

**可达性探测**:
- 目的: 快速失败，减少无意义超时等待
- 非绝对保证: 执行层仍需强容错

### 3.3.2 Phase 2: 任务形成

```cpp
struct MigrationTask {
    std::string taskId;               // "{borrowInNodeId}_{pid/containerId}_{faultNumaId}"
    std::vector<pid_t> pids;          // 容器: 一组PID; 虚机: 单个PID
    std::string containerId;          // 容器ID（容器场景）
    uint16_t faultNumaId;             // 故障远端numa
    uint64_t migrationSizeKB;         // 在故障numa上的总占用 = 天然迁移大小
    uint16_t localNumaId;
    int16_t socketId;
    bool hasSameSocketConstraint;
    NumaBindType bindType;
    std::vector<std::string> relatedBorrowIds;  // 关联借用记录（用于归还）
    TaskPhase phase;                  // NONE/BORROWED/MIGRATED/COMPLETED
};
```

**形成规则**:
- 容器: 按containerId聚合PID → 同一容器形成一个task（不可拆分）
- 虚机: 每个PID独立形成一个task
- 故障numa无PID使用 → DirectReturn（直接归还，不借用）

### 3.3.3 Phase 3: 借用决策

**可借用内存查询（新增专用接口）**:

```cpp
MpResult QueryAvailableBorrowMemory(
    const std::string& borrowInNodeId,  // 借入节点（需排除，防止成环）
    bool hasSameSocketConstraint,
    int16_t faultNumaSocketId,
    uint64_t& availableBorrowSizeKB);
```

实现:
1. 通过 `UbseGetAllNodeNumaInfo` 获取集群所有节点numa信息
2. 排除不可借用节点: 已故障、不可达、已借入（借用不能成环）
3. 有同socket约束: 仅统计可借用节点中与故障numa同socket的可用numa
4. 无同socket约束(BIND_MULTIPLE): 统计可借用节点的所有可用numa

**贪心算法**:
- 按 migrationSizeKB 从大到小排序
- 依次纳入直到超过可借用上限
- 未纳入的task留待下次重试
- 目标: 单次最大化释放故障numa内存

### 3.3.4 Phase 4: 执行

**并行框架**: 按借入节点 `std::async` 并行（不同节点完全独立）

**单节点执行编排（RPC到借入节点本地执行）**:

```
Step 1: 禁用冷热流动 → SmapEnableProcessMigrateHelper(pids, disable)
Step 2: 借用新内存   → UbseMemNumaCreateWithCandidate (黑盒，借入节点自行获取结果)
        成功 → 持久化 BORROWED
Step 3: 迁移业务     → SmapMigratePidToRemoteNuma(pid, faultNuma→newNuma, size)
        成功 → 持久化 MIGRATED
        失败 → 保留已借numa，下次RESUME
Step 4: 归还故障numa → MemFreeWithOps(oldBorrowId)
        成功 → 持久化 COMPLETED
Step 5: 恢复冷热流动 → SmapEnableProcessMigrateHelper(pids, enable)
```

**RESUME恢复**:
- BORROWED → 跳过借用，直接迁移
- MIGRATED → 跳过借用和迁移，直接归还

### 3.3.5 状态持久化

```cpp
struct TaskPersistState {
    std::string taskId;
    std::string borrowInNodeId;
    TaskPhase phase;                    // BORROWED/MIGRATED/COMPLETED
    std::string newBorrowId;
    uint16_t newRemoteNumaId;
    std::vector<std::string> oldBorrowIds;
    std::unordered_set<std::string> freedOldBorrowIds;
    std::vector<pid_t> pids;
    uint64_t migrationSizeKB;
};
```

存储: `UbseStoragePutData("mempooling", "_oc_fault_pid_state", &buffer)`

### 3.3.6 账本不可信处理

| 矛盾情况 | 采信策略 |
|----------|----------|
| 账本有记录但PID查询无进程 | 采信PID查询 → DirectReturn |
| 账本无记录但持久化有BORROWED | 采信持久化 → RESUME归还 |
| 持久化有BORROWED但borrowId已不存在 | 已被其他路径归还 → 清除状态 |

## 3.4 安全隐私与DFX设计

### 可靠性

| 中断点 | 持久化状态 | 恢复行为 | 不扩大故障保证 |
|--------|-----------|----------|---------------|
| 借用前 | NONE | 重新借用 | 无副作用 |
| 借用成功、迁移前 | BORROWED | 复用已借NUMA | 不重复借用 |
| 迁移成功、归还前 | MIGRATED | 直接归还 | 不重复迁移 |
| 归还部分完成 | MIGRATED + freedIds | 仅归还未释放的 | 不重复释放 |

### 兼容性

- 新增RPC Opcode，不修改旧Opcode
- 配置开关默认关闭，不影响现有部署
- 原有 ByMemId/MultiNuma 代码零修改

### 可维护性

- 四阶段分层，职责清晰
- 新代码独立目录 `pid_fault/`
- 配置切换支持灰度验证和快速回退

### 可测试性

- 各Phase可独立UT
- 贪心算法纯函数，易测试
- 故障叠加通过mock RPC/SMAP模拟

## 3.5 编程与调用设计

### 3.5.1 编程模型基本设计

本重构为 RMRS 内部逻辑重构，不涉及外部开发者集成调用。变更仅影响：
- RMRS 插件内部故障处理流程
- 新增内部 RPC Opcode（借出节点 → 借入节点）
- 新增配置项 `pid_fault_handle_enabled`

### 3.5.2 接口定义与设计

#### 3.5.2.1 OPCODE_OVER_COMMIT_FAULT_PID_QUERY

* 接口描述：查询借入节点上指定故障numa的PID列表及内存分布
* 接口原型：RPC (借出节点 → 借入节点)
* 输入参数：

  | 参数名称 | 输入/输出 | 类型 | 描述 | 取值范围 |
  |----------|----------|------|------|----------|
  | faultNumaIds | 输入 | vector\<uint16_t\> | 故障远端numa列表 | 有效numaId |

* 返回参数：

  | 参数名称 | 类型 | 描述 | 取值范围 |
  |----------|------|------|----------|
  | pidMemDistribution | vector\<PidMemInfo\> | PID内存分布列表 | - |
  | retCode | uint32_t | 返回码 | 0=成功 |

#### 3.5.2.2 OPCODE_OVER_COMMIT_FAULT_PID_EXECUTE

* 接口描述：下发执行计划到借入节点本地执行（借用→迁移→归还）
* 接口原型：RPC (借出节点 → 借入节点)
* 输入参数：

  | 参数名称 | 输入/输出 | 类型 | 描述 | 取值范围 |
  |----------|----------|------|------|----------|
  | faultNodeId | 输入 | string | 故障借出节点ID | - |
  | tasks | 输入 | vector\<MigrationTask\> | 待迁移task列表 | - |
  | totalBorrowSizeKB | 输入 | uint64_t | 总借用大小 | >0 |
  | hasSameSocketConstraint | 输入 | bool | 是否有同socket约束 | - |
  | faultNumaSocketId | 输入 | int16_t | 故障numa的socketId | - |
  | resumeFromPhase | 输入 | TaskPhase | 恢复起始阶段 | NONE/BORROWED/MIGRATED |
  | existingBorrowId | 输入 | string | RESUME时已借用ID | - |

* 返回参数：

  | 参数名称 | 类型 | 描述 | 取值范围 |
  |----------|------|------|----------|
  | retCode | uint32_t | 返回码 | 0=成功 |
  | completedPhase | TaskPhase | 实际完成阶段 | - |
  | newBorrowId | string | 新借用ID | - |
  | newRemoteNumaId | uint16_t | 新远端numa | - |
  | freedBorrowIds | vector\<string\> | 已归还的旧borrowId | - |

#### 3.5.2.3 QueryAvailableBorrowMemory

* 接口描述：查询集群范围内指定约束下的可借用内存总量
* 接口原型：`MpResult QueryAvailableBorrowMemory(borrowInNodeId, hasSameSocketConstraint, faultNumaSocketId, &availableBorrowSizeKB)`
* 约束说明：排除已故障、不可达、已借入节点（防止成环）

# 4. 缺点和风险

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|----------|
| **PID查询能力补齐** | 中 | 中 | 虚机用GetVmInfoImmediately，容器用cgroup；按需新增专用接口 |
| **并发安全** | 高 | 低 | 不同借入节点完全独立无共享状态；持久化Store内部mutex |
| **状态持久化写入中断** | 高 | 低 | UbseFs内置原子写入(write-tmp+fsync+rename) |
| **贪心非最优** | 低 | 中 | 贪心是近似最优；未处理task下次重试可继续 |
| **RPC兼容性** | 中 | 低 | 仅新增Opcode不修改旧的；配置关闭时走旧路径 |
| **新路径bug影响** | 高 | 低 | 配置默认关闭；灰度验证；原有逻辑零修改可即时回退 |

**实现成本**: 新增约7个文件（~2000行），修改约5个文件（~30行改动）。

# 5. 现有技术

| 参考 | 借鉴点 | 差异 |
|------|--------|------|
| 碎片场景 `ProcessBorrowOutNodeFaultParallel` | 三阶段流水线、std::async并行、部分成功 | 碎片有精确借用决策（指定节点+NUMA），超分是黑盒借用 |
| Simplified路径 `ProcessSimplifiedFaultPids` | PID粒度处理、PendingMigration重试 | Simplified仅用于裸机Redis，无贪心/聚合决策 |
| V3方案（策略模式） | 状态持久化、可达性探测、故障叠加容错 | V4在决策层做本质增强（贪心+聚合） |

# 6. 未解决问题

无。所有设计决策已确认（详见附录）。

---

附录

## A. 已确认设计决策

| # | 问题 | 结论 |
|---|------|------|
| 1 | PID查询能力 | 虚机用 `Exporter::GetVmInfoImmediately`；容器从 cgroup 获取 containerId→PIDs |
| 2 | 可借用内存查询 | 新增专用接口，兼容BIND_MULTIPLE；排除故障/不可达/已借入节点 |
| 3 | 贪心粒度 | 容器以task（容器整体）为粒度，不可拆分 |
| 4 | 多远端虚机 | 同一平面→同一numaId；单借出节点故障→单PID只涉及一个远端numa |
| 5 | 借用接口 | `UbseMemNumaCreateWithCandidate`（黑盒），借入节点自行获取结果 |
| 6 | 切换策略 | 全新开发，配置开关切换，不改变原有逻辑 |

## B. 关键文件变更

**新增**:
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_context.h`
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_collector.h/.cpp`
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_task_builder.h/.cpp`
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_decision.h/.cpp`
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_executor.h/.cpp`
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_state_store.h/.cpp`
- `over_commit/fault_management/pid_fault/over_commit_pid_fault_handler.h/.cpp`

**修改（微量）**:
- `common/event/event_handler.cpp` (~5行)
- `over_commit/fault_management/over_commit_fault_node_module.cpp` (~10行)
- `common/config/mp_configuration.h/.cpp` (新增配置项)
- `over_commit/fault_management/over_commit_fault_management_handler.cpp` (注册Handler)
- `mem_fragment/manager/mem_manager.cpp` (DataReloadInit)

## C. 术语表

| 术语 | 说明 |
|------|------|
| 超分 | 内存超分（Over-Commit），借入节点使用超过本地物理内存的远端内存 |
| 借出节点 | 提供内存的节点（故障节点） |
| 借入节点 | 使用远端内存的节点（需执行迁移） |
| 远端numa | 借出节点内存在借入节点上呈现的虚拟numa |
| SMAP | 内存迁移控制组件（ubturbo侧） |
| 账本 | UBSE内存控制器记录的借用关系 |
| BIND_SINGLE | 绑定单numa模式 |
| BIND_MULTIPLE | 绑定多numa模式（无同socket约束） |
| DirectReturn | 故障numa无使用时直接归还，不需要借用新内存 |
| RESUME | 从上次中断的断点恢复执行 |

## D. 文档更新计划

| 版本 | 日期 | 变更 |
|------|------|------|
| V1.0 | 2026-07-23 | 初稿，完成全部设计 |
