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

#ifndef IT_LOG_CASES_H
#define IT_LOG_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::log {

// 日志绕接测试（单节点场景）：
//   1) 修改配置 log.max.fileSize=2 (2MB)，重启节点使配置生效；
//   2) 向 ubse.log 追加数据使文件超过 2MB，触发日志绕接；
//   3) 验证有新的绕接压缩文件 ubse_*.tar.gz 生成（绕接成功）；
//   4) 对比绕接前后日志打印时间，验证绕接后日志继续正常打印；
//   5) 恢复配置 log.max.fileSize=20，重启节点。
void RunLogRotationTest(ubse::it::infra::ItCluster& cluster);

// 日志绕接文件数量上限测试（单节点场景）：
//   1) 修改配置 log.max.fileSize=2 (2MB)、log.fileNums=3，重启节点使配置生效；
//   2) 连续触发 4 次日志绕接；
//   3) 验证绕接文件数量始终不超过 log.fileNums(3)，且绕接 4 次后最早生成的日志包已被删除替换；
//   4) 恢复配置 log.max.fileSize=20、log.fileNums=20，重启节点。
void RunLogRotationFileNumTest(ubse::it::infra::ItCluster& cluster);

// 日志文件格式校验测试（单节点场景）：
//   1) 验证默认日志目录存在日志文件（ubse.log 非空）；
//   2) 验证日志目录下所有 .log 文件读写权限为 640；
//   3) 获取最近 50h 的日志，逐条目校验格式：
//      - 标准格式（com_pattern）：[时间戳][等级][进程][线程][traceid][文件:函数:行号] 消息；
//      - 第三方组件格式（third_pattern）：[时间戳][等级][进程][线程][traceid][文件:行号] 消息；
//      - 日志等级（level_pattern）必须在 WARN/DEBUG/INFO/ERROR/CRIT 内。
void RunLogFormatTest(ubse::it::infra::ItCluster& cluster);

// 日志回调注册测试（单节点场景）：
//   1) 通过 ubs_engine_log_callback_register 注册自定义日志处理函数；
//   2) 验证注册成功日志 "Custom log handler registered successfully" 经回调按自定义格式输出；
//   3) 触发多等级日志及真实 SDK 调用日志，验证均按自定义格式输出：
//      [simple_log_handler] {log_level.name}：{message}，等级在 INFO|WARN|ERROR|CRIT 内；
//   4) 恢复默认输出并清理临时文件。
void RunLogCallbackTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::log

#endif // IT_LOG_CASES_H