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

#include "ubse_npu_manage_api.h"

#include <unistd.h>
#include <condition_variable>
#include <functional>
#include <queue>
#include <stack>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"

#include "ubse_npu_resource_collection.h"
#include "ubse_npu_resource_collection_def.h"

#include "ubse_npu_controller_process.h"

#include "vm_state_monitor/ubse_npu_monitor_service_api.h"

namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::common::def;

fe_loc ToFeLoc(const CollectDeviceLoc &loc);
ub_controller ToUbController(const CollectDeviceLoc &loc);
david_loc ToDavidLoc(const CollectDeviceLoc &loc);

const uint8_t CORRECT_RSP = 0;
const uint8_t ERROR_RSP = 1;
const uint8_t SLEEP_TIME = 2;

const uint8_t COMMON_RETRY_TIME = 5;

struct OperationHistory {
    std::function<UbseResult()> operation;
};

UbseResult ListAllocDevices(const UbseAllocRequest &requestInfo, std::vector<std::shared_ptr<IResource>> &devList,
                            std::string &newBusInstanceGuid);

UbseResult AllocDevicesAction(const UbseAllocRequest &requestInfo, std::string &newBusInstanceGuid,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult FreeUbDeviceAction(const UbseAllocRequest &requestInfo,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult UbseGetAllocDeviceList(const UbseAllocRequest &requestInfo,
                                  std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                  std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult AllocDevicesPreparation(const std::string &busInstanceGuid,
                                   std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                   std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult HandleOccupiedFilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                                            std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                            std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult FilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult CheckAndHandleOccupiedDevices(std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                         std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList);

UbseResult CheckCollection(const std::string &action);

void ClearEmptyVMBusInstance();

class UbseNpuManagerApi {
public:
    enum class NpuManagerState {
        AVAILABLE,     // 可用
        ROLLBACK,      // 回滚
        ROLLBACK_BG,   // 后台回滚
        RUNNING_ALLOC, // 使能中
        RUNNING_FREE,  // 去使能中
        FREE_BG,       // 后台去使能
        INIT           // 初始化
    };
    std::condition_variable cv_;
    std::mutex mtx_;

public:
    static UbseNpuManagerApi &GetInstance();

    UbseResult UnRegisterDevFromBusi(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                     const CollectionDevId &busiGuid);

    UbseResult UnRegisterIDevFromBusi(std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList,
                                      const CollectionDevId &busiGuid);

    UbseResult RegisterDevToBusi(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                 const CollectionDevId &busiGuid);

    UbseResult RegisterIDevToBusi(std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList,
                                  const CollectionDevId &busiGuid);

    UbseResult UnbindVfeDavid(uint16_t upi, std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList);

    UbseResult BindVfeDavid(uint16_t upi, std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList);

    UbseResult ResetNpu(std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList);

    UbseResult ResetNpu(const std::vector<UbDevice> &ubDevList);

    UbseResult CreateVMBusi(uint16_t upi, CollectionGuid &busiGuid);

    UbseResult DestroyVMBusi(const CollectionGuid &busiGuid);

    void SetState(NpuManagerState state);

    NpuManagerState GetState();

    bool GetCollectionReady();

    void SetCollectionReady(bool ready);

    void FreeQueue(const UbseAllocRequest &requestInfo, const CollectionDevId &hostBusInstanceGuid,
                   std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                   std::vector<std::shared_ptr<CollectionDeviceNic>> &nics);

    UbseResult ExecuteFreeQueue();

private:
    UbseNpuManagerApi();
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> FilterDevices(
        const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
        const std::shared_ptr<CollectionDeviceBusi> &busInstance);
    std::vector<reg_info> PrepareRegisterInfos(const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList);
    UbseResult RegisterVfeToBusi(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
                                 const CollectionDevId &busiGuid);
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> FilterUnRegisterDevices(
        const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
        const std::shared_ptr<CollectionDeviceBusi> &busInstance);
    std::vector<un_reg_info> PrepareUnRegisterInfos(
        const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList);
    UbseResult UnRegisterVfeFromBusi(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
                                     const CollectionDevId &busiGuid);
    UbseResult RollBack();
    void ExecuteFreeQueueBackGround();
    UbseResult SendUnRegisterRequest(EID eid, const std::vector<un_reg_info> &infos,
                                     std::vector<un_reg_info> &sendInfos);
    UbseResult SendRegisterRequest(const std::vector<reg_info> &infos, const EID &eid,
                                   std::vector<reg_info> &sendInfos);
    UbseResult SendUnbindRequest(uint16_t upi, std::vector<un_bind_info> &unbindInfoList);
    UbseResult SendBindRequest(uint16_t upi, const std::vector<bind_info> &bindInfoList);
    UbseResult CheckSendUnRegister(CtrlQUnRegisterDevProxy &proxy, CtrlQUnRegisterDevReqBuilder &builder,
                                   std::vector<un_reg_info> &sendInfos);
    UbseResult CheckSendRegister(CtrlQRegisterDevProxy &proxy, CtrlQRegisterDevReqBuilder &builder,
                                 std::vector<reg_info> &sendInfos);
    void HandleUnRegisterFailure(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                 const std::shared_ptr<CollectionDeviceBusi> &busInstance,
                                 const std::vector<un_reg_info> &infos, const std::vector<un_reg_info> &infosFailed);

private:
    std::stack<std::shared_ptr<OperationHistory>> operationHistory_;
    std::queue<std::shared_ptr<OperationHistory>> futureProcedure_;
    uint8_t retryTime_ = COMMON_RETRY_TIME;
    NpuManagerState state_;
    bool collectionReady_ = false;
};

void StartCollect()
{
    try {
        std::thread CollectStaticResourceThread([]() {
            UBSE_LOG_INFO << "Start to collect static resource";
            UbseResult res = ResourceCollection::GetInstance().CollectStaticResource();
            if (res != UBSE_OK) {
                UBSE_LOG_WARN << "NpuControllerModule start. Failed to collect static resource. It will retry later";
            } else {
                UBSE_LOG_INFO << "Success to collect static resource";
                auto &manager = UbseNpuManagerApi::GetInstance();
                manager.SetCollectionReady(true);
                manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
            }
        });
        CollectStaticResourceThread.detach();
    } catch (const std::system_error &e) {
        UBSE_LOG_ERROR << "Failed to create collect static resource thread: " << e.what();
    } catch (...) {
        UBSE_LOG_ERROR << "Unknown error occurred while creating collect static resource thread";
    }
}

UbseResult AllocDevicesImpl(const UbseAllocRequest &requestInfo, std::string &newBusInstanceGuid,
                            std::vector<std::shared_ptr<IResource>> &devList)
{
    auto &manager = UbseNpuManagerApi::GetInstance();
    if (CheckCollection("alloc npu,nic devices") != UBSE_OK) {
        return UBSE_ERROR;
    }
    {
        std::unique_lock<std::mutex> lock(manager.mtx_);
        manager.cv_.wait(lock,
                         [&manager] { return manager.GetState() == UbseNpuManagerApi::NpuManagerState::AVAILABLE; });
    }

    auto &collection = ResourceCollection::GetInstance();

    // 如果传入newBusInstanceGuid， 校验(不存在则返回入参错误）
    if (!requestInfo.busInstanceGuid.empty() and collection.GetDeviceByGuid(requestInfo.busInstanceGuid) == nullptr) {
        UBSE_LOG_ERROR << "Bus Instance Guid is invalid";
        return UBSE_ERROR_INVAL;
    }

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npuList;
    std::vector<std::shared_ptr<CollectionDeviceNic>> nicList;
    auto ret = UbseGetAllocDeviceList(requestInfo, npuList, nicList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get alloc npu,nic device list from collection";
        return ret; // 如果绑定失败，返回错误码
    }

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    ret = AllocDevicesAction(requestInfo, newBusInstanceGuid, npuList, nicList);
    if (ret != UBSE_OK) {
        manager.SetState(UbseNpuManagerApi::NpuManagerState::ROLLBACK);
        return ret;
    }

    ret = ListAllocDevices(requestInfo, devList, newBusInstanceGuid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Success to alloc npu,nic devices but failed to transfer devlist info to resource";
        manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
        return ret;
    }

    ClearEmptyVMBusInstance();
    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    return UBSE_OK;
}

UbseResult AllocDevicesAction(const UbseAllocRequest &requestInfo, std::string &newBusInstanceGuid,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList)
{
    auto &collection = ResourceCollection::GetInstance();
    auto &manager = UbseNpuManagerApi::GetInstance();
    uint16_t upi = requestInfo.upiStr;

    UbseResult ret = AllocDevicesPreparation(requestInfo.busInstanceGuid, npuList, nicList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed in alloc npu,nic devices preparation";
        return ret; // 如果绑定失败，返回错误码
    }

    auto hostbusi = collection.GetDeviceHostBusInstance();
    if (hostbusi == nullptr) {
        UBSE_LOG_ERROR << "Failed to get host bus instance";
        return UBSE_ERROR;
    }

    // 调用LCNE接口，创建虚机bus instance （如果传入businstance 跳过）
    if (!requestInfo.busInstanceGuid.empty()) {
        newBusInstanceGuid = requestInfo.busInstanceGuid;
    } else {
        // 如果为空，则创建一个新的VM businstance，返回GUID
        ret = manager.CreateVMBusi(upi, newBusInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to create vm bus instance";
            return ret; // 如果创建失败，返回错误码
        }
    }
    // 调用LCNE接口，将vfe注册到虚机bus instance
    ret = manager.RegisterIDevToBusi(npuList, newBusInstanceGuid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register npus to vm bus instance " << newBusInstanceGuid;
        return ret; // 如果注册失败，返回错误码
    }

    // 调用LCNE接口，绑定npu与vfe（LCNE会在该接口中完成npu与pfe解绑的动作）。
    ret = manager.BindVfeDavid(upi, npuList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to bind npus to vfes";
        return ret; // 如果绑定失败，返回错误码
    }

    if (!nicList.empty()) {
        CollectionDevId hostBusInstanceGuid = hostbusi->GetGuid();

        // 调用LCNE接口，解除1825与host bus instance的注册关系
        ret = manager.UnRegisterDevFromBusi(nicList, hostBusInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister nics from host bus instance " << hostBusInstanceGuid;
            return ret; // 如果解注册失败，返回错误码
        }

        // 调用LCNE接口，将1825注册到虚机bus instance上
        ret = manager.RegisterDevToBusi(nicList, newBusInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register nics to vm bus instance " << newBusInstanceGuid;
            return ret; // 如果注册失败，返回错误码
        }
    }

    UBSE_LOG_INFO << "Success to alloc npu,nic devices. Bus instance guid: " << newBusInstanceGuid;
    return UBSE_OK;
}

UbseResult UbseGetAllocDeviceList(const UbseAllocRequest &requestInfo,
                                  std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                  std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList)
{
    auto &collection = ResourceCollection::GetInstance();
    auto devices = requestInfo.ubDevList;

    for (auto &ubDevice : devices) {
        // 校验npu，nic id in 静态采集数据 (不存在则返回入参错误）.分别抽出NPU, NIC加入vector
        if (ubDevice.type == ResourceType::NPU) {
            CollectionDevId locStr = CollectionStringUtil::CollectionJoinStr(
                static_cast<uint8_t>(CollectionDeviceType::NPU), ubDevice.slotId, ubDevice.chipId);
            std::shared_ptr<CollectionDeviceDavid> npu = CollectionDevice::CollectionToDerived<CollectionDeviceDavid>(
                collection.GetDeviceByDevId(locStr, CollectionDeviceType::NPU));
            if (npu == nullptr) {
                UBSE_LOG_ERROR << "Failed to get npu " << locStr << " from collection";
                return UBSE_ERROR_INVAL;
            }
            npuList.push_back(npu);
        } else if (ubDevice.type == ResourceType::NIC) {
            CollectionDevId locStr = CollectionStringUtil::CollectionJoinStr(
                static_cast<uint8_t>(CollectionDeviceType::NIC), ubDevice.slotId, ubDevice.chipId, ubDevice.index);
            std::shared_ptr<CollectionDeviceNic> nic = CollectionDevice::CollectionToDerived<CollectionDeviceNic>(
                collection.GetDeviceByDevId(locStr, CollectionDeviceType::NIC));
            if (nic == nullptr) {
                UBSE_LOG_ERROR << "Failed to get nic " << locStr << " from collection";
                return UBSE_ERROR_INVAL;
            }
            nicList.push_back(nic);
        } else {
            UBSE_LOG_ERROR << "Invalid resource type in alloc request";
            return UBSE_ERROR_INVAL; // 使能设备类型不符合要求，错误码
        }
    }
    UBSE_LOG_INFO << "Success to get npu,nic devices from collection";
    return UBSE_OK;
}

UbseResult ListAllocDevices(const UbseAllocRequest &requestInfo, std::vector<std::shared_ptr<IResource>> &devList,
                            std::string &newBusInstanceGuid)
{
    UBSE_LOG_INFO << "Success to alloc npu,nic devices. Start to generate info...";
    for (const auto &ubDevice : requestInfo.ubDevList) {
        if (ubDevice.type == ResourceType::NPU) {
            UbseResult result = UbseNpuControllerProcess::ProcessNpuDevice(ubDevice, devList);
            if (result != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to generate npu " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                               << ubDevice.index << " info";
                return result;
            }
        } else if (ubDevice.type == ResourceType::NIC) {
            UbseResult result = UbseNpuControllerProcess::ProcessNicDevice(ubDevice, devList);
            if (result != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to generate nic " << ubDevice.slotId << "-" << ubDevice.chipId << "-"
                               << ubDevice.index << " info";
                return result;
            }
        }
    }
    UBSE_LOG_DEBUG << "Success to generate ubse device info";
    UbseResult res = UbseNpuControllerProcess::ProcessBusiResource(requestInfo, devList, newBusInstanceGuid);
    return res;
}

UbseResult HandleOccupiedFilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                                            std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                            std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList)
{
    UbseResult ret = UBSE_OK;
    if (!npuList.empty()) {
        ret = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(npuList, busInstance->GetGuid());
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister npuList from bus instance " << busInstance->GetIdStr();
            return ret; // 如果解注册失败，返回错误码
        }
        ret = UbseNpuManagerApi::GetInstance().UnbindVfeDavid(0, npuList);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unbind npuList from vfe";
            return ret; // 如果绑定失败，返回错误码
        }
    }

    if (!nicList.empty()) {
        ret = UbseNpuManagerApi::GetInstance().UnRegisterDevFromBusi(nicList, busInstance->GetGuid());
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister nicList from bus instance " << busInstance->GetIdStr();
            return ret; // 如果解注册失败，返回错误码
        }

        ret = RegisterNicsToHost(nicList);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register nicList to host bus instance";
            return ret; // 如果注册失败，返回错误码
        }
    }

    UBSE_LOG_INFO << "Success to handle occupied bus instance " << busInstance->GetIdStr();
    return UBSE_OK;
}

