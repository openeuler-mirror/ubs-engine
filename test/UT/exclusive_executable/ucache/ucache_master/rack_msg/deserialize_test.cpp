/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <regex>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include <regex>
#include "ubse_com.h"
#include "ubse_error.h"
#include "deserialize.h"
#include "securec.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_json_util.h"
#include "ucache_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
using namespace std;
using namespace turbo::ucache;
using namespace ubse::log;
using namespace ucache::deserialize;
using namespace ubse::com;
using namespace ucache;
using namespace ucache::serialize;

class DeserializeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

namespace ucache::deserialize {
uint32_t GetCgroupInfoInNode(const std::string& nodeId, std::map<std::string, CgroupInfos>& dockerInfos,
                             std::map<std::string, uint64_t>& timeStamps);
uint32_t DispatchCollectTask(ResourceQueryType qType, const std::string& nodeId, TaskResponse& tResp);
} // namespace ucache::deserialize

TEST_F(DeserializeTest, GetCgroupInfoInNode_TEST)
{
    std::string nodeId = "0";
    std::map<std::string, CgroupInfos> dockerInfos;
    std::map<std::string, uint64_t> timeStamps;

    uint32_t ret = GetCgroupInfoInNode(nodeId, dockerInfos, timeStamps);
    EXPECT_EQ(ret, ucache::UCACHE_ERR);

    TaskResponse tResp{};
    tResp.resCode = UCACHE_OK;
    CgroupInfos cgInfo = {};
    dockerInfos["cf94cef85795b3872d429d117924faaf89b3e7c9fe01a848c28e97b84ea18a41"] = cgInfo;
    UCacheOutStream out{};
    out << dockerInfos;
    tResp.payload = out.Str();
    MOCKER(DispatchCollectTask)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(tResp))
        .will(returnValue(ucache::UCACHE_OK));
    ret = GetCgroupInfoInNode(nodeId, dockerInfos, timeStamps);
    EXPECT_EQ(ret, ucache::UCACHE_OK);
}

TEST_F(DeserializeTest, ParseBorrowLendMemInfoTest)
{
    Deserialize deserialize;
    std::vector<UbseNumaMemoryDebtInfo> extInfos;
    UbseNumaMemoryDebtInfo extInfo1;
    extInfo1.name = "Resource1";
    extInfo1.size = 1024;
    extInfo1.lentNodeId = "Node1";
    extInfo1.lentNumaIdList = {1, 2};
    extInfo1.lentNumaSizeList = {512, 512};
    extInfo1.lentMemId = {1001, 1002};
    extInfo1.borrowNodeId = "Node2";
    extInfo1.remoteNumaId = 3;
    extInfo1.borrowMemId = {2001, 2002};
    memcpy_s(extInfo1.usrInfo, sizeof(extInfo1.usrInfo), "\x01\x02\x03\x04", 4);
    extInfos.push_back(extInfo1);

    uint32_t res = deserialize.ParseBorrowLendMemInfo(extInfos);
    EXPECT_EQ(res, ucache::UCACHE_OK);

    MOCKER(memcpy_s).stubs().will(returnValue(1));
    res = deserialize.ParseBorrowLendMemInfo(extInfos);
    EXPECT_EQ(res, ucache::EXEC_MEM_BORROW_ERROR);
}

TEST_F(DeserializeTest, GetNumaInfos_TEST)
{
    Deserialize deserialize;
    std::map<std::string, std::map<std::string, NodeInfo>> nodeInfos;

    std::vector<std::string> emptyNodeIds = {};
    UcacheConfig::GetInstance().SetNodeIds(emptyNodeIds);
    int ret = deserialize.GetNumaInfos(nodeInfos);
    EXPECT_EQ(ret, ucache::UCACHE_OK);

    std::vector<std::string> newNodeIds = {"Node0", "Node1"};
    UcacheConfig::GetInstance().SetNodeIds(newNodeIds);
    ret = deserialize.GetNumaInfos(nodeInfos);
    EXPECT_EQ(ret, ucache::UCACHE_ERR);

    TaskResponse tResp{};
    tResp.resCode = UCACHE_OK;
    NodeInfo numainfo{};
    std::map<std::string, NodeInfo> numaInfos{};
    numaInfos["node0"] = numainfo;
    UCacheOutStream out{};
    out << numaInfos;
    tResp.payload = out.Str();
    MOCKER(DispatchCollectTask)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(tResp))
        .will(returnValue(ucache::UCACHE_OK));
    ret = deserialize.GetNumaInfos(nodeInfos);
    EXPECT_EQ(ret, ucache::UCACHE_OK);
}

