/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "securec.h"

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "mp_borrow_conf_util.h"
#undef private
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ubse::nodeController;
namespace mempooling {

class TestMpBorrowConfUtil : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestMpJsonUtil SetUp Begin]" << endl;
        cout << "[TestMpJsonUtil SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMpJsonUtil TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestMpJsonUtil TearDown End]" << endl;
    }
};

TEST_F(TestMpBorrowConfUtil, ParseBorrowConfFailed1)
{
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetMemGroupNodeList,
               uint32_t(*)(ubse::nodeController::UbseNodeController*, UbseMemGroupNodeList & groupList))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    UbseMemGroupNodeList groupList;
    UbseMemProviderNodeList providerList;
    auto res = MpParseGroupProviderConf::Instance().ParseBorrowConf(groupList, providerList);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMpBorrowConfUtil, ParseBorrowConfFailed2)
{
    UbseMemGroupNodeList groupList;
    UbseMemProviderNodeList providerList;
    auto res = MpParseGroupProviderConf::Instance().ParseBorrowConf(groupList, providerList);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMpBorrowConfUtil, ParseBorrowConfSuccess)
{
    auto MakeNode = [](const std::string& id, uint32_t slot, const char* bond, const std::string& host,
                       const std::string& ip) {
        UbseNodeInfo node;
        node.nodeId = id;
        node.slotId = slot;
        strncpy_s(node.bondingEid, sizeof(node.bondingEid) - 1, bond, sizeof(node.bondingEid) - 1);
        node.hostName = host;
        node.comIp = ip;
        return node;
    };
    std::unordered_map<std::string, UbseNodeInfo> maps{};
    maps["1"] = MakeNode("host1", 1, "bond1", "host1", "10.0.0.1");
    maps["2"] = MakeNode("host2", 2, "bond2", "host2", "10.0.0.2");
    MOCKER_CPP(&UbseNodeController::GetAllNodes,
               (std::unordered_map<std::string, UbseNodeInfo>(*)(UbseNodeController*)))
        .stubs()
        .will(returnValue(maps));
    UbseMemGroupNodeList groupList;
    UbseMemProviderNodeList providerList;
    auto res = MpParseGroupProviderConf::Instance().ParseBorrowConf(groupList, providerList);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMpBorrowConfUtil, ParseBorrowConfFailed3)
{
    auto MakeNode = [](const std::string& id, uint32_t slot, const char* bond, const std::string& host,
                       const std::string& ip) {
        UbseNodeInfo node;
        node.nodeId = id;
        node.slotId = slot;
        strncpy_s(node.bondingEid, sizeof(node.bondingEid) - 1, bond, sizeof(node.bondingEid) - 1);
        node.hostName = host;
        node.comIp = ip;
        return node;
    };
    std::unordered_map<std::string, UbseNodeInfo> maps{};
    maps["1"] = MakeNode("host1", 1, "bond1", "host1", "10.0.0.1");
    maps["2"] = MakeNode("host2", 2, "bond2", "host2", "10.0.0.2");
    MOCKER_CPP(&UbseNodeController::GetAllNodes,
               (std::unordered_map<std::string, UbseNodeInfo>(*)(UbseNodeController*)))
        .stubs()
        .will(returnValue(maps));
    MOCKER_CPP(&ubse::nodeController::UbseNodeController::GetMemProviderNodeList,
               uint32_t(*)(ubse::nodeController::UbseNodeController*, UbseMemProviderNodeList & providerList))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    UbseMemGroupNodeList groupList;
    UbseMemProviderNodeList providerList;
    auto res = MpParseGroupProviderConf::Instance().ParseBorrowConf(groupList, providerList);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

MpResult ParseBorrowConfMock1(MpParseGroupProviderConf* This, UbseMemGroupNodeList& groupList,
                              UbseMemProviderNodeList& providerList)
{
    groupList.clear();
    providerList.clear();

    auto MakeNode = [](const std::string& id, uint32_t slot, const char* bond, const std::string& host,
                       const std::string& ip) {
        UbseNodeInfo node;
        node.nodeId = id;
        node.slotId = slot;
        strncpy_s(node.bondingEid, sizeof(node.bondingEid) - 1, bond, sizeof(node.bondingEid) - 1);
        node.hostName = host;
        node.comIp = ip;
        return node;
    };

    // 第一组
    std::vector<UbseNodeInfo> group1;
    group1.push_back(MakeNode("host1", 1, "bond1", "host1", "10.0.0.1"));
    group1.push_back(MakeNode("host2", 2, "bond2", "host2", "10.0.0.2"));
    group1.push_back(MakeNode("host3", 3, "bond3", "host3", "10.0.0.3"));

    // 第二组
    std::vector<UbseNodeInfo> group2;
    group2.push_back(MakeNode("host4", 4, "bond4", "host4", "10.0.0.4"));
    group2.push_back(MakeNode("host5", 5, "bond5", "host5", "10.0.0.5"));
    group2.push_back(MakeNode("host6", 6, "bond6", "host6", "10.0.0.6"));

    groupList.push_back(std::move(group1));
    groupList.push_back(std::move(group2));

    // provider
    providerList.push_back(MakeNode("host1", 1, "bond1", "host1", "10.0.0.1"));
    providerList.push_back(MakeNode("host2", 2, "bond2", "host2", "10.0.0.2"));

    return MEM_POOLING_OK;
}

MpResult ParseBorrowConfMock2(MpParseGroupProviderConf* This, UbseMemGroupNodeList& groupList,
                              UbseMemProviderNodeList& providerList)
{
    groupList.clear();
    providerList.clear();

    auto MakeNode = [](const std::string& id, uint32_t slot, const char* bond, const std::string& host,
                       const std::string& ip) {
        UbseNodeInfo node;
        node.nodeId = id;
        node.slotId = slot;
        strncpy_s(node.bondingEid, sizeof(node.bondingEid) - 1, bond, sizeof(node.bondingEid) - 1);
        node.hostName = host;
        node.comIp = ip;
        return node;
    };

    // 第一组
    std::vector<UbseNodeInfo> group1;
    group1.push_back(MakeNode("host1", 1, "bond1", "host1", "10.0.0.1"));
    group1.push_back(MakeNode("host2", 2, "bond2", "host2", "10.0.0.2"));
    group1.push_back(MakeNode("host3", 3, "bond3", "host3", "10.0.0.3"));

    // 第二组
    std::vector<UbseNodeInfo> group2;
    group2.push_back(MakeNode("host4", 4, "bond4", "host4", "10.0.0.4"));
    group2.push_back(MakeNode("host5", 5, "bond5", "host5", "10.0.0.5"));
    group2.push_back(MakeNode("host6", 6, "bond6", "host6", "10.0.0.6"));

    groupList.push_back(std::move(group1));
    groupList.push_back(std::move(group2));

    return MEM_POOLING_OK;
}

TEST_F(TestMpBorrowConfUtil, BuildBorrowMapSuccess1)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::ParseBorrowConf,
               MpResult(*)(MpParseGroupProviderConf*, UbseMemGroupNodeList & groupList,
                           UbseMemProviderNodeList & providerList))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto res = MpParseGroupProviderConf::Instance().BuildBorrowMap();
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMpBorrowConfUtil, BuildBorrowMapSuccess2)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::ParseBorrowConf,
               MpResult(*)(MpParseGroupProviderConf*, UbseMemGroupNodeList & groupList,
                           UbseMemProviderNodeList & providerList))
        .stubs()
        .will(invoke(ParseBorrowConfMock1));
    auto res = MpParseGroupProviderConf::Instance().BuildBorrowMap();
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMpBorrowConfUtil, BuildBorrowMapSuccess3)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::ParseBorrowConf,
               MpResult(*)(MpParseGroupProviderConf*, UbseMemGroupNodeList & groupList,
                           UbseMemProviderNodeList & providerList))
        .stubs()
        .will(invoke(ParseBorrowConfMock2));
    auto res = MpParseGroupProviderConf::Instance().BuildBorrowMap();
    EXPECT_EQ(res, MEM_POOLING_OK);
}

