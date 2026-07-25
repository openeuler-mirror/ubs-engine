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

#include "test_ubse_cli_mem_cmd_reg.h"
#include <securec.h>
#include <memory>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_mem_cmd_reg.h"
#include "ubse_cli_mem_query.h"
#include "ubse_cli_mem_struct.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_debt_info_partial_fetch_req.h"
#include "ubse_mem_debt_info_partial_fetch_res.h"
#include "ubse_mmi_def.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::cli::reg {
using namespace ubse::adapter_plugins::mmi;
struct ParsedResponse {
    bool success;
    std::string name;
    ubse::mem::controller::UbseMemStage stage;
    int64_t numaId;
    std::string importNode;
    std::string exportNode;
    std::string errorMsg;
};
ParsedResponse ParseResponseBuffer(const ubse_api_buffer_t& responseBuffer);
std::shared_ptr<UbseCliResultEcho> HandleTimeoutRetry(const std::string& name);
bool LinkIsMatch(const std::string& str);
bool SizeIsMatch(const std::string& str, size_t& size);
std::string FormatHostnameSlot(const std::string& hostname, uint32_t slotId);
bool CheckBorrowDetailType(const std::string& type);
bool CheckDeleteType(const std::string& type);
void EnsureDotAtEnd(std::string& str);
bool IsValidIntegerString(const std::string& s);
bool SizeConversion(const std::string& str, uint64_t& size);
bool ParseRegionString(const std::string& regionStr, std::vector<uint32_t>& regions);

class UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl {
public:
    bool UbseCliGetIdsWithHostName(std::unordered_map<std::string, std::string>& node_id_with_hostname);
    bool UbseCliGetAllDebtInfo(std::unordered_map<std::string, std::string>& node_id_with_hostname,
                               std::vector<mem::controller::message::FlatDebtInformation>& borrow_account,
                               std::vector<mem::controller::message::FlatDebtInformation>& lend_account,
                               const Filter& filter);
    void ProcessLendAccount(const std::vector<mem::controller::message::FlatDebtInformation>& lend_account,
                            std::unordered_map<std::string, UbseBorrowDetailInfo>& ubse_borrow_details,
                            std::unordered_map<std::string, UbseShmAttachRecord>& shm_export_account);
    void ProcessBorrowAccount(const std::vector<mem::controller::message::FlatDebtInformation>& borrow_account,
                              std::unordered_map<std::string, UbseBorrowDetailInfo>& ubse_borrow_details,
                              std::unordered_map<std::string, UbseShmAttachRecord>& shm_export_account);

private:
    std::vector<mem::controller::message::DebtFetchType> allDebtFetchTypes = {
        mem::controller::message::DebtFetchType::EXPORT, mem::controller::message::DebtFetchType::IMPORT};
    std::unordered_map<UbseMemState, std::string> UbseMemStateMap = {
        {UBSE_MEM_EXPORT_RUNNING, "exporting"},      {UBSE_MEM_EXPORT_SUCCESS, "single"},
        {UBSE_MEM_EXPORT_DESTROYING, "unexporting"}, {UBSE_MEM_IMPORT_RUNNING, "importing"},
        {UBSE_MEM_IMPORT_SUCCESS, "done"},           {UBSE_MEM_IMPORT_DESTROYING, "unimporting"}};
    std::string errorMsg_{};
};
} // namespace ubse::cli::reg

