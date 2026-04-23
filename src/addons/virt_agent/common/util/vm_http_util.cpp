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

#include "vm_http_util.h"

#include <securec.h>
#include <ubse_logger.h>

#include "response_info_message.h"
#include "mempooling_module.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

VmResult HttpUtil::AddProcessTracking(const int &pid, const int &scanTime, const int &type)
{
    const auto UBSRMRSSmapAddProcessTracking = mempooling::MempoolingModule::UBSRMRSSmapAddProcessTracking();
    if (UBSRMRSSmapAddProcessTracking == nullptr) {
        UBSE_LOG_ERROR << "load mempooling UBSRMRSSmapAddProcessTracking func failed.";
        return VM_ERROR;
    }
    std::vector<pid_t> pidVec;
    pidVec.push_back(pid);
    std::vector<uint32_t> scanTimeVec;
    scanTimeVec.push_back(scanTime);
    int scanType = type;
    std::vector<uint32_t> durationVec{1};
    try {
        VmResult ret = UBSRMRSSmapAddProcessTracking(pidVec, scanTimeVec, scanType, durationVec);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "UBSRMRSSmapAddProcessTracking func failed, " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "UBSRMRSSmapAddProcessTracking func failed, " << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult HttpUtil::RemoveProcessTracking(const int &pid)
{
    const auto UBSRMRSSmapRemoveProcessTracking = mempooling::MempoolingModule::UBSRMRSSmapRemoveProcessTracking();
    if (UBSRMRSSmapRemoveProcessTracking == nullptr) {
        UBSE_LOG_ERROR << "load mempooling UBSRMRSSmapRemoveProcessTracking func failed";
        return VM_ERROR;
    }
    std::vector<pid_t> pidVec;
    pidVec.push_back(pid);
    int flags = 0;
    try {
        VmResult ret = UBSRMRSSmapRemoveProcessTracking(pidVec, flags);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "UBSRMRSSmapRemoveProcessTracking func failed, " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "UBSRMRSSmapRemoveProcessTracking func failed, " << e.what();
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult HttpUtil::EnableProcessMigrate(const int &pid, bool enable)
{
    const auto UBSRMRSSmapEnableProcessMigrate = mempooling::MempoolingModule::UBSRMRSSmapEnableProcessMigrate();
    if (UBSRMRSSmapEnableProcessMigrate == nullptr) {
        UBSE_LOG_ERROR << "load mempooling UBSRMRSSmapEnableProcessMigrate func failed";
        return VM_ERROR;
    }
    std::vector<pid_t> pidVec;
    pidVec.push_back(pid);
    int flags = 0;
    try {
        VmResult ret = UBSRMRSSmapEnableProcessMigrate(pidVec, static_cast<int>(enable), flags);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "UBSRMRSSmapEnableProcessMigrate func failed, " << FormatRetCode(ret);
            return VM_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "UBSRMRSSmapEnableProcessMigrate func failed, " << e.what();
        return VM_ERROR;
    }

    return VM_OK;
}

VmResult HttpUtil::ToUbseByteBuffer(const std::string &bodyString, UbseByteBuffer &resp)
{
    size_t len = bodyString.size();
    auto *body = new (std::nothrow) uint8_t[len];
    if (body == nullptr) {
        UBSE_LOG_ERROR << "new response body error.";
        return VM_ERROR;
    }
    auto ret = memcpy_s(body, len, bodyString.data(), len);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memcpy_s error, " << FormatRetCode(ret);
        SafeDeleteArray(body);
        return ret;
    }
    resp.data = body;
    resp.len = len;
    resp.freeFunc = [](uint8_t *data) {
        SafeDeleteArray(data);
    };
    return VM_OK;
}

VmResult HttpUtil::SetResp(UbseByteBuffer &resp, const VmResult &code, const std::string &msg)
{
    const ResponseInfo responseInfo = {.code = code, .message = msg};
    ResponseInfoMessage responseInfoSimpo(responseInfo);
    auto ret = responseInfoSimpo.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "responseInfoSimpo Serialize failed.";
        return VM_ERROR;
    }
    resp.len = responseInfoSimpo.SerializedDataSize();
    auto *body = new (std::nothrow) uint8_t[resp.len];
    if (body == nullptr) {
        UBSE_LOG_ERROR << "new response body error.";
        return VM_ERROR;
    }
    ret = memcpy_s(body, resp.len, responseInfoSimpo.SerializedData(), resp.len);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "memcpy_s error, " << FormatRetCode(ret);
        SafeDeleteArray(body);
        return VM_ERROR;
    }
    resp.data = body;
    resp.freeFunc = [](uint8_t *data) {
        SafeDeleteArray(data);
    };
    return VM_OK;
}

} // namespace vm