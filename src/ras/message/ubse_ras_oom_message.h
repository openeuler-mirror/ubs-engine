// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef RACK_MANAGER_MXE_RAS_OOM_MESSAGE_H
#define RACK_MANAGER_MXE_RAS_OOM_MESSAGE_H
#include <string>
#include "ubse_base_message.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"

namespace ubse::ras {
using ubse::common::def::UbseResult;
using ubse::message::UbseBaseMessage;
using ubse::serial::UbseDeSerialization;
using ubse::serial::UbseSerialization;
using ubse::utils::Ref;

class UbseRasOomMessage : public UbseBaseMessage {
public:
    UbseRasOomMessage() = default;
    explicit UbseRasOomMessage(uint64_t memNeed, std::string nodeId, int64_t oomNumaId)
        : nodeId(nodeId),
          memNeed(memNeed),
          oomNumaId(oomNumaId)
    {
    }

    std::string inline GetNodeId()
    {
        return nodeId;
    }

    uint64_t inline GetNumaId()
    {
        return oomNumaId;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    void Serialization(UbseSerialization& out);

    UbseResult Deserialization(UbseDeSerialization& in);

private:
    std::string nodeId;
    uint64_t memNeed;
    int64_t oomNumaId;
};

using UbseRasOomMessagePtr = Ref<UbseRasOomMessage>;
} // namespace ubse::ras
#endif // RACK_MANAGER_MXE_RAS_OOM_MESSAGE_H
