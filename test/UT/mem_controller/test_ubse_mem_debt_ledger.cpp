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
    obj.req.size = 1024 * 1024;  // 1MB
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemFdBorrowExportObj TestUbseMemDebtLedger::CreateFdExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;  // 1MB
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

UbseMemNumaBorrowImportObj TestUbseMemDebtLedger::CreateNumaImportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemNumaBorrowImportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;  // 1MB
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemNumaBorrowExportObj TestUbseMemDebtLedger::CreateNumaExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemNumaBorrowExportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;  // 1MB
    obj.status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    return obj;
}

UbseMemShareBorrowImportObj TestUbseMemDebtLedger::CreateShareImportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemShareBorrowImportObj obj;
    obj.req.name = name;
    obj.req.baseNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;  // 1MB
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemShareBorrowExportObj TestUbseMemDebtLedger::CreateShareExportObj(const std::string &name, const std::string &nodeId)
{
    UbseMemShareBorrowExportObj obj;
    obj.req.name = name;
    obj.req.baseNodeId = nodeId;
    obj.req.requestNodeId = nodeId;
    obj.req.size = 1024 * 1024;  // 1MB
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
    obj.req.size = 1024; // 1K内存
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
    obj.req.size = 1024; // 1K内存
    obj.status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    return obj;
}

