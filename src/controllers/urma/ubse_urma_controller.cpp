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

#include "ubse_urma_controller.h"
#include "src/res_plugins/mti/lcne/ubse_lcne_qos.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_logger_inner.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma.h"
#include "ubse_urma_controller_manager.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::lcne;
using namespace ubse::com;
using namespace ubse::urma;
using namespace ubse::task_executor;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

UbseResult UrmaController::UbseUrmaBandWidthSet(const std::string urmaName, uint32_t minBandWidth,
                                                uint32_t maxBandWidth)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthSet Start," << urmaName << ", minBandWidth=" << minBandWidth
                  << ", maxBandWidth=" << maxBandWidth;
    /* 从urma名获取urma信息，如果找不到返回错误码UBS_ENGINE_ERR_NOT_EXIST */
    UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed, urmaName =" << urmaName;
        return UBSE_ERROR_SRCH;
    }
    /* 判断是否独享类型，不是返回不支持 */
    if (urmaInfo.urmaDevType != UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthSet failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERROR_SRCH;
    }

    /* 创建profile */
    UbseQosProfile ubseQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    ubseQosProfile.proflieName = profileName;
    ubseQosProfile.minBandWidth = minBandWidth;
    ubseQosProfile.maxBandWidth = maxBandWidth;
    ret = UbseLcneQos::GetInstance().CreatQosProfile(ubseQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::CreatQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }

    /* 对Fe下发Qos带宽 */
    for (auto i : urmaInfo.feInfoLists) {
        ret = UbseLcneQos::GetInstance().ApplyVfeQos(i, profileName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::ApplyVfeQos failed";
            return UBSE_ERROR_SRCH;
        }
    }
    return ret;
}

UbseResult UrmaController::UbseUrmaBandWidthGet(const std::string urmaName, uint32_t &minBandWidth,
                                                uint32_t &maxBandWidth)
{
    UbseQosProfile ubseQosProfile;
    UBSE_LOG_INFO << "UbseUrmaBandWidthGet Start," << urmaName;
    const std::string profileName = "Profile_" + urmaName;
    uint32_t ret = UbseLcneQos::GetInstance().QureyQosProfile(profileName, ubseQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::QureyQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    minBandWidth = ubseQosProfile.minBandWidth;
    maxBandWidth = ubseQosProfile.maxBandWidth;
    return ret;
}

UbseResult UrmaController::UbseUrmaBandWidthReset(const std::string urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthReset Start," << urmaName;
    /* 从urma名获取urma信息，如果找不到返回错误码UBS_ENGINE_ERR_NOT_EXIST */
    UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed, urmaName =" << urmaName;
        return UBSE_ERROR_SRCH;
    }
    /* 判断是否独享类型，不是返回不支持 */
    if (urmaInfo.urmaDevType != UrmaDevType::UNIQUE) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthReset failed, urmaDevType ="
                       << (uint32_t)urmaInfo.urmaDevType;
        return UBSE_ERROR_SRCH;
    }
    for (auto i : urmaInfo.feInfoLists) {
        UbseFeInfo ubseFeInfo;
        ret = UbseLcneQos::GetInstance().DeleteVfeQos(i);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseLcneQos::DeleteVfeQos failed.";
            return UBSE_ERROR_SRCH;
        }
    }

    const std::string profileName = "Profile_" + urmaName;
    ret = UbseLcneQos::GetInstance().DeleteQosProfile(profileName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseLcneQos::DeleteQosProfile failed," << profileName << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    return ret;
}

void UrmaController::UbseUrmaBandWidthUpdate(const std::string urmaName)
{
    UBSE_LOG_INFO << "UbseUrmaBandWidthUpdate Start," << urmaName;
    UbseUrmaInfo urmaInfo;
    uint32_t ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    {
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseUrmaControllerManager::GetLocalUrmaDevInfo failed," << urmaName;
            return;
        }
    }
    /* 先查询是否存在对应的Qos配置，没有则返回成功 */
    UbseQosProfile ubseQosProfile;
    const std::string profileName = "Profile_" + urmaName;
    ret = UbseLcneQos::GetInstance().QureyQosProfile(profileName, ubseQosProfile);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "UbseLcneQos::QureyQosProfile failed," << profileName << FormatRetCode(ret);
        return;
    }
    /* 接下来遍历该urma下的Fe是否都生效了该proflie配置 */
    bool isNeedUpdate = false;
    for (auto i : urmaInfo.feInfoLists) {
        std::string vfeProfileName;
        ret = UbseLcneQos::GetInstance().QueryVfeQos(i, vfeProfileName);
        if ((ret != UBSE_OK) || (vfeProfileName != profileName)) {
            isNeedUpdate = true;
            break;
        }
    }
    if (!isNeedUpdate) {
        return;
    }
    /* 先删除VFE上面所有的生效Qos，然后再删除profile */
    for (auto i : urmaInfo.feInfoLists) {
        UbseLcneQos::GetInstance().DeleteVfeQos(i);
    }
    UbseLcneQos::GetInstance().DeleteQosProfile(profileName);
    return;
}

} // namespace ubse::urmaController