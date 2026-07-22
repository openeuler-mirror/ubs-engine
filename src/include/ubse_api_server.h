/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_API_SERVER_H
#define UBS_ENGINE_UBSE_API_SERVER_H
#include <functional>

#include "ubse_api_server_def.h"

namespace api::server {
using UbseIpcHandler = std::function<uint32_t(const UbseIpcMessage&, const UbseRequestContext&)>;

/**
 * @brief 注册一个IPC处理回调
 *
 * 该函数用于注册一个处理特定模块和操作代码的IPC消息的处理程序。
 *
 * @param moduleCode 模块代码，标识具体的模块
 * @param opCode 操作代码，标识具体的操作
 * @param handler 处理函数，当接收到对应的IPC消息时会被调用
 * @param object 标识处理的权限点
 * @return uint32_t 返回状态码，表示注册是否成功
 */
uint32_t RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler, const std::string& object);

/**
 * @brief 发送IPC响应
 *
 * 该函数用于向客户端发送IPC响应。
 *
 * @param statusCode 响应的状态码
 * @param requestId 请求的唯一标识符
 * @param response 包含响应数据的IPC消息对象
 * @return uint32_t 返回状态码，表示发送是否成功
 */
uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage& response);
} // namespace api::server
#endif // UBS_ENGINE_UBSE_API_SERVER_H
