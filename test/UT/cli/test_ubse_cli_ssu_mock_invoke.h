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

#ifndef TEST_UBSE_CLI_SSU_MOCK_INVOKE_H
#define TEST_UBSE_CLI_SSU_MOCK_INVOKE_H

#include <cstdint>

#include "ubse_cli_ssu_struct.h"
#include "ubse_ipc_client.h"

// 测试桩：以与 ubse_invoke_call 一致的签名替换真实 IPC，模拟 SSU controller 的各类响应。
// 全局捕获区记录最近一次调用入参，供用例断言 CLI 实际发出的 module/op code 与请求体。
extern uint16_t g_ssuMockLastModuleCode;
extern uint16_t g_ssuMockLastOpCode;
extern ubse::cli::reg::UbseCliSsuAllocSummaryReq g_ssuMockLastSummaryReq;
extern ubse::cli::reg::UbseCliSsuAllocDetailReq g_ssuMockLastDetailReq;
extern ubse::cli::reg::UbseCliSsuAllocCreateReq g_ssuMockLastCreateReq;
extern bool g_ssuMockLastRequestDeserialized;

// 用例起始调用，清空上次捕获，避免用例间状态串扰。
void ResetSsuMockCapture();

uint32_t mock_ssu_alloc_summary_invoke_call_empty(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_summary_invoke_call_bad_response(uint16_t module_code, uint16_t op_code,
                                                         const ubse_api_buffer_t *request_data,
                                                         ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_summary_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                   const ubse_api_buffer_t *request_data,
                                                   ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_summary_invoke_call_subgib(uint16_t module_code, uint16_t op_code,
                                                   const ubse_api_buffer_t *request_data,
                                                   ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_detail_invoke_call_empty(uint16_t module_code, uint16_t op_code,
                                                 const ubse_api_buffer_t *request_data,
                                                 ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_detail_invoke_call_subgib(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_detail_invoke_call_bad_response(uint16_t module_code, uint16_t op_code,
                                                        const ubse_api_buffer_t *request_data,
                                                        ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_detail_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data);

uint32_t mock_ssu_alloc_create_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data);

#endif // TEST_UBSE_CLI_SSU_MOCK_INVOKE_H
