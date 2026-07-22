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

#ifndef UBS_ENGINE_UBSE_SSU_DIRECT_TO_VM_MANAGER_H
#define UBS_ENGINE_UBSE_SSU_DIRECT_TO_VM_MANAGER_H
#include <atomic>
#include <condition_variable>
#include <map>
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_mti_bus_instance.h"
#include "ubse_mti_urma.h"

namespace ubse::ssu::service {
using namespace ubse::plugin::service::ssu;
using namespace ubse::mti::urma;
using namespace ubse::mti::bus_instance;

class UbseSsuDirectToVmManager {
public:
    enum class SsuDirectToVmManagerState
    {
        AVAILABLE,
        RUNNING_ALLOC,
        RUNNING_FREE,
        CLEARING_BG,
    };

    static UbseSsuDirectToVmManager &GetInstance();
    SsuDirectToVmManagerState GetState();
    void SetState(SsuDirectToVmManagerState state);
    uint32_t GetFeDeviceList(std::vector<UbseSsuFe> &feList);
    uint32_t FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid);
    uint32_t FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe);
    uint32_t StartClearTimer();
    void StopClearTimer();

private:
    UbseSsuDirectToVmManager() = default;
    std::atomic<SsuDirectToVmManagerState> state_{SsuDirectToVmManagerState::AVAILABLE};
    std::condition_variable cv_;
    std::mutex mtx_;
    std::atomic<bool> clearTimerStarted_{false};

    static std::string GuidToStr(const UbseMtiGuid &guid);
    static UbseSsuVfe ConvertVfe(const UbseMtiIdevVfe &vfe);
    static UbseSsuFe ConvertFe(const UbseMtiIdevPfe &pfe);
    static uint32_t QueryPfeList(std::vector<UbseMtiIdevPfe> &pfes);
    static uint32_t FillBusInstanceGuid(std::vector<UbseSsuFe> &feList);
    static UbseResult FindVmBusInst(const std::string &busInstanceGuid, UbseMtiBusInst &busInst);
    static UbseResult FindVfeInPfeList(const UbseSsuVfe &vfe, const std::vector<UbseMtiIdevPfe> &pfes,
                                       UbseMtiIdevVfe &mtiVfe);
    static bool FindVfeByGuidInPfes(const std::string &vfeGuid, const std::vector<UbseMtiIdevPfe> &pfes,
                                    UbseMtiIdevVfe &mtiVfe);
    static UbseResult CheckVfeOccupied(const std::string &vfeGuid, std::string &occupiedBusInstanceGuid);
    static bool HasOtherVfe(const UbseMtiBusInst &busInst, const std::string &vfeGuid);
    UbseResult AllocPreparation(const UbseSsuVfe &vfe, std::vector<UbseMtiIdevPfe> &pfes, UbseMtiIdevVfe &mtiVfe,
                                std::string &occupiedBusInstanceGuid);
    UbseResult AllocExecution(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid,
                              const std::vector<UbseMtiIdevPfe> &pfes, const UbseMtiIdevVfe &mtiVfe,
                              UbseMtiBusInst &busInst);
    UbseResult FreePreparation(uint32_t upi, const UbseSsuVfe &vfe, const std::string &busInstanceGuid,
                               UbseMtiBusInst &busInst, UbseMtiIdevVfe &mtiVfe);
    UbseResult HandleOccupiedVfe(const UbseSsuVfe &vfe, const std::string &occupiedBusInstanceGuid,
                                 const UbseMtiIdevVfe &mtiVfe);
    UbseResult HandleExistingVfesOnTargetBusInst(const UbseMtiBusInst &busInst, const UbseSsuVfe &vfe,
                                                 const std::vector<UbseMtiIdevPfe> &pfes);
    UbseResult CreateVmBusInst(uint16_t upi, UbseMtiBusInst &busInst);
    UbseResult DestroyVmBusInst(const UbseMtiBusInst &busInst);
    UbseResult RegVfeToVmBusInst(const UbseMtiBusInst &busInst, const UbseMtiIdevVfe &mtiVfe);
    UbseResult UnRegVfeFromVmBusInst(const UbseMtiBusInst &busInst, const UbseMtiIdevVfe &mtiVfe);
    void ClearEmptyVMBusInstance();
};
} // namespace ubse::ssu::service
#endif // UBS_ENGINE_UBSE_SSU_DIRECT_TO_VM_MANAGER_H
