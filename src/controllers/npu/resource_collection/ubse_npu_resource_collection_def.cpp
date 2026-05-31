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
#include "ubse_npu_resource_collection_def.h"
#include <optional>
#include "ubse_logger.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"
namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::mti::bus_instance;
using namespace ubse::mti::urma;
static const std::string DEV_NIC_PFE = "devNicPfe";
static const std::string DEV_NIC_VFE = "devNicVfe";
static const std::string IDEV_VFE = "idevVfe";
static const std::string DEV_IDEV = "devIdev";
static const std::string DEV_BUSI = "devBusi";
CollectDeviceLoc::CollectDeviceLoc() : slotId(0xff), chipId(0xff), dieId(0xff), pfeId(0xff), vfeId(0xff) {}
CollectDeviceLoc::CollectDeviceLoc(const UbseMtiEid& eid, const CollectionGuid& guid, const CollectionUpi& upi)
    : slotId(0xff),
      chipId(0xff),
      dieId(0xff),
      pfeId(0xff),
      vfeId(0xff),
      eid(eid),
      guid(guid),
      upi(upi)
{
}
CollectionDevice::CollectionDevice(CollectDeviceLoc devLoc, CollectionDeviceType devType)
    : dev(std::move(devLoc)),
      type(devType)
{
}
CollectionDevice::CollectionDevice(const CollectionDevId& guid, const UbseMtiEid& eid, const CollectionUpi& upi,
                                   CollectionDeviceType devType)
    : dev(eid, guid, upi),
      type(devType)
{
}
void CollectionDevice::SetEid(const UbseMtiEid& eid)
{
    dev.eid = eid;
}
void CollectionDevice::SetGuid(const CollectionDevId& guid)
{
    dev.guid = guid;
}
void CollectionDevice::SetType(const CollectionDeviceType devType)
{
    type = devType;
}
CollectionDevId CollectionDevice::GetGuid() const
{
    return dev.guid;
}
UbseMtiEid CollectionDevice::GetEid() const
{
    return dev.eid;
}
CollectionDeviceType CollectionDevice::GetType() const
{
    return type;
}
CollectionDevId CollectionDevice::GetDevIdByDevLoc(const CollectDeviceLoc& devLoc, const CollectionDeviceType devType)
{
    auto devTypeVal = DeviceTypeToUint8(devType);
    if (devType >= CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT || devTypeVal >= GET_DEVID_FUNCTION_NUM) {
        UBSE_LOG_ERROR << "Invalid collection device type: " << static_cast<uint32_t>(devType);
        return "";
    }
    return GET_DEVID_FUNCTION_TABLE[devTypeVal](devLoc);
}
CollectDeviceLoc CollectionDevice::GetDeviceLoc()
{
    return dev;
}
CollectionDeviceBusi::CollectionDeviceBusi(const CollectionDevId& guid, const mti::bus_instance::UbseMtiEid& eid,
                                           const CollectionUpi& upi, CollectionDeviceType devType)
    : CollectionDevice(guid, eid, upi, devType)
{
}
CollectionDevId CollectionDeviceBusi::GetIdStr()
{
    return dev.guid;
}
CollectionUpi CollectionDeviceBusi::GetUpiStr()
{
    return dev.upi;
}
void CollectionDeviceBusi::SetUpiStr(const CollectionUpi& upi)
{
    dev.upi = upi;
}
const std::vector<std::shared_ptr<CollectionDeviceNicPfe>>& CollectionDeviceBusi::GetSubDevNicPfe() const
{
    return subDevNicPfe;
}
const std::vector<std::shared_ptr<CollectionDeviceNicVfe>>& CollectionDeviceBusi::GetSubDevNicVfe() const
{
    return subDevNicVfe;
}
const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>>& CollectionDeviceBusi::GetSubDevIdev() const
{
    return subDevIdev;
}

