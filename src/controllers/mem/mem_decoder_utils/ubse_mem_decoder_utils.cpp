#include "src/res_plugins/mti/lcne/ubse_lcne_decoder_handle.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_node_controller.h"

namespace ubse::mem::decoder::utils {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID);
using namespace ubse::log;
using namespace lcne;

UbseResult MemDecoderUtils::GetChipAndDieId(const uint32_t socketId, std::pair<std::string, std::string> &chipDiePair)
{
    bool isSocketIdExit = false;
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == socketId) {
            isSocketIdExit = true;
            UBSE_LOG_DEBUG << "[MTI_MEM] socketId is " << cpuInfo.second.socketId;
            chipDiePair.first = cpuInfo.second.chipId;
            UBSE_LOG_DEBUG << "[MTI_MEM] chipId is " << cpuInfo.second.chipId;
            chipDiePair.second = cpuInfo.second.cardId;
            UBSE_LOG_DEBUG << "[MTI_MEM] cardId is " << cpuInfo.second.cardId;
            break;
        }
    }

    if (!isSocketIdExit) {
        UBSE_LOG_ERROR << "socketId=" << socketId << " doesn't exit, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult MemDecoderUtils::GetAllHandles(const UbseMamiMemHandleQueryInfo &queryInfo, 
        std::vector<UbseMamiMemHandleValue> &handleValues)
{
    handleValues.clear();
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> outSocketInfo;
    auto res = GetCurNodeSocketInfo(outSocketInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI_MEM] no socketId found, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    UbseMamiMemHandleQueryInfo tempQueryInfo = queryInfo;
    std::vector<UbseMamiMemHandleValue> tempHandleValues{};
    for(const auto &[cpuLoc, valPair] : outSocketInfo) {
        UBSE_LOG_DEBUG << "[MTI_MEM] temp socketId is " << valPair.first;
        tempQueryInfo.ubpuId = valPair.first;
        tempQueryInfo.iouId = valPair.second;
        tempHandleValues.clear();
        auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(tempQueryInfo, tempHandleValues);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MTI_MEM] get handles of socketId=, " << valPair.first << " failed, "
                            << FormatRetCode(UBSE_ERROR);
            return ret;
        }
        handleValues.insert(handleValues.end(), tempHandleValues.begin(), tempHandleValues.end());
    }

    return UBSE_OK;
}

std::vector<uint32_t> MemDecoderUtils::GetAllChipId()
{
    std::vector<uint32_t> chipIdList;
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        chipIdList.emplace_back(stoi(cpuInfo.second.chipId));
    }
    return chipIdList;
}

UbseResult MemDecoderUtils::GetCurNodeSocketInfo(
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &outSocketInfo)
{
    auto nodeInfo = nodeController::UbseNodeController::GetInstance().GetCurNode();
    for (const auto &[cpuLoc, socketInfo] : nodeInfo.cpuInfos) {
        try {
            outSocketInfo[cpuLoc.socketId] = std::make_pair(std::stoi(socketInfo.chipId), std::stoi(socketInfo.cardId));
        } catch (...) {
            UBSE_LOG_ERROR << "stoi throw one exception";
            return UBSE_ERROR;
        }
    }

    return UBSE_OK;
}
}