# UBS RMRS开发者指南

## 源码编译构建指导

用户态工具编译cli_client、cli_server、smap_client，共享对象文件libsmap.so

**根目录**

```shell
sh build.sh
```

**参数说明**

| 参数名                     | 有效性参数                 | 参数类型     | 描述                                                         |
| -------------------------- | -------------------------- | ------------ | ------------------------------------------------------------ |
| `-ub`                      | 无                         | 开关参数     | 启用 UB 环境变量设置 (`ub_environment="ON"`)                 |
| `-h`, `--help`             | 无                         | 帮助参数     | 显示帮助信息并退出                                           |
| `-D`, `--debug`            | 无                         | 模式参数     | 设置构建类型为 Debug                                         |
| `-T`, `--type`             | 必须指定类型值             | 模式参数     | 设置构建类型（如 Debug、Release）                            |
| `-t`, `--target`           | 必须指定目标值             | 构建目标参数 | 设置构建目标（如具体组件名称）                               |
| `-C`, `--coverage`         | 无                         | 开关参数     | 启用代码覆盖率功能 (`enable_coverage='ON'`)                  |
| `-H`, `--http-server`      | 无                         | 开关参数     | 启用 HTTP 服务功能 (`enable_http_server='ON'`)               |
| `-c`, `--clean`            | 无                         | 开关参数     | 清理构建结果 (`enable_clean='ON'`)                           |
| `-S`, `--source-compiling` | 无                         | 开关参数     | 启用源代码编译模式 (`enable_source_compiling='ON'`)          |
| `-j`, `--jobs`             | 必须指定线程数值           | 并行参数     | 指定构建时的并行 job 数量                                    |
| `--std`                    | 必须指定标准版本值         | 编译参数     | 设置 C++ 标准版本（如 11、14、17）                           |
| `-v`, `--verbose`          | 无                         | 日志参数     | 启用详细输出（设置环境变量 `VERBOSE=1`）                     |
| `-V`, `--deploy-version`   | 必须指定版本号             | 版本参数     | 设置部署版本号（如 2.0.0.B099）                              |
| `--skip-run-tests`         | 无                         | 开关参数     | 跳过运行测试阶段                                             |
| `--asan`                   | 无                         | 开关参数     | 启用 AddressSanitizer (`enable_asan='ON'`)                   |
| `--lsan`                   | 无                         | 开关参数     | 启用 LeakSanitizer (`enable_lsan='ON'`)                      |
| `--tsan`                   | 无                         | 开关参数     | 启用 ThreadSanitizer (`enable_tsan='ON'`)                    |
| `--ubsan`                  | 无                         | 开关参数     | 启用 UndefinedBehaviorSanitizer (`enable_ubsan='ON'`)        |
| `--ninja`                  | 无                         | 构建工具参数 | 使用 Ninja 构建系统 (`generator='Ninja'`)                    |
| `--make`                   | 无                         | 构建工具参数 | 使用 Unix Makefiles 构建系统 (`generator='Unix Makefiles'`)  |
| `--`                       | 后续为传递参数             | 特殊标志     | 表示后续参数为传递给其它命令的参数（保存在 `trans_params` 中） |
| `*`                        | 条件处理（默认目标或错误） | 位置参数     | 未识别的参数，若未设置 trans_flag，则视为构建目标（`build_target`） |

**约束和注意事项**
产物目录`cmake-build-release/lib`，包含产物librmrs.so

# API接口

