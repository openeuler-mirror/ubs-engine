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

#include "test_ubse_mem_controller_cleanshm.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_util.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using namespace ubse::mem::util;

void TestUbseMemControllerCleanShm::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerCleanShm::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void addNodeToDebtMap_Export(NodeMemDebtInfoMap& debtMap, const std::string& nodeName, const std::string& resourceID,
                             const std::vector<std::string>& exportNumaInfoNames, UbseMemState state)
{
    UbseMemShareBorrowExportObj obj;
    for (const auto& name : exportNumaInfoNames) {
        obj.algoResult.exportNumaInfos.push_back({name});
    }
    obj.status.state = state;
    obj.req.name = resourceID;
    debtMap[nodeName].shareExportObjMap[resourceID] = obj;
}
void addNodeToDebtMap_Import(NodeMemDebtInfoMap& debtMap, const std::string& nodeName, const std::string& resourceID,
                             const std::vector<std::string>& exportNumaInfoNames, UbseMemState state)
{
    // 创建导入对象
    UbseMemShareBorrowImportObj obj;
    for (const auto& name : exportNumaInfoNames) {
        obj.algoResult.exportNumaInfos.push_back({name});
    }
    obj.req.name = resourceID;
    obj.status.state = state;

    // 将对象添加到 debtMap 的 shareImportObjMap 中
    debtMap[nodeName].shareImportObjMap[resourceID] = obj;
}
// 辅助函数：构建 baseMap 和 refCountMap
void BuildBaseAndRefCountMaps_multi(
    const std::string& name, bool isAnonymous, const std::vector<std::string>& nodeIds,
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>>& baseMap,
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& refCountMap,
    const std::vector<size_t>& refCounts = {})
{
    for (size_t i = 0; i < nodeIds.size(); ++i) {
        UbseMemShareBorrowExportObj obj;
        obj.req.name = name;
        obj.req.shmAnonymous = isAnonymous;
        obj.algoResult.exportNumaInfos = {{nodeIds[i]}};
        baseMap[nodeIds[i]].push_back(obj);

        if (!refCounts.empty()) {
            refCountMap[name][nodeIds[i]] = refCounts[i];
        }
    }
}

// 辅助函数：构建 baseMap 和 refCountMap
void BuildBaseAndRefCountMaps_single(
    const std::string& name, bool isAnonymous, const std::string& nodeId,
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>>& baseMap,
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>>& refCountMap, size_t refCount = 0,
    bool includeInRefCountMap = true)
{
    UbseMemShareBorrowExportObj obj;
    obj.req.name = name;
    obj.req.shmAnonymous = isAnonymous;
    obj.algoResult.exportNumaInfos = {{nodeId}};
    baseMap[nodeId].push_back(obj);

    if (includeInRefCountMap) {
        refCountMap[name][nodeId] = refCount;
    }
}

TEST_F(TestUbseMemControllerCleanShm, Test_CollectExportInfo)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap 函数添加节点
    addNodeToDebtMap_Export(debtMap, "node1", "res1", {"node1"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "node2", "res2", {"node2"}, UBSE_MEM_EXPORT_DESTROYING);
    addNodeToDebtMap_Export(debtMap, "node3", "res3", {}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "node4", "res4", {"node4"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "node5", "res1", {"node5"}, UBSE_MEM_EXPORT_SUCCESS);

    // 调用 collectExportInfo 函数并验证结果
    auto result = collectExportInfo(debtMap);
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> expected = {
        {"res1", {{"node1", 0}, {"node5", 0}}}, {"res4", {{"node4", 0}}}};
    EXPECT_EQ(result, expected);
}

TEST_F(TestUbseMemControllerCleanShm, Test_UpdateRefCount)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap_Import 函数添加节点
    addNodeToDebtMap_Import(debtMap, "node1", "res1", {"nodeA"}, UBSE_MEM_IMPORT_SUCCESS);
    addNodeToDebtMap_Import(debtMap, "node2", "res2", {"nodeB"}, UBSE_MEM_IMPORT_DESTROYED);
    addNodeToDebtMap_Import(debtMap, "node3", "res3", {}, UBSE_MEM_IMPORT_SUCCESS);
    addNodeToDebtMap_Import(debtMap, "node4", "res1", {"nodeC"}, UBSE_MEM_IMPORT_SUCCESS);

    // 初始化 refCountMap
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;
    refCountMap["res1"]["nodeA"] = 0;
    refCountMap["res1"]["nodeC"] = 0;

    // 调用 updateRefCount 函数并验证结果
    updateRefCount(refCountMap, debtMap);

    EXPECT_EQ(refCountMap["res1"]["nodeA"], 1);
    EXPECT_EQ(refCountMap["res1"]["nodeC"], 1);
    EXPECT_EQ(refCountMap.count("res2"), 0);
    EXPECT_EQ(refCountMap.count("res3"), 0);
}

