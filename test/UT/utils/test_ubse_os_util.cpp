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

#include "test_ubse_os_util.h"

#include <thread>

#include "ubse_error.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

void TestUbseOsUtil::SetUp()
{
    Test::SetUp();
}
void TestUbseOsUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseOsUtil, testGetUidByName)
{
    std::string username = "root";
    uid_t uid{};
    auto ret = UbseOsUtil::GetUidByName(username, uid);
    EXPECT_EQ(ret, UBSE_OK);
}

// 并发测试GetUserNameById接口
TEST_F(TestUbseOsUtil, GetUserNameById_MultiUidConcurrency)
{
    std::vector<std::thread> threads;

    auto worker = [](uid_t uid, const std::string &expectUsername) {
        for (int i = 0; i < 100000; ++i) {
            std::string userName;
            EXPECT_EQ(UbseOsUtil::GetUserNameById(uid, userName), UBSE_OK);
            EXPECT_STREQ(userName.c_str(), expectUsername.c_str());
        }
    };

    for (const auto &[uid, username] : users) {
        threads.emplace_back(worker, uid, username);
    }
    for (auto &t : threads) {
        t.join();
    }
}

TEST_F(TestUbseOsUtil, GetUidByName_MultiUserConcurrency)
{
    std::vector<std::thread> threads;

    auto worker = [](uid_t expectUid, const std::string &username) {
        for (int i = 0; i < 100000; ++i) {
            uid_t uid;
            EXPECT_EQ(UbseOsUtil::GetUidByName(username, uid), UBSE_OK);
            EXPECT_EQ(expectUid, uid);
        }
    };

    for (const auto &[uid, username] : users) {
        threads.emplace_back(worker, uid, username);
    }

    for (auto &t : threads) {
        t.join();
    }
}
} // namespace ubse::ut::utils