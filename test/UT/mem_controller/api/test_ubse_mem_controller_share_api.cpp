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
#include "test_ubse_mem_controller_share_api.h"
#include <src/adapter_plugins/mmi/ubse_mmi_module.h>
#include <ubse_mem_scheduler.h>
#include <iostream>
#include "debt/ubse_mem_debt_ledger.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "message/ubse_mem_share_borrow_importobj_simpo.h"
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_node.h"
#include "ubse_topo_util.h"
#include "ubse_mem_sign_verifier.h"

namespace ubse::mem_controller::share::ut {
using namespace ubse::com;
using namespace mem::controller::message;
using namespace mem::controller;
using namespace mem::controller::debt;
using namespace ubse::utils;

const std::string NODE_ONE = "1";
const std::string NODE_TWO = "2";
const std::string SHM_NAME = "share_test_mem";

void TestUbseMemControllerShareApi::SetUp()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    Test::SetUp();
}
void TestUbseMemControllerShareApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void SendShareExportObjMockSet()
{
    const auto func = &UbseComModule::RpcSend<UbseMemShareBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
void SendShareExpoprtObjMockSetError()
{
    const auto func = &UbseComModule::RpcSend<UbseMemShareBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
}
void SendShareImportObjMockSet()
{
    const auto func = &UbseComModule::RpcSend<UbseMemShareBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
void BuildOperationMockSet()
{
    const auto func = &UbseComModule::RpcSend<UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
void BuildOperationSuccessMock(std::shared_ptr<com::UbseComModule> &module)
{
    module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}

void PutShareExportObj(const std::string &nodeId, const std::string &name, const UbseMemShareBorrowExportObj &obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetOrCreateNodeMap(nodeId)->Put(
        name, std::make_shared<UbseMemShareBorrowExportObj>(obj));
}

void PutShareImportObj(const std::string &nodeId, const std::string &name, const UbseMemShareBorrowImportObj &obj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetOrCreateNodeMap(nodeId)->Put(
        name, std::make_shared<UbseMemShareBorrowImportObj>(obj));
}

std::shared_ptr<const UbseMemShareBorrowExportObj> GetShareExportObj(const std::string &nodeId, const std::string &name)
{
    return UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(nodeId, name);
}

std::shared_ptr<const UbseMemShareBorrowImportObj> GetShareImportObj(const std::string &nodeId, const std::string &name)
{
    return UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(nodeId, name);
}

bool ShareImportObjExists(const std::string &nodeId, const std::string &name)
{
    return GetShareImportObj(nodeId, name) != nullptr;
}

void AddToExportObjMap(const std::string &name, const std::string &nodeId,
                       UbseMemShareBorrowExportObj &shareBorrowExportObj)
{
    PutShareExportObj(nodeId, name, shareBorrowExportObj);
}
// 创建并初始化UbseMemShareBorrowImportObj对象，并将其添加到shareImportObjMap中
void AddToImportObjMap(const std::string &name, const std::string &nodeId,
                       UbseMemShareBorrowImportObj &shareBorrowImportObj)
{
    PutShareImportObj(nodeId, name, shareBorrowImportObj);
}

void ConstructShareBorrowAccount()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    const std::string nodeId = NODE_ONE;
    const std::string name = SHM_NAME;
    UbseMemShareBorrowExportObj shareBorrowExportObj{};
    UbseMemShareBorrowImportObj shareBorrowImportObj{};
    shareBorrowExportObj.req.name = name;
    shareBorrowImportObj.req.name = name;
    UbseMemDebtNumaInfo numaInfo;
    // 算法决策导出节点为NODE_TWO
    numaInfo.nodeId = NODE_TWO;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    shareBorrowExportObj.algoResult.exportNumaInfos = numaInfos;
    shareBorrowImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    shareBorrowExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    shareBorrowImportObj.algoResult = shareBorrowExportObj.algoResult;

    AddToExportObjMap(name, nodeId, shareBorrowExportObj);
    AddToImportObjMap(name, nodeId, shareBorrowImportObj);
}

void ExportCallbackExportObjSet(UbseMemShareBorrowExportObj &exportObj, const UbseMemState &memState,
                                const UbseMemState &expectMemState)
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();

    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{.nodeId = NODE_ONE};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{.nodeId = NODE_TWO};

    UbseMemShareBorrowReq req{};
    req.name = SHM_NAME;
    req.requestNodeId = NODE_ONE;
    req.udsInfo = udsInfo;
    req.shmRegion.nodeNum = 2;
    req.shmRegion.nodelist.push_back(node1);
    req.shmRegion.nodelist.push_back(node2);
    req.name = SHM_NAME;

    UbseMemDebtNumaInfo numaInfo;
    // 算法决策导出节点为NODE_TWO
    numaInfo.nodeId = NODE_TWO;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);

    exportObj.req = req;
    exportObj.status.state = memState;
    exportObj.status.expectState = expectMemState;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    UbseMemObmmInfo obmmInfo{};
    obmmInfo.memId = 1;
    exportObj.status.exportObmmInfo.push_back(obmmInfo);
    PutShareExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, SHM_NAME, exportObj);
}

