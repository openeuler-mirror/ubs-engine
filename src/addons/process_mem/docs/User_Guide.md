# 1 ProcessMem 简介

ProcessMem 是 UBS Engine 的一款进程级内存调度插件。通过周期性扫描 `/proc` 发现纳管进程（支持按 PID、进程名配置，并自动发现子进程），采集进程在各 NUMA 节点上的内存分布与本地 NUMA 空闲内存，当本地空闲内存低于水位阈值时自动向远端 NUMA 借用内存并迁移进程页面（依赖 RMRS/SMAP 能力），本地空闲内存回升或收到借出节点的归还请求后自动迁回并归还。同时支持故障处理中的债务保留与重试、启动与运行期账本对账恢复（含 smap 实测回填与孤儿债务清理），以及 OOM 紧急借用与快速检测。

## 1.1 核心功能

- **进程纳管与发现**：支持按 PID 或进程名（comm）配置纳管进程；按进程名配置的进程调度配置持久化保存，同名进程启动时自动重新纳管；已纳管进程的调度参数按 PID 保存为配置快照，进程仍在但无法按名匹配（如进程改名、父进程已退出的子进程）或 UBSE 重启后仍可恢复纳管；自动发现纳管进程的子进程并继承父进程的调度参数；root 进程（uid 0）默认被过滤，不参与纳管。
- **进程 NUMA 内存采集**：周期性读取 `/proc/<pid>/numa_maps` 与 `/proc/<pid>/status`，获取进程在各 NUMA 节点上的内存分布与 VmRSS，并采集本地/远端 NUMA 内存水位快照。
- **自动借用与迁出**：本地 NUMA 总空闲内存低于 `pressing_free_threshold` 时，从纳管进程中筛选候选进程，向远端 NUMA 借用内存并通过 SMAP 迁移进程页面。
- **债务块容量复用**：超分比下调或进程内存回落后，再平衡将超出的远端页面迁回本地，债务槽容量保留而远端占用归零；进程内存再次上涨需要借用时优先复用这部分空闲容量，不新建债务。
- **自动迁回与归还**：空闲内存观测窗口最小值回升超过水位阈值时触发主动归还；借出节点空闲内存不足时向借入节点广播被动归还请求；本地空闲充足时页面迁回本地后归还，不足时支持远端到远端（R2R）迁移归还。
- **故障处理**：借出/归还受故障处理影响时保留借用、不误删，由后续周期自动重试回收；UBSE 重启或故障恢复后自动核对并恢复借用状态（补录未记录的借用、清理失效记录、按实测远端占用量校准），无法归属的借用自动归还。
- **配置持久化**：PID/进程名调度配置及进程配置快照通过 UbseStorage 持久化，UBSE 重启后自动恢复。

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

手动部署不会安装插件配置文件，需按 2.3.1 自行创建 `/etc/ubse/plugins/plugin_process_mem.conf`（RPM 安装方式会自动生成默认配置）。

安装后检查库文件、配置文件路径：

```bash
/usr/lib64/libprocess_mem.so
/etc/ubse/plugins/plugin_process_mem.conf
```

## 2.3 修改配置文件

### 2.3.1 插件主配置 `/etc/ubse/plugins/plugin_process_mem.conf`