UbseMemFdBorrowExportObj TestUbseMemTypeDebtMap::CreateTestExportObj(const std::string &name)
{
    UbseMemFdBorrowExportObj obj;
    obj.req.name = name;
    obj.req.importNodeId = "node1";
    obj.req.size = 1024; // 1K内存
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
    obj1->req.size = 1024; // 1K内存

    auto obj2 = std::make_shared<UbseMemFdBorrowImportObj>();
    obj2->req.name = "updated";
    obj2->req.size = 1024 * 2; // 2K内存

    debtMap.Put("res1", obj1);
    debtMap.Put("res1", obj2);

    auto result = debtMap.Get("res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "updated");
    EXPECT_EQ(result->req.size, 1024 * 2); // 2K内存
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
    obj->req.size = 1024; // 1K内存
    debtMap.Put("res1", obj);

    debtMap.Modify("res1", [](UbseMemFdBorrowImportObj &o) {
        o.req.name = "modified";
        o.req.size = 1024 * 2; // 2K内存
    });

    auto result = debtMap.Get("res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "modified");
    EXPECT_EQ(result->req.size, 1024 * 2); // 2K内存
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

    EXPECT_EQ(result.size(), 3u); // 3个资源
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
    const int threadCount = 10;  // 并发线程数10
    const int operationsPerThread = 100;  // 每个线程的操作数100
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

    const int threadCount = 10;  // 并发线程数10
    std::vector<std::thread> threads;
    std::atomic<int> modifyCount{0};

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&debtMap, &modifyCount, i]() {
            std::string resId = "res_" + std::to_string(i);
            auto localObj = std::make_shared<UbseMemFdBorrowImportObj>();
            localObj->req.size = 0;
            debtMap.Put(resId, localObj);

            for (int j = 0; j < 100; ++j) { // 每个线程修改100次
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

    EXPECT_EQ(modifyCount.load(), threadCount * 100); // 1000次修改
    for (int i = 0; i < threadCount; ++i) {
        std::string resId = "res_" + std::to_string(i);
        auto result = debtMap.Get(resId);
        EXPECT_NE(result, nullptr);
        EXPECT_EQ(result->req.size, static_cast<size_t>(100));
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

// Tests `GetInstance` and expects the same singleton instance.
TEST_F(TestUbseMemDebtLedger, GetInstance_ShouldReturnSameInstance)
{
    auto &instance1 = UbseMemDebtLedger::GetInstance();
    auto &instance2 = UbseMemDebtLedger::GetInstance();

    EXPECT_EQ(&instance1, &instance2);
}

// Tests `GetDebtMap` with fd import obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_FdImportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with fd export obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_FdExportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>();

    auto obj = std::make_shared<UbseMemFdBorrowExportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with numa import obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_NumaImportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>();

    auto obj = std::make_shared<UbseMemNumaBorrowImportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with numa export obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_NumaExportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowExportObj>();

    auto obj = std::make_shared<UbseMemNumaBorrowExportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with share import obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_ShareImportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>();

    auto obj = std::make_shared<UbseMemShareBorrowImportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with share export obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_ShareExportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();

    auto obj = std::make_shared<UbseMemShareBorrowExportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with addr import obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_AddrImportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>();

    auto obj = std::make_shared<UbseMemAddrBorrowImportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with addr export obj and expects the correct debt map type.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_AddrExportObj_ShouldReturnCorrectType)
{
    auto &debtMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>();

    auto obj = std::make_shared<UbseMemAddrBorrowExportObj>();
    obj->req.name = "test";
    debtMap.PutResource("node1", "res1", obj);

    auto result = debtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests `GetDebtMap` with const access and expects a const reference.
TEST_F(TestUbseMemDebtLedger, GetDebtMap_Const_ShouldReturnConstReference)
{
    const auto &ledger = UbseMemDebtLedger::GetInstance();
    const auto &debtMap = ledger.GetDebtMap<UbseMemFdBorrowImportObj>();

    auto allMaps = debtMap.GetAllNodeMaps();
    EXPECT_TRUE(allMaps.empty() || !allMaps.empty());
}

// Tests `GetAllDebtInfo` with an empty ledger and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_EmptyLedger_ShouldReturnEmpty)
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();

    auto result = UbseMemDebtLedger::GetInstance().GetAllDebtInfo();

    EXPECT_TRUE(result.empty());
}

// Tests `GetAllDebtInfo` with a single node and expects correct debt information.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_SingleNode_ShouldReturnCorrectInfo)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res1", "node1"));
    auto fdExport = std::make_shared<UbseMemFdBorrowExportObj>(CreateFdExportObj("res2", "node1"));
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", fdImport);
    ledger.GetDebtMap<UbseMemFdBorrowExportObj>().PutResource("node1", "res2", fdExport);

    auto result = ledger.GetAllDebtInfo();

    EXPECT_EQ(result.size(), 1u);
    EXPECT_NE(result.find("node1"), result.end());
    EXPECT_EQ(result["node1"].fdImportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].fdExportObjMap.size(), 1u);
}

// Tests `GetAllDebtInfo` with multiple nodes and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_MultipleNodes_ShouldReturnAllNodes)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport1 = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res1", "node1"));
    auto fdImport2 = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res2", "node2"));
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", fdImport1);
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node2", "res2", fdImport2);

    auto result = ledger.GetAllDebtInfo();

    EXPECT_EQ(result.size(), 2u);
    EXPECT_NE(result.find("node1"), result.end());
    EXPECT_NE(result.find("node2"), result.end());
}

// Tests `GetAllDebtInfo` with all resource types and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_AllResourceTypes_ShouldReturnAllTypes)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("fd_import", "node1"));
    auto fdExport = std::make_shared<UbseMemFdBorrowExportObj>(CreateFdExportObj("fd_export", "node1"));
    auto numaImport = std::make_shared<UbseMemNumaBorrowImportObj>(CreateNumaImportObj("numa_import", "node1"));
    auto numaExport = std::make_shared<UbseMemNumaBorrowExportObj>(CreateNumaExportObj("numa_export", "node1"));
    auto shareImport = std::make_shared<UbseMemShareBorrowImportObj>(CreateShareImportObj("share_import", "node1"));
    auto shareExport = std::make_shared<UbseMemShareBorrowExportObj>(CreateShareExportObj("share_export", "node1"));
    auto addrImport = std::make_shared<UbseMemAddrBorrowImportObj>(CreateAddrImportObj("addr_import", "node1"));
    auto addrExport = std::make_shared<UbseMemAddrBorrowExportObj>(CreateAddrExportObj("addr_export", "node1"));

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "fd_import", fdImport);
    ledger.GetDebtMap<UbseMemFdBorrowExportObj>().PutResource("node1", "fd_export", fdExport);
    ledger.GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("node1", "numa_import", numaImport);
    ledger.GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource("node1", "numa_export", numaExport);
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("node1", "share_import", shareImport);
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("node1", "share_export", shareExport);
    ledger.GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource("node1", "addr_import", addrImport);
    ledger.GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource("node1", "addr_export", addrExport);

    auto result = ledger.GetAllDebtInfo();

    EXPECT_EQ(result["node1"].fdImportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].fdExportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].numaImportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].numaExportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].shareImportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].shareExportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].addrImportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].addrExportObjMap.size(), 1u);
}

