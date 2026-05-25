# RMRS API参考

## UBSRMRSUpdateAntiNode

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSUpdateAntiNode(const std::map<std::string, std::vector<std::string>> &nodeAntiMap);
```

**描述 DESCRIPTION**

更新反亲和性配置。

**参数 Parameters**

| 参数名      | 数据类型                                        | 有效性规格               | 参数类型 | description                                                  |
| ----------- | ----------------------------------------------- | ------------------------ | -------- | ------------------------------------------------------------ |
| nodeAntiMap | std::map<std::string, std::vector\<std::string>> | - 传入节点必须为真实节点 | 入参     | 反亲和性节点映射。要求格式为“std::map<std::string, std::vector\<std::string>>"，其中key代表节点且不能重复，value代表与其反亲和的节点。 |

**返回值 RETURN VALUE**

- 返回0：更新反亲和性成功。
- 返回1：更新反亲和性失败。

**约束 CONSTRAINTS**

- 本节点调用。
- key和value——nodeId：必须是集群内真实存在的节点
- 一次调用必须设置集群中所有节点的反亲和性
- 支持将反亲和性节点设置为本节点
- 更新反亲和性配置输入{key:[value]}，其中key代表节点且不能重复，value代表与其反亲和的节点。key和value不能设置一样。
- 需要输入不重复的rack内全部节点的反亲和性节点列表。

**附注 NOTES**

暂无

## UBSRMRSMemBorrowStrategy

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrowStrategy(const SrcMemoryBorrowParam &outSrcParam, const uint64_t &borrowSize,
                                  MemBorrowStrategyResult &outBorrowStrategyResult);
```

**描述 DESCRIPTION**

内存借用策略，决策从哪些节点借用内存以及借用内存大小，返回决策结果。

**参数 Parameters**

| 参数名                  | 数据类型                | 有效性规格                                                   | 参数类型 | description                  |
| ----------------------- | ----------------------- | ------------------------------------------------------------ | -------- | ---------------------------- |
| outSrcParam             | SrcMemoryBorrowParam    | - srcNid：借入方节点ID <br>- srcSocketId：借入方Socket ID<br>- srcNumaId：借入方NUMA ID <br>- uid：借入方用户uid <br>- username：借入方用户名 | 入参     | 借入节点信息。               |
| borrowSize              | uint64_t                | 如果传入的borrowSize不是obmm.memory.block.size*1024的整数倍，内部则按obmm.memory.block.size*1024对齐取整后再进行借用决策。obmm.memory.block.size参数详情请参见“《BeiMing 26.0.RC1 UBS Engine 用户指南》> 安装 > 服务配置文件 > ubse.conf”。 | 入参     | 需要借入的内存大小。单位：KB |
| outBorrowStrategyResult | MemBorrowStrategyResult | - destNid：借出方节点ID。<br>-  destSocketId：借出方Socket ID。<br/>-   destNumaNum：同一个Socket单次借出的NUMA数量，当前限制为1。<br/>-  destNumaId：借出方NUMA ID。<br/>-  memSize：借用大小，单位为KB。<br/>-  borrowSize：内存借用大小。 | 出参     | 决策结果。                   |

**返回值 RETURN VALUE**

- 返回0：借用策略执行成功。
- 返回1：借用策略执行失败。

**约束 CONSTRAINTS**

- 本节点调用。

- 所有节点的ubse配置项须保持一致。

**附注 NOTES**

暂无

## UBSRMRSMemBorrowExecute

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrowExecute(const SrcMemoryBorrowParam &outSrcParam,
                                 const std::vector<DestMemoryBorrowParam> &outDestParam,
                                 MemBorrowExecuteResult &outBorrowExecuteResult);
