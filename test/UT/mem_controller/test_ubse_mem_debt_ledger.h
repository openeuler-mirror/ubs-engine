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

#ifndef TEST_UBSE_MEM_DEBT_LEDGER_H
#define TEST_UBSE_MEM_DEBT_LEDGER_H

#include <gtest/gtest.h>
#include "ubse_mem_debt_ledger.h"

namespace ubse::mem::controller::debt::ut {

using namespace ubse::adapter_plugins::mmi;

class TestUbseMemDebtLedger : public testing::Test {
public:
    TestUbseMemDebtLedger() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    UbseMemFdBorrowImportObj CreateFdImportObj(const std::string& name, const std::string& nodeId);
    UbseMemFdBorrowExportObj CreateFdExportObj(const std::string& name, const std::string& nodeId);
    UbseMemNumaBorrowImportObj CreateNumaImportObj(const std::string& name, const std::string& nodeId);
    UbseMemNumaBorrowExportObj CreateNumaExportObj(const std::string& name, const std::string& nodeId);
    UbseMemShareBorrowImportObj CreateShareImportObj(const std::string& name, const std::string& nodeId);
    UbseMemShareBorrowExportObj CreateShareExportObj(const std::string& name, const std::string& nodeId);
    UbseMemAddrBorrowImportObj CreateAddrImportObj(const std::string& name, const std::string& nodeId);
    UbseMemAddrBorrowExportObj CreateAddrExportObj(const std::string& name, const std::string& nodeId);
};

class TestUbseMemNodeDebtMap : public testing::Test {
public:
    TestUbseMemNodeDebtMap() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    UbseMemFdBorrowImportObj CreateTestObj(const std::string& name, UbseMemState state);
};

class TestUbseMemTypeDebtMap : public testing::Test {
public:
    TestUbseMemTypeDebtMap() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    UbseMemFdBorrowImportObj CreateTestImportObj(const std::string& name);
    UbseMemFdBorrowExportObj CreateTestExportObj(const std::string& name);
};

class TestTypeTraits : public testing::Test {
public:
    TestTypeTraits() = default;

    void SetUp() override;

    void TearDown() override;
};

} // namespace ubse::mem::controller::debt::ut
#endif
