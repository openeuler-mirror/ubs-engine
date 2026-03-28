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
#include "libvirt_handler.h"
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

uint32_t GetSendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &sendResponse)
{
    response.length = sendResponse.length;
    response.buffer = new (std::nothrow) uint8_t[sendResponse.length];
    if (response.buffer != nullptr) {
        memcpy_s(response.buffer, response.length, sendResponse.buffer, sendResponse.length);
    }
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

VmResult GetHamMigrateVmInfo(HamMigrateVmInfo &hamMigrateVmInfo, const std::string &nodeId, int pid, VmState vmState)
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

VmResult GetHamMigrateVmInfos(const std::string &nodeId, std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
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

void TestHamMigrate::SetUp()
{
    Test::SetUp();
    VmConfiguration::GetInstance().LoadConfig();
    MOCKER(com::UbseRpcSend).stubs().will(returnValue(VM_OK));
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(invoke(GetSendResponse));
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_OK));
    MOCKER(com::UbseRegRpcService).stubs().will(returnValue(VM_OK));
}

void TestHamMigrate::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// 读Json文件
VmResult ReadJsonFile(const std::string &filename, std::string &jsonString)
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

VmResult setUbseByteBuffer(const std::string &bodyString, UbseByteBuffer &resp)
{
    size_t len = bodyString.size();
    auto *body = new (std::nothrow) uint8_t[len];
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
    resp.freeFunc = [](uint8_t *data) {
        if (data != nullptr) {
            delete[] data;
        }
    };
    return VM_OK;
}

uint32_t UbseNodeGetNodeIdByHostname(const std::string &hostname, std::string &nodeId)
{
    nodeId = hostname;
    return VM_OK;
}

UbseResult UbseNodeGetDebt(const std::string &nodeId, std::vector<UbseMemAddrDesc> &debtInfos)
{
    UbseMemAddrDesc memDebtInfoMap;
    UbseTopoNode ubseTopoNode;
    memDebtInfoMap.name = "ham_asdfdasf";
    ubseTopoNode.slotId = 4;
    memDebtInfoMap.importNode = ubseTopoNode;
    debtInfos.push_back(memDebtInfoMap);
    return UBSE_OK;
}

VmResult GetHostVmDomainInfo(const uint64_t &remoteUsedMem, HostVmDomainInfo &hostVmDomainInfo)
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

VmResult GetHostVmDomainInfoBorrow(HostVmDomainInfo &hostVmDomainInfo)
{
    GetHostVmDomainInfo(SPECS_4G, hostVmDomainInfo);
    return VM_OK;
}

VmResult GetHostVmDomainInfoNoBorrow(HostVmDomainInfo &hostVmDomainInfo)
{
    GetHostVmDomainInfo(0, hostVmDomainInfo);
    return VM_OK;
}

VmResult GetHamMigrateVmInfoBorrow(const std::string &nodeId, int pid, HamMigrateVmInfo &hamMigrateVmInfo)
{
    GetHamMigrateVmInfo(hamMigrateVmInfo, NODE, PID, VmState::BORROWED_NOMIGRATE);
    return VM_OK;
}

VmResult GetHamMigrateVmInfoNoBorrow(const std::string &nodeId, int pid, HamMigrateVmInfo &hamMigrateVmInfo)
{
    GetHamMigrateVmInfo(hamMigrateVmInfo, NODE, PID, VmState::NOBORROW_NOMIGRATE);
    return VM_OK;
}

VmResult GetAllHamMigrateVmInfos(std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    return GetHamMigrateVmInfos("", hamMigrateVmInfos);
}

VmResult VmInfosByDstNodeIdNotMigrating(const std::string &dstNodeId, std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    hamMigrateVmInfo.opState = OpState::START;
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo);
    return VM_OK;
}

VmResult VmInfosByDstNodeIdMigrating(const std::string &dstNodeId, std::vector<HamMigrateVmInfo> &hamMigrateVmInfos)
{
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.uuid = "vm-uuid";
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    hamMigrateVmInfo.opState = OpState::END;
    hamMigrateVmInfo.vmState = VmState::NOBORROW_NOMIGRATE;
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo);
    return VM_OK;
}

VmResult DoUbseBorrowAddress(const BorrowInfo &borrowInfo, BorrowResponse &borrowResponse)
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
    explicit CheckNodeId(const std::string &mNodeId) : nodeId(mNodeId) {}

    std::string nodeId;

    bool operator()(const std::string &checkNodeId)
    {
        return nodeId == checkNodeId;
    }
};

struct CheckPid {
    explicit CheckPid(const int &mPid) : pid(mPid) {}

    int pid;

    bool operator()(const int &checkPid)
    {
        return pid == checkPid;
    }
};

