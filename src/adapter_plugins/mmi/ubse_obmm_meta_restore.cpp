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
#include "ubse_obmm_meta_restore.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_file_util.h"
#include "ubse_logger.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_mmi_def.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_utils.h"
#include "ubse_security_module.h"
#include "fcntl.h"
#include "securec.h"
#include "unistd.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::common::def;
using namespace ubse::election;
using namespace ubse::security;
constexpr auto OBMM_META_PATH = "/sys/devices/obmm/";
constexpr auto OBMM_DEV_PATH = "/dev/";
std::vector<__u32> caps = {CAP_DAC_OVERRIDE};

using FileToPidsMap = std::unordered_map<std::string, std::vector<uint64_t>>;
// 安全读取 symlink，成功返回 true 并填充 target，失败返回 false
bool SafeReadSymlink(const std::filesystem::path& p, std::string& target)
{
    try {
        target = std::filesystem::read_symlink(p).string();
        return true;
    } catch (...) {
        // 忽略所有异常：权限不足、文件已删除、非 symlink 等
        return false;
    }
}

// 安全获取目录条目（返回 vector，失败则为空）
std::vector<std::filesystem::directory_entry> SafeDirectoryEntries(const std::filesystem::path& dirPath)
{
    try {
        return {std::filesystem::directory_iterator(dirPath), std::filesystem::directory_iterator()};
    } catch (...) {
        return {};
    }
}

bool IsSymlink(const std::filesystem::path& path)
{
    try {
        return std::filesystem::is_symlink(path);
    } catch (...) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "Failed to check symlink for path=" << path.string();
        return false;
    }
}

// 一次性扫描 /proc，构建 {file_path -> [pids]} 映射
FileToPidsMap BuildFileToPidsMap(const std::unordered_set<std::string>& targetPaths)
{
    FileToPidsMap result;
    const std::filesystem::path procPath("/proc");

    for (const auto& procEntry : SafeDirectoryEntries(procPath)) {
        if (!procEntry.is_directory())
            continue;

        const std::string pidStr = procEntry.path().filename().string();
        uint64_t pid = 0;
        if (!RmCommonUtils::GetInstance().StrToULong(pidStr, pid))
            continue;

        std::filesystem::path fdDir = procEntry.path() / "fd";
        for (const auto& fdEntry : SafeDirectoryEntries(fdDir)) {
            if (!IsSymlink(fdEntry.path()))
                continue;

            std::string target;
            if (!SafeReadSymlink(fdEntry.path(), target)) {
                continue; // 读取失败，跳过
            }

            if (targetPaths.find(target) != targetPaths.end()) {
                result[target].push_back(pid);
            }
        }
    }
    return result;
}

// 批量查询接口（替代原来的 GetUsedPidCount）
void BatchGetUsedPidCount(const std::unordered_set<std::string>& targetPaths,
                          const std::vector<std::string>& originalOrder, std::vector<uint64_t>& usedPidCounts,
                          std::vector<std::vector<uint64_t>>& usedPidSets)
{
    auto fileToPids = BuildFileToPidsMap(targetPaths);

    size_t vecLen = originalOrder.size();
    usedPidCounts.assign(vecLen, 0);
    usedPidSets.resize(vecLen);

    for (size_t i = 0; i < vecLen; ++i) {
        const auto& path = originalOrder[i];
        if (auto it = fileToPids.find(OBMM_DEV_PATH + path); it != fileToPids.end()) {
            UBSE_LOG_DEBUG << MMI_LOG_INFO << "Pid size is " << it->second.size() << ", path " << OBMM_DEV_PATH + path;
            usedPidCounts[i] = it->second.size();
            usedPidSets[i] = std::move(it->second);
        } else {
            usedPidSets[i].clear();
        }
    }
}

