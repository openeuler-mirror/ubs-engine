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

#include "adapter_plugins/mti/ubse_mti_npu.h"
#include "vm_state_monitor/ubse_npu_monitor_service_api.h"

namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::mti::npu;

constexpr uint8_t SLEEP_TIME = 2;

constexpr uint8_t ROLLBACK_RETRY_TIME = 2;
constexpr uint8_t COMMON_RETRY_TIME = 5;
constexpr uint8_t HEX_RADIX = 16;

struct OperationHistory {
    std::function<UbseResult()> operation;
};

UbseResult ListAllocDevices(const UbseAllocRequest &requestInfo, std::vector<std::shared_ptr<IResource>> &devList,
                            std::string &newBusInstanceGuid);

UbseResult AllocDevicesAction(const UbseAllocRequest &requestInfo, std::string &newBusInstanceGuid,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list);

UbseResult FreeUbDeviceAction(const UbseAllocRequest &requestInfo,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nics);

UbseResult UbseGetAllocDeviceList(const UbseAllocRequest &requestInfo,
                                  std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
                                  std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list);

UbseResult AllocDevicesPreparation(const std::string &busInstanceGuid,
                                   std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                                   std::vector<std::shared_ptr<CollectionDeviceNic>> &nics);

UbseResult HandleOccupiedFilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                                            std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                                            std::vector<std::shared_ptr<CollectionDeviceNic>> &nics);

UbseResult FilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nics);

UbseResult CheckAndHandleOccupiedDevices(std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
                                         std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list);

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
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> PrepareRegisterInfos(
        const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList);
    UbseResult RegisterVfeToBusi(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
                                 const CollectionDevId &busiGuid);
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> FilterUnRegisterDevices(
        const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
        const std::shared_ptr<CollectionDeviceBusi> &busInstance);
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> PrepareUnRegisterInfos(
        const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList);
    UbseResult UnRegisterVfeFromBusi(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
                                     const CollectionDevId &busiGuid);
    UbseResult RollBack();
    void ExecuteFreeQueueBackGround();
    UbseResult SendUnRegisterNicRequest(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList);
    UbseResult SendUnRegisterVfeRequest(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList);
    UbseResult SendRegisterNicRequest(const std::shared_ptr<CollectionDeviceBusi> &busi,
                                      std::vector<std::shared_ptr<CollectionDeviceNic>> &devList);
    UbseResult SendRegisterVfeRequest(const std::shared_ptr<CollectionDeviceBusi> &busi,
                                      std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList);
    UbseResult SendUnbindRequest(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &unbindList);
    UbseResult SendBindRequest(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &bindList);
    void HandleUnRegisterFailure(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                 const std::shared_ptr<CollectionDeviceBusi> &busInstance);

private:
    std::stack<std::shared_ptr<OperationHistory>> operationHistory;
    std::queue<std::shared_ptr<OperationHistory>> futureProcedure;
    uint8_t retryTime = COMMON_RETRY_TIME;
    NpuManagerState state;
    bool collectionReady = false;
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

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npus_list;
    std::vector<std::shared_ptr<CollectionDeviceNic>> nics_list;
    auto ret = UbseGetAllocDeviceList(requestInfo, npus_list, nics_list);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get alloc npu,nic device list from collection";
        return ret; // 如果绑定失败，返回错误码
    }

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_ALLOC);

    ret = AllocDevicesAction(requestInfo, newBusInstanceGuid, npus_list, nics_list);
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
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list)
{
    auto &manager = UbseNpuManagerApi::GetInstance();
    uint16_t upi = requestInfo.upiStr;

    UbseResult ret = AllocDevicesPreparation(requestInfo.busInstanceGuid, npus_list, nics_list);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed in alloc npu,nic devices preparation";
        return ret; // 如果绑定失败，返回错误码
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
    ret = manager.RegisterIDevToBusi(npus_list, newBusInstanceGuid);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register npus to vm bus instance " << newBusInstanceGuid;
        return ret; // 如果注册失败，返回错误码
    }

    // 调用LCNE接口，绑定npu与vfe（LCNE会在该接口中完成npu与pfe解绑的动作）。
    ret = manager.BindVfeDavid(upi, npus_list);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to bind npus to vfes";
        return ret; // 如果绑定失败，返回错误码
    }
    // 对npu进行复位
    ret = manager.ResetNpu(npus_list);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reset npu";
        return ret; // 如果复位失败，返回错误码
    }
    if (!nics_list.empty()) {
        auto &collection = ResourceCollection::GetInstance();
        auto hostbusi = collection.GetDeviceHostBusInstance();
        if (hostbusi == nullptr) {
            UBSE_LOG_ERROR << "Failed to get host bus instance";
            return UBSE_ERROR;
        }
        CollectionDevId hostBusInstanceGuid = hostbusi->GetGuid();

        // 调用LCNE接口，解除1825与host bus instance的注册关系
        ret = manager.UnRegisterDevFromBusi(nics_list, hostBusInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister nics from host bus instance " << hostBusInstanceGuid;
            return ret; // 如果解注册失败，返回错误码
        }

        // 调用LCNE接口，将1825注册到虚机bus instance上
        ret = manager.RegisterDevToBusi(nics_list, newBusInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register nics to vm bus instance " << newBusInstanceGuid;
            return ret; // 如果注册失败，返回错误码
        }
    }

    UBSE_LOG_INFO << "Success to alloc npu,nic devices. Bus instance guid: " << newBusInstanceGuid;
    return UBSE_OK;
}

UbseResult UbseGetAllocDeviceList(const UbseAllocRequest &requestInfo,
                                  std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
                                  std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list)
{
    auto &collection = ResourceCollection::GetInstance();
    auto devices = requestInfo.ubDevList;

    for (auto &ubDevice : devices) {
        // 校验npu，nic id in 静态采集数据 (不存在则返回入参错误）.分别抽出NPU, NIC加入vector
        if (ubDevice.type == ResourceType::NPU) {
            CollectionDevId loc_str = CollectionStringUtil::CollectionJoinStr(
                static_cast<uint8_t>(CollectionDeviceType::NPU), ubDevice.slotId, ubDevice.chipId);
            std::shared_ptr<CollectionDeviceDavid> npu = CollectionDevice::CollectionToDerived<CollectionDeviceDavid>(
                collection.GetDeviceByDevId(loc_str, CollectionDeviceType::NPU));
            if (npu == nullptr) {
                UBSE_LOG_ERROR << "Failed to get npu " << loc_str << " from collection";
                return UBSE_ERROR_INVAL;
            }
            npus_list.push_back(npu);
        } else if (ubDevice.type == ResourceType::NIC) {
            CollectionDevId loc_str = CollectionStringUtil::CollectionJoinStr(
                static_cast<uint8_t>(CollectionDeviceType::NIC), ubDevice.slotId, ubDevice.chipId, ubDevice.index);
            std::shared_ptr<CollectionDeviceNic> nic = CollectionDevice::CollectionToDerived<CollectionDeviceNic>(
                collection.GetDeviceByDevId(loc_str, CollectionDeviceType::NIC));
            if (nic == nullptr) {
                UBSE_LOG_ERROR << "Failed to get nic " << loc_str << " from collection";
                return UBSE_ERROR_INVAL;
            }
            nics_list.push_back(nic);
        } else {
            UBSE_LOG_ERROR << "Invalid resource type in alloc request";
            return UBSE_ERROR_INVAL; // 使能设备类型不符合要求，错误码
        }
    }
    UBSE_LOG_INFO << "Success to get npu,nic devices from collection";
    return UBSE_OK;
}

