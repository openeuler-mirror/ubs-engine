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

#include "test_ham_migrate.h"

#include <gmock/gmock-function-mocker.h>
#include <securec.h>
#include <fstream>
#include <regex>

#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include <ubse_api_server.h>
#include <ubse_com.h>
#include <ubse_election.h>
#include <ubse_error.h>
#include <ubse_mem_controller.h>
#include <ubse_node.h>
#include <ubse_ras.h>
#include <ubse_ut_dir.h>
#include "ham_migrate.h"
#include "ham_migrate_dst_info_message.h"
#include "ham_migrate_vm_info_storage.h"
#include "libvirt_helper.h"
#include "migrate_strategy.h"
#include "resource_query.h"
#include "response_info_message.h"
#include "vm_configuration.h"

using namespace vm;
using namespace ubse::mem::controller;
using namespace api::server;

namespace ubse::vm::ut {
static const int PID = 123;
static const std::string NODE = "Node0";
static const int DST_PID = 789;
static const int WAIT_TIME = 10;
static const int CLEAR_WAIT_TIME = 300;
static const int NUMAID_ONE = 1;
static const int NUMAID_TWO = 2;
static const int UBSE_HTTP_STATUS_CODE_OK = 200;
static const int UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR = 500;

TestHamMigrate::TestHamMigrate() = default;

UbseIpcMessage response{};

uint32_t GetSendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage& sendResponse)
{
    response.length = sendResponse.length;
    response.buffer = new (std::nothrow) uint8_t[sendResponse.length];
    memcpy_s(response.buffer, response.length, sendResponse.buffer, response.length);
    return VM_OK;
}

void freeResponse()
{
    if (response.buffer != nullptr) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
    }
}

std::string GetSendResponseString()
{
    return std::string{response.buffer, response.buffer + response.length};
}

void TestHamMigrate::SetUp()
{
    Test::SetUp();
    VmConfiguration::GetInstance().LoadConfig();
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(invoke(GetSendResponse));
}

void TestHamMigrate::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// 读Json文件
VmResult ReadJsonFile(const std::string& filename, std::string& jsonString)
{
    std::string caseDir = std::string(UT_DIRECTORY) + "/exclusive_executable/virt_agent/migrate/case/";
    std::ifstream file(caseDir + filename);
    if (!file.is_open()) {
        UBSE_LOGGER_ERROR(VM_MODULE_NAME, VM_MODULE_CODE) << "Failed to open file: " << filename;
        return VM_ERROR;
    }
    std::string line;
    while (std::getline(file, line)) {
        jsonString += line;
    }
    file.close();
    return VM_OK;
}

VmResult setUbseByteBuffer(const std::string& bodyString, UbseByteBuffer& resp)
{
    size_t len = bodyString.size();
    auto* body = new (std::nothrow) uint8_t[len];
    if (body == nullptr) {
        UBSE_LOGGER_ERROR(VM_MODULE_NAME, VM_MODULE_CODE) << "new response body error";
        return VM_ERROR;
    }
    auto ret = memcpy_s(body, len, bodyString.data(), len);
    if (ret != VM_OK) {
        delete[] body;
        return VM_ERROR;
    }
    resp.data = body;
    resp.len = len;
    resp.freeFunc = [](uint8_t* data) {
        if (data != nullptr) {
            delete[] data;
        }
    };
    return VM_OK;
}

uint32_t UbseNodeGetNodeIdByHostname(const std::string& hostname, std::string& nodeId)
{
    nodeId = hostname;
    return VM_OK;
}

UbseResult UbseNodeGetDebt(const std::string& nodeId, std::vector<UbseMemAddrDesc>& debtInfos)
{
    UbseMemAddrDesc memDebtInfoMap;
    UbseTopoNode ubseTopoNode;
    memDebtInfoMap.name = "ham_asdfdasf";
    ubseTopoNode.slotId = 4;
    memDebtInfoMap.importNode = ubseTopoNode;
    debtInfos.push_back(memDebtInfoMap);
    return UBSE_OK;
}

VmResult GetHostVmDomainInfo(const uint64_t& remoteUsedMem, HostVmDomainInfo& hostVmDomainInfo)
{
    VmDomainInfo vmDomainInfo;
    mempooling::VmDomainNumaInfo vmDomainNumaInfo{0, 0, 0, 0, 0};
    vmDomainInfo.pid = PID;
    vmDomainInfo.nodeId = NODE;
    vmDomainInfo.numaMemInfo.emplace(0, vmDomainNumaInfo);
    vmDomainInfo.uuid = "test-uuid";
    vmDomainInfo.remoteUsedMem = remoteUsedMem;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.emplace_back(vmDomainInfo);
    hostVmDomainInfo.vmDomainInfos = vmDomainInfos;
    return VM_OK;
}

