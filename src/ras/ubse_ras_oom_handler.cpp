// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras_oom_handler.h"
#include <dlfcn.h>
#include <fstream>
#include <regex>
#include <variant>
#include "message/ubse_ras_oom_message.h"
#include "ubse_com_module.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"
#include "ubse_ras.h"
#include "ubse_ras_com_handler.h"
#include "src/framework/security/ubse_security_module.h"

using namespace ubse::election;
using namespace ubse::common;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::config;
using namespace ubse::security;

namespace ubse::ras {
UBSE_DEFINE_THIS_MODULE("ubse");

using NumaId = int64_t;
using SmapUrgentMigrateOutPtr = void (*)(uint64_t size);
SmapUrgentMigrateOutPtr smapUrgentMigrateOutFunc = nullptr;

static std::unordered_map<int, std::string> oomReasons{{0, "KSWAPD"}, {1, "DIRECT_RECLAIM"}, {2, "HUGEPAGE_RECLAIM"}};
const uint32_t MEM_INFO_INDEX = 3;
const uint32_t HUGEPAGE_INFO_INDEX = 0;
const uint32_t MAX_NUMA_NUM = 8;
const int FREQ_LIMIT = 10;
const uint32_t URGENT_SIZE = 128 * 1024 * 1024;
const uint32_t KB_SIZE = 1024;
const uint64_t TWO_MB_HUGE_PAGE = static_cast<uint64_t>(2048) * 1024;
const uint64_t LEAST_NEED_HUGE_PAGE = 512;
const uint64_t READ_FILE_SLEEP_TIME = 1;
const uint16_t HUGEPAGE_OOM = 2;
constexpr auto UBSE_ADMISSION_CONFIG_SECTION_NAME = "ubse_plugin_admission";
constexpr auto OCK_VM_ENABLE = "vm";
using LibPtr = void *;
constexpr int MAX_OOM_TIMEOUT_MS = 3600000;

std::vector<int> SplitNids(const std::string &nid_str)
{
    std::vector<int> nids;
    std::stringstream ss(nid_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            nids.push_back(std::stoi(item));
        } catch (const std::exception &e) {
            UBSE_LOG_ERROR << "Caught exception: " << e.what();
        }
    }
    return nids;
}

/*
 * eventMessage的形式为: "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2,"
"timesec:1741057335,timeusec:469389}",其中1650指的是msgid
 * */
