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

#include "it_cli_invoker.h"

#include <sys/wait.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

#include "ubse_error.h"
#include "it_console_log.h"
#include "ubs_error.h"

namespace ubse::it::infra {

namespace {

constexpr uint32_t CLI_COMMAND_TIMEOUT_SECONDS = 3;
constexpr uint32_t CLI_COMMAND_KILL_AFTER_SECONDS = 1;
constexpr int32_t TIMEOUT_EXIT_CODE = 124;

std::string ShellQuote(const std::string& value)
{
    if (value.empty()) {
        return "''";
    }
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
            continue;
        }
        quoted += ch;
    }
    quoted += "'";
    return quoted;
}

std::string BuildCliEnvPrefix(const std::string& cliBinaryPath, const std::string& udsSocketPath)
{
    std::string envPrefix =
        "env UBSE_IT_UDS_SOCKET_PATH=" + ShellQuote(udsSocketPath) + " UBSE_UDS_ADDRESS=" + ShellQuote(udsSocketPath);

    std::filesystem::path preloadPath =
        std::filesystem::path(cliBinaryPath).parent_path().parent_path() / "lib" / "libubse_interface_preload.so";
    if (std::filesystem::exists(preloadPath)) {
        envPrefix += " LD_PRELOAD=" + ShellQuote(preloadPath.string());
    }
    return envPrefix;
}

std::string TrimTrailingNewlines(std::string output)
{
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    return output;
}

void LogCliOutput(const std::string& output)
{
    IT_LOG_INFO << "CLI command output:\n" << TrimTrailingNewlines(output);
}

} // namespace

ItCliInvoker::ItCliInvoker(const std::string& cliBinaryPath, const std::string& udsSocketPath)
    : cliBinaryPath_(cliBinaryPath),
      udsSocketPath_(udsSocketPath)
{
}

int32_t ItCliInvoker::QueryClusterInfo(std::vector<ItNodeInfo>& nodeInfos)
{
    std::string cmd = ShellQuote(cliBinaryPath_) + " display cluster";
    IT_LOG_INFO << "Executing command: " << cmd;
    std::string output = ExecuteCommand(cmd);
    if (output.empty()) {
        IT_LOG_ERROR << "CLI command returned empty output: " << cmd;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "CLI command returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    LogCliOutput(output);
    return ParseTableOutput(output, nodeInfos);
}

int32_t ItCliInvoker::QueryNodeInfo(ItNodeInfo& nodeInfo, const std::string& nodeId)
{
    std::string cmd = ShellQuote(cliBinaryPath_) + " display node";
    if (!nodeId.empty()) {
        cmd += " -n " + ShellQuote(nodeId);
    }
    IT_LOG_INFO << "Executing command: " << cmd;
    std::string output = ExecuteCommand(cmd);
    if (output.empty()) {
        IT_LOG_ERROR << "CLI command returned empty output: " << cmd;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "CLI command returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    LogCliOutput(output);
    std::vector<ItNodeInfo> nodeInfos;
    int32_t ret = ParseTableOutput(output, nodeInfos);
    if (ret != UBS_SUCCESS || nodeInfos.empty()) {
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    nodeInfo = nodeInfos[0];
    return UBS_SUCCESS;
}

std::string ItCliInvoker::ExecCli(const std::string& args) const
{
    std::string cmd = ShellQuote(cliBinaryPath_) + " " + args;
    IT_LOG_INFO << "Executing command: " << cmd;
    std::string output = ExecuteCommand(cmd);
    if (!output.empty()) {
        LogCliOutput(output);
    }
    return output;
}

std::string ItCliInvoker::ExecuteCommand(const std::string& command) const
{
    std::string fullCmd = "timeout --kill-after=" + std::to_string(CLI_COMMAND_KILL_AFTER_SECONDS) + "s " +
                          std::to_string(CLI_COMMAND_TIMEOUT_SECONDS) + "s " +
                          BuildCliEnvPrefix(cliBinaryPath_, udsSocketPath_) + " " + command + " 2>&1";
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (pipe == nullptr) {
        IT_LOG_ERROR << "popen failed for command: " << fullCmd;
        return "";
    }
    std::array<char, 4096> buffer{};
    std::ostringstream oss;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        oss << buffer.data();
    }
    int32_t status = pclose(pipe);
    if (status != 0) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == TIMEOUT_EXIT_CODE) {
            IT_LOG_ERROR << "CLI command timed out after " << CLI_COMMAND_TIMEOUT_SECONDS << "s: " << command;
        } else if (WIFEXITED(status)) {
            IT_LOG_ERROR << "CLI command exited with status " << WEXITSTATUS(status) << ": " << command;
        } else {
            IT_LOG_ERROR << "CLI command terminated abnormally: " << command;
        }
        return "";
    }
    return oss.str();
}

int32_t ItCliInvoker::ParseTableOutput(const std::string& output, std::vector<ItNodeInfo>& nodeInfos) const
{
    std::istringstream iss(output);
    std::string line;
    int separatorCount = 0;
    while (std::getline(iss, line)) {
        if (line.find("---") != std::string::npos) {
            ++separatorCount;
            continue;
        }
        // Skip everything before the second separator (top separator + header row + mid separator)
        if (separatorCount < 2) {
            continue;
        }
        std::istringstream lineStream(line);
        std::string node;
        std::string role;
        std::string bondingEid;
        std::string guid;
        lineStream >> node >> role >> bondingEid >> guid;
        if (node.empty() || role.empty()) {
            continue;
        }
        ItNodeInfo info;
        info.node = node;
        info.role = role;
        info.bondingEid = bondingEid;
        info.guid = guid;
        nodeInfos.push_back(info);
    }
    if (nodeInfos.empty()) {
        IT_LOG_WARN << "No node info parsed from CLI output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    return UBS_SUCCESS;
}

std::string ItCliInvoker::ExtractNodeId(const std::string& nodeName)
{
    std::string::size_type openParen = nodeName.find('(');
    std::string::size_type closeParen = nodeName.find(')');
    if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen + 1) {
        return nodeName.substr(openParen + 1, closeParen - openParen - 1);
    }
    return nodeName;
}

} // namespace ubse::it::infra