void ImportCallbackImportObjSet(UbseMemShareBorrowImportObj &importObj, UbseMemShareBorrowExportObj &exportObj,
                                const UbseMemState &memState, const UbseMemState &expectMemState)
{
    importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
    importObj.algoResult = exportObj.algoResult;
    importObj.req = exportObj.req;
    importObj.shareAttr.size = importObj.req.size;
    importObj.req.size = importObj.req.size;
    importObj.shareAttr.gid = exportObj.req.udsInfo.gid;
    importObj.shareAttr.uid = exportObj.req.udsInfo.uid;
    importObj.importNodeId = NODE_ONE;
    importObj.req.name = SHM_NAME;
    importObj.req.requestNodeId = NODE_ONE;
    importObj.status.expectState = expectMemState;
    importObj.status.state = memState;
}
/*
 * 用例描述：共享内存借用，资源已存在
 * 测试步骤：
 * 1.构造账本数据，name均为share_test_mem;
 * 2.调用UbseMemShareBorrow，
 * 预期结果：
 * 1.函数返回UBSE_OK. resp.errorCode: UbseErrorCode::UBS_ERR_EXIST
 */
TEST_F(TestUbseMemControllerShareApi, ShareBorrowResourceExists)
{
    ConstructShareBorrowAccount();
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    // 模拟rpcSend发送成功
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    SendShareExportObjMockSet();
    BuildOperationMockSet();

    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemShareBorrowReq req{};
    req.name = SHM_NAME;
    req.requestNodeId = NODE_ONE;
    req.udsInfo = udsInfo;
    UbseMemOperationResp resp{};
    auto ret = UbseMemShareBorrow(req, resp);
    EXPECT_EQ(UBSE_ERR_EXISTED, resp.errorCode);
}
uint32_t UbseMemShmExportObjStateChangeHandlerMock(UbseMemShareBorrowExportObj &exportObj)
{
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    return UBSE_OK;
}
uint32_t ShareBorrowAffinityStateChangeHandlerMock(UbseMemShareBorrowExportObj &exportObj)
{
    UbseMemDebtNumaInfo numaInfo{.nodeId = NODE_TWO};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    return UBSE_OK;
}
/*
 * 用例描述：共享内存借用，请求成功发送到导出节点
 * 测试步骤：
 * 1.构造账本数据，name均为share_test_mem;
 * 2.调用UbseMemShareBorrow
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseMemControllerShareApi, ShareBorrowSuccess)
{
    // 模拟rpcSend发送成功
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));

    ubse::nodeController::UbseNodeInfo node{.nodeId = NODE_ONE};
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(node));
    MOCKER(mem::scheduler::UbseMemShmExportObjStateChangeHandler)
        .stubs()
        .will(invoke(UbseMemShmExportObjStateChangeHandlerMock));

    SendShareExportObjMockSet();
    BuildOperationMockSet();
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemShareBorrowReq req{};
    req.name = SHM_NAME;
    req.requestNodeId = NODE_ONE;
    req.udsInfo = udsInfo;

    ubse::adapter_plugins::mmi::UbseNodeInfo node1{.nodeId = NODE_ONE};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{.nodeId = NODE_TWO};
    req.shmRegion.nodeNum = 2;
    req.shmRegion.nodelist.push_back(node1);
    req.shmRegion.nodelist.push_back(node2);
    UbseMemOperationResp resp{};
    auto ret = UbseMemShareBorrow(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

void AgentExportCallbackMockSet()
{
    // 当前节点为2号节点
    election::UbseRoleInfo currentInfo{};
    currentInfo.nodeId = NODE_TWO;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));

    // 模拟主节点为1号节点
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));

    // 模拟rpcSend发送成功
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    // 模拟ElectionModule获取本地主节点
    std::shared_ptr<election::UbseElectionModule> electionModule = std::make_shared<election::UbseElectionModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<election::UbseElectionModule>).stubs().will(returnValue(electionModule));
    election::Node localMasterNode{};
    localMasterNode.id = NODE_ONE;
    MOCKER(&election::UbseElectionModule::GetLocalMasterNode).stubs().with(outBound(localMasterNode)).will(returnValue(UBSE_OK));
    SendShareExportObjMockSet();
    BuildOperationMockSet();
}
/*
 * 用例描述：共享内存借用，履行端执行,mmi执行失败，导致内存借用失败
 * 测试步骤：
 * 1. 设置当前节点为2号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造exportObj请求，使请求走入agent测执行端;
 *      export.status.state:UBSE_MEM_EXPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4. 构造UbseMemShmExportExecutor返回失败;
 * 2.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK, 履行节点账本中对应状态设置为UBSE_MEM_EXPORT_DESTROYED
 */