```ini
# Name of the process_mem plugin. Fixed value: process_mem.
ubse.plugin.name=process_mem
# Name of the SO file on which the process_mem module depends. Fixed value: libprocess_mem.so.
ubse.plugin.pkg=libprocess_mem.so

[process_mem]
# Collect cycle: interval between /proc scans (valid range 1-3600s, fallback default 5)
collect_process_interval=5
# Default on: root processes (uid 0) are skipped when matching managed pids
filter_root_process=true
# Decision cycle: interval between borrow rounds (timeout check / borrow / passive return),
# valid range 1-3600s, fallback default 5
schedule_interval=5
# Free-memory threshold that triggers borrowing: borrow starts when total free of all local NUMA nodes
# drops below f, derived from capacity planning as f = S - k x d x r (valid range 1-4096 GB,
# fallback default 15; should not exceed total local memory and should be greater than
# emergency_free_threshold)
pressing_free_threshold=15
# Borrow timeout per 128MB block (ms), valid range 1-1800000 (30 min), fallback default 1000
borrow.timeout=1000
# Whether borrowing must stay on the same plane: true=must (strict filtering),
# false=prefer (soft, score-weighted)
borrow.must_same_plane=false

# Active return (optional, defaults apply)
[process_mem.return]
# Number of consecutive fast-poll cycles: node free is sampled only during emergency fast polling (200ms);
# every n samples the window minimum is settled, and active return is triggered when the window minimum
# exceeds pressing_free_threshold (default 300 x 200ms = 60s observation window);
# valid range 1-4096, fallback default 300
observe_cycles=300

# OOM fast detection (optional, defaults apply)
[process_mem.oom]
# Emergency fast-poll interval (ms), valid range 50-60000, fallback default 200
collect_node_interval=200
# Emergency borrow/pass-off threshold (GB), valid range 1-4096, fallback default 5;
# should be less than pressing_free_threshold
emergency_free_threshold=5
```

参数说明：

| 参数名称 | 默认值 | 单位 | 配置范围 | 说明 |
| -------- | ------ | ---- | -------- | ---- |
| ubse.plugin.name | process_mem | - | - | 插件名称。固定值：process_mem。 |
| ubse.plugin.pkg | libprocess_mem.so | - | - | 插件依赖的动态库文件名。固定值：libprocess_mem.so。 |
| collect_process_interval | 5 | 秒 | [1, 3600] | 进程发现与采集周期，周期内扫描 `/proc`、采集 VmRSS 与 NUMA 内存分布。参数不在配置范围内则采用默认值。 |
| filter_root_process | true | - | true/false | 是否过滤 root 进程（uid 0）。开启后 root 进程不纳管、不参与借用；PID 配置了 root 进程将被拒绝。 |
| schedule_interval | 5 | 秒 | [1, 3600] | 调度决策周期，周期内执行借用超时检查、借用决策与被动归还广播。参数不在配置范围内则采用默认值。 |
| pressing_free_threshold | 15 | GB | [1, 4096] | 本地所有 NUMA 节点总空闲内存低于 pressing_free_threshold 时触发借用，由容量规划推导：pressing_free_threshold = S − k × d × r。不应超过本地总内存，且应大于 emergency_free_threshold。参数不在配置范围内则采用默认值。 |
| borrow.timeout | 1000 | 毫秒/128MB | [1, 1800000]（30 分钟） | 每 128MB 借用量对应的超时时间，超时未完成的借用将被清理并归还。参数为 0 或超出范围则采用默认值。 |
| borrow.must_same_plane | false | - | true/false | true 表示借用必须落在同平面节点（严格过滤）；false 表示软偏好，按评分加权选择借出节点。 |
| observe_cycles | 300 | 次 | [1, 4096] | 连续快速轮询周期数：仅在紧急快速轮询（200ms）期间采样节点空闲内存，每 n 次采样结算一次观测窗口，窗口内空闲内存最小值高于 pressing_free_threshold 时触发主动归还（默认 300 × 200ms = 60s 观测窗口）。参数不在配置范围内则采用默认值。 |
| collect_node_interval | 200 | 毫秒 | [50, 60000] | OOM 紧急快速轮询间隔，用于本地空闲内存的持续监控。参数不在配置范围内则采用默认值。 |
| emergency_free_threshold | 5 | GB | [1, 4096] | 本地空闲内存低于该阈值时触发紧急借用并向借入节点广播被动归还请求；与 OOM 观测窗口配合，空闲内存回升（观测窗口最小值高于 pressing_free_threshold）时触发主动归还。应小于 pressing_free_threshold。参数不在配置范围内则采用默认值。 |

