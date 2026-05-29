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
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "mp_error.h"
#include "mp_memory_info.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
using namespace std;

namespace mempooling {

class TestMpMemoryInfo : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestMpMemoryInfo SetUp Begin]" << endl;
        cout << "[TestMpMemoryInfo SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestMpMemoryInfo TearDown Begin]" << endl;

        GlobalMockObject::verify();

        cout << "[TestMpMemoryInfo TearDown End]" << endl;
    }
};

// 测试空字符串输入
TEST_F(TestMpMemoryInfo, StringToKBEmptyInput)
{
    std::string str;
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 0) << "Empty string should return 0";
}

// 测试无单位的数值输入
TEST_F(TestMpMemoryInfo, StringToKBNoUnit)
{
    std::string str = "123";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 0) << "String without unit should return 0";
}

// 测试有效单位的输入 - KB
TEST_F(TestMpMemoryInfo, StringToKBValidUnitKB)
{
    std::string str = "100KB";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 100) << "100KB should return 100";
}

// 测试有效单位的输入 - MB
TEST_F(TestMpMemoryInfo, StringToKBValidUnitMB)
{
    std::string str = "50MB";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 50 * 1024) << "50MB should return 51200";
}

// 测试有效单位的输入 - GB
TEST_F(TestMpMemoryInfo, StringToKBValidUnitGB)
{
    std::string str = "2GB";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 2 * 1024 * 1024) << "2GB should return 2097152";
}

// 测试有效单位的输入 - TB
TEST_F(TestMpMemoryInfo, StringToKBValidUnitTB)
{
    std::string str = "1TB";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 1 * 1024 * 1024 * 1024) << "1TB should return 1073741824";
}

// 测试无效单位的输入
TEST_F(TestMpMemoryInfo, StringToKBInvalidUnit)
{
    std::string str = "100XX";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 0) << "Invalid unit should return 0";
}

// 测试数值包含小数点的情况
TEST_F(TestMpMemoryInfo, StringToKBWithDecimal)
{
    std::string str = "12.5KB";
    uint64_t res = mempooling::StringToKB(str);
    EXPECT_EQ(res, 12) << "12.5KB should return 12";
}

// 测试正常情况：所有字段都存在且值有效
TEST_F(TestMpMemoryInfo, ParseNodeMemoryInfoMap_Success)
{
    // 构造一个有效的JSON字符串
    string jsonStr = R"({
        "timestamp": "1625097600",
        "nodeId": "node001",
        "totalMemory": "8192MB",
        "usedMemory": "4096MB",
        "freeMemory": "4096MB",
        "borrowedMemory": "0MB",
        "lentMemory": "0MB",
        "numaMemInfo": {},
        "borrowedAndLentInfo": {}
    })";

    NodeMemoryInfo nodeMemoryInfoList;
    JSON_VEC nodeMemoryInfoListVec;
    nodeMemoryInfoListVec.push_back(jsonStr);

    JSON_MAP nodeMemoryInfoMap;
    nodeMemoryInfoMap["timestamp"] = "1625097600";
    nodeMemoryInfoMap["totalMemory"] = "8192MB";
    nodeMemoryInfoMap["usedMemory"] = "4096MB";
    nodeMemoryInfoMap["freeMemory"] = "4096MB";
    nodeMemoryInfoMap["borrowedMemory"] = "0MB";
    nodeMemoryInfoMap["lentMemory"] = "0MB";

    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    obj.ParseNodeMemoryInfoMap(nodeMemoryInfoListVec, 0, nodeMemoryInfoMap);

    // 验证nodeMemoryInfoList中的值
    EXPECT_EQ(static_cast<time_t>(1625097600), obj.nodeMemoryInfoList[0].timestamp);
    EXPECT_EQ("node001", obj.nodeMemoryInfoList[0].nodeId);
    EXPECT_EQ(static_cast<uint64_t>(8192 * 1024), obj.nodeMemoryInfoList[0].totalMemory);
    EXPECT_EQ(static_cast<uint64_t>(4096 * 1024), obj.nodeMemoryInfoList[0].usedMemory);
    EXPECT_EQ(static_cast<uint64_t>(4096 * 1024), obj.nodeMemoryInfoList[0].freeMemory);
}