void ClearEmptyVMBusInstance()
{
    auto &collection = ResourceCollection::GetInstance();
    CollectionDevIdToDevice vmBusInstanceMap;
    auto res = collection.GetDevicesByType(CollectionDeviceType::VM_BUSINSTANCE, vmBusInstanceMap);
    UBSE_LOG_DEBUG << "There are " << vmBusInstanceMap.size() << " bus instances";
    for (auto &vmBusInstance : vmBusInstanceMap) {
        auto busi = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(vmBusInstance.second);
        if (busi == nullptr) {
            continue;
        }
        auto subDevList = busi->GetSubDevNic();
        UBSE_LOG_DEBUG << "Bus instance " << busi->GetGuid() << " can find " << subDevList.size() << " nic";
        if (!subDevList.empty()) {
            continue;
        }
        UBSE_LOG_DEBUG << "Bus instance " << busi->GetGuid() << " has no nic";
        std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfes = busi->GetSubDevIdev();
        bool deleteFlag = true;
        for (auto &vfe : vfes) {
            if (!vfe->GetIsComSharedFe()) {
                UBSE_LOG_DEBUG << "Bus instance " << busi->GetGuid() << " has vfe " << vfe->GetGuid()
                               << ". So skip destroying.";
                deleteFlag = false;
                break;
            }
        }
        if (!deleteFlag) {
            continue;
        }
        UBSE_LOG_DEBUG << "Bus instance " << busi->GetGuid() << " has no npu. Start to destroy empty vm bus instance.";
        res = UbseNpuManagerApi::GetInstance().DestroyVMBusi(busi->GetGuid());
        UBSE_LOG_DEBUG << "Destroy empty vm bus instance End. Result:" << FormatRetCode(res);
    }
    UBSE_LOG_INFO << "Success to clear empty vm bus instances";
}