UbseResult ParseEventMessage(const std::string &message,
                             std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> &messageValue)
{
    std::regex pattern(R"(^(\d+)_\{nr_nid:(\d+),nid:\[(-?\d+(?:,-?\d+)*)\],)"
                       R"(sync:(\d+),timeout:(\d+),reason:(\d+),timesec:(\d+),timeusec:(\d+)\})");
    std::smatch match;
    if (!std::regex_match(message, match, pattern)) {
        UBSE_LOG_ERROR << "The message format is invalid, event message is " << message;
        return UBSE_ERROR_INVAL;
    }
    auto valueIndex = NO_1;
    uint64_t msgId{};
    long timeSec{};
    long timeUsec{};
    int nrNid{};
    int sync{};
    int timeout{};
    int reason{};
    UbseResult convertRet = UBSE_OK;
    convertRet |= ubse::utils::ConvertStrToUint64(match[valueIndex].str(), msgId);
    messageValue["msgid"] = msgId; // 提取前面的msg_id（451）
    convertRet |= ubse::utils::ConvertStrToInt(match[++valueIndex].str(), nrNid);
    messageValue["nr_nid"] = nrNid;
    messageValue["nid"] = SplitNids(match[++valueIndex].str());
    convertRet |= ubse::utils::ConvertStrToInt(match[++valueIndex].str(), sync);
    messageValue["sync"] = sync;
    convertRet |= ubse::utils::ConvertStrToInt(match[++valueIndex].str(), timeout);
    messageValue["timeout"] = timeout;
    convertRet |= ubse::utils::ConvertStrToInt(match[++valueIndex].str(), reason);
    messageValue["reason"] = reason;
    convertRet |= ubse::utils::ConvertStrToLong(match[++valueIndex].str(), timeSec);
    messageValue["timesec"] = timeSec;
    convertRet |= ubse::utils::ConvertStrToLong(match[++valueIndex].str(), timeUsec);
    messageValue["timeusec"] = timeUsec;
    if (convertRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse event message";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult CheckCommonParam(std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> &messageValue,
                            const std::string &eventMessage)
{
    int oomReasonId = 0;
    try {
        oomReasonId = std::get<int>(messageValue.at("reason"));
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Caught exception: " << e.what();
        return UBSE_ERROR_INVAL;
    }
    if (oomReasons.find(oomReasonId) == oomReasons.end()) {
        UBSE_LOG_WARN << "This oom reason is invalid, message is " << eventMessage;
        return UBSE_ERROR_INVAL;
    }
    size_t nidsSize = 0;
    try {
        nidsSize = std::get<std::vector<int>>(messageValue.at("nid")).size();
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Caught exception: " << e.what();
        return UBSE_ERROR_INVAL;
    }
    if (nidsSize > MAX_NUMA_NUM) {
        UBSE_LOG_WARN << "The nidSize=" << nidsSize << " is invalid.";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

uint32_t ProcessSmallpageOom(std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> &messageValue,
                             uint64_t &nrFree)
{
    // 小页场景是为了以后预埋，目前就打日志
    UBSE_LOG_INFO << "Smallpage case is for future, just need ack.";
    return UBSE_OK;
}

/* 虚拟化场景,只需要一个空闲2M大页,并且OS保证单次报上来的oom的numa数量为1,sync恒为1
  OS保证同一个numa的oom在上报未得到ack之前，其他发生oom的虚机不上报消息，即OS内部做频率拦截
 */
UbseResult CheckHugePageOomParam(
    const std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> &messageValue)
{
    try {
        auto nr_nid = std::get<int>(messageValue.at("nr_nid"));
        auto nids = std::get<std::vector<int>>(messageValue.at("nid"));
        if (nr_nid != NO_1 || nids.empty()) {
            UBSE_LOG_ERROR << "Nr_nid is invalid, nr_nid is " << nr_nid << ", nidSize=" << nids.size();
            return UBSE_ERROR_INVAL;
        }
        auto sync = std::get<int>(messageValue.at("sync"));
        if (sync != NO_1) {
            UBSE_LOG_ERROR << "sync is " << sync << ", vm oom scene should be 1.";
            return UBSE_ERROR_INVAL;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Caught exception: " << e.what();
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

static std::vector<std::string> GetLineInfo(const std::string &line)
{
    std::vector<std::string> strs;
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
        strs.push_back(token);
    }
    return strs;
}

/*
 * 用于获取只有一行数据的文件里面的内容
 */
UbseResult GetFileValue(std::string &filePath, int16_t index, uint64_t &value)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        UBSE_LOG_WARN << "Cannot open file , file path is " << filePath;
        return UBSE_ERROR_IO;
    }
    std::string line;
    while (std::getline(file, line)) {
        auto strs = GetLineInfo(line);
        if (index >= strs.size()) {
            UBSE_LOG_ERROR << "Invalid index=" << index << ", expect smaller than " << strs.size();
            return UBSE_ERROR_INVAL;
        }
        try {
            value = stoul(strs[index]);
        } catch (...) {
            UBSE_LOG_WARN << "Get the file value failed, file path is " << filePath;
            return UBSE_ERROR_INVAL;
        }
    }
    return UBSE_OK;
}

UbseResult IsRemoteNuma(int16_t numaId, bool &isRemote)
{
    std::string numaRemoteAttrPath = "/sys/devices/system/node/node" + std::to_string(numaId) + "/remote";
    uint64_t remoteAttr = 0;
    auto ret = GetFileValue(numaRemoteAttrPath, 0, remoteAttr);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get numa remote attr failed, numa id is " << numaId;
        return ret;
    }
    isRemote = (remoteAttr == 1);
    return UBSE_OK;
}

UbseResult GetFreeHugepages(int16_t numaId, uint64_t &memFree)
{
    std::string numaHugepageFilePath =
        "/sys/devices/system/node/node" + std::to_string(numaId) + "/hugepages/hugepages-2048kB/free_hugepages";
    auto ret = GetFileValue(numaHugepageFilePath, 0, memFree);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get numa free hugepage failed, numaId=" << numaId;
        return ret;
    }
    return UBSE_OK;
}

UbseResult GetRemoteFreeHugepages(uint64_t &memFree)
{
    std::string allFreeHugepagePath = "/sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages";
    uint64_t allFreeHugepage = 0;
    auto ret = GetFileValue(allFreeHugepagePath, HUGEPAGE_INFO_INDEX, allFreeHugepage);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get all free huge pages failed";
        return ret;
    }
    uint64_t localFreeHugepage = 0;
    for (int i = 0; i < MAX_NUMA_NUM; i++) {
        bool isRemote = false;
        ret = IsRemoteNuma(i, isRemote);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Can not check the numa is remote numa or not, numaId=" << i;
            continue;
        }
        // os里面的本地numa和远端numa是顺序排列的，当监测到一个numa是远端numa之后，后续的大页信息就不需要获取了
        if (isRemote) {
            UBSE_LOG_DEBUG << "Stop to get the free hugepages, this numa is remote numa, remoteNumaId=" << i << ".";
            break;
        }
        uint64_t tmpHugepage = 0;
        ret = GetFreeHugepages(i, tmpHugepage);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Get free hugepage failed, numaId=" << i;
            return ret;
        }
        localFreeHugepage += tmpHugepage;
    }
    memFree = allFreeHugepage > localFreeHugepage ? (allFreeHugepage - localFreeHugepage) : 0;
    UBSE_LOG_DEBUG << "All free_hugepage=" << allFreeHugepage << ", local free_hugepage=" << localFreeHugepage
                   << ", remote free_hugepage=" << memFree;
    return UBSE_OK;
}

