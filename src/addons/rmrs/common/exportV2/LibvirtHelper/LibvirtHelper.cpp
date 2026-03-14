/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "LibvirtHelper.h"
#include <map>
#include "mp_configuration.h"
#include "ubse_logger.h"

namespace mempooling::exportV2 {
using namespace libvirt;
using namespace ubse::log;

const std::string SUB_MODULE_NAME = "[LibvirtHelper] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME

static std::map<unsigned char, std::string> VirStateStringMap = {{0, "NOSTATE"}, {1, "RUNNING"},     {2, "BLOCKED"},
                                                                 {3, "PAUSED"},  {4, "SHUTDOWN"},    {5, "SHUTOFF"},
                                                                 {6, "CRASHED"}, {7, "PMSUSPENDED"}, {8, "LAST"}};

MpResult LibvirtHelper::Init()
{
    auto ret = LibvirtModule::Init();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Init failed, LibvirtModule Init failed.";
        return MEM_POOLING_ERROR;
    }

    ret = RegisterEventDefaultImpl();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Init failed, RegisterEventDefaultImpl failed.";
        return MEM_POOLING_ERROR;
    }

    ret = this->Connect();
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Cannot connect.";
        return MEM_POOLING_OK;
    }

    ret = ConnectSetKeepAlive();
    if (ret != 0) {
        LOG_WARN << "ConnectSetKeepAlive failed.";
    }

    LOG_INFO << "Initialization succeed.";
    return MEM_POOLING_OK;
}

void LibvirtHelper::KeepAlive()
{
    libvirt::VirEventRunDefaultImplFunc virEventRunDefaultImplFunc = LibvirtModule::VirEventRunDefaultImpl();
    if (virEventRunDefaultImplFunc == nullptr) {
        return;
    }
    while (true) {
        if (virEventRunDefaultImplFunc() < 0) {
            LOG_ERROR << "VirEventRunDefaultImplFunc failed.";
            return;
        }
    }
    return;
}

MpResult LibvirtHelper::Connect()
{
    try {
        LOG_INFO << "Start to get libvirt connection.";
        virConnect = LibvirtModule::VirConnectOpen()("qemu:///system");
        if (virConnect == nullptr) {
            LOG_ERROR << "Libvirt conn failed, please check the virsh environment. error is " << strerror(errno) << ".";
            return MEM_POOLING_ERROR;
        }
        auto ret = ConnectSetKeepAlive();
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "ConnectSetKeepAlive failed.";
            return MEM_POOLING_ERROR;
        }
        LOG_INFO << "Libvirt connection succeed.";
        return MEM_POOLING_OK;
    } catch (std::exception &e) {
        LOG_ERROR << "Connect libvirt failed. err: " << e.what() << ".";
        return MEM_POOLING_ERROR;
    }
}

MpResult LibvirtHelper::CloseConn()
{
    LOG_INFO << "Start to close libvirt connect.";
    if (!virConnect) {
        LOG_ERROR << "Libvirt connect is empty.";
        return MEM_POOLING_ERROR;
    }
    LibvirtModule::VirConnectClose()(virConnect);
    return MEM_POOLING_OK;
}

MpResult LibvirtHelper::Reconnect()
{
    if (virConnect != nullptr) {
        LibvirtModule::VirConnectClose()(virConnect);
        virConnect = nullptr;
    }

    return Connect();
}

bool LibvirtHelper::IsConnectAlive()
{
    libvirt::VirConnectIsAliveFunc virConnectIsAlive = LibvirtModule::VirConnectIsAlive();
    if (virConnectIsAlive == nullptr) {
        return false;
    }

    if (virConnect != nullptr && virConnectIsAlive(virConnect) > 0) {
        return true;
    }

    return false;
}

