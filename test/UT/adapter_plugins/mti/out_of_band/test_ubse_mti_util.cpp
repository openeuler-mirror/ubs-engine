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

#include "test_ubse_mti_util.h"
#include <cstring>
#include "securec.h"

namespace ubse::ut::mti {

using namespace ubse::mti;
void TestUbseMtiUtil::SetUp()
{
    Test::SetUp();
}
void TestUbseMtiUtil::TearDown()
{
    Test::TearDown();
}
// ==================== EidStrToArray 测试 ====================

TEST_F(TestUbseMtiUtil, ValidEid)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_TRUE(EidStrToArray("00001", eid));
    uint32_t value = 0;
    auto ret = memcpy_s(&value, sizeof(value), eid.data(), sizeof(value));
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(1u, value);
}

TEST_F(TestUbseMtiUtil, MaxEidValue)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_TRUE(EidStrToArray("fffff", eid));
    uint32_t value = 0;
    auto ret = memcpy_s(&value, sizeof(value), eid.data(), sizeof(value));
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(0xFFFFFu, value);
}

TEST_F(TestUbseMtiUtil, UppercaseHex)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_TRUE(EidStrToArray("0ABCD", eid));
    uint32_t value = 0;
    auto ret = memcpy_s(&value, sizeof(value), eid.data(), sizeof(value));
    EXPECT_EQ(ret, EOK);
    EXPECT_EQ(0xABCDu, value);
}

TEST_F(TestUbseMtiUtil, MixedCaseHex)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_TRUE(EidStrToArray("aBcDe", eid));
}

TEST_F(TestUbseMtiUtil, TooShort)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_FALSE(EidStrToArray("1234", eid));
}

TEST_F(TestUbseMtiUtil, TooLong)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_FALSE(EidStrToArray("123456", eid));
}

TEST_F(TestUbseMtiUtil, InvalidCharacters)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_FALSE(EidStrToArray("ghijk", eid));
}

TEST_F(TestUbseMtiUtil, EmptyString)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_FALSE(EidStrToArray("", eid));
}

TEST_F(TestUbseMtiUtil, WithDash)
{
    std::array<uint8_t, 16> eid{};
    EXPECT_FALSE(EidStrToArray("12-45", eid));
}

// ==================== EidArrayToStr 测试 ====================

TEST_F(TestUbseMtiUtil, ValidEidArrayToStr)
{
    std::array<uint8_t, 16> eid{};
    uint32_t value = 0xABCD;
    auto ret = memcpy_s(eid.data(), eid.size(), &value, sizeof(value));
    EXPECT_EQ(ret, EOK);
    std::string eidStr;
    EXPECT_TRUE(EidArrayToStr(eid, eidStr));
    EXPECT_EQ("0abcd", eidStr);
}

TEST_F(TestUbseMtiUtil, ZeroEid)
{
    std::array<uint8_t, 16> eid{};
    std::string eidStr;
    EXPECT_TRUE(EidArrayToStr(eid, eidStr));
    EXPECT_EQ("00000", eidStr);
}

TEST_F(TestUbseMtiUtil, MaxEid)
{
    std::array<uint8_t, 16> eid{};
    uint32_t value = 0xFFFFF;
    auto ret = memcpy_s(eid.data(), eid.size(), &value, sizeof(value));
    EXPECT_EQ(ret, EOK);
    std::string eidStr;
    EXPECT_TRUE(EidArrayToStr(eid, eidStr));
    EXPECT_EQ("fffff", eidStr);
}

// ==================== GuidStrToArray 测试 ====================

TEST_F(TestUbseMtiUtil, ValidGuid)
{
    std::array<uint8_t, 16> guid{};
    std::string guidStr = "0123456789abcdef0123456789abcdef";
    EXPECT_TRUE(GuidStrToArray(guidStr, guid));
}

TEST_F(TestUbseMtiUtil, AllZeros)
{
    std::array<uint8_t, 16> guid{};
    std::string guidStr = "00000000000000000000000000000000";
    EXPECT_TRUE(GuidStrToArray(guidStr, guid));
    for (auto byte : guid) {
        EXPECT_EQ(0, byte);
    }
}

TEST_F(TestUbseMtiUtil, AllFs)
{
    std::array<uint8_t, 16> guid{};
    std::string guidStr = "ffffffffffffffffffffffffffffffff";
    EXPECT_TRUE(GuidStrToArray(guidStr, guid));
    for (auto byte : guid) {
        EXPECT_EQ(0xFF, byte);
    }
}