UbseResult FreeUbDeviceImpl(const UbseAllocRequest &requestInfo)
{
    auto &manager = UbseNpuManagerApi::GetInstance();
    if (CheckCollection("free ub device") != UBSE_OK) {
        return UBSE_ERROR;
    }
    {
        std::unique_lock<std::mutex> lock(manager.mtx_);
        manager.cv_.wait(lock,
                         [&manager] { return manager.GetState() == UbseNpuManagerApi::NpuManagerState::AVAILABLE; });
    }

    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDeviceBusi> busInstance = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(
        collection.GetDeviceByGuid(requestInfo.busInstanceGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "Failed to get bus instance by guid " << requestInfo.busInstanceGuid;
        return UBSE_ERROR_INVAL;
    }

    if (busInstance->GetType() != CollectionDeviceType::VM_BUSINSTANCE) {
        UBSE_LOG_ERROR << "Invalid bus instance type";
        return UBSE_ERROR_INVAL;
    }

    uint16_t upi;
    if (ConvertStrToUint16(busInstance->GetUpiStr(), upi, HEX_RADIX) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid upi:" << busInstance->GetUpiStr();
        return UBSE_ERROR_INVAL;
    }

    std::vector<std::shared_ptr<CollectionDeviceNic>> nics = busInstance->GetSubDevNic();

    std::vector<std::shared_ptr<CollectionDeviceDavid>> npus;
    for (auto &vfe : busInstance->GetSubDevIdev()) {
        if (vfe->GetIsComSharedFe()) {
            continue;
        }
        std::shared_ptr<CollectionDeviceDavid> npu = vfe->GetBondingDevDavid();
        if (npu == nullptr) {
            UBSE_LOG_ERROR << "Failed to get bonding npu device for vfe " << vfe->GetIdStr();
            return UBSE_ERROR;
        }
        npus.push_back(npu);
    }

    manager.SetState(UbseNpuManagerApi::NpuManagerState::RUNNING_FREE);
    auto tmpRequest = requestInfo;
    tmpRequest.upiStr = upi;
    auto ret = FreeUbDeviceAction(tmpRequest, npus, nics);
    if (ret != UBSE_OK) {
        manager.SetState(UbseNpuManagerApi::NpuManagerState::FREE_BG);
        return UBSE_OK; // 去使能阶段调用某个lcne协议接口失败
    }

    manager.SetState(UbseNpuManagerApi::NpuManagerState::AVAILABLE);
    UBSE_LOG_INFO << "Success to free bus instance " << requestInfo.busInstanceGuid
                  << " and its related npu,nic devices";

    return UBSE_OK;
}

UbseResult FreeUbDeviceAction(const UbseAllocRequest &requestInfo,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nics)
{
    auto &collection = ResourceCollection::GetInstance();
    auto hostBusInstance = collection.GetDeviceHostBusInstance();
    if (hostBusInstance == nullptr) {
        UBSE_LOG_ERROR << "Failed to get host bus instance";
        return UBSE_ERROR;
    }
    CollectionDevId hostBusInstanceGuid = hostBusInstance->GetGuid();

    auto &manager = UbseNpuManagerApi::GetInstance();

    manager.FreeQueue(requestInfo, hostBusInstanceGuid, npus, nics);
    return manager.ExecuteFreeQueue();
}

std::pair<UbseResult, std::vector<std::shared_ptr<IResource>>> QueryVmBusInstances(ResourceCollection &collection)
{
    std::vector<std::shared_ptr<IResource>> vmBusInstances;
    CollectionDevIdToDevice vmMap;
    auto res = collection.GetDevicesByType(CollectionDeviceType::VM_BUSINSTANCE, vmMap);
    if (res != UBSE_OK || vmMap.empty()) {
        UBSE_LOG_WARN << "Failed to get vm bus instances";
        return {res, vmBusInstances};
    }

    for (auto &dev : vmMap) {
        auto vmBusInstance = CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(dev.second);
        if (vmBusInstance == nullptr) {
            UBSE_LOG_ERROR << "Failed to transfer vm bus instance " << dev.first << " to resource";
            return {UBSE_ERROR, vmBusInstances};
        }
        std::shared_ptr<BusiResource> vmBusiRes = std::make_shared<BusiResource>();
        res = UbseNpuControllerProcess::BusInstanceToResource(vmBusInstance, vmBusiRes);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to transfer vm bus instance " << dev.first << " to resource";
            return {res, vmBusInstances};
        }
        vmBusInstances.push_back(vmBusiRes);
    }

    return {UBSE_OK, vmBusInstances};
}

std::pair<UbseResult, std::vector<std::shared_ptr<IResource>>> QueryNpuDevices(ResourceCollection &collection)
{
    std::vector<std::shared_ptr<IResource>> npuDevices;
    CollectionDevIdToDevice npuDevMap;
    auto res = collection.GetDevicesByType(CollectionDeviceType::NPU, npuDevMap);
    if (res != UBSE_OK || npuDevMap.empty()) {
        UBSE_LOG_WARN << "Failed to get npus";
    }

    for (auto &dev : npuDevMap) {
        auto npuDevice = CollectionDevice::CollectionToDerived<CollectionDeviceDavid>(dev.second);
        std::shared_ptr<NpuResource> npuRes = std::make_shared<NpuResource>();
        res = UbseNpuControllerProcess::DeviceNpuToResource(npuDevice, npuRes);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to transfer npu " << dev.first << " to resource";
            return {res, npuDevices};
        }
        npuDevices.push_back(npuRes);
    }

    return {UBSE_OK, npuDevices};
}