template <typename T>
void SetCollectionDevice(std::vector<std::shared_ptr<T>>& devices, const std::shared_ptr<T>& device,
                         const std::string& name)
{
    if (device == nullptr) {
        UBSE_LOG_ERROR << name << " is null";
        return;
    }
    auto iter = std::find_if(devices.begin(), devices.end(),
                             [&](const auto& dev) { return dev != nullptr && dev->GetIdStr() == device->GetIdStr(); });
    if (iter != devices.end()) {
        UBSE_LOG_DEBUG << name << " exist";
        return;
    }
    devices.emplace_back(device);
}

template <typename T>
void SetCollectionDevice(std::vector<std::weak_ptr<T>>& devices, const std::shared_ptr<T>& device,
                         const std::string& name)
{
    if (device == nullptr) {
        UBSE_LOG_ERROR << name << " is null";
        return;
    }
    auto iter = std::find_if(devices.begin(), devices.end(), [&](const auto& devWeak) {
        auto dev = devWeak.lock();
        return dev != nullptr && dev->GetIdStr() == device->GetIdStr();
    });
    if (iter != devices.end()) {
        UBSE_LOG_DEBUG << name << " exist";
        return;
    }
    devices.emplace_back(device);
}

template <typename T>
void RemoveCollectionDevice(std::vector<std::shared_ptr<T>>& devices, const std::shared_ptr<T>& device,
                            const std::string& name)
{
    if (device == nullptr) {
        UBSE_LOG_ERROR << name << " is null";
        return;
    }
    devices.erase(
        std::remove_if(devices.begin(), devices.end(),
                       [&](const auto& dev) { return dev == nullptr || dev->GetIdStr() == device->GetIdStr(); }),
        devices.end());
}

template <typename T>
void RemoveCollectionDevice(std::vector<std::weak_ptr<T>>& devices, const std::shared_ptr<T>& device,
                            const std::string& name)
{
    if (device == nullptr) {
        UBSE_LOG_ERROR << name << " is null";
        return;
    }
    devices.erase(std::remove_if(devices.begin(), devices.end(),
                                 [&](const auto& devWeak) {
                                     auto dev = devWeak.lock();
                                     return dev == nullptr || dev->GetIdStr() == device->GetIdStr();
                                 }),
                  devices.end());
}

void CollectionDeviceBusi::SetSubDevNicPfe(const std::shared_ptr<CollectionDeviceNicPfe>& devNic)
{
    SetCollectionDevice<CollectionDeviceNicPfe>(subDevNicPfe, devNic, DEV_NIC_PFE);
}

void CollectionDeviceBusi::SetSubDevNicVfe(const std::shared_ptr<CollectionDeviceNicVfe>& devNic)
{
    SetCollectionDevice<CollectionDeviceNicVfe>(subDevNicVfe, devNic, DEV_NIC_VFE);
}

void CollectionDeviceBusi::SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe>& devIdev)
{
    SetCollectionDevice<CollectionDeviceIdevVfe>(subDevIdev, devIdev, DEV_IDEV);
}

