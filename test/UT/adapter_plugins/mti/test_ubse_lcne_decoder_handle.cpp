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

#include "test_ubse_lcne_decoder_handle.h"

#include <mockcpp/mockcpp.hpp>

#include "lcne/ubse_lcne_decoder_handle.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "src/framework/http/ubse_http_module.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;

void TestUbseLcneDecoderHandle ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneDecoderHandle ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesSuccess)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::string body = R"({
  "huawei-vbussw-service:ub-memory-handle": {
    "huawei-vbussw-service:output": {
      "ub-memory-handles": {
        "ub-memory-handle": [
          {
            "handle": "2",
            "hpa": "8796093022208",
            "size": "134217728",
            "entry-start-index": 0,
            "entry-end-index": 0,
            "block-start-index": 0,
            "block-end-index": 0,
            "type": 0
          },
          {
            "handle": "3",
            "hpa": "8796227239936",
            "size": "268435456",
            "entry-start-index": 1,
            "entry-end-index": 1,
            "block-start-index": 1,
            "block-end-index": 1,
            "type": 1
          }
        ]
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesEmptySuccess)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::string body = R"({
  "huawei-vbussw-service:ub-memory-handle": {
    "huawei-vbussw-service:output": {
      "ub-memory-handles": {
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesGetInfoFailed)
{
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesHttpSendFailed)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).stubs().will(returnValue(UBSE_ERROR));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesResponseFailed)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::string body = "";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).stubs().will(returnValue(UBSE_OK));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesResponseHasParseError)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::string body = R"({
  "huawei-vbussw-service:ub-memory-handle": {{{{
    "huawei-vbussw-service:output": {
      "ub-memory-handles": {
        "ub-memory-handle": [
          {
            "handle": "2",
            "hpa": "8796093022208",
            "size": "134217728",
            "entry-start-index": 0,
            "entry-end-index": 0,
            "block-start-index": 0,
            "block-end-index": 0,
            "type": 0
          }
        ]
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesResponseKeyParseFailed)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::string body = R"({
  "huawei-vbussw-service:ub-memory-handle123": {
    "huawei-vbussw-service:output": {
      "ub-memory-handles": {
        "ub-memory-handle": [
          {
            "handle": "2"
          }
        ]
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);

    body = R"({
  "huawei-vbussw-service:ub-memory-handle": {
    "huawei-vbussw-service:output123": {
      "ub-memory-handles": {
        "ub-memory-handle": [
          {
            "handle": "2"
          }
        ]
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).reset();
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneDecoderHandle, GetAllMemHandlesResponseArrayParseFailed)
{
    election::UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeId = "1";
    MOCKER_CPP(&election::UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::string body = R"({
  "huawei-vbussw-service:ub-memory-handle": {
    "huawei-vbussw-service:output": {
      "ub-memory-handles123": {
        "ub-memory-handle": [
          {
            "handle": "2"
          }
        ]
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));

    UbseMamiMemHandleQueryInfo queryInfo{};
    std::vector<UbseMamiMemHandleValue> handleValues;
    auto ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);

    body = R"({
  "huawei-vbussw-service:ub-memory-handle": {
    "huawei-vbussw-service:output": {
      "ub-memory-handles": {
        "ub-memory-123": 2
      }
    }
  }
})";
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest).reset();
    MOCKER_CPP(&UbseHttpModule::UbseHttpPostJsonRequest)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(body))
        .will(returnValue(UBSE_OK));
    ret = UbseLcneDecoderHandle::GetInstance().GetAllMemHandles(queryInfo, handleValues);
    EXPECT_EQ(ret, UBSE_ERROR);
}
}