TEST_F(TestUbseMemControllerShareApi, ShareBorrowAgentFailedByMmi)
{
    UbseMemShareBorrowExportObj exportObj{};
    AgentExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_RUNNING, UBSE_MEM_EXPORT_SUCCESS);

    // 模拟mmi export 执行失败;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmExportExecutor).stubs().will(returnValue(UBSE_ERROR));

    auto ret = UbseMemShareBorrowExportObjCallback(exportObj);
    // mmi执行失败，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
    auto nodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    auto res = !ShareImportObjExists(nodeId, name);
    EXPECT_TRUE(res);
}

/*
 * 用例描述：共享内存借用，履行端执行,mmi执行失败，导致内存借用失败
 * 测试步骤：
 * 1. 设置当前节点为2号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造exportObj请求，使请求走入agent测执行端;
 *      export.status.state:UBSE_MEM_EXPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4. 构造UbseMemShmExportExecutor返回UBSE_OK;
 * 2.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK, 履行节点账本中对应状态设置为UBSE_MEM_EXPORT_SUCCESS
 */
TEST_F(TestUbseMemControllerShareApi, ShareBorrowAgentSuccess)
{
    UbseMemShareBorrowExportObj exportObj{};
    AgentExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_RUNNING, UBSE_MEM_EXPORT_SUCCESS);

    // 模拟mmi export 执行成功;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmExportExecutor).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseMemShareBorrowExportObjCallback(exportObj);
    // mmi执行成功，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
    auto obj = GetShareExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj.req.name);
    EXPECT_TRUE(obj != nullptr);
    EXPECT_EQ(UBSE_MEM_EXPORT_SUCCESS, obj->status.state);
}

