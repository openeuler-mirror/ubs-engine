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
#include "ubse_mem_account_helper.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_controller.h"
#include "ubse_node_topology.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;

void TestUbseAccountHelper::SetUp()
{
    Test::SetUp();
}
void TestUbseAccountHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseAccountHelper, DeleteStrategyNumaLendInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);
    UbseMemStrategyHelper::GetInstance().mNumaLendOutCount["21"] = 128;

    UbseMemAccountHelper::GetInstance().DeleteStrategyNumaLendInfo(algoAccount);
    auto ret = UbseMemStrategyHelper::GetInstance().mNumaLendOutCount;
    EXPECT_EQ(ret["21"], 0);
}

TEST_F(TestUbseAccountHelper, DeleteStrategyNumaSharedInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);
    UbseMemStrategyHelper::GetInstance().mNumaShareOutCount["21"] = 128;

    UbseMemAccountHelper::GetInstance().DeleteStrategyNumaSharedInfo(algoAccount);
    auto ret = UbseMemStrategyHelper::GetInstance().mNumaLendOutCount;
    EXPECT_EQ(ret["21"], 0);
}

TEST_F(TestUbseAccountHelper, AddNumaLentInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc ubseMemNumaLoc{"2", 36, 1};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 2;
    std::shared_ptr<MemNumaInfo> numaInfo =
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).stubs().will(returnValue(numaInfo));

    UbseMemAccountHelper::GetInstance().AddNumaLentInfo(algoAccount);
    EXPECT_EQ(numaInfo->mMemLent, 128);
}

TEST_F(TestUbseAccountHelper, AddNumaBorrowInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc ubseMemNumaLoc{"2", 36, 1};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 2;
    std::shared_ptr<MemNumaInfo> numaInfo =
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).stubs().will(returnValue(numaInfo));

    UbseMemAccountHelper::GetInstance().AddNumaBorrowInfo(algoAccount);
    EXPECT_EQ(numaInfo->mMemBorrowed, 128);
}

TEST_F(TestUbseAccountHelper, AddNumaSharedInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc ubseMemNumaLoc{"2", 36, 1};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 2;
    std::shared_ptr<MemNumaInfo> numaInfo =
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).stubs().will(returnValue(numaInfo));

    UbseMemAccountHelper::GetInstance().AddNumaSharedInfo(algoAccount);
    EXPECT_EQ(numaInfo->mMemShared, 128);
}

TEST_F(TestUbseAccountHelper, DelNumaLentInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc ubseMemNumaLoc{"2", 36, 1};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 2;
    std::shared_ptr<MemNumaInfo> numaInfo =
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex);
    numaInfo->mMemLent = 128;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).stubs().will(returnValue(numaInfo));

    UbseMemAccountHelper::GetInstance().DelNumaLentInfo(algoAccount);
    EXPECT_EQ(numaInfo->mMemLent, 0);
}

TEST_F(TestUbseAccountHelper, DelNumaBorrowInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc ubseMemNumaLoc{"2", 36, 1};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 2;
    std::shared_ptr<MemNumaInfo> numaInfo =
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex);
    numaInfo->mMemBorrowed = 128;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).stubs().will(returnValue(numaInfo));

    UbseMemAccountHelper::GetInstance().DelNumaBorrowInfo(algoAccount);
    EXPECT_EQ(numaInfo->mMemBorrowed, 0);
}

TEST_F(TestUbseAccountHelper, DelNumaSharedInfo)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc ubseMemNumaLoc{"2", 36, 1};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 2;
    std::shared_ptr<MemNumaInfo> numaInfo =
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex);
    numaInfo->mMemShared = 128;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo).stubs().will(returnValue(numaInfo));

    UbseMemAccountHelper::GetInstance().DelNumaSharedInfo(algoAccount);
    EXPECT_EQ(numaInfo->mMemShared, 0);
}