TEST_F(TestMpMemoryInfo, ParseNodeMemoryInfoMap_Failed)
{
    // 构造一个有效的JSON字符串
    string jsonStr = R"({
        "timestamp": "1625097600",
        "nodeId": "node001",
        "totalMemory": "8192MB",
        "usedMemory": "4096MB",
        "freeMemory": "4096MB",
        "borrowedMemory": "0MB",
        "lentMemory": "0MB",
        "numaMemInfo": {},
        "borrowedAndLentInfo": {}
    })";

    NodeMemoryInfo nodeMemoryInfoList;
    JSON_VEC nodeMemoryInfoListVec;
    nodeMemoryInfoListVec.push_back(jsonStr);

    JSON_MAP nodeMemoryInfoMap;
    nodeMemoryInfoMap["timestamp"] = "1625097600";
    nodeMemoryInfoMap["totalMemory"] = "8192MB";
    nodeMemoryInfoMap["usedMemory"] = "4096MB";
    nodeMemoryInfoMap["freeMemory"] = "4096MB";
    nodeMemoryInfoMap["borrowedMemory"] = "0MB";
    nodeMemoryInfoMap["lentMemory"] = "0MB";

    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(false));
    bool res = obj.ParseNodeMemoryInfoMap(nodeMemoryInfoListVec, 0, nodeMemoryInfoMap);
    EXPECT_EQ(res, 1);
}

TEST_F(TestMpMemoryInfo, ParseNumaMemInfoMap_Success)
{
    JSON_VEC numaMemInfoVec;
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(true));
    JSON_MAP numaMemInfoMap;
    numaMemInfoMap["numaId"] = "1";
    numaMemInfoMap["socketId"] = "0";
    numaMemInfoMap["memTotal"] = "8192MB";
    numaMemInfoMap["memFree"] = "4096MB";
    numaMemInfoMap["memUsed"] = "4096MB";
    numaMemInfoMap["vmMemTotal"] = "0MB";
    numaMemInfoMap["vmMemFree"] = "0MB";
    numaMemInfoMap["vmMemUsed"] = "4096MB";
    numaMemInfoMap["reservedMem"] = "4096MB";
    numaMemInfoMap["lentMem"] = "0MB";
    numaMemInfoMap["sharedMem"] = "0MB";

    NodeMemoryInfoList obj;
    NodeMemoryInfo nodeMemoryInfo;
    RackNumaMemInfo numaMemInfo = {};
    nodeMemoryInfo.numaMemInfo.push_back(numaMemInfo);
    obj.nodeMemoryInfoList.push_back(nodeMemoryInfo);
    obj.ParseNumaMemInfoMap(numaMemInfoVec, 0, 0, numaMemInfoMap);
    // 验证numaMemInfo中的值
    EXPECT_EQ(static_cast<uint16_t>(1), obj.nodeMemoryInfoList[0].numaMemInfo[0].numaId);
    EXPECT_EQ(static_cast<uint64_t>(8192 * 1024), obj.nodeMemoryInfoList[0].numaMemInfo[0].memTotal);
    EXPECT_EQ(static_cast<uint64_t>(4096 * 1024), obj.nodeMemoryInfoList[0].numaMemInfo[0].memFree);
    EXPECT_EQ(static_cast<uint64_t>(4096 * 1024), obj.nodeMemoryInfoList[0].numaMemInfo[0].memUsed);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].vmMemTotal);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].vmMemFree);
    EXPECT_EQ(static_cast<uint64_t>(4096 * 1024), obj.nodeMemoryInfoList[0].numaMemInfo[0].vmUsedMem);
    EXPECT_EQ(static_cast<uint64_t>(4096 * 1024), obj.nodeMemoryInfoList[0].numaMemInfo[0].reservedMem);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].lentMem);
    EXPECT_EQ(static_cast<uint64_t>(0), obj.nodeMemoryInfoList[0].numaMemInfo[0].sharedMem);
}

TEST_F(TestMpMemoryInfo, ParseNumaMemInfoMap_Failed)
{
    JSON_VEC numaMemInfoVec;
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(false));
    JSON_MAP numaMemInfoMap;
    numaMemInfoMap["numaId"] = "1";
    numaMemInfoMap["socketId"] = "0";
    numaMemInfoMap["memTotal"] = "8192MB";
    numaMemInfoMap["memFree"] = "4096MB";
    numaMemInfoMap["memUsed"] = "4096MB";
    numaMemInfoMap["vmMemTotal"] = "0MB";
    numaMemInfoMap["vmMemFree"] = "0MB";
    numaMemInfoMap["vmMemUsed"] = "4096MB";
    numaMemInfoMap["reservedMem"] = "4096MB";
    numaMemInfoMap["lentMem"] = "0MB";
    numaMemInfoMap["sharedMem"] = "0MB";

    NodeMemoryInfoList obj;
    NodeMemoryInfo nodeMemoryInfo;
    RackNumaMemInfo numaMemInfo = {};
    nodeMemoryInfo.numaMemInfo.push_back(numaMemInfo);
    obj.nodeMemoryInfoList.push_back(nodeMemoryInfo);
    bool res = obj.ParseNumaMemInfoMap(numaMemInfoVec, 0, 0, numaMemInfoMap);
    EXPECT_EQ(res, MEM_POOLING_ERROR);
}