TEST_F(TestUbseMemControllerCleanShm, Test_CountShareMemoryRefCount)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap_Import 函数添加节点
    addNodeToDebtMap_Import(debtMap, "node1", "res1", {"nodeA"}, UBSE_MEM_IMPORT_SUCCESS);
    addNodeToDebtMap_Import(debtMap, "node2", "res1", {"nodeB"}, UBSE_MEM_IMPORT_SUCCESS);

    // 初始化 mock_refCountMap
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> mock_refCountMap = {
        {"res1", {{"nodeA", 0}, {"nodeB", 0}}}};
    MOCKER_CPP(&collectExportInfo).stubs().will(returnValue(mock_refCountMap));

    // 调用 CountShareMemoryRefCount 函数并验证结果
    auto result = CountShareMemoryRefCount(debtMap);
    EXPECT_EQ(result["res1"]["nodeA"], 1);
    EXPECT_EQ(result["res1"]["nodeB"], 1);
}

TEST_F(TestUbseMemControllerCleanShm, Test_AllRefCountZero_Anonymous)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap 和 refCountMap
    BuildBaseAndRefCountMaps_multi("res1", true, {"node1", "node2"}, baseMap, refCountMap, {0, 0});

    std::vector<UbseMemShareBorrowExportObj> toClean;
    // 调用函数
    CheckAndCleanMultiBaseNode("res1", baseMap, refCountMap, toClean);
    // 验证结果：所有 anonymous 对象被清理，无剩余
    EXPECT_EQ(toClean.size(), 2);
    for (const auto& obj : toClean) {
        EXPECT_TRUE(obj.req.shmAnonymous);
    }
}

TEST_F(TestUbseMemControllerCleanShm, Test_AllRefCountZero_KeepOne)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap 和 refCountMap
    BuildBaseAndRefCountMaps_multi("res1", false, {"node1", "node2"}, baseMap, refCountMap, {0, 0});

    std::vector<UbseMemShareBorrowExportObj> toClean;
    // 调用函数
    CheckAndCleanMultiBaseNode("res1", baseMap, refCountMap, toClean);
    // 验证结果：保留一个，清理一个
    EXPECT_EQ(toClean.size(), 1);
    for (const auto& obj : toClean) {
        EXPECT_FALSE(obj.req.shmAnonymous);
    }
}

TEST_F(TestUbseMemControllerCleanShm, Test_PartialRefCountZero)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap 和 refCountMap
    BuildBaseAndRefCountMaps_multi("res1", false, {"node1", "node2"}, baseMap, refCountMap, {0, 1});

    std::vector<UbseMemShareBorrowExportObj> toClean;
    // 调用函数
    CheckAndCleanMultiBaseNode("res1", baseMap, refCountMap, toClean);
    // 验证结果：只清理 node1 的对象
    EXPECT_EQ(toClean.size(), 1);
    for (const auto& obj : toClean) {
        EXPECT_EQ(obj.algoResult.exportNumaInfos[0].nodeId, "node1");
    }
}

TEST_F(TestUbseMemControllerCleanShm, Test_NameNotInRefCountMap)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;
    // 构建 baseMap（包含 res1）
    BuildBaseAndRefCountMaps_multi("res1", false, {"node1"}, baseMap, refCountMap);

    // refCountMap 不包含 res1
    std::vector<UbseMemShareBorrowExportObj> toClean;
    CheckAndCleanMultiBaseNode("res1", baseMap, refCountMap, toClean);
    EXPECT_TRUE(toClean.empty());
}

TEST_F(TestUbseMemControllerCleanShm, Test_BaseMapEmpty)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;
    std::vector<UbseMemShareBorrowExportObj> toClean;

    CheckAndCleanSingleBaseNode("res1", baseMap, refCountMap, toClean);

    EXPECT_TRUE(toClean.empty());
}