namespace ubse::ut::cli {
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::cli::reg;
using namespace ubse::mem::controller::message;
using namespace ubse::adapter_plugins::mmi;
void TestUbseCliMemCmdReg::SetUp() {}

void TestUbseCliMemCmdReg::TearDown() {}

TEST_F(TestUbseCliMemCmdReg, RegisterMemModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_MEM_MODULE", UbseCliRegMemModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryEmptyParams)
{
    UbseCliRegMemModule mem_module;
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc({})->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryErrorParams)
{
    UbseCliRegMemModule mem_module;
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "error"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeFailed)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "borrow_detail"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeErrorSize)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "borrow_detail"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsInvokeEmpty)
{
    UbseCliRegMemModule mem_module;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "borrow_detail"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

std::vector<FlatDebtInformation> GetSuccessStatusLendAccount()
{
    return {{"test", AccountType::NUMA, UBSE_MEM_EXPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}},
            {"test", AccountType::FD, UBSE_MEM_EXPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}},
            {"test", AccountType::SHM, UBSE_MEM_EXPORT_SUCCESS, "", "2", {{0, 1, 256}, {1, 216, 1024}}},
            {"test", AccountType::ADDR, UBSE_MEM_EXPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}}};
}

std::vector<FlatDebtInformation> GetSuccessStatusBorrowAccount()
{
    return {{"test", AccountType::NUMA, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}, "2"},
            {"test", AccountType::FD, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}, "3,4,5"},
            {"test", AccountType::SHM, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}, "2,3,4"},
            {"test", AccountType::ADDR, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}, {1, 216, 1024}}, "2,3"}};
}

std::vector<FlatDebtInformation> GetOtherStatusLendAccount()
{
    return {{"test", AccountType::NUMA, UBSE_MEM_EXPORT_RUNNING, "1", "2", {{0, 1, 256}, {1, 216, 1024}}},
            {"test", AccountType::FD, UBSE_MEM_EXPORT_DESTROYING, "1", "2", {{0, 1, 256}, {1, 216, 1024}}},
            {"test", AccountType::SHM, UBSE_MEM_EXPORT_RUNNING, "", "2", {{0, 1, 256}, {1, 216, 1024}}},
            {"test", AccountType::ADDR, UBSE_MEM_EXPORT_DESTROYED, "1", "2", {{0, 1, 256}, {1, 216, 1024}}}};
}

std::vector<FlatDebtInformation> GetSuccessStatusDuplicateLendAccount()
{
    return {{"test", AccountType::NUMA, UBSE_MEM_EXPORT_SUCCESS, "1", "2", {{0, 1, 256}}},
            {"test", AccountType::FD, UBSE_MEM_EXPORT_SUCCESS, "1", "2", {{0, 1, 256}}},
            {"test", AccountType::SHM, UBSE_MEM_EXPORT_SUCCESS, "", "2", {{0, 1, 256}}},
            {"test", AccountType::SHM, UBSE_MEM_EXPORT_SUCCESS, "", "1", {{0, 1, 256}}},
            {"test", AccountType::SHM, UBSE_MEM_EXPORT_SUCCESS, "", "3", {{0, 1, 256}}},
            {"test", AccountType::ADDR, UBSE_MEM_EXPORT_SUCCESS, "1", "2", {{0, 1, 256}}},
            {"test", AccountType::NUMA, UBSE_MEM_EXPORT_SUCCESS, "3", "4", {{0, 1, 256}}},
            {"test", AccountType::FD, UBSE_MEM_EXPORT_SUCCESS, "3", "4", {{0, 1, 256}}},
            {"test", AccountType::SHM, UBSE_MEM_EXPORT_SUCCESS, "", "4", {{0, 1, 256}}},
            {"test", AccountType::ADDR, UBSE_MEM_EXPORT_SUCCESS, "3", "4", {{0, 1, 256}}}};
}

