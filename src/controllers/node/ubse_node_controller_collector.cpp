/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_collector.h"

#include <unistd.h>
#include <fstream>
#include <regex>

#include "securec.h"

#include "ubse_com.h"
#include "ubse_memory_interface.h"
#include "ubse_net_util.h"
#include "ubse_node_controller_util.h"
#include "ubse_str_util.h"

namespace ubse::nodeController {
using namespace ubse::com;
using namespace ubse::utils;
static std::regex g_rgx("(\\d+)");
const uint8_t IPV4_LENGTH = 4;

static std::string FIB_TRIE_PATH = "/proc/net/fib_trie";
static std::string CPU_LIST_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string CPU_LIST_SUFFIX_PATH = "/cpulist";
static std::string SOCKET_ID_PREFIX_PATH = "/sys/devices/system/cpu/cpu";
static std::string SOCKET_ID_SUFFIX_PATH = "/topology/physical_package_id";
static std::string MEM_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string MEM_SUFFIX_PATH = "/meminfo";
static std::string NR_PAGE_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string NR_PAGE_SUFFIX_PATH = "/hugepages/hugepages-2048kB/nr_hugepages";
static std::string FREE_PAGE_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string FREE_PAGE_SUFFIX_PATH = "/hugepages/hugepages-2048kB/free_hugepages";
static std::string NUMA_PATH = "/sys/devices/system/node/has_cpu";

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_NODE_CONTROLLER_MID)

UbseResult CollectNodeBaseInfo(UbseNodeInfo &ubseNodeInfo)
{
    GetCurNodeInfo(ubseNodeInfo);
    char hostname[UBSE_HOST_NAME_MAX_LEN + 1];
    auto ret = gethostname(hostname, UBSE_HOST_NAME_MAX_LEN + 1);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "get hostname failed, ErrorCode=" << ret;
        return ret;
    }
    ubseNodeInfo.hostName = hostname;
    return UBSE_OK;
}

