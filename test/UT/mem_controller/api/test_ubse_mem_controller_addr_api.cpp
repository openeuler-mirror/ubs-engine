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
#include "test_ubse_mem_controller_addr_api.h"
#include <src/adapter_plugins/mmi/ubse_mmi_module.h>
#include <ubse_com_module.h>
#include <ubse_error.h>
#include <ubse_mem_scheduler.h>
#include "ubse_election.h"
#include "ubse_mem_account.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_msg.h"
#include "debt/ubse_mem_debt_ledger.h"
#include "message/ubse_mem_addr_borrow_exportobj_simpo.h"
#include "message/ubse_mem_addr_borrow_importobj_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
namespace ubse::mem_controller::addr::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
const std::string NODE_ONE = "1";
const std::string NODE_TWO = "2";

void TestUbseMemControllerAddrApi::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerAddrApi::TearDown()
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    Test::TearDown();
    GlobalMockObject::verify();
}
void SendAddrExportObjMockSet()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
void MasterSendAddrExpoprtObjMockSetError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
}

void AgentSendAddrExportObjMockSetError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}

void SendAddrImportObjMockSet()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
void SendAddrImportObjMockSetError()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemAddrBorrowImportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
}
void BuildOperationMockSet()
{
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
// 创建并初始化UbseMemAddrBorrowExportObj对象，并将其添加到addrExportObjMap中
void AddToExportObjMap(const std::string& name, const std::string& nodeId,
                       UbseMemAddrBorrowExportObj& addrBorrowExportObj)
{
    auto exportKey = mem::controller::GenerateExportObjKey(name, nodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(nodeId, exportKey,
                                                                                          addrBorrowExportObj);
}
// 创建并初始化UbseMemAddrBorrowImportObj对象，并将其添加到addrImportObjMap中
void AddToImportObjMap(const std::string& name, const std::string& nodeId,
                       UbseMemAddrBorrowImportObj& addrBorrowImportObj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(nodeId, name,
                                                                                          addrBorrowImportObj);
}

/*
 * 用例描述
 * CheckAddrResourceState:资源不存在
 * 测试步骤：
 * 1.构造账本数据，name均为addr_test_mem;
 * 2.调用CheckAddrResourceState， 传入不存在得name: addr_test_no_exist
 * 预期结果：
 * 1.函数返回UBS_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemControllerAddrApi, CheckAddrResourceStateNoExist)
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    const std::string nodeId = NODE_ONE;
    const std::string name = "addr_test_mem";

    UbseMemAddrBorrowExportObj addrBorrowExportObj{};
    UbseMemAddrBorrowImportObj addrBorrowImportObj{};
    addrBorrowExportObj.req.name = name;
    addrBorrowImportObj.req.name = name;
    addrBorrowImportObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    addrBorrowExportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    AddToExportObjMap(name, nodeId, addrBorrowExportObj);
    AddToImportObjMap(name, nodeId, addrBorrowImportObj);
    EXPECT_EQ(mem::controller::CheckAddrResourceState("addr_test_no_exist", ""), UBSE_ERR_NOT_EXIST);
    EXPECT_EQ(mem::controller::CheckAddrResourceState("addr_test_no_exist", nodeId), UBSE_ERR_NOT_EXIST);
}

/*
 * 用例描述
 * CheckAddrResourceState:资源存在
 * 测试步骤：
 * 1.构造账本数据，name均为addr_test_mem;
 * 2.调用CheckAddrResourceState， 传入name: addr_test_mem
 * 预期结果：
 * 1.函数返回UBS_ERR_NOT_EXIST
 */
TEST_F(TestUbseMemControllerAddrApi, CheckAddrResourceStateExist)
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    const std::string nodeId = NODE_ONE;
    const std::string name = "addr_test_mem";

    UbseMemAddrBorrowExportObj addrBorrowExportObj{};
    UbseMemAddrBorrowImportObj addrBorrowImportObj{};
    addrBorrowExportObj.req.name = name;
    addrBorrowImportObj.req.name = name;
    addrBorrowImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    addrBorrowExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    AddToExportObjMap(name, nodeId, addrBorrowExportObj);
    AddToImportObjMap(name, nodeId, addrBorrowImportObj);
    EXPECT_EQ(mem::controller::CheckAddrResourceState(name, ""), UBSE_ERR_NOT_EXIST);
    EXPECT_EQ(mem::controller::CheckAddrResourceState(name, nodeId), UBSE_ERR_EXISTED);
}

TEST_F(TestUbseMemControllerAddrApi, UbseMemAddrBorrowSuccess)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemAddrBorrowReq req{};
    req.name = "addr_test_mem";
    req.requestNodeId = NODE_TWO;
    req.udsInfo = udsInfo;
    req.exportNodeId = NODE_ONE;
    req.importNodeId = NODE_TWO;
    UbseMemOperationResp resp{};

    UbseMemAddrBorrowImportObj importObj{.req = req};

    std::vector<UbseMemDebtNumaInfo> AddrInfos;
    UbseMemDebtNumaInfo AddrInfo{.nodeId = NODE_ONE, .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(AddrInfo);
    MOCKER_CPP(&mem::scheduler::UbseMemAddrImportObjStateChangeHandler)
        .stubs()
        .with(outBound(importObj))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&GetNumaInfoFromAgent).stubs().will(returnValue(UBSE_OK));
    SendAddrExportObjMockSet();
    EXPECT_EQ(mem::controller::UbseMemAddrBorrow(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerAddrApi, UbseMemAddrBorrowChangeHandlerFailed)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemAddrBorrowReq req{};
    req.name = "addr_test_mem";
    req.requestNodeId = NODE_TWO;
    req.udsInfo = udsInfo;
    req.exportNodeId = NODE_ONE;
    req.importNodeId = NODE_TWO;
    UbseMemOperationResp resp{};
    MOCKER_CPP(&GetNumaInfoFromAgent).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&mem::scheduler::UbseMemAddrImportObjStateChangeHandler).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(mem::controller::UbseMemAddrBorrow(req, resp), UBSE_OK);
}

