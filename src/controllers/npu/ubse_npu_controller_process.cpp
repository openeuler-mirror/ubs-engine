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

UbseResult UbseNpuControllerProcess::ProcessNpuDevice(const UbDevice &ubDevice,
                                                      std::vector<std::shared_ptr<IResource>> &devList)
{
    UBSE_LOG_DEBUG << "ProcessNpuDevice start, npu " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.index;
    auto &collection = ResourceCollection::GetInstance();
    CollectionDevId locStr = CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(CollectionDeviceType::NPU),
                                                                     ubDevice.slotId, ubDevice.chipId);
    std::shared_ptr<CollectionDeviceDavid> npu = CollectionDevice::CollectionToDerived<CollectionDeviceDavid>(
        collection.GetDeviceByDevId(locStr, CollectionDeviceType::NPU));
    if (npu == nullptr) {
        UBSE_LOG_ERROR << "ProcessNpuDevice failed, npu " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                       << ubDevice.index << " is not found";
        return UBSE_ERROR;
    }
    std::shared_ptr<NpuResource> npuRes = std::make_shared<NpuResource>();
    DeviceNpuToResource(npu, npuRes);
    devList.push_back(npuRes);
    UBSE_LOG_DEBUG << "ProcessNpuDevice end, npu " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.index;
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::ProcessNicDevice(const UbDevice &ubDevice,
                                                      std::vector<std::shared_ptr<IResource>> &devList)
{
    UBSE_LOG_DEBUG << "ProcessNicDevice start, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.index;
    auto &collection = ResourceCollection::GetInstance();
    CollectionDevId locStr = CollectionStringUtil::CollectionJoinStr(static_cast<uint8_t>(CollectionDeviceType::NIC),
                                                                     ubDevice.slotId, ubDevice.chipId, ubDevice.index);
    std::shared_ptr<CollectionDeviceNic> nic = CollectionDevice::CollectionToDerived<CollectionDeviceNic>(
        collection.GetDeviceByDevId(locStr, CollectionDeviceType::NIC));
    if (nic == nullptr) {
        UBSE_LOG_ERROR << "ProcessNicDevice failed, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                       << ubDevice.index << " is not found";
        return UBSE_ERROR;
    }
    std::shared_ptr<NicResource> nicRes = std::make_shared<NicResource>();
    DeviceNicToResource(nic, nicRes);
    devList.push_back(nicRes);
    UBSE_LOG_DEBUG << "ProcessNicDevice end, nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                   << ubDevice.index;
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::ProcessBusiResource(const UbseAllocRequest &requestInfo,
                                                         std::vector<std::shared_ptr<IResource>> &devList,
                                                         const std::string &newBusInstanceGuid)
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
    const std::shared_ptr<CollectionDeviceDavid> &npu, std::shared_ptr<NpuResource> &npuRes)
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
        const std::shared_ptr<CollectionDeviceBusi> &busi = businstances[0];
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

UbseResult UbseNpuControllerProcess::DeviceNpuToResource(const std::shared_ptr<CollectionDeviceDavid> &npu,
                                                         std::shared_ptr<NpuResource> &npuRes)
{
    CollectDeviceLoc npuLoc = npu->GetDeviceLoc();
    npuRes->SetLoc(npuLoc.slotId, npuLoc.chipId, npuLoc.dieId);
    UBSE_LOG_DEBUG << "DeviceNpuToResource npu " << npuLoc.slotId << "-" << npuLoc.chipId << "-" << npuLoc.dieId
                   << " added to npuRes";
    npuRes->SetGuid(npu->GetGuid());
    UBSE_LOG_DEBUG << "DeviceNpuToResource npu " << npu->GetGuid() << " added to npuRes";
    std::shared_ptr<CollectionDeviceIdevPfe> pfe = ProcessBondingDevice(npu, npuRes);
    if (!pfe) {
        UBSE_LOG_ERROR << "DeviceNpuToResource failed, npu " << npu->GetGuid() << " can not find pfe";
        return UBSE_ERROR;
    }

    std::shared_ptr<CollectionDeviceUbCtrl> ubCtl = pfe->GetParentUbCtl();
    if (!ubCtl) {
        UBSE_LOG_ERROR << "DeviceNpuToResource failed, npu " << npu->GetGuid() << " can find pfe " << pfe->GetGuid()
                       << ", but no UBctrl is bind";
        return UBSE_ERROR;
    }

    std::vector<std::shared_ptr<CollectionDeviceNic>> affiNics = ubCtl->GetAffiDevNic();

    for (const auto &affiNic : affiNics) {
        CollectDeviceLoc affinic = affiNic->GetDeviceLoc();
        npuRes->AddAffinityDevice({ResourceType::NIC, affinic.slotId, affinic.chipId, affinic.pfeId});
        UBSE_LOG_DEBUG << "DeviceNpuToResource nic " << affinic.slotId << "-" << affinic.chipId << "-"
                       << affinic.pfeId << " added to npuRes.affinityDevices";
    }

    CollectDeviceLoc ubctl = ubCtl->GetDeviceLoc();
    npuRes->AddAffinityDevice({ResourceType::UBCONTROLLER, ubctl.slotId, ubctl.chipId, ubctl.dieId});
    UBSE_LOG_DEBUG << "DeviceNpuToResource UBctrl " << ubctl.slotId << "-" << ubctl.chipId << "-" << ubctl.dieId
                   << " added to npuRes.affinityDevices";
    return UBSE_OK;
}

