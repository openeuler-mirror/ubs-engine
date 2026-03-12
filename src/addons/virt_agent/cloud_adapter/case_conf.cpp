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

#include "case_conf.h"

#include <string>
#include <securec.h>                 // for memcpy_s, EOK, errno_t
#include <ubse_storage.h>
#include "mempooling_module.h"
#include "vm_configuration.h"
#include "pointer_process.h"
#include "vm_json_util.h"
#include "vm_string_util.h"
#include "rack_vm_plugin.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::storage;
using namespace mempooling;

uint64_t CaseConf::index = 0;

UbseByteBuffer CaseConf::caseConfBuffer{};

CaseConf &CaseConf::GetInstance()
{
    static CaseConf gInstance;
    return gInstance;
}

VmResult CaseConf::Init()
{
    CaseAndOvercommitmentRatio caseConf{};
    auto res = QueryCaseAndOverCommitmentRatio(caseConf);
    if (res != VM_OK) {
        UBSE_LOG_WARN << "QueryCaseAndOverCommitmentRatio query failed.";
    }
    // The query result is empty; there is no data in the database.
    if (caseConf.curCase.empty()) {
        return VM_OK;
    }
    // Data already exists in the database; store it in the cache
    if (caseConf.curCase == OVER_COMMITMENT_CASE || caseConf.curCase == MEM_FRAGMENTATION_CASE) {
        std::string tmpStr("caseType:" + caseConf.curCase +
                           ";overCommitment:" + std::to_string(caseConf.overCommitmentRatio));
        size_t len = tmpStr.size();
        auto *tmpPtr = new (std::nothrow) uint8_t[len];
        if (tmpPtr == nullptr) {
            UBSE_LOG_ERROR << "CaseConf init failed.";
            return VM_ERROR;
        }
        auto ret = memcpy_s(tmpPtr, len, tmpStr.c_str(), len);
        if (ret != EOK) {
            SafeDeleteArray(tmpPtr);
            UBSE_LOG_ERROR << "CaseConf Init memcpy_s failed.";
            return VM_ERROR;
        }
        caseConfBuffer.data = tmpPtr;
        caseConfBuffer.len = len;
    }
    return VM_OK;
}

VmResult CaseConf::QueryCaseAndOverCommitmentRatio(CaseAndOvercommitmentRatio &caseConf)
{
    auto res = UbseStorageQueryData(CASE_CONF_KEY_PREFIX, CASE_CONF_KEY, &caseConf, UbseStorageDealData);
    if (res != VM_OK) {
        UBSE_LOG_WARN << "Failed to query caseConf from db, " << FormatRetCode(res);
        return VM_ERROR;
    }
    return VM_OK;
}

void CaseConf::UbseStorageDealData(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                                   void *ctx)
{
    auto *caseAndOvercommitmentRatio = static_cast<CaseAndOvercommitmentRatio *>(ctx);
    if (caseAndOvercommitmentRatio == nullptr || buff.data == nullptr) {
        UBSE_LOG_WARN << "[dbquery] ctx or buff is nullptr.";
        return;
    }
    // caseConf = "caseType:overCommitment;overCommitment:1.25;index:1"
    const std::string caseConf(buff.data, buff.data + buff.len);
    CaseAndOvercommitmentRatioDeserial(caseConf, *caseAndOvercommitmentRatio);
}

bool IsGreaterForFloat(float_t a, float_t b, float_t ratioEpsilon = 0.00001f)
{
    return (a - b) > ratioEpsilon;
}

bool CaseConf::ConvertOverCommitmentRatioToFloat(const std::string &ratioStr, float_t &ratio)
{
    try {
        auto tmpRatio = VmStringUtil::SafeStof(ratioStr);
        if (IsGreaterForFloat(MEM_FRAGMENTATION_RATIO, tmpRatio) ||
            IsGreaterForFloat(tmpRatio, MAX_OVER_COMMITMENT_RATIO)) {
            UBSE_LOG_ERROR << "The ratioStr is invalid, it should be in the "
                           << "range [" << MEM_FRAGMENTATION_RATIO << ", " << MAX_OVER_COMMITMENT_RATIO << "].";
            return false;
        }
        ratio = tmpRatio;
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "The ratioStr to float failed." << e.what();
        return false;
    }
    return true;
}