std::vector<FlatDebtInformation> GetSuccessStatusDuplicateBorrowAccount()
{
    return {{"test", AccountType::NUMA, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}}, "2"},
            {"test", AccountType::FD, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}}, "2,3,4"},
            {"test", AccountType::SHM, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}}, "3,4,5"},
            {"test", AccountType::ADDR, UBSE_MEM_IMPORT_SUCCESS, "1", "2", {{0, 1, 256}}, "2,3"},
            {"test", AccountType::NUMA, UBSE_MEM_IMPORT_SUCCESS, "3", "4", {{0, 1, 256}}, "2"},
            {"test", AccountType::FD, UBSE_MEM_IMPORT_SUCCESS, "3", "4", {{0, 1, 256}}, "2,3,4"},
            {"test", AccountType::SHM, UBSE_MEM_IMPORT_SUCCESS, "3", "4", {{0, 1, 256}}, "3,4,5"},
            {"test", AccountType::ADDR, UBSE_MEM_IMPORT_SUCCESS, "3", "4", {{0, 1, 256}}, "2,3"}};
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsNormal)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account = GetSuccessStatusBorrowAccount();
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account = GetSuccessStatusLendAccount();
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsNormalWithoutHostName)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account = GetSuccessStatusBorrowAccount();
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account = GetSuccessStatusLendAccount();
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsSingleExport)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account{};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account = GetSuccessStatusLendAccount();
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    for (const auto& [key, value] : shm_export_account) {
        if (value.hasAttached) {
            continue;
        }
        ubse_borrow_details.insert({key + " single_export_shm", value.ubseOneBorrowDetailInfo});
    }
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsSingleImport)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account = GetSuccessStatusBorrowAccount();
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account{};
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsOtherStatus)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account = GetSuccessStatusBorrowAccount();
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account = GetOtherStatusLendAccount();
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    for (const auto& [key, value] : shm_export_account) {
        if (value.hasAttached) {
            continue;
        }
        ubse_borrow_details.insert({key + " single_export_shm", value.ubseOneBorrowDetailInfo});
    }
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsOtherStatusNoBorrowAccount)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account;
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account = GetOtherStatusLendAccount();
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    for (const auto& [key, value] : shm_export_account) {
        if (value.hasAttached) {
            continue;
        }
        ubse_borrow_details.insert({key + " single_export_shm", value.ubseOneBorrowDetailInfo});
    }
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, BorrowDetailsDuplicateAccount)
{
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, std::string> node_id_with_hostname{
        {"1", "controller"}, {"2", "node01"}, {"3", "node02"}, {"4", "node03"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account =
        GetSuccessStatusDuplicateBorrowAccount();
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account =
        GetSuccessStatusDuplicateLendAccount();

    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    query.ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    query.ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    for (const auto& [key, value] : shm_export_account) {
        if (value.hasAttached) {
            continue;
        }
        ubse_borrow_details.insert({key + " single_export_shm", value.ubseOneBorrowDetailInfo});
    }
    EXPECT_NO_THROW(UbseCliRegModule::UbseCliVariableCelReply(
                        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname))
                        ->UbseCliDisplayResult());
}

TEST_F(TestUbseCliMemCmdReg, UbseCliGetIdsWithHostNameInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::unordered_map<std::string, std::string> node_id_with_hostname;
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    auto res = query.UbseCliGetIdsWithHostName(node_id_with_hostname);
    EXPECT_FALSE(res);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliGetIdsWithHostNameInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::unordered_map<std::string, std::string> node_id_with_hostname;
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    auto res = query.UbseCliGetIdsWithHostName(node_id_with_hostname);
    EXPECT_FALSE(res);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliGetIdsWithHostNameInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_get_id_with_hostname_ubse_invoke_call_normal));
    std::unordered_map<std::string, std::string> node_id_with_hostname;
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    auto res = query.UbseCliGetIdsWithHostName(node_id_with_hostname);
    EXPECT_TRUE(res);
    EXPECT_TRUE(!node_id_with_hostname.empty());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliGetAllDebtInfoInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account;
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account;
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    auto res = query.UbseCliGetAllDebtInfo(node_id_with_hostname, borrow_account, lend_account, {});
    EXPECT_FALSE(res);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliGetAllDebtInfoInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_fetch_debt_info_by_page_ubse_invoke_call_normal));
    std::unordered_map<std::string, std::string> node_id_with_hostname{{"1", "controller"}, {"2", "node01"}};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account;
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account;
    UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl query;
    auto res = query.UbseCliGetAllDebtInfo(node_id_with_hostname, borrow_account, lend_account, {});
    EXPECT_TRUE(res);
    EXPECT_TRUE(!borrow_account.empty());
    EXPECT_TRUE(!lend_account.empty());
    MOCKER(&ubse_invoke_call).reset();
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_fetch_debt_info_by_page_ubse_invoke_call_normal));
}

