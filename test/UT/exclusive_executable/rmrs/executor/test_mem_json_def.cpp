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

#include <iostream>

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "mem_json_def.h"
#include "mp_json_util.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {

class TestMemJsonDef : public ::testing::Test {
protected:
    TestMemJsonDef() {}
    virtual ~TestMemJsonDef() {}
    virtual void SetUp() {}
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_To_Json_Vector2JsonStr_Failed)
{
    RackCreateResourceWaterBorrowAttr attr;

    uint64_t size = 1073741824;

    MemMallocAttr memAttr;
    memAttr.srcNid = "Node0";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;

    RackMemNumaLoc loc;
    loc.nodeId = "Node1";
    loc.socketId = 0;
    loc.numaId = 0;

    memAttr.lenderNumaSize = 1;
    memAttr.lenderLocs.push_back(loc);
    memAttr.lenderSizes.push_back(size);

    attr.size = 1073741824;
    attr.waterMallocAttr = memAttr;

    MOCKER(JsonUtil::RackMemConvertVector2JsonStr).stubs().will(returnValue(false));
    std::string res = attr.ToJson();
    ASSERT_EQ("", res);
}

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_To_Json_Map2JsonStr_Failed)
{
    RackCreateResourceWaterBorrowAttr attr;

    uint64_t size = 1073741824;

    MemMallocAttr memAttr;
    memAttr.srcNid = "Node0";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;

    RackMemNumaLoc loc;
    loc.nodeId = "Node1";
    loc.socketId = 0;
    loc.numaId = 0;

    memAttr.lenderNumaSize = 1;
    memAttr.lenderLocs.push_back(loc);
    memAttr.lenderSizes.push_back(size);

    attr.size = 1073741824;
    attr.waterMallocAttr = memAttr;

    MOCKER(JsonUtil::RackMemConvertMap2JsonStr).stubs().will(returnValue(false));
    std::string res = attr.ToJson();
    ASSERT_EQ("", res);
}

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_To_Json_Map2JsonStr_Success)
{
    RackCreateResourceWaterBorrowAttr attr;

    uint64_t size = 1073741824;

    MemMallocAttr memAttr;
    memAttr.srcNid = "Node0";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;

    RackMemNumaLoc loc;
    loc.nodeId = "Node1";
    loc.socketId = 0;
    loc.numaId = 0;

    memAttr.lenderNumaSize = 1;
    memAttr.lenderLocs.push_back(loc);
    memAttr.lenderSizes.push_back(size);

    attr.size = 1073741824;
    attr.waterMallocAttr = memAttr;

    MOCKER(JsonUtil::RackMemConvertMap2JsonStr).stubs().will(returnValue(true)).then(returnValue(false));
    std::string res = attr.ToJson();
    ASSERT_EQ("", res);
}

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_From_Json)
{
    std::string json_str = R"({
    "highWatermark": 88,
    "lowWatermark": 11,
    "name": "",
    "numaPresentId": -1,
    "perfLevel": 0,
    "size": 1073741824,
    "type": "WATER_BORROW",
    "waterMallocAttr": {
        "dstNodeNum": 1,
        "lenderLocs": [
            "Node1/0/0"
        ],
        "lenderNumaSize": 1,
        "lenderSize": [
            1073741824
        ],
        "srcNid": "Node0",
        "srcNuma": 0,
        "srcSocket": 0
    }
})";

    RackCreateResourceWaterBorrowAttr attr1;

    uint64_t size = 1073741824;

    MemMallocAttr memAttr;
    memAttr.srcNid = "Node0";
    memAttr.srcSocket = 0;
    memAttr.srcNuma = 0;
    memAttr.dstNodeNum = 1;

    RackMemNumaLoc loc;
    loc.nodeId = "Node1";
    loc.socketId = 0;
    loc.numaId = 0;

    memAttr.lenderNumaSize = 1;
    memAttr.lenderLocs.push_back(loc);
    memAttr.lenderSizes.push_back(size);

    attr1.size = 1073741824;
    attr1.waterMallocAttr = memAttr;

    RackCreateResourceWaterBorrowAttr attr2;
    attr2.FromJson(json_str);
    ASSERT_EQ(attr1.ToJson(), attr2.ToJson());
}

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_From_Json_JsonStr2Map_Failed)
{
    std::string json_str = R"({
    "highWatermark": 88,
    "lowWatermark": 11,
    "name": "",
    "numaPresentId": -1,
    "perfLevel": 0,
    "size": 1073741824,
    "type": "WATER_BORROW",
    "waterMallocAttr": {
        "dstNodeNum": 1,
        "lenderLocs": [
            "Node1/0/0"
        ],
        "lenderNumaSize": 1,
        "lenderSize": [
            1073741824
        ],
        "srcNid": "Node0",
        "srcNuma": 0,
        "srcSocket": 0
    }
})";

    RackCreateResourceWaterBorrowAttr attr;
    MOCKER(JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(false));
    bool ret = attr.FromJson(json_str);
    ASSERT_EQ(false, ret);
}

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_From_Json_JsonStr2Map_Success)
{
    std::string json_str = R"({
    "highWatermark": 88,
    "lowWatermark": 11,
    "name": "",
    "numaPresentId": -1,
    "perfLevel": 0,
    "size": 1073741824,
    "type": "WATER_BORROW",
    "waterMallocAttr": {
        "dstNodeNum": 1,
        "lenderLocs": [
            "Node1/0/0"
        ],
        "lenderNumaSize": 1,
        "lenderSize": [
            1073741824
        ],
        "srcNid": "Node0",
        "srcNuma": 0,
        "srcSocket": 0
    }
})";

    RackCreateResourceWaterBorrowAttr attr;
    MOCKER(JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(true));
    bool ret = attr.FromJson(json_str);
    ASSERT_EQ(false, ret);
}

