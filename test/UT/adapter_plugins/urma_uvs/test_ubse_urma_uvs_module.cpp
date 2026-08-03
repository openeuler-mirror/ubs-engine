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

#include "test_ubse_urma_uvs_module.h"
#include <dlfcn.h>
#include <mockcpp/mokc.h>
#include <string>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urma::ut {
using namespace ubse::context;
using namespace ubse::urma;

namespace {
uint32_t g_sharingQueryCount = 0;

int SharingEnabled(bool* enabled)
{
    ++g_sharingQueryCount;
    *enabled = true;
    return 0;
}

int SharingQueryFails(bool* enabled)
{
    ++g_sharingQueryCount;
    *enabled = true;
    return -1;
}

void* ResolveSymbolsWithSharing(void*, const char* symbol)
{
    if (std::string(symbol) == "uvs_get_eid_sharing") {
        return reinterpret_cast<void*>(&SharingEnabled);
    }
    if (std::string(symbol) == "uvs_get_eid_sharing_mode") {
        return nullptr;
    }
    return reinterpret_cast<void*>(0x5678);
}

void* ResolveSymbolsWithoutSharing(void*, const char* symbol)
{
    if (std::string(symbol) == "uvs_get_eid_sharing" || std::string(symbol) == "uvs_get_eid_sharing_mode") {
        return nullptr;
    }
    return reinterpret_cast<void*>(0x5678);
}

void* ResolveSymbolsWithSharingFailure(void*, const char* symbol)
{
    if (std::string(symbol) == "uvs_get_eid_sharing") {
        return reinterpret_cast<void*>(&SharingQueryFails);
    }
    if (std::string(symbol) == "uvs_get_eid_sharing_mode") {
        return nullptr;
    }
    return reinterpret_cast<void*>(0x5678);
}
} // namespace

void TestUrmaUvsModule::SetUp()
{
    Test::SetUp();
}

void TestUrmaUvsModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUrmaUvsModule, Initialize_Fail)
{
    UbseUrmaUvsModule module;
    void* mockHandle = nullptr;
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));

    EXPECT_EQ(module.Initialize(), UBSE_ERROR_FILE_NOT_EXIST);
}

TEST_F(TestUrmaUvsModule, Initialize_Success)
{
    UbseUrmaUvsModule module;
    void* mockHandle = reinterpret_cast<void*>(0x1234);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlsym).stubs().will(invoke(ResolveSymbolsWithSharing));

    EXPECT_EQ(module.Initialize(), UBSE_OK);

    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));
    module.UnInitialize();
}

TEST_F(TestUrmaUvsModule, InitializeReadsSharingModeOnce)
{
    UbseUrmaUvsModule module;
    g_sharingQueryCount = 0;
    MOCKER_CPP(&dlopen).stubs().will(returnValue(reinterpret_cast<void*>(0x1234)));
    MOCKER_CPP(&dlsym).stubs().will(invoke(ResolveSymbolsWithSharing));

    EXPECT_EQ(module.Initialize(), UBSE_OK);
    EXPECT_TRUE(module.IsEidSharingModeEnabled());
    EXPECT_TRUE(module.IsEidSharingModeEnabled());
    EXPECT_EQ(g_sharingQueryCount, 1);

    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));
    module.UnInitialize();
}

TEST_F(TestUrmaUvsModule, MissingSharingSymbolMeansDisabled)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&dlopen).stubs().will(returnValue(reinterpret_cast<void*>(0x1234)));
    MOCKER_CPP(&dlsym).stubs().will(invoke(ResolveSymbolsWithoutSharing));

    EXPECT_EQ(module.Initialize(), UBSE_OK);
    EXPECT_FALSE(module.IsEidSharingModeEnabled());

    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));
    module.UnInitialize();
}

TEST_F(TestUrmaUvsModule, SharingQueryFailureMeansDisabled)
{
    UbseUrmaUvsModule module;
    g_sharingQueryCount = 0;
    MOCKER_CPP(&dlopen).stubs().will(returnValue(reinterpret_cast<void*>(0x1234)));
    MOCKER_CPP(&dlsym).stubs().will(invoke(ResolveSymbolsWithSharingFailure));

    EXPECT_EQ(module.Initialize(), UBSE_OK);
    EXPECT_FALSE(module.IsEidSharingModeEnabled());
    EXPECT_EQ(g_sharingQueryCount, 1);

    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));
    module.UnInitialize();
}

} // namespace ubse::urma::ut