void UbseNpuControllerProcess::SetNicLocation(const std::shared_ptr<CollectionDeviceNic> &nic,
                                              std::shared_ptr<NicResource> &nicRes)
{
    CollectDeviceLoc nicLoc = nic->GetDeviceLoc();
    UBSE_LOG_DEBUG << "DeviceNicToResource start, nic " << nic->GetGuid() << " " << nicLoc.slotId << "-"
                   << nicLoc.chipId << "-" << nicLoc.pfeId << " added to nicRes";
    nicRes->SetLoc(nicLoc.slotId, nicLoc.chipId, nicLoc.pfeId);
}

void UbseNpuControllerProcess::SetNicBusInstanceGuid(const std::shared_ptr<CollectionDeviceNic> &nic,
                                                     std::shared_ptr<NicResource> &nicRes)
{
    std::shared_ptr<CollectionDeviceBusi> busi = nic->GetBondingDevBusi();
    if (!busi) {
        UBSE_LOG_WARN << "DeviceNicToResource failed, nic " << nic->GetGuid() << " bonding busi is null";
        nicRes->SetBusInstanceGuid("");
    } else {
        nicRes->SetBusInstanceGuid(busi->GetGuid());
        UBSE_LOG_DEBUG << "DeviceNicToResource bus instance " << busi->GetGuid() << "added to nicRes";
    }
}

UbseResult UbseNpuControllerProcess::DeviceNicToResource(const std::shared_ptr<CollectionDeviceNic> &nic,
                                                         std::shared_ptr<NicResource> &nicRes)
{
    // 1. 设置NIC位置信息
    SetNicLocation(nic, nicRes);
    // 2. 设置NIC的GUID
    nicRes->SetGuid(nic->GetGuid());
    // 3. 处理绑定的Busi设备
    SetNicBusInstanceGuid(nic, nicRes);
    std::shared_ptr<CollectionDeviceUbCtrl> ubCtl = nic->GetAffinityDevUbCtrl();
    if (!ubCtl) {
        UBSE_LOG_ERROR << "DeviceNicToResource failed, nic " << nic->GetGuid() << " affinity ub is null";
        return UBSE_ERROR;
    }
    std::vector<std::shared_ptr<CollectionDeviceIdevPfe>> pfes = ubCtl->GetSubDevIdev();
    if (pfes.empty()) {
        UBSE_LOG_ERROR << "DeviceNicToResource failed, nic " << nic->GetGuid() << " can find UBctrl "
                       << ubCtl->GetGuid() << ", but sub pfe is empty";
        return UBSE_ERROR;
    }
    for (const auto &pfe : pfes) {
        if (pfe->GetIsComSharedFe()) {
            continue;
        }
        std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfes = pfe->GetSubDevVfe();
        if (vfes.empty()) {
            UBSE_LOG_ERROR << "DeviceNicToResource failed, nic " << nic->GetGuid() << " can find pfe " << pfe->GetGuid()
                           << ", but sub vfe is empty";
            return UBSE_ERROR;
        }
        std::shared_ptr<CollectionDeviceDavid> npu = nullptr;
        if (vfes[0] != nullptr) {
            npu = vfes[0]->GetBondingDevDavid();
        }
        if (npu == nullptr) {
            npu = pfe->GetBondingDevDavid();
            if (npu == nullptr) {
                UBSE_LOG_ERROR << "DeviceNicToResource failed, nic " << nic->GetGuid()
                               << " can find pfe and vfe, but no david is bound";
                return UBSE_ERROR;
            }
        }
        CollectDeviceLoc npuLoc = npu->GetDeviceLoc();
        nicRes->AddAffinityDevice({ResourceType::NPU, npuLoc.slotId, npuLoc.chipId, npuLoc.dieId});
        UBSE_LOG_DEBUG << "DeviceNicToResource npu " << npuLoc.slotId << "-" << npuLoc.chipId << "-" << npuLoc.dieId
                       << " added to nicRes.affinityDevices";
    }
    CollectDeviceLoc ubctl = ubCtl->GetDeviceLoc();
    nicRes->AddAffinityDevice({ResourceType::UBCONTROLLER, ubctl.slotId, ubctl.chipId, ubctl.dieId});
    UBSE_LOG_DEBUG << "DeviceNicToResource UBctrl: " << ubctl.slotId << "-" << ubctl.chipId << "-" << ubctl.dieId
                   << " added to nicRes.affinityDevices";
    return UBSE_OK;
}

UbseResult UbseNpuControllerProcess::BusInstanceToResource(const std::shared_ptr<CollectionDeviceBusi> &busi,
                                                           std::shared_ptr<BusiResource> &busRes)
{
    UBSE_LOG_DEBUG << "BusInstanceToResource start, bus instance " << busi->GetGuid() << "added to busRes";
    busRes->SetGuid(busi->GetGuid());
    std::vector<std::shared_ptr<CollectionDeviceNic>> nics = busi->GetSubDevNic();
    UBSE_LOG_DEBUG << "BusInstanceToResource nics size: " << nics.size();
    for (auto &nic : nics) {
        CollectDeviceLoc nicLoc = nic->GetDeviceLoc();
        busRes->AddSubDevice({ResourceType::NIC, nicLoc.slotId, nicLoc.chipId, nicLoc.pfeId});
        UBSE_LOG_DEBUG << "nic: " << nicLoc.slotId << "-" << nicLoc.chipId << "-" << nicLoc.pfeId
                       << " added to busRes.subDevices";
    }
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfes = busi->GetSubDevIdev();
    UBSE_LOG_DEBUG << "BusInstanceToResource npu size: " << vfes.size();
    for (auto &vfe : vfes) {
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