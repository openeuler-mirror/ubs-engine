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

#include "it_lcne_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <cstdlib>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <tuple>

#include "httplib.h"
#include "it_console_log.h"

namespace ubse::it::infra {

namespace {

// System file paths (same as UbseNodeController)
const std::string NUMA_PATH = "/sys/devices/system/node/has_cpu";
const std::string CPU_LIST_PREFIX_PATH = "/sys/devices/system/node/node";
const std::string CPU_LIST_SUFFIX_PATH = "/cpulist";
const std::string SOCKET_ID_PREFIX_PATH = "/sys/devices/system/cpu/cpu";
const std::string SOCKET_ID_SUFFIX_PATH = "/topology/physical_package_id";

/**
 * @brief Parse CPU list line (e.g., "0-3,5,7-9") into a vector of IDs.
 * Same logic as UbseNodeController::ProcessListLine.
 */
template <class T>
UbseResult ParseListLine(const std::string& line, T& idList)
{
    std::string token;
    std::stringstream ss(line);

    while (std::getline(ss, token, ',')) {
        size_t dashPos = token.find('-');
        if (dashPos == std::string::npos) {
            // Single ID
            try {
                auto id = static_cast<typename T::value_type>(std::stoul(token));
                idList.push_back(id);
            } catch (const std::exception& e) {
                IT_LOG_ERROR << "Parse ID failed: " << e.what();
                return UBSE_ERROR_DEF(1);
            }
        } else {
            // Range (e.g., "0-3")
            uint32_t start;
            uint32_t end;
            try {
                start = static_cast<uint32_t>(std::stoul(token.substr(0, dashPos)));
                end = static_cast<uint32_t>(std::stoul(token.substr(dashPos + 1)));
            } catch (const std::exception& e) {
                IT_LOG_ERROR << "Parse ID range failed: " << e.what();
                return UBSE_ERROR_DEF(1);
            }

            for (uint32_t id = start; id <= end; id++) {
                idList.push_back(static_cast<typename T::value_type>(id));
            }
        }
    }

    return UBSE_OK;
}

/**
 * @brief Get local numa list from system file.
 * Same logic as UbseNodeController::GetLocalNumas.
 */
UbseResult GetLocalNumas(std::vector<uint32_t>& numaIds)
{
    std::ifstream inputFile(NUMA_PATH);
    if (!inputFile.is_open()) {
        IT_LOG_ERROR << "Failed to open " << NUMA_PATH;
        return UBSE_ERROR_DEF(1);
    }

    std::string line;
    getline(inputFile, line);
    inputFile.close();

    return ParseListLine(line, numaIds);
}

/**
 * @brief Get CPU list for a specific numa from system file.
 * Same logic as UbseNodeController::CollectCpuList.
 */
UbseResult GetCpuListForNuma(uint32_t numaId, std::vector<uint16_t>& cpuList)
{
    const std::string cpuListPath = CPU_LIST_PREFIX_PATH + std::to_string(numaId) + CPU_LIST_SUFFIX_PATH;
    std::ifstream inputFile(cpuListPath);
    if (!inputFile.is_open()) {
        IT_LOG_ERROR << "Failed to open " << cpuListPath;
        return UBSE_ERROR_DEF(1);
    }

    std::string line;
    getline(inputFile, line);
    inputFile.close();

    return ParseListLine(line, cpuList);
}

/**
 * @brief Get socketId for a specific CPU from system file.
 * Same logic as UbseNodeController::GetSocketId.
 */
UbseResult GetSocketIdForCpu(uint16_t cpuId, uint32_t& socketId)
{
    const std::string socketIdPath = SOCKET_ID_PREFIX_PATH + std::to_string(cpuId) + SOCKET_ID_SUFFIX_PATH;
    std::ifstream inputFile(socketIdPath);
    if (!inputFile.is_open()) {
        IT_LOG_ERROR << "Failed to open " << socketIdPath;
        return UBSE_ERROR_DEF(1);
    }

    inputFile >> socketId;
    inputFile.close();
    return UBSE_OK;
}

/**
 * @brief Collect all socketIds from system files.
 * Similar to UbseNodeController::CollectNumaInfo logic.
 */
UbseResult CollectSocketIds(std::set<uint32_t>& socketIdSet)
{
    std::vector<uint32_t> numaIds;
    UbseResult ret = GetLocalNumas(numaIds);
    if (ret != UBSE_OK || numaIds.empty()) {
        IT_LOG_ERROR << "Failed to get local numa list";
        return ret;
    }

    for (const auto& numaId : numaIds) {
        std::vector<uint16_t> cpuList;
        ret = GetCpuListForNuma(numaId, cpuList);
        if (ret != UBSE_OK || cpuList.empty()) {
            IT_LOG_ERROR << "Failed to get CPU list for numa " << numaId;
            continue;
        }

        uint32_t socketId;
        ret = GetSocketIdForCpu(cpuList[0], socketId);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to get socketId for CPU " << cpuList[0];
            continue;
        }

        socketIdSet.insert(socketId);
    }

