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
#ifndef UBSE_NPU_RESOURCE_COLLECTION_H
#define UBSE_NPU_RESOURCE_COLLECTION_H
#include <array>
#include <map>
#include <regex>
#include <shared_mutex>
#include "adapter_plugins/mti/ubse_mti_1825.h"
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "adapter_plugins/mti/ubse_mti_urma.h"
#include "ubse_common_def.h"
#include "ubse_npu_resource_collection_def.h"
namespace ubse::npu::controller {
using CollectionDevIdToDevice = std::map<CollectionDevId, std::shared_ptr<CollectionDevice>>;
using CollectionGuidToDevice = CollectionDevIdToDevice;
class ResourceCollection {
public:
    static ResourceCollection &GetInstance();
    /**
     * 静态资源采集
     * @return 采集结果， UBSE_OK代表采集成功，其它代表采集失败
     */
    UbseResult CollectStaticResource();
    // 数据查询
    /**
     * 通过devId和type来获取对应类型的设备
     * @param devId devId
     * @param type 设备类型
     * @return 类型对应的设备
     */
    std::shared_ptr<CollectionDevice> GetDeviceByDevId(const CollectionDevId &devId, const CollectionDeviceType &type);
    /**
     * 通过guid查询对应设备
     * @param guid guid
     * @return guid对应设备
     */
    std::shared_ptr<CollectionDevice> GetDeviceByGuid(const CollectionGuid &guid);

    /**
     * 通过type查询一类设备
     * @param type 设备类型
     * @param devices 一类设备集合
     * @return 查询结果， UBSE_OK代表采集成功，其它代表采集失败
     */
    UbseResult GetDevicesByType(const CollectionDeviceType &type, CollectionDevIdToDevice &devices);
    std::shared_ptr<CollectionDeviceBusi> GetDeviceHostBusInstance();
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> GetDeviceAllComSharedIdevVfe();
    UbseResult SetDevice(std::shared_ptr<CollectionDevice> &dev);
    UbseResult RemoveDeviceEmptyVmBusi(const std::shared_ptr<CollectionDevice> &device);
    /**
     * 设备相互绑定(内存概念)
     * @param dev1 需绑定设备1
     * @param dev2 需绑定设备2
     * @return 绑定结果
     */
    static UbseResult BindDevice(const std::shared_ptr<CollectionDevice> &dev1,
                                 const std::shared_ptr<CollectionDevice> &dev2);
    /**
     * 设备相互解绑(内存概念)
     * @param dev1 需解绑设备1
     * @param dev2 需解绑设备2
     * @return 解绑结果
     */
    static UbseResult UnbindDevice(const std::shared_ptr<CollectionDevice> &dev1,
                                   const std::shared_ptr<CollectionDevice> &dev2);
    UbseResult CheckDevTopo(int &cnt);

private:
    UbseResult ValidateDevice(const std::shared_ptr<CollectionDevice> &dev);
    ResourceCollection();
    UbseResult AddDevIdevPfe(const std::shared_ptr<CollectionDeviceUbCtrl> &ubCtrlDev,
                             const std::shared_ptr<CollectionDeviceIdevPfe> &pfeDev);
    UbseResult AddDevIdevVfe(const std::shared_ptr<CollectionDeviceIdevPfe> &pfeDev,
                             const std::shared_ptr<CollectionDeviceIdevVfe> &vfeDev);
    UbseResult AddDavidAndBindToIdevPfe(CollectDeviceLoc &davidDevLoc, CollectDeviceLoc &pfeDevLoc);
    UbseResult GetDevUbCtrlFromNicFeInfo(const mti::_1825::UbseMti1825Pf &feInfo,
                                         std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl);
    UbseResult AddNicFeAndSetAffinity(const mti::_1825::UbseMti1825Vf &mti1825Vf,
                                      std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl);
    UbseResult CollectUbCtrlIdev();
    UbseResult CollectIdevPfeDavid();
    UbseResult CollectAffinityNic();
    std::shared_ptr<CollectionDeviceIdevVfe> GetIdevVfeByGuid(const std::string &guid);
    UbseResult QueryBusiSubDevices(const std::vector<mti::bus_instance::UbseMtiGuid> &guids,
                                   std::shared_ptr<CollectionDeviceBusi> &devBusi);
    UbseResult CollectBusInstance();
    void ClearAllDevices();
    UbseResult BindVfeToNpu();

private:
    std::vector<CollectionDevIdToDevice> devIdToDevice_;
    CollectionGuidToDevice guidToDevice_;
    std::mutex mutex_;
    CollectionState state_;
};

} // namespace ubse::npu::controller
#endif // UBSE_NPU_RESOURCE_COLLECTION_H