TEST_F(TestUbseMemControllerAddrApi, UbseMemAddrBorrowSendFailed)
{
    UbseUdsInfo udsInfo{.uid = 0, .gid = 0, .pid = 0};
    UbseMemAddrBorrowReq req{};
    req.name = "addr_test_mem";
    req.requestNodeId = NODE_TWO;
    req.udsInfo = udsInfo;
    req.exportNodeId = NODE_ONE;
    req.importNodeId = NODE_TWO;
    UbseMemOperationResp resp{};
    UbseMemAddrBorrowImportObj importObj{.req = req};
    std::vector<UbseMemDebtNumaInfo> AddrInfos;
    UbseMemDebtNumaInfo AddrInfo{.nodeId = NODE_ONE, .socketId = 0, .numaId = 0, .size = 0};
    importObj.algoResult.exportNumaInfos.emplace_back(AddrInfo);
    MOCKER_CPP(&mem::scheduler::UbseMemAddrImportObjStateChangeHandler)
        .stubs()
        .with(outBound(importObj))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&GetNumaInfoFromAgent).stubs().will(returnValue(UBSE_OK));
    MasterSendAddrExpoprtObjMockSetError();
    MOCKER_CPP(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(mem::controller::UbseMemAddrBorrow(req, resp), UBSE_OK);
}

