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

#include "test_ubse_node_discovery_root_mode.h"

#include "root_mode/message/ubse_node_discovery_list_serial.h"
#include "ubse_error.h"

namespace ubse::nodeDiscovery {
void TestUbseNodeDiscoveryRootMode::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeDiscoveryRootMode::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeDiscoveryRootMode, Serialize)
{
    UbseNodeStaticInfo info{};
    info.superPodId = 0;
    info.podId = 1;
    info.nodeId = "1";
    info.addr = "192.168.100.100";
    info.bonding0Eid = "4245:4944:0000:0000:0000:0000:0100:0000";
    UbseUrmaEidInfo eidInfo{};
    eidInfo.entityId = "2";
    eidInfo.primaryEid = "4245:4944:0000:0000:0000:0000:0100:0001";
    eidInfo.portEidList["3"] = "4245:4944:0000:0000:0000:0000:0100:0002";
    info.feEidList["5"] = eidInfo;
    std::vector<UbseNodeStaticInfo> nodes{info};
    UbseNodeDiscoveryListSerial serial(nodes);
    EXPECT_EQ(UBSE_OK, serial.Serialize());
    UbseNodeDiscoveryListSerial deserial(serial.SerializedData(), serial.SerializedDataSize());
    EXPECT_EQ(UBSE_OK, deserial.Deserialize());
    std::vector<UbseNodeStaticInfo> result = deserial.GetUbseNodeList();
    EXPECT_EQ(1, result.size());
    EXPECT_EQ(0, result[0].superPodId);
    EXPECT_EQ(1, result[0].podId);
    EXPECT_EQ("1", result[0].nodeId);
    EXPECT_EQ("192.168.100.100", result[0].addr);
    EXPECT_EQ("4245:4944:0000:0000:0000:0000:0100:0000", result[0].bonding0Eid);
    EXPECT_EQ(1, result[0].feEidList.size());
    EXPECT_EQ("2", result[0].feEidList["5"].entityId);
    EXPECT_EQ("4245:4944:0000:0000:0000:0000:0100:0001", result[0].feEidList["5"].primaryEid);
    EXPECT_EQ(1, result[0].feEidList["5"].portEidList.size());
    EXPECT_EQ("4245:4944:0000:0000:0000:0000:0100:0002", result[0].feEidList["5"].portEidList["3"]);
}
} // namespace ubse::nodeDiscovery
