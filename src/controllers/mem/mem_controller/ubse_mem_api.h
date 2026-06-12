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

#ifndef UBSE_MEM_API_H
#define UBSE_MEM_API_H

#include "ubse_api_server_module.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_account.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"

namespace usbe::mem::api {
using ::api::server::UbseApiServerModule;
using ::api::server::UbseIpcMessage;
using ::api::server::UbseRequestContext;
using ubse::common::def::UbseResult;

class UbseMemApi {
public:
    static UbseResult Register();

private:
    static uint32_t UbseBorrowDetailsFetchDebtHandle(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseCheckMemoryStatus(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseNodeMemConfigHandle(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t UbseNumaStatusHandler(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint8_t UbseNumaStatusParseShowAllFlag(const UbseIpcMessage& req);
    static uint32_t UbseNumaStatusSerializeNumaList(
        const std::vector<ubse::mem::account::UbseNumaNodeInfo>& numaInfoList, uint8_t showAll,
        const std::string& pageSizeType, ubse::serial::UbseSerialization& ubse_serial);
    static uint32_t QueryNumaStateHandler(const UbseIpcMessage& request, const UbseRequestContext& context);

    static uint32_t UbseMemCliNumaInfoGetByName(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static uint32_t UbseMemCliFdInfoGetByName(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static uint32_t UbseMemCliNumaCreate(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static uint32_t UbseMemCliFdCreate(const UbseIpcMessage& buffer, const UbseRequestContext& context);

    static uint32_t UbseCliShmCreateDispatch(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static uint32_t UbseCliShmAttachDispatch(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static uint32_t UbseCliShmDetachDispatch(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static uint32_t UbseCliShmGetDispatch(const UbseIpcMessage& buffer, const UbseRequestContext& context);
    static UbseResult UbseRegisterShmCliInterface(const std::shared_ptr<UbseApiServerModule>& apiServerModule);
};
} // namespace usbe::mem::api
#endif