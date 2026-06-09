# 1 ProcessMem 简介

ProcessMem 是 UBS Engine 的一款进程级内存调度插件。通过周期性采集指定进程在各 NUMA 节点上的内存分布，结合用户配置的驱逐/回收水位阈值，自动触发远端内存借用与进程页面迁移（依赖 RMRS/SMAP 能力），以及内存水位回落后的迁回与归还。同时支持借出节点故障隔离与恢复后自动恢复调度。

## 1.1 核心功能

- **进程 NUMA 内存采集**：周期性读取 `/proc/<pid>/numa_maps`，获取进程在各 NUMA 节点上的内存分布。
- **自动借用与迁出**：当进程总内存使用量超过驱逐阈值时，自动向远端 NUMA 借用内存并通过 SMAP 迁移进程页面。
- **自动迁回与归还**：当进程总内存使用量回落至回收阈值以下时，自动迁回页面并归还借用内存。
- **子进程自动发现**：支持自动发现并纳管已配置进程的子进程，继承父进程的调度策略。
- **故障处理**：借出节点发生 Panic/Reboot 时，自动标记受影响进程为 FAULT 状态并隔离；借出节点恢复后自动恢复调度。
- **配置持久化**：进程调度配置通过 UbseStorage 持久化，UBSE 重启后自动恢复。

# 2 ProcessMem 安装与部署

## 2.1 须知

- ProcessMem 是 UBS Engine 的插件，需要已安装 UBS Engine。
- ProcessMem 依赖 RMRS（libmempooling.so）提供的 SMAP 迁移能力，需要已安装 RMRS 插件。
- 需要以 root 用户执行安装与配置操作。

## 2.2 安装 ProcessMem

### 方式一：RPM 包安装

```bash
rpm -ivh process_mem-*.aarch64.rpm
```

### 方式二：手动部署

将编译产物 `libprocess_mem.so` 放置到 `/usr/lib64/`：

```bash
cp libprocess_mem.so /usr/lib64/
```

安装后检查库文件、配置文件路径：

```bash
/usr/lib64/libprocess_mem.so
/etc/ubse/plugins/plugin_process_mem.conf
```

## 2.3 修改配置文件

### 2.3.1 插件主配置 `/etc/ubse/plugins/plugin_process_mem.conf`

```ini
ubse.plugin.name=process_mem
ubse.plugin.pkg=libprocess_mem.so

# 进程 NUMA 内存采集间隔
process_mem.pid.collect.interval=10

# 内存借用超时时间
process_mem.pid.borrow.timeout=30
```

参数说明：

| 参数名称 | 默认值 | 单位 | 配置范围 | 说明 |
| -------- | ------ | ---- | -------- | ---- |
| ubse.plugin.name | process_mem | - | - | 插件名称。 |
| ubse.plugin.pkg | libprocess_mem.so | - | - | 插件动态库包名。 |
| process_mem.pid.collect.interval | 10 | 秒 | [1, 3600] | 进程 NUMA 内存分布采集间隔。参数不在配置范围内则采用默认值。 |
| process_mem.pid.borrow.timeout | 30 | 秒 | > 0 | 内存借用超时时间，超时未完成的借用将被清理。参数为 0 则采用默认值。 |

### 2.3.2 插件准入配置 `/etc/ubse/ubse_plugin_admission.conf`

启用 process_mem 插件（使用预留的插件码 `234`）：

```ini
# process_mem plugin. Default value: 234
process_mem=234
```

新增参数说明：

| 序号 | 参数 | 说明 | 取值 | 配置节点 | 应用场景 |
| ---- | ---- | ---- | ---- | -------- | -------- |
| 1 | process_mem | ProcessMem 插件 | 默认值: 234 | 所有节点 | 进程级内存调度场景 |

## 2.4 启动服务

ProcessMem 作为 UBS Engine 插件，随 UBSE 服务启动：

```bash
sudo systemctl restart ubse
```

## 2.5 验证部署

检查插件是否加载成功：

```bash
# 查看 process_mem 插件日志
tail -f /var/log/ubse/process_mem_plugin.log
```

ubse.log 中应能看到 process_mem 初始化成功的信息。

```bash
# 检查 libprocess_mem.so 是否被 UBSE 进程加载
ldconfig -p | grep process_mem
```

# 3 常用命令

ProcessMem 提供以下 CLI 命令用于进程调度配置。

## 3.1 设置进程调度阈值

```bash
ubse-cli change process-mem -p <pid> -e <evict-thresh> -t <target-evict-thresh> -r <reclaim-thresh> -s <size> [--sn <src-numa>]
```

参数说明：

| 参数 | 短选项 | 说明 |
| ---- | ------ | ---- |
| --pid | -p | 目标进程 PID，范围 1 ~ 4194304 |
| --evict-thresh | -e | 驱逐阈值（百分比），范围 1 ~ 100 |
| --target-evict-thresh | -t | 目标驱逐阈值（百分比），范围 1 ~ 100 |
| --reclaim-thresh | -r | 回收阈值（百分比），范围 1 ~ 100 |
| --size | -s | 预期内存使用量，范围 128M ~ 32G，支持单位 B/K/M/G，最多 2 位小数（如 1.5G） |
| --src-numa | --sn | 进程所在 NUMA ID（可选），不指定则自动选择内存占用最大的 NUMA |

约束：`evict-thresh` 必须至少比 `reclaim-thresh` 大 5，防止来回振荡。

示例：

```bash
ubse-cli change process-mem -p 12345 -e 80 -t 60 -r 50 -s 2G
```

## 3.2 查询已纳管进程

```bash
ubse-cli display process-mem -t config
```

输出以表格回显所有已纳管进程的 PID、evictThreshold、targetEvictThreshold、reclaimThreshold、expectedMemoryUsage 和 srcNuma。

## 3.3 取消进程纳管

```bash
ubse-cli remove process-mem -p <pid>
```

取消前会自动执行迁回与归还，清理借用内存。

# 4 调度流程

## 4.1 调度决策

每个采集周期，ProcessMem 对每个纳管进程执行以下判断：

1. 计算进程总内存（本地 + 远端）占 `expectedMemoryUsage` 的百分比。
2. 若百分比 > `evictThreshold`：触发借用与迁出。
3. 若百分比 < `reclaimThreshold`：触发迁回与归还。
4. 其他情况：维持当前状态。

## 4.2 借用与迁出

1. 检查是否存在超时的 CREATING 状态账本，存在则清理。
2. 计算当前已借用总量，若不足则发起新借用。
3. 借用成功后，通过 RMRS 的 `UBSRMRSMigrateOut` 将进程页面迁移到远端 NUMA。

## 4.3 归还与迁回

1. 通过 RMRS 的 `UBSRMRSMigrateOut`（迁回模式）将远端页面迁回本地。
2. 通过 RMRS 的 `UBSRMRSRemove` 删除迁移关系。
3. 通过 `UbseMemNumaDelete` / `UBSRMRSMemFreeWithMigrate` 归还借用内存。

## 4.4 故障处理

- 借出节点 Panic/Reboot 时：通过 RAS 告警回调通知所有借入节点，标记受影响进程为 FAULT 状态。
- FAULT 状态下暂停该进程的所有借用/归还操作。
- 每个采集周期检查故障节点是否恢复（账本清理 + 集群状态 WORKING），恢复后自动将进程状态重置为 IDLE 并恢复调度。

# 5 日志

ProcessMem 插件日志文件路径：

```bash
/var/log/ubse/process_mem_plugin.log
```

# 6 卸载

```bash
rpm -evh process_mem

# 手动清理
rm -f /usr/lib64/libprocess_mem.so
rm -f /etc/ubse/plugins/plugin_process_mem.conf
```

> 卸载前需先从 `/etc/ubse/ubse_plugin_admission.conf` 中移除或注释 `process_mem=234` 配置行，然后重启 UBSE。