TEST_F(TestUbseAccountHelper, UpdateBorrowDebtDetailAdd)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc borrowLoc{"0", 36, 1};
    UbseMemNumaIndexLoc borrowNumaIndexLoc;
    GlobalNumaIndex borrowGlobalIndex = 0;
    std::shared_ptr<MemNumaInfo> borrowNumaInfo =
        std::make_shared<MemNumaInfo>(borrowLoc, borrowNumaIndexLoc, borrowGlobalIndex);
    UbseMemNumaLoc lentLoc{"1", 36, 1};
    UbseMemNumaIndexLoc lentNumaIndexLoc;
    GlobalNumaIndex lentGlobalIndex = 0;
    std::shared_ptr<MemNumaInfo> lentNumaInfo =
        std::make_shared<MemNumaInfo>(borrowLoc, lentNumaIndexLoc, lentGlobalIndex);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(borrowNumaInfo))
        .then(returnValue(lentNumaInfo));

    auto ret = UbseMemAccountHelper::GetInstance().UpdateBorrowDebtDetail(algoAccount, true);
    auto temp = UbseMemStrategyHelper::GetInstance().debtDetail.numaDebts;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(temp[0][0], 128);
}

TEST_F(TestUbseAccountHelper, UpdateBorrowDebtDetailSub)
{
    AlgoAccount algoAccount;
    algoAccount.name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    algoAccount.importNumaLocs.push_back(importNumaInfo);
    algoAccount.exportNumaLocs.push_back(exportNumaInfo);

    UbseMemNumaLoc borrowLoc{"0", 36, 1};
    UbseMemNumaIndexLoc borrowNumaIndexLoc;
    GlobalNumaIndex borrowGlobalIndex = 0;
    std::shared_ptr<MemNumaInfo> borrowNumaInfo =
        std::make_shared<MemNumaInfo>(borrowLoc, borrowNumaIndexLoc, borrowGlobalIndex);
    UbseMemNumaLoc lentLoc{"1", 36, 1};
    UbseMemNumaIndexLoc lentNumaIndexLoc;
    GlobalNumaIndex lentGlobalIndex = 0;
    UbseMemStrategyHelper::GetInstance().debtDetail.numaDebts[0][0] = 128;
    std::shared_ptr<MemNumaInfo> lentNumaInfo =
        std::make_shared<MemNumaInfo>(borrowLoc, lentNumaIndexLoc, lentGlobalIndex);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(borrowNumaInfo))
        .then(returnValue(lentNumaInfo));

    auto ret = UbseMemAccountHelper::GetInstance().UpdateBorrowDebtDetail(algoAccount, false);
    auto temp = UbseMemStrategyHelper::GetInstance().debtDetail.numaDebts;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(temp[0][0], 0);
}

TEST_F(TestUbseAccountHelper, BorrowSuccess)
{
    std::string name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> importNumaInfos;
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> exportNumaInfos;
    UbseMemStrategyHelper::GetInstance().mNumaLendOutCount["21"] = 128;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetMemNumaLoc).stubs().will(returnValue(UBSE_OK));

    std::shared_ptr<AlgoAccount> algoAccountPtr =
        std::make_shared<AlgoAccount>(name, importNumaInfos, exportNumaInfos, 36, 36, 128);
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);
    MOCKER_CPP(&UbseMemStrategyHelper::AddSocketCnaSize).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseMemStrategyHelper::SubPreSocketCnaSize).stubs().will(returnValue(UBSE_OK));

    UbseMemAccountHelper::GetInstance().BorrowSuccess(name);

    for (auto &debtNumaInfo : algoAccountPtr->exportNumaLocs) {
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        EXPECT_EQ(numaInfo->mMemLent, 128);
    }
    for (auto &debtNumaInfo : algoAccountPtr->exportNumaLocs) {
        auto numaInfo = UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(debtNumaInfo.nodeId, debtNumaInfo.numaId);
        if (numaInfo == nullptr) {
            return;
        }
        EXPECT_EQ(numaInfo->mMemBorrowed, 128);
    }
    auto temp = UbseMemStrategyHelper::GetInstance().debtDetail.numaDebts;
    EXPECT_EQ(temp[0][0], 0);
}

