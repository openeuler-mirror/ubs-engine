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

#include "ubse_ssu_direct_to_vm_manager.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_timer.h"

namespace ubse::ssu::service {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::timer;
constexpr uint32_t SSU_FE_ID = 6; // 6： 设计规定 1650 FE 6给SSU使用
constexpr uint8_t SLEEP_TIME = 2;
constexpr uint8_t COMMON_RETRY_TIME = 5;
constexpr uint32_t CLEAR_VM_BUS_INST_INTERVAL_SECONDS = 60;
constexpr const char *CLEAR_VM_BUS_INST_TIMER_NAME = "SsuClearEmptyVMBusInst";

UbseSsuDirectToVmManager &UbseSsuDirectToVmManager::GetInstance()
{
    static UbseSsuDirectToVmManager ssuManager;
    return ssuManager;
}

UbseSsuDirectToVmManager::SsuDirectToVmManagerState UbseSsuDirectToVmManager::GetState()
{
    return state_.load();
}

void UbseSsuDirectToVmManager::SetState(SsuDirectToVmManagerState stateX)
{
    state_ = stateX;
    if (stateX == SsuDirectToVmManagerState::AVAILABLE) {
        std::lock_guard<std::mutex> lock(mtx_);
        cv_.notify_all();
    }
}

uint32_t UbseSsuDirectToVmManager::StartClearTimer()
{
    bool expected = false;
    if (!clearTimerStarted_.compare_exchange_strong(expected, true)) {
        UBSE_LOG_INFO << "ClearEmptyVMBusInstance timer already started";
        return UBSE_OK;
    }
    auto ret = UbseTimerHandlerRegister(
        CLEAR_VM_BUS_INST_TIMER_NAME,
        [this]() {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                if (GetState() != SsuDirectToVmManagerState::AVAILABLE) {
                    UBSE_LOG_INFO << "Skip clear empty VM bus instance, state is not AVAILABLE";
                    return UBSE_OK;
                }
                state_ = SsuDirectToVmManagerState::CLEARING_BG;
            }
            ClearEmptyVMBusInstance();
            SetState(SsuDirectToVmManagerState::AVAILABLE);
            return UBSE_OK;
        },
        CLEAR_VM_BUS_INST_INTERVAL_SECONDS);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register clear timer, " << FormatRetCode(ret);
        clearTimerStarted_ = false;
        return ret;
    }
    UBSE_LOG_INFO << "ClearEmptyVMBusInstance timer started, interval=" << CLEAR_VM_BUS_INST_INTERVAL_SECONDS << "s";
    return UBSE_OK;
}

void UbseSsuDirectToVmManager::StopClearTimer()
{
    if (!clearTimerStarted_.exchange(false)) {
        return;
    }
    UbseTimerHandlerUnregister(CLEAR_VM_BUS_INST_TIMER_NAME);
    UBSE_LOG_INFO << "ClearEmptyVMBusInstance timer stopped";
}

void UbseSsuDirectToVmManager::ClearEmptyVMBusInstance()
{
    std::vector<UbseMtiBusInst> busInstanceList{};
    const UbseResult ret = UbseMtiBusInstance::GetInstance().GetBusInstanceList(busInstanceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetBusInstanceList failed, " << FormatRetCode(ret);
        return;
    }
    for (const auto &busInst : busInstanceList) {
        if (busInst.type != UbseMtiBusInstanceType::VM) {
            continue;
        }
        if (!busInst.subDeviceGuids.empty()) {
            continue;
        }
        UBSE_LOG_INFO << "Destroy empty VM bus instance " << GuidToStr(busInst.guid);
        const UbseResult destroyRet = DestroyVmBusInst(busInst);
        if (destroyRet != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to destroy empty VM bus instance " << GuidToStr(busInst.guid);
        }
    }
    UBSE_LOG_INFO << "Clear empty VM bus instances done";
}

uint32_t UbseSsuDirectToVmManager::GetFeDeviceList(std::vector<UbseSsuFe> &feList)
{
    std::vector<UbseMtiIdevPfe> pfes{};
    if (const UbseResult ret = QueryPfeList(pfes); ret != UBSE_OK) {
        return ret;
    }
    for (const UbseMtiIdevPfe &pfe : pfes) {
        if (pfe.pfeId == SSU_FE_ID) {
            feList.push_back(ConvertFe(pfe));
        }
    }
    return FillBusInstanceGuid(feList);
}

uint32_t UbseSsuDirectToVmManager::QueryPfeList(std::vector<UbseMtiIdevPfe> &pfes)
{
    const UbseResult ret = UbseMtiUrma::GetInstance().GetIdevFeList(pfes);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get fe list, " << FormatRetCode(ret);
    }
    return ret;
}

