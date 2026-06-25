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

#include <gtest/gtest.h>
#include "ubse_error.h"
#include "ubse_route_table.h"

using namespace ubse::com;

namespace ubse::ut::com {

class TestUbseRouteTable : public testing::Test {
public:
    TestUbseRouteTable() = default;

protected:
    UbseRouteTable table_;
};

/*
 * 用例描述：精确匹配查找
 * 测试步骤：
 * 1. 添加一条精确匹配路由 (capacity=0, priority=1, dstNodeId="nodeA", nextHop="nodeB")
 * 2. 查找 "nodeA"
 * 预期结果：返回 "nodeB"
 */
TEST_F(TestUbseRouteTable, ExactMatchLookup)
{
    RouteEntry entry;
    entry.dstNodeId = "nodeA";
    entry.capacity = 0;
    entry.priority = 1;
    entry.nextHopNodeId = "nodeB";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry));

    std::string nextHop = table_.Lookup("nodeA");
    EXPECT_EQ("nodeB", nextHop);
}

/*
 * 用例描述：精确匹配未命中
 * 测试步骤：
 * 1. 添加一条精确匹配路由 (dstNodeId="nodeA", nextHop="nodeB")
 * 2. 查找 "nodeC"
 * 预期结果：返回空字符串
 */
TEST_F(TestUbseRouteTable, ExactMatchMiss)
{
    RouteEntry entry;
    entry.dstNodeId = "nodeA";
    entry.capacity = 0;
    entry.priority = 1;
    entry.nextHopNodeId = "nodeB";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry));

    std::string nextHop = table_.Lookup("nodeC");
    EXPECT_TRUE(nextHop.empty());
}

/*
 * 用例描述：优先级排序，低优先级优先匹配
 * 测试步骤：
 * 1. 添加 priority=5 的路由 (dstNodeId="nodeA", nextHop="via5")
 * 2. 添加 priority=3 的路由 (dstNodeId="nodeA", nextHop="via3")
 * 3. 查找 "nodeA"
 * 预期结果：返回 "via3"（优先级低者优先）
 */
TEST_F(TestUbseRouteTable, PriorityOrdering)
{
    RouteEntry entry1;
    entry1.dstNodeId = "nodeA";
    entry1.capacity = 0;
    entry1.priority = 5;
    entry1.nextHopNodeId = "via5";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry1));

    RouteEntry entry2;
    entry2.dstNodeId = "nodeA";
    entry2.capacity = 0;
    entry2.priority = 3;
    entry2.nextHopNodeId = "via3";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry2));

    std::string nextHop = table_.Lookup("nodeA");
    EXPECT_EQ("via3", nextHop);
}

/*
 * 用例描述：重复添加相同路由条目（幂等）
 * 测试步骤：
 * 1. 添加路由 (dstNodeId="nodeA", priority=1, nextHop="nodeB")
 * 2. 再次添加相同的路由
 * 预期结果：两次都返回 UBSE_OK
 */
TEST_F(TestUbseRouteTable, DuplicateAddSameValue)
{
    RouteEntry entry;
    entry.dstNodeId = "nodeA";
    entry.capacity = 0;
    entry.priority = 1;
    entry.nextHopNodeId = "nodeB";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry));
    EXPECT_EQ(UBSE_OK, table_.AddRoute(entry));
}

/*
 * 用例描述：重复添加相同key但不同value的路由（拒绝）
 * 测试步骤：
 * 1. 添加路由 (dstNodeId="nodeA", priority=1, nextHop="nodeB")
 * 2. 添加相同key但不同nextHop的路由
 * 预期结果：第二次返回 UBSE_ERROR
 */
TEST_F(TestUbseRouteTable, DuplicateAddDifferentValue)
{
    RouteEntry entry1;
    entry1.dstNodeId = "nodeA";
    entry1.capacity = 0;
    entry1.priority = 1;
    entry1.nextHopNodeId = "nodeB";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry1));

    RouteEntry entry2;
    entry2.dstNodeId = "nodeA";
    entry2.capacity = 0;
    entry2.priority = 1;
    entry2.nextHopNodeId = "nodeC";
    EXPECT_EQ(UBSE_COM_ERROR_ROUTE_EXISTED, table_.AddRoute(entry2));
}

/*
 * 用例描述：外部API不能添加priority=0的路由
 * 测试步骤：
 * 1. 尝试添加 priority=0 的路由
 * 预期结果：返回 UBSE_COM_ERROR_ROUTE_INVAL
 */