```

**描述 DESCRIPTION**

执行内存借用动作，返回执行结果。

**参数 Parameters**

| 参数名                 | 数据类型                           | 有效性规格                                                   | 参数类型 | description                                      |
| ---------------------- | ---------------------------------- | ------------------------------------------------------------ | -------- | ------------------------------------------------ |
| outSrcParam            | SrcMemoryBorrowParam               | - srcNid：借入方节点ID <br>- srcSocketId：借入方Socket ID<br>- srcNumaId：借入方NUMA ID <br>- uid：借入方用户uid <br>- username：借入方用户名 | 入参     | 借入节点信息。                                   |
| outDestParam           | std::vector\<DestMemoryBorrowParam> | - destNid：借出方节点ID。<br/>- destSocketId：借出方Socket ID。<br/>- destNumaNum：同一个Socket单次借出的NUMA数量，当前限制为1。<br/>- destNumaId：借出方NUMA ID，数量[1, 200]。<br/>- memSize：借用大小，单位：KB，需要为2048的正整数倍。 | 入参     | 决策结果。                                       |
| outBorrowExecuteResult | MemborrowExecuteResult             | -  borrowIds：内存描述符数组。<br/>- presentNumaIds：呈现的远端NUMA数组。 | 出参     | 执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

**返回值 RETURN VALUE**

- 返回0：代表借用执行动作集执行成功。
- 返回1：代表借用执行动作集执行失败。

**约束 CONSTRAINTS**

- 本节点调用。

- memSize需要是obmm.memory.block.size的整数倍；

- memSize在碎片场景下不大于4G，否则执行失败；在大虚机场景下无此约束。

- 所有节点的ubse配置项须保持一致。

**附注 NOTES**

暂无

## UBSRMRSMigrateStrategy

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMigrateStrategy(const std::string &borrowInNode, const std::vector<VMPresetParam> &outVmInfoList,const uint64_t &borrowSize, MigrateStrategyResult &outMigrateStrategyResult)
```

**描述 DESCRIPTION**

本接口为封装第三层匀一匀（内存迁移）接口。内存借用之后，第三层策略决策哪些虚拟机迁出多少比例内存到哪些远端NUMA，返回决策结果和评估时间。

**参数 Parameters**

| 参数名                   | 数据类型                   | 有效性规格                                                   | 参数类型 | description                                                  |
| ------------------------ | -------------------------- | ------------------------------------------------------------ | -------- | ------------------------------------------------------------ |
| borrowInNode             | std::string                | - borrowInNode必须为真实存在的节点且为本节点                 | 入参     | 内存借入节点。                                               |
| outVmInfoList            | std::vector\<VMPresetParam> | - destNid：借出方节点ID。<br/>- destSocketId：借出方Socket ID。<br/>- destNumaNum：同一个Socket单次借出的NUMA数量，当前限制为1。<br/>- destNumaId：借出方NUMA ID，数量[1, 200]。<br/>- memSize：借用大小，单位：KB，需要为2048的正整数倍。 | 入参     | 预设迁出最大比例（虚拟机迁出最大比例{进程ID，可迁出最大比例}）。 |
| borrowSize               | std::uint64_t              | - 会向上对2*1024取整。<br>- 不超过3221225472，即大小不超过3T | 入参     | 匀出本地内存大小。单位：KB                                   |
| outMigrateStrategyResult | MigrateStrategyResult      | -  pid：vm对应PID。<br/>- memSize：对应PID迁出大小，指在远端NUMA实际占用（累计迁出）的大小。<br/>- desNumaId：迁移远端NUMA。<br/>- waitingTime：单位为ms。 | 出参     | 内存迁出策略结果{内存迁出策略执行结果，预设迁出最大比例（虚拟机迁出最大比例{进程ID，可迁出最大比例}），内存迁出动作集合预计执行时间}。 |

**返回值 RETURN VALUE**

返回0：表示迁出策略决策成功。

返回1：表示迁出策略决策失败。

**约束 CONSTRAINTS**

- 本节点调用，即borrowInNode为本节点
- borrowInNode的nodeId必须为真实存在的节点

**附注 NOTES**

暂无