## UBSRMRSUpdateAntiNode: 更新反亲和性配置

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSUpdateAntiNode(const std::map<std::string, std::vector<std::string>> &nodeAntiMap);
```

### 描述 DESCRIPTION

更新反亲和性配置。

### 参数 Parameters

| name        | IN/OUT | description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| nodeAntiMap | IN     | 反亲和性节点映射。要求格式为`std::map<std::string, std::vector<std::string>>`，其中key代表节点且不能重复，value代表与其反亲和的节点。 |
| configKey   | IN     | 配置项名称。必须是在plugin_<plugin_name>.conf中存在的配置项名称。否则返回错误1。配置文件中，1<= configKey长度 <= 256。 |
| configValue | OUT    | 配置项的值。执行成功时，configValue即为获取到的配置项的值。执行失败时，configValue不具有任何意义。配置文件中，1<= configValue长度 <= 256。 |

### 返回值 RETURN VALUE

返回0：更新反亲和性成功。

返回1：更新反亲和性失败。

### 约束 CONSTRAINTS

- 本节点调用。
- 更新反亲和性配置输入{key:[value]}，其中key代表节点且不能重复，value代表与其反亲和的节点。key和value不能设置一样。
- 需要输入不重复的rack内全部节点的反亲和性节点列表。

### 附注 NOTES

暂无

## UBSRMRSMemBorrowStrategy: 内存借用策略

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrowStrategy(const SrcMemoryBorrowParam &outSrcParam, const uint64_t &borrowSize,
                                  MemBorrowStrategyResult &outBorrowStrategyResult);
```

### 描述 DESCRIPTION

内存借用策略，决策从哪些节点借用内存以及借用内存大小，返回决策结果。

### 参数 Parameters

| name                    | IN/OUT | description                                          |
| ----------------------- | ------ | ---------------------------------------------------- |
| outSrcParam             | IN     | 借入节点信息。                                       |
| borrowSize              | IN     | 需要借入的内存大小，需要为128*1024的整数倍。单位：KB |
| outBorrowStrategyResult | OUT    | 决策结果。                                           |

### 返回值 RETURN VALUE

返回0：借用策略执行成功。

返回1：借用策略执行失败。

### 约束 CONSTRAINTS

- 本节点调用。

- 所有节点的ubse配置项须保持一致。

- borrowSize需要为128*1024的整数倍，单位：KB。

### 附注 NOTES

暂无

## UBSRMRSBatchBorrowStrategy: 批量内存借用策略

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSBatchBorrowStrategy(const BatchSrcMemoryBorrowParam &outSrcParam, 
                                    const uint64_t &borrowSize,
                                    std::vector<MemBorrowStrategyResult> &outBorrowStrategyResult,
                                    BorrowStrategy borrowStrategy);
```

### 描述 DESCRIPTION

批量内存借用策略，决策从哪些节点借用内存以及借用内存大小，返回决策结果数组。支持多个NUMA同时借用，根据借用策略将总借用大小分配到各个NUMA。

### 参数 Parameters

| name | IN/OUT | description |
|------|--------|-------------|
| outSrcParam | IN | 批量借入节点信息。包含srcNid（借入方节点Id）、srcNumaNum（借入方NUMA数量）、srcNumaId（借入方NUMA Id数组）、uid（借入方用户uid）、username（借入方用户名） |
| borrowSize | IN | 总借用大小，需要为blockSize的整数倍。单位：KB |
| outBorrowStrategyResult | OUT | 决策结果数组。每个NUMA对应一个MemBorrowStrategyResult，包含srcParam（借入方信息）、borrowSize（该NUMA的借用大小）、destParam（借出方信息数组） |
| borrowStrategy | IN | 借用策略。目前支持AVERAGE（平均分配策略） |

### 返回值 RETURN VALUE

返回0：批量借用策略执行成功。

返回1：批量借用策略执行失败。

### 约束 CONSTRAINTS

- 本节点调用。

- 所有节点的ubse配置项须保持一致。

- borrowSize需要为blockSize的整数倍，单位：KB。

- srcNumaId数组不能为空，且每个NUMA Id必须 >= 0，唯一，存在于本地节点。

- 单节点借用上限为1TB，单socket借用上限为512GB。

### 附注 NOTES

暂无

## UBSRMRSMemBorrowExecute: 内存借用执行

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrowExecute(const SrcMemoryBorrowParam &outSrcParam,
                                 const std::vector<DestMemoryBorrowParam> &outDestParam,
                                 MemBorrowExecuteResult &outBorrowExecuteResult);
```

### 描述 DESCRIPTION

执行内存借用动作，返回执行结果。

### 参数 Parameters