std::pair<UbseResult, std::vector<std::shared_ptr<IResource>>> QueryNicDevices(ResourceCollection &collection)
{
    std::vector<std::shared_ptr<IResource>> nicDevices;
    CollectionDevIdToDevice nicDevMap;
    auto res = collection.GetDevicesByType(CollectionDeviceType::NIC, nicDevMap);
    if (res != UBSE_OK || nicDevMap.empty()) {
        UBSE_LOG_WARN << "Failed to get nics";
    }

    for (auto &dev : nicDevMap) {
        auto nicDevice = CollectionDevice::CollectionToDerived<CollectionDeviceNic>(dev.second);
        if (nicDevice == nullptr) {
            UBSE_LOG_ERROR << "Failed to transfer nic " << dev.first << " to resource";
            return {UBSE_ERROR, nicDevices};
        }
        std::shared_ptr<NicResource> nicRes = std::make_shared<NicResource>();
        res = UbseNpuControllerProcess::DeviceNicToResource(nicDevice, nicRes);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to transfer nic " << dev.first << " to resource";
            return {res, nicDevices};
        }
        nicDevices.push_back(nicRes);
    }

    return {UBSE_OK, nicDevices};
}

UbseResult QueryAllDevicesImpl(std::vector<std::shared_ptr<IResource>> &devList)
{
    auto &manager = UbseNpuManagerApi::GetInstance();
    if (CheckCollection("query ub device") != UBSE_OK) {
        return UBSE_ERROR;
    }
    {
        std::unique_lock<std::mutex> lock(manager.mtx_);
        manager.cv_.wait(lock,
                         [&manager] { return manager.GetState() == UbseNpuManagerApi::NpuManagerState::AVAILABLE; });
    }
    UBSE_LOG_INFO << "Start to query all bus instances and nic, npu devices...";
    auto &collection = ResourceCollection::GetInstance();
    // 查询并处理 VM bus instances
    auto vmBusInstances = QueryVmBusInstances(collection);
    if (vmBusInstances.first != UBSE_OK) {
        return vmBusInstances.first;
    }
    devList.insert(devList.end(), vmBusInstances.second.begin(), vmBusInstances.second.end());
    UBSE_LOG_INFO << "Success to add " << vmBusInstances.second.size() << " vm bus instances to devlist";
    // 查询并处理 NPU devices
    auto npuDevices = QueryNpuDevices(collection);
    if (npuDevices.first != UBSE_OK) {
        return npuDevices.first;
    }
    devList.insert(devList.end(), npuDevices.second.begin(), npuDevices.second.end());
    UBSE_LOG_INFO << "Success to add " << npuDevices.second.size() << " npus to devlist";
    // 查询并处理 NIC devices
    auto nicDevices = QueryNicDevices(collection);
    if (nicDevices.first != UBSE_OK) {
        return nicDevices.first;
    }
    devList.insert(devList.end(), nicDevices.second.begin(), nicDevices.second.end());
    UBSE_LOG_INFO << "Success to add " << nicDevices.second.size() << " nics to devlist";
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

UbseResult CheckOccupiedNpus(
    const std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> &occupied_npu_map)
{
    for (const auto &npu : npus_list) {
        std::shared_ptr<CollectionDevice> idev = npu->GetBondingIdev();
        if (idev == nullptr) {
            UBSE_LOG_ERROR << "Failed to get bonding idev for npu " << npu->GetIdStr();
            return UBSE_ERROR;
        }
        if (idev->GetType() == CollectionDeviceType::V_IDEV) {
            auto bondingIdev = CollectionDevice::CollectionToDerived<CollectionDeviceIdevVfe>(npu->GetBondingIdev());
            if (bondingIdev == nullptr) {
                UBSE_LOG_ERROR << "Failed to get bonding idev for npu " << npu->GetIdStr();
                return UBSE_ERROR;
            }
            auto businstances = bondingIdev->GetBondingDevBusi();
            if (businstances.empty()) {
                UBSE_LOG_WARN << "Failed to get bonding busi for npu " << npu->GetIdStr();
                continue;
            }
            CollectionDevId busInstanceGuid = businstances[0]->GetGuid();
            occupied_npu_map[busInstanceGuid].push_back(npu);
            UBSE_LOG_WARN << "NPU " << npu->GetIdStr() << " is already registered to bus instance " << busInstanceGuid;
        }
    }
    return UBSE_OK;
}

void CheckOccupiedNics(const std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list,
                       std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNic>>> &occupied_nic_map,
                       std::vector<std::shared_ptr<CollectionDeviceNic>> &recover_nics)
{
    for (const auto &nic : nics_list) {
        if (nic == nullptr) {
            continue;
        }
        std::shared_ptr<CollectionDeviceBusi> busi = nic->GetBondingDevBusi();
        if (busi == nullptr) {
            UBSE_LOG_WARN << "Failed to get bonding busi for nic " << nic->GetIdStr()
                          << " ,try to register it to host bus instance";
            recover_nics.push_back(nic);
        } else if (busi->GetType() == CollectionDeviceType::VM_BUSINSTANCE) {
            CollectionDevId busInstanceGuid = busi->GetGuid();
            occupied_nic_map[busInstanceGuid].push_back(nic);
            UBSE_LOG_WARN << "nic " << nic->GetIdStr() << " is already registered to bus instance " << busInstanceGuid;
        }
    }
}

UbseResult UnregisterAndUnbindNpus(
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> &occupied_npu_map)
{
    for (auto &[busInstanceGuid, occupied_npu_list] : occupied_npu_map) {
        auto &collection = ResourceCollection::GetInstance();
        std::shared_ptr<CollectionDeviceBusi> busInstance =
            CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busInstanceGuid));
        if (busInstance == nullptr) {
            UBSE_LOG_ERROR << "Failed to get bus instance by guid " << busInstanceGuid;
            return UBSE_ERROR_INVAL;
        }
        uint16_t upi;
        if (ConvertStrToUint16(busInstance->GetUpiStr(), upi, HEX_RADIX) != UBSE_OK) {
            UBSE_LOG_ERROR << "Invalid upi:" << busInstance->GetUpiStr();
            return UBSE_ERROR_INVAL;
        }
        auto ret = UbseNpuManagerApi::GetInstance().UnbindVfeDavid(upi, occupied_npu_list);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to unbind occupied npus from vfe";
            return ret;
        }
        ret = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(occupied_npu_list, busInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister occupied npus from bus instance " << busInstanceGuid;
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult RegisterNicsToHost(std::vector<std::shared_ptr<CollectionDeviceNic>> &recover_nics)
{
    if (!recover_nics.empty()) {
        auto hostBusInstance = ResourceCollection::GetInstance().GetDeviceHostBusInstance();
        if (hostBusInstance == nullptr) {
            UBSE_LOG_ERROR << "Failed to get host bus instance";
            return UBSE_ERROR;
        }
        auto ret = UbseNpuManagerApi::GetInstance().RegisterDevToBusi(recover_nics, hostBusInstance->GetGuid());
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register error nics to host bus instance";
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UnregisterAndRegisterNics(
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNic>>> &occupied_nic_map)
{
    for (auto &[BusInstanceGuid, occupied_nic_list] : occupied_nic_map) {
        auto ret = UbseNpuManagerApi::GetInstance().UnRegisterDevFromBusi(occupied_nic_list, BusInstanceGuid);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister occupied nics from bus instance " << BusInstanceGuid;
            return ret;
        }
        ret = RegisterNicsToHost(occupied_nic_list);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult CheckAndHandleOccupiedDevices(std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus_list,
                                         std::vector<std::shared_ptr<CollectionDeviceNic>> &nics_list)
{
    UBSE_LOG_INFO << "Start to check if there are any npus or nics in alloc request that are already registered to vm "
                     "bus instances";
    // 检查并处理已绑定的 NPU
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceDavid>>> occupied_npu_map;
    auto ret = CheckOccupiedNpus(npus_list, occupied_npu_map);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 检查并处理已绑定的 NIC
    std::map<std::string, std::vector<std::shared_ptr<CollectionDeviceNic>>> occupied_nic_map;
    std::vector<std::shared_ptr<CollectionDeviceNic>> recover_nics;
    CheckOccupiedNics(nics_list, occupied_nic_map, recover_nics);
    UBSE_LOG_INFO << "Start to unregister and unbind occupied " << occupied_npu_map.size() << " npus and "
                  << occupied_nic_map.size() << " nics";

    // 解注册和解绑定已绑定的 NPU
    ret = UnregisterAndUnbindNpus(occupied_npu_map);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 解注册和注册已绑定的 NIC 到主机总线实例
    ret = UnregisterAndRegisterNics(occupied_nic_map);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 注册未绑定的 NIC 到主机总线实例
    ret = RegisterNicsToHost(recover_nics);
    if (ret != UBSE_OK) {
        return ret;
    }

    UBSE_LOG_INFO << "Success to unregister and unbind occupied npus and nics";
    return UBSE_OK;
}

UbseResult HandleOccupiedFilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                                            std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                                            std::vector<std::shared_ptr<CollectionDeviceNic>> &nics)
{
    UbseResult ret = UBSE_OK;
    if (!npus.empty()) {
        ret = UbseNpuManagerApi::GetInstance().UnRegisterIDevFromBusi(npus, busInstance->GetGuid());
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister npus from bus instance " << busInstance->GetIdStr();
            return ret; // 如果解注册失败，返回错误码
        }
        uint16_t upi;
        if (ConvertStrToUint16(busInstance->GetUpiStr(), upi, HEX_RADIX) != UBSE_OK) {
            UBSE_LOG_ERROR << "Invalid upi:" << busInstance->GetUpiStr();
            return UBSE_ERROR_INVAL;
        }
        ret = UbseNpuManagerApi::GetInstance().UnbindVfeDavid(upi, npus);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unbind npus from vfe";
            return ret; // 如果绑定失败，返回错误码
        }
    }

    if (!nics.empty()) {
        ret = UbseNpuManagerApi::GetInstance().UnRegisterDevFromBusi(nics, busInstance->GetGuid());
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to unregister nics from bus instance " << busInstance->GetIdStr();
            return ret; // 如果解注册失败，返回错误码
        }

        ret = RegisterNicsToHost(nics);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register nics to host bus instance";
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

UbseResult AllocDevicesPreparation(const std::string &busInstanceGuid,
                                   std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                                   std::vector<std::shared_ptr<CollectionDeviceNic>> &nics)
{
    UbseResult ret;
    auto &collection = ResourceCollection::GetInstance();

    // 处理businstance占用
    if (!busInstanceGuid.empty()) {
        UBSE_LOG_INFO << "Bus instance " << busInstanceGuid << " already exists.";
        std::shared_ptr<CollectionDeviceBusi> busInstance =
            CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busInstanceGuid));
        if (busInstance == nullptr) {
            return UBSE_ERROR_INVAL;
        }

        ret = FilterDeviceVMBusi(busInstance, npus, nics);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to filter npus or nics";
            return ret; // 如果筛选失败，返回错误码
        }
    }

    // 处理抢占，vm bus instance注销待抢占列表vfe，david归位.
    ret = CheckAndHandleOccupiedDevices(npus, nics);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to handle occupied npus or nics";
        return ret; // 如果抢占失败，返回错误码
    }

    return UBSE_OK;
}

