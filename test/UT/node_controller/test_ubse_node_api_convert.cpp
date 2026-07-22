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

#include "test_ubse_node_api_convert.h"

#include <ubse_pointer_process.h>

#include "ubse_ipc_common.h"
#include "src/framework/ipc/include/ubse_ipc_common.h"
#include "src/framework/ipc/include/ubse_ipc_server.h"
#include "ubse_node_api_convert.cpp"

namespace ubse::node_controller::ut {
using namespace ubse::ipc;

void TestUbseNodeApiConvert::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeApiConvert::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeApiConvert, HtonllCustom)
{
    EXPECT_EQ(HtonllCustom(1), 72057594037927936);
}

TEST_F(TestUbseNodeApiConvert, NtohllCustom)
{
    EXPECT_EQ(NtohllCustom(1), 72057594037927936);
}

TEST_F(TestUbseNodeApiConvert, UbseNodePack_Success)
{
    std::vector<UbseCpuLink> linkList = {UbseCpuLink{}};
    UbseNode ubseNode{};
    UbseIpcMessage message{};
    ubseNode.hostName = std::string(HOST_NAME_MAX + 1, 'a');
    EXPECT_EQ(UbseNodePack(ubseNode, message), UBSE_OK);
    SafeDeleteArray(message.buffer);
}

TEST_F(TestUbseNodeApiConvert, UbseCpuLinkPack)
{
    std::vector<UbseCpuLink> linkList = {UbseCpuLink{}};
    UbseIpcMessage message{};
    auto ret = UbseCpuLinkListPack(linkList, message);
    EXPECT_EQ(ret, UBSE_OK);
    SafeDeleteArray(message.buffer);
}
} // namespace ubse::node_controller::ut