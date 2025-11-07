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

namespace usbe::mem::api {
using namespace ubse::common::def;
using namespace ubse::ipc;

class UbseMemApi {
public:
    static UbseResult Register();

private:
    static uint32_t UbseSeverNodeNumaGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseServerFdGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseServerFdList(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseServerNumaGet(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseServerNumaList(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseNodeBorrowInfoHandle(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseBorrowDetailsInfoHandle(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t UbseCheckMemoryStatus(const UbseIpcMessage &req, const UbseRequestContext &context);
};
} // namespace usbe::mem::api
#endif