UbseResult FilterDeviceVMBusi(std::shared_ptr<CollectionDeviceBusi> &busInstance,
                              std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                              std::vector<std::shared_ptr<CollectionDeviceNic>> &nics)
{
    auto busNics = busInstance->GetSubDevNic();
    // 1. 找出 busNics 中不在当前 nics 中的设备（tmp_nics）
    std::vector<std::shared_ptr<CollectionDeviceNic>> tmp_nics;
    for (const auto &nic : busNics) {
        if (std::find(nics.begin(), nics.end(), nic) == nics.end()) {
            tmp_nics.push_back(nic);
        }
    }
    // 2. 找出当前 nics 中不在 busNics 中的设备（new_nics_list）
    std::vector<std::shared_ptr<CollectionDeviceNic>> new_nics_list;
    for (const auto &nic : nics) {
        if (std::find(busNics.begin(), busNics.end(), nic) == busNics.end()) {
            new_nics_list.push_back(nic);
        }
    }
    nics = std::move(new_nics_list);

    auto vfes = busInstance->GetSubDevIdev();
    // 1. 找出 bus instance 上绑定的 npu 不在当前 npus 中的设备（tmp_npus）
    std::vector<std::shared_ptr<CollectionDeviceDavid>> tmp_npus;
    for (const auto &vfe : vfes) {
        auto npu = vfe->GetBondingDevDavid();
        if (npu != nullptr) {
            if (std::find(npus.begin(), npus.end(), npu) == npus.end()) {
                tmp_npus.push_back(npu);
            }
        }
    }

    UBSE_LOG_WARN << "Bus instance " << busInstance->GetIdStr() << " is already registered with " << tmp_npus.size()
                  << " npus and " << tmp_nics.size() << " nics. Start to unregister them";

    UbseResult ret = HandleOccupiedFilterDeviceVMBusi(busInstance, tmp_npus, tmp_nics);
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

UbseNpuManagerApi::UbseNpuManagerApi() : state(NpuManagerState::INIT) {}

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
    UbseResult ret = SendUnRegisterNicRequest(devList);
    if (ret != UBSE_OK) {
        HandleUnRegisterFailure(devList, busInstance);
        UBSE_LOG_ERROR << "Request UnRegister nic from bus instance failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (const auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingDevBusi() != nullptr) {
            ResourceCollection::UnbindDevice(dev, busInstance);
        }
    }

    if (state == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, devList, busiGuid]() mutable -> UbseResult {
            return this->RegisterDevToBusi(devList, busiGuid);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory success";
    }
    UBSE_LOG_INFO << "UnRegister nic from bus instance success";
    return UBSE_OK;
}
void UbseNpuManagerApi::HandleUnRegisterFailure(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList,
                                                const std::shared_ptr<CollectionDeviceBusi> &busInstance)
{
    if (this->state == NpuManagerState::RUNNING_ALLOC) {
        // 意图注销的全部dev都重新注册，不关注是否成功
        std::vector<std::shared_ptr<CollectionDeviceNic>> nicList;
        for (const auto &dev : devList) {
            if (dev != nullptr && dev->GetBondingDevBusi() != nullptr) {
                nicList.push_back(dev);
            }
        }
        this->SendRegisterNicRequest(busInstance, nicList);
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

    std::vector<std::shared_ptr<CollectionDeviceNic>> nicList;
    for (auto &dev : devList) {
        if (dev == nullptr || dev->GetAffinityDevUbCtrl() == nullptr) {
            continue;
        }
        nicList.push_back(dev);
    }
    UbseResult ret = SendRegisterNicRequest(busInstance, nicList);
    if (ret != UBSE_OK) {
        if (state == NpuManagerState::RUNNING_ALLOC) {
            // 回滚当前
            SendUnRegisterNicRequest(nicList);
        }
        UBSE_LOG_ERROR << "Request Register nic to bus instance failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (auto &dev : devList) {
        ResourceCollection::BindDevice(dev, busInstance);
    }

    if (state == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, devList, busiGuid]() mutable -> UbseResult {
            return this->UnRegisterDevFromBusi(devList, busiGuid);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory success";
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
    std::vector<UbseMtiIdevVfeDavidPair> unbindList;
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::V_IDEV) {
            UbseMtiIdevVfe mtivfe;
            mtivfe.vfeId = dev->GetBondingIdev()->GetDeviceLoc().vf_id;
            mtivfe.pfeId = dev->GetBondingIdev()->GetDeviceLoc().pf_id;
            UbseMtiDavid mtiDavid = ConvertToUbseMtiDavid(dev);
            unbindList.push_back(make_pair(mtivfe, mtiDavid));
        }
    }

    UbseResult res = SendUnbindRequest(upi, unbindList);
    if (res != UBSE_OK) {
        if (state == NpuManagerState::RUNNING_ALLOC) {
            SendBindRequest(upi, unbindList);
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
    if (state == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, upi, devList]() mutable -> UbseResult {
            return this->BindVfeDavid(upi, devList);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory success";
    }
    UBSE_LOG_INFO << "Unbind VFE DAVID success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::BindVfeDavid(uint16_t upi, std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList)
{
    UBSE_LOG_DEBUG << "Bind VFE DAVID start, upi: " << upi << ", dev size: " << devList.size();
    std::vector<UbseMtiIdevVfeDavidPair> bindList;
    for (auto &dev : devList) {
        if (dev != nullptr && dev->GetBondingIdev() != nullptr &&
            dev->GetBondingIdev()->GetType() == CollectionDeviceType::P_IDEV) {
            std::shared_ptr<CollectionDeviceIdevPfe> pfe =
                CollectionDevice::CollectionToDerived<CollectionDeviceIdevPfe>(dev->GetBondingIdev());
            std::shared_ptr<CollectionDeviceIdevVfe> vfe = pfe->GetSubDevVfe()[0];

            UbseMtiIdevVfe mtivfe;
            mtivfe.vfeId = vfe->GetDeviceLoc().vf_id;
            mtivfe.pfeId = vfe->GetDeviceLoc().pf_id;
            UbseMtiDavid mtiDavid = ConvertToUbseMtiDavid(dev);
            bindList.push_back(make_pair(mtivfe, mtiDavid));
        }
    }
    UbseResult res = SendBindRequest(upi, bindList);
    if (res != UBSE_OK) {
        if (state == NpuManagerState::RUNNING_ALLOC) {
            SendUnbindRequest(upi, bindList);
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
    if (state == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, upi, devList]() mutable -> UbseResult {
            return this->UnbindVfeDavid(upi, devList);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory success";
    }
    UBSE_LOG_INFO << "Bind VFE DAVID success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::ResetNpu(std::vector<std::shared_ptr<CollectionDeviceDavid>> &devList)
{
    for (auto &npu : devList) {
        auto loc = npu->GetDeviceLoc();
        auto ret = ubse::npu::vm_monitor::ResetNpu(loc.chip_id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to reset npu, guid:" << npu->GetGuid() << FormatRetCode(ret);
            return ret; // 如果绑定失败，返回错误码
        }
    }
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::ResetNpu(const std::vector<UbDevice> &ubDevList)
{
    for (auto &ubDev : ubDevList) {
        if (ubDev.type != ResourceType::NPU) {
            continue;
        }
        auto ret = ubse::npu::vm_monitor::ResetNpu(ubDev.chipId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to reset npu, chipId:" << ubDev.chipId << FormatRetCode(ret);
            return ret; // 如果绑定失败，返回错误码
        }
    }
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::CreateVMBusi(uint16_t upi, CollectionGuid &busiGuid)
{
    UBSE_LOG_DEBUG << "Create VM bus instance start";
    UbseResult res = UBSE_ERROR;
    UbseMtiBusInstance mtiBusi;
    for (uint8_t i = 0; i < retryTime; i++) {
        UBSE_LOG_DEBUG << "Send Create Request start, retry: " << static_cast<int>(i);
        res = UbseMtiBusInstanceOperation::CreateVMBusInstance(upi, mtiBusi);
        if (res == UBSE_OK) {
            break;
        }
        sleep(SLEEP_TIME);
    }

    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Request Create VM bus instance failed";
        return UBSE_ERROR;
    }

    UBSE_LOG_DEBUG << "change state in collection";
    CollectionDevId guid = CollectionStringUtil::GuidToStr(mtiBusi.guid);
    auto busi = std::make_shared<CollectionDeviceBusi>(guid, mtiBusi.eid, std::to_string(upi),
                                                       CollectionDeviceType::VM_BUSINSTANCE);
    auto &collection = ResourceCollection::GetInstance();
    collection.SetDevice(busi);

    busiGuid = guid;
    UBSE_LOG_INFO << "Create VM bus instance " << busiGuid << " success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::DestroyVMBusi(const CollectionDevId &busiGuid)
{
    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDevice> busInstance = collection.GetDeviceByGuid(busiGuid);
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "bus instance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    UbseResult res = UBSE_ERROR;
    auto mtiBusInstance = ConvertToUbseMtiBusi(busInstance);
    for (uint8_t i = 0; i < retryTime; i++) {
        UBSE_LOG_DEBUG << "Send Destroy Request Msg start, retry: " << static_cast<int>(i);
        res = UbseMtiBusInstanceOperation::DestroyVmBusInstance(mtiBusInstance);
        if (res == UBSE_OK) {
            break;
        }
        sleep(SLEEP_TIME);
    }
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Request Destroy VM bus instance failed";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "change state in collection";
    collection.RemoveDeviceEmptyVmBusi(busInstance);
    UBSE_LOG_INFO << "Destroy VM bus instance " << busiGuid << " success";
    return UBSE_OK;
}

std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> UbseNpuManagerApi::FilterDevices(
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
    const std::shared_ptr<CollectionDeviceBusi> &busInstance)
{
    auto bonding_vfes = busInstance->GetSubDevIdev();
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> newDevList;

    for (auto &dev : devList) {
        // 检查 dev 是否在 bonding_vfes 中
        auto it = std::find(bonding_vfes.begin(), bonding_vfes.end(), dev);
        if (it == bonding_vfes.end()) {
            newDevList.push_back(dev);
        }
    }

    return newDevList;
}

std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> UbseNpuManagerApi::PrepareRegisterInfos(
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList)
{
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList;
    for (auto &dev : devList) {
        if (dev == nullptr) {
            continue;
        }
        vfeList.push_back(dev);
    }
    return vfeList;
}

UbseResult UbseNpuManagerApi::RegisterVfeToBusi(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
                                                const CollectionDevId &busiGuid)
{
    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDeviceBusi> busInstance =
        CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busiGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "bus instance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    // 检查并过滤设备
    auto newDevList = FilterDevices(devList, busInstance);
    devList = std::move(newDevList);
    if (devList.empty()) {
        UBSE_LOG_INFO << "all vfe has bonding to busi, no need to register";
        return UBSE_OK;
    }
    UBSE_LOG_DEBUG << "Start Register vfe to bus instance, vfe size: " << devList.size()
                   << ", bus instance: " << busiGuid;

    // 准备注册信息
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList = PrepareRegisterInfos(devList);

    // 发送注册请求
    UbseResult ret = SendRegisterVfeRequest(busInstance, vfeList);
    if (ret != UBSE_OK) {
        if (state == NpuManagerState::RUNNING_ALLOC) {
            // 回滚当前
            SendUnRegisterNicRequest(vfeList);
        }
        UBSE_LOG_ERROR << "Request Register vfe to bus instance failed";
        return ret;
    }
    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (const auto &dev : devList) {
        ResourceCollection::BindDevice(dev, busInstance);
    }
    if (state == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, devList, busiGuid]() mutable -> UbseResult {
            return this->UnRegisterVfeFromBusi(devList, busiGuid);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory success";
    }
    UBSE_LOG_DEBUG << "Register vfe to bus instance success";
    return UBSE_OK;
}

std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> UbseNpuManagerApi::FilterUnRegisterDevices(
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
    const std::shared_ptr<CollectionDeviceBusi> &busInstance)
{
    auto bonding_vfes = busInstance->GetSubDevIdev();
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> newDevList;

    for (auto &dev : devList) {
        // 检查 dev 是否在 bonding_vfes 中
        auto it = std::find(bonding_vfes.begin(), bonding_vfes.end(), dev);
        if (it != bonding_vfes.end()) {
            newDevList.push_back(dev);
        }
    }

    return newDevList;
}

std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> UbseNpuManagerApi::PrepareUnRegisterInfos(
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList)
{
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList;
    for (auto &dev : devList) {
        if (dev == nullptr) {
            continue;
        }
        vfeList.push_back(dev);
    }
    return vfeList;
}

UbseResult UbseNpuManagerApi::UnRegisterVfeFromBusi(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList,
                                                    const CollectionDevId &busiGuid)
{
    auto &collection = ResourceCollection::GetInstance();
    std::shared_ptr<CollectionDeviceBusi> busInstance =
        CollectionDevice::CollectionToDerived<CollectionDeviceBusi>(collection.GetDeviceByGuid(busiGuid));
    if (busInstance == nullptr) {
        UBSE_LOG_ERROR << "busInstance " << busiGuid << " not found";
        return UBSE_ERROR;
    }

    // 检查并过滤设备
    auto newDevList = FilterDevices(devList, busInstance);
    devList = std::move(newDevList);
    if (devList.empty()) {
        return UBSE_OK;
    }

    // 准备注册信息
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> vfeList = PrepareUnRegisterInfos(devList);

    UBSE_LOG_DEBUG << "Start UnRegister vfe from bus instance, vfe size: " << vfeList.size()
                   << ", bus instance: " << busiGuid;

    UbseResult ret = SendUnRegisterVfeRequest(vfeList);
    if (ret != UBSE_OK) {
        if (state == NpuManagerState::RUNNING_ALLOC) {
            // 回滚当前
            SendRegisterVfeRequest(busInstance, devList);
        }
        UBSE_LOG_ERROR << "Request UnRegister vfe from bus instance failed";
        return ret;
    }

    UBSE_LOG_DEBUG << "Request success, now change state in collection";
    for (auto &dev : devList) {
        ResourceCollection::UnbindDevice(dev, busInstance);
    }

    if (state == NpuManagerState::RUNNING_ALLOC) {
        auto rollbackFunc = [this, devList, busiGuid]() mutable -> UbseResult {
            return this->RegisterVfeToBusi(devList, busiGuid);
        };
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = rollbackFunc;
        operationHistory.push(operation);
        UBSE_LOG_DEBUG << "push operation to operationHistory success";
    }
    UBSE_LOG_DEBUG << "UnRegister vfe from bus instance success";
    return UBSE_OK;
}

UbseResult UbseNpuManagerApi::SendUnRegisterNicRequest(std::vector<std::shared_ptr<CollectionDeviceNic>> &devList)
{
    UBSE_LOG_DEBUG << "Start Send UnRegister Nic Request, infos size: " << devList.size();
    UbseResult ret = UBSE_ERROR;
    auto mti1825Vf = ConvertToUbseMti1825Vf(devList);
    for (uint8_t retry_i = 0; retry_i < retryTime; retry_i++) {
        ret = UbseMti1825Operation::UnReg1825FeFromVmBusInstance(mti1825Vf);
        if (ret != UBSE_OK) {
            UBSE_LOG_DEBUG << "SendMsg failed";
            sleep(SLEEP_TIME);
            continue;
        }
        UBSE_LOG_DEBUG << "Unregister Nic from busi request success";
    }
    UBSE_LOG_DEBUG << "Finish Send UnRegister Request, " << FormatRetCode(ret);
    return ret;
}

UbseResult UbseNpuManagerApi::SendUnRegisterVfeRequest(std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList)
{
    UBSE_LOG_DEBUG << "Start Send UnRegister Vfe Request, infos size: " << devList.size();
    UbseResult ret = UBSE_ERROR;
    auto mtiVfeList = ConvertToUbseMtiIdevVfe(devList);
    for (uint8_t retry_i = 0; retry_i < retryTime; retry_i++) {
        ret = UbseMtiIdevFeOperation::UnRegDavidFeFromVmBusInstance(mtiVfeList);
        if (ret != UBSE_OK) {
            UBSE_LOG_DEBUG << "SendMsg failed";
            sleep(SLEEP_TIME);
            continue;
        }
        UBSE_LOG_DEBUG << "Unregister Vfe from busi request success";
    }
    UBSE_LOG_DEBUG << "Finish Send UnRegister Request, " << FormatRetCode(ret);
    return ret;
}

UbseResult UbseNpuManagerApi::SendRegisterVfeRequest(const std::shared_ptr<CollectionDeviceBusi> &busi,
                                                     std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &devList)
{
    UBSE_LOG_DEBUG << "Start Send Vfe Register Request, infos size: " << devList.size();
    UbseResult ret = UBSE_ERROR;
    auto mtiVfeList = ConvertToUbseMtiIdevVfe(devList);
    auto mtiBusi = ConvertToUbseMtiBusi(busi);

    for (uint8_t retry_i = 0; retry_i < retryTime; retry_i++) {
        UBSE_LOG_DEBUG << "Send Register Vfe Request start, retry: " << retry_i;
        ret = UbseMtiIdevFeOperation::RegDavidFeToVmBusInstance(mtiBusi, mtiVfeList);
        if (ret != UBSE_OK) {
            UBSE_LOG_DEBUG << "Send Register Request failed";
            sleep(SLEEP_TIME);
            continue;
        }
        UBSE_LOG_DEBUG << "Register VFE to busi request success";
    }
    UBSE_LOG_DEBUG << "Finish Send Register Request, " << FormatRetCode(ret);
    return ret;
}

UbseResult UbseNpuManagerApi::SendRegisterNicRequest(const std::shared_ptr<CollectionDeviceBusi> &busi,
                                                     std::vector<std::shared_ptr<CollectionDeviceNic>> &devList)
{
    UBSE_LOG_DEBUG << "Start Send Nic Register Request, infos size: " << devList.size();
    UbseResult ret = UBSE_ERROR;
    auto mti1825Vf = ConvertToUbseMti1825Vf(devList);
    auto mtiBusi = ConvertToUbseMtiBusi(busi);

    for (uint8_t retry_i = 0; retry_i < retryTime; retry_i++) {
        UBSE_LOG_DEBUG << "Send Register Request Msg start, retry: " << retry_i;
        ret = UbseMti1825Operation::Reg1825FeToVmBusInstance(mtiBusi, mti1825Vf);
        if (ret != UBSE_OK) {
            UBSE_LOG_DEBUG << "Send Register Request failed";
            sleep(SLEEP_TIME);
            continue;
        }
        UBSE_LOG_DEBUG << "Register NIC to busi request success";
    }
    UBSE_LOG_DEBUG << "Finish Send Register Request, " << FormatRetCode(ret);
    return ret;
}

UbseResult UbseNpuManagerApi::SendUnbindRequest(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &unbindList)
{
    UBSE_LOG_DEBUG << "Send Unbind Request, unbindInfoList size: " << unbindList.size();

    UbseResult ret;
    for (uint8_t retry_i = 0; retry_i < retryTime; ++retry_i) {
        UBSE_LOG_DEBUG << "Send Unbind Request start, retry: " << retry_i;
        ret = UbseMtiIdevFeOperation::UnBindDavid(upi, unbindList);
        if (ret != UBSE_OK) {
            UBSE_LOG_DEBUG << "Send Unbind Request failed";
            sleep(SLEEP_TIME);
            continue;
        }
        UBSE_LOG_DEBUG << "Unbind VFE and David request success";
    }

    return ret;
}

UbseResult UbseNpuManagerApi::SendBindRequest(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair> &bindList)
{
    UBSE_LOG_DEBUG << "Send Bind Request, bindInfoList size: " << bindList.size();

    UbseResult ret;
    for (uint8_t retry_i = 0; retry_i < retryTime; retry_i++) {
        ret = UbseMtiIdevFeOperation::BindDavid(upi, bindList);
        if (ret != UBSE_OK) {
            UBSE_LOG_DEBUG << "Send bind Request failed";
            sleep(SLEEP_TIME);
            continue;
        }
        UBSE_LOG_DEBUG << "Bind VFE and David request success";
    }
    return ret;
}

void UbseNpuManagerApi::SetState(NpuManagerState stateX)
{
    state = stateX;
    if (stateX == NpuManagerState::AVAILABLE) {
        retryTime = COMMON_RETRY_TIME;
    } else if (stateX == NpuManagerState::ROLLBACK) {
        retryTime = ROLLBACK_RETRY_TIME;
        RollBack();
    } else if (stateX == NpuManagerState::RUNNING_ALLOC) {
        while (!operationHistory.empty()) {
            operationHistory.pop();
        }
        retryTime = COMMON_RETRY_TIME;
    } else if (stateX == NpuManagerState::RUNNING_FREE) {
        while (!futureProcedure.empty()) {
            futureProcedure.pop();
        }
        retryTime = COMMON_RETRY_TIME;
    } else if (stateX == NpuManagerState::ROLLBACK_BG) {
        retryTime = COMMON_RETRY_TIME;
    } else if (stateX == NpuManagerState::FREE_BG) {
        retryTime = COMMON_RETRY_TIME;
        ExecuteFreeQueueBackGround();
    }
}

UbseNpuManagerApi::NpuManagerState UbseNpuManagerApi::GetState()
{
    return state;
}

UbseResult UbseNpuManagerApi::RollBack()
{
    while (!operationHistory.empty()) {
        std::shared_ptr<OperationHistory> op = operationHistory.top();
        // 调用存储的操作
        UbseResult result = op->operation();
        if (result == UBSE_OK) {
            operationHistory.pop();
            continue;
        }
        SetState(NpuManagerState::ROLLBACK_BG);
        // 创建异步线程，继续rollback
        std::thread rollbackThread([this]() {
            std::lock_guard<std::mutex> lock(mtx_);
            while (!operationHistory.empty()) {
                std::shared_ptr<OperationHistory> op_bg = operationHistory.top();
                operationHistory.pop();
                UbseResult res = op_bg->operation();
                if (res == UBSE_ERROR) {
                    UBSE_LOG_ERROR << "rollback failed";
                    break;
                }
            }
            SetState(NpuManagerState::AVAILABLE);
            cv_.notify_all();
        });
        // 分离线程，使其在主线程结束后继续运行
        rollbackThread.detach();
        return result;
    }
    SetState(NpuManagerState::AVAILABLE);
    return UBSE_OK;
}
bool UbseNpuManagerApi::GetCollectionReady()
{
    return collectionReady;
}

void UbseNpuManagerApi::SetCollectionReady(bool ready)
{
    collectionReady = ready;
}
void UbseNpuManagerApi::FreeQueue(const UbseAllocRequest &requestInfo, const CollectionDevId &hostBusInstanceGuid,
                                  std::vector<std::shared_ptr<CollectionDeviceDavid>> &npus,
                                  std::vector<std::shared_ptr<CollectionDeviceNic>> &nics)
{
    auto upi = requestInfo.upiStr;
    auto busInstanceGuid = requestInfo.busInstanceGuid;

    // 辅助函数，用于创建并添加操作到futureProcedure队列
    auto addOperation = [this](const auto &func) {
        auto operation = std::make_shared<OperationHistory>();
        operation->operation = func;
        futureProcedure.push(operation);
    };

    // 调用LCNE接口，绑定npu与pfe（LCNE会在该接口中完成npu与vfe解绑的动作）
    addOperation([this, upi, npus]() mutable -> UbseResult { return this->UnbindVfeDavid(upi, npus); });

    // 调用LCNE接口，解注册vfe
    addOperation([this, npus, busInstanceGuid]() mutable -> UbseResult {
        return this->UnRegisterIDevFromBusi(npus, busInstanceGuid);
    });

    // 调用LCNE接口，解注册1825
    addOperation([this, nics, busInstanceGuid]() mutable -> UbseResult {
        return this->UnRegisterDevFromBusi(nics, busInstanceGuid);
    });

    // 调用LCNE接口，把1825注册到host bus instance上
    addOperation([this, nics, hostBusInstanceGuid]() mutable -> UbseResult {
        return this->RegisterDevToBusi(nics, hostBusInstanceGuid);
    });

    // 调用LCNE接口，删除虚机bus instance
    addOperation([this, busInstanceGuid]() mutable -> UbseResult { return this->DestroyVMBusi(busInstanceGuid); });

    // 调用ipmi接口，复位NPU
    auto ubDevs = requestInfo.ubDevList;
    addOperation([this, ubDevs]() mutable -> UbseResult { return this->ResetNpu(ubDevs); });
}

UbseResult UbseNpuManagerApi::ExecuteFreeQueue()
{
    UbseResult res = UBSE_OK;
    while (!futureProcedure.empty()) {
        std::shared_ptr<OperationHistory> op_bg = futureProcedure.front();
        res = op_bg->operation();
        if (res == UBSE_ERROR) {
            UBSE_LOG_WARN << "Running free queue failed. " << FormatRetCode(res);
            return res;
        } else {
            futureProcedure.pop();
        }
    }
    return res;
}

void UbseNpuManagerApi::ExecuteFreeQueueBackGround()
{
    std::thread futureThread([this]() {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!futureProcedure.empty()) {
            std::shared_ptr<OperationHistory> op_bg = futureProcedure.front();
            UbseResult res = op_bg->operation();
            futureProcedure.pop();
            if (res == UBSE_ERROR) {
                UBSE_LOG_WARN << "Background running free queue failed. " << FormatRetCode(res);
                break;
            }
        }
        SetState(NpuManagerState::AVAILABLE);
        cv_.notify_all();
    });
    futureThread.detach();
}

std::vector<UbseMtiIdevVfe> ConvertToUbseMtiIdevVfe(
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &vfeList)
{
    std::vector<UbseMtiIdevVfe> result;

    for (const auto &vfe : vfeList) {
        if (vfe == nullptr) {
            continue;
        }
        UbseMtiIdevVfe mtivfe;
        mtivfe.vfeId = vfe->GetDeviceLoc().vf_id;
        mtivfe.pfeId = vfe->GetDeviceLoc().pf_id;
        result.push_back(mtivfe);
    }

    return result;
}

std::vector<UbseMti1825Vf> ConvertToUbseMti1825Vf(const std::vector<std::shared_ptr<CollectionDeviceNic>> &nicList)
{
    std::vector<UbseMti1825Vf> result;
    result.reserve(nicList.size());

    for (const auto &nic : nicList) {
        if (nic == nullptr) {
            continue;
        }
        const auto &devLoc = nic->GetDeviceLoc();

        UbseMti1825Vf mti1825Vf;
        mti1825Vf.slotId = devLoc.slot_id;
        mti1825Vf.chipId = devLoc.chip_id;
        mti1825Vf.dieId = devLoc.die_id;
        mti1825Vf.pfId = devLoc.pf_id;
        mti1825Vf.vfId = devLoc.vf_id;

        result.push_back(mti1825Vf);
    }
    return result;
}

UbseMtiBusInstance ConvertToUbseMtiBusi(const std::shared_ptr<CollectionDeviceBusi> &busInstance)
{
    UbseMtiBusInstance mtiBushiInstance;
    mtiBushiInstance.eid = busInstance->GetEid().content;
    return mtiBushiInstance;
}

UbseMtiDavid ConvertToUbseMtiDavid(const std::shared_ptr<CollectionDeviceDavid> &davidDevice)
{
    CollectDeviceLoc loc = davidDevice->GetDeviceLoc();

    UbseMtiDavid mtiDavid;
    mtiDavid.slotId = loc.slot_id;
    mtiDavid.chipId = loc.chip_id;
    mtiDavid.channelId = 0xff; // 默认值

    return mtiDavid;
}
} // namespace ubse::npu::controller