TEST_F(TestUbseCliMemCmdReg, ValidLinks)
{
    EXPECT_TRUE(LinkIsMatch("1/2/3-4/5/6"));
    EXPECT_TRUE(LinkIsMatch("0/0/0-0/0/0"));
    EXPECT_TRUE(LinkIsMatch("9/9/9-8/8/8"));
}

TEST_F(TestUbseCliMemCmdReg, InvalidLinks)
{
    EXPECT_FALSE(LinkIsMatch("1/2/3-4/5"));
    EXPECT_FALSE(LinkIsMatch("1/2/3/4-5/6/7"));
    EXPECT_FALSE(LinkIsMatch("abc-def"));
    EXPECT_FALSE(LinkIsMatch("1/2/3-4/5/6/7"));
    EXPECT_FALSE(LinkIsMatch(""));
}

TEST_F(TestUbseCliMemCmdReg, ValidSizes)
{
    size_t size;

    EXPECT_TRUE(SizeIsMatch("128M", size));
    EXPECT_EQ(size, 128ULL * 1024 * 1024);

    EXPECT_TRUE(SizeIsMatch("256M", size));
    EXPECT_EQ(size, 256ULL * 1024 * 1024);

    EXPECT_TRUE(SizeIsMatch("1G", size));
    EXPECT_EQ(size, 1ULL * 1024 * 1024 * 1024);

    EXPECT_TRUE(SizeIsMatch("256G", size));
    EXPECT_EQ(size, 256ULL * 1024 * 1024 * 1024);
}

TEST_F(TestUbseCliMemCmdReg, InvalidSizes)
{
    size_t size;
    EXPECT_FALSE(SizeIsMatch("3M", size));

    EXPECT_FALSE(SizeIsMatch("abcM", size));
    EXPECT_FALSE(SizeIsMatch("128K", size));
    EXPECT_FALSE(SizeIsMatch("", size));
}

TEST_F(TestUbseCliMemCmdReg, ValidNames)
{
    EXPECT_TRUE(CheckName("test_name"));
    EXPECT_TRUE(CheckName("test123"));
    EXPECT_TRUE(CheckName("test-name"));
    EXPECT_TRUE(CheckName("a")); // 单字符
    EXPECT_TRUE(CheckName("test_name_123"));
    EXPECT_TRUE(CheckName("test-name-456"));
    EXPECT_TRUE(CheckName("test.name"));
}

TEST_F(TestUbseCliMemCmdReg, InvalidNames)
{
    GTEST_SKIP();
    size_t max_name_length = 48;
    EXPECT_FALSE(CheckName(""));
    EXPECT_FALSE(CheckName("test name"));
    EXPECT_FALSE(CheckName("test@name"));
    EXPECT_FALSE(CheckName(std::string(max_name_length + 1, 'a')));
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "node_borrow"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeErrorSize)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "node_borrow"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "node_borrow"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeBorrowInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_node_borrow_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "node_borrow"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeErrorSize)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckMemoryInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_check_memory_ubse_invoke_call_normal));
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeLendInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_node_lend_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "node_lend"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NumaStatusInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_numa_status_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "numa_status"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NumaStatusAllInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_numa_status_all_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "numa_status"}, {"all", ""}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NumaStatusAllUnsupportOtherType)
{
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "node_borrow"}, {"all", ""}};
    auto result = UbseCliRegMemModule::UbseCliMemQueryFunc(params);
    EXPECT_TRUE(result != nullptr);
}