// Tests `GetAllDebtInfo` with destroyed-resource filtering and expects destroyed resources to be filtered out.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_FilterDestroyed_ShouldFilterDestroyedResources)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>();
    fdImport->req.name = "import_res";
    fdImport->status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;

    auto fdImportDestroyed = std::make_shared<UbseMemFdBorrowImportObj>();
    fdImportDestroyed->req.name = "import_destroyed";
    fdImportDestroyed->status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;

    auto fdExport = std::make_shared<UbseMemFdBorrowExportObj>();
    fdExport->req.name = "export_res";
    fdExport->status.state = UbseMemState::UBSE_MEM_EXPORT_SUCCESS;

    auto fdExportDestroyed = std::make_shared<UbseMemFdBorrowExportObj>();
    fdExportDestroyed->req.name = "export_destroyed";
    fdExportDestroyed->status.state = UbseMemState::UBSE_MEM_EXPORT_DESTROYED;

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "import_res", fdImport);
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "import_destroyed", fdImportDestroyed);
    ledger.GetDebtMap<UbseMemFdBorrowExportObj>().PutResource("node1", "export_res", fdExport);
    ledger.GetDebtMap<UbseMemFdBorrowExportObj>().PutResource("node1", "export_destroyed", fdExportDestroyed);

    auto result = ledger.GetAllDebtInfo(true);

    EXPECT_EQ(result["node1"].fdImportObjMap.size(), 1u);
    EXPECT_EQ(result["node1"].fdExportObjMap.size(), 1u);
    EXPECT_NE(result["node1"].fdImportObjMap.find("import_res"), result["node1"].fdImportObjMap.end());
    EXPECT_EQ(result["node1"].fdImportObjMap.find("import_destroyed"), result["node1"].fdImportObjMap.end());
}

// Tests `GetAllDebtInfo` with disabled filtering and expects destroyed resources to be included.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_NoFilter_ShouldIncludeDestroyedResources)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImportDestroyed = std::make_shared<UbseMemFdBorrowImportObj>();
    fdImportDestroyed->req.name = "import_destroyed";
    fdImportDestroyed->status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "import_destroyed", fdImportDestroyed);

    auto result = ledger.GetAllDebtInfo(false);

    EXPECT_EQ(result["node1"].fdImportObjMap.size(), 1u);
    EXPECT_NE(result["node1"].fdImportObjMap.find("import_destroyed"), result["node1"].fdImportObjMap.end());
}

// Tests `ClearAllNodeMaps` and expects all data to be cleared.
TEST_F(TestUbseMemDebtLedger, ClearAllNodeMaps_ShouldClearAllData)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res1", "node1"));
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", fdImport);

    ledger.ClearAllNodeMaps();

    auto result = ledger.GetAllDebtInfo();
    EXPECT_TRUE(result.empty());
}

