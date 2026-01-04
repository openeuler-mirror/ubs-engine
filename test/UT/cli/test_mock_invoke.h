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
#ifndef UBS_ENGINE_TEST_MOCK_INVOKE_H
#define UBS_ENGINE_TEST_MOCK_INVOKE_H

#include <securec.h>
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"

using namespace ubse::serial;

uint32_t mock_ubse_invoke_call_failed(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data);

uint32_t mock_ubse_invoke_call_error_size(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data);

uint32_t mock_ubse_invoke_call_empty(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data);

uint32_t mock_topo_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data);

uint32_t mock_urma_qos_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data);

uint32_t mock_urma_invoke_call_empty(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data);

uint32_t mock_cluster_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data);

uint32_t mock_borrow_details_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data);

uint32_t mock_node_borrow_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data);

uint32_t mock_check_memory_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data);

int8_t mock_import_cert_set_failed(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath,
    const char *caCrlPath);

int8_t mock_import_cert_set_success(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath,
    const char *caCrlPath);

int8_t mock_import_ca_set_failed(const char *caCrlPath);

int8_t mock_import_ca_set_success(const char *caCrlPath);

int8_t mock_delete_cert_failed();

int8_t mock_delete_cert_success();

#endif // UBS_ENGINE_TEST_MOCK_INVOKE_H