TEST_F(TestUbseMemControllerCleanShm, Test_CheckAndCleanSingleBaseNode_NotContainNameOrBaseNodeId)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap（包含 res1）
    BuildBaseAndRefCountMaps_single("res1", false, "node1", baseMap, refCountMap, 0, false);

    std::vector<UbseMemShareBorrowExportObj> toClean;

    CheckAndCleanSingleBaseNode("res1", baseMap, refCountMap, toClean);

    EXPECT_TRUE(toClean.empty());
}

TEST_F(TestUbseMemControllerCleanShm, Test_CheckAndCleanSingleBaseNode_RefCountZeroAndAnonymous)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap 和 refCountMap
    BuildBaseAndRefCountMaps_single("res1", true, "node1", baseMap, refCountMap, 0);

    std::vector<UbseMemShareBorrowExportObj> toClean;

    CheckAndCleanSingleBaseNode("res1", baseMap, refCountMap, toClean);

    EXPECT_EQ(toClean.size(), 1);
    EXPECT_TRUE(toClean[0].req.shmAnonymous);
}

TEST_F(TestUbseMemControllerCleanShm, Test_CheckAndCleanSingleBaseNode_RefCountZeroButNotAnonymous)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap 和 refCountMap
    BuildBaseAndRefCountMaps_single("res1", false, "node1", baseMap, refCountMap, 0);

    std::vector<UbseMemShareBorrowExportObj> toClean;

    CheckAndCleanSingleBaseNode("res1", baseMap, refCountMap, toClean);

    EXPECT_TRUE(toClean.empty());
}

TEST_F(TestUbseMemControllerCleanShm, Test_CheckAndCleanSingleBaseNode_RefCountNotZero)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseMap;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    // 构建 baseMap 和 refCountMap
    BuildBaseAndRefCountMaps_single("res1", true, "node1", baseMap, refCountMap, 1);

    std::vector<UbseMemShareBorrowExportObj> toClean;

    CheckAndCleanSingleBaseNode("res1", baseMap, refCountMap, toClean);

    EXPECT_TRUE(toClean.empty());
}

TEST_F(TestUbseMemControllerCleanShm, Test_NormalCase)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap_Export 函数添加节点
    addNodeToDebtMap_Export(debtMap, "nodeA", "res1", {"node1"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "nodeB", "res1", {"node2"}, UBSE_MEM_EXPORT_DESTROYING);
    addNodeToDebtMap_Export(debtMap, "nodeC", "res1", {}, UBSE_MEM_EXPORT_SUCCESS);

    // 调用 GetExportObjsByBaseNode 函数并验证结果
    auto result = GetExportObjsByBaseNode(debtMap, "res1");

    EXPECT_EQ(result.size(), 1);  // 只有 node1 的对象被保留
    EXPECT_EQ(result["node1"].size(), 1);
}

TEST_F(TestUbseMemControllerCleanShm, CollectBaseRefCount_NormalCase)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap_Export 函数添加节点
    addNodeToDebtMap_Export(debtMap, "nodeA", "res1", {"node1"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "nodeB", "res1", {"node2"}, UBSE_MEM_EXPORT_DESTROYING);

    // 构建 baseToObjs
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseToObjs;
    baseToObjs["node1"].push_back(debtMap["nodeA"].shareExportObjMap["res1"]);
    baseToObjs["node2"].push_back(debtMap["nodeB"].shareExportObjMap["res1"]);

    // 初始化 refCountMap
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;
    refCountMap["res1"]["node1"] = 10;
    refCountMap["res1"]["node2"] = 20;

    // 调用 CollectBaseRefCount 函数并验证结果
    auto result = CollectBaseRefCount(baseToObjs, refCountMap, "res1");

    EXPECT_EQ(result.size(), 2);
}

TEST_F(TestUbseMemControllerCleanShm, CollectBaseRefCount_InvalidName)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseToObjs;
    baseToObjs["node1"].emplace_back();

    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;

    auto result = CollectBaseRefCount(baseToObjs, refCountMap, "res1");
    EXPECT_TRUE(result.empty());
    auto result_1 = CollectBaseRefCount(baseToObjs, refCountMap, "");
    EXPECT_TRUE(result_1.empty());
}