VmResult GetHostVmDomainInfoBorrow(HostVmDomainInfo& hostVmDomainInfo)
{
    GetHostVmDomainInfo(SPECS_4G, hostVmDomainInfo);
    return VM_OK;
}

VmResult GetHostVmDomainInfoNoBorrow(HostVmDomainInfo& hostVmDomainInfo)
{
    GetHostVmDomainInfo(0, hostVmDomainInfo);
    return VM_OK;
}

VmResult GetHamMigrateVmInfo(HamMigrateVmInfo& hamMigrateVmInfo, const std::string& nodeId, int pid, VmState vmState)
{
    hamMigrateVmInfo.pid = pid;
    hamMigrateVmInfo.nodeId = nodeId;
    hamMigrateVmInfo.socketId = 0;
    hamMigrateVmInfo.numaId = 0;
    hamMigrateVmInfo.uuid = "test-uuid";
    hamMigrateVmInfo.borrowName = "Node0/0/0";
    hamMigrateVmInfo.vmState = vmState;
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    return VM_OK;
}

VmResult GetHamMigrateVmInfoBorrow(const std::string& nodeId, int pid, HamMigrateVmInfo& hamMigrateVmInfo)
{
    GetHamMigrateVmInfo(hamMigrateVmInfo, NODE, PID, VmState::BORROWED_NOMIGRATE);
    return VM_OK;
}

VmResult GetHamMigrateVmInfoNoBorrow(const std::string& nodeId, int pid, HamMigrateVmInfo& hamMigrateVmInfo)
{
    GetHamMigrateVmInfo(hamMigrateVmInfo, NODE, PID, VmState::NOBORROW_NOMIGRATE);
    return VM_OK;
}

VmResult GetHamMigrateVmInfos(const std::string& nodeId, std::vector<HamMigrateVmInfo>& hamMigrateVmInfos)
{
    HamMigrateVmInfo hamMigrateVmInfo1;
    GetHamMigrateVmInfo(hamMigrateVmInfo1, NODE, PID, VmState::BORROWED_NOMIGRATE);
    HamMigrateVmInfo hamMigrateVmInfo2;
    GetHamMigrateVmInfo(hamMigrateVmInfo2, NODE, DST_PID, VmState::NOBORROW_NOMIGRATE);
    hamMigrateVmInfo1.timeout = system_clock::now() + milliseconds(WAIT_TIME);
    hamMigrateVmInfo2.timeout = hamMigrateVmInfo1.timeout + milliseconds(WAIT_TIME);
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo1);
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo2);
    return VM_OK;
}

VmResult GetAllHamMigrateVmInfos(std::vector<HamMigrateVmInfo>& hamMigrateVmInfos)
{
    return GetHamMigrateVmInfos("", hamMigrateVmInfos);
}

VmResult GetHamMigrateVmInfoByUUID(const std::string& uuid, HamMigrateVmInfo& hamMigrateVmInfo)
{
    hamMigrateVmInfo.vmState = VmState::BORROWED_NOMIGRATE;
    return VM_OK;
}

VmResult VmInfosByDstNodeIdNotMigrating(const std::string& dstNodeId, std::vector<HamMigrateVmInfo>& hamMigrateVmInfos)
{
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    hamMigrateVmInfo.opState = OpState::START;
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo);
    return VM_OK;
}

VmResult VmInfosByDstNodeIdMigrating(const std::string& dstNodeId, std::vector<HamMigrateVmInfo>& hamMigrateVmInfos)
{
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.uuid = "vm-uuid";
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    hamMigrateVmInfo.opState = OpState::END;
    hamMigrateVmInfo.vmState = VmState::NOBORROW_NOMIGRATE;
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo);
    return VM_OK;
}

VmResult DoUbseBorrowAddress(const BorrowInfo& borrowInfo, BorrowResponse& borrowResponse)
{
    borrowResponse.name = "Node0/0/0";
    std::vector<int16_t> numaList;
    numaList.emplace_back(NUMAID_ONE);
    numaList.emplace_back(NUMAID_TWO);
    borrowResponse.numaIds = numaList;
    borrowResponse.scna = 1;
    return VM_OK;
}

struct CheckNodeId {
    explicit CheckNodeId(const std::string& mNodeId) : nodeId(mNodeId) {}

    std::string nodeId;

    bool operator()(const std::string& checkNodeId)
    {
        return nodeId == checkNodeId;
    }
};

struct CheckPid {
    explicit CheckPid(const int& mPid) : pid(mPid) {}

    int pid;

    bool operator()(const int& checkPid)
    {
        return pid == checkPid;
    }
};

struct CheckVmInfo {
    CheckVmInfo(const VmState& mVmState, const VmOpState& mVmOpState) : vmState(mVmState), vmOpstate(mVmOpState) {}

    VmState vmState;
    VmOpState vmOpstate;