// data = "caseType:overCommitment;overCommitment:1.25"
VmResult CaseConf::CaseAndOvercommitmentRatioDeserial(const std::string &data,
                                                      CaseAndOvercommitmentRatio &caseAndOvercommitmentRatio)
{
    std::vector<std::string> caseconfs;
    VmStringUtil::StrSplit(data, ";", caseconfs);

    bool ratioFlag = false;
    bool caseFlag = false;
    for (const auto &it : caseconfs) {
        std::vector<std::string> curConf;
        VmStringUtil::StrSplit(it, ":", curConf);
        if (curConf.size() != NO_2) {
            UBSE_LOG_ERROR << "data parse failed, data: " << data;
            return VM_ERROR;
        }
        if (curConf[0] == "caseType") {
            caseAndOvercommitmentRatio.curCase = curConf[1];
            caseFlag = true;
        } else if (curConf[0] == "overCommitment" &&
                   ConvertOverCommitmentRatioToFloat(curConf[1], caseAndOvercommitmentRatio.overCommitmentRatio)) {
            ratioFlag = true;
        }
    }
    if (caseFlag && ratioFlag) {
        return VM_OK;
    }

    UBSE_LOG_ERROR << "data parse failed, data: " << data;
    return VM_ERROR;
}

void CaseConf::CaseRegisterRun()
{
    // Start query scenario sub-thread
    std::thread eventThread = std::thread(&CaseConf::RunQueryCaseConf);
    eventThread.detach();
}

void CaseConf::RunQueryCaseConf()
{
    while (true) {
        CaseAndOvercommitmentRatio caseConf{};
        auto res = QueryCaseAndOverCommitmentRatio(caseConf);
        if (res != VM_OK || caseConf.curCase.empty()) {
            UBSE_LOG_ERROR << "Query caseConf from db error or data is empty.";
            std::this_thread::sleep_for(std::chrono::seconds(WAIT_TIME));
            continue;
        }
        // Scene Injection into MemPooling
        SetMemPoolingParams(caseConf.curCase);
        if (caseConf.curCase == OVER_COMMITMENT_CASE) {
            UBSE_LOG_INFO << "Set case and overCommitmentRatio success, curCase=" << caseConf.curCase
                          << ", curOverCommitmentRatio=" << std::to_string(caseConf.overCommitmentRatio);
            // Configure Overcommitment Ratio
            VmConfiguration::GetInstance().SetMaxMemBorrow(caseConf.overCommitmentRatio - 1.0f);
            // Enabling watermark listening and alarm listening in overcommitment scenarios
            res = StrategyInit();
            if (res != VM_OK) {
                UBSE_LOG_ERROR << "Failed to init vm strategy. " << FormatRetCode(res);
                UbsePluginDeInit();
                break;
            }
            UBSE_LOG_INFO << "StrategyInit success.";
            break;
        } else if (caseConf.curCase == MEM_FRAGMENTATION_CASE) {
            // Register non-overcommitment scenario routes and handlers
            res = MemFragRegister();
            if (res != VM_OK) {
                UBSE_LOG_ERROR << "Failed to init memFragmentation router and handler. " << FormatRetCode(res);
                UbsePluginDeInit();
                break;
            }
            UBSE_LOG_INFO << "Set case and overCommitmentRatio success, curCase=" << caseConf.curCase
                          << ", curOverCommitmentRatio=" << std::to_string(caseConf.overCommitmentRatio);
            break;
        }
        UBSE_LOG_WARN << "Failed to set case, caseConf=" << caseConf.curCase
                      << ", overCommitmentRatio=" << std::to_string(caseConf.overCommitmentRatio);
        std::this_thread::sleep_for(std::chrono::seconds(WAIT_TIME));
    }
}

/**
 * Inject scenarios and watermarks into the MemPooling side.
 *
 * @param curCase overCommitment/memFragmentation
 */
void CaseConf::SetMemPoolingParams(const std::string &curCase)
{
    bool runMode = false;
    bool waterMark = false;
    while (true) {
        if (!runMode) {
            runMode = SetMemPoolingRunMode(curCase);
        }
        if (curCase == OVER_COMMITMENT_CASE) {
            if (!waterMark) {
                // Overcommitment Scenario Setting Watermark
                waterMark = SetMemPoolingWaterMark();
            }
        } else {
            waterMark = true;
        }
        if (runMode && waterMark) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(WAIT_TIME_FOR_SET_MODE));
    }
}

/**
 * Injecting scenarios into the MemPooling side
 *
 * @param curCase overCommitment/memFragmentation
 */
