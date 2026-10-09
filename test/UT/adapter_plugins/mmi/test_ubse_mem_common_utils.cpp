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
#include "test_ubse_mem_common_utils.h"
#include "ubse_mem_common_utils.h"

namespace ubse::ut::mmi {
using namespace ubse::mmi;
TEST_F(TestUbseMemCommonUtils, GetFileFirstLine_Success)
{
    std::string path = "/proc/cmdline";
    std::string line;
    auto ret = RmCommonUtils::GetFileFirstLine(path, line);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemCommonUtils, GetFileFirstLine_Fail)
{
    std::string path{};
    std::string line{};
    auto ret = RmCommonUtils::GetFileFirstLine(path, line);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemCommonUtils, SetMaskFromRegionIndex_Supports72Nodes)
{
    UbseMemLocalObmmCustomMeta customMeta{};
    const std::vector<uint32_t> regionNodeIndex = {0u, 31u, 32u, 71u};

    EXPECT_EQ(SetMaskFromRegionIndex(regionNodeIndex, customMeta.regionMask), UBSE_OK);
    EXPECT_TRUE(IsBitSet(customMeta.regionMask, 0u));
    EXPECT_TRUE(IsBitSet(customMeta.regionMask, 31u));
    EXPECT_TRUE(IsBitSet(customMeta.regionMask, 32u));
    EXPECT_TRUE(IsBitSet(customMeta.regionMask, 71u));
    EXPECT_FALSE(IsBitSet(customMeta.regionMask, 70u));
    EXPECT_EQ(sizeof(customMeta.regionMask), 9u);
}

TEST_F(TestUbseMemCommonUtils, SetMaskFromRegionIndex_RejectsOutOfRangeNode)
{
    uint8_t mask[REGION_MASK_BYTE_SIZE]{};

    EXPECT_EQ(SetMaskFromRegionIndex({MAX_NODE_NUM}, mask), UBSE_ERROR_INVAL);
    EXPECT_FALSE(IsBitSet(mask, MAX_NODE_NUM));
}
} // namespace ubse::ut::mmi