    bool operator()(const HamMigrateVmInfo& hamMigrateVmInfo)
    {
        return hamMigrateVmInfo.vmState == vmState && hamMigrateVmInfo.vmOpState == vmOpstate;
    }
};

void mock_migrate_fail_without_rollback()
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo).expects(never());
    MOCKER(HamMigrate::EnterClearQueue, void(HamMigrateVmInfo & hamMigrateVmInfo, const bool& isUpdate))
        .expects(never());
    MOCKER(HamMigrate::EnterClearQueue, void(std::vector<HamMigrateVmInfo> & hamMigrateVmInfos, const bool& isUpdate))
        .expects(never());

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);

    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
}

void mock_migrate_fail_clear_success(uint64_t sleep_ms)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    UbseByteBuffer resp{};
    std::string clearJson = R"({"action": "clear", "type": 2, "srcHostname": "Node0", "srcPid": 123})";
    HttpUtil::ToUbseByteBuffer(clearJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoBorrow));

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::BORROWED_ADDRESS)))
        .will(returnValue(VM_OK))
        .id("0");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .after("0")
        .will(returnValue(VM_OK))
        .id("1");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::DISABLE_PROCESS_MIGRATE)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");
    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(PID)))
        .after("2")
        .will(returnValue(VM_OK))
        .id("3");

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = NODE;
    hamMigrateVmInfo.pid = PID;
    hamMigrateVmInfo.timeout = system_clock::now() + minutes(WAIT_TIME);
    HamMigrate::EnterClearQueue(hamMigrateVmInfo);

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
    std::this_thread::sleep_for(milliseconds(sleep_ms));
    HamMigrate::Stop();
}

void mock_noborrow_migrate_fail_clear_success(uint64_t sleep_ms)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    UbseByteBuffer resp{};
    std::string clearJson = R"({"action": "clear", "type": 2, "srcHostname": "Node0", "srcPid": 123})";
    setUbseByteBuffer(clearJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoNoBorrow));
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::BORROWED_ADDRESS)))
        .will(returnValue(VM_OK))
        .id("0");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .after("0")
        .will(returnValue(VM_OK))
        .id("1");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::NOPE)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");
    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(PID)))
        .after("2")
        .will(returnValue(VM_OK))
        .id("3");

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = NODE;
    hamMigrateVmInfo.pid = PID;
    hamMigrateVmInfo.timeout = system_clock::now() + minutes(WAIT_TIME);
    HamMigrate::EnterClearQueue(hamMigrateVmInfo);

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
    std::this_thread::sleep_for(milliseconds(sleep_ms));
    HamMigrate::Stop();
}

/*
 * 用例描述：借用场景, 使能冷热页失败, 整体失败
 * Borrow_EnableProcessMigrate_failed
 * 测试步骤：
 * 1. mock使能冷热页接口失败
 * 2. 其他接口成功
 * 预期结果：
 * 1. 不存数据库操作
 * 2. 不会有数据进入回退队列
 * 3. code=500
 */

TEST_F(TestHamMigrate, Borrow_EnableProcessMigrate_failed)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_ERROR));
    mock_migrate_fail_without_rollback();
}

/*
* 用例描述：借用场景, 设置冷热扫描失败, 整体失败
* Borrow_AddProcessTracking_failed
* 测试步骤：
* 1. mock设置冷热扫描接口失败
* 2. 其他接口成功
* 预期结果：
* 1. 存数据库操作
* 2. 有数据进入回退队列
* 3. code=500
* code=500
*/

constexpr int64_t srcPid = 123;

std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> globalNumaMemInfoBorrow = {{
                                                                                         0,
                                                                                         {0, 0, 0, 0, false},
                                                                                     },
                                                                                     {
                                                                                         1,
                                                                                         {1, 1, 1, 1, true},
                                                                                     }};

VmResult MockGetVmDomainInfosFromGlobalOne(HostVmDomainInfo& hostVmDomainInfo)
{
    VmDomainInfo vmDomainInfo{.remoteUsedMem = 1, .pid = srcPid};
    vmDomainInfo.numaMemInfo = globalNumaMemInfoBorrow;
    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    return VM_OK;
}

std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> globalNumaMemInfoNoBorrow = {{0, {0, 0, 0, 0, true}}};

VmResult MockGetVmDomainInfosFromGlobalZero(HostVmDomainInfo& hostVmDomainInfo)
{
    VmDomainInfo vmDomainInfo{.remoteUsedMem = 0, .pid = srcPid};
    vmDomainInfo.numaMemInfo = globalNumaMemInfoNoBorrow;
    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    return VM_OK;
}

TEST_F(TestHamMigrate, Borrow_AddProcessTracking_failed)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_ERROR));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

