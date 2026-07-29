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

#include "test_ubse_mem_share_capabilities.h"

#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_share_store.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;

class MockShareStore : public IShareStore {
public:
    bool ExistBorrow(const std::string &name) override { return existBorrowResult; }
    bool ExistAttach(const std::string &importNodeId, const std::string &name) override { return existAttachResult; }
    UbseResult LoadExport(const std::string &name, UbseMemShareBorrowExportObj &out) override { return UBSE_OK; }
    UbseResult LoadImport(const std::string &importNodeId, const std::string &name,
                          UbseMemShareBorrowImportObj &out) override { return UBSE_OK; }
    UbseResult LoadAllImports(const std::string &name,
                              std::vector<UbseMemShareBorrowImportObj> &out) override { return UBSE_OK; }

    void ForEachExport(ExportVisitor visitor) override {}
    void ForEachImport(ImportVisitor visitor) override {}

    void PutExport(const UbseMemShareBorrowExportObj &obj) override {}
    void PutImport(const UbseMemShareBorrowImportObj &obj) override {}
    void UpdateExportState(const UbseMemShareBorrowExportObj &obj, const UbseMemState &state) override {}
    void UpdateImportState(const UbseMemShareBorrowImportObj &obj, const UbseMemState &state) override {}
    void UpdateExportMemIds(const UbseMemShareBorrowExportObj &obj) override {}
    void UpdateImportMemIds(const UbseMemShareBorrowImportObj &obj) override {}
    void RemoveExport(const UbseMemShareBorrowExportObj &obj) override {}
    void RemoveImport(const UbseMemShareBorrowImportObj &obj) override {}

    uint32_t GetCnaTopo(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj &exportObj,
                        UbseMemShareBorrowImportObj &importObj) override { return UBSE_OK; }

    bool existBorrowResult{false};
    bool existAttachResult{false};
};

void TestShareCapabilities::SetUp()
{
    Test::SetUp();
}

void TestShareCapabilities::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// SC-01: CheckBorrowDuplicate returns true when store.ExistBorrow returns true
TEST_F(TestShareCapabilities, SC01_CheckBorrowDuplicate_NameExists_ReturnsTrue)
{
    MockShareStore store;
    store.existBorrowResult = true;
    EXPECT_TRUE(CheckBorrowDuplicate("test_shm", store));
}

// SC-02: CheckBorrowDuplicate returns false when name not found
TEST_F(TestShareCapabilities, SC02_CheckBorrowDuplicate_NotExists_ReturnsFalse)
{
    MockShareStore store;
    store.existBorrowResult = false;
    EXPECT_FALSE(CheckBorrowDuplicate("test_shm", store));
}

// SC-03: CheckRegions returns true when importNodeId is in exportObj's nodelist
TEST_F(TestShareCapabilities, SC03_CheckRegions_ImportNodeInList_ReturnsTrue)
{
    UbseMemShareAttachReq req;
    req.importNodeId = "node2";

    UbseMemShareBorrowExportObj exportObj;
    UbseNodeInfo nodeInfo1;
    nodeInfo1.nodeId = "node1";
    UbseNodeInfo nodeInfo2;
    nodeInfo2.nodeId = "node2";
    exportObj.req.shmRegion.nodelist.push_back(nodeInfo1);
    exportObj.req.shmRegion.nodelist.push_back(nodeInfo2);

    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "node1";
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);

    EXPECT_TRUE(CheckRegions(req, exportObj));
}

// SC-04: CheckRegions returns false when importNodeId not in nodelist
TEST_F(TestShareCapabilities, SC04_CheckRegions_NotInList_ReturnsFalse)
{
    UbseMemShareAttachReq req;
    req.importNodeId = "node3";

    UbseMemShareBorrowExportObj exportObj;
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "node1";
    exportObj.req.shmRegion.nodelist.push_back(nodeInfo);

    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = "node1";
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);

    EXPECT_FALSE(CheckRegions(req, exportObj));
}