UbseSsuVfe UbseSsuDirectToVmManager::ConvertVfe(const UbseMtiIdevVfe &vfe)
{
    UbseSsuVfe ssuVfe;
    ssuVfe.slotId = vfe.ubController.slotId;
    ssuVfe.chipId = vfe.ubController.chipId;
    ssuVfe.dieId = vfe.ubController.dieId;
    ssuVfe.pfeId = vfe.pfeId;
    ssuVfe.vfeId = vfe.vfeId;
    ssuVfe.vfeGuid = GuidToStr(vfe.guid);
    return ssuVfe;
}

UbseSsuFe UbseSsuDirectToVmManager::ConvertFe(const UbseMtiIdevPfe &pfe)
{
    UbseSsuFe ssuFe;
    ssuFe.slotId = pfe.ubController.slotId;
    ssuFe.chipId = pfe.ubController.chipId;
    ssuFe.dieId = pfe.ubController.dieId;
    ssuFe.pfeId = pfe.pfeId;
    ssuFe.pfeGuid = GuidToStr(pfe.guid);
    for (const UbseMtiIdevVfe &vfe : pfe.vfeList) {
        ssuFe.vfeList.push_back(ConvertVfe(vfe));
    }
    return ssuFe;
}

uint32_t UbseSsuDirectToVmManager::FillBusInstanceGuid(std::vector<UbseSsuFe> &feList)
{
    std::vector<UbseMtiBusInst> busInstanceList{};
    const UbseResult ret = UbseMtiBusInstance::GetInstance().GetBusInstanceList(busInstanceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetBusInstanceList failed, " << FormatRetCode(ret);
        return ret;
    }
    std::map<std::string, std::string> vfeToBusInst;
    for (const auto &busInst : busInstanceList) {
        if (busInst.type != UbseMtiBusInstanceType::VM) {
            continue;
        }
        const std::string busInstanceGuid = GuidToStr(busInst.guid);
        for (const auto &subDeviceGuid : busInst.subDeviceGuids) {
            vfeToBusInst[GuidToStr(subDeviceGuid)] = busInstanceGuid;
        }
    }
    for (auto &fe : feList) {
        for (auto &vfe : fe.vfeList) {
            auto it = vfeToBusInst.find(vfe.vfeGuid);
            if (it != vfeToBusInst.end()) {
                vfe.bindBusInstanceGuid = it->second;
            }
        }
    }
    return UBSE_OK;
}

std::string UbseSsuDirectToVmManager::GuidToStr(const UbseMtiGuid &guid)
{
    UbseMtiGuid reversed{};
    size_t reversedSize = guid.size() - 1;
    for (size_t i = 0; i < guid.size(); i++) {
        reversed.at(i) = guid.at(reversedSize - i);
    }
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0');
    for (uint8_t byte : reversed) {
        oss << std::setw(NO_2) << static_cast<int>(byte);
    }
    return oss.str();
}

UbseResult UbseSsuDirectToVmManager::FindVmBusInst(const std::string &busInstanceGuid, UbseMtiBusInst &busInst)
{
    std::vector<UbseMtiBusInst> busInstanceList{};
    const UbseResult ret = UbseMtiBusInstance::GetInstance().GetBusInstanceList(busInstanceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetBusInstanceList failed, " << FormatRetCode(ret);
        return ret;
    }
    auto it = std::find_if(busInstanceList.begin(), busInstanceList.end(), [&](const UbseMtiBusInst &inst) {
        return inst.type == UbseMtiBusInstanceType::VM && GuidToStr(inst.guid) == busInstanceGuid;
    });
    if (it != busInstanceList.end()) {
        busInst = *it;
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "BusInstance not found: " << busInstanceGuid;
    return UBSE_ERROR_INVAL;
}

template <typename Func>
UbseResult RetryWithBackoff(Func &&op, uint8_t retryTime, const std::string &opName)
{
    for (uint8_t i = 0; i < retryTime; i++) {
        const UbseResult ret = op();
        if (ret == UBSE_OK) {
            return UBSE_OK;
        }
        UBSE_LOG_ERROR << opName << " failed (attempt " << i + 1 << "), " << FormatRetCode(ret);
        if (i < retryTime - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(SLEEP_TIME));
        }
    }
    return UBSE_ERROR;
}

UbseResult UbseSsuDirectToVmManager::CreateVmBusInst(uint16_t upi, UbseMtiBusInst &busInst)
{
    return RetryWithBackoff([&]() { return UbseMtiBusInstance::GetInstance().CreateVmBusInstance(upi, busInst); },
                            COMMON_RETRY_TIME, "CreateVmBusInstance");
}