## UBSRMRSMigrateExecute

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMigrateExecute(const std::string &borrowInNode, const std::vector<VMMigrateOutParam> &outVmInfoList,const std::uint64_t &waitingTime, const std::vector<std::string> &borrowIds);
```

**描述 DESCRIPTION**

执行内存迁出动作集，在规定时间内完成返回成功，否则超出等待时间，表示失败，执行回退。

**参数 Parameters**

| 参数名        | 数据类型                       | 有效性规格                                                   | 参数类型 | description                  |
| ------------- | ------------------------------ | ------------------------------------------------------------ | -------- | ---------------------------- |
| borrowInNode  | std::string                    | - borrowInNode必须为真实存在的节点且为本节点                 | 入参     | 内存借入节点。               |
| outVmInfoList | std::vector\<VMMigrateOutParam> | - 数量，取值范围：[1,100] <br>- pid：vm PID。<br/>- memSize：实际迁出大小，指在远端NUMA实际占用（累计迁出）的大小，需要为2*1024的整数倍。<br/>- destNumaId：迁移远端NUMA。 | 入参     | 需要借入的内存大小。单位：KB |
| waitingTime   | uint32_t                       | - 10s-3min                                                   | 入参     | 等待时间。单位：ms           |
| borrowIds     | std::vector<\<std::string>>     | -  数量，取值范围：[1, 512]                                  | 出参     | 内存描述符列表。             |

**返回值 RETURN VALUE**

- 返回0：表示借用迁出动作集执行成功。
- 返回1：代表借用迁出动作集执行失败。

**约束 CONSTRAINTS**

- 本节点调用，即borrowInNode为本节点
- borrowInNode的nodeId必须为真实存在的节点
- borrowIds必须是ubse借用账本中真实存在的name借用标识符
- waitingTime：10s-3min

**附注 NOTES**

暂无

## UBSRMRSMemFree

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemFree(const std::string &nodeId);
```

**描述 DESCRIPTION**

调用第三层内存归还策略，第三层决策并执行哪些虚拟机可以迁回内存，腾出远端NUMA内存，返回给第一层可以归还的远端NUMA节点，第一层执行节点间内存归还（整NUMA归还）。

**参数 Parameters**

| 参数名 | 数据类型    | 有效性规格                             | 参数类型 | description    |
| ------ | ----------- | -------------------------------------- | -------- | -------------- |
| nodeId | std::string | - nodeId必须为真实存在的节点且为本节点 | 入参     | 内存借入节点。 |

**返回值 RETURN VALUE**

 *         返回0：成功
 *         返回1：失败通用错误码
 *         返回2：迁移失败-迁移过程中虚机被删除
 *         返回3：迁移失败通用错误码
 *         返回4：内存资源删除失败

**约束 CONSTRAINTS**

- 本节点调用。
- nodeId必须是集群内真实存在的节点

**附注 NOTES**

暂无

## UBSRMRSMemBorrowRollback

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrowRollback(const std::string &borrowInNode, const std::vector<std::string> &borrowIds);
```

**描述 DESCRIPTION**

覆盖场景为借用成功的前提下，迁出失败以及迁出成功但创建虚拟机失败，回滚逻辑为虚拟机远端占用全部迁回，对应borrowIds全部归还。

**参数 Parameters**

| 参数名       | 数据类型                 | 有效性规格                                              | 参数类型 | description  |
| ------------ | ------------------------ | ------------------------------------------------------- | -------- | ------------ |
| borrowInNode | std::string              | - borrowInNode的nodeId必须是集群内真实存在的节点        | 入参     | 回滚节点ID。 |
| borrowIds    | std::vector\<std::string> | - borrowIds必须是ubse借用账本中真实存在的name借用标识符 | 入参     | 借用记录ID。 |

**返回值 RETURN VALUE**

- 返回0：表示回滚成功。
- 返回1：表示回滚失败。

**约束 CONSTRAINTS**

- 本节点调用
- 两个入参不能为空
- 覆盖场景为借用成功前提
- borrowInNode的nodeId必须是集群内真实存在的节点
- borrowIds必须是ubse借用账本中真实存在的name借用标识符

**附注 NOTES**

暂无

## UBSRMRSGetVmInfoListOnNode

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSGetVmInfoListOnNode(std::vector<VmDomainInfo> &outVmDomainInfos);
```