// 辅助函数，设置公共的mock配置
void ExportCallbackCommonSetup(election::UbseRoleInfo& currentInfo, election::UbseRoleInfo& masterInfo,
                               std::shared_ptr<com::UbseComModule>& module)
{
    // 当前节点为0号节点
    currentInfo.nodeId = NODE_TWO;
    MOCKER(election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    // 模拟主节点为1号节点
    masterInfo.nodeId = NODE_ONE;
    MOCKER_CPP(&election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    // 模拟rpcSend发送成功
    module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
}

// 成功场景
void AgentExportCallbackMockSet(UbseMemAddrBorrowExportObj& exportObj)
{
    election::UbseRoleInfo currentInfo{};
    election::UbseRoleInfo masterInfo{};
    std::shared_ptr<com::UbseComModule> module;
    ExportCallbackCommonSetup(currentInfo, masterInfo, module);
    SendAddrExportObjMockSet();
    // 设置exportObj
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_ONE;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.requestNodeId = NODE_ONE;
    exportObj.req.exportNodeId = NODE_TWO;
    exportObj.req.importNodeId = NODE_ONE;
}

// 错误场景
void AgentExportCallbackMockErrorSet(UbseMemAddrBorrowExportObj& exportObj)
{
    election::UbseRoleInfo currentInfo{};
    election::UbseRoleInfo masterInfo{};
    std::shared_ptr<com::UbseComModule> module;
    ExportCallbackCommonSetup(currentInfo, masterInfo, module);
    AgentSendAddrExportObjMockSetError();
    // 设置exportObj
    exportObj.req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_ONE;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.req.requestNodeId = NODE_ONE;
    exportObj.req.exportNodeId = NODE_TWO;
    exportObj.req.importNodeId = NODE_ONE;
}
/*
* 用例描述
* Addr类型内存的Export对象回调,履行测执行，mmi-export执行失败；
* 测试步骤：
* 1. 设置当前节点为2号，主节点为1号节点；
* 2. 模拟rpcsend发送成功，mmiexport执行失败
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportRunningAgentCallbackFailed)
{
    UbseMemAddrBorrowExportObj exportObj;
    AgentExportCallbackMockSet(exportObj);
    // 模拟mmi export 执行失败;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrExportExecutor).stubs().will(returnValue(UBSE_ERROR));

    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    // mmi执行失败，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
}

/*
* 用例描述
* Addr类型内存的Export对象回调,履行测执行，mmi-export执行成功，rpc发送到主节点成功；
* 测试步骤：
* 1. 设置当前节点为2号，主节点为1号节点；
* 2. 模拟rpcsend发送成功，mmiexport执行成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportRunningAgentCallbackSuccess)
{
    UbseMemAddrBorrowExportObj exportObj;
    AgentExportCallbackMockSet(exportObj);
    // 模拟mmi export 执行失败;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrExportExecutor).stubs().will(returnValue(UBSE_OK));
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    // mmi执行成功，ret结果为UBSE_OK
    EXPECT_EQ(UBSE_OK, ret);
}

/*
* 用例描述
* Addr类型内存的Export对象归还回调,履行测执行，mmi-unexport执行成功，rpc发送到主节点成功；
* 测试步骤：
* 1. 设置当前节点为2号，主节点为1号节点；
* 2. 模拟rpcsend发送成功，mmiexport执行成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportDestroyingAgentCallbackSuccess)
{
    UbseMemAddrBorrowExportObj exportObj;
    AgentExportCallbackMockSet(exportObj);
    // 模拟mmi export 执行失败;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrUnExportExecutor).stubs().will(returnValue(UBSE_OK));

    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    // mmi执行成功，ret结果为UBSE_OK,但状态应该被设置为UBSE_MEM_EXPORT_DESTROYED
    EXPECT_EQ(UBSE_OK, ret);
}
/*
* 用例描述
* Addr类型内存的Export对象归还回调,履行测执行，mmi-unexport执行失败，rpc发送到主节点成功；
* 测试步骤：
* 1. 设置当前节点为2号，主节点为1号节点；
* 2. 模拟rpcsend发送成功，mmiexport执行失败返回UBSE_ERROR
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportDestroyingAgentCallbackFailed)
{
    UbseMemAddrBorrowExportObj exportObj;
    AgentExportCallbackMockSet(exportObj);
    // 模拟mmi export 执行失败;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrUnExportExecutor).stubs().will(returnValue(UBSE_ERROR));

    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    // mmi执行成功
    EXPECT_EQ(UBSE_OK, ret);
}

void MasterExportCallbackMockSet(UbseMemAddrBorrowExportObj& exportObj)
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
    SendAddrExportObjMockSet();
    SendAddrImportObjMockSet();
    BuildOperationMockSet();
    UbseMemAddrBorrowReq req{};
    req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_ONE;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    req.requestNodeId = NODE_ONE;
    req.exportNodeId = NODE_ONE;
    req.importNodeId = NODE_ONE;
    exportObj.req = req;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
}
void MasterExportCallbackImportObjSet(const UbseMemAddrBorrowExportObj& exportObj)
{
    UbseMemAddrBorrowImportObj importObj{};
    importObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    importObj.req = exportObj.req;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        exportObj.req.importNodeId, exportObj.req.name, importObj);
}
/*
* 用例描述
* Addr类型内存的Export对象导出回调,master测执行，；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseAllNumaInfo、GetCnaInfoWhenImport、UbseMemAddrExportObjStateChangeHandler、模拟rpcsend发送成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. 账本中放入key为1号节点的 importObj数据;
* 5.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,importObj.status.state 状态设置为 UBSE_MEM_IMPORT_RUNNING
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportExpectSuccessCallbackSuccess)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    MasterExportCallbackImportObjSet(exportObj);

    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaInfoList{};
    ubse::mem::account::UbseNumaNodeInfo numaNodeInfo;
    numaNodeInfo.mMemTotal = 1024;
    numaNodeInfo.mMemFree = 128;
    numaNodeInfo.nodeId = "1";
    numaNodeInfo.hostName = "1";
    numaInfoList.push_back(numaNodeInfo);
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().with(outBound(numaInfoList)).will(returnValue(UBSE_OK));

    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_OK));
    MOCKER(mem::scheduler::UbseMemAddrExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));

    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        exportObj.req.importNodeId, exportObj.req.name);
    EXPECT_TRUE(importObjPtr != nullptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_RUNNING, importObjPtr->status.state);
}
/*
* 用例描述
* Addr类型内存的Export对象导出回调,agent测执行失败，master测执行；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 设置导出节点为2号节点，导入节点为1号节点
* 3. 账本中放入key为1号节点的 importObj数据;
* 4. exportObj.status.state状态设置为UBSE_MEM_EXPORT_DESTROYED代表导出失败
* 5.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK, 同时账本被删除;
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportExpectSuccessCallbackFailed)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    MasterExportCallbackImportObjSet(exportObj);

    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    auto exportKey = mem::controller::GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportObj.req.exportNodeId,
                                                                                          exportKey, exportObj);
    MOCKER(mem::scheduler::UbseMemAddrImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));

    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        exportObj.req.importNodeId, exportObj.req.name);
    EXPECT_TRUE(importObjPtr == nullptr);
}

/*
* 用例描述
* Addr类型内存的Export对象导出回调,master测执行时，账本中无对应import数据；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 设置导出节点为2号节点，导入节点为1号节点
* 3.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.查不到导入数据，返回UBSE_ERROR
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportMasterCallbackFailed)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    auto exportKey = mem::controller::GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportObj.req.exportNodeId,
                                                                                          exportKey, exportObj);

    MOCKER(mem::scheduler::UbseMemAddrImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));

    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMemControllerAddrApi, AddrExportMasterCallbackRollBack)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    MasterExportCallbackImportObjSet(exportObj);
    std::vector<ubse::mem::account::UbseNumaNodeInfo> numaInfoList{};
    ubse::mem::account::UbseNumaNodeInfo numaNodeInfo;
    numaNodeInfo.mMemTotal = 1024;
    numaNodeInfo.mMemFree = 128;
    numaNodeInfo.nodeId = "1";
    numaNodeInfo.hostName = "1";
    numaInfoList.push_back(numaNodeInfo);
    MOCKER(ubse::mem::account::UbseAllNumaInfo).stubs().with(outBound(numaInfoList)).will(returnValue(UBSE_OK));
    MOCKER(&mem::controller::GetCnaInfoWhenImport).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(mem::scheduler::UbseMemAddrExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
* 用例描述
* Addr类型内存的Export对象归还回调,master测执行成功；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟rpcsend发送成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. exportObj expectState设置为UBSE_MEM_EXPORT_DESTROYED, state设置为UBSE_MEM_EXPORT_DESTROYED;
* 5.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,nodeMemDebtInfoMap[exportNodeId].addrExportObjMap中不存在exportKey对应数据
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportExpectDestroyCallbackSuccess)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    auto exportKey = mem::controller::GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportObj.req.exportNodeId,
                                                                                          exportKey, exportObj);
    MOCKER(mem::scheduler::UbseMemAddrExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));

    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto exportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(
        exportObj.req.exportNodeId, exportKey);
    EXPECT_TRUE(exportObjPtr == nullptr);
}
/*
* 用例描述
* Addr类型内存的Export对象归还回调,master测执行失败；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟rpcsend发送成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. exportObj expectState设置为UBSE_MEM_EXPORT_DESTROYED, state设置为UBSE_MEM_EXPORT_SUCCESS;
* 5.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,对应状态为UBSE_MEM_EXPORT_SUCCESS
*/
TEST_F(TestUbseMemControllerAddrApi, AddrExportExpectDestroyCallbackFailed)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    auto exportKey = mem::controller::GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportObj.req.exportNodeId,
                                                                                          exportKey, exportObj);
    MOCKER(mem::scheduler::UbseMemAddrExportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));

    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto exportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(
        exportObj.req.exportNodeId, exportKey);
    EXPECT_TRUE(exportObjPtr != nullptr);
    EXPECT_EQ(exportObjPtr->status.state, UBSE_MEM_EXPORT_SUCCESS);
}