TEST_F(DeserializeTest, GetMemWaterMark_TEST)
{
    Deserialize deserialize;
    std::map<std::string, uint64_t> memWaterMarkInfos;

    std::vector<std::string> emptyNodeIds = {};
    UcacheConfig::GetInstance().SetNodeIds(emptyNodeIds);
    int ret = deserialize.GetMemWaterMark(memWaterMarkInfos);
    EXPECT_EQ(ret, ucache::UCACHE_OK);

    std::vector<std::string> newNodeIds = {"Node0", "Node1"};
    UcacheConfig::GetInstance().SetNodeIds(newNodeIds);
    ret = deserialize.GetMemWaterMark(memWaterMarkInfos);
    EXPECT_EQ(ret, ucache::UCACHE_ERR);

    TaskResponse tResp{};
    tResp.resCode = UCACHE_OK;
    MemWatermarkInfo memWaterMark{};
    UCacheOutStream out{};
    out << memWaterMark;
    tResp.payload = out.Str();
    MOCKER(DispatchCollectTask)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(tResp))
        .will(returnValue(ucache::UCACHE_OK));
    ret = deserialize.GetMemWaterMark(memWaterMarkInfos);
    EXPECT_EQ(ret, ucache::UCACHE_OK);
}

TEST_F(DeserializeTest, GetBorrowMemInfoTest)
{
    Deserialize deserialize;
    std::string nodeId = "Node0";
    std::vector<BorrowMemInfo> borrowMemInfos;

    MOCKER(UbseGetNumaMemDebtInfoWithNode).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    uint32_t res = deserialize.GetBorrowMemInfo(nodeId, borrowMemInfos);
    EXPECT_EQ(res, ucache::UBSE_API_ERROR);

    nodeId = "";
    MOCKER(UbseGetNumaMemDebtInfo).stubs().will(returnValue(UBSE_ERR_INVALID_ARG));
    res = deserialize.GetBorrowMemInfo(nodeId, borrowMemInfos);
    EXPECT_EQ(res, ucache::UBSE_API_ERROR);

    GlobalMockObject::verify();
    res = deserialize.GetBorrowMemInfo(nodeId, borrowMemInfos);
    EXPECT_EQ(res, ucache::UCACHE_OK);
}

TEST_F(DeserializeTest, GetCgroupInfos_TEST)
{
    Deserialize deserialize;
    std::queue<std::map<std::string, uint64_t>> timeStampsCgroup;
    std::queue<std::map<std::string, std::map<std::string, CgroupInfos>>> cgroupInfosQueue;

    std::vector<std::string> emptyNodeIds = {};
    UcacheConfig::GetInstance().SetNodeIds(emptyNodeIds);

    uint32_t ret = deserialize.GetCgroupInfos(timeStampsCgroup, cgroupInfosQueue);
    EXPECT_EQ(ret, ucache::UCACHE_OK);

    std::vector<std::string> newNodeIds = {"Node0", "Node1"};
    UcacheConfig::GetInstance().SetNodeIds(newNodeIds);
    ret = deserialize.GetCgroupInfos(timeStampsCgroup, cgroupInfosQueue);
    EXPECT_EQ(ret, ucache::UCACHE_ERR);

    std::map<std::string, std::map<std::string, CgroupInfos>> cgroupInfo1;
    std::map<std::string, std::map<std::string, CgroupInfos>> cgroupInfo2;
    cgroupInfosQueue.push(cgroupInfo1);
    cgroupInfosQueue.push(cgroupInfo2);
    std::map<std::string, uint64_t> timeStamp1;
    timeStampsCgroup.push(timeStamp1);
    MOCKER(GetCgroupInfoInNode).stubs().will(returnValue(ucache::UCACHE_OK));
    ret = deserialize.GetCgroupInfos(timeStampsCgroup, cgroupInfosQueue);
    EXPECT_EQ(ret, ucache::UCACHE_OK);
}

TEST_F(DeserializeTest, ToString_Test)
{
    LentNumaInfo lentNumaInfo = {.numaId = 1, .lentSize = 1024};
    std::string result = lentNumaInfo.ToString();
    EXPECT_EQ(result.find("numaId:[1]"), true);

    BorrowMemInfo borrowMemInfo = {.name = "Node1",
                                   .size = 2048,
                                   .lentNodeId = "Node0",
                                   .lentNumaInfos = {{0, 2048}, {1, 10240}},
                                   .borrowNodeId = "1f476bcc7602ce0cb8f226b8527f9176",
                                   .borrowLocalNuma = 3,
                                   .borrowRemoteNuma = 5,
                                   .lentMemId = {1},
                                   .borrowMemId = {1}};
    result = borrowMemInfo.ToString();
    EXPECT_EQ(result.find("name:[Node1]"), true);
}