/*
* 用例描述：借用场景, 借用虚拟机地址失败, 整体失败
* Borrow_BorrowAddress_failed
* 测试步骤：
* 1. mock借用虚拟机地址失败接口失败
* 2. 其他接口成功
* 预期结果：
* 1. 存数据库操作
* 2. 有数据进入回退队列
* 3. code=500
*/

TEST_F(TestHamMigrate, Borrow_BorrowAddress_failed)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(returnValue(VM_ERROR));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::DISABLE_PROCESS_MIGRATE)))
        .will(returnValue(VM_OK))
        .id("0");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .after("0")
        .will(returnValue(VM_OK))
        .id("1");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::BORROWED_ADDRESS)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");

    MOCKER(HamMigrate::EnterClearQueue, void(HamMigrateVmInfo & hamMigrateVmInfo, const bool& isUpdate))
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::BORROWED_ADDRESS)));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

/*
* 用例描述：借用场景, 发送http请求失败, 整体失败
* Borrow_httpSend_failed
* 测试步骤：
* 1. mock发送http请求失败
* 2. 调用借用请求
* 预期结果：
* E2. code=500
*/

TEST_F(TestHamMigrate, Borrow_httpSend_failed)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoBorrow));
    mock_migrate_fail_without_rollback();
}

/*
* 用例描述：借用场景, 采集虚机信息异常(等价存储模块异常), 整体失败
* Borrow_ExportQuery_failed
* 测试步骤：
* 1. mock采集虚机信息异常
* 2. 调用借用请求
* 预期结果：
* E2. code=500
*/

TEST_F(TestHamMigrate, Borrow_ExportQuery_failed)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    mock_migrate_fail_without_rollback();
}

/*
* 用例描述：借用场景, 数据同步失败, 整体失败
* Borrow_SyncData_failed
* 测试步骤：
* 1. mock数据同步接口失败
* 2. 其他接口成功
* 预期结果：
* E2. code=500
*/

TEST_F(TestHamMigrate, Borrow_SyncData_failed)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).expects(atLeast(1)).will(returnValue(VM_OK));
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));

    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfoStorage::GetHamMigrateVmInfos(NODE, hamMigrateVmInfos);
    EXPECT_TRUE(hamMigrateVmInfos.empty());
    HamMigrate::Stop();
}

/*
* 用例描述：借用场景借用成功
* Borrow_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock存数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 存数据库操作，入参校验成功
* 2. 不会有数据进入回退队列
* 3. code=200, name, numaId 符合预期
*/

TEST_F(TestHamMigrate, Borrow_success)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(invoke(DoUbseBorrowAddress));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::DISABLE_PROCESS_MIGRATE)))
        .will(returnValue(VM_OK))
        .id("0");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .after("0")
        .will(returnValue(VM_OK))
        .id("1");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::BORROWED_ADDRESS)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    BorrowResponse borrowResponse;
    borrowResponse.name = "Node0/0/0";
    std::vector<int16_t> numaList;
    numaList.emplace_back(NUMAID_ONE);
    numaList.emplace_back(NUMAID_TWO);
    borrowResponse.numaIds = numaList;
    borrowResponse.scna = 1;
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    respInfo.message = borrowResponse.ToJson();
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
}

TEST_F(TestHamMigrate, Borrow_CheckPid_Failed)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_ERROR));
    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
}

/*
* 用例描述：借用场景, 迁移成功后归还成功
* Migrate_success_clear_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 数据库操作，入参校验成功
* 2. 数据删除成功
* 3. code=200
*/

TEST_F(TestHamMigrate, Migrate_success_clear_success)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string clearJson = R"({"action": "clear", "type": 1, "srcHostname": "Node0", "srcPid": 123})";
    setUbseByteBuffer(clearJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_MIGRATED, VmOpState::BORROWED_ADDRESS)))
        .will(returnValue(VM_OK))
        .id("0");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_MIGRATED, VmOpState::PROCESS_TRACKING)))
        .after("0")
        .will(returnValue(VM_OK))
        .id("1");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_MIGRATED, VmOpState::NOPE)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");

    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(PID)))
        .after("2")
        .will(returnValue(VM_OK))
        .id("3");

    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = NODE;
    hamMigrateVmInfo.pid = PID;
    hamMigrateVmInfo.timeout = system_clock::now() + minutes(WAIT_TIME);
    HamMigrate::EnterClearQueue(hamMigrateVmInfo);

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

/*
* 用例描述：借用场景, 迁移失败后内存归还失败, 重试通过
* Migrate_fail_returnMem_fail_retry_success
* 测试步骤：
* 1. mock第一次内存归还失败，第二次归还成功
* 2. mock设置冷热扫描时间成功，使能冷热页迁移成功
* 3. 调用迁移失败后清理接口
* 预期结果：
* E3. 数据库操作，入参校验成功; 数据删除成功; code=200
*/

