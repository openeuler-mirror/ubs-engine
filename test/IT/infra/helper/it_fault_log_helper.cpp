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

#include "it_fault_log_helper.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#include "it_console_log.h"

namespace ubse::it::infra {

namespace {
constexpr const char* FAULT_LOG_PREFIX = "[UBSE_MEM] ";

std::string ExtractValue(const std::string& line, const std::string& key)
{
    std::string searchKey = key + "=";
    auto pos = line.find(searchKey);
    if (pos == std::string::npos) {
        return {};
    }
    pos += searchKey.size();
    auto endPos = line.find(',', pos);
    if (endPos == std::string::npos) {
        endPos = line.size();
    }
    return line.substr(pos, endPos - pos);
}

uint64_t ExtractUint64(const std::string& line, const std::string& key)
{
    std::string val = ExtractValue(line, key);
    if (val.empty()) {
        return 0;
    }
    // Strip trailing "byte" suffix if present
    auto bytePos = val.find("byte");
    if (bytePos != std::string::npos) {
        val = val.substr(0, bytePos);
    }
    return std::stoull(val);
}

uint32_t ExtractUint32(const std::string& line, const std::string& key)
{
    std::string val = ExtractValue(line, key);
    if (val.empty()) {
        return 0;
    }
    return static_cast<uint32_t>(std::stoul(val));
}
} // namespace

std::string ItFaultLogHelper::ReadFaultLog(const std::string& faultLogPath)
{
    std::ifstream file(faultLogPath);
    if (!file.is_open()) {
        IT_LOG_INFO << "[FaultLogHelper] Cannot open fault log: " << faultLogPath;
        return {};
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

bool ItFaultLogHelper::ParseFaultLogEntry(const std::string& line, FaultLogEntry& entry)
{
    auto prefixPos = line.find(FAULT_LOG_PREFIX);
    if (prefixPos == std::string::npos) {
        return false;
    }

    // Extract processDesc: text between "[UBSE_MEM] " and ". RequestName="
    auto contentStart = prefixPos + strlen(FAULT_LOG_PREFIX);
    auto dotPos = line.find(". RequestName=", contentStart);
    if (dotPos != std::string::npos) {
        entry.processDesc = line.substr(contentStart, dotPos - contentStart);
    }

    entry.requestName = ExtractValue(line, "RequestName");
    entry.borrowType = ExtractValue(line, "BorrowType");
    entry.requestSize = ExtractUint64(line, "RequestSize");
    entry.exportNode = ExtractValue(line, "ExportNode");
    entry.importNode = ExtractValue(line, "ImportNode");
    entry.requestNode = ExtractValue(line, "RequestNode");
    entry.masterNode = ExtractValue(line, "MasterNode");
    entry.errorCode = ExtractValue(line, "ErrorCode");
    entry.errorInfo = ExtractValue(line, "ErrorInfo");
    entry.adviceCode = ExtractUint32(line, "AdviceCode");
    entry.advice = ExtractValue(line, "Advice");

    return !entry.errorCode.empty();
}

std::vector<FaultLogEntry> ItFaultLogHelper::ParseAllEntries(const std::string& faultLogPath)
{
    std::vector<FaultLogEntry> entries;
    std::ifstream file(faultLogPath);
    if (!file.is_open()) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        FaultLogEntry entry{};
        if (ParseFaultLogEntry(line, entry)) {
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

std::vector<FaultLogEntry> ItFaultLogHelper::WaitForFaultLog(const std::string& faultLogPath,
                                                             std::function<bool(const FaultLogEntry&)> matcher,
                                                             uint32_t timeoutMs, uint32_t pollIntervalMs)
{
    auto startTime = std::chrono::steady_clock::now();
    std::vector<FaultLogEntry> allMatches;

    while (true) {
        auto entries = ParseAllEntries(faultLogPath);
        for (auto& entry : entries) {
            if (matcher(entry)) {
                allMatches.push_back(std::move(entry));
            }
        }
        if (!allMatches.empty()) {
            return allMatches;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        if (elapsed.count() >= timeoutMs) {
            IT_LOG_INFO << "[FaultLogHelper] WaitForFaultLog timed out after " << timeoutMs << "ms";
            return {};
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }
}

void ItFaultLogHelper::ClearFaultLog(const std::string& faultLogPath)
{
    std::ofstream file(faultLogPath, std::ios::trunc);
    if (!file.is_open()) {
        IT_LOG_INFO << "[FaultLogHelper] Cannot clear fault log (may not exist yet): " << faultLogPath;
        return;
    }
    file.close();
}

} // namespace ubse::it::infra