void MasterExportCallbackMockSet()
{
    // 当前节点为1号节点
    election::UbseRoleInfo currentInfo{};
    currentInfo.nodeId = NODE_ONE;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));

    // 模拟主节点为1号节点
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));

    // 模拟rpcSend发送成功
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    // 模拟ElectionModule获取本地主节点
    std::shared_ptr<election::UbseElectionModule> electionModule = std::make_shared<election::UbseElectionModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<election::UbseElectionModule>).stubs().will(returnValue(electionModule));
    election::Node localMasterNode{};
    localMasterNode.id = NODE_ONE;
    MOCKER(&election::UbseElectionModule::GetLocalMasterNode).stubs().with(outBound(localMasterNode)).will(returnValue(UBSE_OK));
    SendShareExportObjMockSet();
    SendShareImportObjMockSet();
    BuildOperationMockSet();
}
/*
 * 用例描述：共享内存借用，主节点端处理履行端发送过来的导出失败
 * 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造exportObj请求，使请求走入master测执行端;
 *      export.status.state:UBSE_MEM_EXPORT_DESTROYED
 *      export.status.expectState:UBSE_MEM_EXPORT_SUCCESS
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4. 构造UbseMemShmExportExecutor返回UBSE_OK;
 * 2.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseMemControllerShareApi, ShareBorrowMasterExportFailed)
{
    UbseMemShareBorrowExportObj exportObj{};
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_DESTROYED, UBSE_MEM_EXPORT_SUCCESS);
    MOCKER(mem::scheduler::UbseMemShmExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemShareBorrowExportObjCallback(exportObj);
    // mmi执行成功，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
}
/*
 * 用例描述：共享内存借用，主节点端处理履行端发送过来的导出成功
* 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造exportObj请求，使请求走入master测执行端;
 *      export.status.state:UBSE_MEM_EXPORT_SUCCESS
 *      export.status.expectState:UBSE_MEM_EXPORT_SUCCESS
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4. 构造UbseMemShmExportExecutor返回UBSE_OK;
 * 2.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseMemControllerShareApi, ShareBorrowMasterExportSuccess)
{
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemShareBorrowExportObj exportObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    // 模拟mmi export 执行成功;
    MOCKER(mem::scheduler::UbseMemShmExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemShareBorrowExportObjCallback(exportObj);
    // mmi执行成功，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
}

UbseMemShareAttachReq ConstructAttachReq()
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{.nodeId = NODE_ONE};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{.nodeId = NODE_TWO};

    UbseMemShareAttachReq req{};
    req.name = SHM_NAME;
    req.requestNodeId = NODE_ONE;
    req.udsInfo = udsInfo;
    req.name = SHM_NAME;
    req.importNodeId = NODE_ONE;
    return req;
}
/*
 * 用例描述：共享内存导入，请求成功发送到导入节点
 * 测试步骤：
 * 1.构造账本导出数据，name均为share_test_mem;
 * 2.调用ShareAttachSuccess
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseMemControllerShareApi, ShareAttachSuccess)
{
    UbseMemShareBorrowExportObj exportObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    SendShareImportObjMockSet();
    BuildOperationMockSet();
    UbseMemShareAttachReq req = ConstructAttachReq();
    UbseMemOperationResp resp{};
    auto ret = UbseMemShareAttach(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

void AgentImportCallbackMockSet()
{
    // 当前节点为1号节点
    election::UbseRoleInfo currentInfo{};
    currentInfo.nodeId = NODE_ONE;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));

    // 模拟主节点为1号节点
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));

    // 模拟rpcSend发送成功
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    SendShareExportObjMockSet();
    BuildOperationMockSet();
}
/*
 * 用例描述：共享内存导入，履行端执行,mmi执行失败，导致内存导入失败
 * 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造importObj请求，使请求走入agent测执行端;
 *      importObj.status.state:UBSE_MEM_IMPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4. 构造UbseMemShmImportExecutor返回UBSE_ERROR;
 * 2.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK, 履行节点账本中对应状态设置为UBSE_MEM_IMPORT_DESTROYED
 */
TEST_F(TestUbseMemControllerShareApi, ShareImportAgentFailedByMmi)
{
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_RUNNING, UBSE_MEM_IMPORT_SUCCESS);

    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importNodeId = importObj.importNodeId;
    auto name = importObj.req.name;
    EXPECT_FALSE(ShareImportObjExists(importNodeId, name));
}
/*
 * 用例描述：共享内存导入，履行端执行,但已有导入
 * 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造importObj请求，使请求走入agent测执行端;
 *      importObj.status.state:UBSE_MEM_IMPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK
 */
TEST_F(TestUbseMemControllerShareApi, ShareImportAgentExisted)
{
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_RUNNING, UBSE_MEM_IMPORT_SUCCESS);
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    PutShareImportObj(importObj.importNodeId, importObj.req.name, importObj);
    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}
/*
 * 用例描述：共享内存导入，履行端执行,mmi执行成功，内存导入成功
* 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造importObj请求，使请求走入agent测执行端;
 *      importObj.status.state:UBSE_MEM_IMPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4. 构造UbseMemShmImportExecutor返回UBSE_OK;
 * 2.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK, 履行节点账本中对应状态设置为UBSE_MEM_IMPORT_SUCCESS
 */
TEST_F(TestUbseMemControllerShareApi, ShareImportAgentSuccess)
{
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    MOCKER(&mem::decoder::utils::MemDecoderUtils::SetParamMarId).stubs().will(returnValue(UBSE_OK));
    MOCKER(ImportToAddDecoderEntry).stubs().will(returnValue(UBSE_OK));
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_RUNNING, UBSE_MEM_IMPORT_SUCCESS);

    // 模拟mmi import 执行成功;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmImportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    // mmi执行失败，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
    auto obj = GetShareImportObj(importObj.importNodeId, importObj.req.name);
    EXPECT_TRUE(obj != nullptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, obj->status.state);
}
/*
 * 用例描述：共享内存借用，履行端导入失败，发送到主节点测执行
 * 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造exportObj请求，使请求走入agent测执行端;
 *      export.status.state:UBSE_MEM_EXPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK, 主节点账本中对应状态设置为UBSE_MEM_IMPORT_DESTROYED
 */