TEST_F(TestHamMigrate, Migrate_fail_returnMem_fail_retry_success)
{
    GTEST_SKIP();
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

/*
* 用例描述：借用场景, 迁移失败后设置冷热页扫描时间失败, 重试通过
* Migrate_fail_AddProcessTracking_fail_retry_success
* 测试步骤：
* 1. mock第一次设置冷热扫描时间失败，第二次设置冷热扫描时间成功
* 2. mock设置内存归还成功，使能冷热页迁移成功
* 3. 调用迁移失败后清理接口
* 预期结果：
* E3. 数据库操作，入参校验成功; 数据删除成功; code=200
*/

TEST_F(TestHamMigrate, Migrate_fail_AddProcessTracking_fail_retry_success)
{
    GTEST_SKIP();
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

/*
* 用例描述：借用场景, 迁移失败后使能冷热页迁移失败, 重试通过
* Migrate_fail_EnableProcessMigrate_fail_retry_success
* 测试步骤：
* 1. mock第一次使能冷热页迁移成功失败，第二次使能冷热页迁移成功成功
* 2. mock设置内存归还成功，设置冷热页扫描时间成功
* 3. 调用迁移失败后清理接口
* 预期结果：
* E3. 数据库操作，入参校验成功; 数据删除成功; code=200
*/

TEST_F(TestHamMigrate, Migrate_fail_EnableProcessMigrate_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

/*
* 用例描述：借用场景, 迁移失败后归还成功
* Migrate_fail_clear_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock存数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 数据库操作，入参校验成功
* 2. 数据删除成功
* 3. code=200
*/

TEST_F(TestHamMigrate, Migrate_fail_clear_success)
{
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(CLEAR_WAIT_TIME);
}

/*
* 用例描述：非借用场景, 借用虚拟机地址失败, 整体失败
* NoBorrow_BorrowAddress_failed
* 测试步骤：
* 1. mock借用虚拟机地址失败接口失败
* 2. 其他接口成功
* 预期结果：
* 1. 存数据库操作
* 2. 有数据进入回退队列
* 3. code=500
*/

TEST_F(TestHamMigrate, NoBorrow_BorrowAddress_failed)
{
    GTEST_SKIP();
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalZero));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoNoBorrow));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(returnValue(VM_ERROR));
    MOCKER(HamMigrate::DoProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .will(returnValue(VM_OK))
        .id("1");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::BORROWED_ADDRESS)))
        .will(returnValue(VM_OK))
        .id("1");

    MOCKER(HamMigrate::EnterClearQueue, void(HamMigrateVmInfo & hamMigrateVmInfo, const bool& isUpdate))
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::BORROWED_ADDRESS)));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
    HamMigrate::Stop();
}

/*
* 用例描述：非借用场景，借用远端地址成功
* NoBorrow_Borrow_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock存数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 存数据库操作，入参校验成功
* 2. 不会有数据进入回退队列
* 3. code=200, name, numaId 符合预期
*/

TEST_F(TestHamMigrate, NoBorrow_Borrow_success)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalZero));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetLocalHostVmDomainInfo).stubs().will(invoke(GetHostVmDomainInfoNoBorrow));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(invoke(DoUbseBorrowAddress));
    MOCKER(HamMigrate::DoProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .will(returnValue(VM_OK))
        .id("1");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::BORROWED_ADDRESS)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    BorrowResponse borrowResponse;
    borrowResponse.name = "Node0/0/0";
    std::vector<int16_t> numaList;
    numaList.emplace_back(NUMAID_ONE);
    numaList.emplace_back(NUMAID_TWO);
    borrowResponse.numaIds = numaList;
    borrowResponse.scna = 1;
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    respInfo.message = borrowResponse.ToJson();
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
    HamMigrate::Stop();
}

/*
* 用例描述：非借用场景, 迁移成功后归还成功
* NoBorrow_Migrate_success_clear_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 数据库操作，入参校验成功
* 2. 数据删除成功
* 3. code=200
*/

TEST_F(TestHamMigrate, NoBorrow_Migrate_success_clear_success)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string clearJson = R"({"action": "clear", "type": 1, "srcHostname": "Node0", "srcPid": 123})";
    setUbseByteBuffer(clearJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoNoBorrow));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_MIGRATED, VmOpState::BORROWED_ADDRESS)))
        .will(returnValue(VM_OK))
        .id("0");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_MIGRATED, VmOpState::PROCESS_TRACKING)))
        .after("0")
        .will(returnValue(VM_OK))
        .id("1");
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_MIGRATED, VmOpState::NOPE)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");
    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(PID)))
        .after("2")
        .will(returnValue(VM_OK))
        .id("3");
    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = NODE;
    hamMigrateVmInfo.pid = PID;
    hamMigrateVmInfo.timeout = system_clock::now() + minutes(WAIT_TIME);
    HamMigrate::EnterClearQueue(hamMigrateVmInfo);

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