**描述 DESCRIPTION**

内存调度插件提供接口获取本节点所有虚拟机信息。

**参数 Parameters**

| 参数名           | 数据类型                  | 有效性规格                                                   | 参数类型 | description          |
| ---------------- | ------------------------- | ------------------------------------------------------------ | -------- | -------------------- |
| outVmDomainInfos | std::vector\<VmDomainInfo> | - metaData：元信息。<br>- numaInfo：虚机使用numa信息,key为numaId。<br/>- timestamp：时间戳。<br/> | 出参     | 虚拟机字段信息列表。 |

**返回值 RETURN VALUE**

- 返回0：查询成功。
- 返回1：查询失败。

**约束 CONSTRAINTS**

- 本节点调用，返回本节点虚机信息
- 仅支持查询绑定单个本地numa的虚机
- 仅采集状态为RUNNING和PAUSED状态的虚拟机

**附注 NOTES**

暂无

## UBSRMRSGetNumaInfoListOnNode

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSGetNumaInfoListOnNode(std::vector<NumaInfo> &outNumaInfos);
```

**描述 DESCRIPTION**

内存调度插件提供接口获取本节点NUMA信息。

**参数 Parameters**

| 参数名       | 数据类型              | 有效性规格                                                   | 参数类型 | description        |
| ------------ | --------------------- | ------------------------------------------------------------ | -------- | ------------------ |
| outNumaInfos | std::vector\<NumaInfo> | - numaMetaInfo：numa元信息。<br>- numaVmInfo：numa上虚拟机相关信息。 | 出参     | NUMA字段信息列表。 |

**返回值 RETURN VALUE**

- 返回0：表示成功。

- 返回1：表示失败。

**约束 CONSTRAINTS**

- 本节点调用，返回本节点NUMA信息。

**附注 NOTES**

暂无

## UBSRMRSMemBorrow

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrow(const SrcMemoryBorrowParam &srcParam, const std::vector<uint64_t> &borrowSizes,
                          const WaterMark &waterMark, MemBorrowExecuteResult &result);
```

**描述 DESCRIPTION**

超分场景，水线告警触发内存借用，返回内存描述符。

**参数 Parameters**

| 参数名      | 数据类型               | 有效性规格                                                   | 参数类型 | 描述                                             |
| ----------- | ---------------------- | ------------------------------------------------------------ | -------- | ------------------------------------------------ |
| srcParam    | SrcMemoryBorrowParam   | - srcNid：借入方节点ID。<br/>- srcSocketId：借入方Socket ID。<br/>- srcNumaId：借入方NUMA ID。<br/>- uid：借入方用户uid。<br/>- username：借入方用户名。 | 入参     | 借入节点信息。                                   |
| borrowSizes | std::vector<uint64_t>  | - 数量：[1, 256]。<br/>                                      | 入参     | 决策结果。                                       |
| waterMark   | WaterMark              | - highWaterMark：高水线，不超过100，且不低于低水线。<br/>- lowWaterMark：低水线。 | 入参     | 节点上numa水位线信息                             |
| result      | MemBorrowExecuteResult | - borrowIds：内存描述符数组。<br/>- presentNumaIds：呈现的远端NUMA数组。 | 出参     | 执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

**返回值 RETURN VALUE**

- 返回0，表示内存借用成功。
- 返回1，表示借用内存失败。

**约束 CONSTRAINTS**

- 本节点调用。
- 场景为超分场景
- 该接口由virt进行调用，然后将参数透传给ubse。
- 入参borrowSizes数量需为[1,256]
- 能借入大小的总上限和环境、芯片规格有关系

**附注 NOTES**

暂无

## UBSRMRSMemMigrate

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemMigrate(const SrcMemoryBorrowParam &srcParam, const std::vector<VMPresetParam> &vmPresetParams,
                           const MemBorrowExecuteResult &result);