TEST_F(TestUbseAccountHelper, ShareSuccess)
{
    std::shared_ptr<AlgoAccount> algoAccountPtr = std::make_shared<AlgoAccount>();
    algoAccountPtr->name = "test";
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);

    std::string name = "test";
    UbseMemAccountHelper::GetInstance().ShareSuccess(name);
    EXPECT_NE(AlgoAccountManger::GetInstance().GetAlgoAccount(name), nullptr);
}

TEST_F(TestUbseAccountHelper, BorrowReturnSuccess)
{
    std::string name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> importNumaInfos{importNumaInfo};
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> exportNumaInfos{exportNumaInfo};

    std::shared_ptr<AlgoAccount> algoAccountPtr =
        std::make_shared<AlgoAccount>(name, importNumaInfos, exportNumaInfos, 36, 36, 128);
    MOCKER_CPP(&UbseMemStrategyHelper::SubPreSocketCnaSize).stubs().will(returnValue(UBSE_OK));
    UbseMemStrategyHelper::GetInstance().debtDetail.numaDebts[0][0] = 128;

    UbseMemNumaLoc borrowLoc{"0", 36, 1};
    UbseMemNumaIndexLoc borrowNumaIndexLoc;
    GlobalNumaIndex borrowGlobalIndex = 0;
    std::shared_ptr<MemNumaInfo> borrowNumaInfo =
        std::make_shared<MemNumaInfo>(borrowLoc, borrowNumaIndexLoc, borrowGlobalIndex);
    borrowNumaInfo->mMemBorrowed = 128;
    UbseMemNumaLoc lentLoc{"1", 36, 1};
    UbseMemNumaIndexLoc lentNumaIndexLoc;
    GlobalNumaIndex lentGlobalIndex = 0;
    std::shared_ptr<MemNumaInfo> lentNumaInfo =
        std::make_shared<MemNumaInfo>(borrowLoc, lentNumaIndexLoc, lentGlobalIndex);
    lentNumaInfo->mMemLent = 128;
    MOCKER(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(lentNumaInfo))
        .then(returnValue(borrowNumaInfo))
        .then(returnValue(borrowNumaInfo));

    UbseMemAccountHelper::GetInstance().BorrowReturnSuccess(algoAccountPtr);
    auto temp = UbseMemStrategyHelper::GetInstance().debtDetail.numaDebts;
    EXPECT_EQ(borrowNumaInfo->mMemBorrowed, 0);
    EXPECT_EQ(lentNumaInfo->mMemLent, 0);
    EXPECT_EQ(temp[0][0], 0);
}

TEST_F(TestUbseAccountHelper, BorrowFailed)
{
    std::string name = "account";
    // 借入借出关系
    ubse::mem::obj::UbseMemDebtNumaInfo importNumaInfo{"1", 36, 1, 128};
    ubse::mem::obj::UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 1, 128};
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> importNumaInfos;
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> exportNumaInfos;

    std::shared_ptr<AlgoAccount> algoAccountPtr =
        std::make_shared<AlgoAccount>(name, importNumaInfos, exportNumaInfos, 36, 36, 128);
    AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);
    MOCKER_CPP(&UbseMemStrategyHelper::SubPreSocketCnaSize).stubs().will(returnValue(UBSE_OK));

    UbseMemStrategyHelper::GetInstance().mNumaLendOutCount["21"] = 128;
    UbseMemAccountHelper::GetInstance().BorrowFailed(name);

    auto ret = AlgoAccountManger::GetInstance().GetAlgoAccount(name);
    EXPECT_EQ(ret, nullptr);
}
}  // namespace ubse::mem_scheduler::ut