UbseResult RmObmmMetaRestore::ReadAgentLocalObmmMetaData(
    uint64_t taskId, std::vector<UbseMemLocalObmmMetaData>& ubseMemLocalObmmMetaDatas, bool& lastPage)
{
    lastPage = true; // 单页模式
    ubseMemLocalObmmMetaDatas.clear();
    (void)taskId;

    auto obmmShmDevPath = ReadAllObmmShmDevPath();

    // 构建目标路径集合（用于快速查找）
    std::unordered_set<std::string> targetPaths;
    for (const auto& path : obmmShmDevPath) {
        targetPaths.insert(OBMM_DEV_PATH + path);
    }

    std::vector<uint64_t> usedPidCounts{};
    std::vector<std::vector<uint64_t>> usedPidSets{};
    BatchGetUsedPidCount(targetPaths, obmmShmDevPath, usedPidCounts, usedPidSets);

    UbseRoleInfo currentNode{};
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (UBSE_RESULT_FAIL(ret) || currentNode.nodeId.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get current node id fail, ret=" << ret;
        return ret;
    }

    for (int i = 0; i < obmmShmDevPath.size(); ++i) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << "Restoring obmm meta info, path=" << obmmShmDevPath[i];
        UbseMemLocalObmmMetaData obmmMetaData;
        obmmMetaData.localNodeId = currentNode.nodeId;
        obmmMetaData.usedPidCount = usedPidCounts[i];
        obmmMetaData.usedPidSet = usedPidSets[i];
        auto ret = RestoreAgentLocalObmmMetaData(obmmShmDevPath[i], obmmMetaData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm Meta collect fail, skip device, path=" << obmmShmDevPath[i]
                           << ", memId=" << obmmMetaData.localMemId << ", ret=" << ret;
            continue;
        }
        ubseMemLocalObmmMetaDatas.push_back(obmmMetaData);
    }
    return UBSE_OK;
}

std::vector<std::string> RmObmmMetaRestore::ReadAllObmmShmDevPath()
{
    std::string directory = "/sys/devices/obmm";

    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << directory << " open failed.";
        return {};
    }

    std::vector<std::string> allDir;

    dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.' && entry->d_type == DT_DIR) {
            allDir.emplace_back(entry->d_name);
        }
    }

    std::regex pattern(R"(obmm_shmdev(\d+))");
    std::smatch match;
    std::vector<std::string> result;
    for (const auto& dirname : allDir) {
        try {
            if (!std::regex_search(dirname, match, pattern)) {
                continue;
            }
            result.push_back(dirname);
        } catch (const std::regex_error& e) {
            UBSE_LOG_ERROR << "ReadAllObmmShmDevPath regex error! dirname=" << dirname << ", error=" << e.what();
            closedir(dir);
            return result;
        }
    }

    closedir(dir);
    return result;
}

UbseResult RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(const uint8_t* buffer, uint64_t length,
                                                                      UbseMemLocalObmmCustomMeta& customMeta,
                                                                      UbMemPrivData& privData)
{
    const auto ret = DecodeObmmCustomMeta(buffer, length, customMeta, privData);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Invalid OBMM private metadata, length=" << length << ", ret=" << ret;
    }
    return ret;
}

namespace {
// 过渡态（SWAPPING/FINISHING）短间隔、有限次数重试，等待当前切换操作收敛
constexpr int SWITCH_TRANSIENT_RETRY_TIMES = 5;
constexpr useconds_t SWITCH_TRANSIENT_RETRY_INTERVAL_US = 100 * 1000; // 100ms

bool IsSwitchTransientState(uint8_t switchState)
{
    return switchState == static_cast<uint8_t>(ObmmSwitchState::SWAPPING) ||
           switchState == static_cast<uint8_t>(ObmmSwitchState::FINISHING);
}

std::string SwitchStatePath(const std::string& shmDevDirName)
{
    return OBMM_META_PATH + shmDevDirName + "/switch_state";
}
} // namespace

UbseResult RmObmmDevRead::GetStableSwitchState(mem_id memId, uint8_t& switchState)
{
    auto ret = RmObmmDevRead::GetSwitchState(memId, switchState);
    for (int i = 0; ret == UBSE_OK && IsSwitchTransientState(switchState) && i < SWITCH_TRANSIENT_RETRY_TIMES; ++i) {
        (void)usleep(SWITCH_TRANSIENT_RETRY_INTERVAL_US);
        ret = RmObmmDevRead::GetSwitchState(memId, switchState);
    }
    if (ret == UBSE_OK && IsSwitchTransientState(switchState)) {
        // 重试耗尽仍处于过渡态：本设备读取失败，不能默认成 IDLE。
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Switch state still transient after retry, memId=" << memId
                       << ", state=" << static_cast<uint32_t>(switchState);
        return UBSE_MMI_OPEN_FAILED;
    }
    return ret;
}