UbseResult ForwardOomEventToManager(uint64_t memNeed, NumaId oomNumaId)
{
    UbseRoleInfo curInfo;
    auto ret = UbseGetCurrentNodeInfo(curInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get current node info failed. ";
        return ret;
    }
    UbseRasOomMessagePtr request = new (std::nothrow) UbseRasOomMessage(memNeed, curInfo.nodeId, oomNumaId);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "Construct oom request failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseRasOomMessagePtr response = new (std::nothrow) UbseRasOomMessage();
    if (response == nullptr) {
        UBSE_LOG_ERROR << "Response is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseRoleInfo managerNode;
    ret = UbseGetMasterInfo(managerNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "The manager nodeid is invalid.";
        return ret;
    }
    UBSE_LOG_DEBUG << "Start to send oom message to manager, the oom numaId=" << oomNumaId;
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam{curInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::RAS),
                        static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_OOM)};
    ret = comModule->RpcSend(sendParam, request, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_DEBUG << managerNode.nodeId;
        UBSE_LOG_ERROR << "Get exception when forward oom event to manager, error is in hcom, " << FormatRetCode(ret);
        return ret;
    }
    return response->GetErrCode();
}

UbseResult InitOomHandler()
{
    std::vector<__u32> caps{CAP_DAC_OVERRIDE};
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    static constexpr auto obmmPath = "libubturbo_client.so";
    void *handle = dlopen(obmmPath, RTLD_NOW); // 生命周期与进程一致,进程结束后释放
    if (handle == nullptr) {
        UBSE_LOG_WARN << "Dlopen libubturbo_client.so failed, error is " << dlerror();
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        return UBSE_ERROR;
    }

    smapUrgentMigrateOutFunc =
        reinterpret_cast<SmapUrgentMigrateOutPtr>(dlsym(handle, "ubturbo_smap_urgent_migrate_out"));
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    if (smapUrgentMigrateOutFunc == nullptr) {
        UBSE_LOG_ERROR << "Dlopen SmapUrgentMigrateOut failed " << dlerror();
        dlclose(handle);
        return UBSE_ERROR;
    }
    auto ret = RegisterAlarmFaultHandler({ALARM_OOM_EVENT, "oom_handle", OomHandler});
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register alarm fault oom failed, " << FormatRetCode(ret);
        dlclose(handle);
        smapUrgentMigrateOutFunc = nullptr;
        return ret;
    }
    auto comModulePtr = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModulePtr == nullptr) {
        UBSE_LOG_ERROR << "Get com module failed. ";
        dlclose(handle);
        smapUrgentMigrateOutFunc = nullptr;
        return UBSE_ERROR_NULLPTR;
    }
    UbseComBaseMessageHandlerPtr ubseOomHandlerPtr = new (std::nothrow) UbseOomHandler();
    ret = comModulePtr->RegRpcService<UbseRasOomMessage, UbseRasOomMessage>(ubseOomHandlerPtr);
    if (ret != UBSE_OK) {
        dlclose(handle);
        smapUrgentMigrateOutFunc = nullptr;
        UBSE_LOG_ERROR << "Reg rpc service fail, " << FormatRetCode(ret);
    }
    return ret;
}

