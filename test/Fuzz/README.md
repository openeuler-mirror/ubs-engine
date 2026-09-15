# DT Fuzz 测试框架设计文档

## 1. 概述

DT Fuzz（开发者测试 + Fuzz）用于验证 virt_agent 对外接口在恶意/畸形输入下的健壮性。
框架由自研 fork 批处理 driver 与若干攻击面 harness 组成，不依赖 libFuzzer，GCC/Clang 均可用。

**核心验收指标：崩溃数与挂起数**（每个接口 3000 万次执行或 3 小时连续测试，先到为准）。

## 2. 设计思路

### 2.1 攻击面划分

测试目标限定为**外部输入解析边界**（接口暴露面）：IPC body 反序列化、JSON 解析、SDK unpack
等把不可信字节流转化为内部结构的代码。`Deserialize()` 之后的业务逻辑（libvirt/HTTP/节点
状态等重依赖）不属于 Fuzz 范围，由 UT/ST 覆盖。

virt_agent 共注册 24 个北向接口，按输入解析方式归为 4 类攻击面，对应 4 个 harness：

| harness | 攻击面 | 输入构造 |
|---|---|---|
| `ft_va_msg_deser` | `BaseMessage` 各子类 `Deserialize()`（30+ 消息类） | byte[0] 选择消息类，其余为 IPC body |
| `ft_va_case_conf_json` | `VMJsonUtil`（`FromJson`/`VMConvertJsonStr2Map`/`SafeStof`） | 字节流直接作为 JSON 串 |
| `ft_va_sdk_unpack` | daemon 响应 → C 结构体（`ubs_virt_agent_case_conf_helper` 等） | 按接口码选择 unpack 函数 |
| `ft_va_ipc_raw` | `ubse_invoke_call` 全链路（序列化 → socket 收发 → 响应解析） | fuzz 输入作为伪造 daemon 响应报文 |

### 2.2 执行模型：fork 批处理

父进程负责调度与监控，子进程批量执行（默认每批 1 万次）：

- 子进程被信号杀死 → `saved_crashes`；harness 返回非 0 → `saved_crashes`；
- 子进程单次执行无进度超过 `--hang-timeout`（默认 10s）→ `saved_hangs`；
- 崩溃/挂起输入自动落盘到 `ft_crash_<harness>/`（`crash-N` / `hang-N`，上限 1000 个，超出只计数）；
- 计数不中断运行，结束后输出 `run_time / execs_done / execs_per_sec / saved_crashes / saved_hangs`。

选择 fork 模型的原因：崩溃只终止子进程，主流程可继续积累执行次数并保存引发崩溃的输入；
代价是吞吐从百万级降至 2.5~25 万次/秒（`ft_va_ipc_raw` 走完整 IPC 链路，约 2.6 万次/秒），
可满足 3000 万次/3 小时的测试要求。

### 2.3 harness 约定

harness 只需实现与 libFuzzer 一致的入口（未来可无缝迁移 `-fsanitize=fuzzer`）：

```c
// 必须实现：返回 0 正常；非 0 表示主动报告异常，driver 保存输入并计数
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// 可选（弱符号）：语料目录为空时由 driver 调用，生成结构合法的种子
extern "C" int FtGenSeeds(const char* corpusDir);
```

种子采用"典型 TLV 结构猜测 + 边界值模板"（空长度、超长长度、0/2 计数等），之后由 driver
按字节翻转/切片/交叉等策略变异。每轮测试从零语料开始（脚本先清理 `ft_corpus_*` / `ft_crash_*`）。

### 2.4 单接口模式

`FT_MSG_SELECTOR=<N>` 环境变量固定消息类型选择器，只攻击第 N 号消息类（变异仍作用于全部
字节），满足"单接口 3000 万次"的口径要求。

### 2.5 工程要点

- **日志静音**：harness 内通过 `UbseIpcLog::SetLogFunc` 注册空 sink，避免错误路径高频打日志
  拖慢执行（修复前某 harness 30M 次产生约 5GB 日志、耗时翻 3 倍）；
- **fork 前刷缓冲**：driver 在 fork 前 `fflush(nullptr)`，防止子进程重复输出父进程未刷新的缓冲；
- **覆盖率落盘**：`FT_COVERAGE=ON` 时编译期注入 `FT_ENABLE_GCOV` 宏，fork 子进程 `_exit`
  前强引用 `__gcov_dump` 手动落盘 gcda（弱符号方案无法从静态库提取成员，不可用）；
- **防优化**：getter 结果通过 volatile sink 消费，确保数据搬运路径真实执行。

## 3. 目录结构

