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

#include "test_ubse_mem_controller_query_api.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_query_api.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;

void TestUbseMemControllerQueryApi::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerQueryApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemFdGet)
{
    std::string name(50, 'n');
    mem::def::UbseMemFdDesc fdDesc;
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), -1);

    name = "name";
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_ERROR);

    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().will(returnValue(0));
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_ERROR_NULLPTR);

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].fdImportObjMap[name] = UbseMemFdBorrowImportObj{};
    debtInfoMap["1"].fdImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    MOCKER_CPP(&GetNdeoMemDebtInfoMap).stubs().with(any(), outBound(debtInfoMap)).will(returnValue(0));
    EXPECT_EQ(UbseMemFdGet(name, fdDesc), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemFdList)
{
    std::vector<mem::def::UbseMemFdDesc> fdDescs{};
    EXPECT_EQ(UbseMemFdList(fdDescs), UBSE_ERROR);

    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().will(returnValue(0));
    EXPECT_EQ(UbseMemFdList(fdDescs), UBSE_ERROR_NULLPTR);

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].fdImportObjMap["name"] = UbseMemFdBorrowImportObj{};
    debtInfoMap["1"].fdImportObjMap["name"].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    MOCKER_CPP(&GetNdeoMemDebtInfoMap).stubs().with(any(), outBound(debtInfoMap)).will(returnValue(0));
    EXPECT_EQ(UbseMemFdList(fdDescs), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNumaGet)
{
    std::string name = "name";
    mem::def::UbseMemNumaDesc numaDesc;
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_ERROR);

    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().will(returnValue(0));
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_ERROR_NULLPTR);

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].numaImportObjMap[name] = UbseMemNumaBorrowImportObj{};
    debtInfoMap["1"].numaImportObjMap[name].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    MOCKER_CPP(&GetNdeoMemDebtInfoMap).stubs().with(any(), outBound(debtInfoMap)).will(returnValue(0));
    EXPECT_EQ(UbseMemNumaGet(name, numaDesc), UBSE_OK);
}

TEST_F(TestUbseMemControllerQueryApi, UbseMemNumaList)
{
    std::vector<mem::def::UbseMemNumaDesc> numaDescs{};
    EXPECT_EQ(UbseMemNumaList(numaDescs), UBSE_ERROR);

    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().will(returnValue(0));
    EXPECT_EQ(UbseMemNumaList(numaDescs), UBSE_ERROR_NULLPTR);

    NodeMemDebtInfoMap debtInfoMap{};
    debtInfoMap["1"] = NodeMemDebtInfo{};
    debtInfoMap["1"].numaImportObjMap["name"] = UbseMemNumaBorrowImportObj{};
    debtInfoMap["1"].numaImportObjMap["name"].status.importResults.emplace_back(UbseMemImportResult{});
    debtInfoMap["2"] = NodeMemDebtInfo{};
    MOCKER_CPP(&GetNdeoMemDebtInfoMap).stubs().with(any(), outBound(debtInfoMap)).will(returnValue(0));
    EXPECT_EQ(UbseMemNumaList(numaDescs), UBSE_OK);
}
}