/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_mem_controller_module.h"

#include <atomic>

#include "lcne/ubse_lcne_decoder_entry.h"
#include "lcne/ubse_lcne_decoder_specification.h"
#include "rpc/UbseMemDebInfoQueryHandler.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_mem_rpc.h"

namespace ubse::mem::controller {
using namespace ubse::config;
using namespace ubse::mem::controller::agent;
using namespace ubse::mem::controller;
using namespace ubse::mem_controller::rpc;
using namespace ubse::mem_controller;
DYNAMIC_CREATE(UbseMemControllerModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

const uint32_t RES_MAX_TIMEOUT_SECONDS = 3600;
const uint32_t CYCLE_CHECK_TIME_MS = 300000;
static std::atomic<bool> g_startCheckDecoderHandle{false};
#ifdef UB_ENVIRONMENT
const uint32_t RES_WAIT_TIMEOUT(3600); // 秒
#else
const uint32_t RES_WAIT_TIMEOUT(60); // 秒
#endif

void DelHandleByMapDiff(const decoder::utils::DecoderLocTohandleValueMap &allHandleValues,
                        const decoder::utils::DecoderLocTohandleMap &handleMap,
                        std::vector<mami::UbseMamiMemWithdraw> &faultInfo)
{
    UBSE_LOG_INFO << "DelHandleByMapDiff Begin";
    std::vector<mami::UbseMamiMemWithdraw> diffHandleInfo{};
    for (const auto &[loc, hanValues] : allHandleValues) {
        for (const auto &handValue : hanValues) {
            if (handleMap.find(loc) == handleMap.end() || handleMap.at(loc).count(handValue.handle) != 1) {
                mami::UbseMamiMemWithdraw tmpDelInfo{.ubpuId = loc.ubpuId,
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
        res = lcne::UbseLcneDecoderEntry::GetInstance().DeleteDecoderEntry(delInfo);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed, ubpuId is " << delInfo.ubpuId << " iouId is "
                           << delInfo.iouId << " marId is " << delInfo.marId << " decoderId is " << delInfo.decoderIdx
                           << "handle is " << delInfo.handle << ", " << FormatRetCode(res);
            faultInfo.push_back(delInfo);
        }
    }
}

UbseResult CycleCheckDecoderHandle()
{
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> socketIdToChipDie{};
    auto res = decoder::utils::MemDecoderUtils::GetCurNodeSocketInfo(socketIdToChipDie);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeSocketInfo failed, " << FormatRetCode(res);
        return res;
    }
    decoder::utils::DecoderLocTohandleValueMap allHandleValues{};
    res = decoder::utils::MemDecoderUtils::GetAllHandles(allHandleValues);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllHandles failed, " << FormatRetCode(res);
        return res;
    }

    decoder::utils::DecoderLocTohandleMap handleMap{};
    res = decoder::utils::MemDecoderUtils::GetHandleMapFromImportObj(handleMap);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetHandleMapFromImportObj failed, " << FormatRetCode(res);
        return res;
    }

    std::vector<mami::UbseMamiMemWithdraw> faultInfo{};
    DelHandleByMapDiff(allHandleValues, handleMap, faultInfo);
    if (!faultInfo.empty()) {
        for (const auto delInfo : faultInfo) {
            UBSE_LOG_ERROR << "delete one diff handle failed, ubpuId is " << delInfo.ubpuId << " iouId is "
                           << delInfo.iouId << " marId is " << delInfo.marId << " decoderId is " << delInfo.decoderIdx
                           << "handle is " << delInfo.handle << ", " << FormatRetCode(UBSE_ERROR);
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
    UbseResult ret = RegisterNodeCtlNotify();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg node ctl notify.";
        return ret;
    }
    UbseNodeController::GetInstance().RegLocalStateNotifyHandler(EnableCycleCheck);
    ubse::timer::UbseTimerHandlerRegister(
        "handleCheckTimer",
        []() -> UbseResult {
            if (g_startCheckDecoderHandle.load() != true) {
                return UBSE_OK;
            }
            return CycleCheckDecoderHandle();
        },
        CYCLE_CHECK_TIME_MS);
    return UBSE_OK;
}

void UbseMemControllerModule::UnInitialize()
{
    ubse::timer::UbseTimerHandlerUnregister("handleCheckTimer");
}

void SetDecoderSpecification()
{
    mami::UbseMamiMemDecoderSetInfo decoderSetInfo{};
    mami::UbseMamiMemDecoderInfo decoder0{0, 0, 0, 0};
    mami::UbseMamiMemDecoderInfo decoder1{1, 512, 0, 0};
    decoderSetInfo.decoder.push_back(decoder0);
    decoderSetInfo.decoder.push_back(decoder1);
    decoderSetInfo.decoderNum = 2;
    decoderSetInfo.marId = 0;
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> outSocketInfo;
    auto res = decoder::utils::MemDecoderUtils::GetCurNodeSocketInfo(outSocketInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetCurNodeSocketInfo failed, used default decoderSpecification";
        return;
    }
    for (auto [socketId, valPair] : outSocketInfo) {
        decoderSetInfo.ubpuId = valPair.first;
        decoderSetInfo.iouId = valPair.second;
        lcne::UbseLcneDecoderSpecification::GetInstance().SetDecoderSpecification(decoderSetInfo);
    }
}

UbseResult UbseMemControllerModule::Start()
{
    ubse::mem::controller::Init();
    if (auto ret = MemScheduleHandler::RegHandler(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg mem schedule handler.";
        return ret;
    }
    UbseResult ret = UbseMemDebInfoQueryHandler::RegUbseMemDebInfoQueryHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg UbseMemDebInfoQueryHandler.";
        return ret;
    }
    return ubse::mem::controller::agent::Init();
}

void UbseMemControllerModule::Stop() {}
} // namespace ubse::mem::controller