UbseResult AllocDevicesPreparation(const std::string &BusInstanceGuid,
                                   std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                                   std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList)
{
    UbseResult ret;
    auto &collection = ResourceCollection::GetInstance();

    // 处理businstance占用
    if (!BusInstanceGuid.empty()) {
        UBSE_LOG_INFO << "Bus instance " << BusInstanceGuid << " already exists.";
        std::shared_ptr<CollectionDeviceBusi> busInstance =
            CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(BusInstanceGuid));
        if (busInstance == nullptr) {
            return UBSE_ERROR_INVAL;
        }

        ret = FilterDeviceVMBusi(busInstance, npuList, nicList);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to filter npuList or nicList";
            return ret; // 如果筛选失败，返回错误码
        }
    }

    // 处理抢占，vm bus instance注销待抢占列表vfe，david归位.
    ret = CheckAndHandleOccupiedDevices(npuList, nicList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to handle occupied npuList or nicList";
        return ret; // 如果抢占失败，返回错误码
    }

    return UBSE_OK;
}

UbseResult FilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npuList,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList)
{
    auto busNics = busInstance->GetSubDevNic();
    // 1. 找出 busNics 中不在当前 nicList 中的设备（nicsToRemoveFromBusi）
    std::vector<std::shared_ptr<CollectionDeviceNic>> nicsToRemoveFromBusi;
    for (const auto &nic : busNics) {
        if (std::find(nicList.begin(), nicList.end(), nic) == nicList.end()) {
            nicsToRemoveFromBusi.push_back(nic);
        }
    }
    // 2. 找出当前 nics 中不在 busNics 中的设备（nicsToRegisterToBusi）
    std::vector<std::shared_ptr<CollectionDeviceNic>> nicsToRegisterToBusi;
    for (const auto &nic : nicList) {
        if (std::find(busNics.begin(), busNics.end(), nic) == busNics.end()) {
            nicsToRegisterToBusi.push_back(nic);
        }
    }
    nicList = std::move(nicsToRegisterToBusi);

    auto vfes = busInstance->GetSubDevIdev();
    // 1. 找出 bus instance 上绑定的 npu 不在当前 npuList 中的设备（npusToRemoveFromBusi）
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npusToRemoveFromBusi;
    for (const auto &vfe : vfes) {
        auto npu = vfe->GetBondingDevDavid();
        if (npu != nullptr) {
            if (std::find(npuList.begin(), npuList.end(), npu) == npuList.end()) {
                npusToRemoveFromBusi.push_back(npu);
            }
        }
    }
    // 2. 找出当前 npus 中不在 bus instance 上绑定的 npu 的设备（npusToRegisterToBusi）
    std::vector<std::shared_ptr<CollectionDeviceDavid>> npusToRegisterToBusi;
    for (const auto &npu : npuList) {
        auto idev = npu->GetBondingIdev();
        if (idev != nullptr && idev->GetType() == CollectionDeviceType::V_IDEV) {
            auto vfe = CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(idev);
            if (std::find(vfes.begin(), vfes.end(), vfe) != vfes.end()) {
                continue;
            }
        }
        npusToRegisterToBusi.push_back(npu);
    }
    npuList = std::move(npusToRegisterToBusi);

    UBSE_LOG_WARN << "Bus instance " << busInstance->GetIdStr() << " is already registered with "
                  << npusToRemoveFromBusi.size() << " npus and " << nicsToRemoveFromBusi.size()
                  << " nics. Start to unregister them";

    UbseResult ret = HandleOccupiedFilterDeviceVMBusi(busInstance, npusToRemoveFromBusi, nicsToRemoveFromBusi);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to handle occupied bus instance " << busInstance->GetIdStr();
        return ret; // 如果busi处理失败，返回错误码
    }
    return UBSE_OK;
}

