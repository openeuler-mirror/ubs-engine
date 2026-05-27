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

#include "ubse_cli_mem_pid.h"

#include <string>
#include <vector>

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_constant.h"
#include "ubse_cli_reg.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_pointer_process.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;
using namespace ubse::serial;

namespace {
constexpr uint16_t PID_MODULE_CODE = UBSE_MEM;
constexpr uint16_t PID_SET_THRESHOLD_OP = UBSE_MEM_CLI_PID_SET_THRESHOLD;
constexpr uint16_t PID_PRINT_OP = UBSE_MEM_CLI_PRINT_PID_INFO;
constexpr uint16_t PID_UNSET_OP = UBSE_MEM_CLI_PID_UNSET;

bool PidInfoDeSerialization(UbseDeSerialization& in, std::vector<process_mem::def::ProcessMemPidInfo>& data)
{
    size_t dataSize{};
    in >> dataSize;
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < dataSize; ++i) {
        process_mem::def::ProcessMemPidInfo tmpInfo{};
        auto ret = tmpInfo.configInfo.DeserializeConfigInfo(in);
        if (ret != UBSE_OK) {
            return false;
        }
        data.push_back(tmpInfo);
    }
    return true;
}

std::shared_ptr<UbseCliResultEcho> BuildPidInfoTable(
    const std::vector<process_mem::def::ProcessMemPidInfo>& pidInfoDetail)
{
    UbseCliResBuilder variableCellBuilder(UBSE_CLI_NUM_6, UBSE_CLI_NUM_10 + UBSE_CLI_NUM_10);
    size_t row = variableCellBuilder.UbseCliAddRow();
    variableCellBuilder.UbseCliAddlineSeparate(row);
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "pid");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "evictThreshold");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "targetEvictThreshold");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "reclaimThreshold");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "totalMemoryUsage");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "srcNuma");
    variableCellBuilder.UbseCliAddBottomlineSeparate();
    for (const auto& pidInfo : pidInfoDetail) {
        row = variableCellBuilder.UbseCliAddRow();
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, std::to_string(pidInfo.configInfo.pid));
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, std::to_string(pidInfo.configInfo.evictThreshold));
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_3,
                                               std::to_string(pidInfo.configInfo.targetEvictThreshold));
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_4,
                                               std::to_string(pidInfo.configInfo.reclaimThreshold));
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_5,
                                               std::to_string(pidInfo.configInfo.expectedMemoryUsage));
        variableCellBuilder.UbseCliSetCellData(
            row, UBSE_CLI_NUM_6,
            pidInfo.configInfo.srcNumaId.has_value() ? std::to_string(pidInfo.configInfo.srcNumaId.value()) : "N/A");
    }
    variableCellBuilder.UbseCliAddBottomlineSeparate();
    return UbseCliRegModule::UbseCliVariableCelReply(variableCellBuilder.UbseCliVariableCellBuild());
}
} // anonymous namespace

class UbseCliMemPid::UbseCliMemPidImpl {
public:
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliSetPidThresholdImpl(
        const process_mem::def::ProcessMemPidInfo& pidInfo)
    {
        UbseSerialization serializer;
        auto ret = pidInfo.configInfo.SerializeConfigInfo(serializer);
        if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
        }
        ubse_api_buffer_t reqBuffer{serializer.GetBuffer(), static_cast<uint32_t>(serializer.GetLength())};
        ubse_api_buffer_t resBuffer{};
        UbseCliWaitIndicator waitIndicator("Setting pidInfo");
        ret = ubse_invoke_call(PID_MODULE_CODE, PID_SET_THRESHOLD_OP, &reqBuffer, &resBuffer);
        UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
        if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(
                std::string("ERROR: Internal error with error code " + std::to_string(ret)));
        }
        UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
        int successCode = 0;
        std::string errMsg;
        deserial >> successCode >> errMsg;
        if (deserial.Check() && successCode == 1) {
            return UbseCliRegModule::UbseCliStringPromptReply("Set successfully");
        }
        return UbseCliRegModule::UbseCliStringPromptReply("ERROR: " + errMsg);
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliPrintPidInfoImpl()
    {
        UbseSerialization serial;
        ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
        ubse_api_buffer_t resBuffer{};
        uint32_t ret = ubse_invoke_call(PID_MODULE_CODE, PID_PRINT_OP, &reqBuffer, &resBuffer);
        UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
        if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(
                std::string("ERROR: Internal error with error code " + std::to_string(ret)));
        }
        UbseDeSerialization derial{resBuffer.buffer, resBuffer.length};
        std::vector<process_mem::def::ProcessMemPidInfo> pidInfo{};
        if (!PidInfoDeSerialization(derial, pidInfo)) {
            return UbseCliRegModule::UbseCliStringPromptReply("ERROR");
        }
        return BuildPidInfoTable(pidInfo);
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliUnsetPidImpl(pid_t pid)
    {
        UbseSerialization serial;
        serial << pid;
        ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
        ubse_api_buffer_t resBuffer{};
        uint32_t ret = ubse_invoke_call(PID_MODULE_CODE, PID_UNSET_OP, &reqBuffer, &resBuffer);
        UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
        if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(
                std::string("ERROR: Internal error with error code " + std::to_string(ret)));
        }
        UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
        int successCode = 0;
        std::string errMsg;
        deserial >> successCode >> errMsg;
        if (deserial.Check() && successCode == 1) {
            return UbseCliRegModule::UbseCliStringPromptReply("Unset successfully");
        }
        return UbseCliRegModule::UbseCliStringPromptReply("ERROR: " + errMsg);
    }
};

UbseCliMemPid::UbseCliMemPid()
{
    pImpl_ = SafeMakeUnique<UbseCliMemPidImpl>();
}

UbseCliMemPid::~UbseCliMemPid() noexcept = default;

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliSetPidThreshold(
    const process_mem::def::ProcessMemPidInfo& pidInfo)
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliSetPidThresholdImpl(pidInfo);
}

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliPrintPidInfo()
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliPrintPidInfoImpl();
}

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliUnsetPid(pid_t pid)
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliUnsetPidImpl(pid);
}
} // namespace ubse::cli::reg
