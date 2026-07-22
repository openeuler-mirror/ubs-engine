/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_debt_info_partial_fetch.h"
#include <limits>
#include <memory>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "debt/ubse_mem_debt_ledger.h"

namespace ubse::mem::controller::debt {
inline bool operator==(const UbseNodeMemDebtInfo&, const UbseNodeMemDebtInfo&)
{
    return true;
}
} // namespace ubse::mem::controller::debt

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller::debt;
using namespace ubse::mem::controller::message;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;

constexpr uint64_t BYTES_PER_MEGABYTE = 1024 * 1024;

void TestUbseMemDebtInfoPartialFetch::SetUp()
{
    Test::SetUp();
}

void TestUbseMemDebtInfoPartialFetch::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_NodeIdEmpty)
{
    DebtFetchInfo info;
    info.nodeId = "";
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_NodeIdTooLong)
{
    DebtFetchInfo info;
    info.nodeId = "12345678901";
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_NodeIdNonDigit)
{
    DebtFetchInfo info;
    info.nodeId = "abc123";
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_PageNumNegative)
{
    DebtFetchInfo info;
    info.nodeId = "1";
    info.pageNum = -1;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_PageSizeNegative)
{
    DebtFetchInfo info;
    info.nodeId = "1";
    info.pageSize = -1;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_PageSizeExceedsMax)
{
    DebtFetchInfo info;
    info.nodeId = "1";
    info.pageSize = EACH_PAGE_SIZE + 1;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_PageOverflow)
{
    DebtFetchInfo info;
    info.nodeId = "1";
    info.pageNum = std::numeric_limits<int>::max();
    info.pageSize = 2;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_InvalidType)
{
    DebtFetchInfo info;
    info.nodeId = "1";
    info.type = static_cast<DebtFetchType>(999);
    info.pageSize = 10;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_InvalidName)
{
    DebtFetchInfo info;
    info.nodeId = "1";
    info.name = "bad name!";
    info.pageSize = 10;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, ValidateDebtFetchInfo_Success)
{
    DebtFetchInfo info;
    info.nodeId = "123";
    info.name = "test_name";
    info.borrowType = AccountType::NUMA;
    info.pageNum = 1;
    info.pageSize = 10;
    info.type = DebtFetchType::EXPORT;
    EXPECT_EQ(ValidateDebtFetchInfo(info), UBSE_OK);
}