TEST_F(TestUbseCliMemCmdReg, ConfigInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_config_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "config"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, CheckBorrowDetailTypeValid)
{
    EXPECT_TRUE(CheckBorrowDetailType("fd"));
    EXPECT_TRUE(CheckBorrowDetailType("numa"));
    EXPECT_TRUE(CheckBorrowDetailType("share"));
}

TEST_F(TestUbseCliMemCmdReg, CheckBorrowDetailTypeInvalid)
{
    EXPECT_FALSE(CheckBorrowDetailType(""));
    EXPECT_FALSE(CheckBorrowDetailType("addr"));
    EXPECT_FALSE(CheckBorrowDetailType("invalid"));
}

TEST_F(TestUbseCliMemCmdReg, CheckDeleteTypeValid)
{
    EXPECT_TRUE(CheckDeleteType("fd"));
    EXPECT_TRUE(CheckDeleteType("numa"));
    EXPECT_TRUE(CheckDeleteType("addr"));
    EXPECT_TRUE(CheckDeleteType("share"));
}

TEST_F(TestUbseCliMemCmdReg, CheckDeleteTypeInvalid)
{
    EXPECT_FALSE(CheckDeleteType(""));
    EXPECT_FALSE(CheckDeleteType("invalid"));
}

TEST_F(TestUbseCliMemCmdReg, FormatHostnameSlotEmpty)
{
    EXPECT_EQ(FormatHostnameSlot("", 1), "-(1)");
}

TEST_F(TestUbseCliMemCmdReg, FormatHostnameSlotWithHostname)
{
    EXPECT_EQ(FormatHostnameSlot("node1", 5), "node1(5)");
}

TEST_F(TestUbseCliMemCmdReg, IsValidIntegerStringEmpty)
{
    EXPECT_FALSE(IsValidIntegerString(""));
}

TEST_F(TestUbseCliMemCmdReg, IsValidIntegerStringLeadingZero)
{
    EXPECT_FALSE(IsValidIntegerString("0123"));
}

TEST_F(TestUbseCliMemCmdReg, IsValidIntegerStringNonDigit)
{
    EXPECT_FALSE(IsValidIntegerString("12a3"));
}

TEST_F(TestUbseCliMemCmdReg, IsValidIntegerStringValid)
{
    EXPECT_TRUE(IsValidIntegerString("0"));
    EXPECT_TRUE(IsValidIntegerString("123"));
    EXPECT_TRUE(IsValidIntegerString("4194304"));
}

TEST_F(TestUbseCliMemCmdReg, EnsureDotAtEndEmpty)
{
    std::string s;
    EnsureDotAtEnd(s);
    EXPECT_TRUE(s.empty());
}

TEST_F(TestUbseCliMemCmdReg, EnsureDotAtEndAlreadyHasDot)
{
    std::string s = "error.";
    EnsureDotAtEnd(s);
    EXPECT_EQ(s, "error.");
}

TEST_F(TestUbseCliMemCmdReg, EnsureDotAtEndAddsDot)
{
    std::string s = "error occurred";
    EnsureDotAtEnd(s);
    EXPECT_EQ(s, "error occurred.");
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionIntegerG)
{
    uint64_t size = 0;
    EXPECT_TRUE(SizeConversion("1G", size));
    EXPECT_EQ(size, 1ULL * 1024 * 1024 * 1024);
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionIntegerM)
{
    uint64_t size = 0;
    EXPECT_TRUE(SizeConversion("128M", size));
    EXPECT_EQ(size, 128ULL * 1024 * 1024);
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionDecimalG)
{
    uint64_t size = 0;
    EXPECT_TRUE(SizeConversion("1.5G", size));
    EXPECT_EQ(size, 1610612736ULL); // 1.5 * 1024^3
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionDecimalM)
{
    uint64_t size = 0;
    EXPECT_TRUE(SizeConversion("0.25M", size));
    EXPECT_EQ(size, 262144ULL); // 0.25 * 1024^2
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionKB)
{
    uint64_t size = 0;
    EXPECT_TRUE(SizeConversion("100K", size));
    EXPECT_EQ(size, 102400ULL);
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionBytes)
{
    uint64_t size = 0;
    EXPECT_TRUE(SizeConversion("512B", size));
    EXPECT_EQ(size, 512ULL);
}

