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
#include <filesystem>
#include <iostream>
#include <regex>
#include "ubse_common_def.h"
#include "ubse_mem_def.h"
#include "fcntl.h"
#include "ubse_mem_common_utils.h"
#include "securec.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "unistd.h"
#include "ubse_obmm_utils.h"

namespace ubse::mmi {
using namespace ubse::common::def;
using namespace ubse::election;
constexpr auto OBMM_META_PATH = "/sys/devices/obmm/";
constexpr auto OBMM_DEV_PATH = "/dev/";

UbseResult RmObmmMetaRestore::ReadAgentLocalObmmMetaData(
    uint64_t taskId, std::vector<UbseMemLocalObmmMetaData> &ubseMemLocalObmmMetaDatas, bool &lastPage)
{
    lastPage = true; // 单页模式
    ubseMemLocalObmmMetaDatas.clear();
    (void)taskId;
    std::vector<std::string> obmmShmDevPath;
    auto ret = ReadAllObmmShmDevPath(obmmShmDevPath);
    if (ret != HOK) {
        return ret;
    }

    for (const auto &path : obmmShmDevPath) {
        UBSE_LOG_DEBUG << "Restoring obmm meta info, path: " << path;
        UbseMemLocalObmmMetaData obmmMetaData;
        ret = RestoreAgentLocalObmmMetaData(path, obmmMetaData);
        if (ret != HOK) {
            // 正在删除文件的时候采集可能会失败，其他的元数据不受影响
            UBSE_LOG_WARN << "Obmm Meta collect fail, path: " << path;
            continue;
        }

        ubseMemLocalObmmMetaDatas.push_back(obmmMetaData);
    }

    return HOK;
}

UbseResult RmObmmMetaRestore::ReadAllObmmShmDevPath(std::vector<std::string> &pathList)
{
    std::string directory = "/sys/devices/obmm";

    DIR *dir = opendir(directory.c_str());
    if (dir == nullptr) {
        UBSE_LOG_ERROR << directory << " open failed.";
        return E_CODE_OPEN_FILE;
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
    pathList.clear();
    for (const auto &dirname : allDir) {
        try {
            if (!std::regex_search(dirname, match, pattern)) {
                continue;
            }
            pathList.push_back(dirname);
        } catch (const std::regex_error &e) {
            UBSE_LOG_ERROR << "ReadAllObmmShmDevPath regex error! dirname:" << dirname << ", error: " << e.what();
            closedir(dir);
            return E_CODE_OPEN_FILE;
        }
    }

    closedir(dir);
    return HOK;
}

UbseResult RmObmmMetaRestore::CopyBuffToAgentLocalObmmMetaData(const uint8_t *buffer, uint64_t length,
                                                               UbseMemLocalObmmCustomMeta &customMeta)
{
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "Buffer is nullptr.";
        return E_CODE_NULLPTR;
    }
    if (length != sizeof(customMeta) + sizeof(UbMemPrivData)) {
        UBSE_LOG_ERROR << "The buffer length not equals to size of customMeta.";
        return E_CODE_INVALID_PAR;
    }
    auto ret = memcpy_s(&customMeta, sizeof(customMeta), buffer + sizeof(UbMemPrivData), sizeof(customMeta));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Memory copy failed.";
        return E_CODE_INVALID_PAR;
    }
    return HOK;
}

UbseResult RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(const std::string &path,
                                                            UbseMemLocalObmmMetaData &localObmmMetaData)
{
    auto ret = GetMemIdType(OBMM_META_PATH + path, localObmmMetaData.memIdType);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get memory type fail! Path: " << path;
        return ret;
    }

    ret = GetRemoteNuma(OBMM_META_PATH + path, localObmmMetaData.memIdType, localObmmMetaData.remoteNumaId);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get remote numaId fail! Path: " << path;
        return ret;
    }

    ret = GetUsedPidCount(OBMM_DEV_PATH + path, localObmmMetaData.usedPidCount, localObmmMetaData.usedPidSet);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get memory used pid count fail! Path: " << path;
        return ret;
    }

    ret = GetTotalSize(OBMM_META_PATH + path, localObmmMetaData.totalSize);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get ub memory info fail! Path: " << path;
        return ret;
    }

    ret = GetLocalMemId(OBMM_META_PATH + path, localObmmMetaData.localMemId);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get local memId fail! Path: " << path;
        return ret;
    }

    UbseRoleInfo currentNode{};
    ret = UbseGetCurrentNodeInfo(currentNode);
    localObmmMetaData.localNodeId = currentNode.nodeId;
    if (UBSE_RESULT_FAIL(ret) || localObmmMetaData.localNodeId.empty()) {
        UBSE_LOG_ERROR << "Get current node id fail, ret = " << path;
        return ret;
    }

    ret = GetMemUbMemInfo(OBMM_META_PATH + path, localObmmMetaData.memIdType, localObmmMetaData.obmmMemExportInfo);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get ub memory info fail! Path: " << path;
        return ret;
    }

    ret = GetCustomMeta(OBMM_META_PATH + path, localObmmMetaData.customMeta);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Get custom meta fail! Path: " << path;
        return ret;
    }

    return HOK;
}