```

**描述 DESCRIPTION**

超分场景，内存借用成功后，决策并迁移虚拟机内存到远端内存。

**参数 Parameters**

| name           | IN/OUT | description                                      |
| -------------- | ------ | ------------------------------------------------ |
| srcParam       | IN     | 借入节点信息。                                   |
| vmPresetParams | IN     | 需要迁移虚拟机内存迁移参数。                     |
| result         | IN     | 执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

| 参数名         | 数据类型                   | 有效性规格                                                   | 参数类型 | 描述                                                     |
| -------------- | -------------------------- | ------------------------------------------------------------ | -------- | -------------------------------------------------------- |
| srcParam       | SrcMemoryBorrowParam       | - srcNid：借入方节点ID。<br/>- srcSocketId：借入方Socket ID。<br/>- srcNumaId：借入方NUMA ID。<br/>- uid：借入方用户uid。<br/>- username：借入方用户名。 | 入参     | 借入节点信息。                                           |
| vmPresetParams | std::vector\<VMPresetParam> | - 虚拟机数量，取值范围：[1, 100]。<br/>- pid：vm对应PID。<br/>- ratio：迁出最大比例，取值范围[0, 100]。 | 入参     | 需要迁移虚拟机内存迁移参数。                             |
| result         | MemborrowExecuteResult     | - borrowIds：内存描述符数组。<br/>- presentNumaIds：呈现的远端NUMA数组。 | 入参     | 超分借用执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

**返回值 RETURN VALUE**

- 返回0：表示内存迁移成功。

- 返回1：表示内存迁移失败。

**约束 CONSTRAINTS**

- 本节点调用。
- 超分场景调用

**附注 NOTES**

暂无

## UBSRMRSMemReturn

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemReturn(const SrcMemoryBorrowParam &srcParam, const std::vector<std::string> &borrowIds,
                          const std::vector<pid_t> &pids);
```

**描述 DESCRIPTION**

超分场景，分配内存失败后回滚或归还内存。

**参数 Parameters**

| 参数名    | 数据类型                 | 有效性规格                                                   | 参数类型 | 描述                       |
| --------- | ------------------------ | ------------------------------------------------------------ | -------- | -------------------------- |
| srcParam  | SrcMemoryBorrowParam     | - srcNid：借入方节点ID。<br/>- srcSocketId：借入方Socket ID。<br/>- srcNumaId：借入方NUMA ID。<br/>- uid：借入方用户uid。<br/>- username：借入方用户名。 | 入参     | 借入节点信息。             |
| borrowIds | std::vector\<std::string> | - 为ubse借用账本中存在的借用标识符name                       | 入参     | 内存描述符数组。           |
| pids      | std::vector<pid_t>       | -                                                            | 入参     | 正在迁移的虚拟机进程列表。 |

**返回值 RETURN VALUE**

- 返回0：表示内存归还成功。

- 返回1：表示内存归还失败。

**约束 CONSTRAINTS**

- 本节点调用
- 超分场景调用

## UBSRMRSPidNumaInfoCollect

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSPidNumaInfoCollect(const SrcMemoryBorrowParam &srcParam, const std::vector<pid_t> &pids,
                                   std::vector<PidInfo> &pidInfos);
```

**描述 DESCRIPTION**

超分场景，容器进程信息查询。

**参数 Parameters**

| 参数名   | 数据类型             | 有效性规格                                                   | 参数类型 | 描述                   |
| -------- | -------------------- | ------------------------------------------------------------ | -------- | ---------------------- |
| srcParam | SrcMemoryBorrowParam | - srcNid：借入方节点ID。<br/>- srcSocketId：借入方Socket ID。<br/>- srcNumaId：借入方NUMA ID。<br/>- uid：借入方用户uid。<br/>- username：借入方用户名。 | 入参     | 借入节点信息。         |
| pids     | std::vector<pid_t>   | - pids集合大小：[1,300]                                      | 入参     | 借入NUMA上的进程列表。 |
| pidInfos | std::vector\<PidInfo> | -  pid：待查询进程PID。<br/>- localUsedMem：进程本地内存使用量。<br/>- localNumaIds：进程绑定本地NUMA情况。<br/>- remoteUsedMem：进程使用远端内存情况。<br/>- remoteNumaId：进程使用远端numaid。 | 出参     | 查询到的进程内存信息。 |

**返回值 RETURN VALUE**

返回0：表示成功。

返回1，表示传入PIDs为空，或传入的PIDs全都不存在，或查询失败

**约束 CONSTRAINTS**

- 本节点调用
- 超分容器场景调用

**附注 NOTES**

暂无

## UBSRMRSSetWaterMark

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSSetWaterMark(const WaterMark &waterMark);
```