TEST_F(TestMemJsonDef, RackCreateResourceWaterBorrowAttr_From_Json_JsonStr2Vec_Success)
{
    std::string json_str = R"({
    "highWatermark": 88,
    "lowWatermark": 11,
    "name": "",
    "numaPresentId": -1,
    "perfLevel": 0,
    "size": 1073741824,
    "type": "WATER_BORROW",
    "waterMallocAttr": {
        "dstNodeNum": 1,
        "lenderLocs": [
            "Node1/0/0"
        ],
        "lenderNumaSize": 1,
        "lenderSize": [
            1073741824
        ],
        "srcNid": "Node0",
        "srcNuma": 0,
        "srcSocket": 0
    }
})";

    RackCreateResourceWaterBorrowAttr attr;
    MOCKER(JsonUtil::RackMemConvertJsonStr2Vec).stubs().will(returnValue(false));
    bool ret = attr.FromJson(json_str);
    ASSERT_EQ(false, ret);
}

TEST_F(TestMemJsonDef, ExtractMemIdAndNumaId)
{
    std::string json_str = R"({
        "ctx": {
            "response": {
                "memId": 3,
                "shmSize": 1073741824,
                "numaId": 5,
                "mErrCode": 0
            },
            "type": "ATTACH",
            "export_type": "WATER_BORROW",
            "requestAttr": {
                "waterMallocAttr": {
                    "dstNodeNum": 1,
                    "srcNuma": 0,
                    "srcSocket": 0,
                    "srcNid": "Node0"
                },
                "perfLevel": 0,
                "size": 1073741824,
                "name": "watername",
                "type": "WATER_BORROW",
                "numaPresentId": 5
            },
            "exeNodeId": "Node0",
            "realExe": ""
        },
        "condition": "Success"
    })";

    uint64_t memId;
    int16_t presentNumaId;

    ExtractMemIdAndNumaId(json_str, memId, presentNumaId);

    ASSERT_EQ(3, memId);
    ASSERT_EQ(5, presentNumaId);
}
} // namespace mempooling
