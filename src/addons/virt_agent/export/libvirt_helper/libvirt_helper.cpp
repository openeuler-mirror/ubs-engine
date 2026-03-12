/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "libvirt_helper.h"

#include <ubse_logger.h>

namespace vm {
UBSE_DEFINE_THIS_MODULE("vm_plugin");
using namespace ubse::log;
using namespace libvirt;

VmResult LibvirtHelper::Init()
{
    VmResult ret = LibvirtModule::Init();
    if (ret != VM_OK) {
        return VM_ERROR;
    }

    ret = this->Connect();
    if (ret != VM_OK) {
        LibvirtModule::DeInit();
        return VM_ERROR;
    }
    return VM_OK;
}

void LibvirtHelper::DeInit()
{
    auto ret = this->CloseConn();
    if (ret != VM_OK) {
        return;
    }
    LibvirtModule::DeInit();
}
/**
 * Connect to Libvirt
 * @return 0 for success, non-zero for error
 */
VmResult LibvirtHelper::Connect()
{
    try {
        UBSE_LOG_INFO << "[LibvirtHelper] Start to get libvirt connect.";
        virConnect = LibvirtModule::VirConnectOpen()("qemu:///system");
        if (virConnect == nullptr) {
            UBSE_LOG_ERROR << "Libvirt conn failed, please check the virsh environment. " << FormatRetCode(errno);
            return VM_ERROR;
        }
        return VM_OK;
    } catch (std::exception &e) {
        UBSE_LOG_ERROR << "[LibvirtHelper] Connect libvirt failed. err: " << e.what();
        return VM_ERROR;
    }
}

/**
 * Close the Libvirt connection
 * @return 0 for success, non-zero for error
 */
VmResult LibvirtHelper::CloseConn()
{
    UBSE_LOG_INFO << "Start to close libvirt connect.";
    if (virConnect == nullptr) {
        UBSE_LOG_WARN << "Libvirt connect is empty.";
        return VM_ERROR;
    }
    LibvirtModule::VirConnectClose()(virConnect);
    virConnect = nullptr;
    return VM_OK;
}

void LibvirtHelper::FreeDomain(void *domain)
{
    if (domain == nullptr) {
        UBSE_LOG_ERROR << "Domain ptr is already empty.";
        return;
    }
    libvirt::VirDomainFreeFunc virDomainFree = LibvirtModule::VirDomainFree();
    if (virDomainFree == nullptr) {
        UBSE_LOG_ERROR << "VirDomainFree ptr is nullptr.";
        return;
    }
    virDomainFree(domain);
}

VmResult LibvirtHelper::DomainAbortJobFlags(const string &uuid, VirDomainAbortJobFlagsValues flags, int tryTimes)
{
    libvirt::VirDomainLookupByUUIDStringFunc virDomainLookupByUuidString = LibvirtModule::VirDomainLookupByUuidString();
    if (virDomainLookupByUuidString == nullptr) {
        UBSE_LOG_ERROR << "Get virDomainLookupByUUIDString is nullptr.";
        return VM_ERROR;
    }

    void *domain = virDomainLookupByUuidString(virConnect, uuid.c_str());
    if (domain == nullptr) {
        UBSE_LOG_ERROR << "Failed to get vmDomain info, uuid = " << uuid;
        return VM_ERROR;
    }

    libvirt::VirDomainAbortJobFlagsFunc virDomainAbortJobFlags = LibvirtModule::VirDomainAbortJobFlags();
    if (virDomainAbortJobFlags == nullptr) {
        UBSE_LOG_ERROR << "Failed to get virDomainAbortJobFlags, uuid = " << uuid << ", " << FormatRetCode(errno);
        FreeDomain(domain);
        return VM_ERROR;
    }
    const auto flagsToUInt = static_cast<unsigned int>(flags);
    UBSE_LOG_INFO << "[virDomainAbortJobFlags] begin, uuid = " << uuid << ", flags = " << flagsToUInt;
    VmResult ret;
    for (int tryCount = 1; tryCount <= tryTimes; tryCount++) {
        ret = virDomainAbortJobFlags(domain, flags);
        if (ret != VM_OK) {
            UBSE_LOG_ERROR << "[virDomainAbortJobFlags] failed, uuid = " << uuid << ", flags = " << flagsToUInt
                           << ", tryCount = " << tryCount << ", " << FormatRetCode(ret);
        } else {
            UBSE_LOG_INFO << "[virDomainAbortJobFlags] succeed, uuid = " << uuid << ", flags = " << flagsToUInt
                          << ", tryCount = " << tryCount;
            break;
        }
    }
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[virDomainAbortJobFlags] end, uuid = " << uuid << ", flags = " << flagsToUInt
                       << ", reached times = " << tryTimes << ", " << FormatRetCode(ret);
    }
    FreeDomain(domain);
    return ret;
}
}