**描述 DESCRIPTION**

超分场景，容器进程信息查询。

**参数 Parameters**

| 参数名    | 数据类型  | 有效性规格                                                   | 参数类型 | 描述         |
| --------- | --------- | ------------------------------------------------------------ | -------- | ------------ |
| waterMark | WaterMark | - highWaterMark：高水线，不超过100，且不低于低水线。<br/>- lowWaterMark：低水线。 | 入参     | 注入水线信息 |

**返回值 RETURN VALUE**

- 返回0：表示成功。

- 返回1，表示失败

**约束 CONSTRAINTS**

- 本节点调用
- 每个节点都需设置注入水线
- 超分场景使用

**附注 NOTES**

暂无

## UBSRMRSSetRunMode

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSSetRunMode(const int &runMode);
```

**描述 DESCRIPTION**

设置超分/碎片场景

**参数 Parameters**

| 参数名  | 数据类型 | 有效性规格                                                   | 参数类型 | 描述                |
| ------- | -------- | ------------------------------------------------------------ | -------- | ------------------- |
| runMode | int      | runMode：场景类型。取值范围：0:超分场景。1:内存碎片场景。有效值只有0和1。 | 入参     | 区分超分/碎片场景。 |

**返回值 RETURN VALUE**

返回0：表示成功。

返回1，表示失败

**约束 CONSTRAINTS**

- runMode取值范围：有效值只有0和1。
  - 0:超分场景。
  - 1:内存碎片场景。

- master节点调用

**附注 NOTES**

暂无

## UBSRMRSSmapAddProcessTracking

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapAddProcessTracking(const std::vector<pid_t> &pidVec, const std::vector<uint32_t> &scanTimeVec,
                                  int scanType, const std::optional<std::vector<uint32_t>> &durationVec = std::nullopt);
```

**描述 DESCRIPTION**

通知SMAP添加进程扫描，并设置扫描周期参数。

**参数 Parameters**

| 参数名      | 数据类型              | 有效性规格                                                   | 参数类型 | 描述                                                         |
| ----------- | --------------------- | ------------------------------------------------------------ | -------- | ------------------------------------------------------------ |
| pidVec      | std::vector<pid_t>    | - 必须是目标节点存在的虚拟机PID。<br/>- 取值范围：[1, 100]   | 入参     | 需要添加进程扫描的进程数组                                   |
| scanTimeVec | std::vector<uint32_t> | - 数量：[1,100]<br/>- 值必须为50毫秒的正整数倍，最大值为200毫秒。 | 入参     | 扫描周期数组，50毫秒的倍数，最小值50毫秒，最大值为200毫秒    |
| scanType    | int                   | 标志位。<br/>- 取值范围：[0,2]<br/>- 0：HAM_SCAN，供HAM使用。<br/>- 1：NORMAL_SCAN。<br/>- 2：STATISTIC_SCAN。 | 入参     | 标志位。取值范围：0：HAM_SCAN，供HAM使用。1：NORMAL_SCAN。2：STATISTIC_SCAN, 此模式可以统计特定时长的访存频次信息 |
| durationVec | std::vector<uint32_t> | - 数量:[1,100]<br/>- 取值范围：[1, 300],单位：s              | 入参     | 统计周期数组，在scanType=2时使用，默认值为1s。               |

**返回值 RETURN VALUE**

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

返回值-9：内核态内存申请失败返回-9。

返回值-12：用户态内存申请失败返回-12

**约束 CONSTRAINTS**

- 本节点调用
- 当进程未被SMAP纳管时，可以调用该接口，此时scanType只能传0或者2。