```
test/Fuzz/
├── README.md               # 本文档
├── CMakeLists.txt           # cmake -DBUILD_TESTS=ON -DENABLE_FT=ON 启用
├── driver/
│   ├── ft_driver.h          # driver 接口约定（参数、退出码、执行模型）
│   ├── ft_driver.cpp        # fork 批处理驱动实现
│   └── CMakeLists.txt
├── virt_agent/              # 4 个攻击面 harness
│   ├── ft_va_msg_deser.cpp
│   ├── ft_va_case_conf_json.cpp
│   ├── ft_va_sdk_unpack.cpp
│   ├── ft_va_ipc_raw.cpp
│   └── CMakeLists.txt
├── scripts/
│   ├── run_3harness.sh      # 全量 30M 重跑（输出到 results/）
│   ├── gcov_summary.sh      # 按 harness 采集覆盖率并追加到 results/*.log
│   └── ft_coverage.sh       # lcov/gcov 覆盖率汇总（通用，可选 HTML）
└── results/                 # 各 harness 最终日志（*_final.log，运行时由脚本生成）
```

## 4. 使用教程

在 Linux 环境（openEuler / WSL Ubuntu，GCC 工具链）执行。

### 4.1 构建

```bash
cd <仓库根目录>

# 全量运行构建（无覆盖率插桩，执行速度快）
cmake -S . -B build-ft -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_TESTS=ON -DENABLE_FT=ON
cmake --build build-ft --target \
      ft_va_msg_deser ft_va_case_conf_json ft_va_sdk_unpack ft_va_ipc_raw

# 覆盖率构建（FT_COVERAGE=ON 隐式开启全局 gcov 插桩）
cmake -S . -B build-ft-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_TESTS=ON -DENABLE_FT=ON -DFT_COVERAGE=ON
cmake --build build-ft-cov --target \
      ft_va_msg_deser ft_va_case_conf_json ft_va_sdk_unpack ft_va_ipc_raw
```

产物为 `build-ft*/bin/` 下 4 个 harness 可执行文件。

### 4.2 全量运行（验收口径）

```bash
bash test/Fuzz/scripts/run_3harness.sh
```

脚本对每个 harness 依次执行（driver 默认 `--runs=30000000 --time=10800`，先到为准），
结果写入 `test/Fuzz/results/<harness>_final.log`。
路径均由脚本位置自动推导，跨环境无需修改；构建目录非默认值时可通过
`FT_BUILD_DIR`（run_3harness.sh）/ `FT_COV_BUILD_DIR`（gcov_summary.sh）环境变量覆盖。

### 4.3 常用指令

```bash
cd build-ft

# 单 harness 自定义参数运行
./bin/ft_va_msg_deser --runs=1000000 --time=600

# 复现崩溃输入（driver 退出码 77 表示发现崩溃/挂起）
./bin/ft_va_msg_deser --replay ft_crash_ft_va_msg_deser/crash-0

# 单接口模式：固定消息类型选择器，只 fuzz 某个消息类
FT_MSG_SELECTOR=0 ./bin/ft_va_msg_deser --runs=1000000 --quiet
```

driver 完整参数：`--runs` / `--time` / `--corpus` / `--crash-dir` / `--max-len`（默认 4096）/
`--hang-timeout`（默认 10s）/ `--replay` / `--quiet`，详见 [ft_driver.h](driver/ft_driver.h)。

### 4.4 覆盖率采集（辅助参考）

```bash
bash test/Fuzz/scripts/gcov_summary.sh
# 可选：生成 HTML 覆盖率报告（需 lcov/genhtml，缺失时自动跳过）
bash test/Fuzz/scripts/ft_coverage.sh build-ft-cov src/addons/virt_agent --html
```

`gcov_summary.sh` 逐 harness 清空 gcda、短跑 100 万次（覆盖率已饱和）、逐 gcda 调用
`gcov -t -f` 解析（排除 std/编译器生成符号、跨编译单元去重），将结果追加到 `results/*.log`。

### 4.5 新增 harness

1. 在 `test/Fuzz/<模块>/` 下新建 cpp，实现 `LLVMFuzzerTestOneInput`（必要时实现 `FtGenSeeds`）；
2. 在同目录 `CMakeLists.txt` 中 `add_executable` 并链接 `ft_driver`；
3. 若 harness 命中错误日志路径，注册空日志 sink；
4. 追加到 `run_3harness.sh` 的目标列表。

## 5. 预期结果

| 项 | 要求 |
|---|---|
| 执行次数 | 每 harness ≥ 3000 万次，或连续运行 3 小时（先到为准） |
| 崩溃（saved_crashes） | **0**（核心指标） |
| 挂起（saved_hangs） | **0**（核心指标） |
| 结果输出 | 每个 harness 单独一份 `results/<harness>_final.log` |
| 退出码 | 0 = 正常完成且无崩溃无挂起；77 = 发现崩溃/挂起（输入已落盘可 replay） |
