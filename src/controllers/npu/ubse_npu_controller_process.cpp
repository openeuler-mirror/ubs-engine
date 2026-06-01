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
#include "ubse_npu_controller_process.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_npu_resource_collection.h"
#include "ubse_npu_resource_collection_def.h"
#include "ubse_npu_source_def.h"

namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::common::def;

UbseResult UbseNpuControllerProcess::ProcessNpuDevice(const UbDevice& ubDevice,
                                                      std::vector<std::shared_ptr<IResource>>& devList)
{
    UBSE_LOG_DEBUG << "ProcessNpuDevice start, npu " << ubDevice.slotId << "-" << ubDevice.chipId;
    auto& collection = ResourceCollection::GetInstance();
    CollectionDevId locStr = CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(CollectionDeviceType::NPU),
                                                                     ubDevice.slotId, ubDevice.chipId);
    std::shared_ptr<CollectionDeviceDavid> npu = CollectionDevice::CollectionToDerived<CollectionDeviceDavid>(
        collection.GetDeviceByDevId(locStr, CollectionDeviceType::NPU));
    if (npu == nullptr) {
        UBSE_LOG_ERROR << "ProcessNpuDevice failed, npu " << ubDevice.slotId << "-" << ubDevice.chipId
                       << " is not found";
        return UBSE_ERROR;
    }
    std::shared_ptr<NpuResource> npuRes = std::make_shared<NpuResource>();
    DeviceNpuToResource(npu, npuRes);
    devList.push_back(npuRes);
    UBSE_LOG_DEBUG << "ProcessNpuDevice end, npu " << ubDevice.slotId << "-" << ubDevice.chipId;
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::ProcessNicPfeDevice(const UbDevice& ubDevice,
                                                         std::vector<std::shared_ptr<IResource>>& devList)
{
    UBSE_LOG_DEBUG << "ProcessNicPfeDevice start, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.pfId;
    auto& collection = ResourceCollection::GetInstance();
    CollectionDevId locStr = CollectionStringUtil::CollectionJoinStr(
        static_cast<uint8_t>(CollectionDeviceType::NIC_PFE), ubDevice.slotId, ubDevice.chipId, ubDevice.pfId);
    std::shared_ptr<CollectionDeviceNicPfe> nic = CollectionDevice::CollectionToDerived<CollectionDeviceNicPfe>(
        collection.GetDeviceByDevId(locStr, CollectionDeviceType::NIC_PFE));
    if (nic == nullptr) {
        UBSE_LOG_ERROR << "ProcessNicPfeDevice failed, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                       << ubDevice.pfId << " is not found";
        return UBSE_ERROR;
    }
    std::shared_ptr<NicPfeResource> nicRes = std::make_shared<NicPfeResource>();
    DeviceNicPfeToResource(nic, nicRes);
    devList.push_back(nicRes);
    UBSE_LOG_DEBUG << "ProcessNicPfeDevice end, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.pfId;
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::ProcessNicVfeDevice(const UbDevice& ubDevice,
                                                         std::vector<std::shared_ptr<IResource>>& devList)
{
    UBSE_LOG_DEBUG << "ProcessNicVfeDevice start, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.pfId << "-" << ubDevice.vfId;
    auto& collection = ResourceCollection::GetInstance();
    CollectionDevId locStr =
        CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(CollectionDeviceType::NIC_VFE), ubDevice.slotId,
                                                ubDevice.chipId, ubDevice.pfId, ubDevice.vfId);
    std::shared_ptr<CollectionDeviceNicVfe> nic = CollectionDevice::CollectionToDerived<CollectionDeviceNicVfe>(
        collection.GetDeviceByDevId(locStr, CollectionDeviceType::NIC_VFE));
    if (nic == nullptr) {
        UBSE_LOG_ERROR << "ProcessNicVfeDevice failed, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                       << ubDevice.pfId << "-" << ubDevice.vfId << " is not found";
        return UBSE_ERROR;
    }
    std::shared_ptr<NicVfeResource> nicRes = std::make_shared<NicVfeResource>();
    DeviceNicVfeToResource(nic, nicRes);
    devList.push_back(nicRes);
    UBSE_LOG_DEBUG << "ProcessNicVfeDevice end, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.pfId << "-" << ubDevice.vfId;
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::ProcessBusiResource(const UbseAllocRequest& requestInfo,
                                                         std::vector<std::shared_ptr<IResource>>& devList,
                                                         const std::string& newBusInstanceGuid)
{
    UBSE_LOG_DEBUG << "ProcessBusiResource start, bus instance " << newBusInstanceGuid;
    std::shared_ptr<BusiResource> busiRes = std::make_shared<BusiResource>();
    busiRes->SetGuid(newBusInstanceGuid);
    for (auto dev : requestInfo.ubDevList) {
        busiRes->AddSubDevice(dev);
    }
    devList.push_back(busiRes);
    UBSE_LOG_DEBUG << "ProcessBusiResource end, bus instance " << newBusInstanceGuid;
    return UBSE_OK;
}

std::shared_ptr<CollectionDeviceIdevPfe> UbseNpuControllerProcess::ProcessBondingDevice(
    const std::shared_ptr<CollectionDeviceDavid>& npu, std::shared_ptr<NpuResource>& npuRes)
{
    std::shared_ptr<CollectionDevice> idev = npu->GetBondingIdev();
    std::shared_ptr<CollectionDeviceIdevPfe> pfe;
    if (idev == nullptr) {
        UBSE_LOG_ERROR << "idev is nullptr";
        return nullptr;
    }
    if (idev->GetType() == CollectionDeviceType::V_IDEV) {
        UBSE_LOG_DEBUG << "DeviceNpuToResource npu " << npu->GetGuid() << " is bind with vfe";
        std::shared_ptr<CollectionDeviceIdevVfe> vfe =
            CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(idev);
        if (vfe == nullptr) {
            UBSE_LOG_ERROR << "vfe is nullptr";
            return nullptr;
        }
        std::vector<std::shared_ptr<CollectionDeviceBusi>> businstances = vfe->GetBondingDevBusi();
        if (businstances.empty()) {
            UBSE_LOG_ERROR << "DeviceNpuToResource failed, npu " << npu->GetGuid() << " can find vfe "
                           << idev->GetGuid() << ", but no busi is bound";
            return nullptr;
        }
        const std::shared_ptr<CollectionDeviceBusi>& busi = businstances[0];
        npuRes->SetBusInstanceGuid(busi->GetGuid());
        UBSE_LOG_DEBUG << "DeviceNpuToResource bus instance " << busi->GetGuid() << " added to npuRes";
        pfe = vfe->GetParentPfe();
    } else {
        UBSE_LOG_DEBUG << "DeviceNpuToResource npu " << npu->GetGuid()
                       << " is bind with pfe, npuRes.busInstanceGuid is empty";
        npuRes->SetBusInstanceGuid("");
        pfe = CollectionDevice::CollectionToDerived<CollectionDeviceIdevPfe>(idev);
    }
    return pfe;
}

UbseResult UbseNpuControllerProcess::DeviceNpuToResource(const std::shared_ptr<CollectionDeviceDavid>& npu,
                                                         std::shared_ptr<NpuResource>& npuRes)
{
    CollectDeviceLoc npuLoc = npu->GetDeviceLoc();
    npuRes->SetLoc(npuLoc.slotId, npuLoc.chipId);
    UBSE_LOG_DEBUG << "DeviceNpuToResource npu " << npuLoc.slotId << "-" << npuLoc.chipId << " added to npuRes";
    std::shared_ptr<CollectionDeviceIdevPfe> pfe = ProcessBondingDevice(npu, npuRes);
    if (!pfe) {
        UBSE_LOG_ERROR << "DeviceNpuToResource failed, npu " << npu->GetIdStr() << " can not find pfe";
        return UBSE_ERROR;
    }
    if (!pfe->GetSubDevVfe().empty()) {
        npuRes->SetGuid(pfe->GetSubDevVfe()[0]->GetGuid());
        UBSE_LOG_DEBUG << "DeviceNpuToResource vfe guid: " << pfe->GetSubDevVfe()[0]->GetGuid() << " added to npuRes";
    }

    std::shared_ptr<CollectionDeviceUbCtrl> ubCtl = pfe->GetParentUbCtl();
    if (!ubCtl) {
        UBSE_LOG_ERROR << "DeviceNpuToResource failed, npu " << npu->GetIdStr() << " can find pfe " << pfe->GetGuid()
                       << ", but no UBctrl is bind";
        return UBSE_ERROR;
    }

    CollectDeviceLoc ubctl = ubCtl->GetDeviceLoc();
    npuRes->AddAffinityDevice({ResourceType::UBCONTROLLER, ubctl.slotId, ubctl.chipId, ubctl.dieId});
    UBSE_LOG_DEBUG << "DeviceNpuToResource UBctrl " << ubctl.slotId << "-" << ubctl.chipId << "-" << ubctl.dieId
                   << " added to npuRes.affinityDevices";

    std::shared_ptr<CollectionDeviceNicPfe> affinityDevNicPfe = npu->GetAffinityDevNicPfe();
    if (!affinityDevNicPfe) {
        return UBSE_OK;
    }
    auto nicPfeLoc = affinityDevNicPfe->GetDeviceLoc();
    npuRes->AddAffinityDevice({.type = ResourceType::NIC_PFE,
                               .slotId = nicPfeLoc.slotId,
                               .chipId = nicPfeLoc.chipId,
                               .pfId = nicPfeLoc.pfeId});
    std::shared_ptr<CollectionDeviceNicVfe> affinityDevNicVfe = npu->GetAffinityDevNicVfe();
    if (!affinityDevNicVfe) {
        return UBSE_OK;
    }
    auto nicVfeLoc = affinityDevNicVfe->GetDeviceLoc();
    npuRes->AddAffinityDevice({.type = ResourceType::NIC_VFE,
                               .slotId = nicVfeLoc.slotId,
                               .chipId = nicVfeLoc.chipId,
                               .pfId = nicVfeLoc.pfeId,
                               .vfId = nicVfeLoc.vfeId});

    return UBSE_OK;
}

void UbseNpuControllerProcess::SetNicPfeLocation(const std::shared_ptr<CollectionDeviceNicPfe>& nic,
                                                 std::shared_ptr<NicPfeResource>& nicRes)
{
    CollectDeviceLoc nicLoc = nic->GetDeviceLoc();
    UBSE_LOG_DEBUG << "DeviceNicToResource start, nic " << nic->GetGuid() << " " << nicLoc.slotId << "-"
                   << nicLoc.chipId << "-" << nicLoc.pfeId << " added to nicRes";
    nicRes->SetLoc(nicLoc.slotId, nicLoc.chipId, nicLoc.pfeId);
}

void UbseNpuControllerProcess::SetNicVfeLocation(const std::shared_ptr<CollectionDeviceNicVfe>& nic,
                                                 std::shared_ptr<NicVfeResource>& nicRes)
{
    CollectDeviceLoc nicLoc = nic->GetDeviceLoc();
    UBSE_LOG_DEBUG << "DeviceNicToResource start, nic " << nic->GetGuid() << " " << nicLoc.slotId << "-"
                   << nicLoc.chipId << "-" << nicLoc.pfeId << "-" << nicLoc.vfeId << " added to nicRes";
    nicRes->SetLoc(nicLoc.slotId, nicLoc.chipId, nicLoc.pfeId, nicLoc.vfeId);
}

UbseResult UbseNpuControllerProcess::DeviceNicPfeToResource(const std::shared_ptr<CollectionDeviceNicPfe>& nic,
                                                            std::shared_ptr<NicPfeResource>& nicRes)
{
    // 1. 设置NIC位置信息
    SetNicPfeLocation(nic, nicRes);
    // 2. 设置NIC的GUID
    nicRes->SetGuid(nic->GetGuid());
    // 3. 处理绑定的Busi设备
    SetNicBusInstanceGuid<NicPfeResource>(nic, nicRes);
    std::shared_ptr<CollectionDeviceDavid> npu = nic->GetAffinityDevDavid();
    if (!npu) {
        return UBSE_OK;
    }
    auto npuLoc = npu->GetDeviceLoc();
    nicRes->AddAffinityDevice({.type = ResourceType::NPU, .slotId = npuLoc.slotId, .chipId = npuLoc.chipId});
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::DeviceNicVfeToResource(const std::shared_ptr<CollectionDeviceNicVfe>& nic,
                                                            std::shared_ptr<NicVfeResource>& nicRes)
{
    // 1. 设置NIC位置信息
    SetNicVfeLocation(nic, nicRes);
    // 2. 设置NIC的GUID
    nicRes->SetGuid(nic->GetGuid());
    // 3. 处理绑定的Busi设备
    SetNicBusInstanceGuid<NicVfeResource>(nic, nicRes);
    std::shared_ptr<CollectionDeviceDavid> npu = nic->GetAffinityDevDavid();
    if (!npu) {
        return UBSE_OK;
    }
    auto npuLoc = npu->GetDeviceLoc();
    nicRes->AddAffinityDevice({.type = ResourceType::NPU, .slotId = npuLoc.slotId, .chipId = npuLoc.chipId});
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::BusInstanceToResource(const std::shared_ptr<CollectionDeviceBusi>& busi,
                                                           std::shared_ptr<BusiResource>& busRes)
{
    UBSE_LOG_DEBUG << "BusInstanceToResource start, bus instance " << busi->GetGuid() << "added to busRes";
    busRes->SetGuid(busi->GetGuid());
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> nicPfes = busi->GetSubDevNicPfe();
    UBSE_LOG_DEBUG << "BusInstanceToResource nic pfe size: " << nicPfes.size();
    for (auto& nic : nicPfes) {
        CollectDeviceLoc nicLoc = nic->GetDeviceLoc();
        busRes->AddSubDevice(
            {.type = ResourceType::NIC_VFE, .slotId = nicLoc.slotId, .chipId = nicLoc.chipId, .pfId = nicLoc.pfeId});
        UBSE_LOG_DEBUG << "nic: " << nicLoc.slotId << "-" << nicLoc.chipId << "-" << nicLoc.pfeId
                       << " added to busRes.subDevices";
    }

    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> nicVfes = busi->GetSubDevNicVfe();
    UBSE_LOG_DEBUG << "BusInstanceToResource nic pfe size: " << nicVfes.size();
    for (auto& nic : nicVfes) {
        CollectDeviceLoc nicLoc = nic->GetDeviceLoc();
        busRes->AddSubDevice({.type = ResourceType::NIC_VFE,
                              .slotId = nicLoc.slotId,
                              .chipId = nicLoc.chipId,
                              .pfId = nicLoc.pfeId,
                              .vfId = nicLoc.pfeId});
        UBSE_LOG_DEBUG << "nic: " << nicLoc.slotId << "-" << nicLoc.chipId << "-" << nicLoc.pfeId << "-" << nicLoc.vfeId
                       << " added to busRes.subDevices";
    }

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfes = busi->GetSubDevIdev();
    UBSE_LOG_DEBUG << "BusInstanceToResource npu size: " << vfes.size();
    for (auto& vfe : vfes) {
        if (vfe == nullptr) {
            continue;
        }
        if (vfe->GetIsComSharedFe()) {
            continue;
        }
        std::shared_ptr<CollectionDeviceDavid> dev = vfe->GetBondingDevDavid();
        if (!dev) {
            UBSE_LOG_ERROR << "BusInstanceToResource failed, vfe " << vfe->GetGuid() << " bonding npu is null";
            continue;
        }
        CollectDeviceLoc devLoc = dev->GetDeviceLoc();
        busRes->AddSubDevice({ResourceType::NPU, devLoc.slotId, devLoc.chipId, devLoc.dieId});
        UBSE_LOG_DEBUG << "npu " << devLoc.slotId << "-" << devLoc.chipId << "-" << devLoc.dieId
                       << " added to busRes.subDevices";
    }
    return UBSE_OK;
}

} // namespace ubse::npu::controller