// AWAIT_READY 只读 memId、size、priv，跳过 type、import_info/export_info 等不可读身份字段；
// STRANDED 恢复 memId 及可读元数据，保留不可用资源记录。旧身份与引用按 priv 恢复，不伪造 pending 目标。
// priv 已在版本识别阶段读取，这里不再重复读取。
UbseResult RestoreAgentLocalObmmMetaDataInSwitch(const std::string& path, UbseMemLocalObmmMetaData& localObmmMetaData,
                                                 ObmmSwitchState switchState)
{
    auto ret = RmObmmDevRead::GetTotalSize(OBMM_META_PATH + path, localObmmMetaData.totalSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get total size fail in switch state! Path=" << path
                       << ", state=" << static_cast<uint32_t>(switchState);
        return ret;
    }
    localObmmMetaData.obmmMemExportInfo.length = localObmmMetaData.totalSize;

    // AWAIT_READY/STRANDED 内身份类 sysfs 不可读，导入/导出身份按旧 priv 的迁移标记恢复；
    // 非共享 V2 设备不应处于切换态，无法恢复身份时按采集失败处理
    if (localObmmMetaData.customMeta.version != OBMM_CUSTOM_META_V2) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Device in switch state without V2 migrate marker! Path=" << path
                       << ", state=" << static_cast<uint32_t>(switchState)
                       << ", version=" << static_cast<uint32_t>(localObmmMetaData.customMeta.version);
        return UBSE_ERROR_INVAL;
    }
    localObmmMetaData.memIdType =
        ubse::adapter_plugins::mmi::ShmMigrateIsExport(localObmmMetaData.customMeta.migrateInfo) ?
            static_cast<uint8_t>(UbseObmmType::EXPORT) :
            static_cast<uint8_t>(UbseObmmType::IMPORT);

    UBSE_LOG_WARN << MMI_LOG_INFO << "Restore device in switch state, path=" << path
                  << ", memId=" << localObmmMetaData.localMemId << ", state=" << static_cast<uint32_t>(switchState)
                  << ", memIdType=" << static_cast<uint32_t>(localObmmMetaData.memIdType)
                  << ", name=" << localObmmMetaData.customMeta.name;
    return UBSE_OK;
}

