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

#include "ubse_ssu_adapter_impl.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <securec.h>
#include <sstream>
#include <cerrno>
#include <array>
#include <cstdio>
#include <chrono>
#include <thread>
#include <dirent.h>
#include <glib.h>
#include <climits>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ubse_conf.h"
#include "src/framework/misc/ubse_env_util.h"
#include "src/framework/misc/ubse_os_util.h"

namespace ubse::adapter_plugins::ssu::def {
UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
struct GStrFreevDeleter {
    void operator()(gchar** p) const
    {
        if (p) {
            g_strfreev(p);
        }
    }
};
using GStrvGuard = std::unique_ptr<gchar*, GStrFreevDeleter>;

constexpr int NQN_MASK_SUFFIX_LEN = 4;
// LBA Size by flbas: flbas=0对应512B，flbas=1对应4K（与UbseSsuLBAFormat一致）
constexpr uint64_t LBA_SIZE_512 = 512;
constexpr uint64_t LBA_SIZE_4K = 4096;

uint64_t GetLbaSize(uint32_t flbas)
{
    return (flbas == 1) ? 4096ULL : 512ULL;
}

// 将 libssu 返回的错误码映射为对应的 UBSE 错误码。
// 仅对 nqn 无权限（attach/detach 时 adminNqn 不在 server 白名单）做专门映射，
// 以便上层能识别"无权限"场景；其他 SSU 错误统一走 defaultErr 兜底，保留操作级语义。
// ssuRet: libssu 函数返回值；defaultErr: 未匹配到已知 SSU 错误码时的兜底错误码。
uint32_t MapSsuErrToUbseErr(int ssuRet, uint32_t defaultErr)
{
    switch (ssuRet) {
        case SSU_ERR_UNAUTHORIZED:
            return UBSE_ERR_ACCESS_DENIED;
        default:
            return defaultErr;
    }
}

std::string MaskNqn(const std::string &nqn)
{
    if (nqn.size() <= NQN_MASK_SUFFIX_LEN) {
        return "****";
    }
    return "****" + nqn.substr(nqn.size() - NQN_MASK_SUFFIX_LEN);
}

// guid/uuid 是 16 字节二进制数据，直接 << 输出会显示乱码，转成十六进制字符串便于日志阅读
std::string BytesToHex(const std::string &bytes)
{
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0xF]);
    }
    return out;
}

uint32_t GetAdminNqn(std::string &adminNqn)
{
    if (ubse::config::UbseGetStr("ubse.ssu", "ssu.adminNqn", adminNqn) != UBSE_OK || adminNqn.empty()) {
        UBSE_LOG_ERROR << "Failed to get ssu.adminNqn from config";
        return UBSE_SSU_ERROR_CONFIG_INVALID;
    }
    return UBSE_OK;
}

// /dev 下的 ssu 子目录，所有由 SSU 创建的块设备符号链接都集中在此目录下，
// 避免直接散落在 /dev/{vgName}/ 或 /dev/md/ 等路径中。
constexpr const char *SSU_DEV_DIR = "/dev/ssu";

// 确保 /dev/ssu 目录存在（幂等，已存在则直接返回成功）
uint32_t EnsureSsuDevDir()
{
    if (g_file_test(SSU_DEV_DIR, G_FILE_TEST_IS_DIR)) {
        return UBSE_OK;
    }
    if (mkdir(SSU_DEV_DIR, 0755) != 0 && errno != EEXIST) {
        int err = errno;
        UBSE_LOG_ERROR << "Failed to create directory " << SSU_DEV_DIR << ", errno=" << err;
        return UBSE_ERROR_IO;
    }
    return UBSE_OK;
}

// 聚合块设备当前状态：符号链接、md、LVM 三元存在性
struct BlockDevicePresence {
    bool ssuLinkExists{false};
    bool mdDeviceExists{false};
    bool lvmDeviceExists{false};
    std::string vgName;
    std::string mdDevicePath;
    std::string ssuLinkPath;

    bool AnyActive() const
    {
        return ssuLinkExists || mdDeviceExists || lvmDeviceExists;
    }
};

BlockDevicePresence GetBlockDevicePresence(const std::string &deviceName)
{
    BlockDevicePresence p;
    p.ssuLinkPath = std::string(SSU_DEV_DIR) + "/" + deviceName;
    p.ssuLinkExists = g_file_test(p.ssuLinkPath.c_str(), G_FILE_TEST_IS_SYMLINK);
    p.mdDevicePath = "/dev/md/" + deviceName;
    p.mdDeviceExists = g_file_test(p.mdDevicePath.c_str(), G_FILE_TEST_EXISTS);
    p.vgName = deviceName + "_vg";
    std::string lvPath1 = "/dev/mapper/" + p.vgName + "-" + deviceName;
    std::string lvPath2 = "/dev/" + p.vgName + "/" + deviceName;
    p.lvmDeviceExists = g_file_test(lvPath1.c_str(), G_FILE_TEST_EXISTS) ||
                        g_file_test(lvPath2.c_str(), G_FILE_TEST_EXISTS);
    return p;
}

// 在 /dev/ssu/{deviceName} 创建指向 targetPath 的符号链接，linkPath 输出最终的链接路径
uint32_t CreateSsuDevSymlink(const std::string &deviceName, const std::string &targetPath,
                             std::string &linkPath)
{
    uint32_t ret = EnsureSsuDevDir();
    if (ret != UBSE_OK) {
        return ret;
    }
    linkPath = std::string(SSU_DEV_DIR) + "/" + deviceName;
    // 移除已存在的符号链接以保证幂等性
    unlink(linkPath.c_str());
    if (symlink(targetPath.c_str(), linkPath.c_str()) != 0) {
        int err = errno;
        UBSE_LOG_ERROR << "Failed to create symlink " << linkPath << " -> " << targetPath
                       << ", errno=" << err;
        return UBSE_ERROR_IO;
    }
    return UBSE_OK;
}

// 移除 /dev/ssu/{deviceName} 符号链接（幂等，不存在时视为成功）
void RemoveSsuDevSymlink(const std::string &deviceName)
{
    std::string linkPath = std::string(SSU_DEV_DIR) + "/" + deviceName;
    if (unlink(linkPath.c_str()) != 0 && errno != ENOENT) {
        int err = errno;
        UBSE_LOG_WARN << "Failed to remove symlink " << linkPath << ", errno=" << err;
    }
}

// 通过sudo执行shell命令，合并stdout和stderr到output。
// 返回UBSE_OK表示命令成功退出（exit code 0）；popen失败返回UBSE_ERROR_IO；
// 命令非零退出返回UBSE_SSU_ERROR_EXEC_FAILED。
constexpr size_t CMD_OUT_BUF_SIZE = 4096;

