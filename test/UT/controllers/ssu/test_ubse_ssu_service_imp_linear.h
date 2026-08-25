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


#ifndef TEST_UBSE_SSU_SERVICE_IMP_LINEAR_H
#define TEST_UBSE_SSU_SERVICE_IMP_LINEAR_H

#include <string>
#include "test_ubse_ssu_service_imp_fixture.h"

namespace ubse::ssu::service::ut {

class TestUbseSsuServiceImpLinear : public UbseSsuServiceImpTestBase {
public:
    TestUbseSsuServiceImpLinear() = default;

    void SetUp() override;
    void TearDown() override;

protected:
    // Helper: make a UbseSsuLinearSpaceReq
    static UbseSsuLinearSpaceReq MakeLinearReq(const std::string &name = "test_linear",
                                               const std::string &devName = "md_linear", const std::string &nqn = "",
                                               const UbseSsuAllocIdentityInfo &identity = {});

    // Helper: build a UbseSsuNameSpaceInfo for a ledger entry
    static UbseSsuNameSpaceInfo MakeNsInfo(const UbseSsuDevNameSpace &ns, uint64_t nsSize = 4096);

    // Helper: create a ledger entry with nameSpaceList for attach test
    static void SetupLedgerForAttach(const std::string &name, UbseSsuNsState state,
                                     const std::vector<UbseSsuDevNameSpace> &nsList,
                                     UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::LINEAR);
};

} // namespace ubse::ssu::service::ut

#endif // TEST_UBSE_SSU_SERVICE_IMP_LINEAR_H
