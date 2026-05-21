# UBS turbo API 参考

## 概述

本文档详细描述了SMAP对外提供的API接口信息，包括API接口参数解释和使用样例等。

主要从以下几个方面进行介绍：

- 函数定义

    介绍API接口的定义，实现的功能。

- 实现方法

    介绍API接口的使用样例。

- 参数说明

    详细介绍API接口中的参数，包括函数名、数据类型、有效性规格、参数类型、描述等。

- 返回值

    介绍API中可能出现的返回值和对应的含义。

- 约束和注意事项

    介绍使用API时的约束和注意事项。

## SMAP用户态API

### ubturbo\_smap\_start

**函数定义**

初始化SMAP，设置场景及迁移页面类型，虚拟化场景对应2M大页迁移，通算场景对应4K页迁移。

**实现方法**

```C
int ubturbo_smap_start(uint32_t pageType, Logfunc extlog);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pageType|uint32_t|{0,1}|入参|页面类型：0-4K页。1-2M页。|
|extlog|`typedef void (*Logfunc)(int level, const char *str, const char *moduleName);`|与数据类型匹配。|入参|Logfunc日志函数。|

**返回值**

- 成功返回0。
- 同进程重复初始化返回-1。
- 其他进程已初始化返回-13。
- SMAP初始化异常返回-9 。
- 内存申请失败返回-12。
- SMAP内核驱动未安装返回-19。
- 参数错误返回-22。
- SMAP日志初始化失败返回-5。

**约束和注意事项**

- 不能重复初始化。
- pageType需要和当前场景匹配。

### ubturbo\_smap\_stop

**约束和注意事项**

初始化SMAP才能调用停止SMAP。

**函数定义**

停止SMAP，释放资源（包含移除管理的PID迁出列表、enable状态位等）。

**实现方法**

```c
int ubturbo_smap_stop(void);
```

**参数说明**

N/A

**返回值**

- 成功返回0。
- 已停止返回-1。

### ubturbo\_smap\_migrate\_out

**函数定义**

配置虚拟机/进程的远端NUMA和迁出比例。

**实现方法**

```c
int ubturbo_smap_migrate_out(struct MigrateOutMsg *msg, int pidType);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct MigrateOutMsg*|单次最多配置40个PID。SMAP可管理的最大虚拟机个数为100个，最大4K进程个数为300个。配置的虚拟机PID重复时参数错误。远端NUMA需要存在。迁出比例有效值为[0-100]。迁出内存量memSize，单位为KB，必须为4KB的倍数。使用2M大页时不足2M的迁出内存量向下取整。PID对应进程名在黑名单中时参数错误。虚拟化内存水线场景下，非精确的迁出比例，其含义为PID可迁出到远端NUMA的内存比例，SMAP会根据ubturbo_smap_remote_numa_info_set接口传入的借用内存量进行调整；在多虚拟机场景也会根据虚拟机的冷热信息调整实际迁出比例。内存碎片场景下，migrateMode只能是按照memSize迁移。在保证迁出内存足够的情况下，迁出比例含义为PID总的实际使用内存的迁出比例，迁移大小为PID总的实际使用内存的迁出大小（KB）。|入参|#define MAX_NR_MIGOUT 40 #define REMOTE_NUMA_NUM 18struct MigrateOutPayloadInner {    int destNid;    int ratio;    uint64_t memSize; // 内存迁移大小(KB)    MigrateMode migrateMode; // 内存迁移模式，按照比例或是大小};struct MigrateOutPayload {    int srcNid; // 是否指定迁出源节点（-1表示不指定）    pid_t pid;    int count;    struct MigrateOutPayloadInner inner[REMOTE_NUMA_NUM];};struct MigrateOutMsg {    int count;    struct MigrateOutPayload payload[MAX_NR_MIGOUT];};|
|pidType|int|{0,1}|入参|int配置类型：0-进程（4K）1-虚拟机（2M）|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 部分进程不存在但其余进程配置成功返回-3。
- 内存申请失败返回-12。
- 参数错误返回-22。

**约束和注意事项**

- SMAP初始化后才能调用。
- pidType需要和当前场景匹配。
- 远端NUMA最大值为21。
- UB代际远端NUMA最大值为21。
- 远端NUMA被禁用时无法配置迁出（调用ubturbo\_smap\_migrate\_back接口时会默认禁用远端NUMA）。
- 如果已配置某虚拟机的远端NUMA，后续配置不能改变虚拟机的远端NUMA，只能通过ubturbo\_smap\_pid\_remote\_numa\_migrate接口改变远端NUMA。
- 配置PID内存迁出后，由SMAP线程异步迁移，在迁移周期到来时才会执行迁移操作。
- 远端单NUMA场景下，若在管理某个PID前，PID已经使用了某个远端，管理时的远端nid与使用的远端ID不匹配会返回-22错误码。
- 远端多NUMA场景下，通过传入多个远端NUMA，来给PID新增远端NUMA。
- 当前版本不支持指定迁出源节点，srcNid参数保留。
- 4K进程迁移不支持远端多NUMA。
- 页面迁移会过滤掉共享页。

### ubturbo\_smap\_remote\_numa\_info\_set

**函数定义**

通知SMAP，本地NUMA向远端NUMA借用的内存用量。

**实现方法**

