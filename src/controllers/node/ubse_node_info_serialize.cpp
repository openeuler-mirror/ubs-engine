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

#include "ubse_node_info_serialize.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_serial_util.h"

namespace ubse::nodeController {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::message;

UbseResult UbseNodeInfoSerialize::Serialize()
{
    uint8_t* responseBuffer = nullptr;
    size_t responseSize = 0;
    auto ret = SerializeUbseNode(nodeInfo_, responseBuffer, responseSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ubse node info serialize failed, ret=" << ret;
        return ret;
    }

    mOutputRawDataSize = responseSize;
    mOutputRawData = std::unique_ptr<uint8_t[]>(responseBuffer);
    return UBSE_OK;
}

UbseResult UbseNodeInfoSerialize::Deserialize()
{
    if (mInputRawData == nullptr || mInputRawDataSize == 0) {
        UBSE_LOG_ERROR << "input raw data is null";
        return UBSE_ERROR;
    }
    auto ret = DeSerializeUbseNode(nodeInfo_, mInputRawData.get(), mInputRawDataSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ubse node info deserialize failed, ret=" << ret;
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController