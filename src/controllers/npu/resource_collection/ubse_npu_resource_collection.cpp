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
#include "ubse_logger.h"
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"
namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
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

CollectionDevId CollectionStringUtil::GuidToStr(const UbseMtiGuid &guid)
{
    // UbseMtiGuid内存布局位小端序，与字符串表示相反，字符串用大端序存储
    UbseMtiGuid reversed{};
    size_t reversed_size = guid.size() - 1;
    for (size_t i = 0; i < guid.size(); i++) {
        reversed.at(i) = guid.at(reversed_size - i);
    }
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0'); // 确保小写字母，填充‘0’
    for (uint8_t byte : reversed) {
        oss << std::setw(NO_2) << static_cast<int>(byte); // 强制按 2 字符输出
    }
    return oss.str();
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

UbseResult SetDeviceWithGuid(std::shared_ptr<CollectionDevice> &dev, CollectionGuidToDevice &guidToDevice_)
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

UbseResult ResourceCollection::CollectStaticResource()
{
    UBSE_LOG_INFO << "Start to collect static resource";
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (state_ != CollectionState::WAIT_INIT) {
            UBSE_LOG_ERROR << "Collection has been already started, state_: " << static_cast<uint8_t>(state_);
            return UBSE_ERROR;
        }
        state_ = CollectionState::RUNNING;
    }
    UBSE_LOG_INFO << "Set state to Running, begin collection";
    if (auto ret = CollectUbCtrlIdev(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query idev device among 1650";
        ClearAllDevices();
        return ret;
    }
    if (auto ret = CollectIdevPfeDavid(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query npu device and the relationship with idev among 1650";
        ClearAllDevices();
        return ret;
    }
    if (auto ret = CollectAffinityNic(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query 1825 fe affinity with ubctrl";
        ClearAllDevices();
        return ret;
    }
    if (auto ret = CollectBusInstance(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query existed bus instances and the sub device";
        ClearAllDevices();
        return ret;
    }
    // vfe npu-> upi
    if (auto ret = BindVfeToNpu(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to bind vfe to npu";
        ClearAllDevices();
        return ret;
    }
    state_ = CollectionState::FINISH;
    UBSE_LOG_INFO << "Collect static resource successfully";
    return UBSE_OK;
}
UbseResult ResourceCollection::BindVfeToNpu()
{
    // businstance
    for (auto &[devId, device] : guidToDevice_) {
        if (device->GetType() != CollectionDeviceType::VM_BUSINSTANCE) {
            continue;
        }
        auto busi = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(device);
        if (busi == nullptr) {
            UBSE_LOG_ERROR << "Failed to convert to CollectionDeviceBusi";
            continue;
        }
        // businstance-vfe;
        auto subIdevs = busi->GetSubDevIdev();
        for (auto &vfeIdev : subIdevs) {
            // vfe-pfe
            auto pfeIdev = vfeIdev->GetParentPfe();
            // pfe-david;
            auto npu = pfeIdev->GetBondingDevDavid();
            BindDevice(npu, vfeIdev);
        }
    }
    return UBSE_OK;
}

std::shared_ptr<CollectionDevice> ResourceCollection::GetDeviceByDevId(const CollectionDevId &devId,
                                                                       const CollectionDeviceType &type)
{
    if (devId.empty() || type >= CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT) {
        UBSE_LOG_ERROR << "devId is empty or invalid device type, devId: " << devId
                       << " type: " << static_cast<uint8_t>(type);
        return nullptr;
    }
    if (devIdToDevice_.size() != static_cast<uint32_t>(CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT)) {
        UBSE_LOG_ERROR << "devIdToDevice_ map is incomplete";
        return nullptr;
    }
    auto &devIdToDeviceMap = devIdToDevice_[DeviceTypeToUint8(type)];
    if (devIdToDeviceMap.find(devId) == devIdToDeviceMap.end()) {
        UBSE_LOG_ERROR << "Can not find device by devId";
        return nullptr;
    }
    return devIdToDeviceMap[devId];
}

std::shared_ptr<CollectionDevice> ResourceCollection::GetDeviceByGuid(const CollectionGuid &guid)
{
    if (!ValidateGuid(guid)) {
        UBSE_LOG_ERROR << "Guid is invalid, guid: " << guid;
        return nullptr;
    }
    if (guidToDevice_.find(guid) == guidToDevice_.end()) {
        UBSE_LOG_WARN << "Can not find device by guid, guid: " << guid;
        return nullptr;
    }
    return guidToDevice_[guid];
}

UbseResult ResourceCollection::GetDevicesByType(const CollectionDeviceType &type, CollectionDevIdToDevice &sameTypeDevs)
{
    if (type >= CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT) {
        UBSE_LOG_ERROR << "Type is invalid";
        return UBSE_ERROR_INVAL;
    }
    sameTypeDevs = devIdToDevice_[static_cast<uint8_t>(type)];
    return UBSE_OK;
}

std::shared_ptr<CollectionDeviceBusi> ResourceCollection::GetDeviceHostBusInstance()
{
    if (devIdToDevice_.size() < static_cast<uint32_t>(CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT)) {
        UBSE_LOG_ERROR << "devIdToDevice_ map is incomplete";
        return nullptr;
    }
    // host businstance
    if (devIdToDevice_[static_cast<uint8_t>(CollectionDeviceType::HOST_BUSINSTANCE)].empty()) {
        UBSE_LOG_ERROR << "Host bus instance not exist";
        return nullptr;
    }
    auto first = devIdToDevice_[static_cast<uint8_t>(CollectionDeviceType::HOST_BUSINSTANCE)].begin();
    return CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(first->second);
}

std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> ResourceCollection::GetDeviceAllComSharedIdevVfe()
{
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> devComSharedIdevVfes{};
    for (auto &kv : devIdToDevice_[static_cast<uint8_t>(CollectionDeviceType::V_IDEV)]) {
        auto devVfeBase = kv.second;
        auto devVfe = CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(devVfeBase);
        if (devVfe != nullptr && devVfe->GetIsComSharedFe()) {
            UBSE_LOG_INFO << "Get com shared vfe, id: " << devVfe->GetIdStr();
            devComSharedIdevVfes.push_back(devVfe);
        }
    }
    return devComSharedIdevVfes;
}
UbseResult ResourceCollection::RemoveDeviceEmptyVmBusi(const std::shared_ptr<CollectionDevice> &baseDev)
{
    auto dev = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(std::move(baseDev));
    if (dev == nullptr) {
        UBSE_LOG_ERROR << "dev is nullptr";
        return UBSE_ERROR_INVAL;
    }
    auto &subDevNics = dev->GetSubDevNic();
    if (!subDevNics.empty()) {
        UBSE_LOG_ERROR << "Sub nic device is not empty";
        return UBSE_ERROR;
    }
    auto &subDevIdevs = dev->GetSubDevIdev();
    if (!subDevIdevs.empty()) {
        UBSE_LOG_ERROR << "Sub idev device is not empty";
        return UBSE_ERROR;
    }
    guidToDevice_.erase(dev->GetGuid());
    auto &allVmBusi = devIdToDevice_[DeviceTypeToUint8(CollectionDeviceType::VM_BUSINSTANCE)];
    if (allVmBusi.find(dev->GetGuid()) == allVmBusi.end()) {
        UBSE_LOG_ERROR << "No such device in devIdToDevice_ map, guid: " << dev->GetGuid();
        return UBSE_ERROR;
    }
    allVmBusi.erase(dev->GetGuid());
    return UBSE_OK;
}
UbseResult ResourceCollection::AddDevIdevPfe(const std::shared_ptr<CollectionDeviceUbCtrl> &ubCtrlDev,
                                             const std::shared_ptr<CollectionDeviceIdevPfe> &pfeDev)
{
    auto device = CollectionDevice::CollectionToBase(pfeDev);
    auto ret = SetDevice(device);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to add idev pfe";
        return ret;
    }
    ubCtrlDev->SetSubDevIdev(pfeDev);
    pfeDev->SetParentUbCtl(ubCtrlDev);
    return ret;
}

UbseResult ResourceCollection::AddDevIdevVfe(const std::shared_ptr<CollectionDeviceIdevPfe> &pfeDev,
                                             const std::shared_ptr<CollectionDeviceIdevVfe> &vfeDev)
{
    auto device = CollectionDevice::CollectionToBase(vfeDev);
    auto ret = SetDevice(device);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to add idev vfe";
        return ret;
    }
    pfeDev->SetSubDevIdev(vfeDev);
    vfeDev->SetParentPfe(pfeDev);
    return ret;
}
std::shared_ptr<CollectionDeviceUbCtrl> ConstructUbCtrlObject(const UbseMtiUbController &ubController)
{
    CollectDeviceLoc devLoc;
    devLoc.chipId = ubController.chipId;
    devLoc.dieId = ubController.dieId;
    return std::make_shared<CollectionDeviceUbCtrl>(devLoc);
}
std::shared_ptr<CollectionDeviceIdevPfe> ConstructIdevPfe(const UbseMtiIdevPfe &idevPfe)
{
    CollectDeviceLoc devLoc;
    devLoc.chipId = idevPfe.ubController.chipId;
    devLoc.dieId = idevPfe.ubController.dieId;
    devLoc.pfeId = idevPfe.pfeId;
    devLoc.guid = CollectionStringUtil::GuidToStr(idevPfe.guid);
    return std::make_shared<CollectionDeviceIdevPfe>(devLoc);
}
std::shared_ptr<CollectionDeviceIdevVfe> ConstructIdevVfe(const UbseMtiIdevVfe &idevVfe)
{
    CollectDeviceLoc devLoc;
    devLoc.chipId = idevVfe.ubController.chipId;
    devLoc.dieId = idevVfe.ubController.dieId;
    devLoc.pfeId = idevVfe.pfeId;
    devLoc.vfeId = idevVfe.vfeId;
    devLoc.guid = CollectionStringUtil::GuidToStr(idevVfe.guid);
    return std::make_shared<CollectionDeviceIdevVfe>(devLoc);
}
UbseResult ResourceCollection::CollectUbCtrlIdev()
{
    UBSE_LOG_INFO << "Start to collect ubctrl and idev";
    std::vector<UbseMtiIdevPfe> feList{};
    // 调用GetIdevFeList 来获取pfeList
    if (const UbseResult ret = UbseMtiUrma::GetInstance().GetIdevFeList(feList); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get fe list, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 遍历 pfeList
    for (const auto &idevPfe : feList) {
        // 提取ubController, 转换成std::shared_ptr<CollectionDeviceUbCtrl> ubCtrlDev 并SetDevice(ubContrller)
        auto ubCtrlDevPtr = ConstructUbCtrlObject(idevPfe.ubController);
        auto device = CollectionDevice::CollectionToBase(ubCtrlDevPtr);
        if (auto ret = SetDevice(device); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to add ub controller";
            return ret;
        }
        // 提取pfe
        auto idevPfePtr = ConstructIdevPfe(idevPfe);
        auto ret = AddDevIdevPfe(ubCtrlDevPtr, idevPfePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to add idev pfe";
            return ret;
        }
        // 提取vfe
        for (const auto &idevVfe : idevPfe.vfeList) {
            auto idevVfePtr = ConstructIdevVfe(idevVfe);
            if (ret = AddDevIdevVfe(idevPfePtr, idevVfePtr); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to add idev vfe";
                return ret;
            }
        }
    }
    UBSE_LOG_INFO << "Success to collect ubctrl and idev";
    return UBSE_OK;
}

void GetDavidPfeLoc(const UbseMtiDavid &david, const UbseMtiIdevPfe &idevPfe, CollectDeviceLoc &davidDevLoc,
                    CollectDeviceLoc &pfeDevLoc)
{
    davidDevLoc.slotId = david.slotId;
    davidDevLoc.chipId = david.chipId;
    pfeDevLoc.chipId = idevPfe.ubController.chipId;
    pfeDevLoc.dieId = idevPfe.ubController.dieId;
    pfeDevLoc.pfeId = idevPfe.pfeId;
}

UbseResult ResourceCollection::AddDavidAndBindToIdevPfe(CollectDeviceLoc &davidDevLoc, CollectDeviceLoc &pfeDevLoc)
{
    auto davidDev = std::make_shared<CollectionDeviceDavid>(davidDevLoc);
    // 1650pfevfepfe
    auto pfeDevId = CollectionDevice::GetDevIdByDevLoc(pfeDevLoc, CollectionDeviceType::P_IDEV);
    auto existPfeBase = GetDeviceByDevId(pfeDevId, CollectionDeviceType::P_IDEV);
    if (existPfeBase == nullptr) {
        UBSE_LOG_ERROR << "Pfe device is not exist, devId: " << pfeDevId;
        return UBSE_ERROR;
    }
    auto existPfe = CollectionDevice::CollectionToDerived<CollectionDeviceIdevPfe>(existPfeBase);
    if (existPfe == nullptr) {
        UBSE_LOG_ERROR << "Pfe is nullptr";
        return UBSE_ERROR;
    }
    // slotId chipId 0xff, pfe
    if (davidDevLoc.slotId == SPECIAL_FE_VAL && davidDevLoc.chipId == SPECIAL_FE_VAL) {
        // pfe
        existPfe->SetIsComSharedFe(true);
        for (const auto &vfe : existPfe->GetSubDevVfe()) {
            if (vfe == nullptr) {
                UBSE_LOG_ERROR << "Vfe is null";
                continue;
            }
            vfe->SetIsComSharedFe(true);
        }
    } else {
        auto device = CollectionDevice::CollectionToBase(davidDev);
        if (auto ret = SetDevice(device); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to add david device";
            return ret;
        }
        if (auto ret = BindDevice(davidDev, existPfe); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to bind david and idev pfe, ret: " << ret;
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult ResourceCollection::CollectIdevPfeDavid()
{
    UBSE_LOG_INFO << "Start to collect david and bind idev with david";
    UbseMtiIdevFeDavidMapping davidDevMapping{};
    if (const auto ret = UbseMtiUrma::GetInstance().GetIdevFeDavidMapping(davidDevMapping); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get david mapping, ret: " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &[david, idevPfe] : davidDevMapping) {
        CollectDeviceLoc davidDevLoc;
        CollectDeviceLoc pfeDevLoc;
        GetDavidPfeLoc(david, idevPfe, davidDevLoc, pfeDevLoc);
        if (auto ret = AddDavidAndBindToIdevPfe(davidDevLoc, pfeDevLoc); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to add david and bind pfe";
            return ret;
        }
    }
    UBSE_LOG_INFO << "Success to collect david and bind idev with david";
    return UBSE_OK;
}
void GetUbCtrlLoc(const UbseMti1825Pf &feInfo, CollectDeviceLoc &ubCtrlLoc)
{
    ubCtrlLoc.slotId = feInfo.affinityUbController.slotId;
    ubCtrlLoc.chipId = feInfo.affinityUbController.chipId;
    ubCtrlLoc.dieId = feInfo.affinityUbController.dieId;
}

void GetNicFeLoc(const UbseMti1825Vf &mti1825Vf, CollectDeviceLoc &nicFeLoc)
{
    nicFeLoc.slotId = mti1825Vf.slotId;
    nicFeLoc.chipId = mti1825Vf.chipId;
    nicFeLoc.dieId = mti1825Vf.dieId;
    nicFeLoc.pfeId = mti1825Vf.pfId;
    nicFeLoc.vfeId = mti1825Vf.vfId;
    nicFeLoc.guid = CollectionStringUtil::GuidToStr(mti1825Vf.guid);
}

UbseResult ResourceCollection::GetDevUbCtrlFromNicFeInfo(const UbseMti1825Pf &feInfo,
                                                         std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl)
{
    CollectDeviceLoc ubCtrlLoc;
    GetUbCtrlLoc(feInfo, ubCtrlLoc);
    auto devUbCtrlBase =
        GetDeviceByDevId(CollectionDevice::GetDevIdByDevLoc(ubCtrlLoc, CollectionDeviceType::UBCONTROLLER),
                         CollectionDeviceType::UBCONTROLLER);
    devUbCtrl = CollectionDevice::CollectionToDerived<CollectionDeviceUbCtrl>(devUbCtrlBase);
    if (devUbCtrl == nullptr) {
        UBSE_LOG_ERROR << "Failed to get ub controller device";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult ResourceCollection::AddNicFeAndSetAffinity(const UbseMti1825Vf &mti1825Vf,
                                                      std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl)
{
    CollectDeviceLoc nicFeLoc;
    GetNicFeLoc(mti1825Vf, nicFeLoc);
    auto devNic = std::make_shared<CollectionDeviceNic>(nicFeLoc);
    if (!ValidateGuid(nicFeLoc.guid)) {
        UBSE_LOG_ERROR << "Failed to transfer guid from array to string";
        return UBSE_ERROR;
    }
    auto device = CollectionDevice::CollectionToBase(devNic);
    auto ret = SetDevice(device);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to add nic device";
        return ret;
    }
    devUbCtrl->SetAffinityDevNic(devNic);
    devNic->SetAffinityDevUbCtrl(devUbCtrl);
    return UBSE_OK;
}

UbseResult ResourceCollection::CollectAffinityNic()
{
    std::vector<UbseMti1825Pf> pfList{};
    auto ret = UbseMti1825::GetInstance().Get1825FeList(pfList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get list of nfe fe operations";
        return ret;
    }
    for (auto &feInfo : pfList) {
        std::shared_ptr<CollectionDeviceUbCtrl> devUbCtrl;
        if (ret = GetDevUbCtrlFromNicFeInfo(feInfo, devUbCtrl); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get ub controller from nic fe info";
            return ret;
        }
        for (auto mti1825Vf : feInfo.vfList) {
            if (ret = AddNicFeAndSetAffinity(mti1825Vf, devUbCtrl); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to add nic fe: {" << mti1825Vf.slotId << ", " << mti1825Vf.chipId << ", "
                               << mti1825Vf.dieId << ", " << mti1825Vf.pfId << ", " << mti1825Vf.vfId << "}";
                return ret;
            }
        }
    }
    return UBSE_OK;
}
std::shared_ptr<CollectionDeviceIdevVfe> ResourceCollection::GetIdevVfeByGuid(const std::string &guid)
{
    for (auto &kv : devIdToDevice_[static_cast<uint8_t>(CollectionDeviceType::V_IDEV)]) {
        auto &vfe = kv.second;
        if (vfe->GetGuid() == guid) {
            return CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(vfe);
        }
    }
    UBSE_LOG_WARN << "Can not find vfe by guid: " << guid;
    return nullptr;
}

UbseResult ResourceCollection::QueryBusiSubDevices(const std::vector<UbseMtiGuid> &guids,
                                                   std::shared_ptr<CollectionDeviceBusi> &devBusi)
{
    for (const auto &mtiGuid : guids) {
        CollectionGuid guid = CollectionStringUtil::GuidToStr(mtiGuid);
        if (!ValidateGuid(guid)) {
            UBSE_LOG_ERROR << "Invalid guid: " << guid;
            return UBSE_ERROR;
        }
        // 1825
        if (auto devNicBase = GetDeviceByGuid(guid);
            devNicBase != nullptr && devNicBase->GetType() == CollectionDeviceType::NIC) {
            auto devNic = CollectionDevice::CollectionToDerived<CollectionDeviceNic>(devNicBase);
            if (devNic == nullptr) {
                UBSE_LOG_ERROR << "Failed to convert to 1825, guid: " << guid;
                return UBSE_ERROR;
            }
            BindDevice(devBusi, devNic);
        } else if (auto devVfe = GetIdevVfeByGuid(guid);
                   devVfe != nullptr && devVfe->GetType() == CollectionDeviceType::V_IDEV) {
            if (devBusi->GetType() != CollectionDeviceType::VM_BUSINSTANCE) {
                continue; // VM bus instance vfe
            }
            BindDevice(devBusi, devVfe);
        } else {
            UBSE_LOG_WARN << "Device not found via guid: " << guid;
        }
    }
    return UBSE_OK;
}

UbseResult ResourceCollection::CollectBusInstance()
{
    std::vector<UbseMtiBusInst> busInstanceList{};
    auto ret = UbseMtiBusInstance::GetInstance().GetBusInstanceList(busInstanceList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetBusInstanceList failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    for (const auto &busInstanceInfo : busInstanceList) {
        std::shared_ptr<CollectionDeviceBusi> devBusi;
        CollectionGuid guid = CollectionStringUtil::GuidToStr(busInstanceInfo.guid);
        if (auto baseDev = GetDeviceByGuid(guid); baseDev != nullptr) {
            devBusi = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(baseDev);
        } else {
            devBusi = std::make_shared<CollectionDeviceBusi>(
                guid, busInstanceInfo.eid, std::to_string(busInstanceInfo.upi),
                busInstanceInfo.type == UbseMtiBusInstanceType::HOST ? CollectionDeviceType::HOST_BUSINSTANCE :
                                                                       CollectionDeviceType::VM_BUSINSTANCE);
            auto devBusiBase = CollectionDevice::CollectionToBase(devBusi);
            if (devBusiBase == nullptr) {
                UBSE_LOG_ERROR << "Failed to convert to base device";
                return UBSE_ERROR;
            }

            if (ret = SetDevice(devBusiBase); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Cannot add bus instance, guid: " << devBusiBase->GetGuid();
                return ret;
            }
        }
        if (devBusi == nullptr) {
            UBSE_LOG_ERROR << "Bus instance is null";
            return UBSE_ERROR;
        }
        if (ret = QueryBusiSubDevices(busInstanceInfo.subDeviceGuids, devBusi); ret != UBSE_OK) {
            return ret;
        }
    }
    UBSE_LOG_INFO << "Success to get bus instance and sub devices from sysfs";
    return UBSE_OK;
}

UbseResult CheckDevTopoIdevVfe(int &cnt, const std::shared_ptr<CollectionDeviceIdevVfe> &devVfe)
{
    auto devDavid = devVfe->GetBondingDevDavid();
    if (devDavid != nullptr) {
        if (devDavid->GetBondingIdev() != CollectionDevice::CollectionToBase(devVfe)) {
            UBSE_LOG_ERROR << "No link from david to vfe";
            return UBSE_ERROR;
        }
        UBSE_LOG_INFO << "Found a relation, vfe: " << devVfe->GetIdStr() << " to david: " << devDavid->GetIdStr();
        ++cnt;
    }
    return UBSE_OK;
}

UbseResult CheckDevTopoIdevPfe(int &cnt, const std::shared_ptr<CollectionDeviceIdevPfe> &devPfe)
{
    for (auto &devVfe : devPfe->GetSubDevVfe()) {
        if (devVfe == nullptr) {
            UBSE_LOG_ERROR << "devVfe is nullptr";
            return UBSE_ERROR;
        }
        if (devVfe->GetParentPfe() != devPfe) {
            UBSE_LOG_ERROR << "No link from pfe to ub ctrl";
            return UBSE_ERROR;
        }
        if (CheckDevTopoIdevVfe(cnt, devVfe) != UBSE_OK) {
            UBSE_LOG_ERROR << "Check idev vfe fail";
            return UBSE_ERROR;
        }
        ++cnt;
    }
    auto devDavid = devPfe->GetBondingDevDavid();
    if (devDavid != nullptr) {
        if (devDavid->GetBondingIdev() != CollectionDevice::CollectionToBase(devPfe)) {
            UBSE_LOG_ERROR << "No link from david to pfe";
            return UBSE_ERROR;
        }
        UBSE_LOG_INFO << "Found a relation, pfe: " << devPfe->GetIdStr() << " to david: " << devDavid->GetIdStr();
        ++cnt;
    }
    return UBSE_OK;
}
UbseResult CheckDevTopoForUbCtrlSubIdev(int &cnt, std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl)
{
    auto &devSubIdevVec = devUbCtrl->GetSubDevIdev();
    for (auto &devIdevPfe : devSubIdevVec) {
        if (devIdevPfe == nullptr) {
            UBSE_LOG_ERROR << "Idev pfe is nullptr";
            return UBSE_ERROR;
        }
        if (devIdevPfe->GetParentUbCtl() != devUbCtrl) {
            UBSE_LOG_ERROR << "No link from pfe to ub ctrl";
            return UBSE_ERROR;
        }
        if (CheckDevTopoIdevPfe(cnt, devIdevPfe) != UBSE_OK) {
            UBSE_LOG_ERROR << "Check pfe fail";
            return UBSE_ERROR;
        }
        ++cnt;
    }
    return UBSE_OK;
}
UbseResult CheckDevTopoForUbCtrlAffiDevNic(int &cnt, std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl)
{
    for (auto &devAffinityNic : devUbCtrl->GetAffiDevNic()) {
        if (devAffinityNic == nullptr) {
            continue;
        }
        if (devAffinityNic->GetAffinityDevUbCtrl() != devUbCtrl) {
            UBSE_LOG_ERROR << "No link from nic to ub ctrl";
            return UBSE_ERROR;
        }
        ++cnt;
    }
    return UBSE_OK;
}

UbseResult CheckDevTopoUbCtrl(int &cnt, CollectionDevIdToDevice &devUbCtrls)
{
    for (auto &devUbCtrlKv : devUbCtrls) {
        auto devUbCtrlBase = devUbCtrlKv.second;
        auto devUbCtrl = CollectionDevice::CollectionToDerived<CollectionDeviceUbCtrl>(devUbCtrlBase);
        if (devUbCtrl == nullptr) {
            UBSE_LOG_ERROR << "ub ctrl is nullptr";
            return UBSE_ERROR;
        }
        UBSE_LOG_INFO << "Check ub ctrl, devId: " << devUbCtrlKv.first;
        if (CheckDevTopoForUbCtrlSubIdev(cnt, devUbCtrl) != UBSE_OK) {
            UBSE_LOG_ERROR << "Check ub ctrl sub idev fail";
            return UBSE_ERROR;
        }
        if (CheckDevTopoForUbCtrlAffiDevNic(cnt, devUbCtrl) != UBSE_OK) {
            UBSE_LOG_ERROR << "Check ub ctrl affi nic fail";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult CheckDevTopoBusi(int &cnt, CollectionGuidToDevice &devBusis)
{
    for (auto &devBusiKv : devBusis) {
        auto devBusiBase = devBusiKv.second;
        if (devBusiBase == nullptr || !IsBusInstanceType(devBusiBase)) {
            continue;
        }
        auto devBusi = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(devBusiBase);
        if (devBusi == nullptr) {
            UBSE_LOG_ERROR << "UbCtrl is nullptr";
            return UBSE_ERROR;
        }
        auto &devSubIdevVec = devBusi->GetSubDevIdev();
        for (auto &devIdevVfe : devSubIdevVec) {
            if (devIdevVfe == nullptr) {
                UBSE_LOG_ERROR << "Idev vfe is nullptr";
                return UBSE_ERROR;
            }
            auto bondingBusiVec = devIdevVfe->GetBondingDevBusi();
            if (std::find(bondingBusiVec.begin(), bondingBusiVec.end(), devBusi) == bondingBusiVec.end()) {
                UBSE_LOG_ERROR << "No link from vfe to bus instance";
                return UBSE_ERROR;
            }
            if (CheckDevTopoIdevVfe(cnt, devIdevVfe) != UBSE_OK) {
                UBSE_LOG_ERROR << "Check idev vfe fail";
                return UBSE_ERROR;
            }
            ++cnt;
        }
        for (auto &devNic : devBusi->GetSubDevNic()) {
            if (devNic == nullptr) {
                UBSE_LOG_ERROR << "Nic is nullptr";
                return UBSE_ERROR;
            }
            if (devNic->GetBondingDevBusi() != devBusi) {
                UBSE_LOG_ERROR << "No link from nic to bus instance";
                return UBSE_ERROR;
            }
            if (devNic->GetAffinityDevUbCtrl() == nullptr) {
                UBSE_LOG_ERROR << "No link from nic to ub ctrl";
                return UBSE_ERROR;
            }
            ++cnt;
        }
    }
    return UBSE_OK;
}
UbseResult ResourceCollection::CheckDevTopo(int &cnt)
{
    cnt = 0;
    if (CheckDevTopoUbCtrl(cnt, devIdToDevice_[static_cast<uint8_t>(CollectionDeviceType::UBCONTROLLER)]) != UBSE_OK) {
        UBSE_LOG_ERROR << "Check ub ctrl fail";
        return UBSE_ERROR;
    }
    if (CheckDevTopoBusi(cnt, guidToDevice_) != UBSE_OK) {
        UBSE_LOG_ERROR << "Check bus instance fail";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::npu::controller