UbseResult CollectNodeTopology(UbseNodeInfo &ubseNodeInfo)
{
    auto ret = CollectIpList(ubseNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "collect ip list failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = CollectNumaInfo(ubseNodeInfo, ubseNodeInfo.nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "collect numa failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = CollectCpuInfo(ubseNodeInfo, ubseNodeInfo.nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "collect cpu failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult ParseIP(const std::string &ip, UbseIpV4Addr &ipv4)
{
    std::smatch match;
    auto it = ip.begin();
    uint32_t count = 0;
    while (std::regex_search(it, ip.end(), match, g_rgx)) {
        std::string part = match.str(1);
        int num = 0;
        try {
            num = std::stoi(part);
        } catch (const std::invalid_argument &) {
            UBSE_LOG_ERROR << "addr is invalid argument, value=" << part;
            return UBSE_ERROR_INVAL;
        } catch (const std::out_of_range &) {
            UBSE_LOG_ERROR << "addr out of int range, value=" << part;
            return UBSE_ERROR_INVAL;
        }
        if (num < 0 || num > 255) { // 0 - 255 是ip单项值的范围
            UBSE_LOG_ERROR << "addr out of range from 0 to 255, curr addr=" << num;
            return UBSE_ERROR_INVAL;
        }
        ipv4.addr[count] = num;
        count++;
        it = match.suffix().first;
    }
    // 确保匹配到正好 4 个部分
    if (count != IPV4_LENGTH) {
        UBSE_LOG_ERROR << "analyze ubse ipv4 addr not valid";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

bool IsSpecialIP(const std::string &ip)
{
    // 排除特殊 IP 地址：0.0.0.0, 127.x.x.x, 169.254.x.x
    return std::regex_match(ip, std::regex(R"(^(0\.0\.0\.0|127\..*|169\.254\..*)$)"));
}

uint32_t UbseNodeTelemetryGetIpInfo(std::vector<std::string> &ipInfos)
{
    std::ifstream file(FIB_TRIE_PATH);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "Error opening file /proc/net/fib_trie";
        return UBSE_ERROR;
    }

    std::string line;
    std::unordered_set<std::string> uniqueIPs;
    std::regex ipRegex(R"(\b(\d+\.\d+\.\d+\.\d+)\b)"); // 匹配 IP 地址的正则表达式

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, ipRegex)) {
            std::string ip = match.str(1); // 正则匹配第2项
            if (!IsSpecialIP(ip)) {        // 排除特殊 IP 地址
                uniqueIPs.insert(ip);
            }
        }
    }
    file.close();
    ipInfos.insert(ipInfos.end(), uniqueIPs.begin(), uniqueIPs.end());
    return UBSE_OK;
}

UbseResult CollectIpList(UbseNodeInfo &ubseNodeInfo)
{
    std::vector<std::string> ipInfos{};
    auto ret = UbseNodeTelemetryGetIpInfo(ipInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "collect ip list failed, " << FormatRetCode(ret);
        return ret;
    }
    ubseNodeInfo.ipList.resize(ipInfos.size());
    for (size_t i = 0; i < ubseNodeInfo.ipList.size(); i++) {
        if (!UbseNetUtil::Ipv4StringToArr(ipInfos[i], ubseNodeInfo.ipList[i].ipv4.addr)) {
            UBSE_LOG_WARN << "parse ip list failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

template <class T>
uint32_t ProcessListLine(const std::string &line, T &idList)
{
    std::string token;
    std::stringstream ss(line);

    while (std::getline(ss, token, ',')) {
        size_t dashPos = token.find('-');
        if (dashPos == std::string::npos) {
            // 单个CPU编号
            try {
                int id = std::stoi(token);
                idList.push_back(id);
            } catch (const std::exception &e) {
                UBSE_LOG_ERROR << "get cpu id failed, " << e.what();
                return UBSE_ERROR;
            }
        } else {
            // CPU范围（例如 "0-3"）
            int start;
            int end;
            try {
                start = std::stoi(token.substr(0, dashPos));
                end = std::stoi(token.substr(dashPos + 1));
            } catch (const std::exception &e) {
                UBSE_LOG_ERROR << "Failed to get cpu id:" << e.what();
                return UBSE_ERROR;
            }
            if (start > end) {
                int temp = start;
                start = end;
                end = temp;
            }
            for (int id = start; id <= end; ++id) {
                idList.push_back(id);
            }
        }
    }

    return UBSE_OK;
}

UbseResult CollectCpuList(uint32_t numa, std::vector<uint16_t> &cpuList)
{
    const std::string cpuListPath = CPU_LIST_PREFIX_PATH + std::to_string(numa) + CPU_LIST_SUFFIX_PATH;
    std::ifstream inputFile(cpuListPath);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open cpuList failed, " << cpuListPath;
        return UBSE_ERROR;
    }
    std::string line;
    getline(inputFile, line);
    auto ret = ProcessListLine(line, cpuList);
    if (ret != UBSE_OK || cpuList.empty()) {
        UBSE_LOG_ERROR << "processListLine in GetCpuIdsOfNode failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult GetSocketId(uint32_t &socketId, uint16_t cpuId)
{
    const std::string socketIdPathTemp = SOCKET_ID_PREFIX_PATH + std::to_string(cpuId) + SOCKET_ID_SUFFIX_PATH;
    std::ifstream inputFile(socketIdPathTemp);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open physical_package_id failed";
        return UBSE_ERROR;
    }
    inputFile >> socketId;
    return UBSE_OK;
}

uint32_t CollectMemSize(uint32_t numa, uint64_t &memSize, uint64_t &memFree)
{
    std::string memInfoPathTemp = MEM_PREFIX_PATH + std::to_string(numa) + MEM_SUFFIX_PATH;
    std::ifstream inputFile(memInfoPathTemp);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open meminfo failed";
        return UBSE_ERROR;
    }
    const std::regex memInfoRegex(R"(Node\s+(\d+)\s+(\w+):\s+(\d+)\s*kB)");
    std::string line;

    std::smatch matches;
    uint32_t nameIndex = 2;
    uint32_t valueIndex = 3;
    while (getline(inputFile, line)) {
        regex_match(line, matches, memInfoRegex);
        if (matches.size() < 4) { // 匹配到少于4个说明这行信息有问题
            UBSE_LOG_WARN << "Invalid numa node size info line";
            continue;
        }
        if (matches[nameIndex].str() == "MemTotal") {
            memSize = stoull(matches[valueIndex].str());
        }
        if (matches[nameIndex].str() == "MemFree") {
            memFree = stoul(matches[valueIndex].str());
        }
    }
    return UBSE_OK;
}

UbseResult CollectNrHugePages2048(uint32_t numa, uint32_t &nrHugePages2048)
{
    const std::string nrHugePagesTemp = NR_PAGE_PREFIX_PATH + std::to_string(numa) + NR_PAGE_SUFFIX_PATH;
    std::ifstream inputFile(nrHugePagesTemp);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open nr_hugepages failed";
        return UBSE_ERROR;
    }
    int tempHugePages2048;
    inputFile >> tempHugePages2048;
    if (tempHugePages2048 < 0) {
        UBSE_LOG_ERROR << "error content in " << nrHugePagesTemp << " value=" << tempHugePages2048;
        return UBSE_ERROR;
    }
    nrHugePages2048 = tempHugePages2048;
    return UBSE_OK;
}

UbseResult CollectFreeHugePages2048(uint32_t numa, uint32_t &freeHugePages2048)
{
    const std::string freeHugePagesTemp = FREE_PAGE_PREFIX_PATH + std::to_string(numa) + FREE_PAGE_SUFFIX_PATH;
    std::ifstream inputFile(freeHugePagesTemp);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open free_hugepages failed";
        return UBSE_ERROR;
    }
    int tempHugePages2048;
    inputFile >> tempHugePages2048;
    if (tempHugePages2048 < 0) {
        UBSE_LOG_ERROR << "error content in " << freeHugePagesTemp << " value=" << tempHugePages2048;
        return UBSE_ERROR;
    }
    freeHugePages2048 = tempHugePages2048;
    return UBSE_OK;
}

uint32_t GetLocalNumas(std::vector<uint32_t> &nodeIds)
{
    const std::string hasCpuPath = NUMA_PATH;
    std::ifstream inputFile(hasCpuPath);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "Failed to open" << hasCpuPath;
        return UBSE_ERROR;
    }
    std::string line;
    getline(inputFile, line);
    auto ret = ProcessListLine(line, nodeIds);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to ProcessListLine in GetLocalNodes";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult CollectNumaInfo(UbseNodeInfo &ubseNodeInfo, const std::string &nodeId)
{
    std::vector<uint32_t> numas{};
    auto ret = GetLocalNumas(numas);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "collect numa list failed, " << FormatRetCode(ret);
        return ret;
    }
    for (auto numa : numas) {
        UbseNumaInfo info{};
        UbseNumaLocation location{nodeId, numa};
        info.location = location;
        ret = CollectCpuList(numa, info.bindCore);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect cpu list failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = GetSocketId(info.socketId, info.bindCore[0]);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect socket id failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = CollectMemSize(numa, info.size, info.freeSize);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect mem size failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = CollectNrHugePages2048(numa, info.nr_hugepages_2M);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect nr hugepages failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = CollectFreeHugePages2048(numa, info.free_hugepages_2M);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect free hugepages failed, " << FormatRetCode(ret);
            return ret;
        }
        info.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        ubseNodeInfo.numaInfos[location] = info;
    }
    return UBSE_OK;
}

