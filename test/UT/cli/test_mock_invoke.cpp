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
#include "test_mock_invoke.h"

uint32_t mock_ubse_invoke_call_failed(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data)
{
    return UBSE_ERROR;
}

uint32_t mock_ubse_invoke_call_error_size(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    ser << std::string("5");
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_ubse_invoke_call_empty(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    std::vector<std::string> empty{};
    auto size = empty.size();
    ser << size;
    if (!ser.Check()) {
        return UBSE_ERROR;
    }
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_topo_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 3;
    ser << size;
    ser << std::string("1") << std::string("36") << right_v<size_t>(1) << std::string("2") << std::string("36") <<
        std::string("1") << std::string("236") << right_v<size_t>(0) << std::string("2") << std::string("36") <<
        right_v<size_t>(2) << std::string("1") << std::string("36") << std::string("1") << std::string("76");
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_urma_qos_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                               const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    if (op_code == 0x0008) {
        /* 如果是第一步查询urma信息 */
        uint32_t size = 3;
        uint32_t urmaStatus = 0;
        ser << size;
        ser << std::string("urma1") << std::string("fe1") << std::string("fe2") << urmaStatus << std::string("urma2")
            << std::string("fe3") << std::string("fe4") << urmaStatus << std::string("urma3") << std::string("fe5")
            << std::string("fe6") << urmaStatus;
    } else {
        /* 如果第二步查询Qos信息 */
        uint32_t minBandWidth = 80;
        uint32_t maxBandWidth = 160;
        ser << minBandWidth << maxBandWidth;
    }
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_urma_invoke_call_empty(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
    ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    std::vector<std::string> empty{};
    uint32_t size = empty.size();
    ser << size;
    if (!ser.Check()) {
        return UBSE_ERROR;
    }
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_cluster_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 4;
    ser << size;
    ser << std::string("1") << std::string("master") << std::string("123") << std::string("2") <<
        std::string("standby") << std::string("234") << std::string("3") << std::string("agent") <<
        std::string("345") << std::string("4") << std::string("agent") << std::string("456");
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_borrow_details_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 3;
    ser << size;
    ser << std::string("memory1") << std::string("numa") << std::string("1") << std::string("268435456") <<
        std::string("3") << std::string("268435456") << std::string("done") << std::string("memory2") <<
        std::string("fd") << std::string("1") << std::string("268435456") << std::string("3") <<
        std::string("268435456") << std::string("done") << std::string("memory2") << std::string("fd") <<
        std::string("2") << std::string("268435456") << std::string("3") << std::string("268435456") <<
        std::string("fault");
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_node_borrow_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 3;
    ser << size;
    ser << std::string("1") << std::string("3") << std::string("1024") << std::string("1") << std::string("2") <<
        std::string("2048") << std::string("1") << std::string("4") << std::string("512");
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t mock_check_memory_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 4;
    ser << size;
    ser << std::string("1") << std::string("ok") << std::string("2") << std::string("ok") << std::string("3") <<
        std::string("ok") << std::string("4") << std::string("nok");
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = (uint32_t)ser.GetLength();
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

int8_t mock_import_cert_set_failed(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath,
    const char *caCrlPath)
{
    return UBSE_ERROR;
}
int8_t mock_import_cert_set_success(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath,
    const char *caCrlPath)
{
    return UBSE_OK;
}

int8_t mock_import_ca_set_failed(const char *caCrlPath)
{
    return UBSE_ERROR;
}

int8_t mock_import_ca_set_success(const char *caCrlPath)
{
    return UBSE_OK;
}

int8_t mock_delete_cert_failed()
{
    return UBSE_ERROR;
}

int8_t mock_delete_cert_success()
{
    return UBSE_OK;
}