UbseResult UbseSsuDirectToVmManager::DestroyVmBusInst(const UbseMtiBusInst &busInst)
{
    return RetryWithBackoff([&]() { return UbseMtiBusInstance::GetInstance().DestroyVmBusInstance(busInst); },
                            COMMON_RETRY_TIME, "DestroyVmBusInstance");
}

UbseResult UbseSsuDirectToVmManager::RegVfeToVmBusInst(const UbseMtiBusInst &busInst, const UbseMtiIdevVfe &mtiVfe)
{
    return RetryWithBackoff(
        [&]() {
            std::vector<UbseMtiIdevVfe> vfeList{mtiVfe};
            std::vector<bool> resList{};
            const UbseResult ret = UbseMtiUrma::GetInstance().RegDavidFeToVmBusInstance(busInst, vfeList, resList);
            if (ret != UBSE_OK) {
                return ret;
            }
            if (resList.empty() || !resList[0]) {
                UBSE_LOG_ERROR << "RegDavidFeToVmBusInstance returned OK but VFE registration failed";
                return UBSE_ERROR;
            }
            return UBSE_OK;
        },
        COMMON_RETRY_TIME, "RegDavidFeToVmBusInstance");
}

UbseResult UbseSsuDirectToVmManager::UnRegVfeFromVmBusInst(const UbseMtiBusInst &busInst, const UbseMtiIdevVfe &mtiVfe)
{
    return RetryWithBackoff(
        [&]() {
            std::vector<UbseMtiIdevVfe> vfeList{mtiVfe};
            std::vector<bool> resList{};
            const UbseResult ret = UbseMtiUrma::GetInstance().UnRegDavidFeFromVmBusInstance(busInst, vfeList, resList);
            if (ret != UBSE_OK) {
                return ret;
            }
            if (resList.empty() || !resList[0]) {
                UBSE_LOG_ERROR << "UnRegDavidFeFromVmBusInstance returned OK but VFE unregistration failed";
                return UBSE_ERROR;
            }
            return UBSE_OK;
        },
        COMMON_RETRY_TIME, "UnRegDavidFeFromVmBusInstance");
}

