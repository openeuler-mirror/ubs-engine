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
#include "ubse_npu_resource_collection.h"
#include <regex>
#include <utility>

#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_os_util.h"
namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID);
using namespace ubse::mti::urma;
using namespace ubse::mti::_1825;
using namespace ubse::common::def;
ResourceCollection::ResourceCollection()
    : devIdToDevice_(DeviceTypeToUint8(CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT)),
      state_(CollectionState::WAIT_INIT)
{
}

ResourceCollection &ResourceCollection::GetInstance()
{
    static ResourceCollection instance;
    return instance;
}

void ResourceCollection::ClearAllDevices()
{
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto &devVec : devIdToDevice_) {
        devVec.clear();
    }
    guidToDevice_.clear();
    state_ = CollectionState::WAIT_INIT;
}

bool ValidateGuid(const std::string &guid)
{
    // guid 是长度32位, 只允许-和16进制数的字符串
    return guid.size() == NO_32 && IsValidHexString(guid, true);
}

UbseResult SetDeviceWithDevId(std::shared_ptr<CollectionDevice> &dev, CollectionDevIdToDevice &devIdToDeviceMap)
{
    if (dev->GetType() >= CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT) {
        UBSE_LOG_ERROR << "Invalid device type, devId: " << dev->GetIdStr();
        return UBSE_ERROR;
    }
    auto exist = devIdToDeviceMap.find(dev->GetIdStr()) != devIdToDeviceMap.end();
    if (exist) {
        UBSE_LOG_WARN << "Device: " << dev->GetIdStr() << " has already existed";
        dev = devIdToDeviceMap[dev->GetIdStr()];
    } else {
        UBSE_LOG_INFO << "Success to add device, devId: " << dev->GetIdStr()
                      << ", type: " << static_cast<uint8_t>(dev->GetType());
        devIdToDeviceMap[dev->GetIdStr()] = dev;
    }
    return UBSE_OK;
}

UbseResult SetDeviceWithGuid(const std::shared_ptr<CollectionDevice> &dev, CollectionGuidToDevice &guidToDevice_)
{
    if (!ValidateGuid(dev->GetGuid())) {
        UBSE_LOG_WARN << "Guid is invalid, guid: " << dev->GetGuid();
        return UBSE_ERROR;
    }
    auto type = dev->GetType();
    if (type >= CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT) {
        UBSE_LOG_ERROR << "Invalid device type, guid: " << dev->GetGuid();
        return UBSE_ERROR;
    }
    if (type == CollectionDeviceType::HOST_BUSINSTANCE
        && type == CollectionDeviceType::VM_BUSINSTANCE
        && type == CollectionDeviceType::NIC) {
        auto exist = guidToDevice_.find(dev->GetGuid()) != guidToDevice_.end();
        if (exist) {
            UBSE_LOG_WARN << "Device: " << dev->GetGuid() << " has already existed";
        } else {
            guidToDevice_[dev->GetGuid()] = dev;
        }
    }
    return UBSE_OK;
}