| name                   | IN/OUT | description                                      |
| ---------------------- | ------ | ------------------------------------------------ |
| outSrcParam            | IN     | 借入节点信息。                                   |
| outDestParam           | IN     | 决策结果。                                       |
| outBorrowExecuteResult | OUT    | 执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

### 返回值 RETURN VALUE

返回0：代表借用执行动作集执行成功。

返回1：代表借用执行动作集执行失败。

### 约束 CONSTRAINTS

- 本节点调用。

- memSize需要是128M的整数倍，且不大于4G，否则执行失败。

- 所有节点的ubse配置项须保持一致。

### 附注 NOTES

暂无

## UBSRMRSMigrateStrategy: 内存迁出策略

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMigrateStrategy(const std::string &borrowInNode, const std::vector<VMPresetParam> &outVmInfoList,const uint64_t &borrowSize, MigrateStrategyResult &outMigrateStrategyResult)
```

### 描述 DESCRIPTION

本接口为封装第三层匀一匀（内存迁移）接口。内存借用之后，第三层策略决策哪些虚拟机迁出多少比例内存到哪些远端NUMA，返回决策结果和评估时间。

### 参数 Parameters

| name                     | IN/OUT | description                                                  |
| ------------------------ | ------ | ------------------------------------------------------------ |
| borrowInNode             | IN     | 内存借入节点。                                               |
| outVmInfoList            | IN     | 预设迁出最大比例（虚拟机迁出最大比例{进程ID，可迁出最大比例}）。 |
| borrowSize               | IN     | 匀出本地内存大小,会向上对2*1024取整。单位：KB                |
| outMigrateStrategyResult | OUT    | 内存迁出策略结果{内存迁出策略执行结果，预设迁出最大比例（虚拟机迁出最大比例{进程ID，可迁出最大比例}），内存迁出动作集合预计执行时间}。 |

### 返回值 RETURN VALUE

返回0：表示迁出策略决策成功。

返回1：表示迁出策略决策失败。

### 约束 CONSTRAINTS

- 本节点调用。

### 附注 NOTES

暂无

## UBSRMRSMigrateExecute: 内存迁出执行

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMigrateExecute(const std::string &borrowInNode, const std::vector<VMMigrateOutParam> &outVmInfoList,const std::uint64_t &waitingTime, const std::vector<std::string> &borrowIds);
```

### 描述 DESCRIPTION

执行内存迁出动作集，在规定时间内完成返回成功，否则超出等待时间，表示失败，执行回退。

### 参数 Parameters

| name          | IN/OUT | description                  |
| ------------- | ------ | ---------------------------- |
| borrowInNode  | IN     | 内存借入节点。               |
| outVmInfoList | IN     | 需要借入的内存大小。单位：KB |
| waitingTime   | IN     | 等待时间。单位：ms           |
| borrowIds     | IN     | 内存描述符列表。             |

### 返回值 RETURN VALUE

返回0：表示借用迁出动作集执行成功。

返回1：代表借用迁出动作集执行失败。

### 约束 CONSTRAINTS

- 本节点调用。

### 附注 NOTES

暂无

## UBSRMRSMemFree: 内存归还策略与执行

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemFree(const std::string &nodeId);
```

### 描述 DESCRIPTION

调用第三层内存归还策略，第三层决策并执行哪些虚拟机可以迁回内存，腾出远端NUMA内存，返回给第一层可以归还的远端NUMA节点，第一层执行节点间内存归还（整NUMA归还）。

### 参数 Parameters

| name   | IN/OUT | description    |
| ------ | ------ | -------------- |
| nodeId | IN     | 内存借入节点。 |

### 返回值 RETURN VALUE todo待确认

 *         返回0：成功
 *         返回1：失败通用错误码
 *         返回2：迁移失败-迁移过程中虚机被删除
 *         返回3：迁移失败通用错误码
 *         返回4：内存资源删除失败

### 约束 CONSTRAINTS

本节点调用。

### 附注 NOTES

暂无

## UBSRMRSMemBorrowRollback: 借用内存回滚

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrowRollback(const std::string &borrowInNode, const std::vector<std::string> &borrowIds);
```