    return UBSE_OK;
}

} // namespace

ItLcneClient::ItLcneClient(const std::string& udsPath) : udsPath_(udsPath) {}

ItLcneClient::~ItLcneClient() {}

UbseResult ItLcneClient::GetTopologyNodes(std::vector<LcneNodeInfo>& nodes)
{
    // httplib client for UDS: use path as host, set_address_family(AF_UNIX)
    httplib::Client client(udsPath_);
    client.set_address_family(AF_UNIX);

    auto res = client.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/nodes");
    if (!res) {
        IT_LOG_ERROR << "Failed to get topology nodes from LCNE";
        return UBSE_ERROR_DEF(2);
    }

    if (res->status != httplib::OK_200) {
        IT_LOG_ERROR << "LCNE returned status " << res->status;
        return UBSE_ERROR_DEF(3);
    }

    // Parse XML response
    const std::string& xml = res->body;
    nodes.clear();

    // Simple regex-based XML parsing (for mock data)
    // Pattern: <node>...<slot>1</slot>...<ubpu>1</ubpu>...<iou>1</iou>...
    // Use [\s\S] to match any character including newline
    std::regex nodeRegex("<node>([\\s\\S]*?)</node>");
    std::regex slotRegex("<slot>(\\d+)</slot>");
    std::regex ubpuRegex("<ubpu>(\\d+)</ubpu>");
    std::regex iouRegex("<iou>(\\d+)</iou>");
    std::regex portRegex("<physical-port>([\\s\\S]*?)</physical-port>");
    std::regex portIdRegex("<physical-port-id>(\\d+)</physical-port-id>");
    std::regex interfaceNameRegex("<interface-name>([^<]+)</interface-name>");
    std::regex portStatusRegex("<physical-port-status>(up|down)</physical-port-status>");
    std::regex remoteSlotRegex("<remote-slot>(\\d+|-)</remote-slot>");
    std::regex remoteUbpuRegex("<remote-ubpu>(\\d+|-)</remote-ubpu>");
    std::regex remoteIouRegex("<remote-iou>(\\d+|-)</remote-iou>");
    std::regex remotePortIdRegex("<remote-physical-port-id>(\\d+|-)</remote-physical-port-id>");

    std::sregex_iterator nodeIt(xml.begin(), xml.end(), nodeRegex);
    std::sregex_iterator nodeEnd;

    for (; nodeIt != nodeEnd; ++nodeIt) {
        std::string nodeContent = nodeIt->str(1);
        LcneNodeInfo nodeInfo;

        // Extract slot
        std::smatch slotMatch;
        if (std::regex_search(nodeContent, slotMatch, slotRegex)) {
            nodeInfo.slot = static_cast<uint32_t>(std::stoul(slotMatch.str(1)));
        }

        // Extract ubpu
        std::smatch ubpuMatch;
        if (std::regex_search(nodeContent, ubpuMatch, ubpuRegex)) {
            nodeInfo.ubpu = static_cast<uint32_t>(std::stoul(ubpuMatch.str(1)));
        }

        // Extract iou
        std::smatch iouMatch;
        if (std::regex_search(nodeContent, iouMatch, iouRegex)) {
            nodeInfo.iou = static_cast<uint32_t>(std::stoul(iouMatch.str(1)));
        }

        // Extract ports
        std::sregex_iterator portIt(nodeContent.begin(), nodeContent.end(), portRegex);
        std::sregex_iterator portEnd;

        for (; portIt != portEnd; ++portIt) {
            std::string portContent = portIt->str(1);
            LcnePortInfo portInfo;

            // Extract port ID
            std::smatch portIdMatch;
            if (std::regex_search(portContent, portIdMatch, portIdRegex)) {
                portInfo.portId = static_cast<uint32_t>(std::stoul(portIdMatch.str(1)));
            }

            // Extract interface name
            std::smatch interfaceNameMatch;
            if (std::regex_search(portContent, interfaceNameMatch, interfaceNameRegex)) {
                portInfo.interfaceName = interfaceNameMatch.str(1);
            }

            // Extract port status
            std::smatch portStatusMatch;
            if (std::regex_search(portContent, portStatusMatch, portStatusRegex)) {
                portInfo.status = portStatusMatch.str(1);
            }

            // Extract remote slot
            std::smatch remoteSlotMatch;
            if (std::regex_search(portContent, remoteSlotMatch, remoteSlotRegex)) {
                std::string value = remoteSlotMatch.str(1);
                if (value != "-") {
                    portInfo.remoteSlot = static_cast<uint32_t>(std::stoul(value));
                }
            }

            // Extract remote ubpu
            std::smatch remoteUbpuMatch;
            if (std::regex_search(portContent, remoteUbpuMatch, remoteUbpuRegex)) {
                std::string value = remoteUbpuMatch.str(1);
                if (value != "-") {
                    portInfo.remoteUbpu = static_cast<uint32_t>(std::stoul(value));
                }
            }

            // Extract remote iou
            std::smatch remoteIouMatch;
            if (std::regex_search(portContent, remoteIouMatch, remoteIouRegex)) {
                std::string value = remoteIouMatch.str(1);
                if (value != "-") {
                    portInfo.remoteIou = static_cast<uint32_t>(std::stoul(value));
                }
            }

            // Extract remote port ID
            std::smatch remotePortIdMatch;
            if (std::regex_search(portContent, remotePortIdMatch, remotePortIdRegex)) {
                std::string value = remotePortIdMatch.str(1);
                if (value != "-") {
                    portInfo.remotePortId = static_cast<uint32_t>(std::stoul(value));
                }
            }

            nodeInfo.ports.push_back(portInfo);
        }

        nodes.push_back(nodeInfo);
    }

    IT_LOG_INFO << "Parsed " << nodes.size() << " nodes from LCNE topology";
    return UBSE_OK;
}

