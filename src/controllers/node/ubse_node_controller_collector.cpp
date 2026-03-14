/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_collector.h"

#include <unistd.h>
#include <fstream>
#include <regex>
#include <sys/stat.h>
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "securec.h"

#include "ubse_conf_module.h"
#include "ubse_com.h"
#include "ubse_const_def.h"
#include "ubse_net_util.h"
#include "ubse_node_controller_util.h"
#include "ubse_str_util.h"
#include "ubse_os_util.h"
#include "sentry_observer.h"
#include "ubse_mem_configuration.h"
#include "ubse_security_module.h"

namespace ubse::nodeController {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::config;
using namespace mem::strategy;
using namespace ubse::security;
UBSE_DEFINE_THIS_MODULE("ubse");
static std::regex g_rgx("(\\d+)");
const uint8_t IPV4_LENGTH = 4;
const std::string IS_LENDER_SECTION = "ubse.memory";
const std::string IS_LENDER_KEY = "isLender";

static std::string FIB_TRIE_PATH = "/proc/net/fib_trie";
static std::string CPU_LIST_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string CPU_LIST_SUFFIX_PATH = "/cpulist";
static std::string SOCKET_ID_PREFIX_PATH = "/sys/devices/system/cpu/cpu";
static std::string SOCKET_ID_SUFFIX_PATH = "/topology/physical_package_id";
static std::string MEM_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string MEM_SUFFIX_PATH = "/meminfo";
static std::string NR_PAGE_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string NR_PAGE_SUFFIX_PATH_1 = "/hugepages/hugepages-";
static std::string NR_PAGE_SUFFIX_PATH_2 = "kB/nr_hugepages";
static std::string FREE_PAGE_PREFIX_PATH = "/sys/devices/system/node/node";
static std::string FREE_PAGE_SUFFIX_PATH_1 = "/hugepages/hugepages-";
static std::string FREE_PAGE_SUFFIX_PATH_2 = "kB/free_hugepages";
static std::string NUMA_PATH = "/sys/devices/system/node/has_cpu";
static std::string OBMM_POOL_PATH = "/sys/kernel/obmm_mempool/obmm-";

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
    auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "conf module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    ret = confModule->GetConf<bool>(IS_LENDER_SECTION, IS_LENDER_KEY, ubseNodeInfo.isLender);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "get " << IS_LENDER_KEY << " from config failed, err code: " << ret
                    << ", will use default value: true";
        ubseNodeInfo.isLender = true;
    }
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
        if (count >= IPV4_LENGTH) {
            UBSE_LOG_ERROR << "too many addr parts";
            return UBSE_ERROR_INVAL;
        }
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