UbseResult RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(const std::string& path,
                                                            UbseMemLocalObmmMetaData& localObmmMetaData)
{
    // memId 从目录名解析，不依赖身份类文件，任何切换状态下都可读
    auto ret = RmObmmDevRead::GetLocalMemId(OBMM_META_PATH + path, localObmmMetaData.localMemId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get local memId fail! Path=" << path;
        return ret;
    }

    // 先读 priv 识别元数据版本：priv 在 AWAIT_READY 中仍可读取， type、import_info/export_info 等
    // 不可读；switch_state 自 V2 起才必然存在，V1 及之前不强制读取，避免旧设备因缺少
    // 该文件而恢复失败
    ret = RmObmmDevRead::GetCustomMeta(OBMM_META_PATH + path, localObmmMetaData.customMeta, localObmmMetaData.privData);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta fail! Path=" << path;
        return ret;
    }

    // 切换态恢复依赖 obmm_switch_* 符号：仅符号已加载且 switch_state 文件存在时才读取切换状态并走
    // 内部恢复路径；旧版本 libobmm 未加载或 V1 设备无 switch_state 文件时跳过该分支，走正常恢复逻辑
    if (localObmmMetaData.customMeta.version == OBMM_CUSTOM_META_V2 &&
        RmObmmExecutor::GetInstance().IsSwitchSupported() &&
        ubse::utils::UbseFileUtil::CheckFileExists(SwitchStatePath(path))) {
        // V2 必须读取切换状态（memId 已确定读到，路径拼接收在读取函数内部）：过渡态（SWAPPING/FINISHING）
        // 短间隔、有限次数重试，稳定后重新分支；读取失败或重试耗尽按采集失败上报，不能当成 IDLE
        uint8_t switchState{};
        ret = RmObmmDevRead::GetStableSwitchState(localObmmMetaData.localMemId, switchState);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Get switch state fail or unstable! Path=" << path
                           << ", memId=" << localObmmMetaData.localMemId << ", ret=" << ret;
            return ret;
        }
        if (switchState == static_cast<uint8_t>(ObmmSwitchState::AWAIT_READY) ||
            switchState == static_cast<uint8_t>(ObmmSwitchState::STRANDED)) {
            return RestoreAgentLocalObmmMetaDataInSwitch(path, localObmmMetaData,
                                                         static_cast<ObmmSwitchState>(switchState));
        }
    }

    // V2 的 IDLE（或过渡态收敛后回到 IDLE）、V1 及之前，走现有正常恢复逻辑，读取实际身份、引用及描述
    //（priv 已在版本识别阶段读取，不再重复读取）
    ret = RmObmmDevRead::GetMemIdType(OBMM_META_PATH + path, localObmmMetaData.memIdType);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get memory type fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetRemoteNuma(OBMM_META_PATH + path, localObmmMetaData.memIdType,
                                       localObmmMetaData.remoteNumaId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get remote numaId fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetTotalSize(OBMM_META_PATH + path, localObmmMetaData.totalSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get ub memory info fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetMemUbMemInfo(OBMM_META_PATH + path, localObmmMetaData.memIdType,
                                         localObmmMetaData.obmmMemExportInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get ub memory info fail! Path=" << path;
        return ret;
    }
    if (localObmmMetaData.memIdType == static_cast<uint8_t>(UbseObmmType::IMPORT)) {
        localObmmMetaData.obmmMemExportInfo.length = localObmmMetaData.totalSize;
    }

    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetSwitchState(mem_id memId, uint8_t& switchState)
{
    // 路径由 memId 在内部拼接，调用方只关心设备编号
    std::string path2switchState = SwitchStatePath("obmm_shmdev" + std::to_string(memId));
    std::string stateStr;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2switchState, stateStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get switch_state from obmm meta. Path=" << path2switchState
                       << ", memId=" << memId;
        return ret;
    }
    uint64_t state = 0;
    if (!RmCommonUtils::GetInstance().StrToULL(stateStr, state) ||
        state > static_cast<uint64_t>(ObmmSwitchState::STRANDED)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Invalid switch_state value, path=" << path2switchState
                       << ", value=" << stateStr;
        return UBSE_ERROR_INVAL;
    }
    switchState = static_cast<uint8_t>(state);
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetMemIdType(const std::string& path, uint8_t& memIdType)
{
    std::string path2type = path + "/type";
    std::string type;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2type, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get type from obmm meta. Path=" << path2type;
        return ret;
    }
    if (type == "export") {
        memIdType = 1; // 1 means export
    } else if (type == "import") {
        memIdType = 0; // 0 means import
    } else {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Read memId type from " << path << " fail, value=" << type;
        return UBSE_MMI_OPEN_FAILED;
    }
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetRemoteNuma(const std::string& path, uint8_t memIdType, int16_t& remoteNuma)
{
    if (memIdType == 1) { // 1 means export
        remoteNuma = -1;  // invalid value
        return UBSE_OK;
    }
    std::string path2remoteNuma = path + "/import_info/numa_id";
    std::string numaId;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2remoteNuma, numaId);
    if (ret != UBSE_OK) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << "Failed to get remote numaId from obmm meta. Path=" << path2remoteNuma;
        // 有些属性可能仅在特定条件满足时适用。例如 import_info/numa_id 文件仅在以 NUMA 方式引入时才会出现
        remoteNuma = -1; // invalid value
        return UBSE_OK;
    }
    int64_t tmpRemoteNuma;
    if (!RmCommonUtils::GetInstance().StrToLong(numaId, tmpRemoteNuma)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Convert string to number fail, numaId=" << numaId;
        return UBSE_ERROR_INVAL;
    }
    remoteNuma = static_cast<int16_t>(tmpRemoteNuma);
    return UBSE_OK;
}