### 描述 DESCRIPTION

覆盖场景为借用成功的前提下，迁出失败以及迁出成功但创建虚拟机失败，回滚逻辑为虚拟机远端占用全部迁回，对应borrowIds全部归还。

### 参数 Parameters

| name         | IN/OUT | description  |
| ------------ | ------ | ------------ |
| borrowInNode | IN     | 回滚节点ID。 |
| borrowIds    | IN     | 借用记录ID。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1：表示失败。

### 约束 CONSTRAINTS

- 本节点调用
- 两个入参不能为空，覆盖场景为借用成功前提

### 附注 NOTES

暂无

## UBSRMRSGetVmInfoListOnNode: 查询本节点所有虚机信息

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSGetVmInfoListOnNode(std::vector<VmDomainInfo> &outVmDomainInfos);
```

### 描述 DESCRIPTION

内存调度插件提供接口获取本节点所有虚拟机信息。

### 参数 Parameters

| name             | IN/OUT | description          |
| ---------------- | ------ | -------------------- |
| outVmDomainInfos | OUT    | 虚拟机字段信息列表。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1：表示失败。

### 约束 CONSTRAINTS

- 本节点调用，返回本节点虚机信息。
- 仅支持两种规格虚拟机：
  - 规格一：绑定一本地NUMA的虚拟机
  - 规格二：绑定一本地NUMA一远端NUMA的虚拟机
- 仅采集状态为RUNNING和PAUSED状态的虚拟机

## 附注 NOTES

暂无

## UBSRMRSGetNumaInfoListOnNode: 查询本节点所有NUMA信息

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSGetNumaInfoListOnNode(std::vector<NumaInfo> &outNumaInfos);
```

### 描述 DESCRIPTION

内存调度插件提供接口获取本节点NUMA信息。

### 参数 Parameters

| name         | IN/OUT | description        |
| ------------ | ------ | ------------------ |
| outNumaInfos | OUT    | NUMA字段信息列表。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1：表示失败。

### 约束 CONSTRAINTS

- 本节点调用，返回本节点NUMA信息。

### 附注 NOTES

暂无

## UBSRMRSMemBorrow: 超分内存借用

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemBorrow(const SrcMemoryBorrowParam &srcParam, const std::vector<uint64_t> &borrowSizes,
                          const WaterMark &waterMark, MemBorrowExecuteResult &result);
```

### 描述 DESCRIPTION

超分场景，水线告警触发内存借用，返回内存描述符。

### 参数 Parameters

| name        | IN/OUT | description                                      |
| ----------- | ------ | ------------------------------------------------ |
| srcParam    | IN     | 借入节点信息。                                   |
| borrowSizes | IN     | 借用大小集合。                                   |
| waterMark   | IN     | 当前节点水线信息                                 |
| result      | OUT    | 执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1：表示失败。

### 约束 CONSTRAINTS

- 任意节点调用。
- 场景为超分场景
- 该接口由virt进行调用，然后将参数透传给ubse。
- 入参borrowSizes数量需为[1,256]
- 能借入大小的总上限和环境、芯片规格有关系

### 附注 NOTES

暂无

## UBSRMRSMemMigrate: 超分内存分配（迁移）

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemMigrate(const SrcMemoryBorrowParam &srcParam, const std::vector<VMPresetParam> &vmPresetParams,
                           const MemBorrowExecuteResult &result);
```

### 描述 DESCRIPTION

超分场景，内存借用成功后，决策并迁移虚拟机内存到远端内存。

### 参数 Parameters

| name           | IN/OUT | description                                      |
| -------------- | ------ | ------------------------------------------------ |
| srcParam       | IN     | 借入节点信息。                                   |
| vmPresetParams | IN     | 需要迁移虚拟机内存迁移参数。                     |
| result         | IN     | 执行结果（内存描述符数组，呈现的远端NUMA数组）。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1：表示失败。

### 约束 CONSTRAINTS

- 本节点调用。
- 超分场景调用
- vmPresetParams：集合大小(size)表示虚拟机数量，取值范围：[1, 100]。
- vmPresetParams：ratio表示迁出最大比例，取值范围[0, 100]。