/*
* 用例描述：非借用场景, 迁移失败后内存归还失败, 重试通过
* NoBorrow_Migrate_fail_returnMem_fail_retry_success
* 测试步骤：
* 1. mock第一次内存归还失败，第二次归还成功
* 2. mock移除冷热页扫描成功
* 3. 调用迁移失败后清理接口
* 预期结果：
* E3. 数据库操作，入参校验成功; 数据删除成功; code=200
*/

TEST_F(TestHamMigrate, NoBorrow_Migrate_fail_returnMem_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    mock_noborrow_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

/*
* 用例描述：借用场景, 迁移失败后设置冷热页扫描时间失败, 重试通过
* NoBorrow_Migrate_fail_RemoveProcessTracking_fail_retry_success
* 测试步骤：
* 1. mock第一次移除冷热页扫描失败，第二次移除冷热页扫描成功
* 2. mock内存归还成功
* 3. 调用迁移失败后清理接口
* 预期结果：
* E3. 数据库操作，入参校验成功; 数据删除成功; code=200
*/

TEST_F(TestHamMigrate, NoBorrow_Migrate_fail_RemoveProcessTracking_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    mock_noborrow_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

/*
* 用例描述：非借用场景, 迁移失败后归还成功
* NoBorrow_Migrate_fail_clear_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock存数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 数据库操作，入参校验成功
* 2. 数据删除成功
* 3. code=200
*/

TEST_F(TestHamMigrate, NoBorrow_Migrate_fail_clear_success)
{
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    mock_noborrow_migrate_fail_clear_success(CLEAR_WAIT_TIME);
}

/*
* 用例描述：重启场景, 清理成功
* restart_clear_success
* 测试步骤：
* 1. 接口调用成功
* 2. mock存数据库操作，根据调用顺序校验入参的变化
* 预期结果：
* 1. 数据库操作，入参校验成功
* 2. 数据删除成功
* 3. code=200
*/

TEST_F(TestHamMigrate, restart_clear_success)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetAllHamMigrateVmInfos).stubs().will(invoke(GetAllHamMigrateVmInfos));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .will(returnValue(VM_OK))
        .id("1");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::DISABLE_PROCESS_MIGRATE)))
        .after("1")
        .will(returnValue(VM_OK))
        .id("2");

    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(PID)))
        .after("2")
        .will(returnValue(VM_OK))
        .id("3");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::PROCESS_TRACKING)))
        .after("3")
        .will(returnValue(VM_OK))
        .id("4");

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::NOPE)))
        .after("4")
        .will(returnValue(VM_OK))
        .id("5");

    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(once())
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(DST_PID)))
        .after("5")
        .will(returnValue(VM_OK))
        .id("6");

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

/*
* 用例描述：libvirt重启场景, 清理成功
* libvirt_restart_clear_success
* 测试步骤：
* 1. 接口调用成功
* 预期结果：
* 1. 调用删除数据库的方法
* 2. code=200
*/

TEST_F(TestHamMigrate, libvirt_restart_clear_success)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    UbseByteBuffer resp{};
    std::string clearJson = R"({"action": "clear", "type": 0, "srcHostname": "Node0"})";
    setUbseByteBuffer(clearJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfos).stubs().will(invoke(GetHamMigrateVmInfos));
    MOCKER(HamMigrateVmInfoStorage::GetAllHamMigrateVmInfos).stubs().will(invoke(GetAllHamMigrateVmInfos));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::CheckPid).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(PID)))
        .will(returnValue(VM_OK));
    MOCKER(HamMigrateVmInfoStorage::DelHamMigrateVmInfo)
        .expects(atLeast(1))
        .with(checkWith(CheckNodeId(NODE)), checkWith(CheckPid(DST_PID)))
        .will(returnValue(VM_OK));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_OK);
    EXPECT_EQ(GetSendResponseString().substr(0, GetSendResponseString().length() - 1), respInfo.ToJson());
    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

/*
 * 用例描述：测试启动成功, 停止成功
 * start_stop
 * 测试步骤：
 * 1. 调用start
 * 2. 调用run
 * 3. 调用stop
 * 预期结果：
 * 1. 调用start成功
 * 2. 调用run成功
 * 3. 调用stop成功
 */

