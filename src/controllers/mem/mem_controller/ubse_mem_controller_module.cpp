/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/
#include "ubse_mem_controller_module.h"

#include <atomic>
#include "adapter_plugins/mti/ubse_mti_interface.h"

#include "rpc/ubse_mem_controller_rpc_register.h"
#include "rpc/ubse_mem_get_opt_result_handler.h"
#include "src/adapter_plugins/mmi/ubse_mmi_module.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_decoder_entry.h"
#include "src/controllers/mem/mem_controller/rpc/ubse_mem_debt_info_query_handler.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "ubse_service_registry.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_controller_dispatcher.h"
#include "ubse_mem_controller_fault_handle.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_service_impl.h"
#include "ubse_timer.h"

namespace ubse::mem::controller {
using namespace ubse::mem::controller::agent;
using namespace ubse::mem::controller::rpc;
using namespace ubse::mmi;
using namespace adapter_plugins::mti::mami;
using namespace ubse::utils;
static constexpr auto G_UBSE_MEM_DEPS = std::array<UbseOptionModule, 12>{
    UbseOptionModule::UbseNodeControllerModule,
    UbseOptionModule::UbseElectionModule,
    UbseOptionModule::UbseMmiModule,
    UbseOptionModule::UbseLcneModule,
    UbseOptionModule::UbseComModule,
    UbseOptionModule::UbseStorageModule,
    UbseOptionModule::UbseRasModule,
    UbseOptionModule::SysSentryModule,
    UbseOptionModule::UbseUrmaUvsModule,
    UbseOptionModule::UbseUrmaControllerModule,
    UbseOptionModule::UbseNodeMgrModule,
    UbseOptionModule::UbsePluginModule,
};
PLUGIN_MODULE_IMPL(UbseMemControllerModule, G_UBSE_MEM_DEPS);
UBSE_DEFINE_THIS_MODULE("ubse");

const uint32_t CYCLE_CHECK_TIME_S = 300;
static std::atomic<bool> g_startCheckDecoderHandle{false};

void DelHandleByMapDiff(const mem::decoder::utils::DecoderLocTohandleValueMap &allHandleValues,
                        const mem::decoder::utils::DecoderLocTohandleMap &handleMap,
                        std::vector<UbseMamiMemWithdraw> &faultInfo)
{
    UBSE_LOG_INFO << "DelHandleByMapDiff Begin";
    std::vector<UbseMamiMemWithdraw> diffHandleInfo{};
    for (const auto &[loc, hanValues] : allHandleValues) {
        for (const auto &handValue : hanValues) {
            if (handleMap.find(loc) == handleMap.end() || handleMap.at(loc).count(handValue.handle) != 1) {
                UbseMamiMemWithdraw tmpDelInfo{.ubpuId = loc.ubpuId,
                                               .iouId = loc.iouId,
                                               .marId = loc.marId,
                                               .decoderIdx = loc.decoderId,
                                               .handle = handValue.handle};
                diffHandleInfo.push_back(tmpDelInfo);
            }
        }
    }

    UbseResult res = UBSE_OK;
    for (const auto &delInfo : diffHandleInfo) {
        UBSE_LOG_INFO << "one diff handle, ubpuId is " << delInfo.ubpuId << " iouId is " << delInfo.iouId
                      << " marId is " << delInfo.marId << " decoderId is " << delInfo.decoderIdx << "handle is "
                      << delInfo.handle;
        res = adapter_plugins::mti::UbseMtiInterface::GetInstance().DeleteDecoderEntry(delInfo);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed, ubpuId is " << delInfo.ubpuId << " iouId is "
                           << delInfo.iouId << " marId is " << delInfo.marId << " decoderId is " << delInfo.decoderIdx
                           << "handle is " << delInfo.handle;
            faultInfo.push_back(delInfo);
        }
    }
}

