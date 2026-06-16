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

#include "test_ubse_dl_manager.h"

#include <dlfcn.h>

#include "framework/misc/ubse_dl_manager.h"
#include "ubse_error.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

namespace {
const std::string TEST_LIB_PATH = "/usr/lib64/libtest.so";
int g_testLibHandle = 0;
auto g_nullChar = static_cast<char *>(nullptr);
char g_dlerrorMsg[] = "symbol not found";

using TestFunc = int (*)();

int MockTestFunc()
{
    return UBSE_OK;
}
} // namespace

void TestUbseDlManager::SetUp()
{
    Test::SetUp();
}

void TestUbseDlManager::TearDown()
{
    MOCKER_CPP(&dlopen).reset();
    MOCKER_CPP(&dlclose).reset();
    MOCKER_CPP(&dlsym).reset();
    MOCKER_CPP(dlerror).reset();
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseDlManager, OpenReturnErrorWhenLibPathEmpty)
{
    UbseDlManager manager("");

    EXPECT_EQ(manager.Open(), UBSE_ERROR);
    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, OpenReturnErrorWhenDlopenFailed)
{
    UbseDlManager manager(TEST_LIB_PATH);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(static_cast<void *>(nullptr)));

    EXPECT_EQ(manager.Open(), UBSE_ERROR);
    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, OpenReturnOkAndKeepOpenedWhenDlopenSucceed)
{
    UbseDlManager manager(TEST_LIB_PATH);
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).expects(once()).will(returnValue(mockHandle));

    EXPECT_EQ(manager.Open(), UBSE_OK);
    EXPECT_TRUE(manager.IsOpen());
    EXPECT_EQ(manager.Open(), UBSE_OK);

    MOCKER_CPP(&dlclose).expects(once()).will(returnValue(0));
    manager.Close();
    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, CloseCanBeCalledRepeatedly)
{
    UbseDlManager manager(TEST_LIB_PATH);
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlclose).expects(once()).will(returnValue(0));

    EXPECT_EQ(manager.Open(), UBSE_OK);
    manager.Close();
    manager.Close();

    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, CloseBeforeOpenKeepsManagerClosed)
{
    UbseDlManager manager(TEST_LIB_PATH);

    manager.Close();
    manager.Close();

    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, OpenCanBeCalledAgainAfterClose)
{
    UbseDlManager manager(TEST_LIB_PATH);
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).expects(exactly(2)).will(returnValue(mockHandle));
    MOCKER_CPP(&dlclose).expects(exactly(2)).will(returnValue(0));

    EXPECT_EQ(manager.Open(), UBSE_OK);
    EXPECT_TRUE(manager.IsOpen());
    manager.Close();
    EXPECT_FALSE(manager.IsOpen());

    EXPECT_EQ(manager.Open(), UBSE_OK);
    EXPECT_TRUE(manager.IsOpen());
    manager.Close();
    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, OpenCanRetryAfterDlopenFailed)
{
    UbseDlManager manager(TEST_LIB_PATH);
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).expects(exactly(2)).will(returnObjectList(static_cast<void *>(nullptr), mockHandle));
    MOCKER_CPP(&dlclose).expects(once()).will(returnValue(0));

    EXPECT_EQ(manager.Open(), UBSE_ERROR);
    EXPECT_FALSE(manager.IsOpen());

    EXPECT_EQ(manager.Open(), UBSE_OK);
    EXPECT_TRUE(manager.IsOpen());
    manager.Close();
    EXPECT_FALSE(manager.IsOpen());
}

TEST_F(TestUbseDlManager, DestructorCloseOpenedHandle)
{
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlclose).expects(once()).will(returnValue(0));

    {
        UbseDlManager manager(TEST_LIB_PATH);
        EXPECT_EQ(manager.Open(), UBSE_OK);
        EXPECT_TRUE(manager.IsOpen());
    }
}

TEST_F(TestUbseDlManager, GetFunctionReturnErrorWhenManagerNotOpen)
{
    UbseDlManager manager(TEST_LIB_PATH);
    TestFunc func = nullptr;

    EXPECT_EQ(manager.GetFunction(func, "MockTestFunc"), UBSE_ERROR);
    EXPECT_EQ(func, nullptr);
}

TEST_F(TestUbseDlManager, GetFunctionReturnOkWhenDlsymSucceed)
{
    UbseDlManager manager(TEST_LIB_PATH);
    TestFunc func = nullptr;
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlsym).stubs().will(returnValue(reinterpret_cast<void *>(&MockTestFunc)));
    MOCKER_CPP(dlerror).stubs().will(returnValue(g_nullChar));
    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));

    EXPECT_EQ(manager.Open(), UBSE_OK);
    EXPECT_EQ(manager.GetFunction(func, "MockTestFunc"), UBSE_OK);
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func(), UBSE_OK);
}

TEST_F(TestUbseDlManager, GetFunctionReturnErrorWhenDlsymFailed)
{
    UbseDlManager manager(TEST_LIB_PATH);
    TestFunc func = reinterpret_cast<TestFunc>(&MockTestFunc);
    auto *mockHandle = static_cast<void *>(&g_testLibHandle);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlsym).stubs().will(returnValue(static_cast<void *>(nullptr)));
    MOCKER_CPP(dlerror).stubs().will(returnObjectList(g_nullChar, g_nullChar, static_cast<char *>(g_dlerrorMsg)));
    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));

    EXPECT_EQ(manager.Open(), UBSE_OK);
    EXPECT_EQ(manager.GetFunction(func, "NotExistFunc"), UBSE_ERROR);
    EXPECT_EQ(func, nullptr);
}
} // namespace ubse::ut::utils