UbseResult CheckCollection(const std::string &action)
{
    if (!UbseNpuManagerApi::GetInstance().GetCollectionReady()) {
        UBSE_LOG_INFO << "Find collection resource not ready. Retry to collection static resource.";
        UbseResult res = ResourceCollection::GetInstance().CollectStaticResource();
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to collect static resource. Can not " << action;
            return UBSE_ERROR;
        } else {
            UbseNpuManagerApi::GetInstance().SetCollectionReady(true);
            UBSE_LOG_INFO << "Retry to collect static resource successfully. Go on to " << action;
            UbseNpuManagerApi::GetInstance().SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
        }
    }
    return UBSE_OK;
}

UbseNpuManagerApi &UbseNpuManagerApi::GetInstance()
{
    static UbseNpuManagerApi npuManager;
    return npuManager;
}

UbseNpuManagerApi::UbseNpuManagerApi() : state_(NpuManagerState::INIT) {}

UbseResult UbseNpuManagerApi::UnRegisterDevFromBusi(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                                    const CollectionDevId &busiGuid)
{
    UBSE_LOG_DEBUG << "UnRegister nic from bus instance start, nic size: " << devList.size()
                   << ", bus instance: " << busiGuid;
    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDeviceBusi> busInstance =
        CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busiGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "bus instance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    std::vector<un_reg_info> infos;
    std::vector<un_reg_info> infosFailed;
    for (const auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingDevBusi() != nullptr) {
            infos.push_back(un_reg_info{ToFeLoc(dev->GetDeviceLoc())});
        }
    }
    UbseResult ret = SendUnRegisterRequest(busInstance->GetEid(), infos, infosFailed);
    if (ret != UBSE_OK) {
        HandleUnRegisterFailure(devList, busInstance, infos, infosFailed);
        UBSE_LOG_ERROR << "Request UnRegister nic from bus instance failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (const auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingDevBusi() != nullptr) {
            ResourceCollection::UnbindDevice(dev, busInstance);
        }
    }

    if (state_ == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, devList, busiGuid]() mutable -> UbseResult {
            return this->RegisterDevToBusi(devList, busiGuid);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory_.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory_ success";
    }
    UBSE_LOG_INFO << "UnRegister nic from bus instance success";
    return UBSE_OK;
}
void UbseNpuManagerApi::HandleUnRegisterFailure(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                                const std::shared_ptr<CollectionDeviceBusi> &busInstance,
                                                const std::vector<un_reg_info> &infos,
                                                const std::vector<un_reg_info> &infosFailed)
{
    if (this->state_ == NpuManagerState::RUNNING_ALLOC and !infosFailed.empty() and
        infosFailed.size() != infos.size()) {
        // 意图注销的全部dev都重新注册，不关注是否成功
        std::vector<reg_info> rollRegInfos;
        std::vector<reg_info> rollRegInfosFailed;
        for (const auto &dev : devList) {
            if (dev != nullptr && dev->GetBondingDevBusi() != nullptr) {
                rollRegInfos.push_back(reg_info{ToFeLoc(dev->GetDeviceLoc()),
                                                ToUbController(dev->GetAffinityDevUbCtrl()->GetDeviceLoc())});
            }
        }
        this->SendRegisterRequest(rollRegInfos, busInstance->GetEid(), rollRegInfosFailed);
    }
}

