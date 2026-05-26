/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_topology_interface.h"

#include <mockcpp/mokc.h>

#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"

namespace ubse::mti {
using namespace ubse::context;
using namespace ubse::config;

void TestUbseTopoInterface::SetUp()
{
    Test::SetUp();
}

void TestUbseTopoInterface::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseTopoInterface, UbseGetLocalNodeInfo_ModuleLoadFailed)
{
    UbseMtiNodeInfo info;
    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(nullModule));
    EXPECT_EQ(UbseGetLocalNodeInfo(info), UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseTopoInterface, UbseGetLocalNodeInfo)
{
    UbseMtiNodeInfo info;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseLcneModule::UbseGetLocalNodeInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetLocalNodeInfo(info), UBSE_OK);
}

TEST_F(TestUbseTopoInterface, UbseGetAllNodeInfos_ModuleLoadFailed)
{
    std::vector<UbseMtiNodeInfo> infos;
    std::shared_ptr<UbseLcneModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(nullModule));
    EXPECT_EQ(UbseGetAllNodeInfos(infos), UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseTopoInterface, UbseGetAllNodeInfos_ubEnableTrue)
{
    std::vector<UbseMtiNodeInfo> infos;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(module));
    bool ubEnable = true;
    MOCKER_CPP(&ubse::config::UbseGetBool).stubs().with(any(), any(), outBound(ubEnable)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneModule::UbseGetAllNodeInfos).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseGetAllNodeInfos(infos), UBSE_OK);
}

TEST_F(TestUbseTopoInterface, UbseGetAllNodeInfos_IpsList)
{
    std::vector<UbseMtiNodeInfo> infos;
    std::shared_ptr<UbseLcneModule> module = std::make_shared<UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseLcneModule>).stubs().will(returnValue(module));
    bool ubEnable = false;
    MOCKER_CPP(&ubse::config::UbseGetBool).stubs().with(any(), any(), outBound(ubEnable)).will(returnValue(UBSE_OK));
    std::vector<std::string> ips;
    ips.emplace_back("192.168.100.100");
    MOCKER_CPP(&UbseLcneModule::GetClusterIpList).stubs().will(returnValue(ips));
    EXPECT_EQ(UbseGetAllNodeInfos(infos), UBSE_OK);
}

}  // namespace ubse::mti