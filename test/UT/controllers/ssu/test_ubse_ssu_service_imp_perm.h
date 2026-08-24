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

#ifndef TEST_UBSE_SSU_SERVICE_IMP_PERM_H
#define TEST_UBSE_SSU_SERVICE_IMP_PERM_H

#include <string>
#include "test_ubse_ssu_service_imp_fixture.h"

namespace ubse::ssu::service::ut {

class TestUbseSsuServiceImpPerm : public UbseSsuServiceImpTestBase {
public:
    TestUbseSsuServiceImpPerm() = default;

    void SetUp() override;
    void TearDown() override;

protected:
    // Helper: create a ledger entry with namespaces for permission tests
    static void SetupPermLedger(const std::string &name, UbseSsuNsState state,
                                const std::vector<UbseSsuDevNameSpace> &nsList);

    // Helper: create a ledger entry with namespaces for GetNsStats tests
    static void SetupStatsLedger(const std::string &name,
                                 const std::vector<UbseSsuDevNameSpace> &nsList);
};

// ============================================================================
// Controllable mock state for AddNameSpaceAllowHost
// Tests can set g_addPermFailAfter to inject failures.
// ============================================================================
extern std::atomic<int> g_addPermFailAfter;
extern std::atomic<int> g_addPermCallCount;
extern std::atomic<int> g_removePermFailAfter;
extern std::atomic<int> g_removePermCallCount;

void ResetPermMockState();
void SetupPermAdapterFuncs();

} // namespace ubse::ssu::service::ut

#endif // TEST_UBSE_SSU_SERVICE_IMP_PERM_H