UbseResult CycleCheckDecoderHandle()
{
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie{};
    auto res = decoder::utils::MemDecoderUtils::GetCurNodeSocketInfo(socketIdToChipDie);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeSocketInfo failed";
        return res;
    }
    decoder::utils::DecoderLocTohandleValueMap allHandleValues{};
    res = decoder::utils::MemDecoderUtils::GetAllHandles(UB_MEMORY_HANDLE_DEFAULT_USED_NODE, allHandleValues);
    for (const auto &[loc, handles] : allHandleValues) {
        UBSE_LOG_INFO << "allHandleValues size is " << handles.size();
        for (const auto &handle : handles) {
            UBSE_LOG_INFO << "handles value is " << handle.handle;
        }
    }
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllHandles failed";
        return res;
    }

    decoder::utils::DecoderLocTohandleMap handleMap{};
    res = decoder::utils::MemDecoderUtils::GetAllHandleFromImportObj(handleMap);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllHandleFromImportObj failed";
        return res;
    }

    std::vector<UbseMamiMemWithdraw> faultInfo{};
    DelHandleByMapDiff(allHandleValues, handleMap, faultInfo);
    if (!faultInfo.empty()) {
        for (const auto delInfo : faultInfo) {
            UBSE_LOG_ERROR << "delete one diff handle failed, ubpuId is " << delInfo.ubpuId << " iouId is "
                           << delInfo.iouId << " marId is " << delInfo.marId << " decoderId is " << delInfo.decoderIdx
                           << "handle is " << delInfo.handle;
        }
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

uint32_t EnableCycleCheck(const ubse::nodeController::UbseNodeInfo &node)
{
    if (node.localState == UbseNodeLocalState::UBSE_NODE_READY) {
        CycleCheckDecoderHandle();
        g_startCheckDecoderHandle.store(true);
    }

    if (node.localState == UbseNodeLocalState::UBSE_NODE_RESTORE) {
        g_startCheckDecoderHandle.store(false);
    }

    return UBSE_OK;
}

UbseResult UbseMemControllerModule::Initialize()
{
    auto ret = CreateTaskExecutor();
    if (ret != UBSE_OK) {
        return ret;
    }
    RegisterNodeCtlNotify();
    UbseNodeController::GetInstance().RegLocalStateNotifyHandler(EnableCycleCheck);
    ubse::timer::UbseTimerHandlerRegister(
        "handleCheckTimer",
        []() -> UbseResult {
            if (g_startCheckDecoderHandle.load() != true) {
                return UBSE_OK;
            }
            return CycleCheckDecoderHandle();
        },
        CYCLE_CHECK_TIME_S);
    return UBSE_OK;
}

void UbseMemControllerModule::UnInitialize()
{
    ubse::mem::controller::UnInit();
    ubse::timer::UbseTimerHandlerUnregister("handleCheckTimer");
}

UbseResult UbseMemControllerModule::Start()
{
    ubse::mem::controller::Init();
    if (auto ret = MemScheduleHandler::RegHandler(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg mem schedule handler.";
        return ret;
    }
    UbseResult ret = RegMemControllerHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg MemControllerHandler.";
        return ret;
    }
    ret = UbseMemControllerDispatcher::RegisterSdkDispatcher();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg UbseMemControllerDispatcher.";
        return ret;
    }
    ret = UbseMemFaultManager::InitMemFaultManager();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to initialize mem fault handler.";
        return ret;
    }

    ret = UbseMemGetOptResultHandler::RegUbseMemGetOptResultHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg UbseMemGetOptResultHandler.";
        return ret;
    }

    memService_ = std::make_shared<ubse::service::mem::UbseMemServiceImpl>();
    service::UbseServiceRegistry::GetInstance().RegisterService<ubse::service::mem::UbseMemService>(memService_);

    return ubse::mem::controller::agent::Init();
}

void UbseMemControllerModule::Stop()
{
    service::UbseServiceRegistry::GetInstance().UnRegisterService<ubse::service::mem::UbseMemService>(memService_);
 	 
    auto ret = UbseMemFaultManager::DeInitMemFaultManager();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to delete mem fault handler.";
    }
    ubse::mem::controller::Stop();
}

} // namespace ubse::mem::controller
