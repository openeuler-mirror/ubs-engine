/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef TEST_UBSE_CTRL_Q_GET_1825_FE_MSG_H
#define TEST_UBSE_CTRL_Q_GET_1825_FE_MSG_H
#include "gtest/gtest.h"
#include "src/include/adapter_plugins/mti/ubse_mti_1825.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_1825_fe_msg.h"

namespace ubse::ut::mti::ctrl_q {
class TestUbseCtrlQGet1825FeMsg : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
};
}

#endif // TEST_UBSE_CTRL_Q_GET_1825_FE_MSG_H
