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

#ifndef UBS_ENGINE_TEST_UBSE_SSU_RESERVATION_MGR_H
#define UBS_ENGINE_TEST_UBSE_SSU_RESERVATION_MGR_H

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "ubse_ssu_reservation_mgr.h"

namespace ubse::ssu::service::ut {

class TestUbseSsuReservationMgr : public testing::Test {
public:
    TestUbseSsuReservationMgr() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    UbseSsuReservationMgr mgr_;
};

} // namespace ubse::ssu::service::ut

#endif // UBS_ENGINE_TEST_UBSE_SSU_RESERVATION_MGR_H