// SC-05: CheckRegions returns false when exportNumaInfos is empty
TEST_F(TestShareCapabilities, SC05_CheckRegions_EmptyNumaInfo_ReturnsFalse)
{
    UbseMemShareAttachReq req;
    req.importNodeId = "node1";

    UbseMemShareBorrowExportObj exportObj;
    UbseNodeInfo nodeInfo;
    nodeInfo.nodeId = "node1";
    exportObj.req.shmRegion.nodelist.push_back(nodeInfo);

    EXPECT_FALSE(CheckRegions(req, exportObj));
}

// SC-06: ExistImportObj returns true when matching active import found
TEST_F(TestShareCapabilities, SC06_ExistImportObj_Active_ReturnsTrueWithObj)
{
    std::vector<UbseMemShareBorrowImportObj> existImportObjs;

    UbseMemShareBorrowImportObj obj1;
    obj1.req.name = "test_shm";
    obj1.importNodeId = "node1";
    obj1.status.state = UBSE_MEM_IMPORT_SUCCESS;
    existImportObjs.push_back(obj1);

    UbseMemShareBorrowImportObj resultObj;
    EXPECT_TRUE(ExistImportObj("test_shm", "node1", existImportObjs, resultObj));
    EXPECT_EQ(resultObj.req.name, "test_shm");
    EXPECT_EQ(resultObj.importNodeId, "node1");
    EXPECT_EQ(resultObj.status.state, UBSE_MEM_IMPORT_SUCCESS);
}

// SC-07: ExistImportObj returns false when matching import is destroyed
TEST_F(TestShareCapabilities, SC07_ExistImportObj_Destroyed_ReturnsFalse)
{
    std::vector<UbseMemShareBorrowImportObj> existImportObjs;

    UbseMemShareBorrowImportObj obj1;
    obj1.req.name = "test_shm";
    obj1.importNodeId = "node1";
    obj1.status.state = UBSE_MEM_IMPORT_DESTROYED;
    existImportObjs.push_back(obj1);

    UbseMemShareBorrowImportObj resultObj;
    EXPECT_FALSE(ExistImportObj("test_shm", "node1", existImportObjs, resultObj));
}

// SC-08: ExistImportObj returns false when no name match
TEST_F(TestShareCapabilities, SC08_ExistImportObj_NoNameMatch_ReturnsFalse)
{
    std::vector<UbseMemShareBorrowImportObj> existImportObjs;

    UbseMemShareBorrowImportObj obj1;
    obj1.req.name = "other_shm";
    obj1.importNodeId = "node1";
    obj1.status.state = UBSE_MEM_IMPORT_SUCCESS;
    existImportObjs.push_back(obj1);

    UbseMemShareBorrowImportObj resultObj;
    EXPECT_FALSE(ExistImportObj("test_shm", "node1", existImportObjs, resultObj));
}

// SC-09: ExistImportObj returns false when name matches but nodeId differs
TEST_F(TestShareCapabilities, SC09_ExistImportObj_NoNodeIdMatch_ReturnsFalse)
{
    std::vector<UbseMemShareBorrowImportObj> existImportObjs;

    UbseMemShareBorrowImportObj obj1;
    obj1.req.name = "test_shm";
    obj1.importNodeId = "node2";
    obj1.status.state = UBSE_MEM_IMPORT_SUCCESS;
    existImportObjs.push_back(obj1);

    UbseMemShareBorrowImportObj resultObj;
    EXPECT_FALSE(ExistImportObj("test_shm", "node1", existImportObjs, resultObj));
}

// SC-10: ConstructShareImportObj with req.size > 0 uses req.size
TEST_F(TestShareCapabilities, SC10_ConstructShareImportObj_SizeGiven_UsesReqSize)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.req.size = 0;

    UbseMemShareAttachReq req;
    req.size = 4096;

    ConstructShareImportObj(importObj, req);

    EXPECT_EQ(importObj.shareAttr.size, 4096);
    EXPECT_EQ(importObj.req.size, 4096);
}

// SC-11: ConstructShareImportObj with req.size == 0 reuses existing importObj.req.size
TEST_F(TestShareCapabilities, SC11_ConstructShareImportObj_SizeZero_UsesExistingSize)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.req.size = 8192;

    UbseMemShareAttachReq req;
    req.size = 0;

    ConstructShareImportObj(importObj, req);

    EXPECT_EQ(importObj.shareAttr.size, 8192);
}