TEST_F(TestUbseMtiUtil, StrToArrayUppercaseHex)
{
    std::array<uint8_t, 16> guid{};
    std::string guidStr = "0123456789ABCDEF0123456789ABCDEF";
    EXPECT_TRUE(GuidStrToArray(guidStr, guid));
}

TEST_F(TestUbseMtiUtil, StrToArrayTooShort)
{
    std::array<uint8_t, 16> guid{};
    EXPECT_FALSE(GuidStrToArray("0123456789abcdef", guid));
}

TEST_F(TestUbseMtiUtil, StrToArrayTooLong)
{
    std::array<uint8_t, 16> guid{};
    EXPECT_FALSE(GuidStrToArray("0123456789abcdef0123456789abcdef00", guid));
}

TEST_F(TestUbseMtiUtil, StrToArrayInvalidCharacters)
{
    std::array<uint8_t, 16> guid{};
    EXPECT_FALSE(GuidStrToArray("0123456789abcdef0123456789abcdefg", guid));
}

TEST_F(TestUbseMtiUtil, StrToArrayEmptyString)
{
    std::array<uint8_t, 16> guid{};
    EXPECT_FALSE(GuidStrToArray("", guid));
}

TEST_F(TestUbseMtiUtil, WithDashes)
{
    std::array<uint8_t, 16> guid{};
    EXPECT_FALSE(GuidStrToArray("01234567-89ab-cdef-0123-456789abcdef", guid));
}

TEST_F(TestUbseMtiUtil, LittleEndianByteOrder)
{
    std::array<uint8_t, 16> guid{};
    std::string guidStr = "01000000000000000000000000000000";
    EXPECT_TRUE(GuidStrToArray(guidStr, guid));
    EXPECT_EQ(0x01, guid[15]);
    EXPECT_EQ(0x00, guid[0]);
}

// ==================== UpiStrToUint16 测试 ====================

TEST_F(TestUbseMtiUtil, ValidUpi)
{
    uint16_t upi = 0;
    EXPECT_TRUE(UpiStrToUint16("0123", upi));
    EXPECT_EQ(0x0123, upi);
}

TEST_F(TestUbseMtiUtil, MaxValue)
{
    uint16_t upi = 0;
    EXPECT_TRUE(UpiStrToUint16("ffff", upi));
    EXPECT_EQ(0xFFFF, upi);
}

TEST_F(TestUbseMtiUtil, ZeroValue)
{
    uint16_t upi = 0;
    EXPECT_TRUE(UpiStrToUint16("0000", upi));
    EXPECT_EQ(0, upi);
}

TEST_F(TestUbseMtiUtil, StrToUint16UppercaseHex)
{
    uint16_t upi = 0;
    EXPECT_TRUE(UpiStrToUint16("ABCD", upi));
    EXPECT_EQ(0xABCD, upi);
}

TEST_F(TestUbseMtiUtil, StrToUint16TooShort)
{
    uint16_t upi = 0;
    EXPECT_FALSE(UpiStrToUint16("123", upi));
}

TEST_F(TestUbseMtiUtil, StrToUint16TooLong)
{
    uint16_t upi = 0;
    EXPECT_FALSE(UpiStrToUint16("12345", upi));
}

TEST_F(TestUbseMtiUtil, StrToUint16InvalidCharacters)
{
    uint16_t upi = 0;
    EXPECT_FALSE(UpiStrToUint16("ghij", upi));
}

TEST_F(TestUbseMtiUtil, StrToUint16EmptyString)
{
    uint16_t upi = 0;
    EXPECT_FALSE(UpiStrToUint16("", upi));
}

// ==================== UpiUint16ToStr 测试 ====================

TEST_F(TestUbseMtiUtil, Uint16ToStrZeroValue)
{
    EXPECT_EQ("0000", UpiUint16ToStr(0));
}

TEST_F(TestUbseMtiUtil, Uint16ToStrMaxValue)
{
    EXPECT_EQ("ffff", UpiUint16ToStr(0xFFFF));
}

TEST_F(TestUbseMtiUtil, Uint16ToStrNormalValue)
{
    EXPECT_EQ("0123", UpiUint16ToStr(0x0123));
}

TEST_F(TestUbseMtiUtil, Uint16ToStrLowercaseOutput)
{
    EXPECT_EQ("abcd", UpiUint16ToStr(0xABCD));
}

} // namespace ubse::ut::mti