UbseResult UbseSsuDirectToVmManager::FindVfeInPfeList(const UbseSsuVfe &vfe, const std::vector<UbseMtiIdevPfe> &pfes,
                                                      UbseMtiIdevVfe &mtiVfe)
{
    for (const auto &pfe : pfes) {
        if (pfe.pfeId != SSU_FE_ID) {
            continue;
        }
        auto it = std::find_if(pfe.vfeList.begin(), pfe.vfeList.end(), [&](const UbseMtiIdevVfe &ivfe) {
            return ivfe.vfeId == vfe.vfeId && ivfe.ubController.slotId == vfe.slotId &&
                   ivfe.ubController.chipId == vfe.chipId && ivfe.ubController.dieId == vfe.dieId;
        });
        if (it != pfe.vfeList.end()) {
            mtiVfe = *it;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "VFE not found in PFE list: slotId=" << vfe.slotId << " chipId=" << vfe.chipId
                   << " dieId=" << vfe.dieId << " pfeId=" << vfe.pfeId << " vfeId=" << vfe.vfeId;
    return UBSE_ERROR_INVAL;
}

bool UbseSsuDirectToVmManager::FindVfeByGuidInPfes(const std::string &vfeGuid, const std::vector<UbseMtiIdevPfe> &pfes,
                                                   UbseMtiIdevVfe &mtiVfe)
{
    for (const auto &pfe : pfes) {
        auto it = std::find_if(pfe.vfeList.begin(), pfe.vfeList.end(),
                               [&](const UbseMtiIdevVfe &ivfe) { return GuidToStr(ivfe.guid) == vfeGuid; });
        if (it != pfe.vfeList.end()) {
            mtiVfe = *it;
            return true;
        }
    }
    return false;
}

UbseResult UbseSsuDirectToVmManager::CheckVfeOccupied(const std::string &vfeGuid, std::string &occupiedBusInstanceGuid)
{
    std::vector<UbseMtiBusInst> busInstanceList{};
    const UbseResult ret = UbseMtiBusInstance::GetInstance().GetBusInstanceList(busInstanceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetBusInstanceList failed, " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &busInst : busInstanceList) {
        if (busInst.type != UbseMtiBusInstanceType::VM) {
            continue;
        }
        auto it = std::find_if(busInst.subDeviceGuids.begin(), busInst.subDeviceGuids.end(),
                               [&](const UbseMtiGuid &subGuid) { return GuidToStr(subGuid) == vfeGuid; });
        if (it != busInst.subDeviceGuids.end()) {
            occupiedBusInstanceGuid = GuidToStr(busInst.guid);
            UBSE_LOG_WARN << "VFE " << vfeGuid << " is already registered to VM bus instance "
                          << occupiedBusInstanceGuid;
            return UBSE_OK;
        }
    }
    occupiedBusInstanceGuid.clear();
    return UBSE_OK;
}

UbseResult UbseSsuDirectToVmManager::HandleOccupiedVfe(const UbseSsuVfe &vfe,
                                                       const std::string &occupiedBusInstanceGuid,
                                                       const UbseMtiIdevVfe &mtiVfe)
{
    if (occupiedBusInstanceGuid.empty()) {
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "Preempt: unregister VFE " << vfe.vfeGuid << " from occupied bus instance "
                  << occupiedBusInstanceGuid;
    UbseMtiBusInst occupiedBusInst{};
    const UbseResult findRet = FindVmBusInst(occupiedBusInstanceGuid, occupiedBusInst);
    if (findRet != UBSE_OK) {
        return findRet;
    }
    const UbseResult ret = UnRegVfeFromVmBusInst(occupiedBusInst, mtiVfe);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to preempt VFE from occupied bus instance, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "Success to preempt VFE from occupied bus instance " << occupiedBusInstanceGuid;
    return UBSE_OK;
}

UbseResult UbseSsuDirectToVmManager::HandleExistingVfesOnTargetBusInst(const UbseMtiBusInst &busInst,
                                                                       const UbseSsuVfe &vfe,
                                                                       const std::vector<UbseMtiIdevPfe> &pfes)
{
    for (const auto &subDeviceGuid : busInst.subDeviceGuids) {
        std::string existingVfeGuid = GuidToStr(subDeviceGuid);
        if (existingVfeGuid == vfe.vfeGuid) {
            continue;
        }
        UbseMtiIdevVfe existingVfe{};
        if (!FindVfeByGuidInPfes(existingVfeGuid, pfes, existingVfe)) {
            UBSE_LOG_WARN << "Existing VFE " << existingVfeGuid << " not found in PFE list, skip";
            continue;
        }
        UBSE_LOG_INFO << "Unregister existing VFE " << existingVfeGuid << " from target bus instance "
                      << GuidToStr(busInst.guid);
        const UbseResult ret = UnRegVfeFromVmBusInst(busInst, existingVfe);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    return UBSE_OK;
}

bool UbseSsuDirectToVmManager::HasOtherVfe(const UbseMtiBusInst &busInst, const std::string &vfeGuid)
{
    return std::any_of(busInst.subDeviceGuids.begin(), busInst.subDeviceGuids.end(),
                       [&](const UbseMtiGuid &subGuid) { return GuidToStr(subGuid) != vfeGuid; });
}

UbseResult UbseSsuDirectToVmManager::AllocPreparation(const UbseSsuVfe &vfe, std::vector<UbseMtiIdevPfe> &pfes,
                                                      UbseMtiIdevVfe &mtiVfe, std::string &occupiedBusInstanceGuid)
{
    const UbseResult pfeRet = QueryPfeList(pfes);
    if (pfeRet != UBSE_OK) {
        return pfeRet;
    }
    const UbseResult vfeRet = FindVfeInPfeList(vfe, pfes, mtiVfe);
    if (vfeRet != UBSE_OK) {
        return vfeRet;
    }
    return CheckVfeOccupied(vfe.vfeGuid, occupiedBusInstanceGuid);
}

UbseResult UbseSsuDirectToVmManager::AllocExecution(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid,
                                                    const std::vector<UbseMtiIdevPfe> &pfes,
                                                    const UbseMtiIdevVfe &mtiVfe, UbseMtiBusInst &busInst)
{
    if (!busInstanceGuid.empty()) {
        const UbseResult findRet = FindVmBusInst(busInstanceGuid, busInst);
        if (findRet != UBSE_OK) {
            return findRet;
        }
        const UbseResult existingRet = HandleExistingVfesOnTargetBusInst(busInst, vfe, pfes);
        if (existingRet != UBSE_OK) {
            return existingRet;
        }
    } else {
        const UbseResult createRet = CreateVmBusInst(static_cast<uint16_t>(upi), busInst);
        if (createRet != UBSE_OK) {
            return createRet;
        }
        busInstanceGuid = GuidToStr(busInst.guid);
    }
    return RegVfeToVmBusInst(busInst, mtiVfe);
}

uint32_t UbseSsuDirectToVmManager::FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid)
{
    if (upi > UINT16_MAX) {
        UBSE_LOG_ERROR << "Invalid upi value: " << upi << ", exceeds uint16_t range";
        return UBSE_ERROR_INVAL;
    }
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return GetState() == SsuDirectToVmManagerState::AVAILABLE; });
        SetState(SsuDirectToVmManagerState::RUNNING_ALLOC);
    }

    std::vector<UbseMtiIdevPfe> pfes;
    UbseMtiIdevVfe mtiVfe;
    std::string occupiedBusInstanceGuid;
    const UbseResult prepRet = AllocPreparation(vfe, pfes, mtiVfe, occupiedBusInstanceGuid);
    if (prepRet != UBSE_OK) {
        SetState(SsuDirectToVmManagerState::AVAILABLE);
        return prepRet;
    }

    if (!occupiedBusInstanceGuid.empty()) {
        if (!busInstanceGuid.empty() && busInstanceGuid == occupiedBusInstanceGuid) {
            SetState(SsuDirectToVmManagerState::AVAILABLE);
            return UBSE_OK;
        }
        const UbseResult preemptRet = HandleOccupiedVfe(vfe, occupiedBusInstanceGuid, mtiVfe);
        if (preemptRet != UBSE_OK) {
            SetState(SsuDirectToVmManagerState::AVAILABLE);
            return preemptRet;
        }
    }

    UbseMtiBusInst busInst;
    const UbseResult execRet = AllocExecution(upi, vfe, busInstanceGuid, pfes, mtiVfe, busInst);
    if (execRet != UBSE_OK) {
        SetState(SsuDirectToVmManagerState::AVAILABLE);
        return execRet;
    }

    SetState(SsuDirectToVmManagerState::AVAILABLE);
    UBSE_LOG_INFO << "Success to alloc VFE device. Bus instance guid: " << busInstanceGuid;
    return UBSE_OK;
}

