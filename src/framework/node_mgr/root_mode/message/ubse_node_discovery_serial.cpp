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

#include "ubse_node_discovery_serial.h"

#include "ubse_error.h"

namespace ubse::nodeMgr {
void UbseNodeDiscoverySerial::SerializeUbseNode(UbseSerialization &out, UbseNodeStaticInfo &node)
{
    out << node.groupId << node.superPodId << node.nodeId << node.addr << node.bonding0Eid;
    out << array_len_insert(node.feEidList.size());
    for (const auto &fe : node.feEidList) {
        out << fe.first << fe.second.entityId << fe.second.primaryEid;
        out << array_len_insert(fe.second.portEids.size());
        for (const auto &eid : fe.second.portEids) {
            out << eid.first << eid.second;
        }
    }
}

UbseResult UbseNodeDiscoverySerial::DeSerializeUbseNode(UbseDeSerialization &in, UbseNodeStaticInfo &node)
{
    in >> node.groupId >> node.superPodId >> node.nodeId >> node.addr >> node.bonding0Eid;
    uint32_t feEidListSize = 0;
    in >> array_len_capture(feEidListSize);
    if (!in.Check()) {
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < feEidListSize; i++) {
        std::string ubpuId;
        in >> ubpuId;
        UbseMtiEidGroup eidInfo{};
        in >> eidInfo.entityId >> eidInfo.primaryEid;
        uint32_t eidListSize = 0;
        in >> array_len_capture(eidListSize);
        if (!in.Check()) {
            return UBSE_ERROR;
        }
        for (size_t j = 0; j < eidListSize; j++) {
            std::string portId;
            std::string portEid;
            in >> portId >> portEid;
            eidInfo.portEids[portId] = portEid;
        }
        node.feEidList[ubpuId] = eidInfo;
        if (!in.Check()) {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeDiscoverySerial::Serialize()
{
    UbseSerialization out;
    SerializeUbseNode(out, node_);
    if (!out.Check()) {
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseNodeDiscoverySerial::Deserialize()
{
    if (mInputRawData == nullptr || mInputRawDataSize == 0) {
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (auto ret = DeSerializeUbseNode(in, node_); ret != UBSE_OK) {
        return ret;
    };
    if (!in.Check()) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::nodeDiscovery