`pressing_free_threshold` 由容量规划推导，各变量含义：

| 变量 | 含义 | 单位 |
| ---- | ---- | ---- |
| S | 单节点可对外借出上限（nodeMaxLend），即 ubse.conf `[ubse.memory]` 段的 `scheduler.node_lending_limit` | GB |
| k | 单节点纳管实例数 | 个 |
| d | 单实例突发增量 = maxMemory − 基线内存 | GB/实例 |
| r | 同时突发的实例比例 | - |

- 简化形式：`pressing_free_threshold = S − k × d × r`
- 精确形式：`pressing_free_threshold = S − d × ⌈k × r⌉`（实例数向上取整）

### 2.3.2 插件准入配置 `/etc/ubse/ubse_plugin_admission.conf`

启用 process_mem 插件（使用预留的插件码 `234`）：

```ini
# process_mem plugin. Default value: 234
process_mem=234
```

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
grep process_mem /proc/$(pidof ubse)/maps
```

# 3 常用命令

ProcessMem 提供以下 CLI 命令用于进程调度配置。

## 3.1 设置进程调度配置

```bash
# 按 PID 配置
ubsectl change process-mem -p <pid> -s <size> -r <remote-ratio>

# 按进程名配置
ubsectl change process-mem -n <name> -s <size> -r <remote-ratio>
```

参数说明：

| 参数 | 短选项 | 说明 |
| ---- | ------ | ---- |
| --pid | -p | 目标进程 PID，范围 1 ~ 4194304 |
| --name | -n | 进程名（comm），最多 15 个字符，仅限字母/数字/点/冒号/下划线/连字符，与 --pid 互斥 |
| --size | -s | 进程内存大小（maxMemory），范围 128M ~ 32G，支持单位 B/K/M/G（二进制），最多 2 位小数（如 1.5G），省略单位时默认 MiB |
| --remote-ratio | -r | 远端内存最大占比，范围 0.0 ~ 1.0，最多 2 位小数 |

约束：

- `--pid` 与 `--name` 必须且只能指定其一。
- 按 PID 配置时进程必须存在，且 `filter_root_process=true` 时 root 进程（uid 0）不可配置。
- 按进程名配置时，后续新启动的同名进程会被自动发现并纳管。
- `--remote-ratio` 配置为 0.0 时，该进程不参与远端借用。

示例：

```bash
ubsectl change process-mem -p 12345 -s 2G -r 0.3
ubsectl change process-mem -n test_proc -s 1.5G -r 0.5
```

## 3.2 查询已纳管进程

```bash
# 查询已配置的纳管配置（PID/进程名、Size、RemoteRatio）
ubsectl display process-mem -t config

