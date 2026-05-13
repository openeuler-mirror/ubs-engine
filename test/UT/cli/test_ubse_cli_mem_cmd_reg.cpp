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
    auto res = query.UbseCliGetIdsWithHostName(node_id_with_hostname);
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

TEST_F(TestUbseCliMemCmdReg, ConfigInvokeNormal)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_config_ubse_invoke_call_normal));
    std::map<std::basic_string<char>, std::basic_string<char>> params{{"type", "config"}};
    EXPECT_NO_THROW(UbseCliRegMemModule::UbseCliMemQueryFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