TEST_F(TestUbseMemControllerAddrApi, AddrExportDestroyingAgentCallbackSendFailed)
{
    UbseMemAddrBorrowExportObj exportObj;
    AgentExportCallbackMockErrorSet(exportObj);
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrUnExportExecutor).stubs().will(returnValue(UBSE_OK));
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    auto ret = mem::controller::UbseMemAddrBorrowExportObjCallback(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
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
    SendAddrImportObjMockSet();
    BuildOperationMockSet();
}

void AgentImportCallbackMockSetError()
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
    SendAddrImportObjMockSetError();
    BuildOperationMockSet();
}
void AgentImportCallbackImportObjSet(UbseMemAddrBorrowImportObj& importObj)
{
    UbseMemAddrBorrowReq req{};
    req.name = "test";
    req.requestNodeId = NODE_ONE;
    req.exportNodeId = NODE_TWO;
    req.importNodeId = NODE_ONE;
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.req = req;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(req.importNodeId, req.name,
                                                                                          importObj);
}
/*
* 用例描述
* Addr类型内存的import对象导出回调,agent测执行成功；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟SendAddrImportObj发送成功、UbseMemAddrImportExecutor
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. 设置账本数据，
* 5.调用 UbseMemAddrBorrowImportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,账本中import对应状态为UBSE_MEM_IMPORT_SUCCESS
*/
TEST_F(TestUbseMemControllerAddrApi, DealAddrAgentImportSuccess)
{
    GTEST_SKIP();
    UbseMemAddrBorrowImportObj importObj{};
    importObj.algoResult.importNumaInfos.resize(1);
    importObj.algoResult.exportNumaInfos.resize(1);
    AgentImportCallbackMockSet();
    AgentImportCallbackImportObjSet(importObj);
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrImportExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    MOCKER(ImportToAddDecoderEntry).stubs().will(returnValue(UBSE_OK));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        importObj.req.importNodeId, importObj.req.name);
    EXPECT_TRUE(importObjPtr != nullptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, importObjPtr->status.state);
}
/*
* 用例描述
* Addr类型内存的import对象导出回调,agent测执行失败；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟SendAddrImportObj发送成功、UbseMemAddrImportExecutor返回UBSE_ERROR
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. 设置账本数据，
* 5.调用 UbseMemAddrBorrowImportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseMemControllerAddrApi, DealAddrAgentImportFailed)
{
    UbseMemAddrBorrowImportObj importObj{};
    importObj.algoResult.importNumaInfos.resize(1);
    importObj.algoResult.exportNumaInfos.resize(1);
    AgentImportCallbackMockSet();
    AgentImportCallbackImportObjSet(importObj);
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    MOCKER(&mem::decoder::utils::MemDecoderUtils::SetParamMarId).stubs().will(returnValue(UBSE_OK));
    MOCKER(ImportToAddDecoderEntry).stubs().will(returnValue(UBSE_OK));
    // 模拟mmi import 执行成功;
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerAddrApi, DealAddrAgentImportSendFailed)
{
    UbseMemAddrBorrowImportObj importObj{};
    importObj.req.name = "test1";
    importObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    importObj.req.importNodeId = "1";
    UbseMemAddrBorrowImportObj newObj{};
    newObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    newObj.status.expectState = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(importObj.req.importNodeId,
                                                                                          importObj.req.name, newObj);
    AgentImportCallbackMockSetError();
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

void MasterImportCallbackMockSet()
{
    AgentImportCallbackMockSet();
    SendAddrExportObjMockSet();
    UbseMemAddrBorrowReq req{};
    req.name = "test";
    UbseMemDebtNumaInfo numaInfo;
    numaInfo.nodeId = NODE_ONE;
    std::vector<UbseMemDebtNumaInfo> numaInfos;
    numaInfos.emplace_back(numaInfo);
    req.requestNodeId = NODE_ONE;
    req.exportNodeId = NODE_TWO;
    req.importNodeId = NODE_ONE;
    UbseMemAddrBorrowExportObj exportObj{};
    exportObj.req = req;
    exportObj.algoResult.exportNumaInfos = numaInfos;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    auto exportKey = mem::controller::GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportObj.req.exportNodeId,
                                                                                          exportKey, exportObj);
}
void MasterImportCallbackImportObjSet(UbseMemAddrBorrowImportObj& importObj)
{
    AgentImportCallbackImportObjSet(importObj);
}

/*
* 用例描述
* Addr类型内存的import对象导出回调,master测执行成功；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟Rpc发送成功
* 3. 设置导出节点为2号节点，导入节点为1号节点，,入参importObj状态为UBSE_MEM_IMPORT_SUCCESS
* 4. 设置账本数据，
* 5.调用 UbseMemAddrBorrowImportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackSuccess)
{
    UbseMemAddrBorrowImportObj importObj{};
    MasterImportCallbackMockSet();
    MOCKER(&BuildOperationRespWhenSuccess).stubs().will(returnValue(UBSE_OK));
    MasterImportCallbackImportObjSet(importObj);
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemImportResult importResult{};
    importObj.status.importResults.push_back(importResult);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    MOCKER(mem::scheduler::UbseMemAddrImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}
/*
* 用例描述
* Addr类型内存的import对象导出回调,master测执行失败；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrImportObjStateChangeHandler、模拟RpcSend发送成功、
* 3. 设置导出节点为2号节点，导入节点为1号节点,入参importObj状态为UBSE_MEM_IMPORT_DESTROYED
* 4. 设置账本数据，
* 5.调用 UbseMemAddrBorrowImportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,导出对象状态被设置为UBSE_MEM_EXPORT_DESTROYING
*/
TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackFailed)
{
    UbseMemAddrBorrowImportObj importObj{};
    MasterImportCallbackMockSet();
    MasterImportCallbackImportObjSet(importObj);
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemImportResult importResult{};
    importObj.status.importResults.push_back(importResult);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    MOCKER(mem::scheduler::UbseMemAddrImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto exportKey = mem::controller::GenerateExportObjKey(importObj.req.name, importObj.req.importNodeId);
    auto exportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(
        importObj.req.exportNodeId, exportKey);
    EXPECT_TRUE(exportObjPtr != nullptr);
    EXPECT_TRUE(exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYING);
}

/*
* 用例描述
* Addr类型内存的import对象归还回调,master测执行失败；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟rpcsend发送成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. exportObj expectState设置为UBSE_MEM_EXPORT_DESTROYED, state设置为UBSE_MEM_EXPORT_SUCCESS;
* 5.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,对应状态为UBSE_MEM_EXPORT_SUCCESS
*/
TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackDestroyedFailed)
{
    UbseMemAddrBorrowImportObj importObj{};
    MasterImportCallbackMockSet();
    MasterImportCallbackImportObjSet(importObj);
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    UbseMemImportResult importResult{};
    importObj.status.importResults.push_back(importResult);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        importObj.req.importNodeId, importObj.req.name);
    EXPECT_TRUE(importObjPtr != nullptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, importObjPtr->status.state);
}

TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackUnimportFailed)
{
    MasterImportCallbackMockSet();
    UbseMemAddrBorrowImportObj importObj{};
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYING;
    UbseMemImportResult importResult{};
    importObj.req.name = "test";
    importObj.status.importResults.push_back(importResult);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    election::UbseRoleInfo currentInfo{};
    election::UbseRoleInfo masterInfo{};
    std::shared_ptr<com::UbseComModule> module;
    ExportCallbackCommonSetup(currentInfo, masterInfo, module);
    BuildOperationMockSet();
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackSendUnexportFailed)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemAddrBorrowImportObj importObj{};
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.req.name = "test";
    importObj.req.importNodeId = NODE_ONE;
    importObj.req.exportNodeId = NODE_TWO;
    UbseMemAddrBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    UbseMemImportResult importResult{};
    importObj.status.importResults.push_back(importResult);
    auto key = GenerateExportObjKey(importObj.req.name, importObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(importObj.req.exportNodeId,
                                                                                          key, exportObj);
    election::UbseRoleInfo currentInfo{};
    election::UbseRoleInfo masterInfo{};
    std::shared_ptr<com::UbseComModule> module;
    ExportCallbackCommonSetup(currentInfo, masterInfo, module);
    BuildOperationMockSet();
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackNoExport)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemAddrBorrowImportObj importObj{};
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.req.name = "test";
    importObj.req.importNodeId = NODE_ONE;
    importObj.req.exportNodeId = NODE_TWO;
    UbseMemImportResult importResult{};
    importObj.status.importResults.push_back(importResult);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    election::UbseRoleInfo currentInfo{};
    election::UbseRoleInfo masterInfo{};
    std::shared_ptr<com::UbseComModule> module;
    ExportCallbackCommonSetup(currentInfo, masterInfo, module);
    BuildOperationMockSet();
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
}
/*
* 用例描述
* Addr类型内存的import对象归还回调,agent测执行成功；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟SendAddrImportObj发送成功、UbseMemAddrUnImportExecutor返回UBSE_OK
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. 设置账本数据，
* 5.调用 UbseMemAddrBorrowImportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,账本中没有对应name的数据
*/
TEST_F(TestUbseMemControllerAddrApi, AddrImportAgentCallbackSuccess)
{
    UbseMemAddrBorrowImportObj importObj{};
    AgentImportCallbackMockSet();
    AgentImportCallbackImportObjSet(importObj);
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrUnImportExecutor).stubs().will(returnValue(UBSE_OK));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        importObj.req.importNodeId, importObj.req.name);
    EXPECT_TRUE(importObjPtr == nullptr);
}
/*
* 用例描述
* Addr类型内存的import对象归还回调,agent测执行失败；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟SendAddrImportObj发送成功、UbseMemAddrUnImportExecutor返回UBSE_ERROR
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. 设置账本数据，
* 5.调用 UbseMemAddrBorrowImportObjCallback
* 预期结果：
    * 1.函数返回UBSE_OK,账本状态被设置为UBSE_MEM_IMPORT_SUCCESS
*/
TEST_F(TestUbseMemControllerAddrApi, AddrImportAgentCallbackFailed)
{
    MOCKER(&BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    UbseMemAddrBorrowImportObj importObj{};
    AgentImportCallbackMockSet();
    AgentImportCallbackImportObjSet(importObj);
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    MOCKER(&context::UbseContext::GetModule<mmi::UbseMmiModule>)
        .stubs()
        .will(returnValue(std::make_shared<mmi::UbseMmiModule>()));
    MOCKER(&mmi::UbseMmiModule::UbseMemAddrUnImportExecutor).stubs().will(returnValue(UBSE_ERROR));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        importObj.req.importNodeId, importObj.req.name);
    EXPECT_TRUE(importObjPtr != nullptr);
    EXPECT_EQ(UBSE_MEM_IMPORT_SUCCESS, importObjPtr->status.state);
}
/*
* 用例描述: Addr类型内存的import对象归还回调,master测执行成功；
* 测试步骤：
* 1. 设置当前节点为1号，主节点为1号节点；
* 2. 一些必要mock: UbseMemAddrExportObjStateChangeHandler、模拟rpcsend发送成功
* 3. 设置导出节点为2号节点，导入节点为1号节点
* 4. exportObj expectState设置为UBSE_MEM_EXPORT_DESTROYED, state设置为UBSE_MEM_EXPORT_SUCCESS;
* 5.调用 UbseMemAddrBorrowExportObjCallback
* 预期结果：
* 1.函数返回UBSE_OK,对应状态为UBSE_MEM_EXPORT_SUCCESS
*/
TEST_F(TestUbseMemControllerAddrApi, AddrImportMasterCallbackDestroyedSuccess)
{
    UbseMemAddrBorrowImportObj importObj{};
    MasterImportCallbackMockSet();
    MasterImportCallbackImportObjSet(importObj);
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    UbseMemImportResult importResult{};
    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    importObj.status.importResults.push_back(importResult);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
    MOCKER(mem::scheduler::UbseMemAddrImportObjStateChangeHandler).stubs().will(returnValue(UBSE_OK));
    const auto ret = mem::controller::UbseMemAddrBorrowImportObjCallback(importObj);
    EXPECT_EQ(UBSE_OK, ret);
    auto exportKey = mem::controller::GenerateExportObjKey(importObj.req.name, importObj.req.importNodeId);
    auto exportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(
        importObj.req.exportNodeId, exportKey);
    EXPECT_TRUE(exportObjPtr != nullptr);
    EXPECT_TRUE(exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYING);
}

