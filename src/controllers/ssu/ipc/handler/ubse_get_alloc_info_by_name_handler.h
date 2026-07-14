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
#ifndef UBS_ENGINE_UBSE_GET_ALLOC_INFO_BY_NAME_HANDLER_H
#define UBS_ENGINE_UBSE_GET_ALLOC_INFO_BY_NAME_HANDLER_H
#include "../ubse_ssu_ipc_handler.h"
#include "plugin_services/ssu/ubse_ssu_service.h"

namespace ubse::ssu::ipc {

class UbseGetAllocInfoByNameHandler : public UbseSsuHandler {
public:
    UbseGetAllocInfoByNameHandler() = default;
    ~UbseGetAllocInfoByNameHandler() override = default;
protected:
    common::def::UbseResult Unpack() override;
    common::def::UbseResult Handle() override;
    common::def::UbseResult Pack(api::server::UbseIpcMessage &response) override;

private:
    std::string name;
    plugin::service::ssu::UbseSsuAllocResult result;
};

} // namespace ubse::ssu::ipc

#endif // UBS_ENGINE_UBSE_GET_ALLOC_INFO_BY_NAME_HANDLER_H