UbseResult ResourceCollection::ValidateDevice(const std::shared_ptr<CollectionDevice> &dev)
{
    if (dev == nullptr) {
        UBSE_LOG_ERROR << "Device is nullptr";
        return UBSE_ERROR_INVAL;
    }
    auto type = dev->GetType();
    if (type >= CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT) {
        UBSE_LOG_ERROR << "Invalid device type, devId or guid: " << dev->GetIdStr();
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult ResourceCollection::SetDevice(std::shared_ptr<CollectionDevice> &dev)
{
    if (auto ret = ValidateDevice(dev); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Validate device failed";
        return ret;
    }
    auto type = dev->GetType();
    auto &devIdToDeviceMap = devIdToDevice_[DeviceTypeToUint8(type)];
    if (auto ret = SetDeviceWithDevId(dev, devIdToDeviceMap); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to add device to devId map";
        return ret;
    }
    if (type == CollectionDeviceType::P_IDEV || type == CollectionDeviceType::V_IDEV || !ValidateGuid(dev->GetGuid())) {
        UBSE_LOG_WARN << "No need to add device or guid is invalid, devId: " << dev->GetIdStr()
                      << ", guid: " << dev->GetGuid();
        return UBSE_OK;
    }
    if (auto ret = SetDeviceWithGuid(dev, guidToDevice_); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to add device to guid map";
        return ret;
    }
    UBSE_LOG_DEBUG << "Add device: " << dev->GetGuid() << " to guid map successful";
    return UBSE_OK;
}


bool IsBusInstanceType(const std::shared_ptr<CollectionDevice> &dev)
{
    return dev->GetType() == CollectionDeviceType::HOST_BUSINSTANCE ||
           dev->GetType() == CollectionDeviceType::VM_BUSINSTANCE;
}

bool IsIdevFeType(const std::shared_ptr<CollectionDevice> &dev)
{
    return dev->GetType() == CollectionDeviceType::P_IDEV || dev->GetType() == CollectionDeviceType::V_IDEV;
}

enum BindDevType : uint8_t {
    BUSI_VFE = 0,
    BUSI_NIC = 1,
    IDEV_DAVID = 2,
    ERROR_TYPE = 3
};

BindDevType GetBindDevType(const std::shared_ptr<CollectionDevice> &dev1, const std::shared_ptr<CollectionDevice> &dev2)
{
    if ((IsBusInstanceType(dev1) && dev2->GetType() == CollectionDeviceType::V_IDEV) ||
        (IsBusInstanceType(dev2) && dev1->GetType() == CollectionDeviceType::V_IDEV)) {
        return BindDevType::BUSI_VFE;
    }
    if ((IsBusInstanceType(dev1) && dev2->GetType() == CollectionDeviceType::NIC) ||
        (IsBusInstanceType(dev2) && dev1->GetType() == CollectionDeviceType::NIC)) {
        return BindDevType::BUSI_NIC;
    }
    if ((IsIdevFeType(dev1) && dev2->GetType() == CollectionDeviceType::NPU) ||
        (IsIdevFeType(dev2) && dev1->GetType() == CollectionDeviceType::NPU)) {
        return BindDevType::IDEV_DAVID;
    }
    return BindDevType::ERROR_TYPE;
}

std::shared_ptr<CollectionDevice> GetSpecifyTypeDevFromTwoDev(const std::shared_ptr<CollectionDevice> &dev1,
                                                              const std::shared_ptr<CollectionDevice> &dev2,
                                                              CollectionDeviceType type)
{
    if (dev1->GetType() == type) {
        return CollectionDevice::CollectionToDerived<CollectionDevice>(dev1);
    } else if (dev2->GetType() == type) {
        return CollectionDevice::CollectionToDerived<CollectionDevice>(dev2);
    }
    UBSE_LOG_WARN << "No the type among the device, dev1: " << dev1->GetIdStr() << ", dev2: " << dev2->GetIdStr()
                  << ", type: " << static_cast<uint8_t>(type);
    return nullptr;
}

UbseResult RemovePreviousBondingBusiFromVfe(const std::shared_ptr<CollectionDeviceIdevVfe> &devVfe)
{
    // vfebusi
    auto preBusiVec = devVfe->GetBondingDevBusi();
    if (preBusiVec.size() > 1) { // vfebusi
        UBSE_LOG_ERROR << "More than one busi found for the vfe. Expected at most one busi, but found "
                       << preBusiVec.size() << " busis."
                       << ", vfe: " << devVfe->GetIdStr();
        return UBSE_ERROR;
    }
    if (!preBusiVec.empty()) {
        const auto &preBusi = preBusiVec[0];
        if (preBusi == nullptr) {
            UBSE_LOG_ERROR << "The previous busi is null";
            return UBSE_ERROR;
        }
        preBusi->RemoveSubDevIdev(devVfe);
    }
    return UBSE_OK;
}

UbseResult ManageBusiAndVfeBinding(const std::shared_ptr<CollectionDevice> &dev1,
                                   const std::shared_ptr<CollectionDevice> &dev2, bool isBinding)
{
    auto devBusiBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::VM_BUSINSTANCE);
    auto devVfeBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::V_IDEV);
    auto devBusi = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(devBusiBase);
    auto devVfe = CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(devVfeBase);
    if (devBusi == nullptr || devVfe == nullptr) {
        UBSE_LOG_ERROR << "Device not found, busi: " << static_cast<int>(devBusi == nullptr)
                       << ", vfe: " << static_cast<int>(devVfe == nullptr);
        return UBSE_ERROR;
    }

    if (isBinding) {
        if (!devVfe->GetIsComSharedFe()) {
            // vfebusi
            RemovePreviousBondingBusiFromVfe(devVfe);
        }
        devBusi->SetSubDevIdev(devVfe);
        devVfe->SetBondingDevBusi(devBusi);
    } else {
        devBusi->RemoveSubDevIdev(devVfe);
        devVfe->RemoveBondingDevBusi(devBusi);
    }
    return UBSE_OK;
}

UbseResult ManageBusiAndNicBinding(const std::shared_ptr<CollectionDevice> &dev1,
                                   const std::shared_ptr<CollectionDevice> &dev2, bool isBinding)
{
    auto devBusiBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::VM_BUSINSTANCE);
    if (devBusiBase == nullptr) {
        devBusiBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::HOST_BUSINSTANCE);
    }
    auto devNicBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::NIC);
    auto devBusi = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(devBusiBase);
    auto devNic = CollectionDevice::CollectionToDerived<CollectionDeviceNic>(devNicBase);
    if (devBusi == nullptr || devNic == nullptr) {
        UBSE_LOG_ERROR << "Device not found, busi: " << static_cast<int>(devBusi == nullptr)
                       << ", nic: " << static_cast<int>(devNic == nullptr);
        return UBSE_ERROR;
    }
    // nicbusi
    auto preBusi = devNic->GetBondingDevBusi();
    if (preBusi != nullptr) {
        preBusi->RemoveSubDevNic(devNic);
    }
    if (isBinding) {
        devBusi->SetSubDevNic(devNic);
        devNic->SetBondingDevBusi(devBusi);
    } else {
        devNic->RemoveBondingDevBusi();
    }
    return UBSE_OK;
}

