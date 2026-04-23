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

#include "test_ubse_mem_debt_ledger.h"
#include <thread>
#include <vector>
#include <atomic>

namespace ubse::mem::controller::debt::ut {

void TestUbseMemDebtLedger::SetUp()
{
    Test::SetUp();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

void TestUbseMemDebtLedger::TearDown()
{
    Test::TearDown();
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
}

UbseMemFdBorrowImportObj TestUbseMemDebtLedger::CreateFdImportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemFdBorrowImportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemFdBorrowExportObj TestUbseMemDebtLedger::CreateFdExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

UbseMemNumaBorrowImportObj TestUbseMemDebtLedger::CreateNumaImportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemNumaBorrowImportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemNumaBorrowExportObj TestUbseMemDebtLedger::CreateNumaExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemNumaBorrowExportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

UbseMemShareBorrowImportObj TestUbseMemDebtLedger::CreateShareImportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemShareBorrowImportObj obj;
    obj.req.name = name;
    obj.req.baseNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemShareBorrowExportObj TestUbseMemDebtLedger::CreateShareExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemShareBorrowExportObj obj;
    obj.req.name = name;
    obj.req.baseNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

UbseMemAddrBorrowImportObj TestUbseMemDebtLedger::CreateAddrImportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemAddrBorrowImportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemAddrBorrowExportObj TestUbseMemDebtLedger::CreateAddrExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemAddrBorrowExportObj obj;
    obj.req.name = name;
    obj.req.exportNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

void TestUbseMemNodeDebtMap::SetUp()
{
    Test::SetUp();
}

void TestUbseMemNodeDebtMap::TearDown()
{
    Test::TearDown();
}

UbseMemFdBorrowImportObj TestUbseMemNodeDebtMap::CreateTestObj(const std::string &name, UbseMemState state)
{
    UbseMemFdBorrowImportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = "node1";
    obj.req.size = 1024;
    obj.status.state = state;
    return obj;
}

void TestUbseMemTypeDebtMap::SetUp()
{
    Test::SetUp();
}

void TestUbseMemTypeDebtMap::TearDown()
{
    Test::TearDown();
}

UbseMemFdBorrowImportObj TestUbseMemTypeDebtMap::CreateTestImportObj(const std::string &name)
{
    UbseMemFdBorrowImportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = "node1";
    obj.req.size = 1024;
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemFdBorrowExportObj TestUbseMemTypeDebtMap::CreateTestExportObj(const std::string &name)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = "node1";
    obj.req.size = 1024;
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

void TestTypeTraits::SetUp()
{
    Test::SetUp();
}

void TestTypeTraits::TearDown()
{
    Test::TearDown();
}

// Tests `IsExportObjType` with fd export obj and expects a true result.
TEST_F(TestTypeTraits, IsExportObjType_FdExportObj_ShouldBeTrue)
{
    EXPECT_TRUE(IsExportObjTypeV<UbseMemFdBorrowExportObj>);
    EXPECT_TRUE(IsExportObjType<UbseMemFdBorrowExportObj>::value);
}

// Tests `IsExportObjType` with numa export obj and expects a true result.
TEST_F(TestTypeTraits, IsExportObjType_NumaExportObj_ShouldBeTrue)
{
    EXPECT_TRUE(IsExportObjTypeV<UbseMemNumaBorrowExportObj>);
    EXPECT_TRUE(IsExportObjType<UbseMemNumaBorrowExportObj>::value);
}

// Tests `IsExportObjType` with share export obj and expects a true result.
TEST_F(TestTypeTraits, IsExportObjType_ShareExportObj_ShouldBeTrue)
{
    EXPECT_TRUE(IsExportObjTypeV<UbseMemShareBorrowExportObj>);
    EXPECT_TRUE(IsExportObjType<UbseMemShareBorrowExportObj>::value);
}

// Tests `IsExportObjType` with addr export obj and expects a true result.
TEST_F(TestTypeTraits, IsExportObjType_AddrExportObj_ShouldBeTrue)
{
    EXPECT_TRUE(IsExportObjTypeV<UbseMemAddrBorrowExportObj>);
    EXPECT_TRUE(IsExportObjType<UbseMemAddrBorrowExportObj>::value);
}

// Tests `IsExportObjType` with import objects and expects a false result.
TEST_F(TestTypeTraits, IsExportObjType_ImportObj_ShouldBeFalse)
{
    EXPECT_FALSE(IsExportObjTypeV<UbseMemFdBorrowImportObj>);
    EXPECT_FALSE(IsExportObjTypeV<UbseMemNumaBorrowImportObj>);
    EXPECT_FALSE(IsExportObjTypeV<UbseMemShareBorrowImportObj>);
    EXPECT_FALSE(IsExportObjTypeV<UbseMemAddrBorrowImportObj>);
}

// Tests `IsExportObjType` with other types and expects a false result.
TEST_F(TestTypeTraits, IsExportObjType_OtherTypes_ShouldBeFalse)
{
    EXPECT_FALSE(IsExportObjTypeV<int>);
    EXPECT_FALSE(IsExportObjTypeV<std::string>);
    EXPECT_FALSE(IsExportObjTypeV<UbseMemFdBorrowReq>);
}

// Tests `IsAddrType` with addr import obj and expects a true result.
TEST_F(TestTypeTraits, IsAddrType_AddrImportObj_ShouldBeTrue)
{
    EXPECT_TRUE(IsAddrTypeV<UbseMemAddrBorrowImportObj>);
    EXPECT_TRUE(IsAddrType<UbseMemAddrBorrowImportObj>::value);
}

// Tests `IsAddrType` with addr export obj and expects a true result.
TEST_F(TestTypeTraits, IsAddrType_AddrExportObj_ShouldBeTrue)
{
    EXPECT_TRUE(IsAddrTypeV<UbseMemAddrBorrowExportObj>);
    EXPECT_TRUE(IsAddrType<UbseMemAddrBorrowExportObj>::value);
}

// Tests `IsAddrType` with other types and expects a false result.
TEST_F(TestTypeTraits, IsAddrType_OtherTypes_ShouldBeFalse)
{
    EXPECT_FALSE(IsAddrTypeV<UbseMemFdBorrowImportObj>);
    EXPECT_FALSE(IsAddrTypeV<UbseMemFdBorrowExportObj>);
    EXPECT_FALSE(IsAddrTypeV<UbseMemNumaBorrowImportObj>);
    EXPECT_FALSE(IsAddrTypeV<int>);
}

// Tests `Put` with new resource and expects the operation to succeed.
TEST_F(TestUbseMemNodeDebtMap, Put_NewResource_ShouldSucceed)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "test_resource";
    obj->status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;

    debtMap.Put("res1", obj);

    auto result = debtMap.Get("res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "test_resource");
}

// Tests `Put` with an existing resource and expects the existing resource to be updated.
TEST_F(TestUbseMemNodeDebtMap, Put_ExistingResource_ShouldUpdate)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj1 = std::make_shared<UbseMemFdBorrowImportObj>();
    obj1->req.name = "original";
    obj1->req.size = 1024;

    auto obj2 = std::make_shared<UbseMemFdBorrowImportObj>();
    obj2->req.name = "updated";
    obj2->req.size = 2048;

    debtMap.Put("res1", obj1);
    debtMap.Put("res1", obj2);

    auto result = debtMap.Get("res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "updated");
    EXPECT_EQ(result->req.size, 2048u);
}

// Tests `Put` with an empty resource ID and expects the operation to still work.
TEST_F(TestUbseMemNodeDebtMap, Put_EmptyResourceId_ShouldStillWork)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();

    debtMap.Put("", obj);

    auto result = debtMap.Get("");
    EXPECT_NE(result, nullptr);
}

// Tests `Get` with an existing resource and expects the stored resource to be returned.
TEST_F(TestUbseMemNodeDebtMap, Get_ExistingResource_ShouldReturnResource)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "test";
    debtMap.Put("res1", obj);

    auto result = debtMap.Get("res1");

    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "test");
}

