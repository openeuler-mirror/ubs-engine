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

#include "test_ubse_cli_whitelist.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_whitelist.h"

namespace ubse::ut::cli {
using namespace ubse::cli::framework;
void TestUbseCliWhitelist::SetUp() {}

void TestUbseCliWhitelist::TearDown() {}

TEST_F(TestUbseCliWhitelist, ClearWhiteList)
{
    UbseCliWhitelist wtl;
    wtl.UbseCliClearWhitelist();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyz\"\n"
        "            \"ABCDEFGHIJKLMNOPQRSTUVWXYZ\"\n"
        "            \"0123456789\"\n"
        "            \"-._"));
    EXPECT_TRUE("");
}
TEST_F(TestUbseCliWhitelist, DefaultWhiteList)
{
    UbseCliWhitelist wtl;
    EXPECT_TRUE(wtl.UbseCliIsAllowed("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    EXPECT_TRUE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyz"));
    EXPECT_TRUE(wtl.UbseCliIsAllowed("0123456789"));
    EXPECT_FALSE(wtl.UbseCliIsAllowed("!@#$%^&*()_+="));
}
TEST_F(TestUbseCliWhitelist, AddCharToWhiteList)
{
    UbseCliWhitelist wtl;
    wtl.UbseCliClearWhitelist();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("*"));
    wtl.UbseCliAddChar('*');
    EXPECT_TRUE(wtl.UbseCliIsAllowed("*"));
}
TEST_F(TestUbseCliWhitelist, AddCharsToWhiteList)
{
    UbseCliWhitelist wtl;
    wtl.UbseCliClearWhitelist();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("!@#$%^&*()_+="));
    wtl.UbseCliAddChars("!@#$%^&*()_+=");
    EXPECT_TRUE(wtl.UbseCliIsAllowed("!@#$%^&*()_+="));
}
TEST_F(TestUbseCliWhitelist, DigitsOnlyWhiteList)
{
    UbseCliWhitelist wtl;
    wtl.UbseCliClearWhitelist();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("0123456789"));
    wtl.UbseCliSetDigitsOnly();
    EXPECT_TRUE(wtl.UbseCliIsAllowed("0123456789"));
}
TEST_F(TestUbseCliWhitelist, LettersOnlyWhiteList)
{
    UbseCliWhitelist wtl;
    wtl.UbseCliClearWhitelist();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    wtl.UbseCliSetLowercaseOnly();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    EXPECT_TRUE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyz"));
    wtl.UbseCliSetUppercaseOnly();
    EXPECT_TRUE(wtl.UbseCliIsAllowed("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    EXPECT_TRUE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyz"));
    wtl.UbseCliClearWhitelist();
    EXPECT_FALSE(wtl.UbseCliIsAllowed("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    EXPECT_FALSE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyz"));
    wtl.UbseCliSetLettersOnly();
    EXPECT_TRUE(wtl.UbseCliIsAllowed("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    EXPECT_TRUE(wtl.UbseCliIsAllowed("abcdefghijklmnopqrstuvwxyz"));
}
} // namespace ubse::ut::cli