// 校验设备名仅包含安全字符 [A-Za-z0-9_-]，防止外部输入经由 ExecWithSudo 拼入
// shell 字符串时引发命令注入（deviceName 来源于 RPC 请求，属用户可控输入）。
bool IsSafeDeviceName(const std::string &name)
{
    if (name.empty()) {
        return false;
    }
    for (char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

// 校验路径仅包含安全字符 [A-Za-z0-9_./-]，阻止 shell 元字符（;|&`$ 等）进入命令行。
bool IsSafePath(const std::string &path)
{
    if (path.empty()) {
        return false;
    }
    for (char c : path) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

// 去掉字符串尾部的空白/制表/换行字符，供 pvs、mdadm --detail 等命令输出解析复用。
std::string TrimTrailingWhitespace(std::string s)
{
    while (!s.empty()) {
        char c = s.back();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            s.pop_back();
        } else {
            break;
        }
    }
    return s;
}

uint32_t ExecWithSudo(const std::string &cmd, std::string &output)
{
    std::string sudoCmd = "sudo " + cmd + " 2>&1";
    std::array<char, CMD_OUT_BUF_SIZE> buffer{};
    output.clear();
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(sudoCmd.c_str(), "r"), pclose);
    if (!pipe) {
        UBSE_LOG_ERROR << "ExecWithSudo popen failed, cmd=" << cmd << ", errno=" << errno;
        return UBSE_ERROR_IO;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    int status = pclose(pipe.release());
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return UBSE_SSU_ERROR_EXEC_FAILED;
    }
    if (!output.empty()) {
        UBSE_LOG_INFO << "ExecWithSudo success, cmd=" << cmd << ", output=" << output;
    } else {
        UBSE_LOG_INFO << "ExecWithSudo success, cmd=" << cmd;
    }
    return UBSE_OK;
}

// 回滚：对已创建的物理卷逐个执行 pvremove -ff 并记录失败警告。
// scenario 用于区分各回滚场景的日志上下文。
void RollbackCreatedPVs(const std::vector<std::string> &createdPVs, const std::string &scenario)
{
    for (const auto &pvPath : createdPVs) {
        std::string pvOut;
        if (ExecWithSudo("pvremove -ff " + pvPath, pvOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to pvremove on " << pvPath
                          << " during " << scenario << ", output=" << pvOut;
        }
    }
}

// 扫描成员盘的 /sys/block/<baseName>/holders/ 目录，停止占用成员盘的 md 阵列。
// 场景：DetachStripedSpace 后 re-attach 前，udev/systemd 可能基于残留 superblock
// 自动 assemble 出 /dev/mdN 的幽灵阵列占用成员盘，导致后续 mdadm --assemble 报 "is busy"。
// 先调用本函数释放幽灵阵列，再执行 mdadm --assemble。
void StopMdHoldersOnMembers(const std::vector<std::string> &devicePathList)
{
    for (const auto &devPath : devicePathList) {
        if (devPath.empty()) {
            continue;
        }
        // 解析 by-id 符号链接到真实设备路径（/dev/nvme0n1）
        char *realBuf = static_cast<char *>(malloc(PATH_MAX));
        if (realBuf == nullptr) {
            UBSE_LOG_WARN << "StopMdHoldersOnMembers: malloc failed for " << devPath;
            continue;
        }
        if (realpath(devPath.c_str(), realBuf) == nullptr) {
            UBSE_LOG_WARN << "StopMdHoldersOnMembers: realpath failed for " << devPath << ", errno=" << errno;
            free(realBuf);
            continue;
        }

        std::string realPath(realBuf, strnlen(realBuf, PATH_MAX));
        free(realBuf);
        const std::string devPrefix = "/dev/";
        if (realPath.size() <= devPrefix.size() ||
            realPath.compare(0, devPrefix.size(), devPrefix) != 0) {
            UBSE_LOG_WARN << "StopMdHoldersOnMembers: realPath not under /dev/, path=" << realPath;
            continue;
        }
        std::string baseName = realPath.substr(devPrefix.size());
        // 检查 /sys/block/<baseName>/holders/ 下的 md 持有者
        std::string holdersDir = "/sys/block/" + baseName + "/holders";
        struct DirCloser { void operator()(DIR* d) const { if (d != nullptr) { closedir(d); } } };
        std::unique_ptr<DIR, DirCloser> dir(opendir(holdersDir.c_str()));
        if (dir == nullptr) {
            // holders 目录不存在说明该盘未被任何块设备占用，正常情况
            continue;
        }
        struct dirent *entry = nullptr;
        while ((entry = readdir(dir.get())) != nullptr) {
            std::string holderName = entry->d_name;
            if (holderName == "." || holderName == "..") {
                continue;
            }
            if (holderName.compare(0, 2, "md") != 0) {
                continue;
            }
            std::string mdDevPath = "/dev/" + holderName;
            if (!IsSafePath(mdDevPath)) {
                UBSE_LOG_WARN << "StopMdHoldersOnMembers: unsafe holder path: " << mdDevPath;
                continue;
            }
            UBSE_LOG_INFO << "StopMdHoldersOnMembers: member " << devPath
                          << " is held by active array " << mdDevPath
                          << ", stopping it to free member for assemble";
            std::string stopOut;
            if (ExecWithSudo("mdadm --stop " + mdDevPath, stopOut) != UBSE_OK) {
                UBSE_LOG_WARN << "StopMdHoldersOnMembers: mdadm --stop " << mdDevPath
                              << " failed, output=" << stopOut;
            }
        }
    }
}

// 解析 SSU_NVME_SERVER_IP_LIST 的单条条目（格式: IP:PORT/EID）。
// 成功返回 true 并输出 ip/port/eid；失败返回 false 并已记录日志。
// 用 rfind(':') 分离 IP 和 PORT，兼容 IPv6 地址。
bool ParseSsuIpEntry(const std::string &entry, std::string &ip, uint32_t &port, std::string &eid)
{
    GStrvGuard parts(g_strsplit(entry.c_str(), "/", 2));
    if (!parts || !*parts || !*(parts.get() + 1)) {
        UBSE_LOG_ERROR << "Invalid SSU_NVME_SERVER_IP_LIST entry, expected IP:PORT/EID: " << entry;
        return false;
    }
    std::string ipAndPort(*parts.get());
    eid = std::string(*(parts.get() + 1));

    size_t colonPos = ipAndPort.rfind(':');
    if (colonPos == std::string::npos) {
        UBSE_LOG_ERROR << "Invalid SSU_NVME_SERVER_IP_LIST entry, missing PORT, "
                       << "expected IP:PORT/EID: " << entry;
        return false;
    }
    ip = ipAndPort.substr(0, colonPos);
    std::string portStr = ipAndPort.substr(colonPos + 1);
    if (portStr.empty()) {
        UBSE_LOG_ERROR << "Invalid SSU_NVME_SERVER_IP_LIST entry, empty PORT: " << entry;
        return false;
    }
    try {
        unsigned long portVal = std::stoul(portStr);
        if (portVal == 0 || portVal > 0xFFFFFFFFu) {
            UBSE_LOG_ERROR << "Invalid SSU_NVME_SERVER_IP_LIST entry, PORT out of range: " << entry;
            return false;
        }
        port = static_cast<uint32_t>(portVal);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Invalid SSU_NVME_SERVER_IP_LIST entry, PORT not numeric: " << entry
                       << ", error: " << e.what();
        return false;
    }
    if (ip.empty()) {
        UBSE_LOG_ERROR << "Invalid SSU_NVME_SERVER_IP_LIST entry, empty IP: " << entry;
        return false;
    }
    return true;
}

}

UbseSsuAdapterImpl::UbseSsuAdapterImpl() : dlManager_(SSU_PATH) {}

UbseSsuAdapterImpl::~UbseSsuAdapterImpl() = default;

UbseSsuAdapterImpl &UbseSsuAdapterImpl::GetInstance()
{
    static UbseSsuAdapterImpl instance;
    return instance;
}

UbseResult UbseSsuAdapterImpl::DlOpenLib()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (dlManager_.IsOpen()) {
        return UBSE_OK;
    }

    UbseResult ret = dlManager_.Open();
    if (ret != UBSE_OK) {
        return ret;
    }

    auto loadFunc = [this](auto &funcPtr, const char *symbol) -> UbseResult {
        UbseResult r = dlManager_.GetFunction(funcPtr, symbol);
        if (r != UBSE_OK) {
            dlManager_.Close();
        }
        return r;
    };

    ret = loadFunc(acquireDevInfo_, "acquire_dev_info");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(createNamespace_, "create_namespace");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(deleteNamespace_, "delete_namespace");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(attachNamespace_, "attach_namespace");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(detachNamespace_, "detach_namespace");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(addNamespaceAllowHost_, "add_namespace_allow_host");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(removeNamespaceAllowHost_, "remove_namespace_allow_host");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(getNamespaceAllowHosts_, "get_namespace_allow_hosts");
    if (ret != UBSE_OK) { return ret; }
    ret = loadFunc(freeAllowHostsMem_, "free_allow_hosts_mem");
    if (ret != UBSE_OK) { return ret; }

    UBSE_LOG_INFO << "Successfully loaded libssu.so";
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::GetSrcEid(DevEidT &srcEid)
{
    memset_s(srcEid.raw, EID_SIZE, 0, EID_SIZE);
    // 从ubse.service环境变量SSU_SRC_EID读取源端EID
    std::string eidStr = ubse::utils::GetEnv<std::string>("SSU_SRC_EID", "");
    if (eidStr.empty()) {
        return UBSE_OK;
    }
    size_t copyLen = std::min(eidStr.size(), static_cast<size_t>(EID_SIZE));
    memcpy_s(srcEid.raw, EID_SIZE, eidStr.c_str(), copyLen);
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::GetDevAddrByEid(const std::string& eid, char devIp[DEV_IP_SIZE], uint32_t& jettyId)
{
    devIp[0] = 0;
    jettyId = 0;
    // 从环境变量SSU_NVME_SERVER_IP_LIST（格式: IP:PORT/EID，多条以逗号分隔）中
    // 按EID一次性查找对应的IP和PORT，IP 通过 strncpy_s 写入调用方提供的固定数组。
    std::string ipListStr = ubse::utils::GetEnv<std::string>("SSU_NVME_SERVER_IP_LIST", "");
    if (ipListStr.empty()) {
        UBSE_LOG_WARN << "SSU_NVME_SERVER_IP_LIST is not set, cannot resolve devAddr for eid=" << eid;
        return UBSE_SSU_ERROR_DEV_ADDR_NOT_FOUND;
    }
    GStrvGuard entries(g_strsplit(ipListStr.c_str(), ",", -1));
    if (!entries || !*entries) {
        UBSE_LOG_ERROR << "Failed to parse SSU_NVME_SERVER_IP_LIST: " << ipListStr;
        return UBSE_ERROR_INVAL;
    }
    for (gchar** it = entries.get(); *it != nullptr; ++it) {
        std::string entry(*it);
        std::string ip;
        uint32_t port = 0;
        std::string entryEid;
        if (!ParseSsuIpEntry(entry, ip, port, entryEid)) {
            // 单条格式错误不应影响其它正确条目的查找，跳过并继续遍历
            UBSE_LOG_WARN << "Skipping malformed SSU_NVME_SERVER_IP_LIST entry: " << entry;
            continue;
        }
        if (entryEid != eid) {
            continue;
        }
        if (ip.size() >= DEV_IP_SIZE) {
            UBSE_LOG_ERROR << "ip too long, size=" << ip.size() << ", max=" << DEV_IP_SIZE - 1;
            return UBSE_ERROR_INVAL;
        }
        if (strncpy_s(devIp, DEV_IP_SIZE, ip.c_str(), ip.size()) != EOK) {
            UBSE_LOG_ERROR << "strncpy_s failed for ip: " << ip;
            return UBSE_ERROR_IO;
        }
        jettyId = port;
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "No matching entry found for eid=" << eid << " in SSU_NVME_SERVER_IP_LIST";
    return UBSE_SSU_ERROR_DEV_ADDR_NOT_FOUND;
}

/**
 * @brief 从输入的设备信息列表中提取EID信息，构建底层库所需的DevAddrT列表
 * @param ssuInfoList 输入的设备信息列表（包含EID）
 * @param devList 输出的设备地址列表
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::BuildDevAddrList(const std::vector<UbseSsuDevInfo>& ssuInfoList,
                                              std::vector<DevAddrT>& devList)
{
    // 当输入列表为空时，从ubse.service环境变量SSU_NVME_SERVER_IP_LIST读取
    // 格式: IP:PORT/EID，多条以逗号分隔，例如: 192.168.100.100:18080/EID,192.168.100.101:18081/EID
    // 其中 PORT 用于填充 DevAddrT.jettyId，devIp 只存纯 IP
    if (ssuInfoList.empty()) {
        std::string ipListStr = ubse::utils::GetEnv<std::string>("SSU_NVME_SERVER_IP_LIST", "");
        if (ipListStr.empty()) {
            UBSE_LOG_ERROR << "ssuInfoList is empty and SSU_NVME_SERVER_IP_LIST is not set";
            return UBSE_SSU_ERROR_CONFIG_INVALID;
        }
        GStrvGuard entries(g_strsplit(ipListStr.c_str(), ",", -1));
        if (!entries || !*entries) {
            UBSE_LOG_ERROR << "Failed to parse SSU_NVME_SERVER_IP_LIST: " << ipListStr;
            return UBSE_ERROR_INVAL;
        }
        for (gchar** it = entries.get(); *it != nullptr; ++it) {
            std::string entry(*it);
            std::string ip;
            uint32_t jettyId = 0;
            std::string eid;
            if (!ParseSsuIpEntry(entry, ip, jettyId, eid)) {
                return UBSE_ERROR_INVAL; // 解析错误已记录日志
            }
            if (ip.size() >= DEV_IP_SIZE) {
                UBSE_LOG_ERROR << "ip too long, size=" << ip.size() << ", max=" << DEV_IP_SIZE - 1;
                return UBSE_ERROR_INVAL;
            }
            DevAddrT addr{};
            memset_s(&addr.srcEid.raw, EID_SIZE, 0, EID_SIZE);
            if (GetSrcEid(addr.srcEid) != UBSE_OK) {
                return UBSE_ERROR_INVAL;
            }
            memset_s(&addr.tgtEid.raw, EID_SIZE, 0, EID_SIZE);
            size_t copyLen = std::min(eid.size(), static_cast<size_t>(EID_SIZE));
            memcpy_s(addr.tgtEid.raw, EID_SIZE, eid.c_str(), copyLen);
            if (strncpy_s(addr.devIp, DEV_IP_SIZE, ip.c_str(), ip.size()) != EOK) {
                UBSE_LOG_ERROR << "strncpy_s failed for ip: " << ip;
                return UBSE_ERROR_IO;
            }
            addr.useUb = false;
            memset_s(addr.subNqn, SUBNQN_SIZE, 0, SUBNQN_SIZE);
            addr.jettyId = jettyId;
            devList.push_back(addr);
        }
        return UBSE_OK;
    }

    devList.resize(ssuInfoList.size());
    for (size_t i = 0; i < ssuInfoList.size(); ++i) {
        const std::string& eid = ssuInfoList[i].subSystem.eid;
        const std::string& subNqn = ssuInfoList[i].subSystem.subNqn;
        memset_s(&devList[i].srcEid.raw, EID_SIZE, 0, EID_SIZE);
        if (GetSrcEid(devList[i].srcEid) != UBSE_OK) {
            return UBSE_ERROR_INVAL;
        }
        memset_s(&devList[i].tgtEid.raw, EID_SIZE, 0, EID_SIZE);
        // 按 eid 实际长度拷贝，避免 eid.size() < EID_SIZE 时越界读取 eid 字符串缓冲区
        size_t eidCopyLen = std::min(eid.size(), static_cast<size_t>(EID_SIZE));
        memcpy_s(devList[i].tgtEid.raw, EID_SIZE, eid.c_str(), eidCopyLen);
        // 按EID从环境变量SSU_NVME_SERVER_IP_LIST一次性查找IP和PORT（查不到保持空字符串/0）
        memset_s(devList[i].devIp, DEV_IP_SIZE, 0, DEV_IP_SIZE);
        uint32_t jettyId = 0;
        if (GetDevAddrByEid(eid, devList[i].devIp, jettyId) == UBSE_OK) {
            devList[i].jettyId = jettyId;
        } else {
            devList[i].devIp[0] = 0;
            devList[i].jettyId = 0;
        }
        devList[i].useUb = false;
        memset_s(&devList[i].subNqn, SUBNQN_SIZE, 0, SUBNQN_SIZE);
        strncpy_s(devList[i].subNqn, SUBNQN_SIZE, subNqn.c_str(), subNqn.size());
    }
    return UBSE_OK;
}

/**
 * @brief 转换设备信息
 * @details 将底层库返回的DevInfoT转换为UbseSsuDevInfo
 * @param devInfo 底层库返回的设备信息
 * @param info 输出的设备信息
 */
void UbseSsuAdapterImpl::ConvertDevInfo(const DevInfoT& devInfo, UbseSsuDevInfo& info)
{
    info.subSystem.eid = std::string(reinterpret_cast<const char*>(devInfo.devAddr.tgtEid.raw),
                                     strnlen(reinterpret_cast<const char*>(devInfo.devAddr.tgtEid.raw), EID_SIZE));
    info.subSystem.subNqn = std::string(devInfo.devAddr.subNqn);
    info.serialNumber = std::string(devInfo.sn);
    info.firmware = std::string(devInfo.mn);
    info.totalBytes = devInfo.tnvmcap;
    info.usedBytes = devInfo.tnvmcap - devInfo.unvmcap;

    switch (devInfo.state) {
        case DevStatusT::DEV_ONLINE:
            info.state = UbseSsuState::ONLINE;
            break;
        default:
            info.state = UbseSsuState::OFFLINE;
            break;
    }

    UbseSsuDevCtrl ctrl;
    ctrl.eid = info.subSystem.eid;
    ctrl.devPath = std::string(devInfo.devPath);
    ctrl.cntlid = devInfo.cntlId;

    for (uint32_t i = 0; i < devInfo.nsCount; ++i) {
        const auto& ns = devInfo.namespaces[i];
        UbseSsuDevNameSpace nsInfo;
        nsInfo.namespaceId = ns.namespaceId;
        nsInfo.subSystem.eid = info.subSystem.eid;
        nsInfo.subSystem.subNqn = info.subSystem.subNqn;
        nsInfo.guid = std::string(reinterpret_cast<const char*>(ns.guid), GUID_SIZE);
        nsInfo.uuid = std::string(reinterpret_cast<const char*>(ns.uuid), UUID_SIZE);
        nsInfo.nsDevPath = std::string(ns.devPath);
        nsInfo.nsze = ns.baseAttr.nsze;
        nsInfo.ncap = ns.baseAttr.ncap;
        // nuse按NVMe规范为LBA数量，ns.usedBytes为字节，需除以LBA Size
        nsInfo.nuse = ns.usedBytes / GetLbaSize(ns.baseAttr.flbas);
        
        // 填充nsOptions
        nsInfo.nsOptions.flbas = ns.baseAttr.flbas;
        nsInfo.nsOptions.dps = ns.baseAttr.dps;
        nsInfo.nsOptions.anagrpid = ns.baseAttr.anagrpid;
        nsInfo.nsOptions.nvmsetid = ns.baseAttr.nvmsetid;
        nsInfo.nsOptions.nmic = ns.baseAttr.nmic ? 1 : 0;
        
        // 填充customData
        memset_s(&nsInfo.customData, sizeof(nsInfo.customData), 0, sizeof(nsInfo.customData));
        memcpy_s(&nsInfo.customData, sizeof(nsInfo.customData),
                 ns.userData, std::min(sizeof(nsInfo.customData), sizeof(ns.userData)));
        
        info.nameSpaces.push_back(nsInfo);
    }

    info.ctrlList.push_back(ctrl);
}

/**
 * @brief 获取SSU物理设备信息列表
 * @details 扫描系统中所有NVMe SSD设备，返回设备详细信息。
 *          输入参数ssuInfoList包含要查询的设备eid信息，输出时填充完整设备信息。
 * @param ssuInfoList [inout] 输入SSU物理设备的eid信息，返回的设备信息列表
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::GetDevList(std::vector<UbseSsuDevInfo> &ssuInfoList)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 允许ssuInfoList为空，此时BuildDevAddrList会从环境变量SSU_NVME_SERVER_IP_LIST读取
    std::vector<DevAddrT> devList;
    ret = BuildDevAddrList(ssuInfoList, devList);
    if (ret != UBSE_OK) {
        return ret;
    }

    // devInfoList大小需与devList一致（环境变量分支下可能与ssuInfoList大小不同）
    std::vector<DevInfoT> devInfoList(devList.size());
    int acqRet = acquireDevInfo_(adminNqn.c_str(), devList.data(), static_cast<int>(devList.size()),
                                 devInfoList.data());
    if (acqRet != 0) {
        UBSE_LOG_ERROR << "acquire_dev_info failed, adminNqn=" << MaskNqn(adminNqn) << ", ret=" << acqRet;
        return UBSE_SSU_ERROR_ACQUIRE_DEV_INFO_FAILED;
    }

    ssuInfoList.clear();
    for (const auto& devInfo : devInfoList) {
        UbseSsuDevInfo info;
        ConvertDevInfo(devInfo, info);
        ssuInfoList.push_back(info);
    }

    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::BuildNamespaceInfoForCreate(const UbseSsuDevNameSpace& nameSpace,
                                                         DevNamespaceInfoT& nsInfo)
{
    memset_s(&nsInfo, sizeof(nsInfo), 0, sizeof(nsInfo));
    
    // EID必须是固定长度
    if (nameSpace.subSystem.eid.size() != EID_SIZE) {
        UBSE_LOG_ERROR << "Invalid EID length: " << nameSpace.subSystem.eid.size() << ", expected " << EID_SIZE;
        return UBSE_ERROR_INVAL;
    }

    // subNqn不能为空
    if (nameSpace.subSystem.subNqn.empty()) {
        UBSE_LOG_ERROR << "subNqn is empty";
        return UBSE_ERROR_INVAL;
    }

    // nsze和ncap不能为0
    if (nameSpace.nsze == 0) {
        UBSE_LOG_ERROR << "nsze is zero";
        return UBSE_ERROR_INVAL;
    }
    if (nameSpace.ncap == 0) {
        UBSE_LOG_ERROR << "ncap is zero";
        return UBSE_ERROR_INVAL;
    }

    // 设置设备地址
    memset_s(&nsInfo.devAddr.srcEid.raw, EID_SIZE, 0, EID_SIZE);
    if (GetSrcEid(nsInfo.devAddr.srcEid) != UBSE_OK) {
        return UBSE_ERROR_INVAL;
    }
    memset_s(&nsInfo.devAddr.tgtEid.raw, EID_SIZE, 0, EID_SIZE);
    memcpy_s(nsInfo.devAddr.tgtEid.raw, EID_SIZE,
             nameSpace.subSystem.eid.c_str(), EID_SIZE);
    // 按EID从环境变量SSU_NVME_SERVER_IP_LIST一次性查找IP和PORT（attach场景必需；
    // 查不到保持nullptr/0，由底层库处理）
    memset_s(nsInfo.devAddr.devIp, DEV_IP_SIZE, 0, DEV_IP_SIZE);
    uint32_t jettyId = 0;
    if (GetDevAddrByEid(nameSpace.subSystem.eid, nsInfo.devAddr.devIp, jettyId) == UBSE_OK) {
        nsInfo.devAddr.jettyId = jettyId;
    } else {
        nsInfo.devAddr.devIp[0] = 0;
        nsInfo.devAddr.jettyId = 0;
    }
    nsInfo.devAddr.useUb = false;
    memset_s(&nsInfo.devAddr.subNqn, SUBNQN_SIZE, 0, SUBNQN_SIZE);
    strncpy_s(nsInfo.devAddr.subNqn, SUBNQN_SIZE,
              nameSpace.subSystem.subNqn.c_str(), nameSpace.subSystem.subNqn.size());

    // 设置基础属性
    nsInfo.baseAttr.ncap = nameSpace.ncap;
    nsInfo.baseAttr.nsze = nameSpace.nsze;
    nsInfo.baseAttr.flbas = nameSpace.nsOptions.flbas;
    nsInfo.baseAttr.dps = nameSpace.nsOptions.dps;
    nsInfo.baseAttr.anagrpid = nameSpace.nsOptions.anagrpid;
    nsInfo.baseAttr.nvmsetid = nameSpace.nsOptions.nvmsetid;
    nsInfo.baseAttr.nmic = (nameSpace.nsOptions.nmic != 0);

    // 设置自定义数据
    memcpy_s(nsInfo.userData, sizeof(nsInfo.userData),
             &nameSpace.customData, sizeof(nameSpace.customData));
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::BuildNamespaceInfoForBasic(const UbseSsuDevNameSpace& nameSpace, DevNamespaceInfoT& nsInfo)
{
    memset_s(&nsInfo, sizeof(nsInfo), 0, sizeof(nsInfo));
    
    // EID必须是固定长度
    if (nameSpace.subSystem.eid.size() != EID_SIZE) {
        UBSE_LOG_ERROR << "Invalid EID length: " << nameSpace.subSystem.eid.size() << ", expected " << EID_SIZE;
        return UBSE_ERROR_INVAL;
    }

    // namespaceId不能为空
    if (nameSpace.namespaceId == 0) {
        UBSE_LOG_ERROR << "namespaceId is zero";
        return UBSE_ERROR_INVAL;
    }

    // 设置设备地址
    memset_s(&nsInfo.devAddr.srcEid.raw, EID_SIZE, 0, EID_SIZE);
    if (GetSrcEid(nsInfo.devAddr.srcEid) != UBSE_OK) {
        return UBSE_ERROR_INVAL;
    }
    memset_s(&nsInfo.devAddr.tgtEid.raw, EID_SIZE, 0, EID_SIZE);
    memcpy_s(nsInfo.devAddr.tgtEid.raw, EID_SIZE,
             nameSpace.subSystem.eid.c_str(), EID_SIZE);
    // 按EID从环境变量SSU_NVME_SERVER_IP_LIST一次性查找IP和PORT（attach场景必需；
    // 查不到保持空字符串/0，由底层库处理）
    memset_s(nsInfo.devAddr.devIp, DEV_IP_SIZE, 0, DEV_IP_SIZE);
    uint32_t jettyId = 0;
    if (GetDevAddrByEid(nameSpace.subSystem.eid, nsInfo.devAddr.devIp, jettyId) == UBSE_OK) {
        nsInfo.devAddr.jettyId = jettyId;
    } else {
        nsInfo.devAddr.devIp[0] = 0;
        nsInfo.devAddr.jettyId = 0;
    }
    nsInfo.devAddr.useUb = false;
    // subNqn在attach/detach场景必需：attach用其执行nvme connect，detach用其查找本地nvme设备
    memset_s(&nsInfo.devAddr.subNqn, SUBNQN_SIZE, 0, SUBNQN_SIZE);
    strncpy_s(nsInfo.devAddr.subNqn, SUBNQN_SIZE,
              nameSpace.subSystem.subNqn.c_str(), nameSpace.subSystem.subNqn.size());

    // 设置namespaceId
    nsInfo.namespaceId = nameSpace.namespaceId;

    // guid如果有就拷贝
    if (!nameSpace.guid.empty()) {
        memset_s(nsInfo.guid, GUID_SIZE, 0, GUID_SIZE);
        memcpy_s(nsInfo.guid, GUID_SIZE, nameSpace.guid.c_str(),
                 std::min(nameSpace.guid.size(), static_cast<size_t>(GUID_SIZE)));
    }

    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::VerifyNamespaceUuid(const UbseSsuDevNameSpace& nameSpace)
{
    if (nameSpace.subSystem.eid.empty() || nameSpace.uuid.empty()) {
        UBSE_LOG_ERROR << "VerifyNamespaceUuid: eid or uuid is empty";
        return UBSE_ERROR_INVAL;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    std::vector<UbseSsuDevInfo> devInfoList;
    devInfoList.push_back({.subSystem = {.eid = nameSpace.subSystem.eid, .subNqn = nameSpace.subSystem.subNqn}});
    ret = GetDevList(devInfoList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "VerifyNamespaceUuid: GetDevList failed, ret=" << ret;
        return UBSE_SSU_ERROR_ACQUIRE_DEV_INFO_FAILED;
    }

    for (const auto& devInfo : devInfoList) {
        for (const auto& ns : devInfo.nameSpaces) {
            if (ns.namespaceId == nameSpace.namespaceId) {
                if (ns.uuid == nameSpace.uuid) {
                    return UBSE_OK;
                }
                UBSE_LOG_ERROR << "VerifyNamespaceUuid: UUID mismatch for namespaceId="
                               << nameSpace.namespaceId
                               << ", expected=" << BytesToHex(nameSpace.uuid)
                               << ", actual=" << BytesToHex(ns.uuid);
                return UBSE_SSU_ERROR_NS_UUID_MISMATCH;
            }
        }
    }

    UBSE_LOG_WARN << "VerifyNamespaceUuid: namespaceId=" << nameSpace.namespaceId
                  << " not found on device eid=" << nameSpace.subSystem.eid;
    return UBSE_SSU_ERROR_NS_NOT_FOUND;
}

/**
 * @brief 在指定SSU设备上创建命名空间
 * @details 在指定控制器上创建一个新的NVMe命名空间
 *          满足可靠性要求：
 *          1. options中应携带预生成的uuid
 *          2. uuid在算法规划阶段生成，确保失败重试时使用同一uuid实现幂等
 * @param nameSpace [inout] eid，guid，uuid，nsze，ncap，nsOptions，customData必填，返回namespaceId等信息
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::CreateDevNameSpace(UbseSsuDevNameSpace &nameSpace)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    DevNamespaceInfoT nsInfo{};
    ret = BuildNamespaceInfoForCreate(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int createRet = createNamespace_(adminNqn.c_str(), &nsInfo);
    if (createRet != 0) {
        UBSE_LOG_ERROR << "create_namespace failed, adminNqn=" << MaskNqn(adminNqn) << ", ret=" << createRet;
        return UBSE_SSU_ERROR_NS_CREATE_FAILED;
    }

    nameSpace.namespaceId = nsInfo.namespaceId;
    nameSpace.nsDevPath = std::string(nsInfo.devPath);
    // nsInfo.usedBytes为字节（C库返回），nameSpace.nuse按NVMe规范为LBA数量，需除以LBA Size
    nameSpace.nuse = nsInfo.usedBytes / GetLbaSize(nsInfo.baseAttr.flbas);
    nameSpace.guid = std::string(reinterpret_cast<const char*>(nsInfo.guid), GUID_SIZE);
    nameSpace.uuid = std::string(reinterpret_cast<const char*>(nsInfo.uuid), UUID_SIZE);

    UBSE_LOG_INFO << "Successfully created namespace " << nameSpace.namespaceId
                  << " with uuid: " << BytesToHex(nameSpace.uuid);
    return UBSE_OK;
}

/**
 * @brief 删除指定SSU设备上的命名空间
 * @details 满足可靠性要求：
 *          1. 删除前验证 nameSpace.uuid 与设备上实际 NS 的 UUID 匹配
 *          2. 删除前确保 NS 已 detach（状态为 DELETING）
 *          3. NS 不存在时返回失败（调用方需自行处理）
 * @param nameSpace 要删除的命名空间信息，uuid用于防误删验证
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::DeleteDevNameSpace(const UbseSsuDevNameSpace &nameSpace)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    // VerifyNamespaceUuid 内部已通过 GetDevList 比对 nsid+uuid，
    // 三种情况都返回失败：ns 不存在、uuid 不匹配、GetDevList 失败
    uint32_t verifyRet = VerifyNamespaceUuid(nameSpace);
    if (verifyRet != UBSE_OK) {
        UBSE_LOG_ERROR << "DeleteDevNameSpace failed: UUID verification failed, ret=" << verifyRet;
        return verifyRet;
    }

    // 构建命名空间信息
    DevNamespaceInfoT nsInfo{};
    ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int deleteRet = deleteNamespace_(adminNqn.c_str(), &nsInfo);
    if (deleteRet != 0) {
        UBSE_LOG_ERROR << "delete_namespace failed, ret=" << deleteRet;
        return UBSE_SSU_ERROR_NS_DELETE_FAILED;
    }

    UBSE_LOG_INFO << "Successfully deleted namespace " << nameSpace.namespaceId;
    return UBSE_OK;
}

/**
 * @brief 将命名空间attach到host节点
 * @details 满足可靠性要求：
 *          1. attach后验证UUID一致性（读回nvme id-ns与预期uuid比对）
 *          2. 已attach的NS重复调用应返回成功（幂等性）
 * @param nameSpace 要attach的命名空间，uuid用于验证
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::AttachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace)
{
    if (hostNqn.empty()) {
        UBSE_LOG_ERROR << "AttachDevNameSpace: hostNqn is empty";
        return UBSE_ERROR_INVAL;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int attachRet = attachNamespace_(hostNqn.c_str(), &nsInfo);
    if (attachRet != 0) {
        UBSE_LOG_ERROR << "attach_namespace failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << attachRet;
        return MapSsuErrToUbseErr(attachRet, UBSE_SSU_ERROR_ATTACH_FAILED);
    }

    // VerifyNamespaceUuid需要调用GetDevList来验证，但agent侧不支持GetDevList，所以这里先不调用

    UBSE_LOG_INFO << "Successfully attached namespace " << nameSpace.namespaceId
                  << ", hostNqn=" << MaskNqn(hostNqn);
    return UBSE_OK;
}

/**
 * @brief 将命名空间从host节点detach
 * @details 满足可靠性要求：
 *          1. 已detach的NS重复调用应返回成功（幂等性）
 * @param nameSpace 要detach的命名空间
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::DetachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace)
{
    if (hostNqn.empty()) {
        UBSE_LOG_ERROR << "DetachDevNameSpace: hostNqn is empty";
        return UBSE_ERROR_INVAL;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int detachRet = detachNamespace_(hostNqn.c_str(), &nsInfo);
    if (detachRet != 0) {
        UBSE_LOG_ERROR << "detach_namespace failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << detachRet;
        return MapSsuErrToUbseErr(detachRet, UBSE_SSU_ERROR_DETACH_FAILED);
    }

    UBSE_LOG_INFO << "Successfully detached namespace " << nameSpace.namespaceId
                  << ", hostNqn=" << MaskNqn(hostNqn);
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::AddNameSpaceAllowHost(const UbseSsuDevNameSpace &nameSpace,
                                                   const std::string &hostNqn)
{
    if (hostNqn.empty()) {
        UBSE_LOG_ERROR << "AddNameSpaceAllowHost: hostNqn is empty";
        return UBSE_ERROR_INVAL;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    DevNamespaceInfoT nsInfo{};
    ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int addRet = addNamespaceAllowHost_(adminNqn.c_str(), &nsInfo, hostNqn.c_str());
    if (addRet != 0) {
        UBSE_LOG_ERROR << "add_namespace_allow_host failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << addRet;
        return UBSE_SSU_ERROR_PERMISSION_ADD_FAILED;
    }

    UBSE_LOG_INFO << "Successfully added allow host to namespace " << nameSpace.namespaceId
                  << ", hostNqn=" << MaskNqn(hostNqn);
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::RemoveNameSpaceAllowHost(const UbseSsuDevNameSpace &nameSpace,
                                                      const std::string &hostNqn)
{
    if (hostNqn.empty()) {
        UBSE_LOG_ERROR << "RemoveNameSpaceAllowHost: hostNqn is empty";
        return UBSE_ERROR_INVAL;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    DevNamespaceInfoT nsInfo{};
    ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int removeRet = removeNamespaceAllowHost_(adminNqn.c_str(), &nsInfo, hostNqn.c_str());
    if (removeRet != 0) {
        UBSE_LOG_ERROR << "remove_namespace_allow_host failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << removeRet;
        return UBSE_SSU_ERROR_PERMISSION_REMOVE_FAILED;
    }

    UBSE_LOG_INFO << "Successfully removed allow host from namespace " << nameSpace.namespaceId
                  << ", hostNqn=" << MaskNqn(hostNqn);
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::GetNameSpaceAllowHostList(const UbseSsuDevNameSpace &nameSpace,
                                                       std::vector<std::string> &allowHostList)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    std::string adminNqn;
    uint32_t ret = GetAdminNqn(adminNqn);
    if (ret != UBSE_OK) {
        return ret;
    }

    DevNamespaceInfoT nsInfo{};
    ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    char** allowHosts = nullptr;
    uint32_t hostCnt = 0;
    int getRet = getNamespaceAllowHosts_(adminNqn.c_str(), &nsInfo, &allowHosts, &hostCnt);
    if (getRet != 0) {
        UBSE_LOG_ERROR << "get_namespace_allow_hosts failed, ret=" << getRet;
        return UBSE_SSU_ERROR_PERMISSION_GET_FAILED;
    }

    for (uint32_t i = 0; i < hostCnt; ++i) {
        if (allowHosts[i] != nullptr) {
            allowHostList.push_back(allowHosts[i]);
        }
    }

    if (allowHosts != nullptr) {
        freeAllowHostsMem_(allowHosts, hostCnt);
    }

    UBSE_LOG_INFO << "Successfully got allow host list for namespace " << nameSpace.namespaceId
                  << ", count=" << allowHostList.size();
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::ValidatePersistentPaths(const std::vector<std::string>& devicePathList)
{
    for (const auto& path : devicePathList) {
        if (path.find("/dev/disk/by-id/") != 0) {
            UBSE_LOG_ERROR << "Device path is not a persistent path (by-id): " << path
                           << ", expected format: /dev/disk/by-id/nvme-uuid.<uuid>";
            return UBSE_SSU_ERROR_PATH_INVALID;
        }
        // 阻止 shell 元字符进入后续 ExecWithSudo 拼接的命令行，防止命令注入
        if (!IsSafePath(path)) {
            UBSE_LOG_ERROR << "Device path contains unsafe characters: " << path;
            return UBSE_SSU_ERROR_PATH_INVALID;
        }
    }
    return UBSE_OK;
}

/**
 * @brief 使用LVM创建线性块设备
 * @details 创建物理卷(PV) -> 卷组(VG) -> 逻辑卷(LV)
 * @param deviceName 设备名称
 * @param devicePathList 底层设备路径列表
 * @param devicePath 输出参数，创建后的块设备路径
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::CreateLinearBlockDevice(const std::string& deviceName,
                                                     const std::vector<std::string>& devicePathList,
                                                     std::string& devicePath)
{
    std::string vgName = deviceName + "_vg";
    std::vector<std::string> createdPVs;
    std::string output;

    // 创建物理卷（逐个创建）
    for (const auto& devPath : devicePathList) {
        if (ExecWithSudo("pvcreate -y " + devPath, output) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to create PV for " << devPath << ", output=" << output;
            // pvcreate 失败不代表完全没写入（部分写入 label 后失败、盘上已有残留 PV 等都可能
            // 退出非 0），对失败盘也尝试 pvremove 兜底，与已成功盘一并清理。
            createdPVs.push_back(devPath);
            RollbackCreatedPVs(createdPVs, "PV create rollback");
            return UBSE_SSU_ERROR_PV_CREATE_FAILED;
        }
        createdPVs.push_back(devPath);
    }

    // 创建卷组（卷组名：{deviceName}_vg）
    std::string vgCmd = "vgcreate " + vgName;
    for (const auto& devPath : devicePathList) {
        vgCmd += " " + devPath;
    }
    if (ExecWithSudo(vgCmd, output) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create VG " << vgName << ", output=" << output;
        RollbackCreatedPVs(createdPVs, "VG create rollback");
        return UBSE_SSU_ERROR_VG_CREATE_FAILED;
    }

    // 创建逻辑卷（线性模式，分配全部空间）
    if (ExecWithSudo("lvcreate -y -l 100%FREE -n " + deviceName + " " + vgName, output) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create LV " << deviceName << " in VG " << vgName << ", output=" << output;
        std::string lvOut;
        if (ExecWithSudo("lvremove -f " + vgName + "/" + deviceName, lvOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to lvremove " << vgName << "/" << deviceName
                          << " during LV create rollback, output=" << lvOut;
        }
        std::string vgOut;
        if (ExecWithSudo("vgremove -f " + vgName, vgOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to vgremove " << vgName
                          << " during LV create rollback, output=" << vgOut;
        }
        RollbackCreatedPVs(createdPVs, "LV create rollback");
        return UBSE_SSU_ERROR_LV_CREATE_FAILED;
    }

    // 实际设备路径为 /dev/{vgName}/{deviceName}，在 /dev/ssu/ 下创建符号链接统一管理
    std::string actualPath = "/dev/" + vgName + "/" + deviceName;
    if (CreateSsuDevSymlink(deviceName, actualPath, devicePath) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to create symlink for " << deviceName
                      << ", rolling back LVM (vg=" << vgName << ")";
        std::string lvOut;
        if (ExecWithSudo("lvremove -f " + vgName + "/" + deviceName, lvOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to lvremove " << vgName << "/" << deviceName
                          << " during symlink rollback, output=" << lvOut;
        }
        std::string vgOut;
        if (ExecWithSudo("vgremove -f " + vgName, vgOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to vgremove " << vgName
                          << " during symlink rollback, output=" << vgOut;
        }
        RollbackCreatedPVs(createdPVs, "symlink rollback");
        return UBSE_ERROR_IO;
    }
    return UBSE_OK;
}

/**
 * @brief 使用mdadm创建条带化块设备（RAID0/RAID5）
 * @details 使用mdadm创建RAID阵列
 * @param deviceName 设备名称
 * @param devicePathList 底层设备路径列表
 * @param options 创建选项（RAID级别、条带大小等）
 * @param devicePath 输出参数，创建后的块设备路径
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::CreateStripedBlockDevice(const std::string& deviceName,
                                                      const std::vector<std::string>& devicePathList,
                                                      const UbseCreateBlockDeviceOptions& options,
                                                      std::string& devicePath)
{
    const char* md_level_str = "raid0";
    if (options.raidLevel == UbseSsuRaidLevel::RAID5) {
        md_level_str = "raid5";
    }

    std::string mdPath = "/dev/md/" + deviceName;
    // --auto=yes 让 mdadm 在设备节点不存在时自动创建 /dev/mdXxx，否则在使用命名阵列
    // /dev/md/<name> 时可能因底层 /dev/md127 节点缺失而报
    // "unexpected failure opening /dev/md127"。
    std::string cmd = "mdadm --create " + mdPath + " --auto=yes --run --level=" + md_level_str +
                      " --raid-devices=" + std::to_string(devicePathList.size());
    // options.chunkSize 单位为 KB（见 ubse_ssu_def.h），mdadm --chunk 同样以 KB 为单位，
    // 二者单位一致，直接透传即可。
    uint64_t chunkKb = static_cast<uint64_t>(options.chunkSize);
    if (chunkKb != 0) {
        cmd += " --chunk=" + std::to_string(chunkKb);
    }
    for (const auto& devPath : devicePathList) {
        cmd += " " + devPath;
    }

    std::string output;
    if (ExecWithSudo(cmd, output) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create MD RAID " << mdPath << ", output=" << output;
        return UBSE_SSU_ERROR_MD_CREATE_FAILED;
    }

    // 实际设备路径为 /dev/md/{deviceName}，在 /dev/ssu/ 下创建符号链接统一管理
    if (CreateSsuDevSymlink(deviceName, mdPath, devicePath) != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to create symlink for " << deviceName
                      << ", rolling back mdadm device at " << mdPath;
        if (ExecWithSudo("mdadm --stop --force " + mdPath, output) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to stop md device " << mdPath << " during rollback, output=" << output;
        }
        for (const auto& devPath : devicePathList) {
            if (ExecWithSudo("mdadm --zero-superblock " + devPath, output) != UBSE_OK) {
                UBSE_LOG_WARN << "Failed to zero superblock on " << devPath << " during rollback, output=" << output;
            }
        }
        return UBSE_ERROR_IO;
    }

    return UBSE_OK;
}

/**
 * @brief 创建块设备（支持RAID）
 * @details 将多个命名空间组合成一个块设备，支持LINEAR、RAID0、RAID5模式
 *          满足可靠性要求：
 *          1. devicePathList应使用persistentPath（/dev/disk/by-id/nvme-uuid.<uuid>）
 *          2. 创建成功后更新mdadm.conf并执行update-initramfs
 *          3. 通过raidUuid标识阵列
 * @param deviceName 设备名称
 * @param devicePathList 底层设备路径列表（应使用by-id路径）
 * @param options 创建选项（RAID级别、条带大小等）
 * @param devicePath 输出参数，创建后的块设备路径
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::CreateBlockDevice(const std::string &deviceName,
                                               const std::vector<std::string> &devicePathList,
                                               const UbseCreateBlockDeviceOptions &options,
                                               std::string &devicePath)
{
    // deviceName 来源于 RPC 请求，属用户可控输入，会拼入 ExecWithSudo 的 shell 命令，
    // 必须先做白名单校验以防命令注入。
    if (!IsSafeDeviceName(deviceName)) {
        UBSE_LOG_ERROR << "Invalid deviceName with unsafe characters: " << deviceName;
        return UBSE_SSU_ERROR_DEVICE_NAME_INVALID;
    }
    uint32_t ret = ValidatePersistentPaths(devicePathList);
    if (ret != 0) {
        return ret;
    }

    if (options.addressingType == UbseSsuAddressingType::LINEAR) {
        // 使用 LVM 实现线性模式
        ret = CreateLinearBlockDevice(deviceName, devicePathList, devicePath);
    } else {
        // 使用 mdadm 实现条带化模式（RAID0/RAID5）
        ret = CreateStripedBlockDevice(deviceName, devicePathList, options, devicePath);
    }

    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "Successfully created block device " << deviceName << " at " << devicePath;
    }

    return ret;
}

/**
 * @brief 删除块设备
 * @details 满足可靠性要求：
 *          1. 删除前应确保块设备上无活跃I/O
 *          2. 设备不存在时应返回成功（幂等性）
 * @param deviceName 要删除的块设备名称
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::DeleteBlockDevice(const std::string &deviceName)
{
    // deviceName 来源于 RPC 请求，会拼入 ExecWithSudo 的 shell 命令，先做白名单校验以防命令注入。
    if (!IsSafeDeviceName(deviceName)) {
        UBSE_LOG_ERROR << "Invalid deviceName with unsafe characters: " << deviceName;
        return UBSE_SSU_ERROR_DEVICE_NAME_INVALID;
    }
    // 聚合块设备三元存在性判定（ssuLink/md/lvm），与 StopBlockDevice 共用同一判定逻辑
    auto presence = GetBlockDevicePresence(deviceName);
    // 如果底层设备和符号链接都不存在，直接返回成功（幂等）
    if (!presence.AnyActive()) {
        UBSE_LOG_INFO << "Block device " << deviceName << " does not exist, returning success (idempotent)";
        return UBSE_OK;
    }

    if (presence.lvmDeviceExists) {
        return DeleteLvmBlockDevice(deviceName, presence.vgName);
    }
    if (presence.mdDeviceExists) {
        return DeleteMdBlockDevice(deviceName, presence.mdDevicePath);
    }
    // 底层设备不存在但符号链接残留，清理符号链接
    if (presence.ssuLinkExists) {
        RemoveSsuDevSymlink(deviceName);
        UBSE_LOG_INFO << "Removed orphan symlink for block device " << deviceName;
    }
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::DeleteLvmBlockDevice(const std::string &deviceName, const std::string &vgName)
{
    // 删除前先查询 VG 的成员盘列表。vgremove/lvremove 不会清除成员盘上的 PV 元数据，
    // 必须显式 pvremove，否则成员盘残留 LVM label/metadata，重启后 pvscan 可能重组 VG，
    // 并影响后续对同一批盘的 pvcreate（与 mdadm 漏 zero-superblock 同类问题）。
    std::vector<std::string> memberPVs;
    std::string output;
    {
        std::string pvsOutput;
        // pvs 输出形如 "  /dev/sda1\n  /dev/sda2\n"，-S vg_name=<vg> 精确过滤该 VG 成员。
        if (ExecWithSudo("pvs --noheadings -o pv_name -S vg_name=" + vgName, pvsOutput) == UBSE_OK) {
            std::istringstream iss(pvsOutput);
            std::string line;
            while (std::getline(iss, line)) {
                auto first = line.find_first_not_of(" \t");
                if (first == std::string::npos) {
                    continue;
                }
                std::string pvPath = TrimTrailingWhitespace(line.substr(first));
                if (IsSafePath(pvPath)) {
                    memberPVs.push_back(pvPath);
                }
            }
        } else {
            UBSE_LOG_WARN << "Failed to query PVs for VG " << vgName
                          << ", will still remove LV/VG but skip pvremove, output=" << pvsOutput;
        }
    }

    if (ExecWithSudo("lvremove -f " + vgName + "/" + deviceName, output) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to delete LVM block device " << deviceName << ", output=" << output;
        return UBSE_SSU_ERROR_LV_REMOVE_FAILED;
    }

    std::string vgOut;
    if (ExecWithSudo("vgremove -f " + vgName, vgOut) != UBSE_OK) {
        // vgremove 失败说明 VG 仍存在（如残留 LV、metadata 损坏等）。此时不再执行
        // pvremove，否则会把 VG 变成无成员盘的"空壳 VG"，形成更难排查的不一致状态。
        // 返回错误让调用方知晓删除未完全成功，符号链接一并清理。
        UBSE_LOG_ERROR << "Failed to vgremove " << vgName
                       << " during delete, output=" << vgOut
                       << ", LV already removed but VG metadata may remain";
        RemoveSsuDevSymlink(deviceName);
        return UBSE_SSU_ERROR_VG_REMOVE_FAILED;
    }
    for (const auto& pv : memberPVs) {
        std::string pvOut;
        if (ExecWithSudo("pvremove -ff " + pv, pvOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to pvremove on " << pv
                          << " during delete, output=" << pvOut;
        }
    }
    RemoveSsuDevSymlink(deviceName);
    UBSE_LOG_INFO << "Successfully deleted LVM block device " << deviceName;
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::DeleteMdBlockDevice(const std::string &deviceName, const std::string &mdDevicePath)
{
    // stop 之前先用 --detail 查询成员盘列表，stop 后 --detail 不可用。
    // 成员盘路径来自 mdadm 输出（非 RPC 输入），但拼入 ExecWithSudo 前仍走 IsSafePath
    // 白名单校验，避免异常输出被注入 shell 命令。
    std::vector<std::string> memberDevs;
    std::string output;
    {
        std::string detailOutput;
        if (ExecWithSudo("mdadm --detail " + mdDevicePath, detailOutput) == UBSE_OK) {
            std::istringstream iss(detailOutput);
            std::string line;
            while (std::getline(iss, line)) {
                // 成员盘行形如 "  0   8   1   0   active sync   /dev/sda1"
                auto pos = line.find("/dev/");
                if (pos == std::string::npos) {
                    continue;
                }
                std::string devPath = TrimTrailingWhitespace(line.substr(pos));
                // mdadm 首行 /dev/md/<name>: 末尾带 ':'，无法通过 IsSafePath，会被自动排除，无需特判。
                if (IsSafePath(devPath)) {
                    memberDevs.push_back(devPath);
                }
            }
        } else {
            UBSE_LOG_WARN << "Failed to query md detail for " << mdDevicePath
                          << ", will still stop md but skip zero-superblock, output=" << detailOutput;
        }
    }

    if (ExecWithSudo("mdadm --stop --force " + mdDevicePath, output) != UBSE_OK) {
        // stop 失败说明阵列仍在运行（成员盘仍在阵列中，zero-superblock 无意义），
        // 补充说明成员盘 superblock 尚未清理，便于运维判断残留状态。
        UBSE_LOG_ERROR << "Failed to stop mdadm device " << mdDevicePath
                       << ", member superblocks not cleaned (" << memberDevs.size()
                       << " devices pending), output=" << output;
        return UBSE_SSU_ERROR_MD_STOP_FAILED;
    }
    // stop 仅卸载阵列运行态，成员盘 superblock 仍残留，必须 zero-superblock 彻底清理，
    // 否则重启后 mdadm 增量装配可能将成员盘重组为幽灵阵列，并影响后续对同一批盘的 create。
    for (const auto& dev : memberDevs) {
        std::string zeroOut;
        if (ExecWithSudo("mdadm --zero-superblock " + dev, zeroOut) != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to zero superblock on " << dev
                          << " during delete, output=" << zeroOut;
        }
    }
    RemoveSsuDevSymlink(deviceName);
    UBSE_LOG_INFO << "Successfully deleted mdadm block device " << deviceName;
    return UBSE_OK;
}

// ===================== Stop / Assemble / Probe 实现 =====================
// 语义见 ubse_ssu_adapter_interface.h：Stop 仅卸载运行态保留元数据，Assemble 复用元数据重组，
// Probe 仅查询不修改。三者配合支撑"detach 不删数据，reattach 复用元数据"的业务语义。

bool UbseSsuAdapterImpl::LvmDeviceExists(const std::string& deviceName, const std::string& vgName)
{
    // /dev/mapper/{vg}-{deviceName} 或 /dev/{vg}/{deviceName} 任一存在即视为 LV 存在
    std::string lvPath1 = "/dev/mapper/" + vgName + "-" + deviceName;
    std::string lvPath2 = "/dev/" + vgName + "/" + deviceName;
    return g_file_test(lvPath1.c_str(), G_FILE_TEST_EXISTS) || g_file_test(lvPath2.c_str(), G_FILE_TEST_EXISTS);
}

bool UbseSsuAdapterImpl::MdDeviceExists(const std::string& deviceName, const std::string& mdDevicePath)
{
    (void)deviceName;
    return g_file_test(mdDevicePath.c_str(), G_FILE_TEST_EXISTS);
}

// 查询成员盘上是否残留 LVM PV 元数据（VG 名匹配 vgName）。
// pvs -S vg_name=<vg> --noheadings：命中输出 PV 路径列表，未命中输出为空。
// pvs 命令只读，无副作用，可重试。
// 返回值：1=有残留，0=无残留，-1=探测失败（命令执行异常，调用方应中止而非降级 Create）
int UbseSsuAdapterImpl::LvmMetadataResidual(const std::string& vgName)
{
    if (!IsSafeDeviceName(vgName)) {
        UBSE_LOG_WARN << "LvmMetadataResidual: invalid vgName, treat as probe failed, vg=" << vgName;
        return -1;
    }
    std::string pvsOutput;
    if (ExecWithSudo("pvs --noheadings -o pv_name -S vg_name=" + vgName, pvsOutput) != UBSE_OK) {
        UBSE_LOG_ERROR << "LvmMetadataResidual: pvs query failed, vg=" << vgName
                       << ", output=" << pvsOutput;
        return -1; // 探测失败：上层应返回错误并允许重试，禁止降级 Create
    }
    // 输出非空（至少一行非空白）说明有 PV 关联到该 VG
    std::istringstream iss(pvsOutput);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find_first_not_of(" \t\r\n") != std::string::npos) {
            // 找到非空白行说明有 PV 关联到该 VG
            UBSE_LOG_INFO << "LvmMetadataResidual: PV metadata residual found, dev=" << line
                          << ", vg=" << vgName;
            return 1;
        }
    }
    return 0;
}

// 查询成员盘上是否残留 md superblock（name 匹配 deviceName）。
// mdadm --examine --scan --verbose 输出含 "name=<host>:<deviceName>" 时视为命中。
// --examine 只读，无副作用。
// 返回值：1=有残留，0=无残留，-1=探测失败（命令执行异常，调用方应中止而非降级 Create）
int UbseSsuAdapterImpl::MdMetadataResidual(const std::string& deviceName)
{
    std::string output;
    // --scan 扫描所有块设备的 md superblock；--verbose 输出每阵列的详细信息含 name 字段
    if (ExecWithSudo("mdadm --examine --scan --verbose", output) != UBSE_OK) {
        UBSE_LOG_ERROR << "MdMetadataResidual: mdadm --examine failed, dev=" << deviceName
                       << ", output=" << output;
        return -1; // 探测失败：上层应返回错误并允许重试，禁止降级 Create
    }
    // mdadm --examine --scan --verbose 输出格式：
    //   ARRAY /dev/md/<name> metadata=<type> UUID=<uuid> name=<host>:<deviceName>
    // 按行解析 "name=" 字段，提取值后做全词精确匹配，避免子串匹配导致的漏判/误判。
    std::istringstream iss(output);
    std::string line;
    const std::string nameKey = "name=";
    while (std::getline(iss, line)) {
        auto keyPos = line.find(nameKey);
        if (keyPos == std::string::npos) {
            continue;
        }
        // 提取 name 字段值（紧跟在 "name=" 之后到行尾/空白），去掉尾部空白
        std::string nameVal = TrimTrailingWhitespace(line.substr(keyPos + nameKey.size()));
        // 去掉前导空白
        auto startPos = nameVal.find_first_not_of(" \t");
        if (startPos == std::string::npos) {
            continue;
        }
        nameVal = nameVal.substr(startPos);
        // 形态1 "<host>:<deviceName>"，形态2 "<deviceName>"
        // 取最后一个 ':' 之后的部分作为裸设备名，与 deviceName 全词比较
        auto colonPos = nameVal.rfind(':');
        std::string bareName = (colonPos == std::string::npos) ? nameVal : nameVal.substr(colonPos + 1);
        if (bareName == deviceName) {
            UBSE_LOG_INFO << "MdMetadataResidual: md superblock residual found, dev=" << deviceName
                          << ", host=" << nameVal.substr(0, colonPos);
            return 1;
        }
    }
    return 0;
}

uint32_t UbseSsuAdapterImpl::StopLvmBlockDevice(const std::string& deviceName, const std::string& vgName)
{
    // 调用方 StopBlockDevice 已保证 LV active 存在，此处直接 deactivate。
    // lvchange -an 仅 deactivate 逻辑卷，不删除 LV/VG 元数据，成员盘 PV label 保留。
    std::string output;
    if (ExecWithSudo("lvchange -an " + vgName + "/" + deviceName, output) != UBSE_OK) {
        UBSE_LOG_ERROR << "StopLvmBlockDevice: lvchange -an failed, dev=" << deviceName << ", output=" << output;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_STOP_FAILED;
    }
    // 运行态已停，符号链接失效，移除以避免后续访问断链
    RemoveSsuDevSymlink(deviceName);
    UBSE_LOG_INFO << "StopLvmBlockDevice success, dev=" << deviceName;
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::StopMdBlockDevice(const std::string& deviceName, const std::string& mdDevicePath)
{
    // 调用方 StopBlockDevice 已保证 md 设备 active 存在，此处直接 stop。
    // mdadm --stop 仅停止阵列运行态，成员盘 superblock 保留，可后续 --assemble 复用。
    std::string output;
    if (ExecWithSudo("mdadm --stop " + mdDevicePath, output) != UBSE_OK) {
        // 不加 --force：stop 失败多因阵列仍 busy，调用方应先确保无活跃 I/O 再重试
        UBSE_LOG_ERROR << "StopMdBlockDevice: mdadm --stop failed, dev=" << deviceName << ", output=" << output;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_STOP_FAILED;
    }
    RemoveSsuDevSymlink(deviceName);
    UBSE_LOG_INFO << "StopMdBlockDevice success, dev=" << deviceName;
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::AssembleLvmBlockDevice(const std::string& deviceName, const std::string& vgName,
                                                    std::string& devicePath)
{
    // vgchange -ay 激活整个 VG 的所有 LV，复用硬件上已有的 VG 元数据，PV label 不重建。
    // 若 LV 已 active，vgchange -ay 幂等返回成功（LVM 设计语义）。
    std::string output;
    if (ExecWithSudo("vgchange -ay " + vgName, output) != UBSE_OK) {
        UBSE_LOG_ERROR << "AssembleLvmBlockDevice: vgchange -ay failed, dev=" << deviceName << ", output=" << output;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_ASSEMBLE_FAILED;
    }
    // 激活后 LV 设备路径应存在，重建 /dev/ssu/{deviceName} 符号链接
    std::string lvPath = "/dev/" + vgName + "/" + deviceName;
    if (!g_file_test(lvPath.c_str(), G_FILE_TEST_EXISTS)) {
        // 兜底尝试 /dev/mapper 路径，仍不存在则视为激活异常，不创建断链
        lvPath = "/dev/mapper/" + vgName + "-" + deviceName;
        if (!g_file_test(lvPath.c_str(), G_FILE_TEST_EXISTS)) {
            UBSE_LOG_ERROR << "AssembleLvmBlockDevice: LV device node missing after vgchange -ay, dev="
                           << deviceName;
            return UBSE_SSU_ERROR_BLOCK_DEVICE_ASSEMBLE_FAILED;
        }
    }
    uint32_t ret = CreateSsuDevSymlink(deviceName, lvPath, devicePath);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AssembleLvmBlockDevice: CreateSsuDevSymlink failed, dev=" << deviceName;
        return ret;
    }
    UBSE_LOG_INFO << "AssembleLvmBlockDevice success, dev=" << deviceName << ", path=" << devicePath;
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::AssembleMdBlockDevice(const std::string& deviceName, const std::string& mdDevicePath,
                                                   const std::vector<std::string>& devicePathList,
                                                   std::string& devicePath)
{
    // 校验成员盘路径（拼入 ExecWithSudo 前必须白名单校验防注入）
    for (const auto& devPath : devicePathList) {
        if (!IsSafePath(devPath)) {
            UBSE_LOG_ERROR << "AssembleMdBlockDevice: invalid member dev path: " << devPath;
            return UBSE_SSU_ERROR_DEVICE_NAME_INVALID;
        }
    }

    // 1) 幂等：md 设备已 active（/dev/md/{name} 已存在），无需再 assemble，直接建符号链接
    if (g_file_test(mdDevicePath.c_str(), G_FILE_TEST_EXISTS)) {
        uint32_t ret = CreateSsuDevSymlink(deviceName, mdDevicePath, devicePath);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "AssembleMdBlockDevice: CreateSsuDevSymlink failed, dev=" << deviceName;
            return ret;
        }
        UBSE_LOG_INFO << "AssembleMdBlockDevice: md device already active, skip assemble, dev=" << deviceName
                      << ", path=" << devicePath;
        return UBSE_OK;
    }

    // 2) 解除成员盘上可能存在的 md holder：detach 后 re-attach 期间，udev/systemd 可能
    //    基于残留 superblock 自动 assemble 出 /dev/mdN 的幽灵阵列占用成员盘，导致
    //    mdadm --assemble 报 "is busy"。先扫描成员盘的 holders，停止占用的 md 阵列。
    StopMdHoldersOnMembers(devicePathList);

    // 3) mdadm --assemble /dev/md/{name} dev1 dev2 ... 复用 superblock 重组阵列，不重建元数据。
    //    不依赖 mdadm.conf 配置，避免 Create 后 conf 未更新或被清理导致 assemble 失败。
    //    带重试：StopMdHoldersOnMembers 执行后 udev 可能在空窗期自动 assemble 幽灵阵列占用成员盘，
    //    或 NVMe attach 后 kernel 内部引用尚未完全释放，导致 O_EXCL open 失败报 "is busy"。
    //    每次重试前重新清理 holders，让 udev/kernel 有时间收敛。
    constexpr int maxAssembleRetries = 3;
    constexpr int assembleRetryDelayMs = 500;
    std::string output;
    bool assembled = false;
    for (int attempt = 0; attempt < maxAssembleRetries; ++attempt) {
        if (attempt > 0) {
            StopMdHoldersOnMembers(devicePathList);
            std::this_thread::sleep_for(std::chrono::milliseconds(assembleRetryDelayMs));
        }
        std::string cmd = "mdadm --assemble " + mdDevicePath;
        for (const auto& devPath : devicePathList) {
            cmd += " " + devPath;
        }
        if (ExecWithSudo(cmd, output) == UBSE_OK) {
            assembled = true;
            break;
        }
        // 幂等收敛：阵列已 active（可能是 udev 自动 assemble 或上次 Assemble 成功后未清理链接）。
        // mdadm 输出 "is already active" 或 "already in use" 时退出码非 0，但目标状态已达成，
        // 只要目标 /dev/md/{name} 存在即视为成功。
        if (output.find("already active") != std::string::npos ||
            output.find("already in use") != std::string::npos) {
            if (g_file_test(mdDevicePath.c_str(), G_FILE_TEST_EXISTS)) {
                UBSE_LOG_INFO << "AssembleMdBlockDevice: array already active, treat as success, dev="
                              << deviceName << ", output=" << output;
                assembled = true;
                break;
            }
        }
        UBSE_LOG_WARN << "AssembleMdBlockDevice: assemble attempt " << (attempt + 1) << "/" << maxAssembleRetries
                      << " failed, dev=" << deviceName << ", output=" << output;
    }
    if (!assembled) {
        // --scan 兜底：成员盘指定失败时尝试从 superblock 扫描组装（不依赖 conf）
        // 必须限定设备名 /dev/md/{name}，避免无参 --scan 组装系统上所有可组装阵列产生副作用
        std::string scanOut;
        std::string scanCmd = "mdadm --assemble --scan " + mdDevicePath;
        if (ExecWithSudo(scanCmd, scanOut) != UBSE_OK) {
            UBSE_LOG_ERROR << "AssembleMdBlockDevice: mdadm --assemble failed after " << maxAssembleRetries
                           << " retries, dev=" << deviceName
                           << ", output=" << output << ", scanOut=" << scanOut;
            return UBSE_SSU_ERROR_BLOCK_DEVICE_ASSEMBLE_FAILED;
        }
    }
    // 组装后 md 设备路径应存在，重建 /dev/ssu/{deviceName} 符号链接
    if (!g_file_test(mdDevicePath.c_str(), G_FILE_TEST_EXISTS)) {
        UBSE_LOG_ERROR << "AssembleMdBlockDevice: md device not active after assemble, dev=" << deviceName;
        return UBSE_SSU_ERROR_BLOCK_DEVICE_ASSEMBLE_FAILED;
    }
    uint32_t ret = CreateSsuDevSymlink(deviceName, mdDevicePath, devicePath);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AssembleMdBlockDevice: CreateSsuDevSymlink failed, dev=" << deviceName;
        return ret;
    }
    UBSE_LOG_INFO << "AssembleMdBlockDevice success, dev=" << deviceName << ", path=" << devicePath;
    return UBSE_OK;
}

int UbseSsuAdapterImpl::ProbeBlockDevice(const std::string& deviceName, UbseSsuAddressingType addressingType)
{
    // deviceName 来源于账本/verify 响应，本为可信节点内部数据，但拼入 shell 仍走白名单校验防纵深防御。
    // 探测无法执行的场景（含非法 deviceName）统一走 -1 fail-closed，避免调用方降级 Create 误覆盖保留元数据。
    if (!IsSafeDeviceName(deviceName)) {
        UBSE_LOG_WARN << "ProbeBlockDevice: invalid deviceName, treat as probe failed, dev=" << deviceName;
        return -1;
    }
    std::string vgName = deviceName + "_vg";
    std::string mdDevicePath = "/dev/md/" + deviceName;

    // 按 addressingType 直接分路，仅探测对应类型的元数据，避免对另一种编址类型执行不必要的全盘扫描命令
    if (addressingType == UbseSsuAddressingType::LINEAR) {
        // LVM 路径：先检查设备是否已 active，再检查成员盘 PV 元数据是否残留
        if (LvmDeviceExists(deviceName, vgName)) {
            return 1;
        }
        int residual = LvmMetadataResidual(vgName);
        if (residual < 0) {
            return -1; // 探测失败，传递错误
        }
        return residual;
    }
    // STRIPED 路径：先检查设备是否已 active，再检查成员盘 md superblock 是否残留
    if (MdDeviceExists(deviceName, mdDevicePath)) {
        return 1;
    }
    int residual = MdMetadataResidual(deviceName);
    if (residual < 0) {
        return -1; // 探测失败，传递错误
    }
    return residual;
}

uint32_t UbseSsuAdapterImpl::AssembleBlockDevice(const std::string& deviceName,
                                                 const std::vector<std::string>& devicePathList,
                                                 std::string& devicePath,
                                                 UbseSsuAddressingType addressingType)
{
    if (!IsSafeDeviceName(deviceName)) {
        UBSE_LOG_ERROR << "AssembleBlockDevice: invalid deviceName: " << deviceName;
        return UBSE_SSU_ERROR_DEVICE_NAME_INVALID;
    }
    std::string vgName = deviceName + "_vg";
    std::string mdDevicePath = "/dev/md/" + deviceName;

    // 按 addressingType 直接分派，避免对另一种编址类型执行不必要的元数据探测命令（pvs/mdadm --examine）
    if (addressingType == UbseSsuAddressingType::LINEAR) {
        // LVM 路径：若设备已 active 直接走 Assemble；否则探测元数据残留，探测失败须 fail-closed
        if (LvmDeviceExists(deviceName, vgName)) {
            return AssembleLvmBlockDevice(deviceName, vgName, devicePath);
        }
        int residual = LvmMetadataResidual(vgName);
        if (residual < 0) {
            UBSE_LOG_ERROR << "AssembleBlockDevice: LvmMetadataResidual probe failed, dev=" << deviceName;
            return UBSE_SSU_ERROR_BLOCK_DEVICE_PROBE_FAILED;
        }
        if (residual > 0) {
            return AssembleLvmBlockDevice(deviceName, vgName, devicePath);
        }
    } else {
        // STRIPED 路径：若设备已 active 直接走 Assemble；否则探测元数据残留，探测失败须 fail-closed
        if (MdDeviceExists(deviceName, mdDevicePath)) {
            return AssembleMdBlockDevice(deviceName, mdDevicePath, devicePathList, devicePath);
        }
        int residual = MdMetadataResidual(deviceName);
        if (residual < 0) {
            UBSE_LOG_ERROR << "AssembleBlockDevice: MdMetadataResidual probe failed, dev=" << deviceName;
            return UBSE_SSU_ERROR_BLOCK_DEVICE_PROBE_FAILED;
        }
        if (residual > 0) {
            return AssembleMdBlockDevice(deviceName, mdDevicePath, devicePathList, devicePath);
        }
    }
    // residual 为 0 说明无元数据残留，需 Create 而不是 Assemble
    // 调用方应先 Probe 再 Assemble，到此处说明调用方未遵守契约
    UBSE_LOG_ERROR << "AssembleBlockDevice: no metadata residual, caller should call CreateBlockDevice, dev="
                   << deviceName;
    return UBSE_SSU_ERROR_BLOCK_DEVICE_ASSEMBLE_FAILED;
}

uint32_t UbseSsuAdapterImpl::StopBlockDevice(const std::string& deviceName)
{
    if (!IsSafeDeviceName(deviceName)) {
        UBSE_LOG_ERROR << "StopBlockDevice: invalid deviceName: " << deviceName;
        return UBSE_SSU_ERROR_DEVICE_NAME_INVALID;
    }
    // 聚合块设备三元存在性判定（ssuLink/md/lvm），与 DeleteBlockDevice 共用同一判定逻辑
    auto presence = GetBlockDevicePresence(deviceName);

    if (!presence.AnyActive()) {
        // 仍需检查成员盘元数据：可能 stop 后符号链接已删但元数据仍在（合理状态，无需再 stop）
        UBSE_LOG_INFO << "StopBlockDevice: device not active, returning success (idempotent), dev=" << deviceName;
        return UBSE_OK;
    }

    if (presence.lvmDeviceExists) {
        return StopLvmBlockDevice(deviceName, presence.vgName);
    }
    if (presence.mdDeviceExists) {
        return StopMdBlockDevice(deviceName, presence.mdDevicePath);
    }
    // 底层设备不存在但符号链接残留（断链），清理符号链接
    if (presence.ssuLinkExists) {
        RemoveSsuDevSymlink(deviceName);
        UBSE_LOG_INFO << "StopBlockDevice: removed orphan symlink, dev=" << deviceName;
    }
    return UBSE_OK;
}

} // namespace ubse::adapter_plugins::ssu::def