// SC-12: ConstructShareImportObj with all fields set verifies both paths
TEST_F(TestShareCapabilities, SC12_ConstructShareImportObj_AllFieldsSet)
{
    UbseMemShareBorrowImportObj importObj;

    UbseMemShareAttachReq req;
    req.size = 4096;
    req.udsInfo.uid = 1001;
    req.udsInfo.gid = 1001;
    req.owner.uid = 2001;
    req.owner.gid = 2001;
    req.owner.pid = 3001;
    req.owner.mode = 0644;
    req.importNodeId = "node3";
    req.name = "full_test";
    req.udsInfo.pid = 4001;
    req.udsInfo.username = "test_user";
    req.requestId = 12345;
    req.requestNodeId = "node_req";

    ConstructShareImportObj(importObj, req);

    EXPECT_EQ(importObj.shareAttr.size, 4096);
    EXPECT_EQ(importObj.shareAttr.gid, 1001);
    EXPECT_EQ(importObj.shareAttr.uid, 1001);
    EXPECT_EQ(importObj.shareAttr.owner.uid, 2001);
    EXPECT_EQ(importObj.shareAttr.owner.gid, 2001);
    EXPECT_EQ(importObj.shareAttr.owner.pid, 3001);
    EXPECT_EQ(importObj.shareAttr.owner.mode, 0644);
    EXPECT_EQ(importObj.importNodeId, "node3");
    EXPECT_EQ(importObj.req.name, "full_test");
    EXPECT_EQ(importObj.req.udsInfo.uid, 1001);
    EXPECT_EQ(importObj.req.udsInfo.gid, 1001);
    EXPECT_EQ(importObj.req.udsInfo.pid, 4001);
    EXPECT_EQ(importObj.req.udsInfo.username, "test_user");
    EXPECT_EQ(importObj.req.requestId, 12345);
    EXPECT_EQ(importObj.req.requestNodeId, "node_req");
    EXPECT_EQ(importObj.status.expectState, UBSE_MEM_IMPORT_SUCCESS);
    EXPECT_EQ(importObj.status.state, UBSE_MEM_IMPORT_RUNNING);
}

// SC-13: NormalizeShareRegion when nodelist already has nodes, no change
TEST_F(TestShareCapabilities, SC13_NormalizeShareRegion_AlreadyHasNodes_NoChange)
{
    UbseMemShareBorrowReq req;
    req.shmRegion.nodeNum = 2;
    UbseNodeInfo nodeInfo1;
    nodeInfo1.nodeId = "node1";
    UbseNodeInfo nodeInfo2;
    nodeInfo2.nodeId = "node2";
    req.shmRegion.nodelist.push_back(nodeInfo1);
    req.shmRegion.nodelist.push_back(nodeInfo2);

    NormalizeShareRegion(req);

    EXPECT_EQ(req.shmRegion.nodeNum, 2);
    EXPECT_EQ(req.shmRegion.nodelist.size(), 2);
    EXPECT_EQ(req.shmRegion.nodelist[0].nodeId, "node1");
    EXPECT_EQ(req.shmRegion.nodelist[1].nodeId, "node2");
}

// SC-14: ValidateAffinityParams returns true when affinity is disabled
TEST_F(TestShareCapabilities, SC14_ValidateAffinityParams_Disabled_ReturnsTrue)
{
    UbseMemShareBorrowReq req;
    req.withAffinity.enableCreateWithAffinity = false;

    EXPECT_TRUE(ValidateAffinityParams(req));
}

// SC-15: ValidateAffinityParams returns false when affinity enabled but nodeId empty
TEST_F(TestShareCapabilities, SC15_ValidateAffinityParams_EmptyNodeId_ReturnsFalse)
{
    UbseMemShareBorrowReq req;
    req.withAffinity.enableCreateWithAffinity = true;
    req.withAffinity.createReqNodeId = "";

    EXPECT_FALSE(ValidateAffinityParams(req));
}

} // namespace ubse::mem::controller::ut
