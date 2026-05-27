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

#include <string>
#include <unordered_map>

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "src/adapter_plugins/urma_uvs/ubse_urma_topo_config.h"
#include "src/adapter_plugins/urma_uvs/ubse_urma_uvs_module.h"
#include "ubse_error.h"

namespace ubse::urma {
void FillSelfLinks(const std::string &currentSlotId,
                   std::unordered_map<std::string, UbcoreTopoNode> &nodeMap);
UbseResult FillClosTopoByConfig(const UbseUrmaTopoConfig &topoConfig,
                                std::unordered_map<std::string, UbcoreTopoNode> &nodeMap);
} // namespace ubse::urma

namespace ubse::ut::urma {
using namespace ubse::common::def;
using namespace ubse::urma;

class TestUbseUrmaUvs : public testing::Test {
public:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }

protected:
    static uint32_t PortIndex(uint32_t chipId, uint32_t portId)
    {
        return (chipId - 1) * PORT_NUM + portId;
    }

    static UbcoreTopoNode MakeNode(uint32_t nodeId, bool isCurrent)
    {
        UbcoreTopoNode node{};
        node.id = nodeId;
        node.type = 1;
        node.is_current = isCurrent ? 1 : 0;
        return node;
    }

    static UbseUrmaTopoConfig MakeNonCrossTopoConfig()
    {
        UbseUrmaTopoConfig config{};
        config.version = "1.0";
        config.nodeType = "normal";
        config.linkType = "non-cross";
        config.links = {
            {{1, 1, 1}, {1, 1, 1}},
            {{1, 1, 2}, {1, 1, 2}},
            {{2, 1, 1}, {2, 1, 1}},
            {{2, 1, 2}, {2, 1, 2}},
        };
        return config;
    }

    static UbseUrmaTopoConfig MakeHccsCrossTopoConfig()
    {
        UbseUrmaTopoConfig config{};
        config.version = "1.0";
        config.nodeType = "normal";
        config.linkType = "hccs-cross";
        config.links = {
            {{1, 1, 1}, {1, 1, 1}},
            {{1, 1, 1}, {2, 1, 1}},
            {{1, 1, 2}, {1, 1, 2}},
            {{1, 1, 2}, {2, 1, 2}},
            {{2, 1, 1}, {2, 1, 1}},
            {{2, 1, 1}, {1, 1, 1}},
            {{2, 1, 2}, {2, 1, 2}},
            {{2, 1, 2}, {1, 1, 2}},
        };
        return config;
    }
};

/*
 * 用例描述：按 non-cross 静态拓扑配置填充 CLOS 拓扑矩阵。
 * 预期结果：非当前节点仅填充同 chip 同 port 链路，当前节点 entry 的 links 全部保持 false。
 */
TEST_F(TestUbseUrmaUvs, FillClosTopoByConfigNonCrossSuccess)
{
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"1", MakeNode(1, true)},
        {"2", MakeNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(MakeNonCrossTopoConfig(), nodeMap), UBSE_OK);

    EXPECT_FALSE(nodeMap["1"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 2)][PortIndex(1, 2)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 2)][PortIndex(2, 2)]);
    EXPECT_FALSE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 1)]);
}

/*
 * 用例描述：按 hccs-cross 静态拓扑配置填充 CLOS 拓扑矩阵。
 * 预期结果：非当前节点同时填充同 chip 同 port 链路和同平面的跨 chip 链路。
 */
TEST_F(TestUbseUrmaUvs, FillClosTopoByConfigHccsCrossSuccess)
{
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"1", MakeNode(1, true)},
        {"2", MakeNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(MakeHccsCrossTopoConfig(), nodeMap), UBSE_OK);

    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 2)][PortIndex(2, 2)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 2)][PortIndex(1, 2)]);
    EXPECT_FALSE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 2)]);
}

/*
 * 用例描述：CLOS 静态拓扑配置中端口 chipId 非法。
 * 预期结果：填充失败并返回 UBSE_ERROR。
 */
TEST_F(TestUbseUrmaUvs, FillClosTopoByConfigFailsWhenChipInvalid)
{
    auto config = MakeNonCrossTopoConfig();
    config.links[0].localPort.chipId = 0;
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"2", MakeNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(config, nodeMap), UBSE_ERROR);
}

/*
 * 用例描述：FillSelfLinks 仅为当前节点 entry 的每个 port 设置 links[i][i]=true。
 * 预期结果：当前节点的所有 port 自连通为 true，非当前节点的 links 不受影响。
 */
TEST_F(TestUbseUrmaUvs, FillSelfLinksOnlySetsCurrentNodeSelfConnectivity)
{
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"1", MakeNode(1, true)},
        {"2", MakeNode(2, false)},
    };

    FillSelfLinks("1", nodeMap);

    for (uint32_t i = 0; i < UVS_PORT_NUM; i++) {
        EXPECT_TRUE(nodeMap["1"].links[i][i]);
        EXPECT_FALSE(nodeMap["2"].links[i][i]);
    }
    EXPECT_FALSE(nodeMap["1"].links[0][1]);
    EXPECT_FALSE(nodeMap["2"].links[0][1]);
}

/*
 * 用例描述：FillSelfLinks 与 FillClosTopoByConfig 组合后，当前节点的自连通也为 true。
 * 预期结果：当前节点 self-link 为 true（由 FillSelfLinks 保证），非当前节点的链路由配置决定。
 */
TEST_F(TestUbseUrmaUvs, FillClosTopoByConfigWithSelfLinksCurrentNodeAlsoSelfConnected)
{
    std::unordered_map<std::string, UbcoreTopoNode> nodeMap{
        {"1", MakeNode(1, true)},
        {"2", MakeNode(2, false)},
    };

    EXPECT_EQ(FillClosTopoByConfig(MakeNonCrossTopoConfig(), nodeMap), UBSE_OK);
    FillSelfLinks("1", nodeMap);

    EXPECT_TRUE(nodeMap["1"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(1, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(1, 2)][PortIndex(1, 2)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 1)][PortIndex(2, 1)]);
    EXPECT_TRUE(nodeMap["2"].links[PortIndex(2, 2)][PortIndex(2, 2)]);
    EXPECT_FALSE(nodeMap["2"].links[PortIndex(1, 1)][PortIndex(2, 1)]);
}
} // namespace ubse::ut::urma