TEST_F(TestHamMigrate, start_stop)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_OK));
    auto ret = HamMigrate::Start();
    EXPECT_NE(ret, VM_OK);
    MOCKER(com::UbseRegRpcService).stubs().will(returnValue(VM_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE alarmFaultEvent, std::string name,
                                               AlarmFaultHandler handler, AlarmHandlerPriority priority))
        .stubs()
        .will(returnValue(VM_ERROR));
    ret = HamMigrate::Start();
    EXPECT_NE(ret, VM_OK);
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE alarmFaultEvent, std::string name,
                                               AlarmFaultHandler handler, AlarmHandlerPriority priority))
        .reset();
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE alarmFaultEvent, std::string name,
                                               AlarmFaultHandler handler, AlarmHandlerPriority priority))
        .stubs()
        .will(returnValue(VM_OK));
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_OK));
    ret = HamMigrate::Start();
    EXPECT_EQ(ret, VM_OK);
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = NODE;
    hamMigrateVmInfo.pid = PID;
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    HamMigrate::EnterClearQueue(hamMigrateVmInfo);
    ret = HamMigrate::Run();
    EXPECT_EQ(ret, VM_OK);
    ret = HamMigrate::Stop();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    EXPECT_EQ(ret, VM_OK);
}

/*
 * 用例描述：校验请求数据
 * HamMigrateNorth
 * 测试步骤：
 * 1. 构造非json, 调用HamMigrateNorth
 * 2. 构造json无action, 调用HamMigrateNorth
 * 3. 构造borrow的json, 调用HamMigrateNorth
 * 预期结果：
 * 1. 调用返回失败
 * 2. 调用返回失败
 * 3. 调用返回失败
 */

TEST_F(TestHamMigrate, HamMigrateNorth)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    UbseByteBuffer resp{};
    std::string reJson = "hello world";
    setUbseByteBuffer(reJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;
    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = static_cast<int>(UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR);
    std::string respString{resp.data, resp.data + resp.len};
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();

    reJson = "{}";
    setUbseByteBuffer(reJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;
    HamMigrate::HamMigrateNorth(req, context);
    std::string respErrorString{resp.data, resp.data + resp.len};
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();

    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoNoBorrow));
    reJson = R"({"action": "borrow", "srcHostname": "Node0", "srcPid": 123})";
    setUbseByteBuffer(reJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;
    HamMigrate::HamMigrateNorth(req, context);
    std::string respBorrowString{resp.data, resp.data + resp.len};
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
}

TEST_F(TestHamMigrate, HasTask)
{
    UbseIpcMessage req{};
    UbseByteBuffer reqTmp{};
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    HttpUtil::ToUbseByteBuffer(borrowJson, reqTmp);
    req.buffer = reqTmp.data;
    req.length = reqTmp.len;

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_OK));
    UbseRequestContext context;
    HamMigrate::HamMigrateNorth(req, context);
    RespInfo respInfo;
    respInfo.code = 500;
    EXPECT_EQ(GetSendResponseString(), "");
    freeResponse();
}

TEST_F(TestHamMigrate, HamMigrateCancel)
{
    std::string uuid = "abcd-1234";
    std::string uuid_expect;
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    LibvirtHelper::GetInstance().DeInit();
    HttpUtil::ToUbseByteBuffer(uuid, req);
    HamMigrate::HamMigrateCancel(req, resp);
}

TEST_F(TestHamMigrate, HamMigrateCancelReply)
{
    UbseByteBuffer resp;
    std::string uuid = "abcd-1234";
    HamMigrate::HamMigrateCancelReply(nullptr, resp, VM_ERROR);
    HamMigrate::HamMigrateCancelReply(nullptr, resp, VM_OK);
    HttpUtil::SetResp(resp, VM_OK, uuid);
    HamMigrate::HamMigrateCancelReply(nullptr, resp, VM_OK);
}

TEST_F(TestHamMigrate, PanicEventHandler)
{
    auto panicEvent = NUMAID_ONE;
    std::string node = "Node2";
    MOCKER(UbseGetAddrMemDebtInfoWithNode).stubs().will(returnValue(UBSE_OK));
    auto ret = HamMigrate::PanicEventHandler(panicEvent, node);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(UbseGetAddrMemDebtInfoWithNode).reset();
    MOCKER(UbseGetAddrMemDebtInfoWithNode).stubs().will(invoke(UbseNodeGetDebt));
    MOCKER(com::UbseRpcAsyncSend).stubs().will(returnValue(VM_OK));
    ret = HamMigrate::PanicEventHandler(panicEvent, node);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestHamMigrate, Second_DoUbseBorrowAddress)
{
    BorrowInfo borrowInfo{.srcNodeId = "1", .dstNodeId = "2", .dstPid = 0, .dstSocket = 1, .valist = {{1, 2}}};
    BorrowResponse borrowResponse{};
    MOCKER(UbseMemAddrCreate).stubs().will(returnValue(UBSE_ERR_AUTH_FAILED)).then(returnValue(UBSE_OK));
    EXPECT_EQ(HamMigrate::DoUbseBorrowAddress(borrowInfo, borrowResponse), VM_ERROR);
    EXPECT_EQ(HamMigrate::DoUbseBorrowAddress(borrowInfo, borrowResponse), VM_OK);
}