```c
int ubturbo_smap_remote_numa_info_set(struct SetRemoteNumaInfoMsg *msg);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct SetRemoteNumaInfoMsg*|srcNid为本地NUMA ID或者为-1，其他返回异常。destNid为本节点远端NUMA ID，非本节点远端NUMA ID返回异常。size为本地srcNid对应在远端NUMA上借用的内存量，单位MB，若srcNid为-1，则代表所有本地NUMA可共享这段内存。|入参|struct SetRemoteNumaInfoMsg {    int srcNid;    int destNid;    uint64_t size;};|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。

**约束和注意事项**

- SMAP初始化后才能调用。
- 远端NUMA最大值为21。
- UB代际远端NUMA最大值为21。
- 如果已配置某虚拟机的远端NUMA，后续配置不能改变虚拟机的远端NUMA，只能通过ubturbo\_smap\_pid\_remote\_numa\_migrate接口改变远端NUMA。
- 当配置的PID可迁出的量大于借用内存量时，SMAP不会使用完所有的借用量，每个本地NUMA对应的远端借用都有最小容量（MIN值，借用量的5%，200MB）不会使用，这是为了在迁移内存时能够申请到新的内存页面。例如：NUMA0 NUMA1分别借用了2G和6G共8G，NUMA0借用的2G，预留按5%计算为100M，NUMA1借用6G的预留为200M，合计一共300M。
- 此接口仅在水线场景中生效，且水线场景调用ubturbo\_smap\_migrate\_out接口前需通过此接口设置远端NUMA使用量才能迁出PID的内存。
- 如果未调用ubturbo\_smap\_remote\_numa\_info\_set接口，默认初始化size值为0。

### ubturbo\_smap\_migrate\_back

**函数定义**

将指定远端NUMA的内存迁移到同一远端NUMA的其他地址段。

**实现方法**

```c
int ubturbo_smap_migrate_back(struct MigrateBackMsg *msg);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct MigrateBackMsg*|最多支持18个远端NUMA，当本地有2个NUMA时有效值范围是[2, 19]，当本地有4个NUMA时有效值范围是[4, 21]。远端NUMA和传入的memid需要匹配。|入参|#define MAX_NR_MIGBACK 50struct MigrateBackPayload { int srcNid; int destNid; uint64_t memid; };struct MigrateBackMsg { unsigned long long taskID; int count; struct MigrateBackPayload payload[MAX_NR_MIGBACK]; };|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 系统调用失败返回-9。
- 参数错误返回-22。
- 超时返回-11。

**约束和注意事项**

- SMAP初始化后才能调用。
- 不支持并发调用此接口，否则会引起内存归还失败。
- 远端NUMA和传入的地址段需要匹配。
- 调用此接口后，SMAP默认禁止指定远端NUMA的冷热流动，只允许迁回任务中的迁移。
- 若远端NUMA的其他地址段的空闲页面不够，迁移任务会失败。
- 虚拟化水线场景下，请调用此接口前需调用ubturbo\_smap\_remote\_numa\_info\_set接口通知SMAP更新借用内存量，以保证远端NUMA有足够的内存空间。
- 内存碎片场景下，调用此接口，需由调用方预留足够内存空间，否则会迁回失败。
- 迁回任务为异步执行，执行状态在/sys/kernel/debug/smap/mb\_\[taskID\]中进行查询。
- 同一个远端NUMA不支持并发调用，并发调用可能导致迁移数据无法迁移干净。

### ubturbo\_smap\_remove

**函数定义**

从SMAP管理的虚拟机/进程中移除指定的虚拟机/进程。

**实现方法**

```c
int ubturbo_smap_remove(struct RemoveMsg *msg, int pidType);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct RemoveMsg*|单次最多移除的虚拟机/进程数量为40。|入参|#define MAX_NR_REMOVE 40#define REMOTE_NUMA_NUM 18struct RemovePayload { pid_t pid; int count; int nid[REMOTE_NUMA_NUM]};struct RemovePayload { pid_t pid; };struct RemoveMsg { int count; struct RemovePayload payload[MAX_NR_REMOVE]; };|
|pidType|int|{0,1}|入参|int配置类型：0-进程（4K）1-虚拟机（2M）|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- SMAP处理异常返回-9。
- 参数错误返回-22。
- 申请内存失败返回-12。

**约束和注意事项**

- SMAP初始化后才能调用。
- pidType需要和当前场景匹配。
- 该接口功能为移除指定的虚机/进程的远端NUMA，当远端NUMA全被移除时，整个进程被移除SMAP管理，单次可移除多个NUMA。
- 当调用ubturbo\_smap\_migrate\_back接口迁回完所有地址后，需使用ubturbo\_smap\_remove接口移除虚拟机管理，保证后续流程正常。

### ubturbo\_smap\_node\_enable

**函数定义**

启用NUMA迁入，允许其他NUMA的内存向该NUMA进行迁移。

**实现方法**

```c
int ubturbo_smap_node_enable(struct EnableNodeMsg *msg);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct EnableNodeMsg*|传入NUMA最大值为21，且为远端NUMA。|入参|struct EnableNodeMsg { int enable; int nid; };|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。

**约束和注意事项**

- SMAP初始化后才能调用。
- 此接口与ubturbo\_smap\_migrate\_back接口配合使用，目的是在ubturbo\_smap\_migrate\_back接口调用后，恢复对应远端NUMA的冷热流动。

### ubturbo\_smap\_freq\_query

**函数定义**

查询进程冷热频次信息。

**实现方法**

```c
int ubturbo_smap_freq_query(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut, int dataSource);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pid|int|对应PID需要存在。|入参|进程PID号。|
|data|uint16_t*|非空。|入参|存放冷热信息的数组，数组每1位表示pid使用的1个内存页面在统计时间内被访问的次数，使用本地numa的页面排序在数组前面。dataSource指定为1时，data中存放的是最近一个统计时长的数据。|
|lengthIn|uint32_t|大于0。|入参|指示data数组的大小。|
|lengthOut|uint32_t*|非空。|入参|返回实际写入data数组的大小。|
|dataSource|int|{0,1}|入参|标识数据来源。0表示最近一个迁移周期的进程的冷热信息。1表示统计扫描进程的冷热信息。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 扫描时长未达到统计时长要求，返回-11。

**约束和注意事项**

- SMAP初始化后才能调用。
- dataSource为0表示先调用ubturbo\_smap\_migrate\_out接口，后续可获取到最近一个周期的冷热数据。
- dataSource为1表示先调用ubturbo\_smap\_process\_tracking\_add接口，并且scanType指定为2，统计完成后，后续可通过此接口获取到冷热信息。

### ubturbo\_smap\_run\_mode\_set

**函数定义**

设置SMAP运行模式。

**实现方法**

```c
int ubturbo_smap_run_mode_set(int runMode);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|runMode|int|0：水线场景。1：内存碎片场景。其他值：返回错误。|入参|设置SMAP的运行模式，当前包括水线场景，内存碎片场景。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 同步配置文件失败返回-9。

