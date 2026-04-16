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
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <regex>
#include "fcntl.h"
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_obmm_utils.h"
#include "unistd.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_security_module.h"

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
        return {std::filesystem::directory_iterator(dirPath),
                std::filesystem::directory_iterator()};
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
FileToPidsMap BuildFileToPidsMap(const std::unordered_set<std::string> &targetPaths)
{
    FileToPidsMap result;
    const std::filesystem::path procPath("/proc");

    for (const auto &procEntry : SafeDirectoryEntries(procPath)) {
        if (!procEntry.is_directory()) continue;

        const std::string pidStr = procEntry.path().filename().string();
        uint64_t pid = 0;
        if (!RmCommonUtils::GetInstance().StrToULong(pidStr, pid)) continue;

        std::filesystem::path fdDir = procEntry.path() / "fd";
        for (const auto &fdEntry : SafeDirectoryEntries(fdDir)) {
            if (!IsSymlink(fdEntry.path())) continue;

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
void BatchGetUsedPidCount(const std::unordered_set<std::string > &targetPaths,
    const std::vector<std::string> &originalOrder, std::vector<uint64_t> &usedPidCounts,
    std::vector<std::vector<uint64_t>> &usedPidSets)
{
    auto fileToPids = BuildFileToPidsMap(targetPaths);

    size_t vecLen = originalOrder.size();
    usedPidCounts.assign(vecLen, 0);
    usedPidSets.resize(vecLen);

    for (size_t i = 0; i < vecLen; ++i) {
        const auto &path = originalOrder[i];
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
    uint64_t taskId, std::vector<UbseMemLocalObmmMetaData> &ubseMemLocalObmmMetaDatas, bool &lastPage)
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
            // 正在删除文件的时候采集可能会失败，其他的元数据不受影响
            UBSE_LOG_WARN << MMI_LOG_INFO << "Obmm Meta collect fail, path=" << obmmShmDevPath[i];
            continue;
        }
        ubseMemLocalObmmMetaDatas.push_back(obmmMetaData);
    }
    return UBSE_OK;
}

std::vector<std::string> RmObmmMetaRestore::ReadAllObmmShmDevPath()
{
    std::string directory = "/sys/devices/obmm";

    DIR *dir = opendir(directory.c_str());
    if (dir == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << directory << " open failed.";
        return {};
    }

    std::vector<std::string> allDir;

    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.' && entry->d_type == DT_DIR) {
            allDir.emplace_back(entry->d_name);
        }
    }

    std::regex pattern(R"(obmm_shmdev(\d+))");
    std::smatch match;
    std::vector<std::string> result;
    for (const auto &dirname : allDir) {
        try {
            if (!std::regex_search(dirname, match, pattern)) {
                continue;
            }
            result.push_back(dirname);
        } catch (const std::regex_error &e) {
            UBSE_LOG_ERROR << "ReadAllObmmShmDevPath regex error! dirname=" << dirname << ", error=" << e.what();
            closedir(dir);
            return result;
        }
    }

    closedir(dir);
    return result;
}

UbseResult RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(const uint8_t *buffer, uint64_t length,
    UbseMemLocalObmmCustomMeta &customMeta, UbMemPrivData &privData)
{
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Buffer is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    if (length != sizeof(customMeta) + sizeof(UbMemPrivData)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The buffer length not equals to size of customMeta.";
        return UBSE_ERROR_INVAL;
    }
    auto ret = memcpy_s(&customMeta, sizeof(customMeta), buffer + sizeof(UbMemPrivData), sizeof(customMeta));
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory copy failed: customMeta.";
        return UBSE_ERROR_INVAL;
    }
    ret = memcpy_s(&privData, sizeof(privData), buffer, sizeof(privData));
    if (ret != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Memory copy failed: privData.";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(const std::string &path,
                                                            UbseMemLocalObmmMetaData &localObmmMetaData)
{
    auto ret = RmObmmDevRead::GetMemIdType(OBMM_META_PATH + path, localObmmMetaData.memIdType);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get memory type fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetRemoteNuma(OBMM_META_PATH + path,
        localObmmMetaData.memIdType, localObmmMetaData.remoteNumaId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get remote numaId fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetTotalSize(OBMM_META_PATH + path, localObmmMetaData.totalSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get ub memory info fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetLocalMemId(OBMM_META_PATH + path, localObmmMetaData.localMemId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get local memId fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetMemUbMemInfo(OBMM_META_PATH + path,
        localObmmMetaData.memIdType, localObmmMetaData.obmmMemExportInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get ub memory info fail! Path=" << path;
        return ret;
    }

    ret = RmObmmDevRead::GetCustomMeta(OBMM_META_PATH + path,
        localObmmMetaData.customMeta, localObmmMetaData.privData);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta fail! Path=" << path;
        return ret;
    }

    return UBSE_OK;
}

UbseResult RmObmmDevRead::GetMemIdType(const std::string &path, uint8_t &memIdType)
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

UbseResult RmObmmDevRead::GetRemoteNuma(const std::string &path, uint8_t memIdType, int16_t &remoteNuma)
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

static bool IsFileOpenByProc(const std::filesystem::path &procFdPath, const std::string &targetFile)
{
    std::filesystem::directory_iterator list;
    try {
        list = std::filesystem::directory_iterator(procFdPath);
    } catch (const std::filesystem::filesystem_error &e) {
        // 捕获并处理directory_iterator可能抛出的异常，比如权限问题
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Directory iteration failed=" << e.what();
        return false;
    }
    for (const auto &entry : list) {
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

UbseResult RmObmmDevRead::GetTotalSize(const std::string &path, uint64_t &totalSize)
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

UbseResult RmObmmDevRead::GetLocalMemId(const std::string &path, uint64_t &localMemId)
{
    std::regex pattern(R"((\d+))");
    std::smatch match;

    try {
        if (!std::regex_search(path, match, pattern)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Get memId from path fail! Path=" << path;
            return UBSE_ERROR_INVAL;
        }
    } catch (const std::regex_error &e) {
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

UbseResult RmObmmDevRead::GetUbMemInfoFromFile(const std::string &path, std::string &uba,
    std::string &length, std::string &tokenid, std::string &deid)
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

UbseResult RmObmmDevRead::GetMemUbMemInfo(const std::string &path,
    uint8_t memIdType, ubse_mem_obmm_mem_desc &ubMemInfo)
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

UbseResult RmObmmDevRead::GetCustomMeta(const std::string &path,
    UbseMemLocalObmmCustomMeta &customMeta, UbMemPrivData &privData)
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
    if (length == 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get the priv_len is 0.";
        return UBSE_ERROR_INVAL;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    auto fd = open(path2priv.c_str(), O_RDONLY);
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
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

UbseResult RmObmmDevRead::GetNuma(mem_id memid, uint64_t &numa)
{
    auto filePath = "/sys/devices/obmm/obmm_shmdev" + std::to_string(memid) + "/export_info/node_mem_size";
    std::string line;
    auto ret = RmCommonUtils::GetFileFirstLine(filePath, line);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get node_mem_size from obmm meta. Path=" << filePath;
        return ret;
    }
    int index = 0; // 记录当前值的索引
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
        } catch (const std::exception &e) {
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
} // namespace ubse::mmi