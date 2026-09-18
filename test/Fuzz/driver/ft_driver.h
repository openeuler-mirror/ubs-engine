/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef FT_DRIVER_H
#define FT_DRIVER_H

/*
 * DT Fuzz 通用驱动（不依赖 libFuzzer，gcc/clang 均可用）。
 *
 * harness 侧需要实现（与 libFuzzer 约定一致，可无缝迁移到 -fsanitize=fuzzer）：
 *   extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
 *     - 返回 0 表示正常；非 0 表示 harness 主动报告发现异常，driver 会保存输入并退出。
 *
 * 可选实现（弱符号，由本驱动在语料目录为空时调用，用于生成结构合法的种子）：
 *   extern "C" int FtGenSeeds(const char* corpusDir);
 *     - 向 corpusDir 写入若干种子文件，返回写入个数；不需要时可不实现。
 */

/*
 * driver 命令行：
 *   --runs=N          最大执行次数（与 --time 先到为准），默认 30000000（3000万次）
 *   --time=S          最大运行秒数，默认 10800（3小时）
 *   --corpus=DIR      语料目录（默认 ./ft_corpus_<程序名>，自动创建；为空时调用 FtGenSeeds）
 *   --crash-dir=DIR   崩溃/挂起输入保存目录（默认 ./ft_crash_<程序名>）
 *   --max-len=N       单次输入最大长度，默认 4096
 *   --hang-timeout=MS 单次执行无进度超时毫秒数，超过判定挂起，默认 10000
 *   --replay=FILE     只执行一次指定文件（用于复现崩溃输入）
 *   --quiet           关闭周期性进度输出
 *
 * 执行模型：
 *   fork 批处理——父进程负责调度/监控，子进程批量执行（默认每批 1 万次）。
 *   子进程崩溃（信号）或 harness 返回非 0 计为 saved_crashes；
 *   子进程单次执行无进度超过 --hang-timeout 计为 saved_hangs；
 *   崩溃/挂起输入自动落盘到 --crash-dir（crash-N / hang-N，上限 1000 个，超出只计数）。
 *   计数不中断运行，结束时输出汇总：
 *     run_time / execs_done / execs_per_sec / saved_crashes / saved_hangs
 *
 * 退出码：
 *   0   正常达到 --runs 或 --time 限额，且无崩溃无挂起
 *   77  运行结束但发现崩溃或挂起（输入已保存，--replay 可复现）
 *   其他 driver 自身错误
 */

#endif // FT_DRIVER_H
