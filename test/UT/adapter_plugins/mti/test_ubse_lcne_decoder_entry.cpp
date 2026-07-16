/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_lcne_decoder_entry.h"

#include <mockcpp/mockcpp.hpp>

#include "lcne/ubse_lcne_decoder_entry.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "src/framework/http/ubse_http_module.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;

void TestUbseLcneDecoderEntry ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneDecoderEntry ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntrySuccess)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output": {
            "result": "success",
            "hpa": "8796093022208",
            "handle": "514"
            }
        }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    ubseMamiMemImportInfo.marId = 1;
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryGetNodeInfoFailed)
{
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(static_cast<std::shared_ptr<ubse::mti::UbseLcneModule>>(nullptr)));
    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryHttpSendFailed)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).stubs().will(returnValue(UBSE_ERROR));

    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryResponseFailed)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).stubs().will(returnValue(UBSE_OK));

    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryResponseEmpty)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = "";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryHasError)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output": {
            "result": "success",
            "hpa": "12345",
            "handle": "12345"
            }}}}
        }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryParseFieldFailed)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output": {
            "hpa": "12345",
            "handle": "12345"
            }
        }
    })";

    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_NE(ret, UBSE_OK);

    body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output": {
            "result": "success",
            "hpa": "99999999999999999999999999999999999999999999999999999999999999",
            "handle": "12345"
            }
        }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).reset();
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);

    body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output": {
            "result": "success",
            "hpa": "12345",
            "handle": "12345bbb"
            }
        }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).reset();
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryParseKeyFailed)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = R"({
        "huawei-vbussw-service:1234": {
            "huawei-vbussw-service:output": {
            "result": "success",
            "hpa": "12345",
            "handle": "12345"
            }
        }
    })";

    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);

    body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output123": {
            "result": "success",
            "hpa": "99999999999999999999999999999999999999999999999999999999999999",
            "handle": "12345"
            }
        }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).reset();
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderEntry, AddDecoderEntryFailed)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = R"({
        "huawei-vbussw-service:ub-memory-decoder": {
            "huawei-vbussw-service:output": {
            "result": "failed",
            "hpa": "12345",
            "handle": "12345"
            }
        }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemImportInfo ubseMamiMemImportInfo{};
    UbseMamiMemImportResult importResult{};
    auto ret = UbseLcneDecoderEntry::AddDecoderEntry(ubseMamiMemImportInfo, importResult);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderEntry, DeleteDecoderEntrySuccess)
{
    std::shared_ptr<ubse::mti::UbseLcneModule> module = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(module));
    auto getSlotIdFunc = &ubse::mti::UbseLcneModule::GetCurSlotId;
    MOCKER_CPP(getSlotIdFunc).stubs().will(returnValue(std::string("1")));

    std::string body = R"({
       "huawei-vbussw-service:ub-memory-decoder-delete": {
           "huawei-vbussw-service:output": {
           "result": "success"
           }
       }
    })";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemWithdraw drawInfo{};
    auto ret = UbseLcneDecoderEntry::DeleteDecoderEntry(drawInfo);
    EXPECT_EQ(ret, UBSE_OK);
}
}
