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
#include "ubse_ipc_utils.h"

#include "securec.h"

#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_serial_util.h"

namespace ubse::ipc {
using namespace ubse::serial;
uint64_t RandomId()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    // 定义16位数的范围 (10^15 到 10^16-1)
    const uint64_t min_value = 1000000000000000ULL;
    const uint64_t max_value = 9999999999999999ULL;
    std::uniform_int_distribution<uint64_t> dis(min_value, max_value);
    return dis(gen);
}

uint32_t SerializeRequestMessage(const UbseRequestMessage &requestMessage, std::vector<uint8_t> &buffer)
{
    const uint32_t totalLength = sizeof(bool) + sizeof(UbseRequestHeader) + requestMessage.header.bodyLen;
    // 分配缓冲区
    try {
        buffer.resize(totalLength);
    } catch (const std::bad_alloc &e) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    bool isResp = false;
    auto ret = memcpy_s(buffer.data(), sizeof(bool), &isResp, sizeof(bool));
    if (ret != EOK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    ret = memcpy_s(buffer.data() + sizeof(bool), sizeof(UbseRequestHeader), &(requestMessage.header),
                   sizeof(UbseRequestHeader));
    if (ret != EOK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    if (requestMessage.header.bodyLen > 0) {
        ret = memcpy_s(buffer.data() + sizeof(bool) + sizeof(UbseRequestHeader), requestMessage.header.bodyLen,
                       requestMessage.body, requestMessage.header.bodyLen);
        if (ret != EOK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

uint32_t SerializeResponseMessage(const UbseResponseMessage &responseMessage, std::vector<uint8_t> &buffer)
{
    // 计算总长度
    const uint32_t totalLength = sizeof(bool) + sizeof(UbseResponseHeader) + responseMessage.header.bodyLen;

    // 分配缓冲区
    try {
        buffer.resize(totalLength);
    } catch (const std::bad_alloc &e) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 复制flag 标识为响应
    bool isResp = true;
    auto ret = memcpy_s(buffer.data(), sizeof(bool), &isResp, sizeof(bool));
    if (ret != EOK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 复制头部
    ret = memcpy_s(buffer.data() + sizeof(bool), sizeof(UbseResponseHeader), &(responseMessage.header),
                   sizeof(UbseResponseHeader));
    if (ret != EOK) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    // 复制body
    if (responseMessage.header.bodyLen > 0) {
        ret = memcpy_s(buffer.data() + sizeof(bool) + sizeof(UbseResponseHeader), responseMessage.header.bodyLen,
                       responseMessage.body, responseMessage.header.bodyLen);
        if (ret != EOK) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

uint32_t SerializeShmFault(const UbseShmFault &shmFault, uint8_t *&buffer, size_t &size)
{
    UbseSerialization outStream;
    outStream << shmFault.shmName << shmFault.memId << enum_v(shmFault.type);
    if (!outStream.Check()) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

uint32_t DeSerializeShmFault(UbseShmFault &shmFault, uint8_t *buffer, size_t size)
{
    UbseDeSerialization inStream(buffer, size);
    inStream >> shmFault.shmName >> shmFault.memId >> enum_v(shmFault.type);
    if (!inStream.Check()) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}
} // namespace ubse::ipc