TEST_F(TestUbseMemControllerShareApi, ShareImportMasterFailed)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_DESTROYED, UBSE_MEM_IMPORT_SUCCESS);
    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importNodeId = importObj.importNodeId;
    auto name = importObj.req.name;
    EXPECT_FALSE(ShareImportObjExists(importNodeId, name));
}
/*
 * 用例描述：共享内存借用，履行端导入成功，发送到主节点测执行
 * 测试步骤：
 * 1. 设置当前节点为1号，主节点为1号节点；
 * 2. 设置导出节点为2号节点，导入节点为1号节点
 * 3.构造exportObj请求，使请求走入agent测执行端;
 *      export.status.state:UBSE_MEM_EXPORT_RUNNING
 *      exportObj.algoResult.exportNumaInfos[0].nodeId: NODE_TWO
 * 4.调用UbseMemShareBorrowExportObjCallback
 * 预期结果：
 * 1.函数返回UBSE_OK, 履行节点账本中对应状态设置为UBSE_MEM_EXPORT_SUCCESS
 */
TEST_F(TestUbseMemControllerShareApi, ShareImportMasterSuccess)
{
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_SUCCESS, UBSE_MEM_IMPORT_SUCCESS);
    MOCKER(mem::scheduler::UbseMemShmImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));

    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto obj = GetShareImportObj(importObj.importNodeId, importObj.req.name);
    EXPECT_TRUE(obj != nullptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, obj->status.state);
}

TEST_F(TestUbseMemControllerShareApi, AddShareImportTest)
{
    UbseMemShareBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    EXPECT_EQ(UBSE_OK, AddShareImport(importObj));
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    EXPECT_EQ(UBSE_OK, AddShareImport(importObj));
}

TEST_F(TestUbseMemControllerShareApi, AddShareExportTest)
{
    UbseMemShareBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    EXPECT_EQ(UBSE_OK, AddShareExport(exportObj));
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    EXPECT_EQ(UBSE_OK, AddShareExport(exportObj));
}

TEST_F(TestUbseMemControllerShareApi, DeleteShareExportTest)
{
    UbseMemShareBorrowExportObj exportObj;
    const uint32_t E_CODE_EMPTY_ALGO_RESULT = 1024;
    uint32_t ret = DeleteShareExport(exportObj);
    EXPECT_EQ(UBSE_ERROR, ret);
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    MasterExportCallbackMockSet();
    EXPECT_EQ(UBSE_OK, DeleteShareExport(exportObj));
}

TEST_F(TestUbseMemControllerShareApi, UbseMemShareReturnTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemReturnReq req;
    req.name = SHM_NAME;
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;
    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    UbseMemShareReturn(req, resp, NODE_ONE);

    UbseMemShareBorrowExportObj exportObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    MasterExportCallbackMockSet();
    UbseMemShareReturn(req, resp, NODE_ONE);
}

TEST_F(TestUbseMemControllerShareApi, UbseMemShareReturnSendFailedTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemReturnReq req;
    req.name = SHM_NAME;
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;
    std::shared_ptr<com::UbseComModule> module;
    BuildOperationSuccessMock(module);
    UbseMemShareBorrowExportObj exportObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    UbseMemShareReturn(req, resp, NODE_ONE);
}