// Tests `Get` with a missing resource and expects nullptr.
TEST_F(TestUbseMemNodeDebtMap, Get_NonExistingResource_ShouldReturnNullptr)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;

    auto result = debtMap.Get("non_existent");

    EXPECT_EQ(result, nullptr);
}

// Tests `Get` with an empty resource ID and verifies the expected result.
TEST_F(TestUbseMemNodeDebtMap, Get_EmptyResourceId_ShouldReturnNullptrIfNotExists)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;

    auto result = debtMap.Get("");

    EXPECT_EQ(result, nullptr);
}

// Tests `Remove` with an existing resource and expects true.
TEST_F(TestUbseMemNodeDebtMap, Remove_ExistingResource_ShouldReturnTrue)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    debtMap.Put("res1", obj);

    bool result = debtMap.Remove("res1");

    EXPECT_TRUE(result);
    EXPECT_EQ(debtMap.Get("res1"), nullptr);
}

// Tests `Remove` with a missing resource and expects false.
TEST_F(TestUbseMemNodeDebtMap, Remove_NonExistingResource_ShouldReturnFalse)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;

    bool result = debtMap.Remove("non_existent");

    EXPECT_FALSE(result);
}

// Tests `Remove` with an empty resource ID and expects false.
TEST_F(TestUbseMemNodeDebtMap, Remove_EmptyResourceId_ShouldReturnFalse)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;

    bool result = debtMap.Remove("");

    EXPECT_FALSE(result);
}