TEST_F(TestUbseCliMemCmdReg, SizeConversionInvalid)
{
    uint64_t size = 0;
    EXPECT_FALSE(SizeConversion("", size));
    EXPECT_FALSE(SizeConversion("abc", size));
    EXPECT_FALSE(SizeConversion("1.5X", size));
    EXPECT_FALSE(SizeConversion("1.234M", size)); // 3 decimal places
}

TEST_F(TestUbseCliMemCmdReg, ParseRegionStringEmpty)
{
    std::vector<uint32_t> regions;
    EXPECT_FALSE(ParseRegionString("", regions));
}

TEST_F(TestUbseCliMemCmdReg, ParseRegionStringLeadingComma)
{
    std::vector<uint32_t> regions;
    EXPECT_FALSE(ParseRegionString(",1,2", regions));
}

TEST_F(TestUbseCliMemCmdReg, ParseRegionStringTrailingComma)
{
    std::vector<uint32_t> regions;
    EXPECT_FALSE(ParseRegionString("1,2,", regions));
}

TEST_F(TestUbseCliMemCmdReg, ParseRegionStringNonNumeric)
{
    std::vector<uint32_t> regions;
    EXPECT_FALSE(ParseRegionString("1,a,3", regions));
}

TEST_F(TestUbseCliMemCmdReg, ParseRegionStringValid)
{
    std::vector<uint32_t> regions;
    EXPECT_TRUE(ParseRegionString("1,2,3", regions));
    ASSERT_EQ(regions.size(), 3);
    EXPECT_EQ(regions[0], 1);
    EXPECT_EQ(regions[1], 2);
    EXPECT_EQ(regions[2], 3);
}

TEST_F(TestUbseCliMemCmdReg, ParseRegionStringSingle)
{
    std::vector<uint32_t> regions;
    EXPECT_TRUE(ParseRegionString("5", regions));
    ASSERT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0], 5);
}

TEST_F(TestUbseCliMemCmdReg, ParseResponseBufferDeserFailed)
{
    uint8_t emptyBuf[1] = {0};
    ubse_api_buffer_t buf{emptyBuf, 0};
    auto result = ParseResponseBuffer(buf);
    EXPECT_FALSE(result.success);
}

TEST_F(TestUbseCliMemCmdReg, ParseResponseBufferInvalidState)
{
    UbseSerialization ser;
    ser << std::string("test_name") << std::string("abc");
    uint8_t buffer[256];
    auto len = ser.GetLength();
    memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), len);
    ubse_api_buffer_t buf{buffer, static_cast<uint32_t>(len)};
    auto result = ParseResponseBuffer(buf);
    EXPECT_FALSE(result.success);
}

TEST_F(TestUbseCliMemCmdReg, ParseResponseBufferStageExist)
{
    UbseSerialization ser;
    ser << std::string("test_name") << std::string("3"); // UBSE_EXIST = 3
    ser << right_v<int64_t>(0) << std::string("export_node") << std::string("import_node");
    uint8_t buffer[256];
    auto len = ser.GetLength();
    memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), len);
    ubse_api_buffer_t buf{buffer, static_cast<uint32_t>(len)};
    auto result = ParseResponseBuffer(buf);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.name, "test_name");
    EXPECT_EQ(result.numaId, 0);
}