TEST_F(TestUbseMemControllerCleanShm, CollectBaseRefCount_InvalidBaseNodeId)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseToObjs;
    baseToObjs["node1"].emplace_back();

    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> refCountMap;
    refCountMap["res1"]["node2"] = 10;  // node2 不存在于 baseToObjs 中

    auto result = CollectBaseRefCount(baseToObjs, refCountMap, "res1");

    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemControllerCleanShm, FindMaxRefCountBaseNode_NormalCase)
{
    std::vector<std::pair<std::string, size_t>> baseRefCount = {{"node1", 10}, {"node2", 20}};

    auto result = FindMaxRefCountBaseNode(baseRefCount);

    EXPECT_EQ(result, "node2");
}

TEST_F(TestUbseMemControllerCleanShm, FindMaxRefCountBaseNode_SameRefCount)
{
    std::vector<std::pair<std::string, size_t>> baseRefCount = {{"node1", 10}, {"node2", 10}};

    auto result = FindMaxRefCountBaseNode(baseRefCount);

    EXPECT_EQ(result, "node2");  // 返回最后一个最大值
}

TEST_F(TestUbseMemControllerCleanShm, GetExportObjByBaseNode_NormalCase)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseToObjs;
    UbseMemShareBorrowExportObj exportObj;
    exportObj.req.name = "res1";
    baseToObjs["node1"].emplace_back(exportObj);

    auto result = GetExportObjByBaseNode(baseToObjs, "node1");

    EXPECT_EQ(result.req.name, "res1");
}

TEST_F(TestUbseMemControllerCleanShm, GetExportObjByBaseNode_InvalidBaseNodeId)
{
    std::unordered_map<std::string, std::vector<UbseMemShareBorrowExportObj>> baseToObjs;

    auto result = GetExportObjByBaseNode(baseToObjs, "node1");

    EXPECT_TRUE(result.req.name.empty());
}

TEST_F(TestUbseMemControllerCleanShm, IsImportObjMatch_NormalCase)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "res1";
    importObj.algoResult.exportNumaInfos = {{"node1"}};

    EXPECT_TRUE(IsImportObjMatch(importObj, "res1", "node1"));
}

TEST_F(TestUbseMemControllerCleanShm, IsImportObjMatch_InvalidName)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "res1";
    importObj.algoResult.exportNumaInfos = {{"node1"}};

    EXPECT_FALSE(IsImportObjMatch(importObj, "res2", "node1"));
}

TEST_F(TestUbseMemControllerCleanShm, IsImportObjMatch_InvalidBaseNodeId)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "res1";
    importObj.algoResult.exportNumaInfos = {{"node1"}};

    EXPECT_FALSE(IsImportObjMatch(importObj, "res1", "node2"));
}

TEST_F(TestUbseMemControllerCleanShm, IsImportObjMatch_InvalidNumaInfos)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.req.name = "res1";
    importObj.algoResult.exportNumaInfos = {};

    EXPECT_FALSE(IsImportObjMatch(importObj, "res1", "node1"));
}

TEST_F(TestUbseMemControllerCleanShm, CollectImportObjsFromNode_NormalCase)
{
    std::unordered_map<std::string, UbseMemShareBorrowImportObj> importMap;
    UbseMemShareBorrowImportObj importObj1;
    importObj1.req.name = "res1";
    importObj1.algoResult.exportNumaInfos = {{"node1"}};
    importMap["res1"] = importObj1;

    auto result = CollectImportObjsFromNode(importMap, "res1", "node1");

    EXPECT_EQ(result.size(), 1);
}

TEST_F(TestUbseMemControllerCleanShm, CollectImportObjsFromNode_InvalidMatch)
{
    std::unordered_map<std::string, UbseMemShareBorrowImportObj> importMap;
    UbseMemShareBorrowImportObj importObj1;
    importObj1.req.name = "res1";
    importObj1.algoResult.exportNumaInfos = {{"node1"}};
    importMap["res1"] = importObj1;

    auto result = CollectImportObjsFromNode(importMap, "res2", "node1");

    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemControllerCleanShm, GetImportObjsByBaseNode_NormalCase)
{
    NodeMemDebtInfoMap debtMap;
    UbseMemShareBorrowImportObj importObj1;
    importObj1.req.name = "res1";
    importObj1.algoResult.exportNumaInfos = {{"node1"}};
    debtMap["nodeA"].shareImportObjMap["res1"] = importObj1;

    UbseMemShareBorrowImportObj importObj2;
    importObj2.req.name = "res1";
    importObj2.algoResult.exportNumaInfos = {{"node1"}};
    debtMap["nodeB"].shareImportObjMap["res1"] = importObj2;

    auto result = GetImportObjsByBaseNode(debtMap, "res1", "node1");

    EXPECT_EQ(result.size(), 2);
}