- 当进程已经被SMAP纳管时，须先停止冷热迁移，然后才可以调用该接口，scanType可以传0或1。
- scanType传1的情况为进程已被SMAP纳管，需要从只扫描状态恢复到冷热扫描加迁移状态。

**附注 NOTES**

暂无

## UBSRMRSSmapQueryFreq

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapQueryFreq(const pid_t &pid, std::vector<uint16_t> &dataVec, const uint32_t &lengthIn,
                         uint32_t &lengthOut, const int &dataSource);
```

**描述 DESCRIPTION**

通知SMAP添加进程扫描，并设置扫描周期参数。

**参数 Parameters**

| 参数名     | 数据类型              | 有效性规格                  | 参数类型 | 描述                                                         |
| ---------- | --------------------- | --------------------------- | -------- | ------------------------------------------------------------ |
| pid        | pid_t                 | 目标节点存在的虚拟机PID。   | 入参     | 需要查询冷热信息的虚拟机对应PID。                            |
| dataVec    | std::vector<uint16_t> | 频次信息，取值大于等于0。   | 出参     | 存放冷热信息的数组。                                         |
| lengthIn   | uint32_t              | 大于0.                      | 入参     | 指示data数组的大小，虚拟机内存大小除以2M的数量，或者容器内存大小除以4kb的数量。 |
| lengthOut  | uint32_t              | lengthOut小于等于lengthIn。 | 出参     | 返回实际写入data数组的大小。                                 |
| dataSource | int                   | - 值需为0或1                | 入参     | 标识数据来源。0表示迁移进程的冷热信息。1表示统计扫描进程的冷热信息。 |

**返回值 RETURN VALUE**

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

**约束 CONSTRAINTS**

- 本节点调用
- 当进程未被SMAP纳管时，或未被SMAP添加进程扫描时，不可以调用该接口

- 当进程已经被SMAP纳管时，可以调用该接口，dataSource可以传0。
- 当进程已经被SMAP添加进程扫描时，可以调用该接口，dataSource可以传1。

**附注 NOTES**

暂无

## UBSRMRSSmapRemoveProcessTracking

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapRemoveProcessTracking(const std::vector<pid_t> &pidVec, int flags = 0);
```

**描述 DESCRIPTION**

通知SMAP移除进程扫描

**参数 Parameters**

| 参数名 | 数据类型           | 有效性规格                              | 参数类型 | 描述                         |
| ------ | ------------------ | --------------------------------------- | -------- | ---------------------------- |
| pidVec | std::vector<pid_t> | 目标节点存在的虚拟机PID。数量：[1, 100] | 入参     | 需要移除进程扫描的进程数组。 |
| flags  | int                | -                                       | 入参     | 保留字段。                   |

**返回值 RETURN VALUE**

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

返回值-9：内核态内存申请失败返回-9。

**约束 CONSTRAINTS**

- 本节点调用
- 只有通过OutSmapAddProcessTracking接口设置的scanType为0的PID才能被这个接口移除。

- 只支持虚拟机场景。

**附注 NOTES**

暂无

## UBSRMRSSmapEnableProcessMigrate

**摘要 SYNOPSIS**

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapEnableProcessMigrate(std::vector<pid_t> pidVec, int enable, int flags = 0);
```

**描述 DESCRIPTION**

启用/禁用PID对应虚拟机的冷热迁移和迁回。

**参数 Parameters**

| 参数名 | 数据类型           | 有效性规格    | 参数类型 | 描述             |
| ------ | ------------------ | ------------- | -------- | ---------------- |
| pidVec | std::vector<pid_t> | 数量：[1,100] | 入参     | 虚拟机PID数组。  |
| enable | int                | 值必须为0或1  | 入参     | 0-禁用。1-启用。 |
| flags  | int                | -             | 入参     | 保留字段。       |

**返回值 RETURN VALUE**

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

返回值-110：超时返回-110。

**约束 CONSTRAINTS**

- 本节点调用
- flags为保留字段，暂未使用。

- pidVec的size需为[1,100]

**附注 NOTES**

暂无
