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

#include "test_ubse_mem_account_helper.h"

#include <ubse_borrow_account.h>
#include <ubse_shm_account.h>

#include "ubse_mem_account_helper.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_controller.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;
using namespace ubse::mem::account;
using namespace ubse::adapter_plugins::mmi;

void TestUbseAccountHelper::SetUp()
{
    Test::SetUp();
}
void TestUbseAccountHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseAccountHelper, BorrowSuccess)
{
    std::string name = "account";
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos;
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetMemNumaLoc).stubs().will(returnValue(UBSE_OK));

    std::shared_ptr<BaseAlgoAccount> algoAccountPtr =
        std::make_shared<AccountImpl<BorrowAccount>>(importNumaInfos, exportNumaInfos, 36, 36, 128, 128);
    algoAccountPtr->name = name;
    algoAccountPtr->importNodeId = "1";
    algoAccountPtr->type = BorrowedType::NUMA;
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);
    MOCKER_CPP(&UbseMemStrategyHelper::AddSocketCnaSize).stubs().will(returnValue(UBSE_OK));

    for (auto &debtNumaInfo : algoAccountPtr->GetAlgoResult().exportNumaInfos) {
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        EXPECT_EQ(numaInfo->mMemLent, 128);
    }
    for (auto &debtNumaInfo : algoAccountPtr->GetAlgoResult().exportNumaInfos) {
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        EXPECT_EQ(numaInfo->mMemBorrowed, 128);
    }
    auto temp = UbseMemStrategyHelper::GetInstance().debtDetail_.numaDebts;
    EXPECT_EQ(temp[0][0], 0);
}

TEST_F(TestUbseAccountHelper, ShareSuccess)
{
    AlgoAccountID accountId{"test", "1", BorrowedType::SHM};
    std::shared_ptr<BaseAlgoAccount> algoAccountPtr = std::make_shared<AccountImpl<ShareAlgoAccount>>();
    algoAccountPtr->name = accountId.requestName;
    algoAccountPtr->importNodeId = accountId.nodeId;
    algoAccountPtr->type = accountId.type;
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);

    EXPECT_NE(AlgoAccountManger::GetInstance().GetAlgoAccount(accountId), nullptr);
}
}  // namespace ubse::mem_scheduler::ut
