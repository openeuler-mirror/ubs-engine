/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_vm_vector_util.h"

using namespace vm;
namespace ubse::ut::vm {
// 设置测试环境
void TestVmVectorUtil::SetUp()
{
    // 公共测试数据初始化
    source = {1, 2, 3, 4, 5, 2, 4};
    targets = {2, 4, 6};
    Test::SetUp();
}

// 拆卸测试环境
void TestVmVectorUtil::TearDown()
{
    Test::TearDown();
}

// 测试正常移除场景
TEST_F(TestVmVectorUtil, RemovesCommonElements)
{
    VectorUtil::RemoveCommonElements(source, targets);

    // 验证结果列表长度为3
    EXPECT_EQ(source.size(), 3);
    // 验证结果列表元素预期为 1,3,5
    EXPECT_EQ(source, std::vector<uint16_t>({1, 3, 5}));
}
// 测试空移除列表
TEST_F(TestVmVectorUtil, HandlesEmptyRemoveList)
{
    targets.clear();
    VectorUtil::RemoveCommonElements(source, targets);

    // 验证结果列表元素预期为 1, 2, 3, 4, 5, 2, 4
    EXPECT_EQ(source, std::vector<uint16_t>({1, 2, 3, 4, 5, 2, 4}));
}

// 测试空源列表
TEST_F(TestVmVectorUtil, HandlesEmptySource)
{
    source.clear();
    VectorUtil::RemoveCommonElements(source, targets);

    EXPECT_TRUE(source.empty());
}

// 测试完全移除场景
TEST_F(TestVmVectorUtil, RemovesAllElements)
{
    // 设置待移除元素为 1, 2, 3, 4, 5
    targets = {1, 2, 3, 4, 5};
    VectorUtil::RemoveCommonElements(source, targets);

    EXPECT_TRUE(source.empty());
}

// 测试重复元素处理
TEST_F(TestVmVectorUtil, HandlesDuplicateElements)
{
    // 设置Vector元素为3个相同元素2
    source = {2, 2, 2};
    // 设置待删除元素
    targets = {2};
    VectorUtil::RemoveCommonElements(source, targets);

    EXPECT_TRUE(source.empty());
}

// 测试无交集场景
TEST_F(TestVmVectorUtil, HandlesNoCommonElements)
{
    // 设置待删除元素7,8,9 与source无交集
    targets = {7, 8, 9};
    VectorUtil::RemoveCommonElements(source, targets);

    // 验证结果列表元素预期为 1, 2, 3, 4, 5, 2, 4
    EXPECT_EQ(source, std::vector<uint16_t>({1, 2, 3, 4, 5, 2, 4}));
}

// 测试目标列表包含不存在元素
TEST_F(TestVmVectorUtil, HandlesNonExistingElements)
{
    // 添加待删除元素100,切source中没有这个元素
    targets.push_back(100);
    VectorUtil::RemoveCommonElements(source, targets);
    // 验证结果列表元素预期为 1, 3, 5
    EXPECT_EQ(source, std::vector<uint16_t>({1, 3, 5}));
}
} // namespace ubse::ut::vm