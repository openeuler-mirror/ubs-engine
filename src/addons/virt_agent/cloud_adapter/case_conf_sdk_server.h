/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_CASE_CONF_SDK_SERVER_H
#define VM_CASE_CONF_SDK_SERVER_H

#include <ubse_api_server_def.h>

#include "vm_error.h"
#include "case_conf.h"
#include "case_conf_msg.h"

namespace vm {
using namespace api::server;

class VirtCaseConfSdk {
public:
    static VmResult Register();

private:
    static uint32_t GetCaseConfHandler(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t SetCaseConfHandler(const UbseIpcMessage &req, const UbseRequestContext &context);

    static uint32_t GetCaseConf(CaseConfInfo &caseConfInfo);
    static uint32_t SetCaseConf(const std::string &reqBodyStr, CaseConfParam &caseParam,
                                CaseConfResultParam &caseResult);
    static uint32_t PackGetCaseConfRsp(CaseConfInfo &caseConf, UbseIpcMessage &buffer);
    static uint32_t PackSetCaseConfRsp(const CaseConfSetInfo &caseConfSetInfo, UbseIpcMessage &buffer);
    static uint32_t SetCaseConfCheckReq(const std::string &reqBodyStr, CaseConfParam &caseParam,
                                        CaseConfResultParam &caseResult);
};
} // namespace vm

#endif // VM_CASE_CONF_SDK_SERVER_H