UbseResult ItLcneClient::GetTopologyLinks(std::vector<LcneLinkInfo>& links)
{
    // First get topology nodes information
    std::vector<LcneNodeInfo> nodes;
    UbseResult ret = GetTopologyNodes(nodes);
    if (ret != UBSE_OK) {
        return ret;
    }

    links.clear();

    // Build lookup: (slot, ubpu, portId) -> interfaceName
    std::map<std::tuple<uint32_t, uint32_t, uint32_t>, std::string> interfaceMap;
    for (const auto& node : nodes) {
        for (const auto& port : node.ports) {
            interfaceMap[{node.slot, node.ubpu, port.portId}] = port.interfaceName;
        }
    }

    // Extract link information from nodes
    for (const auto& node : nodes) {
        for (const auto& port : node.ports) {
            // Only extract links with status "up"
            if (port.status == "up") {
                LcneLinkInfo link;
                link.localSlot = node.slot;
                link.localUbpu = node.ubpu;
                link.localPort = port.portId;
                link.localInterfaceName = port.interfaceName;
                link.remoteSlot = port.remoteSlot;
                link.remoteUbpu = port.remoteUbpu;
                link.remotePort = port.remotePortId;
                // Lookup remote interface name
                auto it = interfaceMap.find({port.remoteSlot, port.remoteUbpu, port.remotePortId});
                if (it != interfaceMap.end()) {
                    link.remoteInterfaceName = it->second;
                }
                links.push_back(link);
            }
        }
    }

    IT_LOG_INFO << "Parsed " << links.size() << " links from LCNE topology";
    return UBSE_OK;
}

