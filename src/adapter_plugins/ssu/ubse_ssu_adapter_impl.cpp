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
#include <glib.h>
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

std::string MaskNqn(const std::string &nqn)
{
    if (nqn.size() <= NQN_MASK_SUFFIX_LEN) {
        return "****";
    }
    return "****" + nqn.substr(nqn.size() - NQN_MASK_SUFFIX_LEN);
}

uint32_t GetAdminNqn(std::string &adminNqn)
{
    if (ubse::config::UbseGetStr("ubse.ssu", "ssu.adminNqn", adminNqn) != UBSE_OK || adminNqn.empty()) {
        UBSE_LOG_ERROR << "Failed to get ssu.adminNqn from config";
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }
    return UBSE_OK;
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
        return UBSE_ERROR;
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
// 返回UBSE_OK表示命令成功退出（exit code 0），UBSE_ERROR表示失败。
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
        return UBSE_ERROR;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    int status = pclose(pipe.release());
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }
    GStrvGuard entries(g_strsplit(ipListStr.c_str(), ",", -1));
    if (!entries || !*entries) {
        UBSE_LOG_ERROR << "Failed to parse SSU_NVME_SERVER_IP_LIST: " << ipListStr;
        return UBSE_ERROR;
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
            return UBSE_ERROR;
        }
        if (strncpy_s(devIp, DEV_IP_SIZE, ip.c_str(), ip.size()) != EOK) {
            UBSE_LOG_ERROR << "strncpy_s failed for ip: " << ip;
            return UBSE_ERROR;
        }
        jettyId = port;
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "No matching entry found for eid=" << eid << " in SSU_NVME_SERVER_IP_LIST";
    return UBSE_ERROR;
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
            return UBSE_ERROR;
        }
        GStrvGuard entries(g_strsplit(ipListStr.c_str(), ",", -1));
        if (!entries || !*entries) {
            UBSE_LOG_ERROR << "Failed to parse SSU_NVME_SERVER_IP_LIST: " << ipListStr;
            return UBSE_ERROR;
        }
        for (gchar** it = entries.get(); *it != nullptr; ++it) {
            std::string entry(*it);
            std::string ip;
            uint32_t jettyId = 0;
            std::string eid;
            if (!ParseSsuIpEntry(entry, ip, jettyId, eid)) {
                return UBSE_ERROR; // 解析错误已记录日志
            }
            if (ip.size() >= DEV_IP_SIZE) {
                UBSE_LOG_ERROR << "ip too long, size=" << ip.size() << ", max=" << DEV_IP_SIZE - 1;
                return UBSE_ERROR;
            }
            DevAddrT addr{};
            memset_s(&addr.srcEid.raw, EID_SIZE, 0, EID_SIZE);
            if (GetSrcEid(addr.srcEid) != UBSE_OK) {
                return UBSE_ERROR;
            }
            memset_s(&addr.tgtEid.raw, EID_SIZE, 0, EID_SIZE);
            size_t copyLen = std::min(eid.size(), static_cast<size_t>(EID_SIZE));
            memcpy_s(addr.tgtEid.raw, EID_SIZE, eid.c_str(), copyLen);
            if (strncpy_s(addr.devIp, DEV_IP_SIZE, ip.c_str(), ip.size()) != EOK) {
                UBSE_LOG_ERROR << "strncpy_s failed for ip: " << ip;
                return UBSE_ERROR;
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
            return UBSE_ERROR;
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
        nsInfo.nuse = ns.usedBytes;
        
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
        return UBSE_ERROR;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return UBSE_ERROR;
    }

    // 允许ssuInfoList为空，此时BuildDevAddrList会从环境变量SSU_NVME_SERVER_IP_LIST读取
    std::vector<DevAddrT> devList;
    uint32_t ret = BuildDevAddrList(ssuInfoList, devList);
    if (ret != UBSE_OK) {
        return ret;
    }

    // devInfoList大小需与devList一致（环境变量分支下可能与ssuInfoList大小不同）
    std::vector<DevInfoT> devInfoList(devList.size());
    int acqRet = acquireDevInfo_(adminNqn.c_str(), devList.data(), static_cast<int>(devList.size()),
                                 devInfoList.data());
    if (acqRet != 0) {
        UBSE_LOG_ERROR << "acquire_dev_info failed, adminNqn=" << MaskNqn(adminNqn) << ", ret=" << acqRet;
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }
    
    // subNqn不能为空
    if (nameSpace.subSystem.subNqn.empty()) {
        UBSE_LOG_ERROR << "subNqn is empty";
        return UBSE_ERROR;
    }
    
    // nsze和ncap不能为0
    if (nameSpace.nsze == 0) {
        UBSE_LOG_ERROR << "nsze is zero";
        return UBSE_ERROR;
    }
    if (nameSpace.ncap == 0) {
        UBSE_LOG_ERROR << "ncap is zero";
        return UBSE_ERROR;
    }
    
    // 设置设备地址
    memset_s(&nsInfo.devAddr.srcEid.raw, EID_SIZE, 0, EID_SIZE);
    if (GetSrcEid(nsInfo.devAddr.srcEid) != UBSE_OK) {
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }
    
    // namespaceId不能为空
    if (nameSpace.namespaceId == 0) {
        UBSE_LOG_ERROR << "namespaceId is zero";
        return UBSE_ERROR;
    }
    
    // 设置设备地址
    memset_s(&nsInfo.devAddr.srcEid.raw, EID_SIZE, 0, EID_SIZE);
    if (GetSrcEid(nsInfo.devAddr.srcEid) != UBSE_OK) {
        return UBSE_ERROR;
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

bool UbseSsuAdapterImpl::VerifyNamespaceGuid(const UbseSsuDevNameSpace& nameSpace)
{
    if (nameSpace.subSystem.eid.empty() || nameSpace.guid.empty()) {
        UBSE_LOG_ERROR << "VerifyNamespaceGuid: eid or guid is empty";
        return false;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return false;
    }

    std::vector<UbseSsuDevInfo> devInfoList;
    devInfoList.push_back({.subSystem = {.eid = nameSpace.subSystem.eid, .subNqn = nameSpace.subSystem.subNqn}});
    uint32_t ret = GetDevList(devInfoList);
    if (ret != 0) {
        UBSE_LOG_ERROR << "VerifyNamespaceGuid: GetDevList failed, ret=" << ret;
        return false;
    }

    for (const auto& devInfo : devInfoList) {
        for (const auto& ns : devInfo.nameSpaces) {
            if (ns.namespaceId == nameSpace.namespaceId) {
                if (ns.guid == nameSpace.guid) {
                    return true;
                }
                UBSE_LOG_ERROR << "VerifyNamespaceGuid: GUID mismatch for namespaceId="
                               << nameSpace.namespaceId
                               << ", expected=" << nameSpace.guid
                               << ", actual=" << ns.guid;
                return false;
            }
        }
    }

    UBSE_LOG_WARN << "VerifyNamespaceGuid: namespaceId=" << nameSpace.namespaceId
                  << " not found on device eid=" << nameSpace.subSystem.eid;
    return false;
}

/**
 * @brief 在指定SSU设备上创建命名空间
 * @details 在指定控制器上创建一个新的NVMe命名空间
 *          满足可靠性要求：
 *          1. options中应携带预生成的guid（NGUID）
 *          2. guid在算法规划阶段生成，确保失败重试时使用同一guid实现幂等
 * @param nameSpace [inout] eid，guid，nsze，ncap，nsOptions，customData必填，返回namespaceId等信息
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::CreateDevNameSpace(UbseSsuDevNameSpace &nameSpace)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return UBSE_ERROR;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForCreate(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int createRet = createNamespace_(adminNqn.c_str(), &nsInfo);
    if (createRet != 0) {
        UBSE_LOG_ERROR << "create_namespace failed, adminNqn=" << MaskNqn(adminNqn) << ", ret=" << createRet;
        return UBSE_ERROR;
    }

    nameSpace.namespaceId = nsInfo.namespaceId;
    nameSpace.nsDevPath = std::string(nsInfo.devPath);
    nameSpace.nuse = nsInfo.usedBytes;
    nameSpace.guid = std::string(reinterpret_cast<const char*>(nsInfo.guid), GUID_SIZE);
    nameSpace.uuid = std::string(reinterpret_cast<const char*>(nsInfo.uuid), UUID_SIZE);

    UBSE_LOG_INFO << "Successfully created namespace " << nameSpace.namespaceId
                  << " with guid: " << nameSpace.guid;
    return UBSE_OK;
}

/**
 * @brief 删除指定SSU设备上的命名空间
 * @details 满足可靠性要求：
 *          1. 删除前验证 nameSpace.guid 与设备上实际 NS 的 NGUID 匹配
 *          2. 删除前确保 NS 已 detach（状态为 DELETING）
 *          3. NS 不存在时返回成功（幂等性保证）
 * @param nameSpace 要删除的命名空间信息，guid用于防误删验证
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::DeleteDevNameSpace(const UbseSsuDevNameSpace &nameSpace)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return UBSE_ERROR;
    }

    if (!VerifyNamespaceGuid(nameSpace)) {
        UBSE_LOG_ERROR << "DeleteDevNameSpace failed: GUID verification failed";
        return UBSE_ERROR;
    }

    std::vector<UbseSsuDevInfo> devInfoList;
    devInfoList.push_back({.subSystem = {.eid = nameSpace.subSystem.eid, .subNqn = nameSpace.subSystem.subNqn}});
    uint32_t ret = GetDevList(devInfoList);
    if (ret != 0) {
        UBSE_LOG_ERROR << "GetDevList failed before delete, ret=" << ret;
        return UBSE_ERROR;
    }

    bool nsExists = false;
    for (const auto& devInfo : devInfoList) {
        for (const auto& ns : devInfo.nameSpaces) {
            if (ns.namespaceId == nameSpace.namespaceId && ns.guid == nameSpace.guid) {
                nsExists = true;
                break;
            }
        }
    }

    if (!nsExists) {
        UBSE_LOG_INFO << "Namespace " << nameSpace.namespaceId
                      << " does not exist, returning success (idempotent)";
        return UBSE_OK;
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
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "Successfully deleted namespace " << nameSpace.namespaceId;
    return UBSE_OK;
}

/**
 * @brief 将命名空间attach到host节点
 * @details 满足可靠性要求：
 *          1. attach后验证NGUID一致性（读回nvme id-ns与预期guid比对）
 *          2. 已attach的NS重复调用应返回成功（幂等性）
 * @param nameSpace 要attach的命名空间，guid用于验证
 * @return 0表示成功，非0表示失败
 */
uint32_t UbseSsuAdapterImpl::AttachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace)
{
    if (hostNqn.empty()) {
        UBSE_LOG_ERROR << "AttachDevNameSpace: hostNqn is empty";
        return UBSE_ERROR;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int attachRet = attachNamespace_(hostNqn.c_str(), &nsInfo);
    if (attachRet != 0) {
        UBSE_LOG_ERROR << "attach_namespace failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << attachRet;
        return UBSE_ERROR;
    }

    // VerifyNamespaceGuid需要调用GetDevList来验证，但agent侧不支持GetDevList，所以这里先不调用

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
        return UBSE_ERROR;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int detachRet = detachNamespace_(hostNqn.c_str(), &nsInfo);
    if (detachRet != 0) {
        UBSE_LOG_ERROR << "detach_namespace failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << detachRet;
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return UBSE_ERROR;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int addRet = addNamespaceAllowHost_(adminNqn.c_str(), &nsInfo, hostNqn.c_str());
    if (addRet != 0) {
        UBSE_LOG_ERROR << "add_namespace_allow_host failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << addRet;
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }

    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return UBSE_ERROR;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    int removeRet = removeNamespaceAllowHost_(adminNqn.c_str(), &nsInfo, hostNqn.c_str());
    if (removeRet != 0) {
        UBSE_LOG_ERROR << "remove_namespace_allow_host failed, hostNqn=" << MaskNqn(hostNqn) << ", ret=" << removeRet;
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "Successfully removed allow host from namespace " << nameSpace.namespaceId
                  << ", hostNqn=" << MaskNqn(hostNqn);
    return UBSE_OK;
}

uint32_t UbseSsuAdapterImpl::GetNameSpaceAllowHostList(const UbseSsuDevNameSpace &nameSpace,
                                                       std::vector<std::string> &allowHostList)
{
    if (DlOpenLib() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string adminNqn;
    if (GetAdminNqn(adminNqn) != UBSE_OK) {
        return UBSE_ERROR;
    }

    DevNamespaceInfoT nsInfo{};
    uint32_t ret = BuildNamespaceInfoForBasic(nameSpace, nsInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    char** allowHosts = nullptr;
    uint32_t hostCnt = 0;
    int getRet = getNamespaceAllowHosts_(adminNqn.c_str(), &nsInfo, &allowHosts, &hostCnt);
    if (getRet != 0) {
        UBSE_LOG_ERROR << "get_namespace_allow_hosts failed, ret=" << getRet;
        return UBSE_ERROR;
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
            return UBSE_ERROR;
        }
        // 阻止 shell 元字符进入后续 ExecWithSudo 拼接的命令行，防止命令注入
        if (!IsSafePath(path)) {
            UBSE_LOG_ERROR << "Device path contains unsafe characters: " << path;
            return UBSE_ERROR;
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
            return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
    }
    // /dev/ssu/{deviceName} 是对外暴露的符号链接，底层设备可能为 LVM 或 mdadm。
    // 注意必须用 IS_SYMLINK 而非 EXISTS：EXISTS 会跟随链接判断目标是否存在，
    // 当底层设备已被外部删除导致断链时 EXISTS 返回 FALSE，会漏掉断链的清理。
    std::string ssuLinkPath = std::string(SSU_DEV_DIR) + "/" + deviceName;
    bool ssuLinkExists = g_file_test(ssuLinkPath.c_str(), G_FILE_TEST_IS_SYMLINK);

    // 先检查底层设备是否存在
    std::string mdDevicePath = "/dev/md/" + deviceName;
    bool mdDeviceExists = g_file_test(mdDevicePath.c_str(), G_FILE_TEST_EXISTS);

    std::string vgName = deviceName + "_vg";
    // 检查LVM逻辑卷是否存在，通过两种路径检查
    std::string lvPath1 = "/dev/mapper/" + vgName + "-" + deviceName;
    std::string lvPath2 = "/dev/" + vgName + "/" + deviceName;
    bool lvmDeviceExists = g_file_test(lvPath1.c_str(), G_FILE_TEST_EXISTS) ||
                           g_file_test(lvPath2.c_str(), G_FILE_TEST_EXISTS);
    // 如果底层设备和符号链接都不存在，直接返回成功（幂等）
    if (!ssuLinkExists && !mdDeviceExists && !lvmDeviceExists) {
        UBSE_LOG_INFO << "Block device " << deviceName << " does not exist, returning success (idempotent)";
        return UBSE_OK;
    }

    if (lvmDeviceExists) {
        return DeleteLvmBlockDevice(deviceName, vgName);
    }
    if (mdDeviceExists) {
        return DeleteMdBlockDevice(deviceName, mdDevicePath);
    }
    // 底层设备不存在但符号链接残留，清理符号链接
    if (ssuLinkExists) {
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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
        return UBSE_ERROR;
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

} // namespace ubse::adapter_plugins::ssu::def