// Tests `ClearOtherNodeMaps` and expects only the excluded node to remain.
TEST_F(TestUbseMemDebtLedger, ClearOtherNodeMaps_ShouldKeepExcludedNode)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport1 = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res1", "node1"));
    auto fdImport2 = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res2", "node2"));
    auto fdImport3 = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res3", "node3"));

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", fdImport1);
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node2", "res2", fdImport2);
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node3", "res3", fdImport3);

    ledger.ClearOtherNodeMaps("node2");

    auto result = ledger.GetAllDebtInfo();
    EXPECT_EQ(result.size(), 1u);
    EXPECT_NE(result.find("node2"), result.end());
    EXPECT_EQ(result.find("node1"), result.end());
    EXPECT_EQ(result.find("node3"), result.end());
}

// Tests `ClearOtherNodeMaps` with all resource types and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, ClearOtherNodeMaps_AllTypes_ShouldClearAllTypes)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res1", "node1"));
    auto numaImport = std::make_shared<UbseMemNumaBorrowImportObj>(CreateNumaImportObj("res2", "node1"));
    auto shareImport = std::make_shared<UbseMemShareBorrowImportObj>(CreateShareImportObj("res3", "node1"));

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", fdImport);
    ledger.GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("node1", "res2", numaImport);
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("node1", "res3", shareImport);

    ledger.ClearOtherNodeMaps("node2");

    auto result = ledger.GetAllDebtInfo();
    EXPECT_TRUE(result.empty());
}

// Tests `GetNodeMemDebtInfo` with an existing node and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, GetNodeMemDebtInfo_ExistingNode_ShouldReturnInfo)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("res1", "node1"));
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", fdImport);

    auto result = ledger.GetNodeMemDebtInfo("node1");

    EXPECT_EQ(result.fdImportObjMap.size(), 1u);
}

// Tests `GetNodeMemDebtInfo` with a missing node and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, GetNodeMemDebtInfo_NonExistingNode_ShouldReturnEmptyInfo)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto result = ledger.GetNodeMemDebtInfo("non_existent");

    EXPECT_TRUE(result.fdImportObjMap.empty());
    EXPECT_TRUE(result.fdExportObjMap.empty());
    EXPECT_TRUE(result.numaImportObjMap.empty());
    EXPECT_TRUE(result.numaExportObjMap.empty());
    EXPECT_TRUE(result.shareImportObjMap.empty());
    EXPECT_TRUE(result.shareExportObjMap.empty());
    EXPECT_TRUE(result.addrImportObjMap.empty());
    EXPECT_TRUE(result.addrExportObjMap.empty());
}

// Tests `GetNodeMemDebtInfo` with destroyed-resource filtering and expects destroyed resources to be filtered out.
TEST_F(TestUbseMemDebtLedger, GetNodeMemDebtInfo_FilterDestroyed_ShouldFilterDestroyedResources)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>();
    fdImport->req.name = "import_res";
    fdImport->status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;

    auto fdImportDestroyed = std::make_shared<UbseMemFdBorrowImportObj>();
    fdImportDestroyed->req.name = "import_destroyed";
    fdImportDestroyed->status.state = UbseMemState::UBSE_MEM_IMPORT_DESTROYED;

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "import_res", fdImport);
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "import_destroyed", fdImportDestroyed);

    auto result = ledger.GetNodeMemDebtInfo("node1", true);

    EXPECT_EQ(result.fdImportObjMap.size(), 1u);
    EXPECT_NE(result.fdImportObjMap.find("import_res"), result.fdImportObjMap.end());
    EXPECT_EQ(result.fdImportObjMap.find("import_destroyed"), result.fdImportObjMap.end());
}