TEST_F(TestUbseRouteTable, ExternalApiRejectsPriorityZero)
{
    RouteEntry entry;
    entry.dstNodeId = "nodeA";
    entry.capacity = 0;
    entry.priority = 0;
    entry.nextHopNodeId = "nodeB";
    EXPECT_EQ(UBSE_COM_ERROR_ROUTE_INVAL, table_.AddRoute(entry));
}

/*
 * 用例描述：删除指定节点的路由
 * 测试步骤：
 * 1. 添加路由 (dstNodeId="nodeA")
 * 2. 删除 "nodeA"
 * 3. 查找 "nodeA"
 * 预期结果：查找返回空
 */
TEST_F(TestUbseRouteTable, DeleteRouteByNodeId)
{
    RouteEntry entry;
    entry.dstNodeId = "nodeA";
    entry.capacity = 0;
    entry.priority = 1;
    entry.nextHopNodeId = "nodeB";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry));

    table_.DelRoute("nodeA");
    EXPECT_TRUE(table_.Lookup("nodeA").empty());
}

/*
 * 用例描述：直连路由自动管理
 * 测试步骤：
 * 1. 添加直连路由 AddDirectRoute("nodeX")
 * 2. 查找 "nodeX"（应命中 priority=0 的直连路由）
 * 3. 删除直连路由
 * 4. 再次查找
 * 预期结果：添加后能查到；删除后查不到
 */
TEST_F(TestUbseRouteTable, DirectRouteAutoManagement)
{
    table_.AddDirectRoute("nodeX");
    EXPECT_EQ("nodeX", table_.Lookup("nodeX"));

    table_.DelDirectRoute("nodeX");
    EXPECT_TRUE(table_.Lookup("nodeX").empty());
}

/*
 * 用例描述：模糊匹配查找
 * 测试步骤：
 * 1. 添加模糊匹配路由 (capacity=4, dstNodeId="6", nextHop="hop6")
 *    分组算法 ComputeGroup: (nodeIdNum - 1) >> ctz(capacity)
 *    capacity=4, shift=2 → group = (n-1) >> 2
 *    group 1: nodes [5,6,7,8]
 * 2. 查找 "7"（与 "6" 同组 group 1）
 * 3. 查找 "4"（不同组 group 0）
 * 预期结果："7" 命中；"4" 未命中
 */
TEST_F(TestUbseRouteTable, FuzzyMatch)
{
    RouteEntry entry;
    entry.dstNodeId = "6";
    entry.capacity = 4;
    entry.priority = 1;
    entry.nextHopNodeId = "hop6";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry));

    // "7" 应通过模糊匹配命中（与"6"同属 group 1）
    EXPECT_EQ("hop6", table_.Lookup("7"));

    // "4" 不应命中（group 0 ≠ group 1）
    EXPECT_TRUE(table_.Lookup("4").empty());
}

/*
 * 用例描述：删除默认路由 ("0")
 * 测试步骤：
 * 1. 添加默认路由 (dstNodeId="0")
 * 2. 删除 "0"
 * 3. 查找
 * 预期结果：默认路由被删除
 */
TEST_F(TestUbseRouteTable, DeleteDefaultRoute)
{
    RouteEntry entry;
    entry.dstNodeId = "0";
    entry.capacity = DEFAULT_ROUTE_MAX_CAPACITY;
    entry.priority = 1;
    entry.nextHopNodeId = "defaultHop";
    ASSERT_EQ(UBSE_OK, table_.AddRoute(entry));

    table_.DelRoute("0");
    EXPECT_TRUE(table_.Lookup("0").empty());
}

/*
 * 用例描述：DelRoute 不能删除 priority=0 的直连路由
 * 测试步骤：
 * 1. 添加直连路由 AddDirectRoute("nodeX")
 * 2. 尝试用 DelRoute 删除 "nodeX"
 * 3. 查找 "nodeX"
 * 预期结果：直连路由依然存在，查找返回 "nodeX"
 */
TEST_F(TestUbseRouteTable, DelRouteCannotDeleteDirectRoute)
{
    table_.AddDirectRoute("nodeX");
    table_.DelRoute("nodeX");
    EXPECT_EQ("nodeX", table_.Lookup("nodeX"));
}

/*
 * 用例描述：空表查找
 * 测试步骤：
 * 1. 不添加任何路由
 * 2. 查找任意节点
 * 预期结果：返回空字符串
 */
TEST_F(TestUbseRouteTable, EmptyTableLookup)
{
    std::string nextHop = table_.Lookup("anything");
    EXPECT_TRUE(nextHop.empty());
}

} // namespace ubse::ut::com