**约束和注意事项**

- SMAP初始化后才能调用。
- 未设置的情况下，默认为水线场景。
- 如果是4K场景，不支持设置SMAP运行模式为内存碎片模式。

### ubturbo\_smap\_process\_migrate\_enable

**函数定义**

启用/禁用PID对应虚拟机的冷热迁移和迁回。

**实现方法**

```c
int ubturbo_smap_process_migrate_enable(pid_t *pidArr, int len, int enable, int flags);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pidArr|pid_t *|NA|入参|虚拟机PID数组。|
|len|int|虚拟化场景：1到100的整数。通算场景：1到300的整数。|入参|虚拟机PID数组长度。|
|enable|int|01|入参|0-禁用。1-启用。|
|flags|int|NA|入参|保留字段。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 超时返回-110。

**约束和注意事项**

flags为保留字段，暂未使用。

### ubturbo\_smap\_remote\_numa\_migrate

**函数定义**

通知SMAP迁移远端NUMA的内存到另一个远端NUMA。

**实现方法**

```c
int ubturbo_smap_remote_numa_migrate(struct MigrateNumaMsg *msg);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct MigrateNumaMsg *|NA|入参|迁移远端NUMA的消息。|
|msg.srcNid|int|远端NUMA ID|入参|源NUMA ID。|
|msg.destNid|int|远端NUMA ID|入参|目的NUMA ID。|
|msg.count|int|1-50的整数|入参|源NUMA地址段数量。|
|msg.memids|uint64_t|长度固定为50|入参|memid数组。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 迁移成功但修改进程远端NUMA失败返回-9。
- 迁移失败返回-12。
- 参数错误返回-22。

**约束和注意事项**

- 传入的地址段需要和源NUMA ID对应的地址段匹配。
- 调用该接口前必须调用ubturbo\_smap\_process\_migrate\_enable禁用PID迁移功能。
- 不支持重复调用。

### ubturbo\_smap\_pid\_remote\_numa\_migrate

**函数定义**

通知SMAP按PID级迁移远端内存到其他远端内存。

**实现方法**

