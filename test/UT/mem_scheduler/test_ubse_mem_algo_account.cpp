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

#include "test_ubse_mem_algo_account.h"

#include <ubse_borrow_account.h>
#include <ubse_error.h>
#include <ubse_node.h>

#include "ubse_mem_account_helper.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_controller.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;
using namespace ubse::mem::account;
using namespace ubse::adapter_plugins::mmi;

void TestAlgoAccountManager::SetUp()
{
    Test::SetUp();
}
void TestAlgoAccountManager::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestAlgoAccountManager, GetAllAlgoAccountByNode)
{
    AlgoAccountManger::GetInstance().Clear();
    std::string account1 = "account1";
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo1{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo1{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos1{importNumaInfo1};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos1{exportNumaInfo1};
    std::shared_ptr<BaseAlgoAccount> algoAccountPtr1 =
        std::make_shared<AccountImpl<BorrowAccount>>(importNumaInfos1, exportNumaInfos1, 36, 36, 128, 128);
    algoAccountPtr1->name = account1;
    algoAccountPtr1->importNodeId = "1";
    algoAccountPtr1->exportNodeId = "2";
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr1);

    std::string account2 = "account2";
    UbseMemDebtNumaInfo importNumaInfo2{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo2{"3", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos2{importNumaInfo2};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos2{exportNumaInfo2};
    std::shared_ptr<BaseAlgoAccount> algoAccountPtr2 =
        std::make_shared<AccountImpl<BorrowAccount>>(importNumaInfos2, exportNumaInfos2, 36, 36, 128, 128);
    algoAccountPtr2->name = account2;
    algoAccountPtr2->importNodeId = "1";
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr2);

    std::vector<std::shared_ptr<BaseAlgoAccount>> ret = AlgoAccountManger::GetInstance().GetAllAlgoAccountByNode("1");
    size_t expect = 2;
    EXPECT_EQ(ret.size(), expect);
    expect = 1;
    ret = AlgoAccountManger::GetInstance().GetAllAlgoAccountByNode("2");
    EXPECT_EQ(ret.size(), expect);
}

TEST_F(TestAlgoAccountManager, AddAlgoAccountByLcneTopo)
{
    AlgoAccountManger::GetInstance().Clear();
    std::string account1 = "account1";
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo1{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo1{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos1{importNumaInfo1};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos1{exportNumaInfo1};
    std::shared_ptr<BaseAlgoAccount> algoAccountPtr1 =
        std::make_shared<AccountImpl<BorrowAccount>>(importNumaInfos1, exportNumaInfos1, 36, 36, 128, 128);
    algoAccountPtr1->name = account1;
    algoAccountPtr1->importNodeId = "1";
    algoAccountPtr1->type = BorrowedType::NUMA;
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr1);

    std::string account2 = "account2";
    UbseMemDebtNumaInfo importNumaInfo2{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo2{"3", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos2{importNumaInfo2};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos2{exportNumaInfo2};
    std::shared_ptr<BaseAlgoAccount> algoAccountPtr2 =
        std::make_shared<AccountImpl<BorrowAccount>>(importNumaInfos2, exportNumaInfos2, 36, 36, 128, 128);
    algoAccountPtr2->name = account2;
    algoAccountPtr1->importNodeId = "1";
    algoAccountPtr1->type = BorrowedType::NUMA;
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr2);
    UbseNodeMemCnaInfoOutput cnaOutput{36};

    UbseNodeMemCnaInfoOutput nodeMemCnaInfoOutput{1, std::to_string(36), std::to_string(36)};
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo)
        .stubs()
        .with(any(), outBound(nodeMemCnaInfoOutput))
        .will(returnValue(UBSE_OK));

    auto algoAccountPtr = AlgoAccountManger::GetInstance().GetAlgoAccount({account1, "1", BorrowedType::NUMA});
    int64_t expect = 36;
    EXPECT_EQ(algoAccountPtr->GetAlgoResult().exportNumaInfos[0].socketId, expect);
    EXPECT_EQ(algoAccountPtr->GetAlgoResult().attachSocketId, expect);
}

TEST_F(TestAlgoAccountManager, GetAllAlgoAccount)
{
    AlgoAccountManger::GetInstance().Clear();
    std::string name = "account";
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    std::shared_ptr<BaseAlgoAccount> algoAccountPtr =
        std::make_shared<AccountImpl<BorrowAccount>>(importNumaInfos, exportNumaInfos, 36, 36, 128, 128);
    algoAccountPtr->name = name;
    algoAccountPtr->importNodeId = "1";
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);

    std::vector<std::shared_ptr<BaseAlgoAccount>> ret = AlgoAccountManger::GetInstance().GetAllAlgoAccount();
    size_t expect = 1;
    EXPECT_EQ(ret.size(), expect);
}
} // namespace ubse::mem_scheduler::ut