UbseResult UbseNpuManagerApi::UnRegisterIDevFromBusi(std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList,
                                                     const CollectionDevId &busiGuid)
{
    UBSE_LOG_DEBUG << "UnRegister david from bus instance start, david size: " << devList.size()
                   << ", bus instance: " << busiGuid;
    auto &collection = ResourceCollection::GetInstance();

    std::shared_ptr<CollectionDeviceBusi> busInstance =
        CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busiGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "bus instance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList;
    for (auto &dev : devList) {
        if (dev == nullptr) {
            UBSE_LOG_ERROR << "david is null";
            return UBSE_ERROR;
        }
        auto idev = dev->GetBondingIdev();
        if (idev == nullptr) {
            UBSE_LOG_ERROR << "david " << dev->GetIdStr() << " bonding idev is null";
            return UBSE_ERROR;
        }
        if (idev->GetType() == CollectionDeviceType::V_IDEV) {
            std::shared_ptr<CollectionDeviceIdevVfe> vfe =
                CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(dev->GetBondingIdev());
            vfeList.push_back(vfe);
        }
    }
    if (vfeList.size() != devList.size()) {
        UBSE_LOG_WARN << "There are some david not bonding vfe, david size: " << devList.size()
                      << ", vfe size: " << vfeList.size();
    }
    auto ret = UnRegisterVfeFromBusi(vfeList, busiGuid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UnRegister david from bus instance failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "UnRegister david from bus instance success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::RegisterDevToBusi(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                                const CollectionDevId &busiGuid)
{
    UBSE_LOG_DEBUG << "Register nic to bus instance start, nic size: " << devList.size()
                   << ", bus instance: " << busiGuid;
    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDeviceBusi> busInstance =
        CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busiGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "bus instance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    std::vector<reg_info> infos;
    std::vector<reg_info> infosFailed;
    for (auto &dev : devList) {
        if (dev == nullptr || dev->GetAffinityDevUbCtrl() == nullptr) {
            continue;
        }
        reg_info regInfo{ToFeLoc(dev->GetDeviceLoc()), ToUbController(dev->GetAffinityDevUbCtrl()->GetDeviceLoc())};
        infos.push_back(regInfo);
    }
    UbseResult ret = SendRegisterRequest(infos, busInstance->GetEid(), infosFailed);
    if (ret != UBSE_OK) {
        if (state_ == NpuManagerState::RUNNING_ALLOC and !infosFailed.empty() and infosFailed.size() != infos.size()) {
            // 回滚当前
            std::vector<un_reg_info> rollRegInfos;
            std::vector<un_reg_info> rollRegInfosFailed;
            for (auto &dev : devList) {
                rollRegInfos.push_back(un_reg_info{ToFeLoc(dev->GetDeviceLoc())});
            }
            SendUnRegisterRequest(busInstance->GetEid(), rollRegInfos, rollRegInfosFailed);
        }
        UBSE_LOG_ERROR << "Request Register nic to bus instance failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (auto &dev : devList) {
        ResourceCollection::BindDevice(dev, busInstance);
    }

    if (state_ == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, devList, busiGuid]() mutable -> UbseResult {
            return this->UnRegisterDevFromBusi(devList, busiGuid);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory_.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory_ success";
    }
    UBSE_LOG_INFO << "Register nic to bus instance success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::RegisterIDevToBusi(std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList,
                                                 const CollectionDevId &busiGuid)
{
    UBSE_LOG_DEBUG << "Register david to bus instance start, david size: " << devList.size()
                   << ", bus instance: " << busiGuid;
    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDeviceBusi> busInstance =
        CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busiGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "bus instance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList;
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::P_IDEV) {
            std::shared_ptr<CollectionDeviceIdevPfe> pfe =
                CollectionDevice::CollectionToDerived<CollectionDeviceIdevPfe>(dev->GetBondingIdev());
            std::shared_ptr<CollectionDeviceIdevVfe> vfe = pfe->GetSubDevVfe()[0];
            vfeList.push_back(vfe);
        }
    }

    if (vfeList.size() != devList.size()) {
        UBSE_LOG_WARN << "There are some david not bonding vfe, david size: " << devList.size()
                      << ", vfe size: " << vfeList.size();
    }
    auto ret = RegisterVfeToBusi(vfeList, busiGuid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register david to bus instance failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "Register david to bus instance success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::UnbindVfeDavid(uint16_t upi, std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList)
{
    UBSE_LOG_DEBUG << "Unbind VFE DAVID start, upi: " << upi << ", dev size: " << devList.size();
    std::vector<un_bind_info> unbindInfoList;
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::V_IDEV) {
            un_bind_info info{ToFeLoc(dev->GetBondingIdev()->GetDeviceLoc()), ToDavidLoc(dev->GetDeviceLoc())};
            unbindInfoList.push_back(info);
        }
    }

    UbseResult res = SendUnbindRequest(upi, unbindInfoList);
    if (res != UBSE_OK) {
        if (state_ == NpuManagerState::RUNNING_ALLOC) {
            std::vector<bind_info> bindInfoList;
            for (auto &info : unbindInfoList) {
                bindInfoList.push_back(bind_info{info.fe, info.david});
            }
            SendBindRequest(upi, bindInfoList);
        }
        UBSE_LOG_ERROR << "Request Unbind VFE DAVID failed, res: " << FormatRetCode(res);
        return res;
    }
    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::V_IDEV) {
            std::shared_ptr<CollectionDeviceIdevVfe> vfe =
                CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(dev->GetBondingIdev());
            std::shared_ptr<CollectionDeviceIdevPfe> pfe = vfe->GetParentPfe();
            ResourceCollection::BindDevice(pfe, dev);
        }
    }
    if (state_ == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, upi, devList]() mutable -> UbseResult {
            return this->BindVfeDavid(upi, devList);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory_.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory_ success";
    }
    UBSE_LOG_INFO << "Unbind VFE DAVID success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::BindVfeDavid(uint16_t upi, std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList)
{
    UBSE_LOG_DEBUG << "Bind VFE DAVID start, upi: " << upi << ", dev size: " << devList.size();
    std::vector<bind_info> bindInfoList;
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::P_IDEV) {
            std::shared_ptr<CollectionDeviceIdevPfe> pfe =
                CollectionDevice::CollectionToDerived<CollectionDeviceIdevPfe>(dev->GetBondingIdev());
            std::shared_ptr<CollectionDeviceIdevVfe> vfe = pfe->GetSubDevVfe()[0];
            bind_info info{ToFeLoc(vfe->GetDeviceLoc()), ToDavidLoc(dev->GetDeviceLoc())};
            bindInfoList.push_back(info);
        }
    }
    UbseResult res = SendBindRequest(upi, bindInfoList);
    if (res != UBSE_OK) {
        if (state_ == NpuManagerState::RUNNING_ALLOC) {
            std::vector<un_bind_info> unbindInfoList;
            for (auto &info : bindInfoList) {
                unbindInfoList.push_back(un_bind_info{info.fe, info.david});
            }
            SendUnbindRequest(upi, unbindInfoList);
        }
        UBSE_LOG_ERROR << "Request Bind VFE DAVID failed, res: " << FormatRetCode(res);
        return res;
    }

    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::P_IDEV) {
            std::shared_ptr<CollectionDeviceIdevPfe> pfe =
                CollectionDevice::CollectionToDerived<CollectionDeviceIdevPfe>(dev->GetBondingIdev());
            if (pfe == nullptr || pfe->GetSubDevVfe().empty()) {
                continue;
            }
            std::shared_ptr<CollectionDeviceIdevVfe> vfe = pfe->GetSubDevVfe()[0];
            ResourceCollection::BindDevice(vfe, dev);
        }
    }
    if (state_ == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, upi, devList]() mutable -> UbseResult {
            return this->UnbindVfeDavid(upi, devList);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory_.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory_ success";
    }
    UBSE_LOG_INFO << "Bind VFE DAVID success";
    return UBSE_OK;
}