TEST_F(TestUbseMemControllerCleanShm, GetImportObjsByBaseNode_InvalidMatch)
{
    NodeMemDebtInfoMap debtMap;
    UbseMemShareBorrowImportObj importObj1;
    importObj1.req.name = "res1";
    importObj1.algoResult.exportNumaInfos = {{"node1"}};
    debtMap["nodeA"].shareImportObjMap["res1"] = importObj1;

    auto result = GetImportObjsByBaseNode(debtMap, "res2", "node1");

    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemControllerCleanShm, Test_GetAllImportObjsByName)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap_Import 函数添加节点
    addNodeToDebtMap_Import(debtMap, "node1", "res1", {"node0"}, UBSE_MEM_IMPORT_SUCCESS);
    addNodeToDebtMap_Import(debtMap, "node2", "res1", {"node3"}, UBSE_MEM_IMPORT_SUCCESS);
    addNodeToDebtMap_Import(debtMap, "node2", "res2", {"node0"}, UBSE_MEM_IMPORT_SUCCESS);

    // 调用 GetAllImportObjsByName 函数并验证结果
    auto result = GetAllImportObjsByName(debtMap, "res1");

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].req.name, "res1");
    EXPECT_EQ(result[1].req.name, "res1");

    // 测试获取 "res2" 的所有导入对象
    result = GetAllImportObjsByName(debtMap, "res2");

    // 验证结果
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].req.name, "res2");

    // 测试获取不存在的 name
    result = GetAllImportObjsByName(debtMap, "res3");

    // 验证结果
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemControllerCleanShm, GetMaxRefCountExportObj_NormalCase)
{
    auto result = GetMaxRefCountExportObj("res1");
    EXPECT_TRUE(result.first.req.name.empty());
    EXPECT_EQ(result.second.size(), 0);
}

TEST_F(TestUbseMemControllerCleanShm, Test_CollectExportObjsByBaseNode)
{
    NodeMemDebtInfoMap debtMap;

    // 使用 addNodeToDebtMap_Export 函数添加节点
    addNodeToDebtMap_Export(debtMap, "node1", "res1", {"node1"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "node2", "res2", {"node2"}, UBSE_MEM_EXPORT_DESTROYING);
    addNodeToDebtMap_Export(debtMap, "node3", "res3", {}, UBSE_MEM_EXPORT_SUCCESS);

    // 收集共享内存对象
    auto result = CollectExportObjsByBaseNode(debtMap);

    // 验证结果
    ASSERT_EQ(result.size(), 1);  // 只有 exportObj1 是有效的
    ASSERT_EQ(result["res1"].size(), 1);
    ASSERT_EQ(result["res1"]["node1"].size(), 1);
    EXPECT_EQ(result["res1"]["node1"][0].req.name, "res1");
}

// 测试 GetIntersection 函数
TEST_F(TestUbseMemControllerCleanShm, EmptyInput)
{
    std::vector<UbseMemShareBorrowExportObj> toClean1;
    std::vector<UbseMemShareBorrowExportObj> toClean2;

    auto result = GetIntersection(toClean1, toClean2);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemControllerCleanShm, NoIntersection)
{
    UbseMemShareBorrowExportObj obj1;
    obj1.req.name = "shared_memory_1";
    obj1.algoResult.exportNumaInfos = {{"node1"}};

    UbseMemShareBorrowExportObj obj2;
    obj2.req.name = "shared_memory_2";
    obj2.algoResult.exportNumaInfos = {{"node2"}};

    std::vector<UbseMemShareBorrowExportObj> toClean1 = {obj1};
    std::vector<UbseMemShareBorrowExportObj> toClean2 = {obj2};

    auto result = GetIntersection(toClean1, toClean2);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestUbseMemControllerCleanShm, PartialIntersection)
{
    UbseMemShareBorrowExportObj obj1;
    obj1.req.name = "shared_memory_1";
    obj1.algoResult.exportNumaInfos = {{"node1"}};

    UbseMemShareBorrowExportObj obj2;
    obj2.req.name = "shared_memory_2";
    obj2.algoResult.exportNumaInfos = {{"node2"}};

    UbseMemShareBorrowExportObj obj3;
    obj3.req.name = "shared_memory_1";
    obj3.algoResult.exportNumaInfos = {{"node1"}};

    std::vector<UbseMemShareBorrowExportObj> toClean1 = {obj1, obj2};
    std::vector<UbseMemShareBorrowExportObj> toClean2 = {obj3};

    auto result = GetIntersection(toClean1, toClean2);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].req.name, "shared_memory_1");
    EXPECT_EQ(result[0].algoResult.exportNumaInfos[0].nodeId, "node1");
}