static long GetMillisecondsTime()
{
    auto now = std::chrono::system_clock::now();
    auto nowTimeMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return nowTimeMilliseconds;
}

UbseResult GetFreeMemInfo(int16_t numaId, uint64_t &memFree)
{
    std::string numaFilePath = "/sys/devices/system/node/node" + std::to_string(numaId) + "/meminfo";
    std::ifstream file(numaFilePath);
    if (!file.is_open()) {
        UBSE_LOG_WARN << "Can not open file , numa file path is " << numaFilePath;
        return UBSE_ERROR_IO;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("MemFree") == std::string::npos) {
            continue;
        }
        auto strs = GetLineInfo(line);
        if (MEM_INFO_INDEX >= strs.size()) {
            UBSE_LOG_ERROR << "Invalid index=" << MEM_INFO_INDEX << ", expect smaller than " << strs.size();
            return UBSE_ERROR_INVAL;
        }
        try {
            memFree = stoul(strs[MEM_INFO_INDEX]);
        } catch (...) {
            UBSE_LOG_WARN << "Get integer value from string failed, string=" << strs[MEM_INFO_INDEX];
            return UBSE_ERROR_INVAL;
        }
    }
    return UBSE_OK;
}

uint64_t InitOomWaitTime()
{
    uint64_t oomWaitTime;
    std::string oomWaitTimePath = "/sys/module/obmm/parameters/message_ack_timeout_ms";
    auto ret = GetFileValue(oomWaitTimePath, 0, oomWaitTime);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get the oom wait time failed, use the default wait time 100 ms.";
    }
    return oomWaitTime;
}

bool IsNumaMemFreeEnough(NumaId numaId, uint64_t startTime, uint64_t memNeed, uint64_t &memFree, int timeOut)
{
    auto currentTime = GetMillisecondsTime();
    UBSE_LOG_DEBUG << "[OOM] startTime=" << startTime << "ms, currentTime=" << currentTime
                   << "ms, oomWaitTime=" << timeOut << "ms.";
    if (startTime > currentTime || timeOut > MAX_OOM_TIMEOUT_MS) {
        UBSE_LOG_ERROR << "Invalid time parameters, startTime=" << startTime
                       << "ms, currentTime=" << currentTime << "ms, timeOut=" << timeOut << "ms.";
        return false;
    }
    while (currentTime - startTime <= timeOut) {
        UbseResult ret = UBSE_OK;
        std::string vmCode;
        if ((UbseGetStr(UBSE_ADMISSION_CONFIG_SECTION_NAME, OCK_VM_ENABLE, vmCode) == UBSE_OK)) {
            ret = GetFreeHugepages(numaId, memFree);
            memFree = memFree * TWO_MB_HUGE_PAGE;
        } else {
            // 未开启VM插件，不用轮询
            ret = GetFreeMemInfo(numaId, memFree);
            memFree = memFree * KB_SIZE;
            return ret;
        }
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Get mem free info failed, numaid=" << numaId << ", erroCode=" << ret;
            return false;
        }
        UBSE_LOG_DEBUG << "The memFree of numa=" << numaId << ", memFree=" << memFree << ", memNeed=" << memNeed;
        if (memFree > memNeed) {
            return true;
        }
        UBSE_LOG_INFO << "Mem is not enough, wait for 1s, memFree=" << memFree << ", memNeed=" << memNeed;
        currentTime = GetMillisecondsTime();
        sleep(READ_FILE_SLEEP_TIME);
    }
    return false;
}

UbseResult SmapUrgentMigrateOut(uint64_t memNeed)
{
    std::string vmCode;
    if (!(UbseGetStr(UBSE_ADMISSION_CONFIG_SECTION_NAME, OCK_VM_ENABLE, vmCode) == UBSE_OK)) {
        UBSE_LOG_INFO << "Smap not need enable.";
        return UBSE_OK;
    }
    if (smapUrgentMigrateOutFunc == nullptr) {
        UBSE_LOG_ERROR << "Smap urgent migrate out func is not initialized.";
        return UBSE_ERROR;
    }
    smapUrgentMigrateOutFunc(memNeed);
    UBSE_LOG_DEBUG << "Smap migrate out urgently success, migrate_size=" << memNeed;
    return UBSE_OK;
}