TEST_F(TestHamMigrate, GetMasterNodeId_Success)
{
    MOCKER(election::UbseGetMasterInfo).stubs().will(returnValue(VM_OK));
    std::string masterNodeId = HamMigrate::GetMasterNodeId();
    EXPECT_FALSE(masterNodeId.empty());
}

TEST_F(TestHamMigrate, GetMasterNodeId_Failed)
{
    MOCKER(election::UbseGetMasterInfo).stubs().will(returnValue(VM_ERROR));
    std::string masterNodeId = HamMigrate::GetMasterNodeId();
    EXPECT_TRUE(masterNodeId.empty());
}

TEST_F(TestHamMigrate, CheckPid_Serialize_Failed)
{
    BorrowInfo borrowInfo;
    borrowInfo.dstPid = 123;
    borrowInfo.dstNodeId = "node01";

    VmResult result = HamMigrate::CheckPid(borrowInfo);
    EXPECT_EQ(result, VM_ERROR);
}

TEST_F(TestHamMigrate, SrcNodeInfoReplyHandler_Success)
{
    unsigned int result = VM_ERROR;
    void* ctx = &result;
    ResponseInfoMessage response;
    response.SetResponseInfo(VM_OK, "");
    response.Serialize();
    const UbseByteBuffer respData = {.data = response.SerializedData(), .len = response.SerializedDataSize()};

    HamMigrate::SrcNodeInfoReplyHandler(ctx, respData, VM_OK);
    EXPECT_EQ(result, VM_OK);
}

TEST_F(TestHamMigrate, SrcNodeInfoReplyHandler_Failed)
{
    unsigned int result = VM_ERROR;
    void* ctx = &result;
    const UbseByteBuffer respData = {.data = nullptr, .len = 0};

    HamMigrate::SrcNodeInfoReplyHandler(ctx, respData, VM_ERROR);
    EXPECT_EQ(result, VM_ERROR);
}

TEST_F(TestHamMigrate, MasterDstInfoReplyHandler_Success)
{
    unsigned int result = VM_ERROR;
    void* ctx = &result;
    ResponseInfoMessage response;
    response.SetResponseInfo(VM_OK, "");
    response.Serialize();
    const UbseByteBuffer respData = {.data = response.SerializedData(), .len = response.SerializedDataSize()};

    HamMigrate::MasterDstInfoReplyHandler(ctx, respData, VM_OK);
    EXPECT_EQ(result, VM_OK);
}

TEST_F(TestHamMigrate, MasterDstInfoReplyHandler_Failed)
{
    unsigned int result = VM_ERROR;
    void* ctx = &result;
    const UbseByteBuffer respData = {.data = nullptr, .len = 0};

    HamMigrate::MasterDstInfoReplyHandler(ctx, respData, VM_ERROR);
    EXPECT_EQ(result, VM_ERROR);
}

TEST_F(TestHamMigrate, MasterDstInfoHandler_Request)
{
    HamMigrateDstInfoMessage hamMigrateDstInfoMessage;
    hamMigrateDstInfoMessage.SetHamMigrateDstInfo(1234, "1");
    hamMigrateDstInfoMessage.Serialize();
    const UbseByteBuffer respData = {.data = hamMigrateDstInfoMessage.SerializedData(),
                                     .len = hamMigrateDstInfoMessage.SerializedDataSize()};
    UbseByteBuffer resp;
    MOCKER(com::UbseRpcSend).stubs().will(returnValue(VM_OK));
    HamMigrate::MasterDstInfoHandler(respData, resp);
    ResponseInfoMessage responseInfoSimpo(resp.data, resp.len);
    responseInfoSimpo.Deserialize();
    const auto [code, message] = responseInfoSimpo.GetResponseInfo();
    EXPECT_EQ(code, VM_ERROR);
}

TEST_F(TestHamMigrate, AgentDstInfoHandler_Request)
{
    uint64_t pid = 123;
    const UbseByteBuffer respData = {.data = (uint8_t*)&pid, .len = sizeof(uint64_t)};
    UbseByteBuffer resp;
    HamMigrate::AgentDstInfoHandler(respData, resp);
    MOCKER(HamMigrate::PidIsVm).stubs().will(returnValue(VM_OK));
    ResponseInfoMessage responseInfoSimpo(resp.data, resp.len);
    responseInfoSimpo.Deserialize();
    const auto [code, message] = responseInfoSimpo.GetResponseInfo();
    EXPECT_EQ(code, VM_ERROR);
}

TEST_F(TestHamMigrate, PidIsVm_NoHugePage)
{
    uint64_t pid = 456;
    EXPECT_EQ(HamMigrate::PidIsVm(pid), VM_ERROR);
}

} // namespace ubse::vm::ut