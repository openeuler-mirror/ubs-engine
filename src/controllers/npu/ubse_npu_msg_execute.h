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

#ifndef UBSE_NPU_MSG_EXECUTE_H
#define UBSE_NPU_MSG_EXECUTE_H
#include <cstdint>
namespace ubse::npu::controller {
struct TransMsg {
    uint8_t *buffer;
    uint32_t length;
};

using TransReqMsg = TransMsg;
using TransRespMsg = TransMsg;
/**
 * device设备查询-sdk服务端执行
 * @param req 请求参数
 * @param resp 返回响应
 * @return 函数执行结果；UBSE_OK代表成功，其它代表错误
 */
uint32_t QueryDeviceExecute(TransReqMsg req, TransRespMsg &resp);

/**
 * 绑定npu-sdk服务端执行
 * @param req 请求参数
 * @param resp 返回响应
 * @return 函数执行结果；UBSE_OK代表成功，其它代表错误
 */
uint32_t AllocDeviceExecute(TransReqMsg req, TransRespMsg &resp);

/**
 * 解绑npu-sdk服务端执行
 * @param req 请求参数
 * @param resp 返回响应
 * @return 函数执行结果；UBSE_OK代表成功，其它代表错误
 */
uint32_t FreeDeviceExecute(TransReqMsg req, TransRespMsg &resp);

/**
 * TidUbaSize查询-sdk服务端执行
 * @param req 请求参数
 * @param resp 返回响应
 * @return 函数执行结果；UBSE_OK代表成功，其它代表错误
 */
uint32_t QueryTidUbaSizeExecute(TransReqMsg req, TransRespMsg &resp);
} // namespace ubse::npu::controller
#endif // UBSE_NPU_MSG_EXECUTE_H