UbseResult GetOomNumaId(
    const std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> &messageValue, int &oomNumaId)
{
    try {
        auto nids = std::get<std::vector<int>>(messageValue.at("nid"));
        if (nids.empty()) {
            UBSE_LOG_ERROR << "Get empty numa ids";
            return UBSE_ERROR_INVAL;
        }
        oomNumaId = nids[NO_0];
        if (oomNumaId < 0) {
            UBSE_LOG_ERROR << "Invalid numa id=" << oomNumaId;
            return UBSE_ERROR_INVAL;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Caught exception: " << e.what();
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

uint32_t ProcessHugepageOom(
    const std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> &messageValue, uint64_t &nrFree)
{
    auto ret = CheckHugePageOomParam(messageValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "HugePage oom message is invalid.";
        return ret;
    }
    int oomNumaId;
    ret = GetOomNumaId(messageValue, oomNumaId);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto memNeed = TWO_MB_HUGE_PAGE;
    uint64_t memFreeHugePages = 0;
    ret = GetRemoteFreeHugepages(memFreeHugePages);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get remote free hugePages failed, oom numaid=" << oomNumaId;
        return ret;
    }
    UBSE_LOG_DEBUG << "The remote 2M memFreeHugePages=" << memFreeHugePages;
    if (memFreeHugePages < LEAST_NEED_HUGE_PAGE) {
        ret = ForwardOomEventToManager(memNeed, oomNumaId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Forward oom event to manager failed, erroCode=" << ret << ", oom_numaid=" << oomNumaId;
            return ret;
        }
    }
    ret = SmapUrgentMigrateOut(URGENT_SIZE);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Urgent smap out failed, migrate_size=" << URGENT_SIZE << ", errorCode=" << ret;
    }
    // 4、扫描内存的剩余量
    uint64_t memFree = 0;
    bool isEnough = false;
    try {
        auto timeOut = std::get<int>(messageValue.at("timeout"));
        auto timeSec = std::get<long>(messageValue.at("timesec"));
        auto timeUsec = std::get<long>(messageValue.at("timeusec"));
        uint64_t startTime = static_cast<uint64_t>(timeSec) * NO_1000 + timeUsec / NO_1000;
        isEnough = IsNumaMemFreeEnough(oomNumaId, startTime, memNeed, memFree, timeOut);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Caught exception: " << e.what();
        return UBSE_OK; // 已经成功给virt发布事件，返回成功
    }
    if (isEnough) {
        nrFree = memFree / TWO_MB_HUGE_PAGE;
        UBSE_LOG_INFO << "memFree=" << memFree << " bytes, 2M hugePages=" << nrFree;
    }
    UBSE_LOG_INFO << "Process oom event success.";
    return UBSE_OK;
}

/*
 * faultInfo: "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2,"
"timesec:1741057335,timeusec:469389}",其中1650指的是msgid
 * */
UbseResult OomHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOG_INFO << "Start to process oom event, alarmFaultEvent=" << static_cast<int>(alarmFaultEvent)
                  << ", info=" << faultInfo;
    std::map<std::string, std::variant<uint64_t, long, int, std::vector<int>>> messageValue;
    auto ret = ParseEventMessage(faultInfo, messageValue);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Parse event message failed, the event message is " << faultInfo;
        return ret;
    }
    ret = CheckCommonParam(messageValue, faultInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Param is invalid, eventMessage is " << faultInfo;
        return UBSE_OK;
    }
    auto oomReasonId = std::get<int>(messageValue.at("reason"));
    uint64_t nrFree = 0;
    if (oomReasonId != HUGEPAGE_OOM) {
        ret = ProcessSmallpageOom(messageValue, nrFree);
    } else {
        ret = ProcessHugepageOom(messageValue, nrFree);
    }
    UBSE_LOG_INFO << "Process oom event end, nrFree=" << nrFree;
    // 处理只打印错误日志进行便于定位，在任何时候都需要进行ack
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Process oom event failed, event message is " << faultInfo << ", " << FormatRetCode(ret);
    }
    return ret;
}
} // namespace ubse::ras