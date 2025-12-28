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

#ifndef UBSE_URMA_CONTROLLER_RPC_H
#define UBSE_URMA_CONTROLLER_RPC_H
#include <cstdint>
#include <string>
#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_error.h"
#include "ubse_urma.h"

namespace ubse::urmaController {
using namespace ubse::com;
using namespace ubse::utils;

struct UrmaQosRpcReq {
    uint32_t nodeId;
    std::string urmaName;
};

struct UrmaQosRpcRsp {
    uint32_t minBandWidth;
    uint32_t maxBandWidth;
};

class UbseUrmaQosReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaQosReqSimpo() = default;
    explicit UbseUrmaQosReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaQosReq(UrmaQosRpcReq request)
    {
        req = std::move(request);
    }

    inline UrmaQosRpcReq GetUbseUrmaQosReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQosRpcReq req;
};
using UbseUrmaQosReqPtr = Ref<UbseUrmaQosReqSimpo>;

class UbseUrmaQosRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaQosRspSimpo() = default;
    explicit UbseUrmaQosRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaQosRsp(UrmaQosRpcRsp response)
    {
        rsp = std::move(response);
    }

    inline UrmaQosRpcRsp GetUbseUrmaQosRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQosRpcRsp rsp;
};
using UbseUrmaQosRspPtr = Ref<UbseUrmaQosRspSimpo>;

class UbseUrmaQosMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

} // namespace ubse::urmaController
#endif