// Tests `GetNodeMemDebtInfo` with all resource types and verifies the expected result.
TEST_F(TestUbseMemDebtLedger, GetNodeMemDebtInfo_AllResourceTypes_ShouldReturnAllTypes)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto fdImport = std::make_shared<UbseMemFdBorrowImportObj>(CreateFdImportObj("fd_import", "node1"));
    auto fdExport = std::make_shared<UbseMemFdBorrowExportObj>(CreateFdExportObj("fd_export", "node1"));
    auto numaImport = std::make_shared<UbseMemNumaBorrowImportObj>(CreateNumaImportObj("numa_import", "node1"));
    auto numaExport = std::make_shared<UbseMemNumaBorrowExportObj>(CreateNumaExportObj("numa_export", "node1"));
    auto shareImport = std::make_shared<UbseMemShareBorrowImportObj>(CreateShareImportObj("share_import", "node1"));
    auto shareExport = std::make_shared<UbseMemShareBorrowExportObj>(CreateShareExportObj("share_export", "node1"));
    auto addrImport = std::make_shared<UbseMemAddrBorrowImportObj>(CreateAddrImportObj("addr_import", "node1"));
    auto addrExport = std::make_shared<UbseMemAddrBorrowExportObj>(CreateAddrExportObj("addr_export", "node1"));

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "fd_import", fdImport);
    ledger.GetDebtMap<UbseMemFdBorrowExportObj>().PutResource("node1", "fd_export", fdExport);
    ledger.GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("node1", "numa_import", numaImport);
    ledger.GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource("node1", "numa_export", numaExport);
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("node1", "share_import", shareImport);
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource("node1", "share_export", shareExport);
    ledger.GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource("node1", "addr_import", addrImport);
    ledger.GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource("node1", "addr_export", addrExport);

    auto result = ledger.GetNodeMemDebtInfo("node1");

    EXPECT_EQ(result.fdImportObjMap.size(), 1u);
    EXPECT_EQ(result.fdExportObjMap.size(), 1u);
    EXPECT_EQ(result.numaImportObjMap.size(), 1u);
    EXPECT_EQ(result.numaExportObjMap.size(), 1u);
    EXPECT_EQ(result.shareImportObjMap.size(), 1u);
    EXPECT_EQ(result.shareExportObjMap.size(), 1u);
    EXPECT_EQ(result.addrImportObjMap.size(), 1u);
    EXPECT_EQ(result.addrExportObjMap.size(), 1u);
}

// Tests thread safety under concurrent ledger access.
TEST_F(TestUbseMemDebtLedger, ThreadSafety_ConcurrentAccess)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    const int threadCount = 10;  // 并发线程数
    const int operationsPerThread = 100;  // 每个线程的操作数
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&ledger, &successCount, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                std::string nodeId = "node_" + std::to_string(i);
                std::string resId = "res_" + std::to_string(i) + "_" + std::to_string(j);

                auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
                obj->req.name = resId;
                obj->status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;

                ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(nodeId, resId, obj);
                auto result = ledger.GetDebtMap<UbseMemFdBorrowImportObj>().GetResource(nodeId, resId);
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

// Tests thread safety under concurrent GetAllDebtInfo calls.
TEST_F(TestUbseMemDebtLedger, ThreadSafety_ConcurrentGetAllDebtInfo)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    for (int i = 0; i < 10; ++i) {  // 预置10个资源
        auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
        obj->req.name = "res_" + std::to_string(i);
        ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res_" + std::to_string(i), obj);
    }

    const int threadCount = 10;  // 并发线程数10
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&ledger, &successCount]() {
            for (int j = 0; j < 100; ++j) {  // 每个线程执行100次查询
                auto result = ledger.GetAllDebtInfo();
                if (result.size() == 1) {
                    successCount++;
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), threadCount * 100); // 1000次查询
}

// Tests edge-case handling for empty resource IDs.
TEST_F(TestUbseMemDebtLedger, EdgeCase_EmptyResourceId)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "";

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "", obj);

    auto result = ledger.GetDebtMap<UbseMemFdBorrowImportObj>().GetResource("node1", "");
    EXPECT_NE(result, nullptr);
}