// Tests `Remove` with after remove and verifies the expected result.
TEST_F(TestUbseMemNodeDebtMap, Remove_AfterRemove_ShouldNotBeAccessible)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    debtMap.Put("res1", obj);

    debtMap.Remove("res1");

    EXPECT_EQ(debtMap.Get("res1"), nullptr);
}

// Tests `Modify` with an existing resource and expects the resource to be modified successfully.
TEST_F(TestUbseMemNodeDebtMap, Modify_ExistingResource_ShouldModifySuccessfully)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "original";
    obj->req.size = 1024;
    debtMap.Put("res1", obj);

    debtMap.Modify("res1", [](UbseMemFdBorrowImportObj &o) {
        o.req.name = "modified";
        o.req.size = 2048;
    });

    auto result = debtMap.Get("res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "modified");
    EXPECT_EQ(result->req.size, 2048u);
}

// Tests `Modify` with a missing resource and expects no state change.
TEST_F(TestUbseMemNodeDebtMap, Modify_NonExistingResource_ShouldDoNothing)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    bool called = false;

    debtMap.Modify("non_existent", [&called](UbseMemFdBorrowImportObj &) {
        called = true;
    });

    EXPECT_FALSE(called);
}

// Tests `Modify` and expects the original object to remain unchanged.
TEST_F(TestUbseMemNodeDebtMap, Modify_ShouldNotAffectOriginalObject)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "original";
    debtMap.Put("res1", obj);

    debtMap.Modify("res1", [](UbseMemFdBorrowImportObj &o) {
        o.req.name = "modified";
    });

    EXPECT_EQ(obj->req.name, "original");
}

// Tests `GetAll` with empty map and expects an empty map.
TEST_F(TestUbseMemNodeDebtMap, GetAll_EmptyMap_ShouldReturnEmptyMap)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;

    auto result = debtMap.GetAll();

    EXPECT_TRUE(result.empty());
}

// Tests `GetAll` with multiple resources and expects all relevant entries to be returned.
TEST_F(TestUbseMemNodeDebtMap, GetAll_MultipleResources_ShouldReturnAll)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj1 = std::make_shared<UbseMemFdBorrowImportObj>();
    obj1->req.name = "res1";
    auto obj2 = std::make_shared<UbseMemFdBorrowImportObj>();
    obj2->req.name = "res2";
    auto obj3 = std::make_shared<UbseMemFdBorrowImportObj>();
    obj3->req.name = "res3";

    debtMap.Put("key1", obj1);
    debtMap.Put("key2", obj2);
    debtMap.Put("key3", obj3);

    auto result = debtMap.GetAll();

    EXPECT_EQ(result.size(), 3u);
    EXPECT_NE(result.find("key1"), result.end());
    EXPECT_NE(result.find("key2"), result.end());
    EXPECT_NE(result.find("key3"), result.end());
}

