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

#include "test_ubse_mti_eid_interface.h"

#include "ubse_error.h"
#include "adapter_plugins/mti/ubse_mti_eid_interface.h"

namespace ubse::utils {

void TestUbseMtiEidInterface::SetUp()
{
    Test::SetUp();
}

void TestUbseMtiEidInterface::TearDown()
{
    Test::TearDown();
}

/*
 * 用例描述
 * 测试GenerateUrmaDevEid正常生成EID
 * 测试步骤
 * 1. 传入合法的superPodId、nodeId、fe0Id、fe1Id
 * 2. 调用GenerateUrmaDevEid
 * 预期结果
 * 1. 返回非空字符串
 * 2. 返回的EID格式正确（8段，每段4位十六进制）
 */
TEST_F(TestUbseMtiEidInterface, GenerateUrmaDevEid_Normal)
{
    auto eid = GenerateUrmaDevEid(0, 1, 0, 0);
    EXPECT_EQ(eid, "4245:4944:0000:0000:0000:0000:0100:0000");
    auto eid2 = GenerateUrmaDevEid(0, 2, 0, 0);
    EXPECT_EQ(eid2, "4245:4944:0000:0000:0000:0000:0200:0000");
}

TEST_F(TestUbseMtiEidInterface, GenerateUrmaDevEid_FeId)
{
    auto eid1 = GenerateUrmaDevEid(0, 1, 1, 1);
    EXPECT_EQ(eid1, "4245:4944:0000:0000:0100:0100:0100:0000");
    auto eid2 = GenerateUrmaDevEid(0, 1, 2, 1);
    EXPECT_EQ(eid2, "4245:4944:0000:0000:0100:0200:0100:0000");
}

/*
 * 用例描述
 * 测试ParseBaseEid正常解析
 * 测试步骤
 * 1. 传入合法的EID字符串
 * 2. 调用ParseBaseEid
 * 预期结果
 * 1. 返回UBSE_OK
 * 2. bitStr长度为128
 */
TEST_F(TestUbseMtiEidInterface, ParseBaseEid_Normal)
{
    std::string baseEid = "4245:4944:0000:0000:0000:0000:0100:0000";
    std::string bitStr;
    auto ret = ParseBaseEid(baseEid, bitStr);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(bitStr.size(), 128);
    // 预期bitStr为各段16进制值转换为16位二进制字符串的拼接结果
    std::string expectedBitStr = "01000010010001010100100101000100";
    expectedBitStr += "00000000000000000000000000000000"
                      "00000000000000000000000000000000"
                      "00000001000000000000000000000000";
    EXPECT_EQ(bitStr, expectedBitStr);
}

TEST_F(TestUbseMtiEidInterface, ParseBaseEid_TooFewSegments)
{
    std::string baseEid = "4245:4944:0000";
    std::string bitStr;
    auto ret = ParseBaseEid(baseEid, bitStr);
    EXPECT_EQ(ret, UBSE_ERROR);
}
TEST_F(TestUbseMtiEidInterface, ParseBaseEid_EmptyString)
{
    std::string baseEid;
    std::string bitStr;
    auto ret = ParseBaseEid(baseEid, bitStr);
    EXPECT_EQ(ret, UBSE_ERROR);
}

/*
 * 用例描述
 * 测试ParseCnaFromEid正常解析
 * 测试步骤
 * 1. 传入合法的EID字符串
 * 2. 调用ParseCnaFromEid
 * 预期结果
 * 1. 返回UBSE_OK
 * 2. cna值正确（从第97位起取24位）
 */
TEST_F(TestUbseMtiEidInterface, ParseCnaFromEid_Normal)
{
    std::string eid = "0000:0000:0044:5200:0010:0000:140B:C510";
    std::string cna;
    auto ret = ParseCnaFromEid(eid, cna);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(cna, "0000:0000:0000:0000:0000:0000:140B:C500");
}

TEST_F(TestUbseMtiEidInterface, ParseCnaFromEid_EmptyString)
{
    std::string eid;
    std::string cna;
    auto ret = ParseCnaFromEid(eid, cna);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMtiEidInterface, ParseCnaFromEid_InvalidFormat)
{
    std::string eid = "0000:0000:0044:5200:0010:0000:140B:C510:0000";
    std::string cna;
    auto ret = ParseCnaFromEid(eid, cna);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMtiEidInterface, ParseCnaFromEid_SameCna)
{
    std::string eid1 = "0000:0000:0044:5200:0010:0000:140B:C520";
    std::string eid2 = "0000:0000:0044:5200:0010:0000:140B:C518";
    std::string cna1;
    std::string cna2;
    auto ret1 = ParseCnaFromEid(eid1, cna1);
    auto ret2 = ParseCnaFromEid(eid2, cna2);
    EXPECT_EQ(ret1, UBSE_OK);
    EXPECT_EQ(ret2, UBSE_OK);
    EXPECT_EQ(cna1, cna2);
}

/*
 * 用例描述
 * 测试ConstructEid正常构造
 * 测试步骤
 * 1. 先通过ParseBaseEid获取bitStr
 * 2. 调用ConstructEid
 * 预期结果
 * 1. 构造出的EID与原始EID一致
 */
TEST_F(TestUbseMtiEidInterface, ConstructEid_Normal)
{
    std::string baseEid = "4245:4944:0000:0000:0000:0000:0100:0000";
    std::string bitStr;
    ParseBaseEid(baseEid, bitStr);
    std::string eid;
    ConstructEid(bitStr, eid);
    EXPECT_EQ(eid, baseEid);
}
TEST_F(TestUbseMtiEidInterface, ConstructEid_EmptyBitStr)
{
    std::string bitStr;
    std::string eid;
    ConstructEid(bitStr, eid);
    EXPECT_TRUE(eid.empty());
}

/*
 * 用例描述
 * 测试OverwriteEid(serverIdx)正常重写
 * 测试步骤
 * 1. 传入合法的serverIdx和baseEid
 * 2. 调用OverwriteEid
 * 预期结果
 * 1. 返回UBSE_OK
 * 2. result非空且与baseEid相同
 */
TEST_F(TestUbseMtiEidInterface, OverwriteEid_ServerIdx_Normal)
{
    std::string baseEid = "0000:0000:0004:0200:0010:0000:1404:0500";
    std::string result;
    auto ret = OverwriteEid(8, baseEid, result);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(result, baseEid);
}

TEST_F(TestUbseMtiEidInterface, OverwriteEid_ServerIdx_DifferentResult)
{
    std::string baseEid = "0000:0000:0004:0200:0010:0000:1404:0500";
    std::string result1;
    std::string result2;
    std::string result3;
    auto ret1 = OverwriteEid(0, baseEid, result1);
    auto ret2 = OverwriteEid(11, baseEid, result2);
    auto ret3 = OverwriteEid(511, baseEid, result3);
    EXPECT_EQ(ret1, UBSE_OK);
    EXPECT_EQ(ret2, UBSE_OK);
    EXPECT_EQ(ret3, UBSE_OK);
    EXPECT_EQ(result1, "0000:0000:0004:0200:0010:0000:1400:0500");
    EXPECT_EQ(result2, "0000:0000:0004:0200:0010:0000:1405:8500");
    EXPECT_EQ(result3, "0000:0000:0004:0200:0010:0000:14FF:8500");
}

TEST_F(TestUbseMtiEidInterface, OverwriteEid_ServerIdx_InvalidBaseEid)
{
    std::string baseEid = "invalid";
    std::string result;
    auto ret = OverwriteEid(0, baseEid, result);
    EXPECT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::utils
