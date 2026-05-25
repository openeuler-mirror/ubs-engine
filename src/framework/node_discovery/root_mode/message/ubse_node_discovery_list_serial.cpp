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

#include "ubse_node_discovery_list_serial.h"

#include "ubse_error.h"

namespace ubse::nodeDiscovery {
UbseResult UbseNodeDiscoveryListSerial::Serialize()
{
    UbseSerialization out;
    out << array_len_insert(nodeList_.size());
    for (auto &node : nodeList_) {
        UbseNodeDiscoverySerial::SerializeUbseNode(out, node);
    }
    if (!out.Check()) {
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryListSerial::Deserialize()
{
    if (mInputRawData == nullptr || mInputRawDataSize == 0) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    uint32_t nodeListSize = 0;
    in >> array_len_capture(nodeListSize);
    if (!in.Check()) {
        return UBSE_ERROR;
    }
    if (nodeListSize > MAX_CLUSTER_SIZE) {
        return UBSE_ERR_OUT_OF_RANGE;
    }
    UbseResult ret = UBSE_OK;
    nodeList_.resize(nodeListSize);
    for (size_t i = 0; i < nodeListSize; i++) {
        UbseNodeStaticInfo node{};
        ret = UbseNodeDiscoverySerial::DeSerializeUbseNode(in, node);
        if (ret != UBSE_OK) {
            return ret;
        }
        if (!in.Check()) {
            return UBSE_ERROR;
        }
        nodeList_[i] = node;
    }
    return UBSE_OK;
}
} // namespace ubse::nodeDiscovery