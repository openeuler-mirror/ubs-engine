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

#include "test_ubse_lcne_cna_seg_rule.h"

#include <mockcpp/mokc.h>

#include <cstdint>
#include <tuple>
#include <vector>

#include "ubse_error.h"
#include "lcne/ubse_lcne_cna_seg_rule.h"
#include "src/framework/http/ubse_http_module.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::utils;
using namespace ubse::http;
using namespace ubse::common::def;

// 含 group-id 的接口返回样例
std::string responseXml = R"(<lingqu-topology xmlns="urn:huawei:yang:huawei-lingqu-topology">
  <address-segment-rule>
    <group-id>
      <start-bit>20</start-bit>
      <end-bit>23</end-bit>
      <bit-count>4</bit-count>
    </group-id>
    <plane-id>
      <start-bit>16</start-bit>
      <end-bit>19</end-bit>
      <bit-count>4</bit-count>
    </plane-id>
    <server-index>
      <start-bit>7</start-bit>
      <end-bit>15</end-bit>
      <bit-count>9</bit-count>
    </server-index>
    <internal-address>
      <start-bit>0</start-bit>
      <end-bit>6</end-bit>
      <bit-count>7</bit-count>
    </internal-address>
  </address-segment-rule>
</lingqu-topology>)";

// 不含 group-id 的接口返回样例
std::string responseXml2 = R"(<lingqu-topology xmlns="urn:huawei:yang:huawei-lingqu-topology">
  <address-segment-rule>
    <plane-id>
      <start-bit>20</start-bit>
      <end-bit>23</end-bit>
      <bit-count>4</bit-count>
    </plane-id>
    <server-index>
      <start-bit>7</start-bit>
      <end-bit>19</end-bit>
      <bit-count>13</bit-count>
    </server-index>
    <internal-address>
      <start-bit>0</start-bit>
      <end-bit>6</end-bit>
      <bit-count>7</bit-count>
    </internal-address>
  </address-segment-rule>
</lingqu-topology>)";

void TestUbseLcneCnaSegRule::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneCnaSegRule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// 用例：解析含 group-id 与 server-index 的响应，应得到 2 条位段规则
// 步骤：调用 ParseCnaRule 解析 responseXml
// 预期：返回 UBSE_OK，位段为 server-index{7,15,0}、group-id{20,23,1}
TEST_F(TestUbseLcneCnaSegRule, ParseCnaRuleSuccess)
{
    std::vector<CnaBitRange> cnaRules;
    UbseResult ret = UbseLcneCnaSegRule::GetInstance().ParseCnaRule(responseXml, cnaRules);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(2U, cnaRules.size());
    EXPECT_EQ(7, static_cast<int>(std::get<0>(cnaRules[0])));
    EXPECT_EQ(15, static_cast<int>(std::get<1>(cnaRules[0])));
    EXPECT_EQ(0, static_cast<int>(std::get<2>(cnaRules[0])));
    EXPECT_EQ(20, static_cast<int>(std::get<0>(cnaRules[1])));
    EXPECT_EQ(23, static_cast<int>(std::get<1>(cnaRules[1])));
    EXPECT_EQ(1, static_cast<int>(std::get<2>(cnaRules[1])));
}

// 用例：解析不含 group-id 的响应，应只得到 server-index 的 1 条位段规则
// 步骤：调用 ParseCnaRule 解析 responseXml2
// 预期：返回 UBSE_OK，位段为 server-index{7,19,0}
TEST_F(TestUbseLcneCnaSegRule, ParseCnaRuleWithoutGroupIdSuccess)
{
    std::vector<CnaBitRange> cnaRules;
    UbseResult ret = UbseLcneCnaSegRule::GetInstance().ParseCnaRule(responseXml2, cnaRules);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(1U, cnaRules.size());
    EXPECT_EQ(7, static_cast<int>(std::get<0>(cnaRules[0])));
    EXPECT_EQ(19, static_cast<int>(std::get<1>(cnaRules[0])));
    EXPECT_EQ(0, static_cast<int>(std::get<2>(cnaRules[0])));
}

