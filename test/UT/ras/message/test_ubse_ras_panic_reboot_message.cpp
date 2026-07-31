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

#include <gtest/gtest.h>
#include "ubse_error.h"
#include "ubse_ras.h"
#include "message/ubse_ras_panic_reboot_message.h"

namespace ubse::ras::message::ut {
TEST(TestUbseRasPanicRebootMessage, SerializeDeserializeRoundTrip)
{
    const std::string faultEid = "0000:0000:003f:0200:0010:0000:df00:0b00";
    UbseRasPanicRebootMessagePtr message = new UbseRasPanicRebootMessage(faultEid, "123", ALARM_PANIC_EVENT, "2");
    ASSERT_EQ(message->Serialize(), UBSE_OK);

    UbseRasPanicRebootMessagePtr decoded = new UbseRasPanicRebootMessage();
    decoded->SetInputRawDataFromShared(message->GetSharedOutputData(), message->SerializedDataSize());
    ASSERT_EQ(decoded->Deserialize(), UBSE_OK);
    EXPECT_EQ(decoded->GetFaultEid(), faultEid);
    EXPECT_EQ(decoded->GetMsgId(), "123");
    EXPECT_EQ(decoded->GetFaultType(), static_cast<uint32_t>(ALARM_PANIC_EVENT));
    EXPECT_EQ(decoded->GetForwardNodeId(), "2");
    EXPECT_EQ(decoded->GetResult(), UBSE_OK);
}
} // namespace ubse::ras::message::ut
