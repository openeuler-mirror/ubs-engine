// Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
#ifndef UBSE_MEM_CALLBACK_MESSAGE_H
#define UBSE_MEM_CALLBACK_MESSAGE_H

#include <string>
#include "ubse_com_base.h"

namespace ubse::mem::controller {
using ubse::utils::Ref;

class UbseMemCallbackMessage : public UbseBaseMessage {
public:
    UbseMemCallbackMessage();

    explicit UbseMemCallbackMessage(std::string dataRaw);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    std::string data;
};

using UbseMemCallbackMessagePtr = Ref<UbseMemCallbackMessage>;
} // namespace ubse::mem::controller
#endif
