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
#include "ubse_mem_debt_info_partial_fetch_req.h"
#include "ubse_mem_debt_info_partial_fetch_res.h"

using namespace ubse::mem::controller::message;

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
    ser << std::string("1") << std::string("36") << right_v<size_t>(1) << std::string("2") << std::string("36")
        << std::string("1") << std::string("236") << right_v<size_t>(0) << std::string("2") << std::string("36")
        << right_v<size_t>(2) << std::string("1") << std::string("36") << std::string("1") << std::string("76");
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

uint32_t mock_node_borrow_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 1;
    ser << array_len_insert(size);
    uint32_t borrowSlotId{ 1 };
    uint32_t lendSlotId{ 3 };
    uint64_t borrowSize{ 1024 };
    ser << borrowSlotId << std::string("3") << lendSlotId << std::string("1") << borrowSize;
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

uint32_t mock_fetch_debt_info_by_page_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    if (response_data == nullptr) {
        return UBSE_ERROR;
    }
    UbseDeSerialization deser(request_data->buffer, request_data->length);

    DebtFetchInfo debtFetchInfo;
    if (!DebtFetchInfo::Deserialize(deser, debtFetchInfo)) {
        return UBSE_ERROR;
    }
    PartialFetchRes res;
    res.NeedToContinue = false;
    if (debtFetchInfo.nodeId == "1" && debtFetchInfo.type == DebtFetchType::EXPORT) {
        res.flatDebt.emplace_back(FlatDebtInformation{ "test", AccountType::ADDR, 16, "1", "1", { { 1, 1, 1 } }, "2" });
    } else if (debtFetchInfo.nodeId == "2" && debtFetchInfo.type == DebtFetchType::IMPORT) {
        res.flatDebt.emplace_back(
            FlatDebtInformation{ "test", AccountType::FD, 16, "2", "3", { { 1, 1, 1 } }, "2,3,4" });
    } else if (debtFetchInfo.nodeId == "3" && debtFetchInfo.type == DebtFetchType::EXPORT) {
        res.flatDebt.emplace_back();
    } else if (debtFetchInfo.nodeId == "4" && debtFetchInfo.type == DebtFetchType::IMPORT) {
        res.flatDebt.emplace_back();
    }
    UbseSerialization ser;
    if (!PartialFetchRes::Serialize(ser, res)) {
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

uint32_t mock_get_id_with_hostname_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 2;
    ser << size;
    ser << std::string("controller(1)") << std::string("standby3") <<
        std::string("4245:4944:0000:0000:0000:0000:0100:0000") << "guid" << std::string("node01(2)") <<
        std::string("master") << std::string("4245:4944:0000:0000:0000:0000:0200:0000") << "guid";
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
    ser << std::string("1") << std::string("ok") << std::string("2") << std::string("ok") << std::string("3")
        << std::string("ok") << std::string("4") << std::string("nok");
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

bool mock_import_cert_set_failed(const std::string &serverCertPath, const std::string &trustCertPath,
                                 const std::string &serverKeyPath, const std::string &caCrlPath, std::string &errMsg)
{
    return false;
}
bool mock_import_cert_set_success(const std::string &serverCertPath, const std::string &trustCertPath,
                                  const std::string &serverKeyPath, const std::string &caCrlPath, std::string &errMsg)
{
    return true;
}

bool mock_import_ca_set_failed(const std::string &caCrlPath, std::string &errMsg)
{
    return false;
}

bool mock_import_ca_set_success(const std::string &caCrlPath, std::string &errMsg)
{
    return true;
}

uint32_t mock_delete_cert_failed(std::string &errMsg)
{
    return UBSE_ERROR;
}

uint32_t mock_delete_cert_success(std::string &errMsg)
{
    return UBSE_OK;
}

uint32_t mock_cpu_topo_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                               const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    UbseSerialization ser1;
    UbseSerialization ser2;
    size_t size = 2;
    std::string node1 = "1";
    std::string node2 = "2";
    ser << size;
    ser1 << node1 << node1 << node1 << node2 << node2 << node2 << node2;
    ser2 << node2 << node2 << node2 << node1 << node1 << node1 << node1;
    ser << ser1 << ser2;
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

uint32_t mock_node_lend_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 1;
    ser << array_len_insert(size);
    uint32_t borrowSlotId{ 1 };
    uint32_t lendSlotId{ 3 };
    uint64_t borrowSize{ 1024 };
    ser << borrowSlotId << std::string("3") << lendSlotId << std::string("1") << borrowSize;
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

uint32_t mock_numa_status_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 1;
    ser << array_len_insert(size);
    ser << "node01(2)"
        << "0"
        << "64398"
        << "11083"
        << "53316"
        << "17.2";
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

uint32_t mock_config_ubse_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                             const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    size_t size = 1;
    ser << array_len_insert(size);
    ser << "controller(1)"
        << "true";
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

uint32_t mock_urma_dev_info_invoke_call_empty(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    uint32_t urmaSize = 0;
    ser << urmaSize;
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

uint32_t mock_urma_dev_info_invoke_call_deserialize_failed(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    uint32_t urmaSize = 1;
    ser << urmaSize;
    std::string urmaName = "urma1";
    uint32_t urmaType = 0;
    std::string devEid = "eid1";
    std::vector<std::string> feNames = {"dev1"};
    std::vector<std::string> feEids = {"eid1"};
    uint32_t urmaStatus = 0;
    ser << urmaName << urmaType << devEid << feNames << feEids << urmaStatus;
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

uint32_t mock_urma_dev_info_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    uint32_t urmaSize = 1;
    ser << urmaSize;
    std::string urmaName = "urma1";
    uint32_t urmaType = 0;
    std::string devEid = "eid1";
    std::vector<std::string> feNames = {"dev1", "dev2"};
    std::vector<std::string> feEids = {"eid1", "eid2"};
    uint32_t urmaStatus = 0;
    ser << urmaName << urmaType << devEid << feNames << feEids << urmaStatus;
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

uint32_t mock_urma_qos_invoke_call_empty(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    uint32_t urmaSize = 0;
    ser << urmaSize;
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

uint32_t mock_urma_qos_invoke_call_deserialize_failed(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    uint32_t urmaSize = 1;
    ser << urmaSize;
    std::string urmaName = "urma1";
    std::vector<std::string> feNames = {"dev1"};
    uint32_t minBandWidth = 100;
    uint32_t maxBandWidth = 1000;
    ser << urmaName << feNames << minBandWidth << maxBandWidth;
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

uint32_t mock_urma_qos_invoke_call_normal(uint16_t module_code, uint16_t op_code,
    const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    UbseSerialization ser;
    uint32_t urmaSize = 1;
    ser << urmaSize;
    std::string urmaName = "urma1";
    std::vector<std::string> feNames = {"dev1", "dev2"};
    uint32_t minBandWidth = 100;
    uint32_t maxBandWidth = 1000;
    ser << urmaName << feNames << minBandWidth << maxBandWidth;
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