TEST_F(TestUbseMemControllerAddrApi, UbseMemAddrReturnSuccess)
{
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    MOCKER(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    const std::string nodeId = NODE_ONE;
    const std::string name = "addr_test_mem";
    UbseMemAddrBorrowExportObj addrBorrowExportObj{};
    UbseMemAddrBorrowImportObj addrBorrowImportObj{};
    addrBorrowExportObj.req.name = name;
    addrBorrowExportObj.req.importNodeId = NODE_ONE;
    addrBorrowImportObj.req.name = name;
    addrBorrowImportObj.req.importNodeId = NODE_ONE;
    addrBorrowImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    addrBorrowExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    AddToExportObjMap(name, nodeId, addrBorrowExportObj);
    AddToImportObjMap(name, nodeId, addrBorrowImportObj);
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    req.name = name;
    req.importNodeId = NODE_ONE;
    AgentImportCallbackMockSet();
    const auto ret = mem::controller::UbseMemAddrReturn(req, resp, NODE_ONE);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerAddrApi, UbseMemAddrReturnSuccessNotExist)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    const auto ret = mem::controller::UbseMemAddrReturn(req, resp, NODE_ONE);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMemControllerAddrApi, UbseMemAddrReturnSuccessWithoutExport)
{
    UbseMemReturnReq req;
    req.name = "test";
    req.importNodeId = NODE_ONE;
    UbseMemOperationResp resp;
    UbseMemAddrBorrowExportObj exportObj;
    exportObj.req.name = req.name;
    exportObj.req.importNodeId = req.importNodeId;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    AddToExportObjMap(req.name, NODE_ONE, exportObj);
    auto ret = mem::controller::UbseMemAddrReturn(req, resp, NODE_ONE);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseMemControllerAddrApi, DeleteAddrExport)
{
    UbseMemAddrBorrowExportObj exportObj;
    MasterExportCallbackMockSet(exportObj);
    SendAddrExportObjMockSet();
    const auto ret = mem::controller::DeleteAddrExport(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerAddrApi, AddAddrExportDestroyed)
{
    UbseMemAddrBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    const auto ret = mem::controller::AddAddrExport(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemControllerAddrApi, AddAddrExportSuccess)
{
    UbseMemAddrBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    const auto ret = mem::controller::AddAddrExport(exportObj);
    EXPECT_EQ(UBSE_OK, ret);
}
} // namespace ubse::mem_controller::addr::ut