// Tests `GetAll` with returned map is copy.
TEST_F(TestUbseMemNodeDebtMap, GetAll_ReturnedMapIsCopy)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    debtMap.Put("key1", obj);

    auto result = debtMap.GetAll();
    result["key2"] = obj;

    auto result2 = debtMap.GetAll();
    EXPECT_EQ(result2.size(), 1u);
}

// Tests thread safety under concurrent put/get operations.
TEST_F(TestUbseMemNodeDebtMap, ThreadSafety_ConcurrentPutAndGet)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    std::atomic<int> successCount{0};
    const int threadCount = 10;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&debtMap, &successCount, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
                obj->req.name = "res_" + std::to_string(i) + "_" + std::to_string(j);
                debtMap.Put(obj->req.name, obj);
                auto result = debtMap.Get(obj->req.name);
                if (result != nullptr) {
                    successCount++;
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), threadCount * operationsPerThread);
}

// Tests thread safety under concurrent modify operations.
TEST_F(TestUbseMemNodeDebtMap, ThreadSafety_ConcurrentModify)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.size = 0;
    debtMap.Put("res1", obj);

    const int threadCount = 10;
    std::vector<std::thread> threads;
    std::atomic<int> modifyCount{0};

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&debtMap, &modifyCount, i]() {
            std::string resId = "res_" + std::to_string(i);
            auto localObj = std::make_shared<UbseMemFdBorrowImportObj>();
            localObj->req.size = 0;
            debtMap.Put(resId, localObj);

            for (int j = 0; j < 100; ++j) {
                debtMap.Modify(resId, [&modifyCount](UbseMemFdBorrowImportObj &o) {
                    o.req.size++;
                    modifyCount++;
                });
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(modifyCount.load(), threadCount * 100);
    for (int i = 0; i < threadCount; ++i) {
        std::string resId = "res_" + std::to_string(i);
        auto result = debtMap.Get(resId);
        EXPECT_NE(result, nullptr);
        EXPECT_EQ(result->req.size, 100u);
    }
}

// Tests `GetOrCreateNodeMap` with a new node and expects a new node map to be created.
TEST_F(TestUbseMemTypeDebtMap, GetOrCreateNodeMap_NewNode_ShouldCreate)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    auto nodeMap = typeDebtMap.GetOrCreateNodeMap("node1");

    EXPECT_NE(nodeMap, nullptr);
}

// Tests `GetOrCreateNodeMap` with an existing node and expects the existing node map to be returned.
TEST_F(TestUbseMemTypeDebtMap, GetOrCreateNodeMap_ExistingNode_ShouldReturnExisting)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    auto nodeMap1 = typeDebtMap.GetOrCreateNodeMap("node1");
    auto nodeMap2 = typeDebtMap.GetOrCreateNodeMap("node1");

    EXPECT_EQ(nodeMap1, nodeMap2);
}

// Tests `FindNodeMap` with an existing node and expects the node map to be returned.
TEST_F(TestUbseMemTypeDebtMap, FindNodeMap_ExistingNode_ShouldReturnNodeMap)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");

    auto nodeMap = typeDebtMap.FindNodeMap("node1");

    EXPECT_NE(nodeMap, nullptr);
}

// Tests `FindNodeMap` with a missing node and expects nullptr.
TEST_F(TestUbseMemTypeDebtMap, FindNodeMap_NonExistingNode_ShouldReturnNullptr)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    auto nodeMap = typeDebtMap.FindNodeMap("non_existent");

    EXPECT_EQ(nodeMap, nullptr);
}

// Tests `FindNodeMap` with const access and expects a const pointer.
TEST_F(TestUbseMemTypeDebtMap, FindNodeMap_Const_ShouldReturnConstPointer)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");

    const auto &constMap = typeDebtMap;
    auto nodeMap = constMap.FindNodeMap("node1");

    EXPECT_NE(nodeMap, nullptr);
}

// Tests `RemoveNodeMap` with an existing node and expects true.
TEST_F(TestUbseMemTypeDebtMap, RemoveNodeMap_ExistingNode_ShouldReturnTrue)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");

    bool result = typeDebtMap.RemoveNodeMap("node1");

    EXPECT_TRUE(result);
    EXPECT_EQ(typeDebtMap.FindNodeMap("node1"), nullptr);
}