void CollectionDeviceBusi::RemoveSubDevNicPfe(const std::shared_ptr<CollectionDeviceNicPfe>& devNic)
{
    RemoveCollectionDevice<CollectionDeviceNicPfe>(subDevNicPfe, devNic, DEV_NIC_PFE);
}
void CollectionDeviceBusi::RemoveSubDevNicVfe(const std::shared_ptr<CollectionDeviceNicVfe>& devNic)
{
    RemoveCollectionDevice<CollectionDeviceNicVfe>(subDevNicVfe, devNic, DEV_NIC_VFE);
}
void CollectionDeviceBusi::RemoveSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe>& devIdev)
{
    RemoveCollectionDevice<CollectionDeviceIdevVfe>(subDevIdev, devIdev, DEV_IDEV);
}
CollectionDeviceUbCtrl::CollectionDeviceUbCtrl(const CollectDeviceLoc& devLoc)
    : CollectionDevice(devLoc, CollectionDeviceType::UBCONTROLLER)
{
}
CollectionDevId CollectionDeviceUbCtrl::GetIdStr()
{
    return CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(type), dev.chipId, dev.dieId);
}
const std::vector<std::shared_ptr<CollectionDeviceIdevPfe>>& CollectionDeviceUbCtrl::GetSubDevIdev()
{
    return subDevIdev;
}
void CollectionDeviceUbCtrl::SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevPfe>& idevPfe)
{
    SetCollectionDevice<CollectionDeviceIdevPfe>(subDevIdev, idevPfe, IDEV_VFE);
}
CollectionDeviceIdev::CollectionDeviceIdev(const CollectDeviceLoc& devLoc, CollectionDeviceType type)
    : CollectionDevice(devLoc, type),
      isComSharedFe(false)
{
}
std::shared_ptr<CollectionDeviceDavid> CollectionDeviceIdev::GetBondingDevDavid() const
{
    return bondingDevDavid.lock();
}
void CollectionDeviceIdev::SetBondingDevDavid(const std::shared_ptr<CollectionDeviceDavid>& devDavid)
{
    bondingDevDavid = devDavid;
}
void CollectionDeviceIdev::RemoveBondingDevDavid()
{
    bondingDevDavid.reset();
}
void CollectionDeviceIdev::SetIsComSharedFe(const bool comSharedFe)
{
    this->isComSharedFe = comSharedFe;
}
bool CollectionDeviceIdev::GetIsComSharedFe() const
{
    return isComSharedFe;
}
CollectionDeviceIdevPfe::CollectionDeviceIdevPfe(const CollectDeviceLoc& devLoc)
    : CollectionDeviceIdev(devLoc, CollectionDeviceType::P_IDEV)
{
}
CollectionDevId CollectionDeviceIdevPfe::GetIdStr()
{
    return CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(type), dev.chipId, dev.dieId, dev.pfeId);
}
std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> CollectionDeviceIdevPfe::GetSubDevVfe()
{
    return subDevIdev;
}
void CollectionDeviceIdevPfe::SetParentUbCtl(const std::shared_ptr<CollectionDeviceUbCtrl>& devUbCtrl)
{
    parentUbCtl = devUbCtrl;
}
std::shared_ptr<CollectionDeviceUbCtrl> CollectionDeviceIdevPfe::GetParentUbCtl() const
{
    return parentUbCtl.lock();
}
void CollectionDeviceIdevPfe::SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe>& idevVfe)
{
    SetCollectionDevice<CollectionDeviceIdevVfe>(subDevIdev, idevVfe, IDEV_VFE);
}
CollectionDeviceIdevVfe::CollectionDeviceIdevVfe(const CollectDeviceLoc& devLoc)
    : CollectionDeviceIdev(devLoc, CollectionDeviceType::V_IDEV)
{
}
CollectionDevId CollectionDeviceIdevVfe::GetIdStr()
{
    return CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(type), dev.chipId, dev.dieId, dev.pfeId,
                                                   dev.vfeId);
}
std::shared_ptr<CollectionDeviceIdevPfe> CollectionDeviceIdevVfe::GetParentPfe() const
{
    return parentPfe.lock();
}
std::vector<std::shared_ptr<CollectionDeviceBusi>> CollectionDeviceIdevVfe::GetBondingDevBusi() const
{
    std::vector<std::shared_ptr<CollectionDeviceBusi>> bondingBusi;
    for (auto& busiWeak : bondingDevBusi) {
        auto busiShared = busiWeak.lock();
        if (busiShared != nullptr) {
            bondingBusi.emplace_back(busiShared);
        }
    }
    return bondingBusi;
}
void CollectionDeviceIdevVfe::SetParentPfe(const std::shared_ptr<CollectionDeviceIdevPfe>& idevPfe)
{
    parentPfe = idevPfe;
}
void CollectionDeviceIdevVfe::SetBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi>& devBusi)
{
    SetCollectionDevice<CollectionDeviceBusi>(bondingDevBusi, devBusi, DEV_BUSI);
}
void CollectionDeviceIdevVfe::RemoveBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi>& devBusi)
{
    RemoveCollectionDevice<CollectionDeviceBusi>(bondingDevBusi, devBusi, DEV_BUSI);
}
CollectionDeviceDavid::CollectionDeviceDavid(const CollectDeviceLoc& devLoc)
    : CollectionDevice(devLoc, CollectionDeviceType::NPU)
{
}
CollectionDevId CollectionDeviceDavid::GetIdStr()
{
    return CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(type), dev.slotId, dev.chipId);
}
std::shared_ptr<CollectionDevice> CollectionDeviceDavid::GetBondingIdev()
{
    return bondingIdev;
}
void CollectionDeviceDavid::SetBondingIdev(const std::shared_ptr<CollectionDevice>& idev)
{
    bondingIdev = idev;
}
void CollectionDeviceDavid::RemoveBondingIdev()
{
    bondingIdev.reset();
}
std::shared_ptr<CollectionDeviceNicPfe> CollectionDeviceDavid::GetAffinityDevNicPfe()
{
    return affinityDevNicPfe;
}
void CollectionDeviceDavid::SetAffinityDevNicPfe(const std::shared_ptr<CollectionDeviceNicPfe>& nicPfe)
{
    affinityDevNicPfe = nicPfe;
}
std::shared_ptr<CollectionDeviceNicVfe> CollectionDeviceDavid::GetAffinityDevNicVfe()
{
    return affinityDevNicVfe;
}
void CollectionDeviceDavid::SetAffinityDevNicVfe(const std::shared_ptr<CollectionDeviceNicVfe>& nicVfe)
{
    affinityDevNicVfe = nicVfe;
}
CollectionDeviceNic::CollectionDeviceNic(const CollectDeviceLoc& devLoc, CollectionDeviceType type)
    : CollectionDevice(devLoc, type)
{
}
std::shared_ptr<CollectionDeviceBusi> CollectionDeviceNic::GetBondingDevBusi()
{
    return bondingDevBusi.lock();
}
void CollectionDeviceNic::SetBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi>& devBusi)
{
    bondingDevBusi = devBusi;
}
void CollectionDeviceNic::RemoveBondingDevBusi()
{
    bondingDevBusi.reset();
}

