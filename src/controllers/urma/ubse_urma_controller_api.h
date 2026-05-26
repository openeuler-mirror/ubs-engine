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

#ifndef UBSE_URMA_CONTROLLER_API_H
#define UBSE_URMA_CONTROLLER_API_H

#include "ubse_api_server_module.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_urma_controller_rpc.h"

namespace ubse::urmaController {

class UbseUrmaControllerApi {
public:
    static UbseResult Register();

private:
    static uint32_t UbseUrmaSdkQosCreate(const ubse::ipc::UbseIpcMessage &req,
                                         const ubse::ipc::UbseRequestContext &context);
    static uint32_t UbseUrmaSdkQosDelete(const ubse::ipc::UbseIpcMessage &req,
                                         const ubse::ipc::UbseRequestContext &context);

    static uint32_t UbseUrmaCliQosCreate(const ubse::ipc::UbseIpcMessage &req,
                                         const ubse::ipc::UbseRequestContext &context);
    static uint32_t UbseUrmaCliQosGet(const ubse::ipc::UbseIpcMessage &req,
                                      const ubse::ipc::UbseRequestContext &context);
    static uint32_t UbseUrmaCliQosDelete(const ubse::ipc::UbseIpcMessage &req,
                                         const ubse::ipc::UbseRequestContext &context);

    static uint32_t UbseUrmaSdkDevGet(const ubse::ipc::UbseIpcMessage &req,
                                      const ubse::ipc::UbseRequestContext &context);
    static uint32_t UbseUrmaCliDevGet(const ubse::ipc::UbseIpcMessage &req,
                                      const ubse::ipc::UbseRequestContext &context);
    static uint32_t UbseUrmaSdkDevAlloc(const ubse::ipc::UbseIpcMessage &req,
                                        const ubse::ipc::UbseRequestContext &context);
    static uint32_t UbseUrmaSdkDevFree(const ubse::ipc::UbseIpcMessage &req,
                                       const ubse::ipc::UbseRequestContext &context);
};
} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_API_H