static std::shared_ptr<UbseMemNumaBorrowExportObj> MakeNumaExportObj(const std::string& name, int socketId,
                                                                     int64_t numaId, uint64_t sizeMB)
{
    auto obj = std::make_shared<UbseMemNumaBorrowExportObj>();
    obj->req.name = name;
    obj->status.state = UBSE_MEM_IMPORT_RUNNING;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendNode";
    exportInfo.socketId = socketId;
    exportInfo.numaId = numaId;
    exportInfo.size = sizeMB * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    return obj;
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_EmptyNode)
{
    UbseNodeMemDebtInfo emptyInfo;
    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(emptyInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    EXPECT_TRUE(result.flatDebt.empty());
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ExportNuma)
{
    auto obj = MakeNumaExportObj("test_numa", 1, 2, 5);
    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.numaExportObjMap["test_numa"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_numa");
    EXPECT_EQ(result.flatDebt[0].lendId, "lendNode");
    EXPECT_EQ(result.flatDebt[0].importId, "importNode");
    EXPECT_EQ(static_cast<uint32_t>(result.flatDebt[0].status), UBSE_MEM_IMPORT_RUNNING);
    ASSERT_EQ(result.flatDebt[0].numaLendInfos.size(), 1);
    EXPECT_EQ(result.flatDebt[0].numaLendInfos[0].socketId, 1);
    EXPECT_EQ(result.flatDebt[0].numaLendInfos[0].numaId, 2);
    EXPECT_EQ(result.flatDebt[0].numaLendInfos[0].size, 5);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ExportFd)
{
    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    obj->req.name = "test_fd";
    obj->status.state = UBSE_MEM_EXPORT_SUCCESS;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendFdNode";
    exportInfo.socketId = 1;
    exportInfo.numaId = 10;
    exportInfo.size = 2 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importFdNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.fdExportObjMap["test_fd"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::FD;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_fd");
    EXPECT_EQ(result.flatDebt[0].lendId, "lendFdNode");
    EXPECT_EQ(result.flatDebt[0].importId, "importFdNode");
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ExportShm)
{
    auto obj = std::make_shared<UbseMemShareBorrowExportObj>();
    obj->req.name = "test_shm";
    obj->status.state = UBSE_MEM_EXPORT_RUNNING;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendShmNode";
    exportInfo.socketId = 1;
    exportInfo.numaId = 2;
    exportInfo.size = 4 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.shareExportObjMap["test_shm"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::SHM;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_shm");
    EXPECT_TRUE(result.flatDebt[0].importId.empty());
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ExportAddr)
{
    auto obj = std::make_shared<UbseMemAddrBorrowExportObj>();
    obj->req.name = "test_addr";
    obj->status.state = UBSE_MEM_EXPORT_RUNNING;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendAddrNode";
    exportInfo.socketId = 3;
    exportInfo.numaId = 30;
    exportInfo.size = 7 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importAddrNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.addrExportObjMap["test_addr"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::ADDR;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_addr");
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ImportNuma)
{
    auto obj = std::make_shared<UbseMemNumaBorrowImportObj>();
    obj->req.name = "test_import_numa";
    obj->status.state = UBSE_MEM_IMPORT_RUNNING;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendNode";
    exportInfo.socketId = 1;
    exportInfo.numaId = 2;
    exportInfo.size = 3 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    UbseMemImportResult importResult;
    importResult.numaId = 42;
    obj->status.importResults.push_back(importResult);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.numaImportObjMap["test_import_numa"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::IMPORT;
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_import_numa");
    EXPECT_EQ(result.flatDebt[0].handle, "42");
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ImportShm)
{
    auto obj = std::make_shared<UbseMemShareBorrowImportObj>();
    obj->req.name = "test_shm_import";
    obj->status.state = UBSE_MEM_IMPORT_SUCCESS;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendShmNode";
    exportInfo.socketId = 5;
    exportInfo.numaId = 50;
    exportInfo.size = 6 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.shareImportObjMap["test_shm_import"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::IMPORT;
    fetchInfo.borrowType = AccountType::SHM;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_shm_import");
    EXPECT_EQ(result.flatDebt[0].importId, "1");
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ImportFd)
{
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "test_fd_import";
    obj->status.state = UBSE_MEM_IMPORT_RUNNING;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendFdNode";
    exportInfo.socketId = 2;
    exportInfo.numaId = 20;
    exportInfo.size = 8 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importFdNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    UbseMemImportResult importResult;
    importResult.memId = 100;
    obj->status.importResults.push_back(importResult);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.fdImportObjMap["test_fd_import"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::IMPORT;
    fetchInfo.borrowType = AccountType::FD;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_fd_import");
    EXPECT_EQ(result.flatDebt[0].handle, "100");
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_InitBorrowType)
{
    auto obj = MakeNumaExportObj("test_numa", 1, 2, 3);
    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.numaExportObjMap["test_numa"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::INIT;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_NameFilter)
{
    auto obj1 = MakeNumaExportObj("match_name", 1, 2, 3);
    auto obj2 = MakeNumaExportObj("other_name", 3, 4, 5);
    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.numaExportObjMap["match_name"] = obj1;
    nodeInfo.numaExportObjMap["other_name"] = obj2;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.name = "match_name";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "match_name");
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_InvalidType)
{
    UbseNodeMemDebtInfo nodeInfo;
    auto obj = MakeNumaExportObj("test", 1, 2, 3);
    nodeInfo.numaExportObjMap["test"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = static_cast<DebtFetchType>(999);
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_Pagination)
{
    UbseNodeMemDebtInfo nodeInfo;
    for (int i = 0; i < 5; ++i) {
        auto obj = MakeNumaExportObj("obj_" + std::to_string(i), i, i * 10, 1);
        nodeInfo.numaExportObjMap["obj_" + std::to_string(i)] = obj;
    }

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageSize = 2;

    fetchInfo.pageNum = 1;
    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 2);
    EXPECT_TRUE(result.NeedToContinue);

    fetchInfo.pageNum = 2;
    PartialFetchRes result2;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result2), UBSE_OK);
    ASSERT_EQ(result2.flatDebt.size(), 2);
    EXPECT_TRUE(result2.NeedToContinue);

    fetchInfo.pageNum = 3;
    PartialFetchRes result3;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result3), UBSE_OK);
    ASSERT_EQ(result3.flatDebt.size(), 1);
    EXPECT_FALSE(result3.NeedToContinue);
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_StateFilter)
{
    auto obj = std::make_shared<UbseMemNumaBorrowExportObj>();
    obj->req.name = "filtered_state";
    obj->status.state = UBSE_MEM_STATE_INIT;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendNode";
    exportInfo.socketId = 1;
    exportInfo.numaId = 2;
    exportInfo.size = 1 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.numaExportObjMap["filtered_state"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::EXPORT;
    fetchInfo.borrowType = AccountType::NUMA;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    EXPECT_TRUE(result.flatDebt.empty());
}

TEST_F(TestUbseMemDebtInfoPartialFetch, FetchDebtInfoByTypeAndPage_ImportAddr)
{
    auto obj = std::make_shared<UbseMemAddrBorrowImportObj>();
    obj->req.name = "test_addr_import";
    obj->status.state = UBSE_MEM_IMPORT_RUNNING;

    UbseMemDebtNumaInfo exportInfo;
    exportInfo.nodeId = "lendAddrNode";
    exportInfo.socketId = 4;
    exportInfo.numaId = 40;
    exportInfo.size = 9 * BYTES_PER_MEGABYTE;
    obj->algoResult.exportNumaInfos.push_back(exportInfo);

    UbseMemDebtNumaInfo importInfo;
    importInfo.nodeId = "importAddrNode";
    obj->algoResult.importNumaInfos.push_back(importInfo);

    UbseNodeMemDebtInfo nodeInfo;
    nodeInfo.addrImportObjMap["test_addr_import"] = obj;

    MOCKER_CPP(&UbseMemDebtLedger::GetNodeMemDebtInfo).stubs().will(returnValue(nodeInfo));

    DebtFetchInfo fetchInfo;
    fetchInfo.nodeId = "1";
    fetchInfo.type = DebtFetchType::IMPORT;
    fetchInfo.borrowType = AccountType::ADDR;
    fetchInfo.pageNum = 1;
    fetchInfo.pageSize = 10;

    PartialFetchRes result;
    EXPECT_EQ(FetchDebtInfoByTypeAndPage(fetchInfo, result), UBSE_OK);
    ASSERT_EQ(result.flatDebt.size(), 1);
    EXPECT_EQ(result.flatDebt[0].name, "test_addr_import");
}
} // namespace ubse::mem_controller::ut