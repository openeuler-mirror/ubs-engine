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

#include "test_ubse_cli_builder.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"
#include "ubse_cli_res_builder.h"

namespace ubse::ut::cli {
using namespace ubse::cli::framework;
using namespace ubse::cli::reg;

void TestUbseCliBuilder::SetUp() {}

void TestUbseCliBuilder::TearDown() {}

std::string UbseFuncA()
{
    return "INFO: Call \t UbseFuncA!";
}

std::string UbseFuncB()
{
    return "INFO: Call \t UbseFuncB!";
}

std::shared_ptr<UbseCliResultEcho> UbseCliTestFunString(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    return std::make_shared<UbseCliStringEcho>(std::string(UbseFuncA() + UbseFuncB()));
}

std::shared_ptr<UbseCliResultEcho> UbseCliTestInvDemo1(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    UbseCliResBuilder res_builder(UBSE_CLI_NUM_3, UBSE_CLI_NUM_10);
    size_t row = res_builder.UbseCliAddRow();
    res_builder.UbseCliAddlineSeparate(res_builder.UbseCliGetRows());
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "Name");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "Description");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "Age");
    res_builder.UbseCliAddBottomlineSeparate();

    UbseCliResBuilder res_builder1(UBSE_CLI_NUM_3, UBSE_CLI_NUM_10 + UBSE_CLI_NUM_10);
    row = res_builder1.UbseCliAddRow();
    res_builder1.UbseCliAddlineSeparate(res_builder1.UbseCliGetRows());
    res_builder1.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "Alice");
    res_builder1.UbseCliSetCellData(row, UBSE_CLI_NUM_2,
                                    "A wonderful person who enjoys writing code in C++ and solving complex problems.");
    res_builder1.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "30");
    row = res_builder1.UbseCliAddRow();
    res_builder1.UbseCliAddlineSeparate(res_builder1.UbseCliGetRows());
    res_builder1.UbseCliSetCellData(row, UBSE_CLI_NUM_1,
                                    "Bob Bob Bob Bob Bob Bob Bob Bob Bob Bob Bob Bob Bob Bob Bob ");
    res_builder1.UbseCliSetCellData(row, UBSE_CLI_NUM_2, UbseFuncB());
    res_builder1.UbseCliSetCellData(row, UBSE_CLI_NUM_3,
                                    "25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 25 ");
    res_builder1.UbseCliAddBottomlineSeparate();

    std::vector<UbseCliVariableCellInfo> cells;
    cells.push_back(res_builder.UbseCliVariableCellBuild());
    cells.push_back(res_builder1.UbseCliVariableCellBuild());
    return std::make_shared<UbseCliVariableCellsEcho>(cells);
}
std::shared_ptr<UbseCliResultEcho> UbseCliTestInvDemo2(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    UbseCliResBuilder fault_res_builder(UBSE_CLI_NUM_4, UBSE_CLI_NUM_10 * UBSE_CLI_NUM_10);
    UbseCliResBuilder res_builder(UBSE_CLI_NUM_4, UBSE_CLI_NUM_10 * UBSE_CLI_NUM_3);
    res_builder.UbseCliSetMinWidth(UBSE_CLI_NUM_10 * UBSE_CLI_NUM_5);
    res_builder.UbseCliSetMinWidth(UBSE_CLI_NUM_3 * UBSE_CLI_NUM_4);
    res_builder.UbseCliSetMinColWidth(UBSE_CLI_NUM_2, UBSE_CLI_NUM_3 * UBSE_CLI_NUM_3);
    size_t row = res_builder.UbseCliAddRow();
    res_builder.UbseCliAddlineSeparate(res_builder.UbseCliGetRows());
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "PRODUCT NAME");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "CATEGORY");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "PRICE");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "STOCK");
    res_builder.UbseCliAddBottomlineSeparate();

    row = res_builder.UbseCliAddRow();
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "Smartphone");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "Electronics");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "$399.99");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "50");

    std::map<size_t, std::string> mergeCellDat{
        {UBSE_CLI_NUM_1, "Home Appliances"},
        {res_builder.UbseCliGetCols(),
         "A comprehensive range of essential household devices designed to enhance convenience and efficiency in "
         "daily life"}};
    res_builder.UbseCliAddMergeRow(mergeCellDat);

    row = res_builder.UbseCliAddRow();
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "Television");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "$199.99");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "15");

    row = res_builder.UbseCliAddRow();
    res_builder.UbseCliAddNolineSeparate(row);
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "Refrigerator");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "$499.99");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "10");

    row = res_builder.UbseCliAddRow();
    res_builder.UbseCliAddBottomNolineSeparate();
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "Washing Machine");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "$299.99");
    res_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "25");

    res_builder.UbseCliAddBottomlineSeparate();

    return UbseCliRegModule::UbseCliVariableCelReply(res_builder.UbseCliVariableCellBuild());
}

TEST_F(TestUbseCliBuilder, CmdBuilder)
{
    UbseCliRegBuilder cmd_builder;
    auto cmd = cmd_builder.UbseCliSetCommand("command")
                   .UbseCliSetType("type")
                   .UbseCliAddOption("t1", "test1", "one")
                   .UbseCliAddOption("t2", "test2", "two")
                   .UbseCliSetFunc(UbseCliTestFunString)
                   .UbseCliBuild();
    EXPECT_EQ(cmd.command, "command");
    EXPECT_EQ(cmd.type, "type");
    EXPECT_EQ(cmd.options.size(), 2);
    EXPECT_NO_THROW(cmd.commandFunc({})->UbseCliDisplayResult());
}

TEST_F(TestUbseCliBuilder, MultCellBuilder)
{
    EXPECT_NO_THROW(UbseCliTestInvDemo1({})->UbseCliDisplayResult());
}

TEST_F(TestUbseCliBuilder, MergeCellBuilder)
{
    EXPECT_NO_THROW(UbseCliTestInvDemo2({})->UbseCliDisplayResult());
}
} // namespace ubse::ut::cli
