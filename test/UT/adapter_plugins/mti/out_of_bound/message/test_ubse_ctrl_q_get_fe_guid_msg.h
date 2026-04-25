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

#ifndef TEST_UBSE_CTRL_Q_GET_FE_GUID_MSG_H
#define TEST_UBSE_CTRL_Q_GET_FE_GUID_MSG_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_get_fe_guid_msg.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_message.h"
#include "src/adapter_plugins/mti/out_of_band/message/ubse_ctrl_q_msg_helper.h"
#include "ubse_error.h"

namespace ubse::ut::mti::ctrl_q {
using namespace ubse::mti::urma;
using namespace ubse::mti::_1825;
using namespace ubse::mti::ctrl_q;

class UbseCtrlQGetIdevPfeGuidReqMsgTest : public ::testing::Test {
protected:
    void SetUp() override;

    UbseMtiIdevPfe pfe_;
};

class UbseCtrlQGetIdevPfeGuidRespMsgTest : public ::testing::Test {
protected:
    void TearDown() override;

    CtrlQRespMessage resp_{};
};

class UbseCtrlQGetIdevVfeGuidReqMsgTest : public ::testing::Test {
protected:
    void SetUp() override;

    UbseMtiIdevVfe vfe_;
};

class UbseCtrlQGet1825PfGuidReqMsgTest : public ::testing::Test {
protected:
    void SetUp() override;

    UbseMti1825Pf pf_;
};

class UbseCtrlQGet1825PfGuidRespMsgTest : public ::testing::Test {
protected:
    void TearDown() override;

    CtrlQRespMessage resp_{};
};

class UbseCtrlQGet1825VfGuidReqMsgTest : public ::testing::Test {
protected:
    void SetUp() override;

    UbseMti1825Vf vf_;
};
} // namespace ubse::ut::mti::ctrl_q
#endif // TEST_UBSE_CTRL_Q_GET_FE_GUID_MSG_H