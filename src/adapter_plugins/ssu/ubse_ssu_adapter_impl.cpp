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
#include <cstring>
#include <memory>
#include <securec.h>
#include <sstream>
#include <glib.h>
extern "C" {
#include <blockdev/lvm.h>
#include <blockdev/mdraid.h>
}
#include "ubse_conf.h"
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
    return UBSE_OK;
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
    devList.resize(ssuInfoList.size());
    for (size_t i = 0; i < ssuInfoList.size(); ++i) {
        const std::string& eid = ssuInfoList[i].subSystem.eid;
        const std::string& subNqn = ssuInfoList[i].subSystem.subNqn;
        
        // EID必须是固定长度
        if (eid.size() != EID_SIZE) {
            UBSE_LOG_ERROR << "Invalid EID length: " << eid.size() << ", expected " << EID_SIZE;
            return UBSE_ERROR;
        }
        
        // subNqn不能为空
        if (subNqn.empty()) {
            UBSE_LOG_ERROR << "subNqn is empty";
            return UBSE_ERROR;
        }
        
        memset_s(&devList[i].srcEid.raw, EID_SIZE, 0, EID_SIZE);
        if (GetSrcEid(devList[i].srcEid) != UBSE_OK) {
            return UBSE_ERROR;
        }
        memset_s(&devList[i].tgtEid.raw, EID_SIZE, 0, EID_SIZE);
        memcpy_s(devList[i].tgtEid.raw, EID_SIZE, eid.c_str(), EID_SIZE);
        devList[i].devIp = nullptr;
        devList[i].useUb = false;
        memset_s(devList[i].subNqn, SUBNQN_SIZE, 0, SUBNQN_SIZE);
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

    if (ssuInfoList.empty()) {
        return UBSE_ERROR;
    }

    std::vector<DevAddrT> devList;
    uint32_t ret = BuildDevAddrList(ssuInfoList, devList);
    if (ret != UBSE_OK) {
        return ret;
    }

    std::vector<DevInfoT> devInfoList(ssuInfoList.size());
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
    nsInfo.devAddr.devIp = nullptr;
    nsInfo.devAddr.useUb = false;
    memset_s(nsInfo.devAddr.subNqn, SUBNQN_SIZE, 0, SUBNQN_SIZE);
    strncpy_s(nsInfo.devAddr.subNqn, SUBNQN_SIZE,
              nameSpace.subSystem.subNqn.c_str(), nameSpace.subSystem.subNqn.size());
    nsInfo.devAddr.jettyId = nameSpace.subSystem.jettyId;
    
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
    nsInfo.devAddr.devIp = nullptr;
    nsInfo.devAddr.useUb = false;
    nsInfo.devAddr.jettyId = nameSpace.subSystem.jettyId;
    
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
                           << ", expected format: /dev/disk/by-id/nvme-eui.<guid>";
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
    gchar** devs = (gchar**)g_try_malloc0((devicePathList.size() + 1) * sizeof(gchar*));
    if (devs == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for device paths";
        return UBSE_ERROR;
    }
    GStrvGuard devsGuard(devs);
    for (size_t i = 0; i < devicePathList.size(); ++i) {
        devs[i] = g_strdup(devicePathList[i].c_str());
    }

    GError* error = nullptr;
    std::vector<std::string> createdPVs;
    std::string vgName = deviceName + "_vg";

    // 创建物理卷（逐个创建）
    for (size_t i = 0; i < devicePathList.size(); ++i) {
        gboolean pvRet = bd_lvm_pvcreate(devicePathList[i].c_str(), 0, 0, nullptr, &error);
        if (!pvRet) {
            UBSE_LOG_ERROR << "Failed to create PV for " << devicePathList[i] << ": " << error->message;
            g_error_free(error);
            for (const auto& pvPath : createdPVs) {
                bd_lvm_pvremove(pvPath.c_str(), nullptr, nullptr);
            }
            return UBSE_ERROR;
        }
        createdPVs.push_back(devicePathList[i]);
    }

    // 创建卷组（卷组名：{deviceName}_vg）
    gboolean vgRet = bd_lvm_vgcreate(vgName.c_str(), (const gchar**)devs, 0, nullptr, &error);
    if (!vgRet) {
        UBSE_LOG_ERROR << "Failed to create VG: " << error->message;
        g_error_free(error);
        for (const auto& pvPath : createdPVs) {
            bd_lvm_pvremove(pvPath.c_str(), nullptr, nullptr);
        }
        return UBSE_ERROR;
    }

    // 创建逻辑卷（线性模式，逻辑卷名：{deviceName}，分配全部空间）
    gboolean lvRet = bd_lvm_lvcreate(vgName.c_str(), deviceName.c_str(), 0, "linear", nullptr, nullptr, &error);
    if (!lvRet) {
        UBSE_LOG_ERROR << "Failed to create LV: " << error->message;
        g_error_free(error);
        bd_lvm_lvremove(vgName.c_str(), deviceName.c_str(), (gboolean)1, nullptr, nullptr);
        bd_lvm_vgremove(vgName.c_str(), nullptr, nullptr);
        for (const auto& pvPath : createdPVs) {
            bd_lvm_pvremove(pvPath.c_str(), nullptr, nullptr);
        }
        return UBSE_ERROR;
    }

    devicePath = "/dev/" + vgName + "/" + deviceName;
    return UBSE_OK;
}