struct CheckVmInfo {
    CheckVmInfo(const VmState &mVmState, const VmOpState &mVmOpState) : vmState(mVmState), vmOpstate(mVmOpState) {}

    VmState vmState;
    VmOpState vmOpstate;

    bool operator()(const HamMigrateVmInfo &hamMigrateVmInfo)
    {
        return hamMigrateVmInfo.vmState == vmState && hamMigrateVmInfo.vmOpState == vmOpstate;
    }
};

void mock_migrate_fail_without_rollback()
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(HamMigrateVmInfoStorage::SetHamMigrateVmInfo).expects(never());
    MOCKER(HamMigrate::EnterClearQueue, void(HamMigrateVmInfo & hamMigrateVmInfo, const bool &isUpdate))
        .expects(never());
    MOCKER(HamMigrate::EnterClearQueue, void(std::vector<HamMigrateVmInfo> & hamMigrateVmInfos, const bool &isUpdate))
        .expects(never());

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    // 直接调用HandleHamMigrateBorrow，模拟之前通过North入口调用的测试
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
}

void mock_migrate_fail_clear_success(uint64_t sleep_ms)
{
    rapidjson::Document msgJson;
    std::string clearJson = R"({"action": "clear", "type": 2, "srcHostname": "Node0", "srcPid": 123})";
    msgJson.Parse(clearJson.c_str());

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateClear(msgJson, respInfo, resp, context);

    // 验证ProcessResponse是否被调用，此处不深入Mock ProcessResponse内部，因为这是HamMigrate单元测试
    freeResponse();
    std::this_thread::sleep_for(milliseconds(sleep_ms));
    HamMigrate::Stop();
}

void mock_noborrow_migrate_fail_clear_success(uint64_t sleep_ms)
{
    rapidjson::Document msgJson;
    std::string clearJson = R"({"action": "clear", "type": 2, "srcHostname": "Node0", "srcPid": 123})";
    msgJson.Parse(clearJson.c_str());

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateClear(msgJson, respInfo, resp, context);
    freeResponse();
    std::this_thread::sleep_for(milliseconds(sleep_ms));
    HamMigrate::Stop();
}

constexpr int64_t srcPid = 123;

std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> globalNumaMemInfoBorrow = {{
                                                                                         0,
                                                                                         {0, 0, 0, 0, false},
                                                                                     },
                                                                                     {
                                                                                         1,
                                                                                         {1, 1, 1, 1, true},
                                                                                     }};

VmResult MockGetVmDomainInfosFromGlobalOne(HostVmDomainInfo &hostVmDomainInfo)
{
    VmDomainInfo vmDomainInfo{.remoteUsedMem = 1, .pid = srcPid};
    vmDomainInfo.numaMemInfo = globalNumaMemInfoBorrow;
    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    return VM_OK;
}

std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> globalNumaMemInfoNoBorrow = {{0, {0, 0, 0, 0, true}}};

VmResult MockGetVmDomainInfosFromGlobalZero(HostVmDomainInfo &hostVmDomainInfo)
{
    VmDomainInfo vmDomainInfo{.remoteUsedMem = 0, .pid = srcPid};
    vmDomainInfo.numaMemInfo = globalNumaMemInfoNoBorrow;
    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    return VM_OK;
}

TEST_F(TestHamMigrate, Borrow_EnableProcessMigrate_failed)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_ERROR));
    mock_migrate_fail_without_rollback();
}

TEST_F(TestHamMigrate, Borrow_AddProcessTracking_failed)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_ERROR));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, Borrow_BorrowAddress_failed)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(returnValue(VM_ERROR));

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

    MOCKER(HamMigrate::EnterClearQueue, void(HamMigrateVmInfo & hamMigrateVmInfo, const bool &isUpdate))
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::BORROWED_NOMIGRATE, VmOpState::BORROWED_ADDRESS)));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, Borrow_httpSend_failed)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    mock_migrate_fail_without_rollback();
}

TEST_F(TestHamMigrate, Borrow_ExportQuery_failed)
{
    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(returnValue(VM_ERROR));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    mock_migrate_fail_without_rollback();
}

TEST_F(TestHamMigrate, Borrow_SyncData_failed)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HttpUtil::EnableProcessMigrate).expects(atLeast(1)).will(returnValue(VM_OK));
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));

    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfoStorage::GetHamMigrateVmInfos(NODE, hamMigrateVmInfos);
    EXPECT_TRUE(hamMigrateVmInfos.empty());
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, Borrow_success)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalOne));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(invoke(DoUbseBorrowAddress));

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    BorrowResponse borrowResponse;
    borrowResponse.name = "Node0/0/0";
    std::vector<int16_t> numaList;
    numaList.emplace_back(NUMAID_ONE);
    numaList.emplace_back(NUMAID_TWO);
    borrowResponse.numaIds = numaList;
    borrowResponse.scna = 1;

    freeResponse();
}

