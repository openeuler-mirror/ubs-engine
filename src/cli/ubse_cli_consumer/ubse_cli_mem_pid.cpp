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

#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_constant.h"
#include "ubse_cli_mem_struct.h"
#include "ubse_cli_reg.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"
#include "ubse_pointer_process.h"
#include "ubse_serial_util.h"

UBSE_DEFINE_THIS_MODULE("ubse_cli");

namespace ubse::cli::reg {
using namespace ubse::cli::framework;
using namespace ubse::serial;

namespace {
constexpr uint16_t PID_MODULE_CODE = UBSE_MEM;

std::shared_ptr<UbseCliResultEcho> HandleSimpleIpcResponse(ubse_api_buffer_t& resBuffer, uint32_t invokeRet,
                                                           const char* successMsg)
{
    UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
    if (invokeRet != UBSE_OK) {
        UBSE_LOG_ERROR << "IPC invoke failed, ret=" << invokeRet;
        return UbseCliRegModule::UbseCliStringPromptReply(GetErrorMessage(invokeRet));
    }
    UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
    int successCode = 0;
    std::string errMsg;
    deserial >> successCode >> errMsg;
    if (deserial.Check() && successCode == 1) {
        return UbseCliRegModule::UbseCliStringPromptReply(successMsg);
    }
    if (!deserial.Check()) {
        return UbseCliRegModule::UbseCliStringPromptReply("ERROR: Invalid response from daemon.");
    }
    UBSE_LOG_ERROR << "Daemon error response: successCode=" << successCode << ", errMsg=" << errMsg;
    return UbseCliRegModule::UbseCliStringPromptReply("ERROR: " + errMsg);
}

std::shared_ptr<UbseCliResultEcho> HandleStructIpcResponse(ubse_api_buffer_t& resBuffer, uint32_t invokeRet,
                                                           std::function<std::string()> formatSuccess)
{
    UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
    if (invokeRet != UBSE_OK) {
        UBSE_LOG_ERROR << "IPC invoke failed, ret=" << invokeRet;
        return UbseCliRegModule::UbseCliStringPromptReply(GetErrorMessage(invokeRet));
    }
    UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
    int successCode = 0;
    std::string errMsg;
    deserial >> successCode >> errMsg;
    if (deserial.Check() && successCode == 1) {
        return UbseCliRegModule::UbseCliStringPromptReply(formatSuccess());
    }
    if (!deserial.Check()) {
        return UbseCliRegModule::UbseCliStringPromptReply("ERROR: Invalid response from daemon.");
    }
    UBSE_LOG_ERROR << "Daemon error response: successCode=" << successCode << ", errMsg=" << errMsg;
    return UbseCliRegModule::UbseCliStringPromptReply("ERROR: " + errMsg);
}

static constexpr uint64_t kBytesPerKiB = 1024ULL;
static constexpr uint64_t kBytesPerMiB = kBytesPerKiB * kBytesPerKiB;
static constexpr uint64_t kBytesPerGiB = kBytesPerKiB * kBytesPerKiB * kBytesPerKiB;

static std::string FormatMaxMemoryHuman(uint64_t bytes)
{
    // 统一以 G 为单位回显: 整除显示整数 G, 否则显示小数 G (去尾零)
    if (bytes % kBytesPerGiB == 0) {
        return std::to_string(bytes / kBytesPerGiB) + "G";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(5) << (static_cast<double>(bytes) / static_cast<double>(kBytesPerGiB));
    std::string value = oss.str();
    size_t end = value.find_last_not_of('0');
    if (end != std::string::npos) {
        value.erase(end + 1);
    }
    if (!value.empty() && value.back() == '.') {
        value.pop_back();
    }
    return value + "G";
}

static std::string FormatRatioHuman(double ratio)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << ratio;
    return oss.str();
}

std::shared_ptr<UbseCliResultEcho> BuildProcessMemConfigTable(
    const std::vector<process_mem::def::ProcessMemDisplayEntry>& entries)
{
    if (entries.empty()) {
        return UbseCliRegModule::UbseCliStringPromptReply("No process is currently managed by process_mem");
    }
    constexpr size_t COL_COUNT = 4;
    constexpr size_t COL_WIDTH = 30;
    UbseCliResBuilder builder(COL_COUNT, COL_WIDTH);
    size_t row = builder.UbseCliAddRow();
    builder.UbseCliAddlineSeparate(row);
    builder.UbseCliSetCellData(row, 1, "PID");
    builder.UbseCliSetCellData(row, 2, "Name");
    builder.UbseCliSetCellData(row, 3, "Size");
    builder.UbseCliSetCellData(row, 4, "RemoteRatio");
    builder.UbseCliAddBottomlineSeparate();

    for (const auto& entry : entries) {
        row = builder.UbseCliAddRow();
        builder.UbseCliSetCellData(row, 1, entry.pid > 0 ? std::to_string(entry.pid) : "N/A");
        builder.UbseCliSetCellData(row, 2, entry.name);
        builder.UbseCliSetCellData(row, 3, FormatMaxMemoryHuman(entry.maxMemory));
        builder.UbseCliSetCellData(row, 4, FormatRatioHuman(entry.remoteRatio));
    }
    builder.UbseCliAddBottomlineSeparate();
    return UbseCliRegModule::UbseCliVariableCelReply(builder.UbseCliVariableCellBuild());
}

} // namespace

class UbseCliMemPid::UbseCliMemPidImpl {
public:
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliSetProcessMemImpl(
        const process_mem::def::ProcessMemNewConfigInfo& newConfig)
    {
        UbseSerialization serializer;
        auto ret = newConfig.Serialize(serializer);
        if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
        }
        ubse_api_buffer_t reqBuffer{serializer.GetBuffer(), static_cast<uint32_t>(serializer.GetLength())};
        ubse_api_buffer_t resBuffer{};
        UbseCliWaitIndicator waitIndicator("Setting process-mem config");
        ret = ubse_invoke_call(PID_MODULE_CODE, UBSE_MEM_CLI_PROC_MEM_SET, &reqBuffer, &resBuffer);
        return HandleStructIpcResponse(resBuffer, ret, [&newConfig]() {
            std::ostringstream echo;
            if (newConfig.isPid) {
                echo << "PID: " << newConfig.identifier << "\n";
            } else {
                echo << "Name: " << newConfig.identifier << "\n";
            }
            echo << "Size: " << FormatMaxMemoryHuman(newConfig.maxMemory) << "\n";
            echo << "RemoteRatio: " << std::fixed << std::setprecision(2) << newConfig.remoteRatio << "\n";
            echo << "Set process-mem config successfully";
            return echo.str();
        });
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliRemoveProcessMemImpl(bool isPid, const std::string& identifier)
    {
        process_mem::def::ProcessMemNewConfigInfo newConfig{};
        newConfig.isPid = isPid;
        newConfig.identifier = identifier;
        UbseSerialization serializer;
        auto serRet = newConfig.Serialize(serializer);
        if (serRet != UBSE_OK || !serializer.Check()) {
            return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
        }
        ubse_api_buffer_t reqBuffer{serializer.GetBuffer(), static_cast<uint32_t>(serializer.GetLength())};
        ubse_api_buffer_t resBuffer{};
        uint32_t ret = ubse_invoke_call(PID_MODULE_CODE, UBSE_MEM_CLI_PROC_MEM_REMOVE, &reqBuffer, &resBuffer);
        std::string successMsg = isPid ? ("pid " + identifier + " removed") : ("name '" + identifier + "' removed");
        return HandleSimpleIpcResponse(resBuffer, ret, successMsg.c_str());
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliDisplayProcessMemImpl(uint32_t opcode)
    {
        UbseSerialization serial;
        ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
        ubse_api_buffer_t resBuffer{};
        uint32_t ret = ubse_invoke_call(PID_MODULE_CODE, opcode, &reqBuffer, &resBuffer);
        UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "DisplayProcessMem IPC invoke failed, ret=" << ret;
            return UbseCliRegModule::UbseCliStringPromptReply(GetErrorMessage(ret));
        }

        UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
        size_t count = 0;
        deserial >> count;
        if (!deserial.Check()) {
            return UbseCliRegModule::UbseCliStringPromptReply("ERROR: Deserialization failed.");
        }

        std::vector<process_mem::def::ProcessMemDisplayEntry> entries;
        for (size_t i = 0; i < count; ++i) {
            process_mem::def::ProcessMemDisplayEntry entry;
            if (entry.Deserialize(deserial) != UBSE_OK) {
                return UbseCliRegModule::UbseCliStringPromptReply("ERROR: Deserialization failed.");
            }
            entries.push_back(std::move(entry));
        }

        return BuildProcessMemConfigTable(entries);
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliDisplayProcessMemConfigImpl()
    {
        return UbseCliDisplayProcessMemImpl(UBSE_MEM_CLI_PROC_MEM_DISPLAY_CONFIG);
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliDisplayProcessMemDetailImpl()
    {
        return UbseCliDisplayProcessMemImpl(UBSE_MEM_CLI_PROC_MEM_DISPLAY_DETAIL);
    }
};

UbseCliMemPid::UbseCliMemPid()
{
    pImpl_ = SafeMakeUnique<UbseCliMemPidImpl>();
}

UbseCliMemPid::~UbseCliMemPid() noexcept = default;

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliSetProcessMem(
    const process_mem::def::ProcessMemNewConfigInfo& newConfig)
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliSetProcessMemImpl(newConfig);
}

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliRemoveProcessMem(bool isPid,
                                                                                     const std::string& identifier)
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliRemoveProcessMemImpl(isPid, identifier);
}

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliDisplayProcessMemConfig()
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliDisplayProcessMemConfigImpl();
}

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemPid::UbseCliDisplayProcessMemDetail()
{
    if (pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return pImpl_->UbseCliDisplayProcessMemDetailImpl();
}
} // namespace ubse::cli::reg