// Tests `RemoveNodeMap` with a missing node and expects false.
TEST_F(TestUbseMemTypeDebtMap, RemoveNodeMap_NonExistingNode_ShouldReturnFalse)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    bool result = typeDebtMap.RemoveNodeMap("non_existent");

    EXPECT_FALSE(result);
}

// Tests `RemoveNodeMap` with export objects and expects the export resId-to-nodeId index to be cleared.
TEST_F(TestUbseMemTypeDebtMap, RemoveNodeMap_WithExportObj_ShouldClearExportResIdToNodeIdIndex)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    obj->req.name = "res1";
    typeDebtMap.PutResource("node1", "res1", obj);

    typeDebtMap.RemoveNodeMap("node1");

    auto result = typeDebtMap.GetExportResourceByResId("res1");
    EXPECT_EQ(result, nullptr);
}

// Tests `RemoveOtherNodeMaps` and expects only the excluded node to remain.
TEST_F(TestUbseMemTypeDebtMap, RemoveOtherNodeMaps_ShouldKeepExcludedNode)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");
    typeDebtMap.GetOrCreateNodeMap("node2");
    typeDebtMap.GetOrCreateNodeMap("node3");

    typeDebtMap.RemoveOtherNodeMaps("node2");

    EXPECT_EQ(typeDebtMap.FindNodeMap("node1"), nullptr);
    EXPECT_NE(typeDebtMap.FindNodeMap("node2"), nullptr);
    EXPECT_EQ(typeDebtMap.FindNodeMap("node3"), nullptr);
}

// Tests `RemoveOtherNodeMaps` with export objects and expects the export resId-to-nodeId index to be cleared.
TEST_F(TestUbseMemTypeDebtMap, RemoveOtherNodeMaps_WithExportObj_ShouldClearExportResIdToNodeIdIndex)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;
    auto obj1 = std::make_shared<UbseMemFdBorrowExportObj>();
    obj1->req.name = "res1";
    auto obj2 = std::make_shared<UbseMemFdBorrowExportObj>();
    obj2->req.name = "res2";

    typeDebtMap.PutResource("node1", "res1", obj1);
    typeDebtMap.PutResource("node2", "res2", obj2);

    typeDebtMap.RemoveOtherNodeMaps("node2");

    EXPECT_EQ(typeDebtMap.GetExportResourceByResId("res1"), nullptr);
    EXPECT_NE(typeDebtMap.GetExportResourceByResId("res2"), nullptr);
}

// Tests `GetAllNodeMaps` with empty and verifies the expected result.
TEST_F(TestUbseMemTypeDebtMap, GetAllNodeMaps_Empty_ShouldReturnEmpty)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    auto result = typeDebtMap.GetAllNodeMaps();

    EXPECT_TRUE(result.empty());
}

// Tests `GetAllNodeMaps` with multiple nodes and expects all relevant entries to be returned.
TEST_F(TestUbseMemTypeDebtMap, GetAllNodeMaps_MultipleNodes_ShouldReturnAll)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");
    typeDebtMap.GetOrCreateNodeMap("node2");

    auto result = typeDebtMap.GetAllNodeMaps();

    EXPECT_EQ(result.size(), 2u);
}

// Tests `GetResource` with an existing resource and expects the stored resource to be returned.
TEST_F(TestUbseMemTypeDebtMap, GetResource_ExistingResource_ShouldReturnResource)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "res1";
    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetResource("node1", "res1");

    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "res1");
}

// Tests `GetResource` with a missing node and expects nullptr.
TEST_F(TestUbseMemTypeDebtMap, GetResource_NonExistingNode_ShouldReturnNullptr)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    auto result = typeDebtMap.GetResource("non_existent", "res1");

    EXPECT_EQ(result, nullptr);
}

// Tests `GetResource` with a missing resource and expects nullptr.
TEST_F(TestUbseMemTypeDebtMap, GetResource_NonExistingResource_ShouldReturnNullptr)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");

    auto result = typeDebtMap.GetResource("node1", "non_existent");

    EXPECT_EQ(result, nullptr);
}

