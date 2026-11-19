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

#include <memory>
#include <unordered_map>

#include "ubse_error.h"
#include "gtest/gtest.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"

namespace ubse::ut::serial {

TEST(UbseAutoSimpoTest, DeserializeNullInput)
{
    ubse::simpo::util::UbseAutoSimpo<uint32_t> simpo;
    EXPECT_EQ(UBSE_ERROR, simpo.Deserialize());
}

TEST(UbseAutoSimpoTest, DeserializeTruncatedPayload)
{
    constexpr uint32_t inputSize = sizeof(uint32_t);
    auto input = std::shared_ptr<uint8_t[]>(new uint8_t[inputSize]{});
    ubse::simpo::util::UbseAutoSimpo<uint32_t> simpo;
    ASSERT_EQ(UBSE_OK, simpo.SetInputRawDataFromShared(input, inputSize));

    EXPECT_EQ(UBSE_ERROR, simpo.Deserialize());
}

TEST(UbseAutoSimpoTest, DeserializeOversizedMap)
{
    ubse::serial::UbseSerialization output;
    uint32_t errCode = 0;
    size_t itemCount = ubse::serial::ONCE_LIMIT_LEN + 1;
    output << errCode << itemCount;
    ASSERT_TRUE(output.Check());

    auto input = std::shared_ptr<uint8_t[]>(output.GetBuffer(true), std::default_delete<uint8_t[]>());
    ubse::simpo::util::UbseAutoSimpo<std::unordered_map<uint32_t, uint32_t>> simpo;
    ASSERT_EQ(UBSE_OK, simpo.SetInputRawDataFromShared(input, output.GetLength()));

    EXPECT_EQ(UBSE_ERROR, simpo.Deserialize());
}

} // namespace ubse::ut::serial