TEST_F(TestHamMigrate, Migrate_success_clear_success)
{
    rapidjson::Document msgJson;
    std::string clearJson = R"({"action": "clear", "type": 1, "srcHostname": "Node0", "srcPid": 123})";
    msgJson.Parse(clearJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoBorrow));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateClear(msgJson, respInfo, resp, context);

    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, Migrate_fail_returnMem_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(repeat(VM_ERROR, 1)).then(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

TEST_F(TestHamMigrate, Migrate_fail_AddProcessTracking_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(repeat(VM_ERROR, 1)).then(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

TEST_F(TestHamMigrate, Migrate_fail_EnableProcessMigrate_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(repeat(VM_ERROR, 1)).then(returnValue(VM_OK));
    mock_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

TEST_F(TestHamMigrate, Migrate_fail_clear_success)
{
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    mock_migrate_fail_clear_success(CLEAR_WAIT_TIME);
}

TEST_F(TestHamMigrate, NoBorrow_BorrowAddress_failed)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalZero));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(returnValue(VM_ERROR));
    MOCKER(HamMigrate::DoProcessMigrate).stubs().will(returnValue(VM_OK));

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

    MOCKER(HamMigrate::EnterClearQueue, void(HamMigrateVmInfo & hamMigrateVmInfo, const bool &isUpdate))
        .expects(atLeast(1))
        .with(checkWith(CheckVmInfo(VmState::NOBORROW_NOMIGRATE, VmOpState::BORROWED_ADDRESS)));

    std::thread ClearThread(&HamMigrate::ClearQueueOperation);
    ClearThread.detach();

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, NoBorrow_Borrow_success)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfosFromGlobalZero));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_ERROR));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::DoUbseBorrowAddress).stubs().will(invoke(DoUbseBorrowAddress));
    MOCKER(HamMigrate::DoProcessMigrate).stubs().will(returnValue(VM_OK));

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, NoBorrow_Migrate_success_clear_success)
{
    rapidjson::Document msgJson;
    std::string clearJson = R"({"action": "clear", "type": 1, "srcHostname": "Node0", "srcPid": 123})";
    msgJson.Parse(clearJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(invoke(GetHamMigrateVmInfoNoBorrow));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateClear(msgJson, respInfo, resp, context);

    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, NoBorrow_Migrate_fail_returnMem_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(repeat(VM_ERROR, 1)).then(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    mock_noborrow_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

TEST_F(TestHamMigrate, NoBorrow_Migrate_fail_RemoveProcessTracking_fail_retry_success)
{
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(repeat(VM_ERROR, 1)).then(returnValue(VM_OK));
    mock_noborrow_migrate_fail_clear_success(WAIT_TIME * CLEAR_WAIT_TIME);
}

TEST_F(TestHamMigrate, NoBorrow_Migrate_fail_clear_success)
{
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));
    mock_noborrow_migrate_fail_clear_success(CLEAR_WAIT_TIME);
}

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

TEST_F(TestHamMigrate, libvirt_restart_clear_success)
{
    rapidjson::Document msgJson;
    std::string clearJson = R"({"action": "clear", "type": 0, "srcHostname": "Node0"})";
    msgJson.Parse(clearJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfos).stubs().will(invoke(GetHamMigrateVmInfos));
    MOCKER(HamMigrateVmInfoStorage::GetAllHamMigrateVmInfos).stubs().will(invoke(GetAllHamMigrateVmInfos));
    MOCKER(HttpUtil::AddProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::RemoveProcessTracking).stubs().will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(HamMigrate::UbseRollbackBorrowAddress).stubs().will(returnValue(VM_OK));

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

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateClear(msgJson, respInfo, resp, context);

    freeResponse();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    HamMigrate::Stop();
}

TEST_F(TestHamMigrate, start_stop)
{
    // 模拟LibvirtHandler::Start成功，这里只测试HamMigrate::Run和Stop
    auto ret = HamMigrate::Run();
    EXPECT_EQ(ret, VM_OK);

    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfo.nodeId = NODE;
    hamMigrateVmInfo.pid = PID;
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    HamMigrate::EnterClearQueue(hamMigrateVmInfo);

    ret = HamMigrate::Stop();
    std::this_thread::sleep_for(milliseconds(CLEAR_WAIT_TIME));
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestHamMigrate, HasTask)
{
    rapidjson::Document msgJson;
    std::string borrowJson;
    ReadJsonFile("borrowInfo.json", borrowJson);
    msgJson.Parse(borrowJson.c_str());

    MOCKER(ubse::nodeController::UbseNodeGetNodeIdByHostname).stubs().will(invoke(UbseNodeGetNodeIdByHostname));
    MOCKER(HamMigrateVmInfoStorage::GetHamMigrateVmInfo).stubs().will(returnValue(VM_OK));

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;
    HamMigrate::HandleHamMigrateBorrow(msgJson, respInfo, resp, context);

    freeResponse();
}

TEST_F(TestHamMigrate, HamMigrateCancel)
{
    std::string uuid = "abcd-1234";
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
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
    MOCKER(com::UbseRpcSend).stubs().will(returnValue(VM_ERROR));
    MOCKER(UbseMemAddrCreate).stubs().will(returnValue(UBSE_ERR_AUTH_FAILED));
    EXPECT_EQ(HamMigrate::DoUbseBorrowAddress(borrowInfo, borrowResponse), VM_ERROR);
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

TEST_F(TestHamMigrate, SrcNodeInfoReplyHandler_Success)
{
    UbseMemAddrDesc result{};
    void *ctx = &result;
    ResponseInfoMessage response;
    response.SetResponseInfo(VM_OK, "1");
    response.Serialize();
    const UbseByteBuffer respData = {.data = response.SerializedData(), .len = response.SerializedDataSize()};

    HamMigrate::SrcNodeInfoReplyHandler(ctx, respData, VM_OK);
    EXPECT_EQ(result.numaId, 1);
}

TEST_F(TestHamMigrate, SrcNodeInfoReplyHandler_Failed)
{
    UbseMemAddrDesc result{};
    void *ctx = &result;
    const UbseByteBuffer respData = {.data = nullptr, .len = 0};

    HamMigrate::SrcNodeInfoReplyHandler(ctx, respData, VM_ERROR);
    EXPECT_EQ(result.numaId, -1);
}

TEST_F(TestHamMigrate, MasterDstInfoReplyHandler_Success)
{
    ResponseInfo result{};
    void *ctx = &result;
    ResponseInfoMessage response;
    response.SetResponseInfo(VM_OK, "Test");
    response.Serialize();
    const UbseByteBuffer respData = {.data = response.SerializedData(), .len = response.SerializedDataSize()};

    HamMigrate::MasterDstInfoReplyHandler(ctx, respData, VM_OK);
    EXPECT_EQ(result.code, VM_OK);
}

TEST_F(TestHamMigrate, MasterDstInfoReplyHandler_Failed)
{
    ResponseInfo result{};
    void *ctx = &result;
    const UbseByteBuffer respData = {.data = nullptr, .len = 0};

    HamMigrate::MasterDstInfoReplyHandler(ctx, respData, VM_ERROR);
    EXPECT_EQ(result.code, VM_ERROR);
}

TEST_F(TestHamMigrate, MasterDstInfoHandler_Request)
{
    HamMigrateDstInfoMessage hamMigrateDstInfoMessage;
    hamMigrateDstInfoMessage.Serialize();
    const UbseByteBuffer reqData = {.data = hamMigrateDstInfoMessage.SerializedData(),
                                    .len = hamMigrateDstInfoMessage.SerializedDataSize()};
    UbseByteBuffer resp;
    MOCKER(com::UbseRpcSend).stubs().will(returnValue(VM_OK));
    HamMigrate::MasterDstInfoHandler(reqData, resp);
    ResponseInfoMessage responseInfoSimpo(resp.data, resp.len);
    responseInfoSimpo.Deserialize();
    const auto [code, message] = responseInfoSimpo.GetResponseInfo();
    EXPECT_EQ(code, VM_OK);
}

TEST_F(TestHamMigrate, AgentDstInfoHandler_Request)
{
    HamMigrateDstInfoMessage hamMigrateDstInfoMessage;
    hamMigrateDstInfoMessage.Serialize();

    const UbseByteBuffer reqData = {.data = hamMigrateDstInfoMessage.SerializedData(),
                                    .len = hamMigrateDstInfoMessage.SerializedDataSize()};
    UbseByteBuffer resp;

    MOCKER(HamMigrate::PidIsVm).stubs().will(returnValue(VM_OK));
    MOCKER(UbseMemAddrCreate).stubs().will(returnValue(UBSE_OK));

    HamMigrate::AgentDstInfoHandler(reqData, resp);
}

TEST_F(TestHamMigrate, PidIsVm_NoHugePage)
{
    uint64_t pid = 456; // Assuming this pid won't have hugepages in test env
    EXPECT_EQ(HamMigrate::PidIsVm(pid), VM_ERROR);
}

} // namespace ubse::vm::ut