TEST_F(TestUbseMemControllerCleanShm, FullIntersection)
{
    UbseMemShareBorrowExportObj obj1;
    obj1.req.name = "shared_memory_1";
    obj1.algoResult.exportNumaInfos = {{"node1"}};

    UbseMemShareBorrowExportObj obj2;
    obj2.req.name = "shared_memory_2";
    obj2.algoResult.exportNumaInfos = {{"node2"}};

    std::vector<UbseMemShareBorrowExportObj> toClean1 = {obj1, obj2};
    std::vector<UbseMemShareBorrowExportObj> toClean2 = {obj1, obj2};

    auto result = GetIntersection(toClean1, toClean2);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].req.name, "shared_memory_1");
    EXPECT_EQ(result[0].algoResult.exportNumaInfos[0].nodeId, "node1");
    EXPECT_EQ(result[1].req.name, "shared_memory_2");
    EXPECT_EQ(result[1].algoResult.exportNumaInfos[0].nodeId, "node2");
}

// 测试 ProcessAndClean 函数
TEST_F(TestUbseMemControllerCleanShm, Test_ProcessCurrentCleanList_MultiObj)
{
    // 准备测试数据
    NodeMemDebtInfoMap debtMap;
    std::vector<UbseMemShareBorrowExportObj> toClean;

    // 使用 addNodeToDebtMap_Export 函数添加节点
    addNodeToDebtMap_Export(debtMap, "node1", "res1", {"node1"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "node2", "res2", {"node2"}, UBSE_MEM_EXPORT_DESTROYING);
    addNodeToDebtMap_Export(debtMap, "node3", "res1", {"node3"}, UBSE_MEM_EXPORT_SUCCESS);
    addNodeToDebtMap_Export(debtMap, "node4", "res1", {"node4"}, UBSE_MEM_EXPORT_DESTROYING);
    ProcessCurrentCleanList(debtMap, toClean);
    ASSERT_EQ(toClean.size(), 1);  // 预期清理一个对象
    EXPECT_EQ(toClean[0].req.name, "res1");
}

TEST_F(TestUbseMemControllerCleanShm, TestProcessCurrentCleanList_SingelObj)
{
    NodeMemDebtInfoMap debtMap;
    std::vector<UbseMemShareBorrowExportObj> toClean;

    // 使用 addNodeToDebtMap_Export 函数添加节点
    addNodeToDebtMap_Export(debtMap, "node1", "res1", {"node1"}, UBSE_MEM_EXPORT_SUCCESS);
    debtMap["node1"].shareExportObjMap["res1"].req.shmAnonymous = true;

    addNodeToDebtMap_Export(debtMap, "node2", "res2", {"node2"}, UBSE_MEM_EXPORT_SUCCESS);
    debtMap["node2"].shareExportObjMap["res2"].req.shmAnonymous = false;
    ProcessCurrentCleanList(debtMap, toClean);
    ASSERT_EQ(toClean.size(), 1);  // 预期清理一个匿名对象
    EXPECT_EQ(toClean[0].req.name, "res1");
    EXPECT_TRUE(toClean[0].req.shmAnonymous);
}

TEST_F(TestUbseMemControllerCleanShm, GetMaxRefCountExportObj_NoExportNoImport)
{
    auto result = GetMaxRefCountExportObj("res1");
    EXPECT_TRUE(result.first.req.name.empty());
    EXPECT_TRUE(result.second.empty());
}
}  // namespace ubse::mem_controller::ut