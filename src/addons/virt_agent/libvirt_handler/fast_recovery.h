/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef VM_FAST_RECOVERY_H
#define VM_FAST_RECOVERY_H

#include "libvirt_handler.h"

namespace vm {
    class FastRecovery {
    public:
        static VmResult HandleFastRecoveryBorrow(const Document& msgJson, RespInfo& respInfo,
                                                 UbseIpcMessage& resp, const UbseRequestContext& context);
        static VmResult HandleFastRecoveryClear(const Document& msgJson, RespInfo& respInfo,
                                           UbseIpcMessage& resp, const UbseRequestContext& context);
    private:
        static VmResult ConvertToBorrowFastRecovery(const Value &msgJson, BorrowInfo& borrowInfo);
        static VmResult ConvertToClearFastRecovery(const Value &msgJson, std::string &uuid);
        static VmResult BorrowFastRecovery(BorrowInfo& borrowInfo, BorrowResponse& borrowResponse);
};
} // namespace vm

#endif // VM_FAST_RECOVERY_H