UbseResult CollectIpList(UbseNodeInfo &ubseNodeInfo)
{
    std::vector<std::string> ipInfos{};
    auto ret = UbseNetUtil::GetIpInfo(ipInfos);
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
            for (int id = start; id <= end && id >= start; ++id) {
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
    uint32_t expectedMatchSize = 4;
    while (getline(inputFile, line)) {
        regex_match(line, matches, memInfoRegex);
        if (matches.size() < expectedMatchSize) {
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

UbseResult CollectNrHugePages(uint32_t numa, uint32_t &nrHugePages, uint32_t hugePagesType)
{
    const std::string nrHugePagesTemp = NR_PAGE_PREFIX_PATH + std::to_string(numa) + NR_PAGE_SUFFIX_PATH_1 +
                                        std::to_string(hugePagesType) + NR_PAGE_SUFFIX_PATH_2;
    std::ifstream inputFile(nrHugePagesTemp);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open nr_hugepages failed";
        return UBSE_ERROR;
    }
    int tempHugePages;
    inputFile >> tempHugePages;
    if (tempHugePages < 0) {
        UBSE_LOG_ERROR << "error content in " << nrHugePagesTemp << " value=" << tempHugePages;
        return UBSE_ERROR;
    }
    nrHugePages = tempHugePages;
    return UBSE_OK;
}

UbseResult CollectFreeHugePages(uint32_t numa, uint32_t &freeHugePages, uint32_t hugePagesType)
{
    const std::string freeHugePagesTemp = FREE_PAGE_PREFIX_PATH + std::to_string(numa) + FREE_PAGE_SUFFIX_PATH_1 +
                                          std::to_string(hugePagesType) + FREE_PAGE_SUFFIX_PATH_2;
    std::ifstream inputFile(freeHugePagesTemp);
    if (!inputFile.is_open()) {
        UBSE_LOG_ERROR << "open free_hugepages failed";
        return UBSE_ERROR;
    }
    int tempHugePages;
    inputFile >> tempHugePages;
    if (tempHugePages < 0) {
        UBSE_LOG_ERROR << "error content in " << freeHugePagesTemp << " value=" << tempHugePages;
        return UBSE_ERROR;
    }
    freeHugePages = tempHugePages;
    return UBSE_OK;
}

UbseResult CollectObmmPoolFile(const std::string &filePath, std::string &outPut)
{
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        UBSE_LOG_WARN << "open " << filePath << " failed";
        return UBSE_ERROR;
    }
    if (!std::getline(inputFile, outPut)) {
        UBSE_LOG_WARN << "open " << filePath << " get info is empty";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void CollectObmmMemPoolInfo(const uint32_t numa, uint64_t &total, uint64_t &used, uint64_t &available_cleared,
                            uint64_t &available_uncleared)
{
    total = 0;
    used = 0;
    available_cleared = 0;
    available_uncleared = 0;

    const std::string totalPath = OBMM_POOL_PATH + std::to_string(numa) + "/total";
    const std::string usedPath = OBMM_POOL_PATH + std::to_string(numa) + "/used";
    const std::string availableClearedPath = OBMM_POOL_PATH + std::to_string(numa) + "/available_cleared";
    const std::string availableunClearedPath = OBMM_POOL_PATH + std::to_string(numa) + "/available_uncleared";

    std::string totalStr;
    if (auto ret = CollectObmmPoolFile(totalPath, totalStr); ret != UBSE_OK) {
        return;
    }
    std::string usedStr;
    if (auto ret = CollectObmmPoolFile(usedPath, usedStr); ret != UBSE_OK) {
        return;
    }
    std::string availableClearedStr;
    if (auto ret = CollectObmmPoolFile(availableClearedPath, availableClearedStr); ret != UBSE_OK) {
        return;
    }
    std::string availableunClearedStr;
    if (auto ret = CollectObmmPoolFile(availableunClearedPath, availableunClearedStr); ret != UBSE_OK) {
        return;
    }
    try {
        total = std::stoull(totalStr, nullptr, 0);
        used = std::stoull(usedStr, nullptr, 0);
        available_cleared = std::stoull(availableClearedStr, nullptr, 0);
        available_uncleared = std::stoull(availableunClearedStr, nullptr, 0);
    } catch (const std::invalid_argument& e) {
        UBSE_LOG_ERROR << "Invalid para: " <<e.what();
        return;
    } catch (const std::out_of_range& e) {
        UBSE_LOG_ERROR << "Out of range: " <<e.what();
        return;
    }
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

UbseResult CollectHugePagesInfo(UbseNumaInfo &info, uint32_t numa)
{
    constexpr uint32_t HUGE_PAGE_2M = 2048;
    auto ret = CollectNrHugePages(numa, info.nr_hugepages_2M, HUGE_PAGE_2M);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "numa=" << numa << " collect nr hugepages 2M failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = CollectFreeHugePages(numa, info.free_hugepages_2M, HUGE_PAGE_2M);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "numa=" << numa << " collect free hugepages 2M failed, " << FormatRetCode(ret);
        return ret;
    }
    std::string osPageSize;
    ret = GetUbseConf("os", "page_size", osPageSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get environment page size type failed, Use default value 4096";
        osPageSize = PAGE_SIZE_4K;
    }
    if (osPageSize == PAGE_SIZE_64K) {
        constexpr uint32_t HUGE_PAGE_512M = 524288;
        ret = CollectNrHugePages(numa, info.nr_hugepages_512M, HUGE_PAGE_512M);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect nr hugepages 512M failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = CollectFreeHugePages(numa, info.free_hugepages_512M, HUGE_PAGE_512M);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect free hugepages 512M failed, " << FormatRetCode(ret);
            return ret;
        }
    } else {
        constexpr uint32_t HUGE_PAGE_1G = 1048576;
        ret = CollectNrHugePages(numa, info.nr_hugepages_1G, HUGE_PAGE_1G);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect nr hugepages 1G failed, " << FormatRetCode(ret);
            return ret;
        }
        ret = CollectFreeHugePages(numa, info.free_hugepages_1G, HUGE_PAGE_1G);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "numa=" << numa << " collect free hugepages 1G failed, " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult CollectBasicInfo(UbseNumaInfo &info, uint32_t numa)
{
    auto ret = CollectCpuList(numa, info.bindCore);
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
    return UBSE_OK;
}

UbseResult CollectSingleNumaInfo(UbseNumaInfo &info, const std::string &nodeId, uint32_t numa)
{
    info.location = {nodeId, numa};

    auto ret = CollectBasicInfo(info, numa);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = CollectHugePagesInfo(info, numa);
    if (ret != UBSE_OK) {
        return ret;
    }
    CollectObmmMemPoolInfo(numa, info.mempool_total, info.mempool_used, info.mempool_available_cleared,
                           info.mempool_available_uncleared);
    info.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
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

    std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    for (auto numa : numas) {
        UbseNumaInfo info{};
        ret = CollectSingleNumaInfo(info, nodeId, numa);
        if (ret != UBSE_OK) {
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
            return ret;
        }
        ubseNodeInfo.numaInfos[info.location] = info;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    return UBSE_OK;
}

void AddEdgeInfo(
    std::pair<const adapter_plugins::mti::UbseDevPortName, adapter_plugins::mti::UbseMtiCpuTopoPortInfo>& port,
    UbsePortInfo& portInfo)
{
    portInfo.portId = port.second.portId;
    portInfo.ifName = port.second.ifName;
    portInfo.portRole = port.second.portRole;
    portInfo.portStatus = static_cast<PortStatus>(port.second.portStatus);
    portInfo.portCna = port.second.portCna;
    portInfo.urmaEid = port.second.urmaEid;
    portInfo.remoteSlotId = port.second.remoteSlotId;
    portInfo.remoteChipId = port.second.remoteChipId;
    portInfo.remoteCardId = port.second.remoteCardId;
    portInfo.remoteIfName = port.second.remoteIfName;
    portInfo.remotePortId = port.second.remotePortId;
}

UbseResult CollectCpuInfo(UbseNodeInfo &ubseNodeInfo, const std::string &nodeId)
{
    adapter_plugins::mti::UbseDevTopology devTopology{};
    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfosGroupByDevName{};
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetClusterCpuTopo(cpuTopoInfosGroupByDevName);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] get cpuTopoInfo not successful, ret: " << FormatRetCode(ret);
        return ret;
    }
    for (auto& [devName, cpuTopoInfo] : cpuTopoInfosGroupByDevName) {
        std::string devNodeId, socketId;
        devName.SplitDevName(devNodeId, socketId);
        if (devNodeId != nodeId) {
            continue;
        }
        UbseCpuInfo info{};
        info.slotId = cpuTopoInfo.slotId;
        info.socketId = cpuTopoInfo.socketId;
        UbseCpuLocation location{nodeId, info.socketId};
        auto cpyRet = strcpy_s(info.primaryEid, sizeof(info.primaryEid), cpuTopoInfo.primaryEid.c_str());
        if (cpyRet != EOK) {
            UBSE_LOG_ERROR << "copy primaryEid failed, ErrorCode=" << cpyRet;
            return cpyRet;
        }
        info.chipId = cpuTopoInfo.chipId;
        info.cardId = cpuTopoInfo.cardId;
        info.busNodeCna = cpuTopoInfo.busNodeCna;
        info.eid = cpuTopoInfo.eid;  // LCNE获取时能保证key存在
        info.guid = cpuTopoInfo.guid;  // LCNE获取时能保证key存在
        for (auto& port : cpuTopoInfo.portInfos) {
            nodeController::UbsePortInfo portInfo{};
            AddEdgeInfo(port, portInfo);
            info.portInfos[portInfo.portId] = portInfo;
        }
        ubseNodeInfo.cpuInfos[location] = info;
    }
    return UBSE_OK;
}

UbseResult CollectSysSentryState(UbseNodeInfo &ubseNodeInfo)
{
    std::string command = "sentryctl status sentry_msg_monitor 2>/dev/null";
    std::string result;
    auto ret = ubse::utils::UbseOsUtil::Exec(command, result);
    // sysSentry 服务未启动时也会返回错误，因此命令执行失败后视作sysSentry服务异常
    if (ret != UBSE_OK || result.find("RUNNING") == std::string::npos ||
        !syssentry::UbseRasObserver::GetInstance().IsConfigSuccess()) {
        ubseNodeInfo.sysSentryState = UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_NOK;
    } else {
        ubseNodeInfo.sysSentryState = UbseNodeSysSentryState::UBSE_NODE_SYSSENTRY_OK;
    }
    return UBSE_OK;
}

UbseResult CollectObmmKernelState(UbseNodeInfo &ubseNodeInfo)
{
    std::string path = "/sys/module/obmm";
    struct stat info;
    bool isModuleLoaded = stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
    ubseNodeInfo.obmmState = isModuleLoaded ? UbseNodeObmmState::UBSE_NODE_OBMM_INSERTED :
                                              UbseNodeObmmState::UBSE_NODE_OBMM_NOT_INSERTED;
    return UBSE_OK;
}
} // namespace ubse::nodeController