### 附注 NOTES

暂无

## UBSRMRSMemReturn: 超分归还请求

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSMemReturn(const SrcMemoryBorrowParam &srcParam, const std::vector<std::string> &borrowIds,
                          const std::vector<pid_t> &pids);
```

### 描述 DESCRIPTION

超分场景，分配内存失败后回滚或归还内存。

## 参数 Parameters

| name        | IN/OUT | description                |
| ----------- | ------ | -------------------------- |
| srcParam    | IN     | 借入节点信息。             |
| borrowIds   | IN     | 内存描述符数组。           |
| migratePids | IN     | 正在迁移的虚拟机进程列表。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1：表示失败。

### 约束 CONSTRAINTS

- 本节点调用
- 超分场景调用

## UBSRMRSPidNumaInfoCollect: 容器numa信息采集

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSPidNumaInfoCollect(const SrcMemoryBorrowParam &srcParam, const std::vector<pid_t> &pids,
                                   std::vector<PidInfo> &pidInfos);
```

### 描述 DESCRIPTION

超分场景，容器进程信息查询。

### 参数 Parameters

| name     | IN/OUT | description            |
| -------- | ------ | ---------------------- |
| srcParam | IN     | 借入节点信息。         |
| pids     | IN     | 借入NUMA上进程列表。   |
| pidInfos | OUT    | 查询到的进程内存信息。 |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1，表示传入PIDs为空，或传入的PIDs全都不存在，或查询失败

### 约束 CONSTRAINTS

- 本节点调用
- 超分容器场景调用
- pids取值范围[1,300]

### 附注 NOTES

暂无

## UBSRMRSSetWaterMark: 超分场景注入水线

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSSetWaterMark(const WaterMark &waterMark);
```

### 描述 DESCRIPTION

超分场景，容器进程信息查询。

### 参数 Parameters

| name      | IN/OUT | description |
| --------- | ------ | ----------- |
| waterMark | IN     | 水线信息    |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1，表示失败

### 约束 CONSTRAINTS

- 本节点调用
- 每个节点都需设置注入水线
- 超分场景使用
- highWaterMark：高水线，取值范围(0,100]。
- lowWaterMark：低水线，取值范围[0, highWaterMark)。

### 附注 NOTES

暂无

## UBSRMRSSetRunMode: 设置超分/碎片场景

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSSetRunMode(const int &runMode);
```

### 描述 DESCRIPTION

设置超分/碎片场景

### 参数 Parameters

| name    | IN/OUT | description |
| ------- | ------ | ----------- |
| runMode | IN     | 场景类型    |

### 返回值 RETURN VALUE

返回0：表示成功。

返回1，表示失败

### 约束 CONSTRAINTS

- runMode取值范围：有效值只有0和1。
  - 0:超分场景。
  - 1:内存碎片场景。

- master节点调用

### 附注 NOTES

暂无

## UBSRMRSSmapAddProcessTracking: 通知SMAP添加进程扫描，并设置扫描周期参数

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapAddProcessTracking(const std::vector<pid_t> &pidVec, const std::vector<uint32_t> &scanTimeVec,
                                  int scanType, const std::optional<std::vector<uint32_t>> &durationVec = std::nullopt);
```

### 描述 DESCRIPTION

通知SMAP添加进程扫描，并设置扫描周期参数。

### 参数 Parameters

| name        | IN/OUT | description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| pidVec      | IN     | 需要添加进程扫描的进程数组                                   |
| scanTimeVec | IN     | 扫描周期数组，50毫秒的倍数，最小值50毫秒，最大值为200毫秒    |
| scanType    | IN     | 标志位，0：HAM_SCAN，供HAM使用。1：NORMAL_SCAN。2：STATISTIC_SCAN, 此模式可以统计特定时长的访存频次信息 |
| durationVec | IN     | 统计周期数组，有效值{1~300}，单位s，在scanType=2时使用，默认值为1s |

### 返回值 RETURN VALUE

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

返回值-9：内核态内存申请失败返回-9。

返回值-12：用户态内存申请失败返回-12

### 约束 CONSTRAINTS

- 本节点调用
- 当进程未被SMAP纳管时，可以调用该接口，此时scanType只能传0或者2。

- 当进程已经被SMAP纳管时，须先停止冷热迁移，然后才可以调用该接口，scanType可以传0或1。
- scanType传1的情况为进程已被SMAP纳管，需要从只扫描状态恢复到冷热扫描加迁移状态。

### 附注 NOTES

暂无

## UBSRMRSSmapQueryFreq: 查询虚拟机冷热信息

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapQueryFreq(const pid_t &pid, std::vector<uint16_t> &dataVec, const uint32_t &lengthIn,
                         uint32_t &lengthOut, const int &dataSource);
```

### 描述 DESCRIPTION

通知SMAP添加进程扫描，并设置扫描周期参数。

### 参数 Parameters

| name       | IN/OUT | description                                                  |
| ---------- | ------ | ------------------------------------------------------------ |
| pid        | IN     | 需要查询冷热信息的虚拟机对应PID。                            |
| dataVec    | IN     | 存放冷热信息的数组。                                         |
| lengthIn   | IN     | 指示data数组的大小，虚拟机内存大小除以2M的数量。             |
| lengthOut  | IN     | 返回实际写入data数组的大小。                                 |
| dataSource | OUT    | 标识数据来源。0表示迁移进程的冷热信息。1表示统计扫描进程的冷热信息。 |

### 返回值 RETURN VALUE

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

### 约束 CONSTRAINTS

- 本节点调用
- 当进程未被SMAP纳管时，或未被SMAP添加进程扫描时，不可以调用该接口

- 当进程已经被SMAP纳管时，可以调用该接口，dataSource可以传0。
- 当进程已经被SMAP添加进程扫描时，可以调用该接口，dataSource可以传1。

### 附注 NOTES

暂无


## UBSRMRSSmapEnableProcessMigrateGrouped: 分组启用进程冷热迁移

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

uint32_t UBSRMRSSmapEnableProcessMigrateGrouped(pid_t pid, const std::vector<PageSwapPair> &pageSwapPairs);
```

### 描述 DESCRIPTION

分组启用进程冷热迁移，每个PageSwapPair映射为一个MigrationGroup，支持多组本地NUMA与远端NUMA配对配置。

### 参数 PARAMETERS

| name          | IN/OUT | description                                          |
| ------------- | ------ | ---------------------------------------------------- |
| pid           | IN     | 进程PID                                              |
| pageSwapPairs | IN     | 页交换配对数组，每个PageSwapPair包含localNumas和remoteNumas |

**PageSwapPair结构**：
| field       | type                  | description                      |
| ----------- | --------------------- | -------------------------------- |
| localNumas  | vector\<NumaQuota\>   | 本地NUMA配额数组，大小范围[1, 4] |
| remoteNumas | vector\<NumaQuota\>   | 远端NUMA配额数组，大小范围[1, 18] |

**NumaQuota结构**：
| field  | type     | description        |
| ------ | -------- | ------------------ |
| numaId | uint32_t | NUMA节点ID         |
| quota  | uint32_t | 配额，单位KB       |

### 返回值 RETURN VALUE

返回值0：表示成功

返回非0：表示失败

### 约束 CONSTRAINTS

- 本节点调用
- pageSwapPairs数组大小范围[1, 8]（MAX_MIGRATION_GROUP_NUM）
- 每个PageSwapPair的localNumas数组大小范围[1, 4]（MAX_GROUP_LOCAL_NUMA）
- 每个PageSwapPair的remoteNumas数组大小范围[1, 18]（REMOTE_NUMA_NUM）
- NUMA配额需通过VM numatune XML限制校验（MpVmQuotaUtil::ValidateNumaQuota）

### 附注 NOTES

暂无


## UBSRMRSSmapRemoveProcessTracking: 通知SMAP移除进程扫描

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapRemoveProcessTracking(const std::vector<pid_t> &pidVec, int flags = 0);
```