MpResult BuildBorrowMapMock(MpParseGroupProviderConf* obj)
{
    obj->borrowMap["1"].insert("2");
    return MEM_POOLING_OK;
}

TEST_F(TestMpBorrowConfUtil, GetBorrowableListSuccess)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::BuildBorrowMap, MpResult(*)(MpParseGroupProviderConf*))
        .stubs()
        .will(invoke(BuildBorrowMapMock));
    std::unordered_set<std::string> borrowableNidSet;
    auto res = MpParseGroupProviderConf::Instance().GetBorrowableList("1", borrowableNidSet);
    EXPECT_EQ(res, MEM_POOLING_OK);
}

TEST_F(TestMpBorrowConfUtil, GetBorrowableListFailed1)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::BuildBorrowMap, MpResult(*)(MpParseGroupProviderConf*))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::unordered_set<std::string> borrowableNidSet;
    auto res = MpParseGroupProviderConf::Instance().GetBorrowableList("1", borrowableNidSet);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMpBorrowConfUtil, GetBorrowableListFailed2)
{
    MOCKER_CPP(&mempooling::MpParseGroupProviderConf::BuildBorrowMap, MpResult(*)(MpParseGroupProviderConf*))
        .stubs()
        .will(invoke(BuildBorrowMapMock));
    std::unordered_set<std::string> borrowableNidSet;
    auto res = MpParseGroupProviderConf::Instance().GetBorrowableList("2", borrowableNidSet);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}
} // namespace mempooling