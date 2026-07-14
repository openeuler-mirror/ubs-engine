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
#ifndef UBS_ENGINE_UBSE_FE_DEVICE_ALLOC_HANDLER_H
#define UBS_ENGINE_UBSE_FE_DEVICE_ALLOC_HANDLER_H
#include "../ubse_ssu_ipc_handler.h"
#include "plugin_services/ssu/ubse_ssu_service.h"

namespace ubse::ssu::ipc {

class UbseFeDeviceAllocHandler : public UbseSsuHandler {
public:
    UbseFeDeviceAllocHandler() = default;
    ~UbseFeDeviceAllocHandler() override = default;
protected:
    common::def::UbseResult Pack(api::server::UbseIpcMessage &response) override;
    common::def::UbseResult Handle() override;
    common::def::UbseResult Unpack() override;

private:
    uint32_t upi;
    plugin::service::ssu::UbseSsuVfe vfe;
    std::string busInstanceGuid;
};

} // namespace ubse::ssu::ipc

#endif //UBS_ENGINE_UBSE_FE_DEVICE_ALLOC_HANDLER_H