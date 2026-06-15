# 配置项说明

## 配置原则说明

1. UBS VirtAgent的配置能力通过文件的方式暴露
2. UBS VirtAgent配置文件采用ini文件格式：

    - 由键（key）和值（value）构成

    - 配置文件包含多个键值对

    - 每个键值对用等号（**=**）进行连接；左边为键，作为配置项的名称；右边为值，作为配置项内容

3. 配置项设计，各节点配置独立
4. 配置后需要重启节点UBS Engine进程（UBS VirtAgent属于UBS Engine的插件，在同一进程内）

UBS VirtAgent配置文件样例如下：

```conf
# Name of the virt_agent plugin. Fixed value: virt_agent.
ubse.plugin.name=virt_agent
# Name of the SO file on which the virt_agent module depends. Fixed value: libvirtagent.so.
ubse.plugin.pkg=libvirtagent.so
# Collection period of the VM module, in seconds. The value range is [1, 60].Any invalid value will be reset to 2.
ubse.plugin.vm.export.interval=2

# Decision configuration item
# Second escape watermark of the decision-making module. A value greater than the second escape watermark will trigger memory borrowing. The value range is [70, 95](and borrow.watermark >= high.watermark + 5).Any invalid value will be reset to 92.
borrow.watermark=92
# High and low watermark percentage configuration with a value range of [0, 100]. A high watermark triggers borrowing or migration, and a low watermark triggers returning. The value is configured to the memory subsystem and used for decision-making modules.
# [Only for virtualization scenario] High watermark percentage configuration with a value range of [65, 90](and high.watermark >= low.watermark + 5)
# [Only for virtualization scenario] Low watermark percentage configuration with a value range of [60, 80]
high.watermark=85
low.watermark=80

```

## 配置详细说明

配置项说明：

| 序号  | 参数                              | 说明                                                | 取值                                                                                                                                                               |
|:----|:--------------------------------|:--------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1   | ubse.plugin.name                | VM模块插件名。                                          | 固定值为virt_agent。                                                                                                                                                  |
| 2   | ubse.plugin.pkg                 | VM模块依赖的so文件名称。                                    | 固定值为libvirtagent.so。                                                                                                                                             |
| 3   | ubse.plugin.vm.export.interval  | VM模块数据采集周期。                                       | 默认值：2<br>单位：s<br>取值范围：[1, 60]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                        |
| 4   | borrow.watermark                | 决策模块第二逃生水线占比配置，高于会触发内存借用。                         | 默认值：92<br>单位：%<br>取值范围：[70, 95]<br>且high.watermark <= borrow.watermark - 5<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                           |
| 5   | high.watermark                  | 高水线百分比，高触发借用或迁移，低触发归还，且用于决策模块。                    | 默认值：85<br>单位：%<br>取值范围：[65, 90]<br>且low.watermark +5 <= high.watermark <= borrow.watermark - 5<br>参数配置取值范围之外或错误值都会被重置为默认值。                                       |
| 6   | low.watermark                   | 低水线百分比，高触发借用或迁移，低触发归还，且用于决策模块。                    | 默认值：80<br>单位：%<br>取值范围：[60, 80]<br>且low.watermark +5 <= high.watermark<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                               |
| 7   | borrow.maxMemPerBorrowSize      | 单块借用内存的借用量上限。                                     | 默认值：4096<br>单位：MB<br>取值范围：{1024,2048,3072,4096}<br>且borrow.minMemPerBorrowSize <= borrow.maxMemPerBorrowSize<br>参数配置取值范围之外或错误值都会被重置为默认值。                         |
| 8   | borrow.minMemPerBorrowSize      | 单块借用内存的借用量下限。                                     | 默认值：1024<br>单位：MB<br>取值范围：{1024,2048,3072,4096}<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                      |
| 9   | borrow.maxPerTotalMemBorrowSize | 单次决策最多内存借用上限。                                     | 默认值：16384<br>单位：MB<br>取值范围：[4096, 20480]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                             |
| 10  | borrow.oomBorrowMemSize         | oom事件期望借用内存大小。                                    | 默认值：1024<br>单位：MB<br>取值范围：[1024,4096]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                |
| 11  | mig.ham.maxTimeout              | 单个VM确定性热迁移的任务的超时时间。                               | 默认值：60<br>单位：s<br>取值范围：[10, 10800]<br>参数配置取值范围之外或错误值都会被重置为默认值。                                                                                                   |
| 12  | escape.algorithm.dir            | 自定义逃生算法so文件的绝对路径，配置中应包含so文件名称。由配置方保证加载so文件的安全可靠性。 | 默认值：/usr/lib64/libstrategy.so<br>支持用户配置为自己实现的算法so文件。                                                                                                             |
| 13  | mig.isEnableHamMigrate          | 是否支持确定性热迁移开关。                                     | 默认值：false<br>取值范围：<br>-false<br>-true<br>开启确定性热迁移，使用true。                                                                                                        |
| 14  | mig.migrateOneCopyMemoryBound   | UB代际下虚机热迁移决策OneCopy迁移策略的虚机内存阈值。                   | 默认值：4096<br>单位：MB<br>取值范围：[256,4096]<br>当虚机内存规格不大于mig.migrateOneCopyMemoryBound时，为OneCopy迁移；当开启确定性热迁移开关，目的节点为当前节点的邻居节点，虚拟机使用2M大页，节点间内存借用没有成环，且虚拟机内存规格大于mig.migrateOneCopyMemoryBound配置项的值时，为确定性热迁移；其他情况为MultiCopy迁移。<br>参数配置取值范围之外或错误值都会被重置为默认值。 |
| 15  | virt.pageType                   | 虚拟化场景页面类型配置。                                      | 默认值：1<br>取值范围：<br>0<br>1<br>用于设置冷内存迁出时的页面单位，标准设置为0，2M设置为1。                                                                                                       |
| 16  | virt.sceneType                  | 虚拟化场景类型应用配置。                                      | 默认值：1<br>取值范围：<br>0<br>1<br>容器场景设置为0，虚机场景设置为1。                                                                                                                   |
