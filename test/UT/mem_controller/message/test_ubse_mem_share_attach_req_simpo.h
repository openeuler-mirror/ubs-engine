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

#ifndef UBSE_ENGINE_TEST_UBSE_MEM_SHARE_ATTACH_REQ_SIMPO_H
#define UBSE_ENGINE_TEST_UBSE_MEM_SHARE_ATTACH_REQ_SIMPO_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "message/ubse_mem_share_attach_req_simpo.h"

namespace ubse::mem::controller::message::ut {
class TestUbseMemShareAttachReqSimpo : public testing::Test {
public:
    void SetUp() override;

    void TearDown() override;

private:
    UbseMemShareAttachReqSimpoPtr obj;
};
} // namespace ubse::mem::controller::message::ut

#endif // UBSE_ENGINE_TEST_UBSE_MEM_SHARE_ATTACH_REQ_SIMPO_H