std::shared_ptr<CollectionDeviceDavid> CollectionDeviceNic::GetAffinityDevDavid()
{
    return affinityDevDavid.lock();
}

void CollectionDeviceNic::SetAffinityDevDavid(const std::shared_ptr<CollectionDeviceDavid>& devDavid)
{
    affinityDevDavid = devDavid;
}

CollectionDeviceNicPfe::CollectionDeviceNicPfe(const CollectDeviceLoc& devLoc)
    : CollectionDeviceNic(devLoc, CollectionDeviceType::NIC_PFE)
{
}

CollectionDevId CollectionDeviceNicPfe::GetIdStr()
{
    return CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(type), dev.slotId, dev.chipId, dev.pfeId);
}

std::vector<std::shared_ptr<CollectionDeviceNicVfe>> CollectionDeviceNicPfe::GetSubNicVfe()
{
    return subNicVfe;
}

void CollectionDeviceNicPfe::SetSubNicVfe(const std::shared_ptr<CollectionDeviceNicVfe>& nicVfe)
{
    SetCollectionDevice(subNicVfe, nicVfe, DEV_NIC_VFE);
}

CollectionDeviceNicVfe::CollectionDeviceNicVfe(const CollectDeviceLoc& devLoc)
    : CollectionDeviceNic(devLoc, CollectionDeviceType::NIC_VFE)
{
}

CollectionDevId CollectionDeviceNicVfe::GetIdStr()
{
    return CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(type), dev.slotId, dev.chipId, dev.pfeId,
                                                   dev.vfeId);
}

std::shared_ptr<CollectionDeviceNicPfe> CollectionDeviceNicVfe::GetParentNicPfe() const
{
    return parentNicPfe.lock();
}

void CollectionDeviceNicVfe::SetParentNicPfe(const std::shared_ptr<CollectionDeviceNicPfe>& nicPfe)
{
    parentNicPfe = nicPfe;
}

} // namespace ubse::npu::controller