UbseResult RmObmmMetaRestore::GetMemIdType(const std::string &path, uint8_t &memIdType)
{
    std::string path2type = path + "/type";
    std::string type;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2type, type);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get type from obmm meta. path: " << path2type;
        return ret;
    }
    if (type == "export") {
        memIdType = 1; // 1 means export
    } else if (type == "import") {
        memIdType = 0; // 0 means import
    } else {
        UBSE_LOG_ERROR << "Read memId type from " << path << " fail, value: " << type;
        return E_CODE_OPEN_FILE;
    }
    return HOK;
}

UbseResult RmObmmMetaRestore::GetRemoteNuma(const std::string &path, uint8_t memIdType, int16_t &remoteNuma)
{
    if (memIdType == 1) { // 1 means export
        remoteNuma = -1;  // invalid value
        return HOK;
    }
    std::string path2remoteNuma = path + "/import_info/numa_id";
    std::string numaId;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2remoteNuma, numaId);
    if (ret != HOK) {
        UBSE_LOG_DEBUG << "Failed to get remote numaId from obmm meta. path: " << path2remoteNuma;
        // 有些属性可能仅在特定条件满足时适用。例如 import_info/numa_id 文件仅在以 NUMA 方式引入时才会出现
        remoteNuma = -1; // invalid value
        return HOK;
    }
    int64_t tmpRemoteNuma;
    if (!RmCommonUtils::GetInstance().StrToLong(numaId, tmpRemoteNuma)) {
        UBSE_LOG_ERROR << "Convert string to number fail, numaId: " << numaId;
        return E_CODE_INVALID_PAR;
    }
    remoteNuma = static_cast<int16_t>(tmpRemoteNuma);
    return HOK;
}

static bool IsFileOpenByProc(const std::filesystem::path &procFdPath, const std::string &targetFile)
{
    std::filesystem::directory_iterator list;
    try {
        list = std::filesystem::directory_iterator(procFdPath);
    } catch (const std::filesystem::filesystem_error &e) {
        UBSE_LOG_ERROR << "Directory iteration failed: " << e.what();
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
            continue;
        }
    }
    return false;
}

UbseResult RmObmmMetaRestore::GetUsedPidCount(const std::string &path, uint64_t &usedPidCount,
                                              std::vector<uint64_t> &usedPidSet)
{
    usedPidCount = 0;
    usedPidSet.clear();
    std::filesystem::directory_iterator list;
    try {
        list = std::filesystem::directory_iterator("/proc");
    } catch (const std::filesystem::filesystem_error &e) {
        UBSE_LOG_ERROR << "Directory iteration failed: " << e.what();
        return E_CODE_AGENT;
    }
    for (const auto &entry : list) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string absPath = entry.path().filename().string();
        uint64_t pid = 0;
        if (!RmCommonUtils::GetInstance().StrToULong(absPath, pid)) {
            continue;
        }
        std::filesystem::path fdPath = entry.path() / "fd";
        if (std::filesystem::exists(fdPath) && IsFileOpenByProc(fdPath, path)) {
            ++usedPidCount;
            usedPidSet.push_back(pid);
        }
    }
    UBSE_LOG_DEBUG << "GetUsedPidCount path is " << path << " usedPidCount is " << usedPidCount
                 << " pids: " << RmCommonUtils::GetInstance().IntListToStr(usedPidSet);
    return HOK;
}

UbseResult RmObmmMetaRestore::GetTotalSize(const std::string &path, uint64_t &totalSize)
{
    std::string path2totalSize = path + "/size";
    std::string strSize;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2totalSize, strSize);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get total size from obmm meta. path: " << path2totalSize;
        return ret;
    }
    if (!RmCommonUtils::GetInstance().StrToULL(strSize, totalSize, 16U)) {
        UBSE_LOG_ERROR << "Convert string to number fail, string  size: " << strSize;
        return E_CODE_INVALID_PAR;
    }
    return HOK;
}

UbseResult RmObmmMetaRestore::GetTotalSizeFromMemId(const uint64_t memId, uint64_t &totalSize,
                                                    const uint64_t defaultSize)
{
    std::ostringstream oss;
    oss << OBMM_META_PATH << "obmm_shmdev" << memId;
    return GetTotalSize(oss.str(), totalSize);
}