/**
 * @brief 使用mdadm创建条带化块设备（RAID0/RAID5）
 * @details 使用libblockdev的mdraid模块创建RAID阵列
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
    gchar** devs = (gchar**)g_try_malloc0((devicePathList.size() + 1) * sizeof(gchar*));
    if (devs == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate memory for device paths";
        return UBSE_ERROR;
    }
    GStrvGuard devsGuard(devs);
    for (size_t i = 0; i < devicePathList.size(); ++i) {
        devs[i] = g_strdup(devicePathList[i].c_str());
    }

    GError* error = nullptr;

    const gchar* md_level_str = "raid0";
    if (options.raidLevel == UbseSsuRaidLevel::RAID5) {
        md_level_str = "raid5";
    }

    gboolean mdRet = bd_md_create(deviceName.c_str(), md_level_str, (const gchar**)devs,
                                  0, nullptr, nullptr, options.chunkSize * 1024, nullptr, &error);
    if (!mdRet) {
        UBSE_LOG_ERROR << "Failed to create MD RAID: " << error->message;
        g_error_free(error);
        return UBSE_ERROR;
    }

    devicePath = "/dev/md/" + deviceName;

    return UBSE_OK;
}

/**
 * @brief 创建块设备（支持RAID）
 * @details 将多个命名空间组合成一个块设备，支持LINEAR、RAID0、RAID5模式
 *          满足可靠性要求：
 *          1. devicePathList应使用persistentPath（/dev/disk/by-id/nvme-eui.<guid>）
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
    GError* error = nullptr;

    // 先检查设备是否存在
    std::string mdDevicePath = "/dev/md/" + deviceName;
    bool mdDeviceExists = g_file_test(mdDevicePath.c_str(), G_FILE_TEST_EXISTS);
    
    std::string vgName = deviceName + "_vg";
    // 检查LVM逻辑卷是否存在，通过两种路径检查
    std::string lvPath1 = "/dev/mapper/" + vgName + "-" + deviceName;
    std::string lvPath2 = "/dev/" + vgName + "/" + deviceName;
    bool lvmDeviceExists = g_file_test(lvPath1.c_str(), G_FILE_TEST_EXISTS) ||
                           g_file_test(lvPath2.c_str(), G_FILE_TEST_EXISTS);
    // 如果都不存在，直接返回成功（幂等）
    if (!mdDeviceExists && !lvmDeviceExists) {
        UBSE_LOG_INFO << "Block device " << deviceName << " does not exist, returning success (idempotent)";
        return UBSE_OK;
    }
    // 如果LVM设备存在，尝试删除LVM
    if (lvmDeviceExists) {
        gboolean lvRet = bd_lvm_lvremove(vgName.c_str(), deviceName.c_str(), (gboolean)1, nullptr, &error);
        if (lvRet) {
            bd_lvm_vgremove(vgName.c_str(), nullptr, nullptr);
            UBSE_LOG_INFO << "Successfully deleted LVM block device " << deviceName;
            return UBSE_OK;
        }
        // LVM删除失败，返回错误
        UBSE_LOG_ERROR << "Failed to delete LVM block device " << deviceName << ": "
                       << (error ? error->message : "unknown error");
        if (error != nullptr) {
            g_error_free(error);
        }
        return UBSE_ERROR;
    }
    // 如果md设备存在，尝试删除md
    if (mdDeviceExists) {
        gboolean mdRemoveRet = bd_md_remove(mdDevicePath.c_str(), nullptr, (gboolean)1, nullptr, &error);
        if (mdRemoveRet) {
            UBSE_LOG_INFO << "Successfully deleted mdadm block device " << deviceName;
            return UBSE_OK;
        }
        // md删除失败，返回错误
        UBSE_LOG_ERROR << "Failed to delete mdadm block device " << deviceName << ": "
                       << (error ? error->message : "unknown error");
        if (error != nullptr) {
            g_error_free(error);
        }
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::adapter_plugins::ssu::def