```c
int ubturbo_smap_pid_remote_numa_migrate(struct MigrateEscapeMsg *msg);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct MigrateEscapeMsg*|单次最多迁移PID数量由当前场景决定。SMAP可管理的最大虚拟机个数为100个，最大4K进程个数为300个。配置的PID重复时参数错误。srcNid，destNid需要存在。迁出比例有效值为0到当前进程配置在srcNid上的最大比例。迁出内存量memSize，单位为KB，必须为2MB的倍数，且必须小于当前进程配置在srcNid上的大小，并小于进程本身的内存大小。|入参|#define MAX_NR_MIGRATE_ESCAPE 300struct MigrateEscapePayload {    pid_t pid;    int srcNid;    int destNid;    int ratio;    uint64_t memSize;    MigrateMode migrateMode;};struct MigrateEscapeMsg {    int count; // payload数组长度    struct MigrateEscapePayload payload[MAX_NR_MIGRATE_ESCAPE];};|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 迁移成功但修改进程远端NUMA失败返回-9。
- PID的远端NUMA不全为srcNid，返回-6。
- 参数错误返回-22（如：PID不在SMAP管理中）。
- 内存迁移失败返回-92。

**约束和注意事项**

- 目标NUMA ID内存余量充足。
- 不支持重复调用。
- 调用该接口前必须调用ubturbo\_smap\_process\_migrate\_enable禁用PID迁移功能。
- 如果虚拟机进程已被[ubturbo\_smap\_process\_tracking\_add](#ubturbo_smap_process_tracking_add)设置为scanType 0只扫描状态或scanType 2统计特定时长冷热信息状态，则无法被迁移。

### ubturbo\_smap\_process\_tracking\_add

**函数定义**

通知SMAP添加进程扫描，并设置扫描周期参数。

**实现方法**

```c
int ubturbo_smap_process_tracking_add(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pidArr|pid_t *|NA|入参|PID数组。|
|scanTime|uint32_t *|50毫秒的倍数，最小值50毫秒，最大值为200毫秒。|入参|扫描周期数组。|
|duration|uint32_t *|与len长度相符的扫描周期数组，在scanType=2时使用。取值范围：[1,300]，单位：秒|入参|访存数据统计时长。|
|len|int|1-40整数。|入参|PID数组长度。|
|scanType|int|{0,1,2}|入参|扫描模式标志位。0：表示进程设置为只扫描状态，此时调用/proc/{PID}_t/tracking_info获取扫描频次信息。1：表示进程恢复为冷热扫描加迁移状态。2：表示进程设置为统计特定时长冷热信息状态。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 内核态内存申请失败返回-9。
- 用户态内存申请失败返回-12。

**约束和注意事项**

- scanType为0或1时只支持虚机场景。
- 当进程未被SMAP纳管时，可以调用该接口，此时scanType不能传1。
- 当进程已经被SMAP纳管时，须先停止冷热迁移，然后才可以调用该接口，scanType可以传0/1/2。
- scanType传1的情况为进程已被SMAP纳管，需要从只扫描状态恢复到冷热扫描加迁移状态。
- 当进程未被SMAP纳管时，只允许进程使用本地NUMA。
- 如果传入不存在的PID，接口返回0。
- 当指定scanType为2时，smap会根据入参申请（duration/scanTime \* PID使用内存页数）字节的内存用来保存每个扫描周期的数据。
- 由于ubturbo IPC通信最大限制32MB内存传输，所以scanType为2时，在4k场景下pid最大规格限制最多40G，超过此规格统计数据可能存在传输超过IPC通信限制。

### ubturbo\_smap\_process\_tracking\_remove

**函数定义**

通知SMAP移除进程扫描。

**实现方法**

```c
int ubturbo_smap_process_tracking_remove(pid_t *pidArr, int len, int flags)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pidArr|pid_t *|NA。|入参|PID数组。|
|len|int|0整数。取值范围：1-40|入参|PID数组长度。|
|flags|int|NA。|入参|保留字段。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- ioctl接口调用失败返回-9。

**约束和注意事项**

- 只有通过ubturbo\_smap\_process\_tracking\_add接口设置的scanType为0或2的PID才能被这个接口移除。
- 只支持虚拟机场景。
- 如果传入不存在的PID，接口返回0。

### /proc/\{PID\}\_t/tracking\_info

**函数定义**

通过文件方式查询虚拟机的访问频次信息，PID为\{PID\}的虚拟机的访存频次文件位置为：/proc/\{PID\}\_t/tracking\_info

**实现方法**

```c
// 第一次调用读取数量
int fread(int *num, sizeof(int), int n, FILE *file)
// 第二次调用读取物理地址和频次信息
int fread(struct FreqInfo *info, sizeof(FreqInfo), int n, FILE *file)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|FreqInfo|struct FreqInfo {u64 paddr;u16 freq;}|NA|入参|存储频次信息结构体。|

**返回值**

- 读取成功返回频次个数以及频次信息。
- 读取失败返回errno。

**约束和注意事项**

只支持虚拟化场景下，成功添加仅扫描模式的虚拟机到SMAP后调用。

### ubturbo\_smap\_migrate\_out\_sync

**函数定义**

通知SMAP调用内存同步迁出接口。

**实现方法**

```c
int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct MigrateOutMsg *|NA|入参|迁移信息。|
|msg.count|int|1-40的整数。|入参|迁移数量。|
|msg.payload[].pid|pid_t|NA|入参|进程PID。|
|msg.payload[].srcNid|int|是否指定迁出源节点|入参|是否指定迁出源节点（-1表示不指定）。|
|msg.payload[].count|int|1-18的整数|入参|迁移远端NUMA数量。|
|msg.payload[].inner[].ratio|int|0-100的整数。|入参|迁移比例。|
|msg.payload[].inner[].destNid|int|大于等于本地NUMA数量且小于10。|入参|目的NUMA ID。|
|msg.payload[].inner[].memSize|uint64_t|单位：KB必须为2MB的整数倍。|入参|内存迁移大小。|
|msg.payload[].inner[].migrateMode|MigrateMode|枚举类型：枚举值为MIG_RATIO_MODE = 0， MIG_MEMSIZE_MODE = 1|入参|MIG_RATIO_MODE表示按照比例迁移，MIG_MEMSIZE_MODE表示按照内存大小迁移|
|pidType|int|1|入参|进程PID类型，1表示虚拟机类型。|
|maxWaitTime|uint64_t|10s-3min单位：ms|入参|一次调用最大等待时间。传入0表示大虚拟机场景一直等待直到迁移完成。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误（包含非法入参、非内存池化场景）返回-22。
- 等待超时返回-16。
- 迁移过程中部分进程不存在返回-3。
- 内存申请失败返回-12。

**约束和注意事项**

- 只支持在虚拟化场景调用。
- 只支持内存池化场景。
- 当前版本不支持指定迁出源节点，srcNid参数保留。

### ubturbo\_smap\_process\_config\_query

**函数定义**

查询SMAP进程配置的接口。

**实现方法**

```c
int ubturbo_smap_process_config_query(int nid, struct OldProcessPayload *result, int inLen, int *outLen)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|nid|int|有效的远端NUMA。|入参|远端NUMA NID。|
|result|struct OldProcessPayload|非空数组。|出参|保存结果的数组。|
|result[].pid|pid_t|NA|出参|进程PID。|
|result[].ratio|uint8_t|NA|出参|进程内存本地比例。|
|result[].scanType|uint8_t|NA|出参|进程扫描类型。|
|result[].type|uint8_t|NA|出参|进程类型。0-PROCESS_TYPE1-VM_TYPE|
|result[].state|uint8_t|NA|出参|进程状态。0-空闲1-冷热迁移2-迁回3-远端搬迁|
|result[].l1Node[4]|int16_t|NA|出参|进程L1 Node。|
|result[].l2Node[4]|int16_t|NA|出参|进程L2 Node。|
|result[].scanTime|uint32_t|NA|出参|进程扫描间隔。|
|result[].memSize|uint64_t|NA|出参|进程迁出到远端的内存大小。单位：MB|
|inLen|int|与数组长度一致。虚拟机场景小于等于100。普通进程场景小于等于300。|入参|数组长度。|
|outLen|int *|非空整型指针。|出参|进程数量。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。

**约束和注意事项**

切换场景时需要删除SMAP配置文件/dev/shm/smap\_config。

### ubturbo\_smap\_urgent\_migrate\_out

**函数定义**

SMAP紧急迁移接口。

**实现方法**

```c
void ubturbo_smap_urgent_migrate_out(uint64_t size)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|size|uint64_t|NA|入参|内存迁移量。单位：字节|

**返回值**

无返回值。

**约束和注意事项**

在OOM场景下由上层组件调用。

### ubturbo\_smap\_is\_running

**函数定义**

查询SMAP是否运行。

**实现方法**

```c
bool ubturbo_smap_is_running()
```

**参数说明**

N/A

**返回值**

- true表示SMAP正在运行。
- false表示SMAP停止运行。

**约束和注意事项**

N/A

### ubturbo\_smap\_remote\_numa\_freq\_query

**函数定义**

SMAP查询远端NUMA频次接口。

**实现方法**

```c
int ubturbo_smap_remote_numa_freq_query(uint16_t *numa, uint64_t *freq, uint16_t length)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|numa|uint16_t*|对应NUMA需要存在。|入参|存放NUMA-ID的数组。|
|freq|uint64_t*|非空数组。|出参|存放频次信息的数组。|
|length|uint16_t|传入数组的长度，length大于0小于等于18。|入参|传入数组的长度。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 清空内存错误返回-5。

**约束和注意事项**

- SMAP初始化后，至少经过一个完整的迁移周期后才能调用（\>2s）。
- 由于硬件扫描窗口大小有限，SMAP使用滑窗扫描策略，不保证流量统计值与真实流量一致。
- 只支持查询远端NUMA。

### ubturbo\_smap\_migrate\_out\_grouped

**函数定义**
配置大规格虚拟机的本地+远端NUMA可用配额。

**实现方法**

```c
int ubturbo_smap_migrate_out_grouped(struct GroupedMigrateOutMsg *msg, int pidType);
```

**参数说明**

**表 1** 参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|msg|struct GroupedMigrateOutMsg*|struct GroupedMigrateOutMsg*|入参|-|
|pidType|int|int|入参|int配置类型：<ul><li>0-进程（4K）（当前不支持）</li><li>1-虚拟机（2M）</li></ul>|

**返回值**

- 成功返回0.
- SMAP未初始化返回-1。
- 部分进程不存在返回 -3，本接口会回滚已添加配置，不提供部分成功语义。
- 远端 NUMA 被禁用、已有 grouped PID 正在迁移等临时冲突返回 -11。
- 内存申请失败返回-12。
- 参数错误返回-22。

**约束和注意事项**

- SMAP初始化后才能调用。
- 仅支持 2M huge page VM，不支持 4K 进程或普通进程。
- pidType 需要和当前 2M VM 场景匹配；当前非 huge page 模式会返回参数错误。
- msg 不能为空，msg->count 取值范围为 1 ~ MAX_NR_GROUPED_MIGOUT。
- 同一次调用内不能传入重复 PID。
- 同一 PID 可配置多个 migration group，groupCount 取值范围为 1 ~ MAX_MIGRATION_GROUP_NUM，当前最大为 8。
- 每个 group 需要配置：
    - 本地 source NUMA 集合；
    - 远端 target NUMA 集合；
    - 每个本地 NUMA 的保留水线 locals[].size，单位 KB；
    - 每个远端 target 的 quota targets[].size，单位 KB。
- 每个 group 的 localCount 取值范围为 1 ~ MAX_GROUP_LOCAL_NUMA，当前最大为 4。
- 每个 group 的 targetCount 取值范围为 1 ~ MAX_GROUP_REMOTE_NUMA。
- 本地 NUMA 必须是当前机器有效 local NUMA，且同一 PID 的不同 group 之间不能复用同一个 local NUMA。
- 远端 NUMA 必须是当前机器有效 remote NUMA，远端 NUMA 最大值受当前本地 NUMA 数和 REMOTE_NUMA_BITS 约束，通常最大为 21。
- 同一个 group 内不能配置重复 target NUMA
- 远端 NUMA 被禁用时无法配置为 grouped target；调用 ubturbo_smap_migrate_back 时会默认禁用对应远端 NUMA.
- 每个 target quota 至少需要 2MB，小于 2MB 会返回 -22。
- group policy 不能和普通 ubturbo_smap_migrate_out policy 混用；已按普通迁出接口管理的 PID，不能再配置 grouped policy。
- 已存在 grouped policy 的 PID 只有在进程状态为 PROC_IDLE 时才能更新配置，否则返回 -11。
- group policy 配置时会基于 /proc/\<pid>/numa_maps 初始化远端 target 的 usedPages 账本。
- 如果管理前 PID 已经使用 remote NUMA：
    - 该 remote NUMA 必须被当前 grouped policy 的 target 管理；
    - remote resident pages 不能超过对应 target quota 或 shared target 的 quota 总和；
    - 否则返回 -22。
- 允许多个 group 共享同一个 remote target，但当前只维护容量级账本，不保证页级 ownership。
- 配置成功后由 SMAP 线程异步执行迁移，迁移周期到来后才会真正迁移页面。
- grouped policy 当前不支持 smap_config 持久化与恢复，SMAP 重启后需要重新下发配置。
- 页面迁移会过滤掉共享页。

## Mempooling API

### UBSRMRSMemBorrowStrategy

**函数定义**

内存借用策略，决策从哪些节点借用内存以及借用内存大小，返回决策结果。

**实现方法**

```c
uint32_t UBSRMRSMemBorrowStrategy(const SrcMemoryBorrowParam &outSrcParam, const uint64_t &borrowSize,MemBorrowStrategyResult &outBorrowStrategyResult)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|outSrcParam|SrcMemoryBorrowParam|srcNid：借入方节点IDsrcSocketId：借入方Socket IDsrcNumaId：借入方NUMA IDuid：借入方用户uidusername：借入方用户名|入参|借入节点信息。|
|borrowSize|uint64_t|如果传入的borrowSize不是obmm.memory.block.size*1024的整数倍，内部则按obmm.memory.block.size*1024对齐取整后再进行借用决策。obmm.memory.block.size参数详情请参见[内存池化配置说明](../../ubse_configration_instructions.md#内存池化配置说明)。|入参|需要借入的内存大小。单位：KB|
|outBorrowStrategyResult|MemBorrowStrategyResult|destNid：借出方节点ID。destSocketId：借出方Socket ID。destNumaNum：同一个Socket单次借出的NUMA数量，当前限制为1。destNumaId：借出方NUMA ID。memSize：借用大小，单位为KB。borrowSize：内存借用大小。|出参|决策结果。|

**返回值**

- 返回0：借用策略执行成功。
- 返回1：借用策略执行失败。

**约束和注意事项**

本节点调用。

所有节点的ubse配置项须保持一致。

### UBSRMRSMemBorrowExecute

**函数定义**

执行内存借用动作，返回执行结果。

**实现方法**

```c
uint32_t UBSRMRSMemBorrowExecute(const SrcMemoryBorrowParam &outSrcParam,const std::vector<DestMemoryBorrowParam> &outDestParam,MemBorrowExecuteResult &outBorrowExecuteResult)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|outSrcParam|SrcMemoryBorrowParam|srcNid：借入方节点ID。srcSocketId：借入方Socket ID。srcNumaId：借入方NUMA ID。uid：借入方用户uid。username：借入方用户名。|入参|借入节点信息。|
|outDestParam|std::vector\<DestMemoryBorrowParam>|destNid：借出方节点ID。destSocketId：借出方Socket ID。destNumaNum：同一个Socket单次借出的NUMA数量，当前限制为1。destNumaId：借出方NUMA ID，数量[1, 200]。memSize：借用大小，单位：KB，需要为2048的正整数倍。|入参|决策结果。|
|outBorrowExecuteResult|MemborrowExecuteResult|borrowIds：内存描述符数组。presentNumaIds：呈现的远端NUMA数组。|出参|执行结果（内存描述符数组，呈现的远端NUMA数组）。|

**返回值**

- 返回0：代表借用执行动作集执行成功。
- 返回1：代表借用执行动作集执行失败。

**约束和注意事项**

- 本节点调用。
- memSize需要是obmm.memory.block.size的整数倍。
- memSize在碎片场景下不大于4G，否则执行失败；在大虚机场景下无此约束。
- 所有节点的ubse配置项须保持一致。

### UBSRMRSMigrateStrategy

**函数定义**

本接口为封装第三层匀一匀（内存迁移）接口。内存借用之后，第三层策略决策哪些虚拟机迁出多少比例内存到哪些远端NUMA，返回决策结果和评估时间。

**实现方法**

```c
uint32_t UBSRMRSMigrateStrategy(const std::string &borrowInNode, const std::vector<VMPresetParam> &outVmInfoList,const uint64_t &borrowSize, MigrateStrategyResult &outMigrateStrategyResult)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|borrowInNode|std::string|-|入参|内存借入节点。|
|outVmInfoList|std::vector\<VMPresetParam>|数量：[1, 100]pid：vm对应PID。ratio：迁出最大比例，取值范围[0，100]。|入参|预设迁出最大比例（虚拟机迁出最大比例{进程ID，可迁出最大比例}）。|
|borrowSize|std::uint64_t|-|入参|匀出本地内存大小。单位：KB会向上对2*1024取整。|
|outMigrateStrategyResult|MigrateStrategyResult|pid：vm对应PID。memSize：对应PID迁出大小，指在远端NUMA实际占用（累计迁出）的大小。desNumaId：迁移远端NUMA。waitingTime：单位为ms。|出参|内存迁出策略结果{内存迁出策略执行结果，预设迁出最大比例（虚拟机迁出最大比例{进程ID，可迁出最大比例}），内存迁出动作集合预计执行时间}。|

**返回值**

- 返回0：表示迁出策略决策成功。
- 返回1：表示迁出策略决策失败。

**约束和注意事项**

本节点调用。

### UBSRMRSMigrateExecute

**函数定义**

执行内存迁出动作集，在规定时间内完成返回成功，否则超出等待时间，表示失败，执行回退。

**实现方法**

```c
uint32_t UBSRMRSMigrateExecute(const std::string &borrowInNode, const std::vector<VMMigrateOutParam> &outVmInfoList,const std::uint64_t &waitingTime, const std::vector<std::string> &borrowIds)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|borrowInNode|*std::string*|-|入参|内存借入节点。|
|outVmInfoList|std::vector\<mempooling::VMMigrateOutParam>|数量，取值范围：[1,100]pid：vm PID。memSize：实际迁出大小，指在远端NUMA实际占用（累计迁出）的大小，必须为2*1024的整数倍。destNumaId：迁移远端NUMA。|入参|需要借入的内存大小。单位：KB|
|waitingTime|*uint32_t*|10s-3min单位：ms|入参|等待时间。单位：ms|
|borrowIds|*std::vector\<std::string>*|数量，取值范围：[1, 512]|入参|内存描述符列表。|

**返回值**

- 返回0：表示借用迁出动作集执行成功。
- 返回1：代表借用迁出动作集执行失败。

**约束和注意事项**

本节点调用。

### UBSRMRSMemFree

**函数定义**

调用第三层内存归还策略，第三层决策并执行哪些虚拟机可以迁回内存，腾出远端NUMA内存，返回给第一层可以归还的远端NUMA节点，第一层执行节点间内存归还（整NUMA归还）。

**实现方法**

```c
uint32_t UBSRMRSMemFree(const std::string &nodeId)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|nodeId|std::string|-|入参|内存借入节点。|

**返回值**

- 返回0：决策成功（包括决策出可以归还的NumaId列表，可以为空）且执行成功（决策可以归还的NumaId列表为空则不执行）。
- 返回1：决策失败或执行失败。

**约束和注意事项**

本节点调用。

### UBSRMRSMemBorrowRollback

**函数定义**

覆盖场景为借用成功的前提下，迁出失败以及迁出成功但创建虚拟机失败，回滚逻辑为虚拟机远端占用全部迁回，对应borrowIds全部归还。

**实现方法**

```c
uint32_t UBSRMRSMemBorrowRollback(const std::string &borrowInNode, const std::vector<std::string> &borrowIds)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|borrowInNode|std::string|-|入参|回滚节点ID。|
|borrowIds|std::vector\<std::string>|-|入参|借用记录ID。|

**返回值**

- 返回0：表示回滚成功。
- 返回1：表示回滚失败。

**约束和注意事项**

- 本节点调用。
- 两个入参不能为空，覆盖场景为借用成功前提。

### UBSRMRSUpdateAntiNode

**函数定义**

更新反亲和性列表，VM插件可以调用该接口传递节点反亲和性。

**实现方法**

```c
uint32_t UBSRMRSUpdateAntiNode(const std::map<std::string,std::vector<std::string>> &nodeAntiMap)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|nodeAntiMap|std::map<std::string, std::vector\<std::string>>|-|入参|反亲和性节点映射。|

**返回值**

- 返回0：更新反亲和性成功。
- 返回1：更新反亲和性失败。

**约束和注意事项**

- 本节点调用。
- 更新反亲和性配置输入\{key:\[value\]\}，其中key代表节点且不能重复，value代表与其反亲和性的节点。
- 需要输入不重复的rack内全部节点的反亲和性节点列表。

### UBSRMRSGetVmInfoListOnNode

**函数定义**

内存调度插件提供接口获取本节点所有虚拟机信息。

**实现方法**

```c
uint32_t UBSRMRSGetVmInfoListOnNode(std::vector<VmDomainInfo> &outVmDomainInfos)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|outVmDomainInfos|std::vector\<VmDomainInfo>|nodeId：物理节点ID。hostName：物理节点hostName。uuid：虚拟机uuid。name：虚拟机name。socketId：cpu对应Socket ID。numaId：cpu对应的NUMA ID。vmCreateTime：虚拟机创建时间。maxMem：虚拟机申请内存，单位KBytes。PID：虚拟机进程PID。pageSize：虚拟机页大小，单位KBytes。localNumaId：本地NUMA ID。localUsedMem：本地使用内存，单位为KBytes。remoteNumaId：远端NUMA ID。remoteUsedMem：远端使用内存，单位为KBytes。state：虚拟机状态。timestamp：时间戳。|出参|虚拟机字段信息列表。|

**返回值**

- 返回0：查询成功。
- 返回1：查询失败。

**约束和注意事项**

本节点调用，返回本节点信息。

仅支持两种规格虚拟机：

- 规格一：绑定1本地NUMA的虚拟机。
- 规格二：绑定1本地NUMA1远端NUMA的虚拟机。

仅采集状态为RUNNING和PAUSED状态的虚拟机。

### UBSRMRSGetNumaInfoListOnNode

**函数定义**

内存调度插件提供接口获取本节点NUMA信息。

**实现方法**

```c
uint32_t UBSRMRSGetNumaInfoListOnNode(std::vector<NumaInfo> &outNumaInfos)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|outNumaInfos|std::vector\<NumaInfo>|nodeId：节点ID。lhostName：节点HostName。numaId：numaId。isLocal：是否是本地NUMA 0：非本地 ，1：本地。memTotal：该NUMA节点内存总量（包含），单位为KBytes。memFree：该NUMA上空闲内存，单位KBytes。hugePageTotal：该NUMA上大页数量，单位为个。hugePageFree：该NUMA上空闲的大页数量，单位为个。|出参|NUMA字段信息列表。|

**约束和注意事项**

任意节点调用，返回本节点信息。

### UBSRMRSMemBorrow

**函数定义**

超分场景，水线告警触发内存借用，返回内存描述符。

**实现方法**

```c
uint32_t UBSRMRSMemBorrow(const SrcMemoryBorrowParam &srcParam, const std::vector<uint64_t> &borrowSizes,const WaterMark &waterMark, MemBorrowExecuteResult &result
)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|srcParam|SrcMemoryBorrowParam|srcNid：借入方节点ID。srcSocketId：借入方Socket ID。srcNumaId：借入方NUMA ID。|入参|借入节点信息。|
|borrowSizes|std::vector<uint64_t>|数量：[1, 256]。内存值为128*1024的整数倍。|入参|决策结果。|
|waterMark|WaterMark|highWaterMark：高水线。lowWaterMark：低水线。|-|-|
|result|MemBorrowExecuteResult|borrowIds：内存描述符数组。presentNumaIds：呈现的远端NUMA数组。|出参|执行结果（内存描述符数组，呈现的远端NUMA数组）。|

**返回值**

- 返回0，表示内存借用成功。
- 返回1，表示借用内存失败。

**约束和注意事项**

任意节点调用。

### UBSRMRSMemMigrate

**函数定义**

超分场景，内存借用成功后，决策并迁移虚拟机内存到远端内存。

**实现方法**

```c
uint32_t UBSRMRSMemMigrate(const SrcMemoryBorrowParam &srcParam, const vector<VMPresetParam> &vmPresetParams, const MemborrowExecuteResult &result)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|srcParam|SrcMemoryBorrowParam|srcNid：借入方节点ID。srcSocketId：借入方Socket ID。srcNumaId：借入方NUMA ID。|入参|借入节点信息。|
|vmPresetParams|std::vector\<VMPresetParam>|虚拟机数量，取值范围：[1, 100]。pid：vm对应PID。ratio：迁出最大比例，取值范围[0, 100]。|入参|需要迁移虚拟机内存迁移参数。|
|result|MemborrowExecuteResult|borrowIds：内存描述符数组。presentNumaIds：呈现的远端NUMA数组。|入参|执行结果（内存描述符数组，呈现的远端NUMA数组）。|

**返回值**

- 返回0，表示迁移成功。
- 返回1，表示迁移失败。

**约束和注意事项**

任意节点调用。

### UBSRMRSMemReturn

**函数定义**

超分场景，分配内存失败后回滚或归还内存。

**实现方法**

```c
uint32_t UBSRMRSMemReturn(const SrcMemoryBorrowParam &srcParam, const std::vector<std::string> &borrowIds, const std::vector<pid_t>& pids)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|srcParam|SrcMemoryBorrowParam|srcNid：借入方节点ID。srcSocketId：借入方Socket ID。srcNumaId：借入方NUMA ID。|入参|借入节点信息。|
|borrowIds|std::vector\<std::string>|-|入参|内存描述符数组。|
|pids|std::vector<pid_t>|-|入参|正在迁移的虚拟机进程列表。|

**返回值**

- 返回0，表示归还成功或borrowID已被归还。
- 返回1，表示归还失败。

**约束和注意事项**

任意节点调用。

### UBSRMRSSetRunMode

**函数定义**

设置超分/碎片场景。

**实现方法**

```c
uint32_t UBSRMRSSetRunMode(const int &runMode)
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|runMode|int|runMode：场景类型。取值范围：0:超分场景。1:内存碎片场景。有效值只有0和1。|入参|区分超分/碎片场景。|

**返回值**

- 返回0，代表设置成功。
- 返回1，代表设置失败。

**约束和注意事项**

master节点调用。

### UBSRMRSSmapAddProcessTracking

**函数定义**

通知SMAP添加进程扫描，并设置扫描周期参数。

**实现方法**

```c
int UBSRMRSSmapAddProcessTracking( const std::vector<pid_t> &pidVec,
                              const std::vector<uint32_t> &scanTimeVec, int scanType,
                              const std::optional<std::vector<uint32_t>> &durationVec = std::nullopt);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pidVec|std::vector<pid_t>|目标节点存在的虚拟机PID。取值范围：[1, 100]|入参|需要添加进程扫描的进程数组|
|scanTimeVec|std::vector<uint32_t>|取值范围：[1,100]50毫秒的正整数倍，最大值为200毫秒。|入参|扫描周期数组，50毫秒的倍数，最小值50毫秒，最大值为200毫秒|
|scanType|int|标志位。取值范围：0：HAM_SCAN，供HAM使用。1：NORMAL_SCAN。2：STATISTIC_SCAN。有效值只有0、1、2。|入参|标志位。取值范围：0：HAM_SCAN，供HAM使用。1：NORMAL_SCAN。2：STATISTIC_SCAN, 此模式可以统计特定时长的访存频次信息|
|durationVec|std::vector<uint32_t>|数量，[1,100]取值范围：[1, 300]单位：s|入参|统计周期数组，在scanType=2时使用，默认值为1s。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 内核态内存申请失败返回-9。
- 用户态内存申请失败返回-12。

**约束和注意事项**

- 本节点调用。
- 当进程未被SMAP纳管时，可以调用该接口，此时scanType只能传0或者2。
- 当进程已经被SMAP纳管时，须先停止冷热迁移，然后才可以调用该接口，scanType可以传0或1。
- scanType传1的情况为进程已被SMAP纳管，需要从只扫描状态恢复到冷热扫描加迁移状态。

### UBSRMRSSmapQueryFreq

**函数定义**

查询进程冷热频次信息。

**实现方法**

```c
int UBSRMRSSmapQueryFreq( const pid_t &pid, std::vector<uint16_t> &dataVec,
                       const uint32_t &lengthIn, uint32_t &lengthOut, const int &dataSource);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pid|pid_t|目标节点存在的虚拟机PID。|入参|需要查询冷热信息的虚拟机对应PID。|
|dataVec|std::vector<uint16_t>|频次信息，取值大于等于0。|出参|存放冷热信息的数组。|
|lengthIn|uint32_t|大于0.|入参|指示data数组的大小，虚拟机内存大小除以2M的数量，或者容器内存大小除以4kb的数量。|
|lengthOut|uint32_t|lengthOut小于等于lengthIn。|出参|返回实际写入data数组的大小。|
|dataSource|int|01|入参|标识数据来源。0表示迁移进程的冷热信息。1表示统计扫描进程的冷热信息。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。

**约束和注意事项**

- 本节点调用。
- 当虚拟机/容器进程已经被SMAP纳管时（执行smap迁出），可以调用该接口，dataSource传0。
- 当虚拟机/容器进程已经被SMAP添加进程扫描时，可以调用该接口，dataSource传1。

### UBSRMRSSmapRemoveProcessTracking

**函数定义**

通知SMAP移除进程扫描。

**实现方法**

```c
int UBSRMRSSmapRemoveProcessTracking(const std::vector<pid_t> &pidVec, int flags = 0);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pidVec|std::vector<pid_t>|目标节点存在的虚拟机PID。取值范围：[1, 100]|入参|需要移除进程扫描的进程数组。|
|flags|int|-|入参|保留字段。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 内核接口调用失败返回-9。

**约束和注意事项**

- 本节点调用。
- 只有通过OutSmapAddProcessTracking接口设置的scanType为0的PID才能被这个接口移除。
- 只支持虚拟机场景。

### UBSRMRSSmapEnableProcessMigrate

**函数定义**

启用/禁用PID对应虚拟机的冷热迁移和迁回。

**实现方法**

```c
int UBSRMRSSmapEnableProcessMigrate(const std::vector<pid_t> &pidVec, int enable, int flags = 0);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|pidVec|std::vector<pid_t>|数组大小。取值范围：[1,100]|入参|虚拟机PID数组。|
|enable|int|01|入参|0-禁用。1-启用。|
|flags|int|-|入参|保留字段。|

**返回值**

- 成功返回0。
- SMAP未初始化返回-1。
- 参数错误返回-22。
- 超时返回-110。

**约束和注意事项**

- 本节点调用。
- flags为保留字段，暂未使用。

### UBSRMRSPidNumaInfoCollect

**函数定义**

超分场景，容器进程信息查询。

**实现方法**

```c
uint32_t UBSRMRSPidNumaInfoCollect(const SrcMemoryBorrowParam &srcParam, const std::vector<pid_t> &pids,
                            std::vector<PidInfo> &pidInfos);
```

**参数说明**

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|srcParam|SrcMemoryBorrowParam|srcNid：借入方节点ID。srcSocketId：借入方Socket ID。srcNumaId：借入方NUMA ID。|入参|借入节点信息。|
|pids|std::vector<pid_t>|取值范围：[1, 300]|入参|借入NUMA上进程列表。|
|pidInfos|std::vector\<PidInfo>|pid：待查询进程PID。localUsedMem：进程本地内存使用量。localNumaIds：进程绑定本地NUMA情况。remoteUsedMem：进程使用远端内存情况。remoteNumaId：进程使用远端NUMA ID。|出参|查询到的进程内存信息。|

**返回值**

- 返回0，表示传入的PID查询成功。
- 返回1，表示传入PIDs为空，或传入的PIDs全不存在，或查询失败。

**约束和注意事项**

任意节点调用，返回本节点信息。

### UBSRMRSSetWaterMark

**函数定义**

超分场景，注入水线。

**实现方法**

```c
uint32_t UBSRMRSSetWaterMark(const WaterMark &waterMark);
```

**参数说明**

**表 1**  参数说明

|参数名|数据类型|有效性规格|参数类型|描述|
|--|--|--|--|--|
|waterMark|WaterMark|highWaterMark：高水线，取值范围(0,100]。lowWaterMark：低水线，取值范围[0, highWaterMark)。|-|-|

**返回值**

- 返回0，表示设置成功。
- 返回1，表示设置失败。

**约束和注意事项**

任意节点调用，每个节点需要注入。

## 环境变量

**表 1**  环境变量说明

|参数名称|参数类型|**参数说明**|**取值范围**|缺省值|
|--|--|--|--|--|
|HSHMEM_LOG_LEVEL|string|HSHMEM组件的日志等级。|debug、info、warn、error/DEBUG、INFO、WARN、ERROR。|info/INFO|
|HSHMEM_CONFIG_PATH|string|HSHMEM配置文件路径。|自定义ini配置文件，参考hshmem.ini。|./hshmem.ini|

环境变量设置示例如下：

```shell
export HSHMEM_LOG_LEVEL=debug
export HSHMEM_CONFIG_PATH=/home/yjc/test/hshmem.ini
```

## 错误码

**表 1**  错误码说明

|错误码数|错误码|含义（中文）|含义（英文）|
|--|--|--|--|
|1|H_FAIL|失败|fail|
