/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef TEST_UBSE_NPU_MANAGER_API_H
#define TEST_UBSE_NPU_MANAGER_API_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_npu_manager_api.h"
#include "ubse_npu_resource_collection.h"
#include "ubse_npu_resource_collection_def.h"

namespace ubse::npu::controller::ut {

struct DeviceTopology {
    CollectDeviceLoc hostBusiLoc;
    CollectDeviceLoc vmBusiLoc;
    CollectDeviceLoc npuLoc;
    CollectDeviceLoc ubctlLoc;
    CollectDeviceLoc idevPfeLoc;
    CollectDeviceLoc idevVfeLoc;
    CollectDeviceLoc nicPfeLoc;
    CollectDeviceLoc nicVfeLoc;

    std::shared_ptr<CollectionDeviceBusi> hostBusi;
    std::shared_ptr<CollectionDeviceBusi> vmBusi;
    std::shared_ptr<CollectionDeviceDavid> npu;
    std::shared_ptr<CollectionDeviceUbCtrl> ubctl;
    std::shared_ptr<CollectionDeviceIdevPfe> idevPfe;
    std::shared_ptr<CollectionDeviceIdevVfe> idevVfe;
    std::shared_ptr<CollectionDeviceNicPfe> nicPfe;
    std::shared_ptr<CollectionDeviceNicVfe> nicVfe;
};

class TestUbseNpuManagerApi : public testing::Test {
public:
    TestUbseNpuManagerApi() = default;
    void SetUp() override;
    void TearDown() override;

    DeviceTopology BuildTopology();
    void PopulateCollection(const DeviceTopology& topo);

    DeviceTopology topo_;
};

} // namespace ubse::npu::controller::ut

#endif // TEST_UBSE_NPU_MANAGER_API_H