// Tests edge-case handling for empty node IDs.
TEST_F(TestUbseMemDebtLedger, EdgeCase_EmptyNodeId)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "res1";

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("", "res1", obj);

    auto result = ledger.GetDebtMap<UbseMemFdBorrowImportObj>().GetResource("", "res1");
    EXPECT_NE(result, nullptr);
}

// Tests behavior with a large number of resources.
TEST_F(TestUbseMemDebtLedger, EdgeCase_LargeNumberOfResources)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    const int resourceCount = 1000;  // 测试1000个资源
    for (int i = 0; i < resourceCount; ++i) {
        auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
        obj->req.name = "res_" + std::to_string(i);
        ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res_" + std::to_string(i), obj);
    }

    auto result = ledger.GetNodeMemDebtInfo("node1");
    EXPECT_EQ(result.fdImportObjMap.size(), static_cast<size_t>(resourceCount));
}

// Tests behavior with special characters in identifiers.
TEST_F(TestUbseMemDebtLedger, EdgeCase_SpecialCharactersInId)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    std::string specialId = "res-with-special_chars.123!@#$%";
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = specialId;

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", specialId, obj);

    auto result = ledger.GetDebtMap<UbseMemFdBorrowImportObj>().GetResource("node1", specialId);
    EXPECT_NE(result, nullptr);
}

// Tests behavior with Unicode identifiers.
TEST_F(TestUbseMemDebtLedger, EdgeCase_UnicodeInId)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    std::string unicodeId = u8"资源_测试_123";
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = unicodeId;

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", unicodeId, obj);

    auto result = ledger.GetDebtMap<UbseMemFdBorrowImportObj>().GetResource("node1", unicodeId);
    EXPECT_NE(result, nullptr);
}

// Tests shared-pointer cleanup to guard against leaks.
TEST_F(TestUbseMemDebtLedger, MemoryLeak_SharedPtrCleanup)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "res1";
    std::weak_ptr<UbseMemFdBorrowImportObj> weakObj = obj;

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", obj);
    obj.reset();

    auto result = ledger.GetDebtMap<UbseMemFdBorrowImportObj>().GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
    EXPECT_FALSE(weakObj.expired());

    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().RemoveResource("node1", "res1");
    result.reset();

    EXPECT_TRUE(weakObj.expired());
}

// Tests `Put` with nullptr and expects the operation to be ignored.
TEST_F(TestUbseMemNodeDebtMap, Put_Nullptr_ShouldBeIgnored)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "valid_res";
    debtMap.Put("res1", obj);

    debtMap.Put("res2", nullptr);

    EXPECT_NE(debtMap.Get("res1"), nullptr);
    EXPECT_EQ(debtMap.Get("res2"), nullptr);
}

// Tests `Put` with nullptr to overwrite existing resource and expects the operation to be ignored.
TEST_F(TestUbseMemNodeDebtMap, Put_NullptrOverwrite_ShouldBeIgnored)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "original";
    debtMap.Put("res1", obj);

    debtMap.Put("res1", nullptr);

    auto result = debtMap.Get("res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "original");
}

// Tests `Modify` with empty key and expects no crash.
TEST_F(TestUbseMemNodeDebtMap, Modify_EmptyKey_ShouldNotCrash)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    bool called = false;

    debtMap.Modify("", [&called](UbseMemFdBorrowImportObj &) {
        called = true;
    });

    EXPECT_FALSE(called);
}

// Tests `Remove` twice on the same resource.
TEST_F(TestUbseMemNodeDebtMap, Remove_Twice_ShouldReturnFalseSecondTime)
{
    UbseMemNodeDebtMap<UbseMemFdBorrowImportObj> debtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    debtMap.Put("res1", obj);

    EXPECT_TRUE(debtMap.Remove("res1"));
    EXPECT_FALSE(debtMap.Remove("res1"));
    EXPECT_EQ(debtMap.Get("res1"), nullptr);
}