TEST_F(TestMpMemoryInfo, CreateNodeMemoryInfoListVec_Success)
{
    std::string jsonString;
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;

    nodeMemoryInfoListMAP["nodeMemoryInfoList"] = "1";

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR& jsonStr, JSON_VEC& strVec))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    ASSERT_EQ(res, 0);
}

TEST_F(TestMpMemoryInfo, CreateNodeMemoryInfoListVec_Failed1)
{
    std::string jsonString;
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;

    nodeMemoryInfoListMAP["nodeMemoryInfoList"] = "1";

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(false));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    ASSERT_EQ(res, 1);
}

TEST_F(TestMpMemoryInfo, CreateNodeMemoryInfoListVec_Failed2)
{
    std::string jsonString;
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;

    nodeMemoryInfoListMAP["nodeMemoryInfoList"] = "1";

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Map, bool (*)(const JSON_STR& jsonStr, JSON_MAP& strMap))
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR& jsonStr, JSON_VEC& strVec))
        .stubs()
        .will(returnValue(false));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    ASSERT_EQ(res, 1);
}

TEST_F(TestMpMemoryInfo, CreateNumaMemInfoVec_Success)
{
    JSON_VEC numaMemInfoVec;
    JSON_MAP nodeMemoryInfoMap;
    numaMemInfoVec.push_back("1");
    nodeMemoryInfoMap["numaMemInfo"] = "1";
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR& jsonStr, JSON_VEC& strVec))
        .stubs()
        .will(returnValue(true));
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    auto res = obj.CreateNumaMemInfoVec(numaMemInfoVec, 0, nodeMemoryInfoMap);
    ASSERT_EQ(res, true);
}

TEST_F(TestMpMemoryInfo, FromJson_Failed1)
{
    std::string jsonString;
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    MOCKER_CPP(
        &mempooling::NodeMemoryInfoList::CreateNodeMemoryInfoListVec,
        bool (*)(const std::string& jsonString, JSON_MAP& nodeMemoryInfoListMAP, JSON_VEC& nodeMemoryInfoListVec))
        .stubs()
        .will(returnValue(true));
    obj.FromJson(jsonString);
}

TEST_F(TestMpMemoryInfo, FromJson_Succeed1)
{
    std::string jsonString = R"({
    "nodeMemoryInfoList": [
        {
            "timestamp": "1744025731",
            "nodeId": "Node0",
            "totalMemory": "942.2GB",
            "usedMemory": "69.6GB",
            "freeMemory": "872.6GB",
            "borrowedMemory": "4.0GB",
            "lentMemory": "0.0GB",
            "numaMemInfo": [
                {
                    "numaId": 0,
                    "socketId": 0,
                    "memTotal": "251.7GB",
                    "memFree": "243.2GB",
                    "memUsed": "8.6GB",
                    "memUsageRate": "3.4%",
                    "vmMemTotal": "0MB",
                    "vmMemFree": "0MB",
                    "vmMemUsed": "0MB",
                    "vmMemUsageRate": "0.0%",
                    "reservedMem": "62.9GB",
                    "lentMem": "0.0GB",
                    "sharedMem": "0.0GB"
                }
            ],
            "borrowedAndLentInfo": {
                "borrowedItem": [
                    {
                        "nodeId": "Node1",
                        "numaId": 2,
                        "memory": "4.1GB"
                    }
                ],
                "lentItem": [
                ]
            }
        }
    ]
})";
    NodeMemoryInfoList obj;
    obj.nodeMemoryInfoList.push_back({});
    obj.FromJson(jsonString);
}

TEST_F(TestMpMemoryInfo, ToString_Succeed1)
{
    NodeMemoryInfoWithReservedMem obj;
    obj.reservedMem = 1024;
    obj.timestamp = std::time(nullptr);
    obj.ToString();
}

} // namespace mempooling