UbseResult ItLcneClient::GetLogicEntities(std::vector<LcneLogicEntityInfo>& entities)
{
    httplib::Client client(udsPath_);
    client.set_address_family(AF_UNIX);

    auto res = client.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entities");
    if (!res) {
        IT_LOG_ERROR << "Failed to get logic-entities from LCNE";
        return UBSE_ERROR_DEF(2);
    }
    if (res->status != httplib::OK_200) {
        IT_LOG_ERROR << "LCNE logic-entities returned status " << res->status;
        return UBSE_ERROR_DEF(3);
    }

    const std::string& xml = res->body;
    entities.clear();

    std::regex entityRegex("<logic-entity>([\\s\\S]*?)</logic-entity>");
    std::regex eidRegex("<bus-instance-eid>([^<]+)</bus-instance-eid>");
    std::regex guidRegex("<guid>([^<]+)</guid>");
    std::regex typeRegex("<type>([^<]+)</type>");
    std::regex upiRegex("<upi>([^<]+)</upi>");
    std::regex stateRegex("<state>([^<]+)</state>");

    auto begin = std::sregex_iterator(xml.begin(), xml.end(), entityRegex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string content = it->str(1);
        LcneLogicEntityInfo info{};

        std::smatch match;
        if (std::regex_search(content, match, eidRegex)) {
            info.busInstanceEid = match.str(1);
        }
        if (std::regex_search(content, match, guidRegex)) {
            info.guid = match.str(1);
        }
        if (std::regex_search(content, match, typeRegex)) {
            info.type = match.str(1);
        }
        if (std::regex_search(content, match, upiRegex)) {
            info.upi = match.str(1);
        }
        if (std::regex_search(content, match, stateRegex)) {
            info.state = match.str(1);
        }

        entities.push_back(info);
    }

    IT_LOG_INFO << "Parsed " << entities.size() << " logic-entities from LCNE";
    return UBSE_OK;
}

UbseResult ItLcneClient::BuildUbpuToSocketIdMapping(std::vector<LcneLinkInfo>& links)
{
    // 1. Collect all Ubpu (chipId) from topology links, sort them
    std::set<uint32_t> ubpuSet;
    for (const auto& link : links) {
        ubpuSet.insert(link.localUbpu);
        ubpuSet.insert(link.remoteUbpu);
    }

    // 2. Collect all SocketId from system files, sort them
    // Similar to UbseNodeController::UbseSocketIdChange logic:
    // OS socketId comes from numaInfos (collected from system files)
    std::set<uint32_t> socketIdSet;
    UbseResult ret = CollectSocketIds(socketIdSet);
    if (ret != UBSE_OK || socketIdSet.empty()) {
        IT_LOG_ERROR << "Failed to collect socketIds from system files";
        return ret;
    }

    // 3. Build Ubpu -> SocketId mapping in sorted order
    // Similar to UbseNodeController::UbseSocketIdChange logic:
    // LCNE chipId -> OS socketId (sorted order)
    std::map<uint32_t, uint32_t> ubpuToSocketIdMap;
    auto it1 = ubpuSet.begin();
    auto it2 = socketIdSet.begin();
    for (size_t i = 0; i < ubpuSet.size() && i < socketIdSet.size(); i++) {
        ubpuToSocketIdMap[*it1] = *it2;
        ++it1;
        ++it2;
    }

    // 4. Apply mapping to LcneLinkInfo structures
    // Same as daemon SocketIdMapping: if chipId not found in mapping, use chipId as socketId
    for (auto& link : links) {
        auto it = ubpuToSocketIdMap.find(link.localUbpu);
        if (it != ubpuToSocketIdMap.end()) {
            link.localSocketId = it->second;
        } else {
            link.localSocketId = link.localUbpu;
        }

        it = ubpuToSocketIdMap.find(link.remoteUbpu);
        if (it != ubpuToSocketIdMap.end()) {
            link.remoteSocketId = it->second;
        } else {
            link.remoteSocketId = link.remoteUbpu;
        }
    }

    IT_LOG_INFO << "Built Ubpu to SocketId mapping for " << ubpuSet.size() << " Ubpu IDs and " << socketIdSet.size()
                << " SocketId IDs";
    return UBSE_OK;
}

} // namespace ubse::it::infra