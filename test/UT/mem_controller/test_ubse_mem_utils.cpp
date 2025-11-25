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

#include "test_ubse_mem_utils.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_mem_utils.h"

namespace ubse::mem_controller::ut {
using namespace resource::mem;
using namespace mem::utils;

void TestUbseMemUtils::SetUp()
{
    Test::SetUp();
}
void TestUbseMemUtils::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemUtils, GetExecutor)
{
    std::shared_ptr<UbseTaskExecutorModule> nullModule = nullptr;
    std::shared_ptr<UbseTaskExecutorModule> module =
        std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    std::string name = "name";
    EXPECT_EQ(mem::utils::GetExecutor(name), nullptr);

    auto nullPtr = UbseTaskExecutor::Create(name, 0, 1000);
    auto ubseTaskExecutorPtr = UbseTaskExecutor::Create(name, 1, 1000);
    MOCKER_CPP(&task_executor::UbseTaskExecutorModule::Get)
        .stubs()
        .will(returnValue(nullPtr))
        .then(returnValue(ubseTaskExecutorPtr));
    EXPECT_EQ(mem::utils::GetExecutor(name), nullptr);
    EXPECT_EQ(mem::utils::GetExecutor(name), ubseTaskExecutorPtr);
}

TEST_F(TestUbseMemUtils, GetCurNodeId)
{
    std::string nodeId = "1";
    election::UbseRoleInfo currentRoleInfo{};
    currentRoleInfo.nodeId = nodeId;
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mem::utils::GetCurNodeId(), nodeId);
}

TEST_F(TestUbseMemUtils, FilterFdAndNumaObjs)
{
    NodeMemDebtInfoMap data{};
    NodeMemDebtInfo  nodeMemDebtInfo{};

    UbseMemFdBorrowExportObj ubseMemFdBorrowExportObj{};
    ubseMemFdBorrowExportObj.req.name = "name";
    ubseMemFdBorrowExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    nodeMemDebtInfo.fdExportObjMap["resourceId"] = ubseMemFdBorrowExportObj;

    UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj{};
    ubseMemFdBorrowImportObj.req.name = "name";
    ubseMemFdBorrowImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    nodeMemDebtInfo.fdImportObjMap["resource"] = ubseMemFdBorrowImportObj;

    UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj{};
    ubseMemNumaBorrowExportObj.req.name = "name";
    ubseMemNumaBorrowExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    nodeMemDebtInfo.numaExportObjMap["resource"] = ubseMemNumaBorrowExportObj;

    UbseMemNumaBorrowImportObj ubseMemNumaBorrowImportObj{};
    ubseMemNumaBorrowImportObj.req.name = "name";
    ubseMemNumaBorrowImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    nodeMemDebtInfo.numaImportObjMap["resource"] = ubseMemNumaBorrowImportObj;

    data["1"] = nodeMemDebtInfo;
    NodeMemDebtInfoMap filterData{};
    EXPECT_NO_THROW(FilterFdAndNumaObjs(filterData, data));
}
}