TEST_F(TestUbseMemControllerShareApi, ShareBorrowAffinity)
{
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    SendShareExportObjMockSet();
    BuildOperationMockSet();
    ubse::nodeController::UbseNodeInfo nodeInfo;
    ubse::nodeController::UbseNumaLocation location1;
    location1.nodeId = "1";
    location1.numaId = 1;
    UbseNumaInfo numaInfo;
    numaInfo.socketId = 1;
    nodeInfo.numaInfos.insert({location1, numaInfo});

    UbseMemShareBorrowReq req{};
    req.name = "test_name_affinity";
    req.requestNodeId = NODE_ONE;
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    req.udsInfo = udsInfo;
    req.withAffinity.enableCreateWithAffinity = true;
    UbseMemOperationResp resp{};
    // createNodeId为空
    auto ret = UbseMemShareBorrow(req, resp);
    EXPECT_EQ(UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL, resp.errorCode);
    // GetNodeById获取为空
    req.withAffinity.createReqNodeId = NODE_ONE;
    ret = UbseMemShareBorrow(req, resp);
    EXPECT_EQ(UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL, resp.errorCode);

    // 当前的socketId不在nodeId里面
    MOCKER_CPP(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    req.withAffinity.affinitySocketId = 100;
    ret = UbseMemShareBorrow(req, resp);
    EXPECT_EQ(UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL, resp.errorCode);

    // 校验通过
    req.withAffinity.affinitySocketId = numaInfo.socketId;
    // 填充shmRegion使NormalizeShareRegion直接返回，避免走真实GetAllNodes
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{.nodeId = NODE_ONE};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{.nodeId = NODE_TWO};
    req.shmRegion.nodeNum = 2;
    req.shmRegion.nodelist.clear();
    req.shmRegion.nodelist.push_back(node1);
    req.shmRegion.nodelist.push_back(node2);
    MOCKER(mem::scheduler::UbseMemShmExportObjStateChangeHandler)
        .stubs()
        .will(invoke(ShareBorrowAffinityStateChangeHandlerMock));
    ret = UbseMemShareBorrow(req, resp);
}

TEST_F(TestUbseMemControllerShareApi, UbseMemShareDetachTest)
{
    MOCKER(WaitNodeStateWork).stubs().will(returnValue(UBSE_OK));
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> module = std::make_shared<UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    BuildOperationMockSet();
    UbseMemShareDetachReq req;
    UbseMemOperationResp resp;
    req.name = SHM_NAME;
    EXPECT_EQ(UBSE_OK, UbseMemShareDetach(req, resp, NODE_ONE));
    EXPECT_EQ(UBSE_ERR_SHM_NODE_EMPTY, resp.errorCode);

    req.unImportNodeId = NODE_ONE;
    UbseMemShareDetach(req, resp, NODE_ONE);
    EXPECT_EQ(UBSE_ERR_SHM_NO_ATTACH, resp.errorCode);

    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};

    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_SUCCESS, UBSE_MEM_IMPORT_SUCCESS);
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    PutShareImportObj(importObj.importNodeId, importObj.req.name, importObj);
    SendShareImportObjMockSet();
    UbseUdsInfo invalidUdsInfo{.uid = 1000, .gid = 1000, .pid = 1000, .username = "ubsmd"};
    req.udsInfo = invalidUdsInfo;
    UbseMemShareDetach(req, resp, NODE_ONE);
    EXPECT_EQ(UBSE_ERR_AUTH_FAILED, resp.errorCode);

    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    req.udsInfo = udsInfo;
    UbseMemShareDetach(req, resp, NODE_ONE);

    MasterExportCallbackMockSet();
    UbseMemShareDetach(req, resp, NODE_ONE);
}

TEST_F(TestUbseMemControllerShareApi, ShareExportDestroyingAgentCallbackTest)
{
    election::UbseRoleInfo currentInfo{};
    currentInfo.nodeId = NODE_TWO;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = NODE_TWO;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    // 模拟rpcSend发送成功
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    SendShareExportObjMockSet();
    SendShareImportObjMockSet();
    BuildOperationMockSet();

    std::shared_ptr<election::UbseElectionModule> electionModule = std::make_shared<election::UbseElectionModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<election::UbseElectionModule>).stubs().will(returnValue(electionModule));
    election::Node localMasterNode{};
    localMasterNode.id = NODE_TWO;
    MOCKER(&election::UbseElectionModule::GetLocalMasterNode).stubs().with(outBound(localMasterNode)).will(returnValue(UBSE_OK));

    UbseMemShareBorrowExportObj exportObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_DESTROYING, UBSE_MEM_EXPORT_SUCCESS);
    MOCKER(mem::scheduler::UbseMemShmExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemShareBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);

    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmUnExportExecutor).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemShareBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerShareApi, ShareExportMasterCallbackTest)
{
    UbseMemShareBorrowExportObj exportObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_DESTROYED);
    MOCKER(mem::scheduler::UbseMemShmExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemShareBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_DESTROYED, UBSE_MEM_EXPORT_DESTROYED);
    ret = UbseMemShareBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerShareApi, ShareAttachTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemShareBorrowExportObj exportObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    SendShareImportObjMockSet();
    BuildOperationMockSet();
    UbseMemShareAttachReq req = ConstructAttachReq();
    UbseMemOperationResp resp{};
    MOCKER_CPP(&IsSameSocketMultiPortTopo).stubs().will(returnValue(true));
    ubse::nodeController::UbseNodeMemCnaInfoOutput cnaOutput;
    cnaOutput.borrowSocketId = NODE_ONE;
    MOCKER_CPP(&ubse::nodeController::UbseNodeMemGetTopologyCnaInfo)
        .stubs()
        .with(any(), outBound(cnaOutput))
        .will(returnValue(UBSE_OK));
    ubse::nodeController::UbseNodeInfo nodeInfo;
    UbseCpuLocation cpuLocation{.nodeId = NODE_ONE, .chipId = 1};
    UbseCpuInfo ubseCpuInfo;
    ubseCpuInfo.socketId = 1;
    ubseCpuInfo.chipId = "1";
    nodeInfo.cpuInfos.insert({cpuLocation, ubseCpuInfo});
    MOCKER_CPP(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    auto ret = UbseMemShareAttach(req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerShareApi, ShareAttachExistTest)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_SUCCESS, UBSE_MEM_IMPORT_SUCCESS);
    PutShareImportObj(importObj.importNodeId, importObj.req.name, importObj);

    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    SendShareImportObjMockSet();
    BuildOperationMockSet();
    UbseMemShareAttachReq req = ConstructAttachReq();
    UbseMemOperationResp resp{};
    auto ret = UbseMemShareAttach(req, resp);
    EXPECT_EQ(ret, UBSE_ERR_EXISTED);
    UbseUdsInfo udsInfo{.uid = 1000, .gid = 1000, .pid = 1000};
    req.udsInfo = udsInfo;
    ret = UbseMemShareAttach(req, resp);
    EXPECT_EQ(ret, UBSE_ERR_AUTH_FAILED);
}

