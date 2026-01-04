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

#ifndef UBSE_URMA_CONTROLLER_API_H
#define UBSE_URMA_CONTROLLER_API_H

#include "ubse_api_server_module.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_urma_controller_rpc.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::ipc;

class UbseUrmaControllerApi {
public:
    static UbseResult Register();

private:
    static uint32_t UbseUrmaBandWidthSet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaBandWidthGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaBandWidthCliGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaBandWidthReset(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaBandWidthGetNeighber(const uint32_t nodeId, const std::string name,
                                                   const UbseRequestContext &context);
    static uint32_t UbseUrmaSendQosRsp(const uint64_t requestId, UrmaQosRpcRsp urmaQosRsp);
    static uint32_t UbseUrmaSendCliQosRsp(const uint64_t requestId, UrmaQosRpcRsp urmaQosRsp);

    static uint32_t UbseUrmaDevGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaLocalDevGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaDevAlloc(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseUrmaDevFree(const UbseIpcMessage &req, const UbseRequestContext &context);
};
} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_API_H