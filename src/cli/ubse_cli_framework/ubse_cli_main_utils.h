/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_CLI_MAIN_UTILS_H
#define UBSE_CLI_MAIN_UTILS_H

#include <cstddef>
#include <string>
#include <vector>

#include "ubse_cli_reg.h"

namespace ubse::cli::framework {
/**
 * @brief 在读取argv前校验进程启动参数。
 *
 * CLI接受的argc范围为[1, 43]，且要求argv非空。该函数用于统一保留原
 * ubsectl和ubsectl-ssu的启动保护逻辑。
 *
 * @param argc main函数接收到的参数个数。
 * @param argv main函数接收到的参数数组。
 * @return 校验成功返回UBSE_OK；打印错误信息后返回UBSE_ERROR。
 */
int UbseCliValidateStartupConditions(int argc, char *argv[]);

/**
 * @brief 构造注册表解析器使用的CLI参数列表。
 *
 * argv[0]是可执行文件名，会被有意跳过。extraReserve用于ubsectl-ssu等
 * 需要注入隐式type的专用CLI。
 *
 * @param argc main函数接收到的参数个数。
 * @param argv main函数接收到的参数数组。
 * @param extraReserve 为后续插入预留的额外vector容量。
 * @return 不包含argv[0]的解析参数；输入非法时返回空列表。
 */
std::vector<std::string> UbseCliBuildArgs(int argc, char *argv[], size_t extraReserve = 0);

/**
 * @brief 按CLI白名单校验command、type和option-name所在位置。
 *
 * 解析器参数的前两个位置分别是command和type。之后仅奇数解析位置是option
 * name，必须通过白名单；偶数位置是option value，保留历史上的宽松校验。
 *
 * @param args 不包含argv[0]的解析器参数。
 * @return 所有需校验位置均合法时返回UBSE_OK，否则返回UBSE_ERROR。
 */
int UbseCliValidateArgsByWhitelist(const std::vector<std::string> &args);

/**
 * @brief 在注册表解析前确保command和type均已提供。
 *
 * @param args 不包含argv[0]的解析器参数。
 * @param programName 错误帮助提示中使用的程序名。
 * @return args包含command和type时返回UBSE_OK，否则返回UBSE_ERROR。
 */
int UbseCliValidateCommandArgsCount(const std::vector<std::string> &args, const std::string &programName);

/**
 * @brief 注册SIGALRM处理函数并启动CLI命令超时定时器。
 *
 * @return 信号处理函数和定时器设置成功时返回UBSE_OK，否则返回UBSE_ERROR。
 */
int UbseCliStartTimeoutTimer();

/**
 * @brief 停止CLI命令超时定时器。
 *
 * @return 定时器清除成功时返回UBSE_OK，否则返回UBSE_ERROR。
 */
int UbseCliStopTimeoutTimer();

/**
 * @brief 在CLI命令执行期间屏蔽IPC日志。
 */
void UbseCliDisableIpcLog();

/**
 * @brief 执行注册表解析器匹配到的命令并打印结果。
 *
 * 调用方必须先成功调用UbseCliArgsParse，确保registry.GetMatchCommand()和
 * 输入选项map处于有效状态。
 *
 * @param registry 包含已匹配命令的CLI模块注册表。
 */
void UbseCliExecuteMatchedCommand(ubse::cli::reg::UbseCliModuleRegistry &registry);

/**
 * @brief CLI超时保护使用的SIGALRM处理函数。
 *
 * 收到SIGALRM时打印超时信息，并使用signum作为退出码结束进程。
 *
 * @param signum 内核传递的信号编号。
 */
void UbseCliTimeoutSignalHandler(int signum);
} // namespace ubse::cli::framework

#endif // UBSE_CLI_MAIN_UTILS_H