static bool IsFileOpenByProc(const std::filesystem::path& procFdPath, const std::string& targetFile)
{
    std::filesystem::directory_iterator list;
    try {
        list = std::filesystem::directory_iterator(procFdPath);
    } catch (const std::filesystem::filesystem_error& e) {
        // 捕获并处理directory_iterator可能抛出的异常，比如权限问题
        // 只打印错误码，不打印 what()（其中包含文件路径）
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Directory iteration failed, error=" << e.code().value();
        return false;
    }
    for (const auto& entry : list) {
        if (!std::filesystem::is_symlink(entry.path())) {
            continue;
        }
        try {
            if (std::filesystem::read_symlink(entry.path()) == targetFile) {
                return true;
            }
        } catch (...) {
            // 忽略无法读取的符号链接
            continue;
        }
    }
    return false;
}

UbseResult RmObmmDevRead::GetTotalSize(const std::string& path, uint64_t& totalSize)
{
    std::string path2totalSize = path + "/size";
    std::string strSize;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2totalSize, strSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get total size from obmm meta. Path=" << path2totalSize;
        return ret;
    }
    if (!RmCommonUtils::GetInstance().StrToULL(strSize, totalSize, 16U)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Convert string to number fail, string  size=" << strSize;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetLocalMemId(const std::string& path, uint64_t& localMemId)
{
    std::regex pattern(R"((\d+))");
    std::smatch match;

    try {
        if (!std::regex_search(path, match, pattern)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Get memId from path fail! Path=" << path;
            return UBSE_ERROR_INVAL;
        }
    } catch (const std::regex_error& e) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get memId from path regex error! Path=" << path << ", error=" << e.what();
        return UBSE_ERROR_INVAL;
    }

    std::string strMemId = match.str();
    if (!RmCommonUtils::GetInstance().StrToULong(strMemId, localMemId)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Convert string to memId fail! Str=" << strMemId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetUbMemInfoFromFile(const std::string& path, std::string& uba, std::string& length,
                                               std::string& tokenid, std::string& deid)
{
    std::string path2uba = path + "/export_info/uba";
    std::string path2length = path + "/size";
    std::string path2tokenid = path + "/export_info/tokenid";
    std::string path2deid = path + "/export_info/deid";

    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2uba, uba);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get uba from obmm meta. Path=" << path2uba;
        return ret;
    }
    ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2length, length);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get length from obmm meta. Path=" << path2length;
        return ret;
    }
    ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2tokenid, tokenid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get tokenid from obmm meta. Path=" << path2tokenid;
        return ret;
    }
    ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2deid, deid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get deid from obmm meta. Path=" << path2deid;
        return ret;
    }

    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetMemUbMemInfo(const std::string& path, uint8_t memIdType, ubse_mem_obmm_mem_desc& ubMemInfo)
{
    if (memIdType == 0) { // 0 means import
        return UBSE_OK;
    }

    std::string uba;
    std::string length;
    std::string tokenid;
    std::string deid;

    auto ret = GetUbMemInfoFromFile(path, uba, length, tokenid, deid);
    if (ret != UBSE_OK) {
        return ret;
    }

    uint64_t tmpTokenId;
    uint32_t tmpDeid;
    auto parseRes = ParsePreOnlineEidStr(deid, tmpDeid);
    if (!parseRes || !RmCommonUtils::GetInstance().StrToULL(uba, ubMemInfo.addr, 16U) ||
        !RmCommonUtils::GetInstance().StrToULL(length, ubMemInfo.length, 16U) ||
        !RmCommonUtils::GetInstance().StrToULL(tokenid, tmpTokenId, 16U)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Convert string to number fail. uba=" << uba << ", length=" << length
                       << ", tokenid=" << tokenid << ", deid=" << deid;
        return UBSE_ERROR_INVAL;
    }

    ubMemInfo.tokenid = tmpTokenId;
    ubMemInfo.deid = tmpDeid;
    ubMemInfo.seid = 0;
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetCustomMeta(const std::string& path, UbseMemLocalObmmCustomMeta& customMeta,
                                        UbMemPrivData& privData)
{
    std::string path2privLen = path + "/priv_len";
    std::string path2priv = path + "/priv";
    std::string tmpPrivLen;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2privLen, tmpPrivLen);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get priv_len from obmm meta. Path=" << path2privLen;
        return ret;
    }
    uint64_t length;
    if (!RmCommonUtils::GetInstance().StrToULL(tmpPrivLen, length)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Convert str to long error, tmpPrivLen=" << tmpPrivLen;
        return UBSE_ERROR_INVAL;
    }
    if (length <= sizeof(UbMemPrivData) || length > OBMM_CUSTOM_META_MAX_LENGTH) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Invalid OBMM priv_len=" << length;
        return UBSE_ERROR_INVAL;
    }
    ubse::security::CapabilityGuard capsGuard(caps);
    auto fd = open(path2priv.c_str(), O_RDONLY);
    // 只包住这一次 open：后续 new/read/解析都不需要 CAP_DAC_OVERRIDE，显式收起保持最小权限窗口
    capsGuard.Disable();
    if (fd == -1) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Open sysfs_priv failed, Path=" << path2priv;
        return UBSE_MMI_OPEN_FAILED;
    }
    auto buffer = new (std::nothrow) uint8_t[length];
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to malloc memory, path=" << path;
        close(fd);
        return UBSE_ERROR_INVAL;
    }
    auto num_bytes = read(fd, buffer, length);
    if (num_bytes != length) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to read full data, num_bytes=" << num_bytes
                       << ", request_bytes=" << length;
        close(fd);
        RmCommonUtils::GetInstance().SafeDeleteArray(buffer);
        return UBSE_MMI_OPEN_FAILED;
    }
    ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(buffer, length, customMeta, privData);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Copy buffer to custom meta fail! length=" << length;
        RmCommonUtils::GetInstance().SafeDeleteArray(buffer);
        close(fd);
        return ret;
    }
    close(fd);
    RmCommonUtils::GetInstance().SafeDeleteArray(buffer);
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetNuma(mem_id memid, uint64_t& numa)
{
    auto filePath = "/sys/devices/obmm/obmm_shmdev" + std::to_string(memid) + "/export_info/node_mem_size";
    std::string line;
    auto ret = RmCommonUtils::GetFileFirstLine(filePath, line);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get node_mem_size from obmm meta. Path=" << filePath;
        return ret;
    }
    int index = 0;                // 记录当前值的索引
    std::istringstream iss(line); // 将行内容放入字符串流中
    std::string token;
    bool isNonZero = false;
    while (std::getline(iss, token, ',')) {
        token.erase(std::remove(token.begin(), token.end(), ' '), token.end());
        try {
            size_t pos;
            uint64_t value = std::stoul(token, &pos, 16); // 以十六进制解析
            if (value != 0x00) {
                isNonZero = true;
                break;
            }

            ++index;
        } catch (const std::exception& e) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Error parsing token." << e.what();
        }
    }

    if (!isNonZero) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get numa failed, memid=" << memid;
        return UBSE_ERROR_INVAL;
    }
    numa = index;
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetNameByMemId(mem_id memId, std::string& name)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev" + std::to_string(memId);
    UbseMemLocalObmmCustomMeta customMeta{};
    UbMemPrivData privData{};
    auto ret = GetCustomMeta(path, customMeta, privData);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "GetNameByMemId failed to read customMeta, memid=" << memId << ", ret=" << ret;
        return ret;
    }
    name = std::string(customMeta.name);
    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetBorrowTypeByMemId(mem_id memId, uint8_t& borrowType)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev" + std::to_string(memId);
    UbseMemLocalObmmCustomMeta customMeta{};
    UbMemPrivData privData{};
    auto ret = GetCustomMeta(path, customMeta, privData);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "GetBorrowTypeByMemId failed, memid=" << memId << ", ret=" << ret;
        return ret;
    }
    borrowType = customMeta.type;
    return UBSE_OK;
}

} // namespace ubse::mmi