UbseResult RmObmmMetaRestore::GetLocalMemId(const std::string &path, uint64_t &localMemId)
{
    std::regex pattern(R"((\d+))");
    std::smatch match;

    try {
        if (!std::regex_search(path, match, pattern)) {
            UBSE_LOG_ERROR << "Get memId from path fail! Path: " << path;
            return E_CODE_INVALID_PAR;
        }
    } catch (const std::regex_error &e) {
        UBSE_LOG_ERROR << "Get memId from path regex error! Path: " << path << ", error: " << e.what();
        return E_CODE_INVALID_PAR;
    }

    std::string strMemId = match.str();
    if (!RmCommonUtils::GetInstance().StrToULong(strMemId, localMemId)) {
        UBSE_LOG_ERROR << "Convert string to memId fail! Str: " << strMemId;
        return E_CODE_INVALID_PAR;
    }
    return HOK;
}

UbseResult RmObmmMetaRestore::ParseUbMemInfoFromFile(const std::string &path, std::string &uba, std::string &length,
                                                     std::string &tokenid, std::string &deid)
{
    std::string path2uba = path + "/export_info/uba";
    std::string path2length = path + "/size";
    std::string path2tokenid = path + "/export_info/tokenid";
    std::string path2deid = path + "/export_info/deid";

    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2uba, uba);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get uba from obmm meta. path: " << path2uba;
        return ret;
    }
    ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2length, length);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get length from obmm meta. path: " << path2length;
        return ret;
    }
    ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2tokenid, tokenid);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get tokenid from obmm meta. path: " << path2tokenid;
        return ret;
    }
    ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2deid, deid);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get deid from obmm meta. path: " << path2deid;
        return ret;
    }

    return HOK;
}

UbseResult RmObmmMetaRestore::GetMemUbMemInfo(const std::string &path, uint8_t memIdType,
                                              ubse_mem_obmm_mem_desc &ubMemInfo)
{
    if (memIdType == 0) { // 0 means import
        return HOK;
    }

    std::string uba;
    std::string length;
    std::string tokenid;
    std::string deid;

    auto ret = ParseUbMemInfoFromFile(path, uba, length, tokenid, deid);
    if (ret != HOK) {
        return ret;
    }

    uint64_t tmpTokenId;
    uint32_t tmpDeid;
    auto parseRes = RmObmmUtils::ParsePreOnlineEidStr(deid, tmpDeid);
    if (!parseRes || !RmCommonUtils::GetInstance().StrToULL(uba, ubMemInfo.addr, 16U) ||
        !RmCommonUtils::GetInstance().StrToULL(length, ubMemInfo.length, 16U) ||
        !RmCommonUtils::GetInstance().StrToULL(tokenid, tmpTokenId, 16U)) {
        UBSE_LOG_ERROR << "Convert string to number fail. uba: " << uba << ", length: " << length
                     << ", tokenid: " << tokenid << ", deid: " << deid;
        return E_CODE_INVALID_PAR;
    }

    ubMemInfo.tokenid = tmpTokenId;
    ubMemInfo.deid = tmpDeid;
    ubMemInfo.seid = 0;
    return HOK;
}

UbseResult RmObmmMetaRestore::GetCustomMeta(const std::string &path, UbseMemLocalObmmCustomMeta &customMeta)
{
    std::string path2privLen = path + "/priv_len";
    std::string path2priv = path + "/priv";
    std::string tmpPrivLen;
    auto ret = RmCommonUtils::GetInstance().GetFileFirstLine(path2privLen, tmpPrivLen);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Failed to get priv_len from obmm meta. path: " << path2privLen;
        return ret;
    }
    uint64_t length;
    if (!RmCommonUtils::GetInstance().StrToULL(tmpPrivLen, length)) {
        UBSE_LOG_ERROR << "Convert str to long error, tmpPrivLen is " << tmpPrivLen;
        return E_CODE_INVALID_PAR;
    }
    if (length == 0) {
        UBSE_LOG_ERROR << "Get the priv_len is 0.";
        return E_CODE_INVALID_PAR;
    }

    auto fd = open(path2priv.c_str(), O_RDONLY);
    if (fd == -1) {
        UBSE_LOG_ERROR << "Open sysfs_priv failed, path: " << path2priv;
        return E_CODE_OPEN_FILE;
    }
    auto buffer = new (std::nothrow) uint8_t[length];
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "Failed to malloc memory, path is " << path;
        close(fd);
        return E_CODE_INVALID_PAR;
    }
    auto num_bytes = read(fd, buffer, length);
    if (num_bytes != length) {
        UBSE_LOG_ERROR << "Failed to read full data, num_bytes=" << num_bytes << ", request_bytes=" << length;
        close(fd);
        RmCommonUtils::GetInstance().SafeDeleteArray(buffer);
        return E_CODE_OPEN_FILE;
    }
    ret = CopyBuffToAgentLocalObmmMetaData(buffer, length, customMeta);
    if (ret != HOK) {
        UBSE_LOG_ERROR << "Copy buffer to custom meta fail! length=" << length;
        RmCommonUtils::GetInstance().SafeDeleteArray(buffer);
        close(fd);
        return ret;
    }
    close(fd);
    RmCommonUtils::GetInstance().SafeDeleteArray(buffer);
    return HOK;
}

} // namespace ubse::mmi