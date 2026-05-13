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

#ifndef UBSE_STORAGE_REQ_HANDLER_H
#define UBSE_STORAGE_REQ_HANDLER_H

#include "ubse_com_base.h"

namespace ubse::storage {
using namespace ubse::com;
class UbseStorageReqHandler : public UbseComBaseMessageHandler {
public:
    UbseStorageReqHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    inline uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseStorageOpCode::STORAGE_REQ);
    }

    inline uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::STORAGE);
    }
};
} // namespace ubse::storage
#endif // UBSE_STORAGE_REQ_HANDLER_H