UbseResult ManageIdevAndDavidBinding(const std::shared_ptr<CollectionDevice> &dev1,
                                     const std::shared_ptr<CollectionDevice> &dev2, bool isBinding)
{
    auto devIdevBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::P_IDEV);
    if (devIdevBase == nullptr) {
        devIdevBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::V_IDEV);
    }
    auto devDavidBase = GetSpecifyTypeDevFromTwoDev(dev1, dev2, CollectionDeviceType::NPU);
    auto devIdev = CollectionDevice::CollectionToDerived<CollectionDeviceIdev>(devIdevBase);
    auto devDavid = CollectionDevice::CollectionToDerived<CollectionDeviceDavid>(devDavidBase);
    if (devDavid == nullptr || devIdev == nullptr) {
        UBSE_LOG_ERROR << "Device not found, david: " << static_cast<int>(devDavid == nullptr)
                       << ", idev: " << static_cast<int>(devIdev == nullptr);
        return UBSE_ERROR;
    }
    if (isBinding) {
        // idevdavid
        auto preIdevBase = devDavid->GetBondingIdev();
        auto preIdev = CollectionDevice::CollectionToDerived<CollectionDeviceIdev>(preIdevBase);
        if (preIdev != nullptr) {
            preIdev->RemoveBondingDevDavid();
        }
        // davididev
        auto preDavid = devIdev->GetBondingDevDavid();
        if (preDavid != nullptr) {
            preDavid->RemoveBondingIdev();
        }
        devIdev->SetBondingDevDavid(devDavid);
        devDavid->SetBondingIdev(devIdev);
    } else {
        devIdev->RemoveBondingDevDavid();
        devDavid->RemoveBondingIdev();
    }
    return UBSE_OK;
}
UbseResult ResourceCollection::BindDevice(const std::shared_ptr<CollectionDevice> &dev1,
                                          const std::shared_ptr<CollectionDevice> &dev2)
{
    if (dev1 == nullptr || dev2 == nullptr) {
        UBSE_LOG_ERROR << "Failed to bind nullptr device, " << (dev1 == nullptr) << " " << (dev2 == nullptr);
        return UBSE_ERROR_INVAL;
    }
    auto bindDevType = GetBindDevType(dev1, dev2);
    if (bindDevType == BindDevType::ERROR_TYPE) {
        UBSE_LOG_ERROR << "Device type to be bind is not allowed";
        return UBSE_ERROR_INVAL;
    }
    if (bindDevType == BindDevType::BUSI_VFE) {
        if (auto ret = ManageBusiAndVfeBinding(dev1, dev2, true); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to bind bus instance and vfe, dev1: " << dev1->GetIdStr()
                           << ", dev2: " << dev2->GetIdStr();
            return ret;
        }
    } else if (bindDevType == BindDevType::BUSI_NIC) {
        if (auto ret = ManageBusiAndNicBinding(dev1, dev2, true); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to bind bus instance and 1825, dev1: " << dev1->GetIdStr()
                           << ", dev2: " << dev2->GetIdStr();
            return ret;
        }
    } else {
        if (auto ret = ManageIdevAndDavidBinding(dev1, dev2, true); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to bind idev and david, dev1: " << dev1->GetIdStr()
                           << ", dev2: " << dev2->GetIdStr();
            return ret;
        }
    }
    UBSE_LOG_INFO << "Success to bind two device, guid1: " << dev1->GetGuid() << " guid2: " << dev2->GetGuid();
    return UBSE_OK;
}

UbseResult ResourceCollection::UnbindDevice(const std::shared_ptr<CollectionDevice> &dev1,
                                            const std::shared_ptr<CollectionDevice> &dev2)
{
    if (dev1 == nullptr || dev2 == nullptr) {
        UBSE_LOG_ERROR << "Failed to unbind nullptr device";
        return UBSE_ERROR_INVAL;
    }
    auto unbindDevType = GetBindDevType(dev1, dev2);
    if (unbindDevType == BindDevType::ERROR_TYPE) {
        UBSE_LOG_ERROR << "Device type to be unbind is not allowed";
        return UBSE_ERROR_INVAL;
    }
    if (unbindDevType == BindDevType::BUSI_VFE) {
        if (auto ret = ManageBusiAndVfeBinding(dev1, dev2, false); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unbind bus instance and vfe";
            return ret;
        }
    } else if (unbindDevType == BindDevType::BUSI_NIC) {
        if (auto ret = ManageBusiAndNicBinding(dev1, dev2, false); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unbind bus instance and 1825";
            return ret;
        }
    } else {
        if (auto ret = ManageIdevAndDavidBinding(dev1, dev2, false); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unbind idev and david";
            return ret;
        }
    }
    UBSE_LOG_INFO << "Success to unbind two device, guid1: " << dev1->GetGuid() << " guid2: " << dev2->GetGuid();
    return UBSE_OK;
}

} // namespace ubse::npu::controller
