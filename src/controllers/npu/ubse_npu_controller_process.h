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

#ifndef UBSE_NPU_CONTROLLER_PROCESS_H
#define UBSE_NPU_CONTROLLER_PROCESS_H

#include <vector>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_npu_resource_collection_def.h"
#include "ubse_npu_source_def.h"

namespace ubse::npu::controller {
using namespace ubse::common::def;
class UbseNpuControllerProcess {
public:
    static UbseResult ProcessNpuDevice(const UbDevice& ubDevice, std::vector<std::shared_ptr<IResource>>& devList);

    static UbseResult ProcessNicPfeDevice(const UbDevice& ubDevice, std::vector<std::shared_ptr<IResource>>& devList);

    static UbseResult ProcessNicVfeDevice(const UbDevice& ubDevice, std::vector<std::shared_ptr<IResource>>& devList);

    static UbseResult ProcessBusiResource(const UbseAllocRequest& requestInfo,
                                          std::vector<std::shared_ptr<IResource>>& devList,
                                          const std::string& newBusInstanceGuid);

    static UbseResult DeviceNpuToResource(const std::shared_ptr<CollectionDeviceDavid>& npu,
                                          std::shared_ptr<NpuResource>& npuRes);

    static UbseResult DeviceNicPfeToResource(const std::shared_ptr<CollectionDeviceNicPfe>& nic,
                                             std::shared_ptr<NicPfeResource>& nicRes);

    static UbseResult DeviceNicVfeToResource(const std::shared_ptr<CollectionDeviceNicVfe>& nic,
                                             std::shared_ptr<NicVfeResource>& nicRes);

    static UbseResult BusInstanceToResource(const std::shared_ptr<CollectionDeviceBusi>& busi,
                                            std::shared_ptr<BusiResource>& busRes);

private:
    static std::shared_ptr<CollectionDeviceIdevPfe> ProcessBondingDevice(
        const std::shared_ptr<CollectionDeviceDavid>& npu, std::shared_ptr<NpuResource>& npuRes);

    static void SetNicPfeLocation(const std::shared_ptr<CollectionDeviceNicPfe>& nic,
                                  std::shared_ptr<NicPfeResource>& nicRes);

    static void SetNicVfeLocation(const std::shared_ptr<CollectionDeviceNicVfe>& nic,
                                  std::shared_ptr<NicVfeResource>& nicRes);

    template <typename T>
    static void SetNicBusInstanceGuid(const std::shared_ptr<CollectionDeviceNic>& nic, std::shared_ptr<T>& nicRes)
    {
        std::shared_ptr<CollectionDeviceBusi> busi = nic->GetBondingDevBusi();
        if (!busi) {
            nicRes->SetBusInstanceGuid("");
        } else {
            nicRes->SetBusInstanceGuid(busi->GetGuid());
        }
    };
};

} // namespace ubse::npu::controller
#endif // UBSE_NPU_CONTROLLER_PROCESS_H