TEST_F(TestUbseCliMemCmdReg, ParseResponseBufferStageNotExist)
{
    UbseSerialization ser;
    ser << std::string("test_name") << std::string("0"); // UBSE_NOT_EXIST = 0
    uint8_t buffer[256];
    auto len = ser.GetLength();
    memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), len);
    ubse_api_buffer_t buf{buffer, static_cast<uint32_t>(len)};
    auto result = ParseResponseBuffer(buf);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.name, "test_name");
    EXPECT_EQ(result.stage, ubse::mem::controller::UbseMemStage::UBSE_NOT_EXIST);
}

TEST_F(TestUbseCliMemCmdReg, DisplayProcessMemFuncNoType)
{
    std::map<std::string, std::string> params;
    auto result = UbseCliRegMemModule::DisplayProcessMemFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, DisplayProcessMemFuncInvalidType)
{
    std::map<std::string, std::string> params;
    params["type"] = "invalid";
    auto result = UbseCliRegMemModule::DisplayProcessMemFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryFuncBorrowTypeDetailUnsupported)
{
    std::map<std::string, std::string> params;
    params["type"] = "node_borrow";
    params["borrow-type"] = "fd";
    auto result = UbseCliRegMemModule::UbseCliMemQueryFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryFuncNameUnsupported)
{
    std::map<std::string, std::string> params;
    params["type"] = "node_borrow";
    params["name"] = "test";
    auto result = UbseCliRegMemModule::UbseCliMemQueryFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryFuncBorrowTypeInvalid)
{
    std::map<std::string, std::string> params;
    params["type"] = "borrow_detail";
    params["borrow-type"] = "invalid";
    auto result = UbseCliRegMemModule::UbseCliMemQueryFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemQueryFuncNameInvalid)
{
    std::map<std::string, std::string> params;
    params["type"] = "borrow_detail";
    params["name"] = "invalid name!";
    auto result = UbseCliRegMemModule::UbseCliMemQueryFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, NodeLendInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::string, std::string> params{{"type", "node_lend"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeLendInvokeErrorSize)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    std::map<std::string, std::string> params{{"type", "node_lend"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NodeLendInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::string, std::string> params{{"type", "node_lend"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NumaStatusInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::string, std::string> params{{"type", "numa_status"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, NumaStatusInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::string, std::string> params{{"type", "numa_status"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, ConfigInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    std::map<std::string, std::string> params{{"type", "config"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, ConfigInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    std::map<std::string, std::string> params{{"type", "config"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemCmdReg, UbseCliProcessNumaStatusDataNormal)
{
    UbseSerialization ser;
    size_t infoSize = 1;
    ser << std::string("node1") << std::string("0") << std::string("65536") << std::string("16384")
        << std::string("49152") << std::string("25%");
    uint8_t buffer[256];
    auto len = ser.GetLength();
    memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), len);
    UbseDeSerialization deser(buffer, len);
    auto result = UbseCliRegMemModule::UbseCliProcessNumaStatusData(deser, infoSize, false);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliProcessNumaStatusDataDeserFailed)
{
    uint8_t emptyBuf[1] = {0};
    UbseDeSerialization deser(emptyBuf, 0);
    auto result = UbseCliRegMemModule::UbseCliProcessNumaStatusData(deser, 1, false);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemoryStatusDataNormal)
{
    UbseSerialization ser;
    size_t infoSize = 1;
    ser << std::string("node1") << std::string("ok") << std::string("All good");
    uint8_t buffer[256];
    auto len = ser.GetLength();
    memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), len);
    UbseDeSerialization deser(buffer, len);
    auto result = UbseCliRegMemModule::UbseCliMemoryStatusData(deser, infoSize);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliMemCmdReg, UbseCliMemoryStatusDataDeserFailed)
{
    uint8_t emptyBuf[1] = {0};
    UbseDeSerialization deser(emptyBuf, 0);
    auto result = UbseCliRegMemModule::UbseCliMemoryStatusData(deser, 1);
    EXPECT_NE(result, nullptr);
}
} // namespace ubse::ut::cli