TEST_F(TestUbseMemControllerShareApi, UbseMemShareBorrowImportObjAgentCallbackTest)
{
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    MasterExportCallbackMockSet();
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_DESTROYING, UBSE_MEM_IMPORT_SUCCESS);
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    PutShareImportObj(importObj.importNodeId, importObj.req.name, importObj);
    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemShmUnImportExecutor).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerShareApi, UbseMemShareBorrowImportObjAgentSendFailed)
{
    // 当前节点为1号节点
    election::UbseRoleInfo currentInfo{};
    currentInfo.nodeId = NODE_ONE;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    // 模拟主节点为1号节点
    election::UbseRoleInfo masterInfo{};
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));

    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));

    std::shared_ptr<election::UbseElectionModule> electionModule = std::make_shared<election::UbseElectionModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<election::UbseElectionModule>).stubs().will(returnValue(electionModule));
    election::Node localMasterNode{};
    localMasterNode.id = NODE_ONE;
    MOCKER(&election::UbseElectionModule::GetLocalMasterNode).stubs().with(outBound(localMasterNode)).will(returnValue(UBSE_OK));

    const auto func = &UbseComModule::RpcSend<UbseMemShareBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    ExportCallbackExportObjSet(exportObj, UBSE_MEM_EXPORT_SUCCESS, UBSE_MEM_EXPORT_SUCCESS);
    ImportCallbackImportObjSet(importObj, exportObj, UBSE_MEM_IMPORT_DESTROYING, UBSE_MEM_IMPORT_SUCCESS);
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    PutShareImportObj(importObj.importNodeId, importObj.req.name, importObj);
    auto ret = UbseMemShareBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerShareApi, UpdateFaultShareExportObjTest)
{
    const std::string nodeId = NODE_ONE;
    uint64_t memId = 1;
    std::string memName = SHM_NAME;
    UbMemFaultType type;
    UbseMemShareBorrowExportObj exportObj{};
    auto ret = UpdateFaultShareExportObj(nodeId, memId, memName, type);
    EXPECT_EQ(UBSE_ERROR, ret);
    PutShareExportObj(nodeId, SHM_NAME, exportObj);
    ret = UpdateFaultShareExportObj(nodeId, memId, memName, type);
    EXPECT_EQ(UBSE_ERROR, ret);
    UbseMemObmmInfo obmmInfo;
    obmmInfo.memId = memId;
    exportObj.status.exportObmmInfo.push_back(obmmInfo);
    PutShareExportObj(nodeId, SHM_NAME, exportObj);
    ret = UpdateFaultShareExportObj(nodeId, memId, memName, type);
    EXPECT_EQ(UBSE_OK, ret);
}
} // namespace ubse::mem_controller::share::ut