// Tests `GetExportResourceByResId` with export objects and expects the stored resource to be returned.
TEST_F(TestUbseMemTypeDebtMap, GetExportResourceByResId_ExportObj_ShouldReturnResource)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    obj->req.name = "res1";
    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetExportResourceByResId("res1");

    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "res1");
}

// Tests `GetExportResourceByResId` with import objects and expects nullptr.
TEST_F(TestUbseMemTypeDebtMap, GetExportResourceByResId_ImportObj_ShouldReturnNullptr)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetExportResourceByResId("res1");

    EXPECT_EQ(result, nullptr);
}

// Tests `GetExportResourceByResId` with non existing and expects nullptr.
TEST_F(TestUbseMemTypeDebtMap, GetExportResourceByResId_NonExisting_ShouldReturnNullptr)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;

    auto result = typeDebtMap.GetExportResourceByResId("non_existent");

    EXPECT_EQ(result, nullptr);
}

// Tests `PutResource` with the shared-pointer overload and expects the resource to be stored.
TEST_F(TestUbseMemTypeDebtMap, PutResource_SharedPtrVersion_ShouldStoreResource)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "res1";

    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `PutResource` with the reference overload and expects the resource to be stored.
TEST_F(TestUbseMemTypeDebtMap, PutResource_ReferenceVersion_ShouldStoreResource)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = CreateTestImportObj("res1");

    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `PutResource` with export objects and expects the export resId-to-nodeId index to be updated.
TEST_F(TestUbseMemTypeDebtMap, PutResource_ExportObj_ShouldUpdateExportResIdToNodeIdIndex)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    obj->req.name = "res1";

    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetExportResourceByResId("res1");
    EXPECT_NE(result, nullptr);
}

// Tests `RemoveResource` with an existing resource and expects true.
TEST_F(TestUbseMemTypeDebtMap, RemoveResource_ExistingResource_ShouldReturnTrue)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    typeDebtMap.PutResource("node1", "res1", obj);

    bool result = typeDebtMap.RemoveResource("node1", "res1");

    EXPECT_TRUE(result);
    EXPECT_EQ(typeDebtMap.GetResource("node1", "res1"), nullptr);
}

// Tests `RemoveResource` with a missing resource and expects false.
TEST_F(TestUbseMemTypeDebtMap, RemoveResource_NonExistingResource_ShouldReturnFalse)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;

    bool result = typeDebtMap.RemoveResource("node1", "res1");

    EXPECT_FALSE(result);
}

// Tests `RemoveResource` with export objects and expects the export resId-to-nodeId index to be cleared.
TEST_F(TestUbseMemTypeDebtMap, RemoveResource_ExportObj_ShouldClearExportResIdToNodeIdIndex)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    typeDebtMap.PutResource("node1", "res1", obj);

    typeDebtMap.RemoveResource("node1", "res1");

    auto result = typeDebtMap.GetExportResourceByResId("res1");
    EXPECT_EQ(result, nullptr);
}

// Tests `ClearAllNodeMaps` and expects all data to be cleared.
TEST_F(TestUbseMemTypeDebtMap, ClearAllNodeMaps_ShouldClearAllData)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");
    typeDebtMap.GetOrCreateNodeMap("node2");

    typeDebtMap.ClearAllNodeMaps();

    EXPECT_EQ(typeDebtMap.FindNodeMap("node1"), nullptr);
    EXPECT_EQ(typeDebtMap.FindNodeMap("node2"), nullptr);
}

// Tests `ClearAllNodeMaps` with export objects and expects the export resId-to-nodeId index to be cleared.
TEST_F(TestUbseMemTypeDebtMap, ClearAllNodeMaps_WithExportObj_ShouldClearExportResIdToNodeIdIndex)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowExportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    typeDebtMap.PutResource("node1", "res1", obj);

    typeDebtMap.ClearAllNodeMaps();

    auto result = typeDebtMap.GetExportResourceByResId("res1");
    EXPECT_EQ(result, nullptr);
}


} // namespace ubse::mem::controller::debt::ut
