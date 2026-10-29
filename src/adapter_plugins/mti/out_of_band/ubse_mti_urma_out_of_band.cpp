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
#include "ubse_mti_urma_out_of_band.h"
#include <map>
#include <regex>
#include "ubse_error.h"
#include "ubse_file_util.h"
#include "ubse_logger.h"
#include "ubse_mti_util.h"
#include "ubse_os_util.h"
#include "ubse_str_util.h"
#include "./message/ubse_ctrl_q_businstance_msg.h"
#include "./message/ubse_ctrl_q_fe_opt_msg.h"
#include "./message/ubse_ctrl_q_get_idev_fe_david_mapping_msg.h"
#include "./message/ubse_ctrl_q_get_idev_fe_msg.h"
#include "./message/ubse_ctrl_q_vfe_david_opt_msg.h"
#include "./message/ubse_ictrl_q_req_msg.h"
#include "./proxy/ubse_ctrl_q_msg_proxy.h"
namespace ubse::mti::urma {
using namespace ubse::mti::ctrl_q;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
// 遍历/sys/bus/ub/devices/下的eid目录，读取guid文件内容，建立guid到eid的映射
std::map<UbseMtiGuid, UbseMtiEid> BuildGuidToEidMap()
{
    std::map<UbseMtiGuid, UbseMtiEid> guidToEidMap;
    const std::regex eidDirPattern("^[0-9a-fA-F]{5}$");
    const auto devDirs = utils::UbseFileUtil::ListFiles(UB_DEVICES_PATH, eidDirPattern);
    for (const auto& dev : devDirs) {
        const auto filePath = UB_DEVICES_PATH + dev + "/guid";
        std::string fileContent{};
        if (utils::UbseOsUtil::ReadFileContent(filePath, fileContent) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to read guid file, dev :" << dev;
            continue;
        }
        const auto nativeGuid = utils::Trim(fileContent);
        std::string guidStr;
        std::copy_if(nativeGuid.begin(), nativeGuid.end(), std::back_inserter(guidStr),
                     [](char c) { return c != '-'; });
        UbseMtiGuid guid;
        if (!GuidStrToArray(guidStr, guid)) {
            UBSE_LOG_WARN << "Invalid guid format, dev :" << dev << ", guid :" << nativeGuid;
            continue;
        }
        UbseMtiEid eid;
        if (!EidStrToArray(dev, eid)) {
            UBSE_LOG_WARN << "Invalid eid format, dev :" << dev;
            continue;
        }
        guidToEidMap[guid] = eid;
    }
    return guidToEidMap;
}

// 根据feList中pfe的guid反查eid并填充。部分pfe idevice挂在Static_Server类型bus instance下，
// GetBusInstanceSubDevices无法覆盖此类设备，需从/sys/bus/ub/devices/<eid>/guid文件反查
void FillPfeEidByGuid(std::vector<UbseMtiIdevPfe>& feList)
{
    const auto guidToEidMap = BuildGuidToEidMap();
    for (auto& pfe : feList) {
        const auto it = guidToEidMap.find(pfe.guid);
        if (it == guidToEidMap.end()) {
            UBSE_LOG_WARN << "Failed to find eid by guid, chipId :" << static_cast<int>(pfe.ubController.chipId)
                          << ", dieId :" << static_cast<int>(pfe.ubController.dieId)
                          << ", pfeId :" << static_cast<int>(pfe.pfeId);
            continue;
        }
        pfe.eid = it->second;
    }
}
} // namespace

UbseResult UbseMtiUrmaOutOfBand::GetIdevFeList(std::vector<UbseMtiIdevPfe>& feList)
{
    UbseCtrlQGetIdevFeReqMsg reqMsg;
    UbseCtrlQGetIdevFeRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetIdevFeList failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    feList = respMsg.GetPfeList();
    FillPfeEidByGuid(feList);
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::GetIdevFeDavidMapping(UbseMtiIdevFeDavidMapping& mapping)
{
    UbseCtrlQGetIdevFeDavidMappingReqMsg reqMsg;
    UbseCtrlQGetIdevFeDavidMappingRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetIdevFeDavidMapping failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    mapping = respMsg.GetMapping();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                                           std::vector<bool>& resList)
{
    UbseCtrlQBindVfeDavidReqMsg reqMsg(upi, vfeDavidList);
    UbseCtrlQBindVfeDavidRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "BindDavid failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                                             std::vector<bool>& resList)
{
    UbseCtrlQUnBindVfeDavidReqMsg reqMsg(upi, vfeDavidList);
    UbseCtrlQUnBindVfeDavidRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnBindDavid failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::RegDavidFeToVmBusInstance(const UbseMtiBusInst& busInstance,
                                                           const std::vector<UbseMtiIdevVfe>& vfeList,
                                                           std::vector<bool>& resList)
{
    UbseCtrlQRegDavidFeToBusInstanceReqMsg reqMsg(busInstance, vfeList);
    UbseCtrlQRegDavidFeToBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegDavidFeToVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}

UbseResult UbseMtiUrmaOutOfBand::UnRegDavidFeFromVmBusInstance(const UbseMtiBusInst& busInstance,
                                                               const std::vector<UbseMtiIdevVfe>& vfeList,
                                                               std::vector<bool>& resList)
{
    UbseCtrlQUnRegDavidFeFromBusInstanceReqMsg reqMsg(busInstance, vfeList);
    UbseCtrlQUnRegDavidFeFromBusInstanceRespMsg respMsg;
    auto ret = CtrlQMsgProxy::GetInstance().SendRequest(reqMsg, respMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnRegDavidFeFromVmBusInstance failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    resList = respMsg.GetRetList();
    return UBSE_OK;
}
} // namespace ubse::mti::urma
