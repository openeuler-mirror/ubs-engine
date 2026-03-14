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

#include "test_ubse_mem_borrow_account.h"
#include "ubse_borrow_account.h"
#include "ubse_mem_account_helper.h"
#include "ubse_mmi_def.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::account;
using namespace ubse::mem::strategy;

void TestMemBorrowAccount::SetUp()
{
    Test::SetUp();
}
void TestMemBorrowAccount::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMemBorrowAccount, TestUpdateStateByImportExistExportSuccess)
{
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo1{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo1{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos1{importNumaInfo1};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos1{exportNumaInfo1};
    ubse::adapter_plugins::mmi::UbseMemAlgoResult algoResult;
    algoResult.importNumaInfos = importNumaInfos1;
    algoResult.exportNumaInfos = exportNumaInfos1;
    int64_t attachSocketId = 36;
    int64_t exportSocketId = 36;
    uint64_t totalBorrowSize = 128 * 1024 * 1024;
    uint64_t blockSize = 128;
    std::shared_ptr<BorrowAccount> algoAccountPtr1 = std::make_shared<BorrowAccount>(
        importNumaInfos1, exportNumaInfos1, attachSocketId, exportSocketId, totalBorrowSize, blockSize);
    UbseMemState state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_NO_THROW(algoAccountPtr1->UpdateStateByImportExist(state));
}

TEST_F(TestMemBorrowAccount, TestUpdateStateByImportExistImportSuccess)
{
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo1{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo1{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos1{importNumaInfo1};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos1{exportNumaInfo1};
    ubse::adapter_plugins::mmi::UbseMemAlgoResult algoResult;
    algoResult.importNumaInfos = importNumaInfos1;
    algoResult.exportNumaInfos = exportNumaInfos1;
    int64_t attachSocketId = 36;
    int64_t exportSocketId = 36;
    uint64_t totalBorrowSize = 128 * 1024 * 1024;
    uint64_t blockSize = 128;
    std::shared_ptr<BorrowAccount> algoAccountPtr1 = std::make_shared<BorrowAccount>(
        importNumaInfos1, exportNumaInfos1, attachSocketId, exportSocketId, totalBorrowSize, blockSize);
    UbseMemState state = UBSE_MEM_IMPORT_DESTROYED;
    EXPECT_NO_THROW(algoAccountPtr1->UpdateStateByImportExist(state));
}

TEST_F(TestMemBorrowAccount, TestUpdateStateByExportExistImportSuccess)
{
    // 借入借出关系
    UbseMemDebtNumaInfo importNumaInfo1{"1", 36, 1, 128};
    UbseMemDebtNumaInfo exportNumaInfo1{"2", 36, 1, 128};
    std::vector<UbseMemDebtNumaInfo> importNumaInfos1{importNumaInfo1};
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos1{exportNumaInfo1};
    ubse::adapter_plugins::mmi::UbseMemAlgoResult algoResult;
    algoResult.importNumaInfos = importNumaInfos1;
    algoResult.exportNumaInfos = exportNumaInfos1;
    int64_t attachSocketId = 36;
    int64_t exportSocketId = 36;
    uint64_t totalBorrowSize = 128 * 1024 * 1024;
    uint64_t blockSize = 128;
    std::shared_ptr<BorrowAccount> algoAccountPtr1 = std::make_shared<BorrowAccount>(
        importNumaInfos1, exportNumaInfos1, attachSocketId, exportSocketId, totalBorrowSize, blockSize);
    UbseMemState state = UBSE_MEM_IMPORT_SUCCESS;
    EXPECT_NO_THROW(algoAccountPtr1->UpdateStateByExportExist(state));
}

} // namespace ubse::mem_scheduler::ut