MpResult LibvirtHelper::RegisterEventDefaultImpl()
{
    libvirt::VirEventRegisterDefaultImplFunc virEventRegisterDefaultImpl = LibvirtModule::VirEventRegisterDefaultImpl();
    if (virEventRegisterDefaultImpl == nullptr) {
        return MEM_POOLING_ERROR;
    }

    int ret = virEventRegisterDefaultImpl();
    if (ret != 0) {
        LOG_ERROR << "Registers default event implementation failed.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult LibvirtHelper::CheckConnectAndReconnect()
{
    if (!IsConnectAlive()) {
        LOG_WARN << "Libvirt connect is not alive. Try to reconnect.";
        if (Reconnect() != MEM_POOLING_OK) {
            LOG_ERROR << "Reconnect libvirt failed. Please check environment.";
            return MEM_POOLING_ERROR;
        }
    }

    return MEM_POOLING_OK;
}

static constexpr int KEEP_ALIVE_INTERVAL = 10;
static constexpr unsigned int KEEP_ALIVE_COUNT = 3;

MpResult LibvirtHelper::ConnectSetKeepAlive()
{
    libvirt::VirConnectSetKeepAliveFunc virConnectSetKeepAlive = LibvirtModule::VirConnectSetKeepAlive();
    if (virConnectSetKeepAlive == nullptr) {
        return MEM_POOLING_ERROR;
    }

    if (virConnectSetKeepAlive(virConnect, KEEP_ALIVE_INTERVAL, KEEP_ALIVE_COUNT) < 0) {
        LOG_ERROR << "VirConnectSetKeepAlive failed.";
        return MEM_POOLING_ERROR;
    }

    virKeepAliveThread = new std::thread([this]() { this->KeepAlive(); });
    virKeepAliveThread->detach();
    return MEM_POOLING_OK;
}

MpResult LibvirtHelper::GetDomainByName(const std::string &name, VirDomainPtr &domain)
{
    libvirt::VirDomainLookupByNameFunc virDomainLookupByName = LibvirtModule::VirDomainLookupByName();
    if (virDomainLookupByName == nullptr) {
        return MEM_POOLING_ERROR;
    }
    domain = virDomainLookupByName(virConnect, name.c_str());
    if (!domain) {
        LOG_ERROR << "Get vir domain failed, vm name: " << name;
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult LibvirtHelper::GetVmUuidByDomain(VirDomainPtr domain, std::string &uuidStr)
{
    if (domain == nullptr) {
        LOG_ERROR << "Get vm domain failed, domain is nullptr.";
        return MEM_POOLING_ERROR;
    }
    libvirt::VirDomainGetUUIDStringFunc virDomainGetUUIDString = LibvirtModule::VirDomainGetUUIDString();
    if (virDomainGetUUIDString == nullptr) {
        LOG_ERROR << "virDomainGetUUIDString is nullptr.";
        return MEM_POOLING_ERROR;
    }
    char uuid[VM_UUID_LEN];
    auto ret = virDomainGetUUIDString(domain, uuid);
    if (ret < 0) {
        LOG_ERROR << "Get vmDomain UUID failed. " << strerror(errno) << ".";
        return MEM_POOLING_ERROR;
    }
    uuidStr = std::string(uuid);
    return MEM_POOLING_OK;
}

MpResult LibvirtHelper::GetVmStateAndMaxMemByDomain(VirDomainPtr domain, VmDomainInfo &vmInfo)
{
    if (domain == nullptr) {
        LOG_ERROR << "Get vm domain failed, domain is nullptr.";
        return MEM_POOLING_ERROR;
    }
    libvirt::VirDomainGetInfoFunc virDomainGetInfo = LibvirtModule::VirDomainGetInfo();
    if (virDomainGetInfo == nullptr) {
        LOG_ERROR << "VirDomainGetInfo is nullptr.";
        return MEM_POOLING_ERROR;
    }
    VirDomainInfo info;
    auto ret = virDomainGetInfo(domain, &info);
    if (ret < 0) {
        LOG_ERROR << "Get vm domain info error.";
        return MEM_POOLING_ERROR;
    }
    vmInfo.metaData.state = VirStateStringMap[info.state];
    vmInfo.metaData.maxMem = info.maxMem;
    return MEM_POOLING_OK;
}

void LibvirtHelper::FreeDomain(VirDomainPtr domain)
{
    if (domain == nullptr) {
        LOG_ERROR << "Domain ptr is already empty.";
        return;
    }

    libvirt::VirDomainFreeFunc virDomainFree = LibvirtModule::VirDomainFree();
    if (virDomainFree == nullptr) {
        LOG_ERROR << "VirDomainFree function is not available.";
        return;
    }
    virDomainFree(domain);
    domain = nullptr; // 防止后续的双重释放
    return;
}

void LibvirtHelper::Shutdown()
{
    LibvirtModule::CloseLibvirtHandle();
    auto ret = CloseConn();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Close conn failed.";
        return;
    }
    LOG_INFO << "Shutdown end.";
}

} // namespace mempooling::exportV2