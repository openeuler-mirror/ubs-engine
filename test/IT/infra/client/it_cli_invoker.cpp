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
#include "it_cli_table_parser.h"

#include <sys/wait.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>

#include "ubse_error.h"
#include "it_console_log.h"
#include "it_string_util.h"
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

int32_t ItCliInvoker::QueryTopoCpu(std::vector<ItTopoCpuLink>& topoLinks)
{
    std::string output = ExecCli("display topo -t cpu");
    if (output.empty()) {
        IT_LOG_WARN << "display topo -t cpu returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "display topo -t cpu returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    UbseCliTableParser parser(output);
    auto records = parser.Parse();
    topoLinks.clear();
    for (const auto& rec : records) {
        ItTopoCpuLink link;
        auto it = rec.find("link-id");
        if (it != rec.end())
            link.linkId = it->second;
        it = rec.find("node");
        if (it != rec.end())
            link.node = it->second;
        it = rec.find("socket");
        if (it != rec.end())
            link.socket = it->second;
        it = rec.find("port");
        if (it != rec.end())
            link.port = it->second;
        it = rec.find("peer-node");
        if (it != rec.end())
            link.peerNode = it->second;
        it = rec.find("peer-socket");
        if (it != rec.end())
            link.peerSocket = it->second;
        it = rec.find("peer-port");
        if (it != rec.end())
            link.peerPort = it->second;
        it = rec.find("status");
        if (it != rec.end())
            link.status = it->second;
        topoLinks.push_back(std::move(link));
    }

    if (topoLinks.empty()) {
        IT_LOG_WARN << "No topo links parsed from CLI output";
    }
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::QueryMemBorrowDetail(std::vector<ItMemBorrowDetail>& borrowDetails)
{
    std::string output = ExecCli("display memory -t borrow_detail");
    if (output.empty()) {
        IT_LOG_WARN << "display memory -t borrow_detail returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "display memory -t borrow_detail returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    UbseCliTableParser parser(output);
    auto records = parser.Parse();
    borrowDetails.clear();
    for (const auto& rec : records) {
        ItMemBorrowDetail detail;
        auto it = rec.find("name");
        if (it != rec.end())
            detail.name = it->second;
        it = rec.find("type");
        if (it != rec.end())
            detail.type = it->second;
        it = rec.find("borrow_node");
        if (it != rec.end())
            detail.borrowNode = it->second;
        it = rec.find("lend_node");
        if (it != rec.end())
            detail.lendNode = it->second;
        it = rec.find("lend_numa");
        if (it != rec.end())
            detail.lendNuma = it->second;
        it = rec.find("lend_size");
        if (it != rec.end())
            detail.lendSize = it->second;
        it = rec.find("status");
        if (it != rec.end())
            detail.status = it->second;
        it = rec.find("handle");
        if (it != rec.end())
            detail.handle = it->second;
        borrowDetails.push_back(std::move(detail));
    }

    if (borrowDetails.empty()) {
        IT_LOG_WARN << "No borrow records parsed from CLI output";
    }
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
    return util::ExtractNodeId(nodeName);
}

int32_t ItCliInvoker::GetRole(std::string& role)
{
    ItNodeInfo nodeInfo;
    int32_t ret = QueryNodeInfo(nodeInfo);
    if (ret != UBS_SUCCESS) {
        IT_LOG_WARN << "CLI-based GetRole failed: " << ret;
        return ret;
    }
    role = nodeInfo.role;
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::GetMasterNodeId(std::string& masterNodeId)
{
    std::vector<ItNodeInfo> nodeInfos;
    int32_t ret = QueryClusterInfo(nodeInfos);
    if (ret != UBS_SUCCESS) {
        IT_LOG_WARN << "CLI-based GetMasterNodeId failed: " << ret;
        return ret;
    }

    for (const auto& info : nodeInfos) {
        if (info.role == ubse::election::ELECTION_ROLE_MASTER) {
            masterNodeId = ExtractNodeId(info.node);
            return UBS_SUCCESS;
        }
    }
    IT_LOG_WARN << "CLI-based GetMasterNodeId did not find master role";
    return UBS_ENGINE_ERR_CONNECTION_FAILED;
}

int32_t ItCliInvoker::GetCurrentNodeId(std::string& currentNodeId)
{
    ItNodeInfo nodeInfo;
    int32_t ret = QueryNodeInfo(nodeInfo);
    if (ret != UBS_SUCCESS) {
        IT_LOG_WARN << "CLI-based GetCurrentNodeId failed: " << ret;
        return ret;
    }
    currentNodeId = ExtractNodeId(nodeInfo.node);
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::GetAllNodeInfos(std::vector<ubse::election::UbseRoleInfo>& roleInfos)
{
    std::vector<ItNodeInfo> nodeInfos;
    int32_t ret = QueryClusterInfo(nodeInfos);
    if (ret != UBS_SUCCESS) {
        return ret;
    }
    for (const auto& info : nodeInfos) {
        ubse::election::UbseRoleInfo roleInfo;
        roleInfo.nodeId = ExtractNodeId(info.node);
        roleInfo.nodeRole = info.role;
        roleInfo.status = ubse::election::ELECTION_NODE_ONLINE;
        roleInfos.push_back(roleInfo);
    }
    return UBS_SUCCESS;
}

// --- Memory CLI operations ---

namespace {
std::string ParseCliField(const std::string& output, const std::string& fieldName)
{
    std::string key = fieldName + ":";
    auto pos = output.find(key);
    if (pos != std::string::npos) {
        auto start = pos + key.length();
        auto end = output.find('\n', start);
        if (end == std::string::npos)
            end = output.length();
        std::string value = output.substr(start, end - start);
        auto trimStart = value.find_first_not_of(" \t");
        auto trimEnd = value.find_last_not_of(" \t\r");
        if (trimStart != std::string::npos && trimEnd != std::string::npos)
            return value.substr(trimStart, trimEnd - trimStart + 1);
    }
    return std::string();
}
} // namespace

int32_t ItCliInvoker::CreateMemoryNuma(ItMemCreateInfo& createInfo, const std::string& name, const std::string& size,
                                       const std::string& link, bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string nameOpt = useLongOptions ? "--name" : "-n";
    std::string sizeOpt = useLongOptions ? "--size" : "-s";
    std::string linkOpt = useLongOptions ? "--link-id" : "-l";

    std::string cmd = "create memory " + typeOpt + " numa " + nameOpt + " " + ShellQuote(name) + " " + sizeOpt + " " +
                      ShellQuote(size);
    if (!link.empty()) {
        cmd += " " + linkOpt + " " + ShellQuote(link);
    }

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "create memory returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "create memory returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    // NUMA output: name, size, numa-id, import-node, export-node
    createInfo.name = ParseCliField(output, "name");
    createInfo.size = ParseCliField(output, "size");
    createInfo.numaId = ParseCliField(output, "numa-id");
    createInfo.importNode = ParseCliField(output, "import-node");
    createInfo.exportNode = ParseCliField(output, "export-node");

    IT_LOG_INFO << "Parsed create memory numa info: name=" << createInfo.name << ", size=" << createInfo.size
                << ", numa-id=" << createInfo.numaId << ", import-node=" << createInfo.importNode
                << ", export-node=" << createInfo.exportNode;

    return UBS_SUCCESS;
}

int32_t ItCliInvoker::CreateMemoryFd(ItMemCreateInfo& createInfo, const std::string& name, const std::string& size,
                                     bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string nameOpt = useLongOptions ? "--name" : "-n";
    std::string sizeOpt = useLongOptions ? "--size" : "-s";

    std::string cmd =
        "create memory " + typeOpt + " fd " + nameOpt + " " + ShellQuote(name) + " " + sizeOpt + " " + ShellQuote(size);

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "create memory fd returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "create memory fd returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    // FD output: name, size, mem-ids, import-node, export-node
    createInfo.name = ParseCliField(output, "name");
    createInfo.size = ParseCliField(output, "size");
    createInfo.memIds = ParseCliField(output, "mem-ids");
    createInfo.importNode = ParseCliField(output, "import-node");
    createInfo.exportNode = ParseCliField(output, "export-node");

    IT_LOG_INFO << "Parsed create memory fd info: name=" << createInfo.name << ", size=" << createInfo.size
                << ", mem-ids=" << createInfo.memIds << ", import-node=" << createInfo.importNode
                << ", export-node=" << createInfo.exportNode;

    return UBS_SUCCESS;
}

int32_t ItCliInvoker::CreateMemoryShare(ItMemCreateInfo& createInfo, const std::string& name, const std::string& size,
                                        const std::string& region, bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string nameOpt = useLongOptions ? "--name" : "-n";
    std::string sizeOpt = useLongOptions ? "--size" : "-s";
    std::string regionOpt = useLongOptions ? "--region" : "-r";

    std::string cmd = "create memory " + typeOpt + " share " + nameOpt + " " + ShellQuote(name) + " " + sizeOpt + " " +
                      ShellQuote(size) + " " + regionOpt + " " + ShellQuote(region);

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "create memory share returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "create memory share returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    // SHM create output: name, size, export-node, region
    createInfo.name = ParseCliField(output, "name");
    createInfo.size = ParseCliField(output, "size");
    createInfo.exportNode = ParseCliField(output, "export-node");
    createInfo.region = ParseCliField(output, "region");

    IT_LOG_INFO << "Parsed create memory share info: name=" << createInfo.name << ", size=" << createInfo.size
                << ", export-node=" << createInfo.exportNode << ", region=" << createInfo.region;

    return UBS_SUCCESS;
}

int32_t ItCliInvoker::AttachMemory(ItMemCreateInfo& attachInfo, const std::string& name, bool useLongOptions)
{
    std::string nameOpt = useLongOptions ? "--name" : "-n";

    std::string cmd = "attach memory " + nameOpt + " " + ShellQuote(name);

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "attach memory returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "attach memory returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    // SHM attach output: name, size, mem-ids, import-node, export-node, region
    attachInfo.name = ParseCliField(output, "name");
    attachInfo.size = ParseCliField(output, "size");
    attachInfo.memIds = ParseCliField(output, "mem-ids");
    attachInfo.importNode = ParseCliField(output, "import-node");
    attachInfo.exportNode = ParseCliField(output, "export-node");
    attachInfo.region = ParseCliField(output, "region");

    IT_LOG_INFO << "Parsed attach memory info: name=" << attachInfo.name << ", size=" << attachInfo.size
                << ", mem-ids=" << attachInfo.memIds << ", import-node=" << attachInfo.importNode
                << ", export-node=" << attachInfo.exportNode << ", region=" << attachInfo.region;

    return UBS_SUCCESS;
}

int32_t ItCliInvoker::DetachMemory(const std::string& name, bool useLongOptions)
{
    std::string nameOpt = useLongOptions ? "--name" : "-n";

    std::string cmd = "detach memory " + nameOpt + " " + ShellQuote(name);

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "detach memory returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "detach memory returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    IT_LOG_INFO << "detach memory succeeded: " << name;
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::DeleteMemory(const std::string& name, const std::string& type, bool useLongOptions)
{
    std::string nameOpt = useLongOptions ? "--name" : "-n";
    std::string typeOpt = useLongOptions ? "--type" : "-t";

    std::string cmd = "delete memory " + nameOpt + " " + ShellQuote(name) + " " + typeOpt + " " + ShellQuote(type);

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "delete memory returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "delete memory returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("Delete successfully") == std::string::npos) {
        IT_LOG_ERROR << "delete memory did not return success message: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    IT_LOG_INFO << "delete memory succeeded: " << name;
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::DisplayMemoryBorrowDetail(std::vector<ItMemBorrowDetail>& borrowDetails,
                                                const std::string& borrowType, const std::string& name,
                                                bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string btOpt = useLongOptions ? "--borrow-type" : "-bt";
    std::string nameOpt = useLongOptions ? "--name" : "-n";

    std::string cmd = "display memory " + typeOpt + " borrow_detail";
    if (!borrowType.empty()) {
        cmd += " " + btOpt + " " + ShellQuote(borrowType);
    }
    if (!name.empty()) {
        cmd += " " + nameOpt + " " + ShellQuote(name);
    }

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "display memory -t borrow_detail returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "display memory -t borrow_detail returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    UbseCliTableParser parser(output);
    auto records = parser.Parse();
    borrowDetails.clear();
    for (const auto& rec : records) {
        ItMemBorrowDetail detail;
        auto it = rec.find("name");
        if (it != rec.end())
            detail.name = it->second;
        it = rec.find("type");
        if (it != rec.end())
            detail.type = it->second;
        it = rec.find("borrow_node");
        if (it != rec.end())
            detail.borrowNode = it->second;
        it = rec.find("lend_node");
        if (it != rec.end())
            detail.lendNode = it->second;
        it = rec.find("lend_numa");
        if (it != rec.end())
            detail.lendNuma = it->second;
        it = rec.find("lend_size");
        if (it != rec.end())
            detail.lendSize = it->second;
        it = rec.find("status");
        if (it != rec.end())
            detail.status = it->second;
        it = rec.find("handle");
        if (it != rec.end())
            detail.handle = it->second;
        borrowDetails.push_back(std::move(detail));
    }

    if (borrowDetails.empty()) {
        IT_LOG_WARN << "No borrow records parsed from CLI output";
    }
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::DisplayMemoryNodeBorrow(std::vector<ItNodeBorrowInfo>& nodeBorrows, bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string cmd = "display memory " + typeOpt + " node_borrow";

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "display memory -t node_borrow returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "display memory -t node_borrow returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    UbseCliTableParser parser(output);
    auto records = parser.Parse();
    nodeBorrows.clear();
    for (const auto& rec : records) {
        ItNodeBorrowInfo info;
        auto it = rec.find("borrow_node");
        if (it != rec.end())
            info.borrowNode = it->second;
        it = rec.find("lend_node");
        if (it != rec.end())
            info.lendNode = it->second;
        it = rec.find("size");
        if (it != rec.end())
            info.size = it->second;
        nodeBorrows.push_back(std::move(info));
    }

    if (nodeBorrows.empty()) {
        IT_LOG_WARN << "No node borrow records parsed from CLI output";
    }
    return UBS_SUCCESS;
}

int32_t ItCliInvoker::DisplayMemoryNodeLend(std::vector<ItNodeLendInfo>& nodeLends, bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string cmd = "display memory " + typeOpt + " node_lend";

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "display memory -t node_lend returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "display memory -t node_lend returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    UbseCliTableParser parser(output);
    auto records = parser.Parse();
    nodeLends.clear();
    for (const auto& rec : records) {
        ItNodeLendInfo info;
        auto it = rec.find("lend_node");
        if (it != rec.end())
            info.lendNode = it->second;
        it = rec.find("borrow_node");
        if (it != rec.end())
            info.borrowNode = it->second;
        it = rec.find("size");
        if (it != rec.end())
            info.size = it->second;
        nodeLends.push_back(std::move(info));
    }

    if (nodeLends.empty()) {
        IT_LOG_WARN << "No node lend records parsed from CLI output";
    }
    return UBS_SUCCESS;
}

std::string ItCliInvoker::DisplayMemoryNumaStatus(bool showAll, bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string allOpt = useLongOptions ? "--all" : "-a";

    std::string cmd = "display memory " + typeOpt + " numa_status";
    if (showAll) {
        cmd += " " + allOpt;
    }
    return ExecCli(cmd);
}

int32_t ItCliInvoker::DisplayMemoryConfig(std::vector<ItMemConfigInfo>& configs, bool useLongOptions)
{
    std::string typeOpt = useLongOptions ? "--type" : "-t";
    std::string cmd = "display memory " + typeOpt + " config";

    std::string output = ExecCli(cmd);
    if (output.empty()) {
        IT_LOG_WARN << "display memory -t config returned empty output";
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }
    if (output.find("ERROR:") != std::string::npos) {
        IT_LOG_ERROR << "display memory -t config returned error: " << output;
        return UBS_ENGINE_ERR_CONNECTION_FAILED;
    }

    UbseCliTableParser parser(output);
    auto records = parser.Parse();
    configs.clear();
    for (const auto& rec : records) {
        ItMemConfigInfo config;
        auto it = rec.find("node");
        if (it != rec.end())
            config.node = it->second;
        it = rec.find("isLender");
        if (it != rec.end())
            config.isLender = it->second;
        configs.push_back(std::move(config));
    }

    if (configs.empty()) {
        IT_LOG_WARN << "No config records parsed from CLI output";
    }
    return UBS_SUCCESS;
}

} // namespace ubse::it::infra
