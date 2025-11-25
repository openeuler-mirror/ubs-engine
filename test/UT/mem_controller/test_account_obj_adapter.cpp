/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* ubs-engine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
* http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include "test_account_obj_adapter.h"
#include <mockcpp/mockcpp.hpp>
#include "AccountObjAdapter.h"
namespace ubse::mem_controller::ut {
using namespace ubse::mem::strategy;
void TestAccountObjAdapter::SetUp()
{
    Test::SetUp();
}

void TestAccountObjAdapter::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
TEST_F(TestAccountObjAdapter, GetExportNode)
{
    UbseMemAlgoResult algoResultNotEmpty;
    algoResultNotEmpty.exportNumaInfos.push_back({.nodeId = "1"});
    AccountObjAdapter accountObjAdapter;
    EXPECT_EQ(accountObjAdapter.GetExportNode(algoResultNotEmpty), "1");
    UbseMemAlgoResult algoResultEmpty;
    EXPECT_EQ(accountObjAdapter.GetExportNode(algoResultEmpty), "");
}
TEST_F(TestAccountObjAdapter, GetImportNode)
{
    UbseMemAlgoResult algoResultNotEmpty;
    algoResultNotEmpty.importNumaInfos.push_back({.nodeId = "1"});
    AccountObjAdapter accountObjAdapter;
    EXPECT_EQ(accountObjAdapter.GetImportNode(algoResultNotEmpty), "1");
    UbseMemAlgoResult algoResultEmpty;
    EXPECT_EQ(accountObjAdapter.GetImportNode(algoResultEmpty), "");
}
} // namespace ubse::mem_controller::ut