// 用例：空响应解析失败
TEST_F(TestUbseLcneCnaSegRule, ParseCnaRuleEmptyFailed)
{
    std::vector<CnaBitRange> cnaRules;
    UbseResult ret = UbseLcneCnaSegRule::GetInstance().ParseCnaRule("", cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：缺少 address-segment-rule 节点解析失败
TEST_F(TestUbseLcneCnaSegRule, ParseCnaRuleMissingRuleFailed)
{
    std::string xml = R"(<lingqu-topology xmlns="urn:huawei:yang:huawei-lingqu-topology"></lingqu-topology>)";
    std::vector<CnaBitRange> cnaRules;
    UbseResult ret = UbseLcneCnaSegRule::GetInstance().ParseCnaRule(xml, cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：缺少必选 server-index 节点解析失败
TEST_F(TestUbseLcneCnaSegRule, ParseCnaRuleMissingServerIndexFailed)
{
    std::string xml = R"(<lingqu-topology xmlns="urn:huawei:yang:huawei-lingqu-topology">
  <address-segment-rule>
    <group-id><start-bit>20</start-bit><end-bit>23</end-bit><bit-count>4</bit-count></group-id>
  </address-segment-rule>
</lingqu-topology>)";
    std::vector<CnaBitRange> cnaRules;
    UbseResult ret = UbseLcneCnaSegRule::GetInstance().ParseCnaRule(xml, cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：start-bit 为非数字解析失败
TEST_F(TestUbseLcneCnaSegRule, ParseCnaRuleInvalidNumberFailed)
{
    std::string xml = R"(<lingqu-topology xmlns="urn:huawei:yang:huawei-lingqu-topology">
  <address-segment-rule>
    <server-index><start-bit>abc</start-bit><end-bit>15</end-bit><bit-count>9</bit-count></server-index>
  </address-segment-rule>
</lingqu-topology>)";
    std::vector<CnaBitRange> cnaRules;
    UbseResult ret = UbseLcneCnaSegRule::GetInstance().ParseCnaRule(xml, cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：查询成功，HttpSend 返回正常且响应含 group-id 与 server-index
// 步骤：打桩 HttpSend 回填含 group-id 的响应，调用 QueryCnaRule
// 预期：返回 UBSE_OK，位段为 server-index{7,15,0}、group-id{20,23,1}
TEST_F(TestUbseLcneCnaSegRule, QueryCnaRuleSuccess)
{
    std::vector<CnaBitRange> cnaRules;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = responseXml;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneCnaSegRule::GetInstance().QueryCnaRule(cnaRules);
    EXPECT_EQ(UBSE_OK, ret);
    ASSERT_EQ(2U, cnaRules.size());
    EXPECT_EQ(7, static_cast<int>(std::get<0>(cnaRules[0])));
    EXPECT_EQ(15, static_cast<int>(std::get<1>(cnaRules[0])));
    EXPECT_EQ(0, static_cast<int>(std::get<2>(cnaRules[0])));
    EXPECT_EQ(20, static_cast<int>(std::get<0>(cnaRules[1])));
    EXPECT_EQ(23, static_cast<int>(std::get<1>(cnaRules[1])));
    EXPECT_EQ(1, static_cast<int>(std::get<2>(cnaRules[1])));
}

// 用例：HttpSend 失败
TEST_F(TestUbseLcneCnaSegRule, QueryCnaRule_HttpSendFailed)
{
    std::vector<CnaBitRange> cnaRules;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneCnaSegRule::GetInstance().QueryCnaRule(cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：HTTP 状态码非 200
TEST_F(TestUbseLcneCnaSegRule, QueryCnaRule_RspStatusFailed)
{
    std::vector<CnaBitRange> cnaRules;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneCnaSegRule::GetInstance().QueryCnaRule(cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：响应体为空
TEST_F(TestUbseLcneCnaSegRule, QueryCnaRule_RspEmpty)
{
    std::vector<CnaBitRange> cnaRules;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneCnaSegRule::GetInstance().QueryCnaRule(cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：解析响应失败
TEST_F(TestUbseLcneCnaSegRule, QueryCnaRule_ParseFailed)
{
    std::vector<CnaBitRange> cnaRules;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneCnaSegRule::ParseCnaRule;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneCnaSegRule::GetInstance().QueryCnaRule(cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}

// 用例：解析得到空位段，校验失败
TEST_F(TestUbseLcneCnaSegRule, QueryCnaRule_ValidFailed)
{
    std::vector<CnaBitRange> cnaRules;
    std::vector<CnaBitRange> parsedRules;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneCnaSegRule::ParseCnaRule;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(parsedRules)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneCnaSegRule::GetInstance().QueryCnaRule(cnaRules);
    EXPECT_EQ(UBSE_ERROR, ret);
}
} // namespace ubse::ut::lcne
