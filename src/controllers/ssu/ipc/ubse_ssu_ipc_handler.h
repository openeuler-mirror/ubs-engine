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
#ifndef UBS_ENGINE_UBSE_SSU_IPC_HANDLER_H
#define UBS_ENGINE_UBSE_SSU_IPC_HANDLER_H

#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_api_server_def.h"
#include "ubse_common_def.h"
namespace ubse::ssu::ipc {
class UbseSsuHandler {
public:
    virtual ~UbseSsuHandler() = default;
    common::def::UbseResult Execute(const api::server::UbseIpcMessage &buffer,
                                    const api::server::UbseRequestContext &context);

protected:
    virtual common::def::UbseResult Unpack() = 0;

    virtual common::def::UbseResult Handle() = 0;
    virtual common::def::UbseResult Pack(api::server::UbseIpcMessage &response) = 0;
    common::def::UbseResult Init(const api::server::UbseIpcMessage &buffer,
                                 const api::server::UbseRequestContext &context);

private:
    common::def::UbseResult PopulateIdentityInfo();

protected:
    const api::server::UbseIpcMessage *buffer_ = nullptr;
    const api::server::UbseRequestContext *context_ = nullptr;
    plugin::service::ssu::UbseSsuAllocIdentityInfo identity_{};
};
common::def::UbseResult RegisterSdkDispatcher();
} // namespace ubse::ssu::ipc
#endif //UBS_ENGINE_UBSE_SSU_IPC_HANDLER_H