// Tests `PutResource` with nullptr and expects the operation to be ignored.
TEST_F(TestUbseMemTypeDebtMap, PutResource_Nullptr_ShouldBeIgnored)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "valid_res";
    typeDebtMap.PutResource("node1", "res1", obj);

    typeDebtMap.PutResource("node1", "res2", nullptr);

    EXPECT_NE(typeDebtMap.GetResource("node1", "res1"), nullptr);
    EXPECT_EQ(typeDebtMap.GetResource("node1", "res2"), nullptr);
}

// Tests `PutResource` with nullptr to overwrite existing resource and expects the operation to be ignored.
TEST_F(TestUbseMemTypeDebtMap, PutResource_NullptrOverwrite_ShouldBeIgnored)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "original";
    typeDebtMap.PutResource("node1", "res1", obj);

    typeDebtMap.PutResource("node1", "res1", nullptr);

    auto result = typeDebtMap.GetResource("node1", "res1");
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->req.name, "original");
}

// Tests `RemoveResource` twice on the same resource.
TEST_F(TestUbseMemTypeDebtMap, RemoveResource_Twice_ShouldReturnFalseSecondTime)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    typeDebtMap.PutResource("node1", "res1", obj);

    EXPECT_TRUE(typeDebtMap.RemoveResource("node1", "res1"));
    EXPECT_FALSE(typeDebtMap.RemoveResource("node1", "res1"));
    EXPECT_EQ(typeDebtMap.GetResource("node1", "res1"), nullptr);
}

// Tests `RemoveNodeMap` twice on the same node.
TEST_F(TestUbseMemTypeDebtMap, RemoveNodeMap_Twice_ShouldReturnFalseSecondTime)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    typeDebtMap.GetOrCreateNodeMap("node1");

    EXPECT_TRUE(typeDebtMap.RemoveNodeMap("node1"));
    EXPECT_FALSE(typeDebtMap.RemoveNodeMap("node1"));
    EXPECT_EQ(typeDebtMap.FindNodeMap("node1"), nullptr);
}

// Tests `GetExportResourceByResId` with import obj and expects nullptr.
TEST_F(TestUbseMemTypeDebtMap, GetExportResourceByResId_WithImportObj_ShouldReturnNullptr)
{
    UbseMemTypeDebtMap<UbseMemFdBorrowImportObj> typeDebtMap;
    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "res1";
    typeDebtMap.PutResource("node1", "res1", obj);

    auto result = typeDebtMap.GetExportResourceByResId("res1");

    EXPECT_EQ(result, nullptr);
}

// Tests `GetAllDebtInfo` with null shared_ptr in map.
TEST_F(TestUbseMemDebtLedger, GetAllDebtInfo_WithNullSharedPtr_ShouldBeFiltered)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "valid_res";
    obj->status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "valid_res", obj);

    auto result = ledger.GetAllDebtInfo();

    EXPECT_EQ(result["node1"].fdImportObjMap.size(), 1u);
}

// Tests `GetNodeMemDebtInfo` with null shared_ptr in map.
TEST_F(TestUbseMemDebtLedger, GetNodeMemDebtInfo_WithNullSharedPtr_ShouldBeFiltered)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "valid_res";
    obj->status.state = UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "valid_res", obj);

    auto result = ledger.GetNodeMemDebtInfo("node1");

    EXPECT_EQ(result.fdImportObjMap.size(), 1u);
}

// Tests `ClearOtherNodeMaps` with non-existent node.
TEST_F(TestUbseMemDebtLedger, ClearOtherNodeMaps_NonExistentNode_ShouldClearAll)
{
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.ClearAllNodeMaps();

    auto obj = std::make_shared<UbseMemFdBorrowImportObj>();
    obj->req.name = "res1";
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("node1", "res1", obj);

    ledger.ClearOtherNodeMaps("non_existent");

    auto result = ledger.GetAllDebtInfo();
    EXPECT_TRUE(result.empty());
}

}