bool CaseConf::SetMemPoolingRunMode(const std::string &curCase)
{
    const auto UBSRMRSSetRunMode = MempoolingModule::UBSRMRSSetRunMode();
    if (UBSRMRSSetRunMode == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSetRunMode ptr failed.";
        return false;
    }
    // Call the UBSRMRSSetRunMode interface to set the MemPooling operation mode.
    try {
        VmResult res;
        if (curCase == OVER_COMMITMENT_CASE) {
            res = UBSRMRSSetRunMode(OVER_COMMITMENT_MODE);
        } else {
            res = UBSRMRSSetRunMode(MEM_FRAGMENTATION_MODE);
        }
        if (VmResultFail(res)) {
            UBSE_LOG_ERROR << "Set RMRS runMode " << curCase << " failed, " << FormatRetCode(res);
            return false;
        } else {
            UBSE_LOG_INFO << "Set RMRS runMode successfully, runMode=" << curCase;
            return true;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Set RMRS runMode failed, maybe RMRS is not ready. "
                       << "Exception: " << e.what();
        return false;
    }
}

/**
 * Injecting waterline to MemPooling side in overcommitment scenarios
 *
 */
bool CaseConf::SetMemPoolingWaterMark()
{
    // Obtain the UBSRMRSSetWaterMark function pointer
    const auto UBSRMRSSetWaterMark = MempoolingModule::UBSRMRSSetWaterMark();
    if (UBSRMRSSetWaterMark == nullptr) {
        UBSE_LOG_ERROR << "Get UBSRMRSSetWaterMark ptr failed.";
        return false;
    }
    WaterMark waterMark;
    try {
        waterMark.highWaterMark = VmStringUtil::SafeStou16(VmConfiguration::GetInstance().GetBorrowWatermark());
        waterMark.lowWaterMark = VmStringUtil::SafeStou16(VmConfiguration::GetInstance().GetLowWatermark());

        VmResult ret = UBSRMRSSetWaterMark(waterMark);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "Set MemPooling WaterMark failed,  highWaterMark=" << waterMark.highWaterMark
                           << ", lowWaterMark=" << waterMark.lowWaterMark << ". " << FormatRetCode(ret);
            return false;
        } else {
            UBSE_LOG_INFO << "Set MemPooling WaterMark successfully, highWaterMark=" << waterMark.highWaterMark
                          << ", lowWaterMark=" << waterMark.lowWaterMark;
            return true;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Set MemPooling WaterMark failed, " << e.what();
        return false;
    }
}

VmResult SetCaseConfToDB(CaseConfParam &caseParam)
{
    const std::string tmpStr =
        "caseType:" + caseParam.caseType + ";overCommitment:" + std::to_string(caseParam.overCommitmentRatio) + ";";
    const size_t len = tmpStr.size();
    auto *tmpPtr = new (std::nothrow) uint8_t[len];
    if (tmpPtr == nullptr) {
        UBSE_LOG_ERROR << "SetCaseConf new failed.";
        return VM_ERROR;
    }
    if (const auto ret = memcpy_s(tmpPtr, len, tmpStr.c_str(), len); ret != EOK) {
        SafeDeleteArray(tmpPtr);
        UBSE_LOG_ERROR << "SetCaseConf memcpy_s failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UbseByteBuffer tmpBuffer{.data = tmpPtr, .len = len, .freeFunc = nullptr};
    auto res = UbseStoragePutData(CASE_CONF_KEY_PREFIX, CASE_CONF_KEY, &tmpBuffer);
    if (res != VM_OK) {
        SafeDeleteArray(tmpPtr);
        UBSE_LOG_ERROR << "SetCaseConf failed, " << FormatRetCode(res);
        return VM_ERROR;
    }
    SafeDeleteArray(tmpPtr);
    UBSE_LOG_INFO << "SetCaseConf success.";
    return VM_OK;
}

std::string CaseConfResultParam::ToJson()
{
    JSON_MAP resultMap;

    resultMap.emplace("ret", std::to_string(this->ret));
    resultMap.emplace("msg", this->msg);
    resultMap.emplace("data", this->data);
    JSON_STR result;
    if (!VMJsonUtil::VMConvertMap2JsonStr(resultMap, result)) {
        UBSE_LOG_ERROR << "VMConvertMap2JsonStr error.";
        return "";
    }
    return result;
}

bool CaseConfParam::FromJson(const std::string &jsonString)
{
    JSON_MAP caseConfParamMap;
    caseConfParamMap.emplace("caseType", "");
    caseConfParamMap.emplace("overCommitment", "");

    if (!VMJsonUtil::VMConvertJsonStr2Map(jsonString, caseConfParamMap)) {
        UBSE_LOG_ERROR << "VMConvertJsonStr2Map error. Json:" << jsonString;
        return false;
    }

    this->caseType = caseConfParamMap["caseType"];

    float_t tmpCommitmentRatio;
    try {
        tmpCommitmentRatio = VmStringUtil::SafeStof(caseConfParamMap["overCommitment"]);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "str to float error. " << e.what();
        return false;
    }
    this->overCommitmentRatio = tmpCommitmentRatio;
    return true;
}
}
