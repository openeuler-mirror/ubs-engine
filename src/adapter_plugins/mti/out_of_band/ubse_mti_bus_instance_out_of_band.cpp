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
#include "ubse_mti_bus_instance_out_of_band.h"
#include <regex>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mti_util.h"
#include "ubse_os_util.h"
#include "ubse_str_util.h"
#include "./message/ubse_ctrl_q_businstance_msg.h"
#include "./message/ubse_ctrl_q_get_d2h_memory_msg.h"
#include "./message/ubse_ictrl_q_req_msg.h"
#include "./proxy/ubse_ctrl_q_msg_proxy.h"
#include "securec.h"
namespace ubse::mti::bus_instance {
using namespace ubse::mti::ctrl_q;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
const uint16_t DEFAULT_VENDOR = 0;

const std::string LSUB_HEADER = "BusInstance show format: guid type eid upi";
const std::string LSUB_STATIC_SERVER_TYPE = "Static_Server";
const std::string LSUB_CLUSTER_SERVER_TYPE = "Static_Cluster";
const std::string UB_DEVICES_PATH = "/sys/bus/ub/devices/";
const std::string LSUB_SUB_DEVICE_HEADER = "Uents under this busInstance:";
void ParseLsubBusInstanceOutput(std::vector<UbseMtiBusInst>& busInstanceList, const std::string& output)
{
    std::istringstream iss(output);
    std::string line;
    bool foundHeader = false;
    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (!foundHeader) {
            if (line.find(LSUB_HEADER) != std::string::npos) {
                foundHeader = true;
            }
            continue;
        }
        if (line.empty() || line[0] == '-') {
            UBSE_LOG_WARN << "Get invalid output from lsub -b, line: " << line;
            continue;
        }
        std::istringstream lineStream(line);
        UbseMtiBusInst info;
        std::string eidStr;
        std::string guidStr;
        std::string typeStr;
        std::string upiStr;
        if (!(lineStream >> guidStr >> typeStr >> eidStr >> upiStr)) {
            UBSE_LOG_WARN << "Get invalid output from lsub -b, line: " << line;
            continue;
        }
        if (typeStr == LSUB_STATIC_SERVER_TYPE) {
            continue;
        }
        info.type = typeStr == LSUB_CLUSTER_SERVER_TYPE ? UbseMtiBusInstanceType::HOST : UbseMtiBusInstanceType::VM;
        guidStr = utils::RemoveDashes(guidStr);
        if (!EidStrToArray(eidStr, info.eid) || !GuidStrToArray(guidStr, info.guid) ||
            !UpiStrToUint16(upiStr, info.upi)) {
            continue;
        }
        busInstanceList.emplace_back(info);
    }
}

std::vector<std::string> ParseGetSubDeviceOutput(const std::string& output)
{
    std::vector<std::string> devices;
    std::regex header(LSUB_SUB_DEVICE_HEADER);
    std::regex deviceRegex("^\\s*([0-9a-fA-F]+)\\s*$");
    bool subDeviceSec = false;
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (std::regex_match(line, header)) {
            subDeviceSec = true;
            continue;
        }
        if (!subDeviceSec) {
            continue;
        }
        std::smatch match;
        if (std::regex_match(line, match, deviceRegex)) {
            // 匹配成功，校验长度为2，提取设备ID
            if (match.size() == 2) {
                devices.emplace_back(match[1].str());
            }
        } else if (!line.empty() && !std::all_of(line.begin(), line.end(), isspace)) {
            break;
        }
    }
    return devices;
}

UbseResult GetBusInstanceSubDevices(const std::string& eidStr, std::vector<UbseMtiGuid>& guidList)
{
    auto cmd = "lsub -b -E " + eidStr;
    std::string cmdResult{};
    auto ret = utils::UbseOsUtil::Exec(cmd, cmdResult);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to exec lsub, ret :" << FormatRetCode(ret);
        return ret;
    }
    auto subDevices = ParseGetSubDeviceOutput(cmdResult);
    for (const auto& dev : subDevices) {
        auto filePath = UB_DEVICES_PATH + dev + "/guid";
        ret = utils::UbseOsUtil::ReadFileContent(filePath, cmdResult);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to read file content, dev :" << dev << ", ret :" << FormatRetCode(ret);
            return UBSE_ERROR;
        }
        auto nativeGuid = utils::Trim(cmdResult);
        std::string guidStr;
        std::copy_if(nativeGuid.begin(), nativeGuid.end(), std::back_inserter(guidStr),
                     [](char c) { return c != '-'; });
        UbseMtiGuid guid;
        if (!GuidStrToArray(guidStr, guid)) {
            UBSE_LOG_ERROR << "Invalid guid format, dev :" << dev << ", guid :" << nativeGuid;
            return UBSE_ERROR;
        }
        guidList.emplace_back(guid);
    }
    return UBSE_OK;
}

UbseResult GetBusInstanceFromLsub(std::vector<UbseMtiBusInst>& busInstanceList)
{
    UBSE_LOG_INFO << "Start to get bus instance and sub devices";
    std::string cmdResult{};
    auto ret = utils::UbseOsUtil::Exec("lsub -b", cmdResult);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to exec lsub, ret :" << FormatRetCode(ret);
        return ret;
    }
    try {
        ParseLsubBusInstanceOutput(busInstanceList, cmdResult);
        for (auto& busInstance : busInstanceList) {
            std::string eidStr;
            if (!EidArrayToStr(busInstance.eid, eidStr)) {
                UBSE_LOG_ERROR << "Failed to convert eid to string";
                return UBSE_ERROR;
            }
            ret = GetBusInstanceSubDevices(eidStr, busInstance.subDeviceGuids);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to get bus instance sub devices, eid :" << eidStr
                               << ", ret :" << FormatRetCode(ret);
                return ret;
            }
        }
    } catch (std::exception& e) {
        UBSE_LOG_ERROR << "Get bus instance from lsub failed, error :" << e.what();
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMtiBusInstanceOutOfBand::GetBusInstanceList(std::vector<UbseMtiBusInst>& busInstanceList)
{
    return GetBusInstanceFromLsub(busInstanceList);
}

UbseResult UbseMtiBusInstanceOutOfBand::CreateVmBusInstance(uint16_t upi, UbseMtiBusInst& busInstance)
{
    UbseCtrlQCreateBusInstanceReqMsg reqMsg(upi, DEFAULT_VENDOR);
    UbseCtrlQCreateBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "CreateVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    busInstance = respMsg.GetBusInstance();
    busInstance.upi = upi;
    busInstance.vendor = DEFAULT_VENDOR;
    return UBSE_OK;
}

UbseResult UbseMtiBusInstanceOutOfBand::DestroyVmBusInstance(const UbseMtiBusInst& busInstance)
{
    UbseCtrlQDestroyBusInstanceReqMsg reqMsg(busInstance);
    UbseCtrlQDestroyBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DestroyVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    if (!respMsg.GetRet()) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMtiBusInstanceOutOfBand::GetD2hMemory(const UbseMtiBusInst& busInstance, uint32_t& tid, uint64_t& uba,
                                                     uint64_t& size)
{
    UbseCtrlQGetD2hMemoryReqMsg reqMsg(busInstance);
    UbseCtrlQGetD2hMemoryRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetD2hMemory failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    tid = respMsg.GetTid();
    uba = respMsg.GetUba();
    size = respMsg.GetSize();
    return UBSE_OK;
}

} // namespace ubse::mti::bus_instance