void AddEdgeInfo(std::pair<const UbseDevPortName, mti::UbsePortInfo> &port, UbsePortInfo &portInfo)
{
    portInfo.portId = port.second.portId;
    portInfo.ifName = port.second.ifName;
    portInfo.portRole = port.second.portRole;
    portInfo.portStatus = static_cast<PortStatus>(port.second.portStatus);
    portInfo.portCna = port.second.portCna;
    portInfo.remoteSlotId = port.second.remoteSlotId;
    portInfo.remoteChipId = port.second.remoteChipId;
    portInfo.remoteCardId = port.second.remoteCardId;
    portInfo.remoteIfName = port.second.remoteIfName;
    portInfo.remotePortId = port.second.remotePortId;
}

UbseResult CollectCpuInfo(UbseNodeInfo &ubseNodeInfo, const std::string &nodeId)
{
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    DevTopology devTopology{};
    auto ret = module->UbseGetDevTopology(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] get topology info not successful, ret: " << FormatRetCode(ret);
        return ret;
    }
    std::map<DevName, UbseLcneSocketInfo> allSocketComEid = module->GetAllSocketComEid();
    std::map<DevName, UbseLcneIODieInfo> localBoardIOInfo = module->GetLocalBoardIOInfo();
    for (auto dev : devTopology) {
        std::string devNodeId, socketId;
        dev.first.SplitDevName(devNodeId, socketId);
        if (devNodeId != nodeId) {
            continue;
        }
        UbseCpuInfo info{};
        ConvertStrToUint32(dev.second.first.slotId, info.slotId);
        ConvertStrToUint32(socketId, info.socketId);
        UbseCpuLocation location{nodeId, info.socketId};
        auto cpyRet = strcpy_s(info.primaryEid, sizeof(info.primaryEid), allSocketComEid[dev.first].primaryEid.c_str());
        if (cpyRet != EOK) {
            UBSE_LOG_ERROR << "copy primaryEid failed, ErrorCode=" << cpyRet;
            return cpyRet;
        }
        info.chipId = dev.second.first.chipId;
        info.cardId = dev.second.first.cardId;
        info.busNodeCna = dev.second.first.busNodeCna;
        info.eid = localBoardIOInfo[dev.first].ubControllerEid; // LCNE获取时能保证key存在
        info.guid = localBoardIOInfo[dev.first].guid;           // LCNE获取时能保证key存在
        for (auto port : dev.second.second) {
            UbsePortInfo portInfo{};
            portInfo.urmaEid = allSocketComEid[dev.first].portEidList[port.second.portId].urmaEid;
            AddEdgeInfo(port, portInfo);
            info.portInfos[portInfo.portId] = portInfo;
        }
        ubseNodeInfo.cpuInfos[location] = info;
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController