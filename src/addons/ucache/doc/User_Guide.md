# 1 UCache 简介

针对裸机容器场景，基于灵衢总线构建全局PageCache池，I/O敏感应用节点通过灵衢总线借用邻居节点扩充可用PageCache空间，并且在Linux内核PageCache顺序预取的基础上，优化不同文件顺序读场景的性能，提升存算一体型应用的IO效率。

# 2 UCache 安装&部署

## 2.1 须知

- UCache组件由UCache UBS Engine插件与UCache UBTurbo插件组成。
- 需要已安装SMAP、UBS Engine、UB Turbo。
- 需要关闭文件页透明大页设置，执行以下命令。
```bash
echo never > /sys/kernel/mm/transparent_hugepage/file_enabled
```


## 2.2 UBS Engine UCache组件安装与启动

执行以下命令安装UBS Engine UCache组件

```bash
rpm -ivh ucache-*.aarch64.rpm
```

检查库文件、配置文件、头文件路径如下所示：
```bash
/usr/lib64/libucache_plugin.so
/etc/ubse/plugins/plugin_ucache.conf
```
参数配置说明
```bash
ubse.plugin.name=ucache
ubse.plugin.pkg=libucache_plugin.so
# 采集间隔
ucache.export.interval=10
# 借用策略粒度
strategy.borrow_size=1073741824
# 借用策略最大输出动作数量
strategy.maxActionSize=10
# 借用策略平衡阈值
strategy.balanceThreshold=1.5
# 借用策略最小借用稀缺度
strategy.scarcityThreshold=0.01
# 瓶颈识别 - 阈值（类型：uint32, 单位：kb/s, 默认配置：10240 kb/s）
bottleneck.threshold=10240
# 瓶颈识别 - 短窗口长度（类型：uint32，单位：秒，默认配置：180秒）
bottleneck.short.size=180
# 瓶颈识别 - 短窗口阈值（类型：uint32，单位：百分比，默认配置：70%）
bottleneck.short.threshold=70
# 瓶颈识别 - 长窗口长度（类型：uint32，单位：秒，默认配置：600秒）
bottleneck.long.size=600
# 瓶颈识别 - 长窗口阈值（类型：uint32，单位：百分比，默认配置：30%）
bottleneck.long.threshold=30
# 借用策略数据最大可靠周期
strategy.maxReliableTimes=3
# master主流程间隔时间（s）
master.masterInterval=10
# 最大借用内存量，Byte
master.maxBorrowSize=1073741824
```

参数含义说明如下表所示
| 参数名称                       | 参数值                                                                 | 说明                                                                                                   |
| ------------------------------ | ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| ubse.plugin.name               | 默认值：ucache                                                         | ucache插件名称。                                                                                       |
| ubse.plugin.pkg                | 默认值：libucache_plugin.so                                            | 包名。                                                                                                 |
| ucache.export.interval         | 默认值：2<br>单位：秒<br>配置范围：> 0                                 | 采集间隔，整数型。<br>*参数不在配置范围，插件初始化失败。* |
| strategy.borrow_size           | 默认值：1073741824<br>单位：字节<br>配置范围：[134217728,4294967296]   | 用16进制表示，单次借用或归还动作操作的最小内存大小。<br>*参数不在配置范围，插件初始化失败。* |
| strategy.maxActionSize         | 默认值：10<br>配置范围：>=0                                            | 借用策略动作集动作的最大数量，整数型。<br>*参数不在配置范围，插件初始化失败。* |
| strategy.balanceThreshold      | 默认值：1.5<br>配置范围：>=1                                           | 借用策略中平衡阈值，节点的平衡状态如果大于平衡阈值将会发生借用调度来平衡系统，浮点型。<br>*参数不在配置范围，插件初始化失败。* |
| strategy.scarcityThreshold     | 默认值：0.01<br>配置范围：>=0                                          | 发起借用动作时的最小内存稀缺度，浮点型。<br>*参数不在配置范围，插件初始化失败。* |
| bottleneck.threshold           | 默认值：10240<br>单位：KB/s<br>配置范围：[1,10*1024*1024]              | 应用瓶颈识别的阈值，来判断PageCache in和IO读写带宽，整数型。<br>*参数不在配置范围，则采用默认值。* |
| bottleneck.short.size          | 默认值：180<br>单位：秒<br>配置范围：[30,1200]                         | 应用瓶颈识别的短窗口长度，整数型。<br>*参数不在配置范围，则采用默认值。* |
| bottleneck.short.threshold     | 默认值：70<br>单位：百分比<br>配置范围：[0,100]                        | 应用瓶颈识别的短窗口阈值，整数型。<br>*参数不在配置范围，则采用默认值。* |
| bottleneck.long.size           | 默认值：600<br>单位：秒<br>配置范围：[90,3600]                         | 应用瓶颈识别的长窗口长度，整数型。<br>*参数不在配置范围，则采用默认值。* |
| bottleneck.long.threshold      | 默认值：30<br>单位：百分比<br>配置范围：[0,100]                        | 应用瓶颈识别的长窗口阈值，整数型。<br>*参数不在配置范围，则采用默认值。* |
| strategy.maxReliableTimes      | 默认值：3<br>配置范围：[1,10]                                          | 借用策略数据最大可靠周期数，每个周期的时间由master.masterInterval 决定，整数型。<br>*参数不在配置范围，则采用默认值。* |
| master.masterInterval          | 默认值：3<br>单位：秒<br>配置范围：[1,15]                              | master主流程间隔时间，整数型。                                                                         |
| master.maxBorrowSize           | 默认值：1073741824<br>单位：字节<br>配置范围：strategy.borrow_size的正整数倍 | 最大借用内存量，整数型。<br>*参数不在配置范围，插件初始化失败。* |


修改/etc/ubse/ubse_plugin_admission.conf，启用“ucache=778”
```bash
# vm plugin. Default value: 205
# vm=205
# mempooling plugin. Default value: 777
# mempooling=777
# ucache plugin. Default value: 778
ucache=778
```
新增参数说明
| 序号 | 参数 | 说明 | 取值 | 配置节点 | 应用场景 |
| - | - | - | - | - | - |
| 1 | ucache | UCache插件 | 默认值：778 | 所有节点 | 通算大数据场景 |