# 查询当前实际纳管（匹配到）的进程明细
ubsectl display process-mem -t proc_detail
```

输出以表格回显，列为 PID、Name、Size、RemoteRatio。`config` 回显所有持久化的纳管配置（按 PID 配置的 Name 显示为 N/A）；`proc_detail` 回显当前真正纳管的进程（按 PID 配置命中时 Name 固定为 N/A，否则显示实际匹配的配置名）。

## 3.3 取消进程纳管

```bash
ubsectl remove process-mem -p <pid>
ubsectl remove process-mem -n <name>
```

取消前会自动执行迁回与归还，清理借用内存。

# 4 调度流程

## 4.1 调度决策

ProcessMem 周期性运行三套任务：

- **采集**（间隔 `collect_process_interval`）：扫描系统进程，发现符合进程名配置的新进程并纳入管理，发现纳管进程的子进程并继承其调度参数，同时记录各纳管进程的内存占用与本地/远端 NUMA 内存水位。
- **调度决策**（间隔 `schedule_interval`），每轮执行以下判断：

1. 检查本地 NUMA 总空闲内存，低于 `pressing_free_threshold` 时计算缺口。计算时扣除**在途迁移量**：页面迁移需要时间，已发起但尚未到达远端的内存实际仍占用本地，这部分计入已借出量，避免缺口重复计算、借出超出实际需要。
2. 从纳管进程中筛选可借用候选：单个进程可迁移到远端的内存上限 = 当前内存占用 × `remoteRatio`，再减去已位于远端的内存和在途迁移部分；不足一个迁移块（blockSize）或未配置（maxMemory/remoteRatio 非法）的进程不参与。
3. 候选进程按优先级排序：普通进程优先于子进程、当前无在途借用优先、最近 60s 内未迁移过优先、实际占用超过预期（内存占用 > maxMemory × remoteRatio）优先、内存占用大的优先。
4. 按序为候选进程借出内存，直到缺口填满或候选耗尽。借出量按整块（blockSize）向上取整：缺口不足一块时也按一块借出（前提是进程仍有可迁移内存），因此实际借出量可能略大于缺口。借出前优先复用进程已有债务槽的空闲容量（见 4.2），复用成功时不新建债务。
5. 借用超时检查：借用发起后按借出量计算超时时间（每 128MB 对应 `borrow.timeout`），超时未完成的借用将被取消并归还。

- **OOM 快速检测**（间隔 `collect_node_interval`，默认 200ms）：持续快速轮询本地 NUMA 空闲内存，并以 `observe_cycles` 次采样（默认 300 次，约 60s）为一个观测窗口，记录窗口内空闲内存的最小值：
  1. 当前空闲内存低于 `emergency_free_threshold` 时，立即执行紧急借用（决策流程与普通借用一致），同时向借入节点广播紧急归还请求，请其尽快迁回页面。
  2. 观测窗口结算时，若窗口内空闲内存最小值高于 `pressing_free_threshold`，说明本地内存已持续回升，触发主动归还：归还额度 = 窗口最小值 − `pressing_free_threshold`。从账本中找出归还成本（实际迁移量）不超过剩余归还额度的账本，按迁移量从大到小逐笔迁回归还；迁移量为 0 的账本远端无数据，归还零成本，恒符合条件。

## 4.2 借用与迁出

1. **优先复用空闲容量**：借用前先检查本进程已完成债务槽的空闲容量（槽容量 − 已迁移量）。超分比下调或进程内存回落后，再平衡按「内存占用 × 超分比」削减各远端 NUMA 的迁出目标，超出部分页面迁回本地，债务槽容量保留、远端占用归零，即形成空闲容量。仅当空闲容量足以**完全覆盖**本次借用需求时复用：在 pid 锁内直接补齐账本占用，并按各远端 NUMA 的全量目标重新下发一次迁移，全程不新建债务；空闲容量不足以覆盖时不做部分复用，整体走新建债务流程（避免账本迁移量非块大小整数倍）。本进程存在其他在途借用时本轮跳过复用，留待下一轮重试；复用后下发失败时账本已更新而 smap 未生效，由对账流程按账本期望全量对齐自愈。
2. 为候选进程确定借用量，向远端 NUMA 借出内存，随后将进程页面迁移到远端。
3. 借出成功后，进程的远端占用随之增加。同一进程的迁移、迁回、归还操作串行执行，避免并发操作互相覆盖。
4. 迁移失败时：若借出节点正处于故障处理状态，本次借用予以保留（不撤销），待故障恢复后由归还流程自动回收；其他原因失败则撤销借用并归还已借出的内存。

## 4.3 归还与迁回

归还场景：

- **主动归还**：本地空闲内存持续回升（快速检测窗口内最小值高于 `pressing_free_threshold`）时，以"窗口最小值 − `pressing_free_threshold`"为归还额度，从账本中找出归还成本（实际迁移量）不超过剩余额度的账本逐笔归还；迁移量为 0 的账本（远端无数据）归还零成本，优先符合条件。
- **被动归还**：借出节点本地空闲低于 `pressing_free_threshold`（或紧急阈值 `emergency_free_threshold`）时，向借入节点发起归还请求，由借入方迁回并归还。
- **超时归还**：借用超时的借用直接取消并归还。
- **进程退出归还**：进程退出、取消纳管或 PID 被新进程复用（启动时间变化）时，归还该进程的全部远端内存。

归还路径：

1. 本地空闲充足时：先将页面从远端迁回本地，核对实际分布与预期一致后，再归还借用的内存。
2. 本地空闲不足时：将页面从原远端直接转移到其他可用远端（替换原借出节点），再归还原借用，腾出原借出节点的空间。
3. 归还失败的兜底：被动归还不主动重试，等待借出节点下一轮请求；其余场景按 1s 间隔重试（最多 100 次）；借用已不存在时视为归还成功，重复归还不影响结果。

## 4.4 故障处理

- 借出节点故障处理期间，相关借用/归还操作不会被误删或误撤，由后续周期自动重试，故障恢复后继续。
- UBSE 重启或故障恢复后，自动核对并恢复借用状态：补录已存在但未记录的借用、清理已失效的借用记录，并按各进程实际位于远端的内存量校准记录；无法归属的借用自动归还。
- PID 复用检测：纳管进程的启动时间与记录不一致时视为新进程，清空旧借用记录并归还旧进程的内存。

# 5 使用建议

- **建议关闭 swap**：部署节点建议关闭 swap（`swapoff -a`，并取消开机自动挂载），否则进程页面迁移到远端 NUMA 的过程中会不断触发页面换入换出，造成 swap 颠簸，影响迁移效率与业务性能。
- **迁移粒度约束**：页面迁移以迁移块（blockSize）为最小粒度，进程可迁移量不足一个迁移块时不参与借用。单个进程的可迁移上限 = 进程实际内存占用 × `remoteRatio`，节点内存紧张时应保证实际内存占用 × `remoteRatio` ≥ 一个迁移块（默认 128MB），否则进程无法借出远端内存；`--size` 配置的 maxMemory 不影响该上限的计算，仅用于配置合法性校验（为 0 的进程不参与借用）与优先级判定。迁移块大小由 ubse.conf 中 `[ubse.memory]` 段的 `obmm.memory.block.size` 配置（默认 128MB，范围 4 ~ 4096MB，numa借用粒度必须为128MB）。
- **调度算法建议配置空闲优先**：建议在 ubse.conf 的 `[ubse.memory]` 段配置 `scheduler.mode=free-priority`（空闲优先，默认值）。空闲优先算法优先选择空闲内存占比最高的节点作为借出节点，保证借出节点自身内存充足，降低被动归还发生的概率；若配置为 `reliability-priority`（可靠性优先），借出量会跨节点均衡，可能选中内存紧张的节点，增大被动归还概率。
- **阈值配置关系**：`emergency_free_threshold` 应小于 `pressing_free_threshold`，且 `pressing_free_threshold` 应小于本地总内存。否则可能出现借用持续无法触发，或空闲内存回升后永远达不到主动归还条件的情况。
- **绑 NUMA 进程暂不支持纳管**：当前版本暂不支持纳管绑定 NUMA 的进程（如通过 taskset、numactl 指定 CPU 或内存节点的进程），此类进程的页面迁移可能无效，请勿将其纳入纳管配置。借用/归还决策以本地 NUMA 总空闲内存为判断依据，不区分单个 NUMA 节点的空闲差异，无法为绑 NUMA 进程做针对性的 NUMA 节点级调度。
- **root 进程过滤**：`filter_root_process=true`（默认）时 root 进程（uid 0）不参与纳管，按 PID 配置 root 进程会被拒绝；确有需要纳管 root 进程时，可将该参数置为 false 后重启 UBSE 生效。
- **进程名配置**：进程名匹配的是进程的 comm（进程名），进程名超过 15 字符时无法配置；同名进程新启动时会自动纳管，无需重复配置。

# 6 内存开销估算与 MemoryMax 配置

UBSE 以 systemd 服务（`ubse.service`）运行，进程内存上限由 unit 中的 `MemoryMax` 控制（默认 `256M`）。`MemoryMax` 约束的是 cgroup 口径的占用（`memory.current`，含页缓存与内核内存，即 `systemctl status` 的 Memory 行、`systemctl show ubse -p MemoryCurrent` 的读数），与进程 PSS/RSS 口径不同。

- **超出上限的后果**：UBSE 进程会被 cgroup 直接杀死（由 `Restart=on-failure` 重启），内存借用业务随之中断，务必预留余量。
- **调整方法**：修改 `/usr/lib/systemd/system/ubse.service` 中的 `MemoryMax`（建议用 `systemctl edit ubse` 创建 drop-in，避免升级时被覆盖），执行 `systemctl daemon-reload && systemctl restart ubse` 生效。
- **是否需要调整**：部署插件后用 `systemctl show ubse -p MemoryCurrent` 观察实际占用，接近 `MemoryMax` 时按实测值并预留余量上调；也可先按下文估算判断。

下文估算为可选参考，用于判断默认 `256M` 是否够用，无需精确。

**基线占用**：未启用插件、空跑 UBSE 时的占用随集群节点数增长，可按 `baseline(MB) ≈ 50 + 24 × (n − 1)` 估算（n 为集群节点数，cgroup 口径）；也可在目标节点停用插件后直接读取 `systemctl show ubse -p MemoryCurrent` 核对。

process_mem + rmrs redis 超分的内存借用场景下，开启插件后相对该基线（未启用任何插件、空跑 UBSE）的内存增量可用下式估算（进程 PSS 口径，由 `/proc/<pid>/smaps_rollup` 汇总实测拟合）：

`ΔPSS(MB) ≈ 5.1MB + 0.101MB/GB × B + 0.196MB × N`

| 变量 | 含义 | 单位 |
| ---- | ---- | ---- |
| N | 纳管实例数 | 个 |
| B | 远端 NUMA 上实际借出的容量 | GB |

该场景下 UBSE 进程的预计总占用为：

`占用(MB) ≈ baseline(n) + 5.1MB + 0.101MB/GB × B + 0.196MB × N`

其中 `baseline(n)` 为上文按集群节点数估算（或目标节点实测）的空跑 UBSE 占用，`5.1MB + 0.101MB/GB × B + 0.196MB × N` 为插件增量。插件增量按 PSS 口径拟合，cgroup 口径的增量通常略高于该值，据此判断 `MemoryMax` 余量时应适当放宽。

- 5.1MB 为插件空载开销（含 mempooling 与 process_mem，本场景两插件同时启用）；其余两项折算约 13KB/128MB 借出块、约 200KB/纳管实例。
- 判断余量时按 `baseline(n) + 插件增量` 与 `MemoryMax` 比较：如 3 节点（baseline ≈ 98MB）、100 实例、借出 100GB（增量 ≈ 35MB）时约 133MB，默认 `256M` 足够；节点数较多时应按式核算（如 8 节点 baseline ≈ 218MB，加上增量后已接近 256M），必要时提前上调。

# 7 日志

ProcessMem 插件日志文件路径：

```bash
/var/log/ubse/process_mem_plugin.log
```

# 8 卸载

```bash
rpm -evh process_mem

# 手动清理
rm -f /usr/lib64/libprocess_mem.so
rm -f /etc/ubse/plugins/plugin_process_mem.conf
```

> 卸载前需先从 `/etc/ubse/ubse_plugin_admission.conf` 中移除或注释 `process_mem=234` 配置行，然后重启 UBSE。