### 描述 DESCRIPTION

通知SMAP移除进程扫描

### 参数 Parameters

| name   | IN/OUT | description                                    |
| ------ | ------ | ---------------------------------------------- |
| pidVec | IN     | 需要移除进程扫描的进程数组。取值范围：[1, 100] |
| flags  | IN     | 保留字段。                                     |

### 返回值 RETURN VALUE

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

返回值-9：内核态内存申请失败返回-9。

### 约束 CONSTRAINTS

- 本节点调用
- 只有通过OutSmapAddProcessTracking接口设置的scanType为0的PID才能被这个接口移除。

- 只支持虚拟机场景。
- pidVec的size需为[1,100]

### 附注 NOTES

暂无

## UBSRMRSSmapEnableProcessMigrate: 启用/禁用PID对应虚机的冷热迁移和迁回

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int UBSRMRSSmapEnableProcessMigrate(std::vector<pid_t> pidVec, int enable, int flags = 0);
```

### 描述 DESCRIPTION

启用/禁用PID对应虚拟机的冷热迁移和迁回。

### 参数 Parameters

| name   | IN/OUT | description               |
| ------ | ------ | ------------------------- |
| pidVec | IN     | 虚拟机PID数组。           |
| enable | IN     | 启用/禁用：0-禁用；1-启用 |
| flags  | IN     | 保留字段。                |

### 返回值 RETURN VALUE

返回值0：成功返回0。

返回值-1：SMAP未初始化返回-1。

返回值-22：参数错误返回-22。

返回值-110：超时返回-110。

### 约束 CONSTRAINTS

- 本节点调用
- flags为保留字段，暂未使用。

- pidVec的size需为[1,100]

### 附注 NOTES

暂无

## MigrateOut: 设置进程迁移到远端NUMA，异步调用接口

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int MigrateOut(const std::vector<MigrateOutPayload> &items, int pidType);
```

### 描述 DESCRIPTION

启用/禁用PID对应虚拟机的冷热迁移和迁回。

### 参数 Parameters

| name    | IN/OUT | description                                                  |
| ------- | ------ | ------------------------------------------------------------ |
| items   | IN     | 迁移进程信息，包含迁移进程、远端NUMA和迁移比例               |
| pidType | IN     | 进程类型，目前支持4KB和2MB进程类型，int配置类型：0-进程（4K）1-虚拟机（2M） |

### 返回值 RETURN VALUE

返回值0：表示成功

返回非0：表示失败

### 约束 CONSTRAINTS

- 本节点调用

### 附注 NOTES

暂无

## Remove: 移除进程的冷热页迁移

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int Remove(const std::vector<pid_t>& pids, int pidType);
```

### 描述 DESCRIPTION

移除进程的冷热页迁移

### 参数 Parameters

| name    | IN/OUT | description                                                  |
| ------- | ------ | ------------------------------------------------------------ |
| pids    | IN     | 移除的进程信息，包含进程的PID                                |
| pidType | IN     | 进程类型，目前支持4KB和2MB进程类型，int配置类型：0-进程（4K）1-虚拟机（2M） |

### 返回值 RETURN VALUE

返回值0：表示成功

返回非0：表示失败

### 约束 CONSTRAINTS

- 本节点调用

### 附注 NOTES

暂无

## RemoteNumaMigrate: 迁移指定进程远端内存到远端内存

### 摘要 SYNOPSIS

```cpp
#include "mempooling_interface.h"

int RemoteNumaMigrate(const std::vector<pid_t>& pids, int srcNid, int destNid);
```

### 描述 DESCRIPTION

迁移指定进程远端内存到远端内存

### 参数 Parameters

| name    | IN/OUT | description                   |
| ------- | ------ | ----------------------------- |
| pids    | IN     | 移除的进程信息，包含进程的PID |
| srcNid  | IN     | 源远端NUMA                    |
| destNid | IN     | 目标远端NUMA                  |

### 返回值 RETURN VALUE

返回值0：表示成功

返回非0：表示失败

### 约束 CONSTRAINTS

- 本节点调用

### 附注 NOTES

暂无
