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

#include "test_ubse_node_controller_msg.h"

#include "src/framework/com/ubse_com.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_node_controller_msg.cpp"

namespace ubse::node_controller::ut {
using namespace ubse::com;
using namespace ubse::event;
void TestUbseNodeControllerMsg::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeControllerMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeControllerMsg, RegNodeControllerHandler)
{
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(RegNodeControllerHandler());
}

TEST_F(TestUbseNodeControllerMsg, ReportTopologyHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};

    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(ReportTopologyHandler(req, resp), UBSE_ERROR_NULLPTR);
    MOCKER(&UbseNodeController::UpdateNodeInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(ReportTopologyHandler(req, resp), UBSE_ERROR_NULLPTR);
    MOCKER(UbsePubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ReportTopologyHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMsg, CollectNodeInfoHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};

    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectNodeInfoHandler(req, resp), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(CollectNodeInfoHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMsg, GetAllNodeInfoFromRemoteHandler)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};

    UbseNodeInfo nodeInfo{};
    UbseNodeController::GetInstance().nodeInfos = {{"lnode1", nodeInfo}};

    MOCKER(SerializeUbseNodeList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(GetAllNodeInfoFromRemoteHandler(req, resp), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(GetAllNodeInfoFromRemoteHandler(req, resp), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMsg, ReportUbseNodeInfo)
{
    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(ReportUbseNodeInfo("node1", UbseNodeInfo{}), UBSE_ERROR);
    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(ReportUbseNodeInfo("node1", UbseNodeInfo{}), UBSE_ERROR);
    EXPECT_EQ(ReportUbseNodeInfo("node1", UbseNodeInfo{}), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMsg, CollectRemoteNodeInfoRespHandler)
{
    UbseByteBuffer respData{};
    UbseNodeInfo info{};
    UbseResult collectRet = UBSE_OK;
    EXPECT_NO_THROW(CollectRemoteNodeInfoRespHandler("node1", respData, UBSE_ERROR, info, collectRet));
    EXPECT_EQ(collectRet, UBSE_ERROR);

    EXPECT_NO_THROW(CollectRemoteNodeInfoRespHandler("node1", respData, UBSE_OK, info, collectRet));
    EXPECT_EQ(collectRet, UBSE_ERROR_NULLPTR);

    respData.data = new uint8_t[1];
    respData.len = 1;
    MOCKER(DeSerializeUbseNode).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(CollectRemoteNodeInfoRespHandler("node1", respData, UBSE_OK, info, collectRet));
    EXPECT_EQ(collectRet, UBSE_OK);
    delete[] respData.data;
}

TEST_F(TestUbseNodeControllerMsg, CollectRemoteNodeInfo)
{
    UbseNodeInfo info{};
    MOCKER(SerializeUbseNode).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectRemoteNodeInfo("node1", info), UBSE_ERROR);

    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectRemoteNodeInfo("node1", info), UBSE_ERROR);
    EXPECT_EQ(CollectRemoteNodeInfo("node1", info), UBSE_OK);
}

TEST_F(TestUbseNodeControllerMsg, GetAllNodeInfoFromRemoteRespHandler)
{
    UbseByteBuffer respData{};
    std::vector<UbseNodeInfo> infos{};
    UbseResult getRet = UBSE_OK;

    EXPECT_NO_THROW(GetAllNodeInfoFromRemoteRespHandler("node1", respData, UBSE_ERROR, infos, getRet));
    EXPECT_EQ(getRet, UBSE_ERROR);

    EXPECT_NO_THROW(GetAllNodeInfoFromRemoteRespHandler("node1", respData, UBSE_OK, infos, getRet));
    EXPECT_EQ(getRet, UBSE_ERROR_NULLPTR);

    respData.data = new uint8_t[1];
    respData.len = 1;
    MOCKER(DeSerializeUbseNodeList).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(GetAllNodeInfoFromRemoteRespHandler("node1", respData, UBSE_OK, infos, getRet));
    EXPECT_EQ(getRet, UBSE_OK);
    delete[] respData.data;
}

TEST_F(TestUbseNodeControllerMsg, GetAllNodeInfoFromRemote)
{
    std::vector<UbseNodeInfo> infos{};
    MOCKER(SerializeUbseNodeList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(GetAllNodeInfoFromRemote("node1", infos), UBSE_ERROR);

    MOCKER(UbseRpcSend).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(GetAllNodeInfoFromRemote("node1", infos), UBSE_ERROR);
    EXPECT_EQ(GetAllNodeInfoFromRemote("node1", infos), UBSE_OK);
}
} // namespace ubse::node_controller::ut