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

#ifndef UBSE_SSU_UTILS_H
#define UBSE_SSU_UTILS_H

#include "ubse_thread_pool_module.h"
#include <string>

namespace ubse::ssu::utils {

using namespace ubse::task_executor;

// 获取SSU任务执行器
UbseTaskExecutorPtr GetSsuExecutor();

// 将原始16字节二进制UUID转换为标准连字符格式（xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）
// 输入长度不是16字节时返回空串
std::string StrToUuid(const std::string &id);

// 生成SSU主机NQN，格式：nqn.2024-01.org.nvmexpress:uuid:12345678-1234-1234-1234-1234567890ab
// uuid函数内生成，HostNqn用于创建namespace时customDate持久化
std::string GenerateHostNqn();

// 校验HostNqn格式，NVMe 规范 NQN 格式：nqn.YYYY-MM.<reverse-domain>[:subdomain...]
// 其中 reverse-domain 为反向域名，仅含 [a-zA-Z0-9.-]
// 若子域中包含 :uuid:，则后续部分须为 8-4-4-4-12 UUID 十六进制格式
// 长度上限校验：NQN 总长度不得超过序列化层上限 UBSE_SSU_MAX_NQN_LENGTH - 1 （68字节），
bool IsValidHostNqn(const std::string &hostNqn);

// 校验可选NQN：空串表示使用defaultNqn（合法），非空串须为合法HostNqn格式
// 封装了 attach/detach/权限入口共用的"空值跳过，非空则校验"模式
bool IsOptionalNqnValid(const std::string &nqn);

// 校验聚合块设备名称：仅允许白名单字符[A-Za-z0-9_-]，且长度不超过协议上限32（MAX_DEV_NAME_LEN-1）。
// 与adapter侧IsSafeDeviceName保持一致：该名字会拼接为/dev/ssu/{devName}路径返回，
// 白名单天然阻断路径分隔符、路径穿越（..）及shell元字符，防止拼接后逃逸/dev/ssu/目录
bool IsValidDevName(const std::string &devName);

} // namespace ubse::ssu::utils

#endif // UBSE_SSU_UTILS_H