UbseResult UbseSsuDirectToVmManager::FreePreparation(uint32_t upi, const UbseSsuVfe &vfe,
                                                     const std::string &busInstanceGuid, UbseMtiBusInst &busInst,
                                                     UbseMtiIdevVfe &mtiVfe)
{
    const UbseResult findRet = FindVmBusInst(busInstanceGuid, busInst);
    if (findRet != UBSE_OK) {
        return findRet;
    }
    if (busInst.upi != static_cast<uint16_t>(upi)) {
        UBSE_LOG_ERROR << "UPI mismatch for bus instance " << busInstanceGuid;
        return UBSE_ERROR_INVAL;
    }
    std::vector<UbseMtiIdevPfe> pfes;
    const UbseResult pfeRet = QueryPfeList(pfes);
    if (pfeRet != UBSE_OK) {
        return pfeRet;
    }
    return FindVfeInPfeList(vfe, pfes, mtiVfe);
}

uint32_t UbseSsuDirectToVmManager::FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe, const std::string &busInstanceGuid)
{
    if (upi > UINT16_MAX) {
        UBSE_LOG_ERROR << "Invalid upi value: " << upi << ", exceeds uint16_t range";
        return UBSE_ERROR_INVAL;
    }
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return GetState() == SsuDirectToVmManagerState::AVAILABLE; });
        SetState(SsuDirectToVmManagerState::RUNNING_FREE);
    }

    UbseMtiBusInst busInst;
    UbseMtiIdevVfe mtiVfe;
    const UbseResult prepRet = FreePreparation(upi, vfe, busInstanceGuid, busInst, mtiVfe);
    if (prepRet != UBSE_OK) {
        SetState(SsuDirectToVmManagerState::AVAILABLE);
        return prepRet;
    }

    const UbseResult unregRet = UnRegVfeFromVmBusInst(busInst, mtiVfe);
    if (unregRet != UBSE_OK) {
        SetState(SsuDirectToVmManagerState::AVAILABLE);
        return UBSE_ERROR;
    }
    if (!HasOtherVfe(busInst, vfe.vfeGuid)) {
        const UbseResult destroyRet = DestroyVmBusInst(busInst);
        if (destroyRet != UBSE_OK) {
            SetState(SsuDirectToVmManagerState::AVAILABLE);
            return UBSE_ERROR;
        }
    } else {
        UBSE_LOG_WARN << "Bus instance " << busInstanceGuid << " still has other devices, skip destroying";
    }

    SetState(SsuDirectToVmManagerState::AVAILABLE);
    UBSE_LOG_INFO << "Success to free VFE device from bus instance " << busInstanceGuid;
    return UBSE_OK;
}

} // namespace ubse::ssu::service
