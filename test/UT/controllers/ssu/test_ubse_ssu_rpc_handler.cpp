/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * @note 本文件通过 forward-declaration 访问 ubse_ssu_rpc_processor.cpp 中 detail 命名空间的handler函数，
 *       无需包含该 .cpp 文件，避免与 ubse_ssu 库产生重复符号链接。
 */

#include "test_ubse_ssu_rpc_handler.h"

#include <memory>
#include "framework/misc/ubse_future_mgr.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_free_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "trace_context.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_ssu_utils.h"

// ====== 直接包含production源码，static保证内部链接不冲突 ======
#include "ubse_ssu_rpc_processor.cpp"

namespace ubse::ssu::controller::ut {

using namespace ubse::com;
using namespace ubse::misc::future;
using namespace ubse::ssu::message;
using namespace ubse::ssu::service;
using namespace ubse::task_executor;

// ====== Test fixture ======

void TestUbseSsuRpcHandler::SetUp()
{
    Test::SetUp();
}


void TestUbseSsuRpcHandler::TearDown()
{
    CleanupLedger();
    Test::TearDown();
    GlobalMockObject::verify();
}

void TestUbseSsuRpcHandler::PutLedgerEntry(const std::string &name, UbseSsuNsState state)
{
    auto entry = std::make_shared<const UbseSsuLedgerEntry>(UbseSsuLedgerEntry{name, {}, {}, state, {}});
    UbseSsuDebtLedger::GetInstance().Put(name, entry);
}

void TestUbseSsuRpcHandler::CleanupLedger()
{
    auto allEntries = UbseSsuDebtLedger::GetInstance().GetAll();
    for (const auto &entry : allEntries) {
        UbseSsuDebtLedger::GetInstance().Remove(entry->name);
    }
}

// ========================================================================
// 工具结构体：将UbseRpcMessage序列化为 (data, size) 供handler消费
// ========================================================================
struct SerializedData {
    std::unique_ptr<uint8_t[]> data;
    uint32_t size{0};
    explicit SerializedData(const UbseRpcMessage &msg)
    {
        msg.Serialize(data, size);
    }
};

// ========================================================================
// 1. IsStatusTransitionValid 状态转换合法性测试
// ========================================================================

TEST_F(TestUbseSsuRpcHandler, StatusTransition_CreatingToCreated)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::CREATING, UbseSsuNsState::CREATED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_CreatedToAttaching)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::CREATED, UbseSsuNsState::ATTACHING));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_CreatedToAttached)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::CREATED, UbseSsuNsState::ATTACHED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_AttachingToAttached)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::ATTACHING, UbseSsuNsState::ATTACHED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_AttachingToCreated)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::ATTACHING, UbseSsuNsState::CREATED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_AttachedToCreated)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::ATTACHED, UbseSsuNsState::CREATED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_Invalid_CreatingToAttached)
{
    EXPECT_FALSE(IsStatusTransitionValid(UbseSsuNsState::CREATING, UbseSsuNsState::ATTACHED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_Invalid_IdleToCreated)
{
    EXPECT_FALSE(IsStatusTransitionValid(UbseSsuNsState::IDLE, UbseSsuNsState::CREATED));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_Invalid_AttachedToAttaching)
{
    EXPECT_FALSE(IsStatusTransitionValid(UbseSsuNsState::ATTACHED, UbseSsuNsState::ATTACHING));
}

TEST_F(TestUbseSsuRpcHandler, StatusTransition_Invalid_CreatingToIdle)
{
    EXPECT_FALSE(IsStatusTransitionValid(UbseSsuNsState::CREATING, UbseSsuNsState::IDLE));
}

// ========================================================================
// 2. HandleStatusReceiver — master端处理状态更新
// ========================================================================

// 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00, 0x01, 0x02};
    HandleStatusReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// 账本中无对应entry → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_EntryNotFound)
{
    UbseSsuStatusReqMsg reqMsg("non_existent_name", UbseSsuNsState::CREATED);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// 合法转换：CREATING → CREATED
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_TransitionCreatingToCreated)
{
    PutLedgerEntry("test_ns", UbseSsuNsState::CREATING);
    UbseSsuStatusReqMsg reqMsg("test_ns", UbseSsuNsState::CREATED);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_ns");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// 合法转换：CREATED → ATTACHING
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_TransitionCreatedToAttaching)
{
    PutLedgerEntry("test_ns", UbseSsuNsState::CREATED);
    UbseSsuStatusReqMsg reqMsg("test_ns", UbseSsuNsState::ATTACHING);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_ns");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHING);
}

// 合法转换：ATTACHING → ATTACHED
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_TransitionAttachingToAttached)
{
    PutLedgerEntry("test_ns", UbseSsuNsState::ATTACHING);
    UbseSsuStatusReqMsg reqMsg("test_ns", UbseSsuNsState::ATTACHED);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_ns");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
}

// 合法转换：ATTACHING → CREATED（回退）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_TransitionAttachingToCreated)
{
    PutLedgerEntry("test_ns", UbseSsuNsState::ATTACHING);
    UbseSsuStatusReqMsg reqMsg("test_ns", UbseSsuNsState::CREATED);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_ns");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// 合法转换：ATTACHED → CREATED（detach）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_TransitionAttachedToCreated)
{
    PutLedgerEntry("test_ns", UbseSsuNsState::ATTACHED);
    UbseSsuStatusReqMsg reqMsg("test_ns", UbseSsuNsState::CREATED);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_ns");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// 非法转换：CREATED → CREATING（不允许回退到CREATING）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_InvalidTransitionCreatedToCreating)
{
    PutLedgerEntry("test_ns", UbseSsuNsState::CREATED);
    UbseSsuStatusReqMsg reqMsg("test_ns", UbseSsuNsState::CREATING);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_STATE_INVALID);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_ns");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
}

// ========================================================================
// 3. HandleAllocRespReceiver — agent端处理分配响应
// ========================================================================

// 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleAllocRespReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00, 0x01};
    HandleAllocRespReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// 正常接收分配响应 → 验证sync resp返回OK
// 注：未mock UbseFutureMgr::SetResult，handler内SetResult失败仅打日志，不影响resp
TEST_F(TestUbseSsuRpcHandler, HandleAllocRespReceiver_Success)
{
    UbseSsuAllocResp respData;
    respData.requestId = "test_request_id";
    respData.errorCode = UBSE_OK;
    respData.state = UbseSsuNsState::CREATED;
    UbseSsuAllocRespMsg respMsg(respData);
    SerializedData sd(respMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAllocRespReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
}

// ========================================================================
// 4. HandleFreeRespReceiver — agent端处理释放响应
// ========================================================================

// 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleFreeRespReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleFreeRespReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// 正常接收释放响应
TEST_F(TestUbseSsuRpcHandler, HandleFreeRespReceiver_Success)
{
    UbseSsuFreeResp respData;
    respData.requestId = "test_free_id";
    respData.errorCode = UBSE_OK;
    UbseSsuFreeRespMsg respMsg(respData);
    SerializedData sd(respMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleFreeRespReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
}

// ========================================================================
// 5. HandlePermRespReceiver — agent端处理权限响应
// ========================================================================

// 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandlePermRespReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0xFF};
    HandlePermRespReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// 正常接收权限响应
TEST_F(TestUbseSsuRpcHandler, HandlePermRespReceiver_Success)
{
    UbseSsuPermResp respData;
    respData.requestId = "test_perm_id";
    respData.errorCode = UBSE_OK;
    UbseSsuPermRespMsg respMsg(respData);
    SerializedData sd(respMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandlePermRespReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
}

// ========================================================================
// 6. HandleAttachDetachVerifyReqReceiver — master端处理验证请求（sync resp）
// ========================================================================

// 反序列化失败 → 返回error
TEST_F(TestUbseSsuRpcHandler, HandleAttachDetachVerifyReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleAttachDetachVerifyReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto verifyResp = dynamic_cast<UbseSsuAttachDetachVerifyRespMsg *>(resp.get());
    ASSERT_NE(verifyResp, nullptr);
    EXPECT_EQ(verifyResp->GetAttachDetachVerifyResp().errorCode, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// ========================================================================
// 7. 查询类Handler测试（sync resp）
// ========================================================================

// HandleGetNsStatsReqReceiver: 反序列化失败
TEST_F(TestUbseSsuRpcHandler, HandleGetNsStatsReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleGetNsStatsReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto nsStatsResp = dynamic_cast<UbseSsuGetNsStatsRespMsg *>(resp.get());
    ASSERT_NE(nsStatsResp, nullptr);
    EXPECT_EQ(nsStatsResp->GetGetNsStatsResp().errorCode, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// HandleGetNsStatsReqReceiver: 正常查询（验证MOCKER_CPP_VIRTUAL拦截成功）
TEST_F(TestUbseSsuRpcHandler, HandleGetNsStatsReqReceiver_Success)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::GetNsStats)
        .stubs().will(returnValue(UBSE_OK));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuGetNsStatsReqMsg reqMsg("req_ns_stats", "agent_node_1", "test_name", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleGetNsStatsReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto nsStatsResp = dynamic_cast<UbseSsuGetNsStatsRespMsg *>(resp.get());
    ASSERT_NE(nsStatsResp, nullptr);
    EXPECT_EQ(nsStatsResp->GetGetNsStatsResp().errorCode, UBSE_OK);
}

// 验证成功 → 返回带nsVerifyList和nameSpaceList的响应
TEST_F(TestUbseSsuRpcHandler, HandleAttachDetachVerifyReqReceiver_Success)
{
    MOCKER(&UbseSsuServiceImp::VerifyAttachDetachPrecondition)
        .stubs()
        .will(returnValue(UBSE_OK));

    PutLedgerEntry("test_verify", UbseSsuNsState::CREATED);

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuAttachDetachVerifyReqMsg reqMsg("req_001", "agent_node_1", "test_verify", identity, {});
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAttachDetachVerifyReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto verifyResp = dynamic_cast<UbseSsuAttachDetachVerifyRespMsg *>(resp.get());
    ASSERT_NE(verifyResp, nullptr);
    EXPECT_EQ(verifyResp->GetAttachDetachVerifyResp().errorCode, UBSE_OK);
}

// HandleListAllocInfoReqReceiver: 反序列化失败
TEST_F(TestUbseSsuRpcHandler, HandleListAllocInfoReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleListAllocInfoReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto listResp = dynamic_cast<UbseSsuListAllocInfoRespMsg *>(resp.get());
    ASSERT_NE(listResp, nullptr);
    EXPECT_EQ(listResp->GetListAllocInfoResp().errorCode, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// HandleListAllocInfoReqReceiver: 正常查询（验证MOCKER_CPP_VIRTUAL拦截成功）
TEST_F(TestUbseSsuRpcHandler, HandleListAllocInfoReqReceiver_Success)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::ListAllocInfo)
        .stubs().will(returnValue(UBSE_OK));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuListAllocInfoReqMsg reqMsg("req_list", "agent_node_1", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleListAllocInfoReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto listResp = dynamic_cast<UbseSsuListAllocInfoRespMsg *>(resp.get());
    ASSERT_NE(listResp, nullptr);
    EXPECT_EQ(listResp->GetListAllocInfoResp().errorCode, UBSE_OK);
}

// HandleGetAllocInfoReqReceiver: 反序列化失败
TEST_F(TestUbseSsuRpcHandler, HandleGetAllocInfoReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleGetAllocInfoReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto getResp = dynamic_cast<UbseSsuGetAllocInfoRespMsg *>(resp.get());
    ASSERT_NE(getResp, nullptr);
    EXPECT_EQ(getResp->GetGetAllocInfoResp().errorCode, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// HandleGetAllocInfoReqReceiver: 正常查询（验证MOCKER_CPP_VIRTUAL拦截成功）
TEST_F(TestUbseSsuRpcHandler, HandleGetAllocInfoReqReceiver_Success)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::GetAllocInfoByName)
        .stubs().will(returnValue(UBSE_OK));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuGetAllocInfoReqMsg reqMsg("req_get", "agent_node_1", "test_name", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleGetAllocInfoReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto getResp = dynamic_cast<UbseSsuGetAllocInfoRespMsg *>(resp.get());
    ASSERT_NE(getResp, nullptr);
    EXPECT_EQ(getResp->GetGetAllocInfoResp().errorCode, UBSE_OK);
}

// HandleGetConnectInfoReqReceiver: 反序列化失败
TEST_F(TestUbseSsuRpcHandler, HandleGetConnectInfoReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleGetConnectInfoReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto connResp = dynamic_cast<UbseSsuGetConnectInfoRespMsg *>(resp.get());
    ASSERT_NE(connResp, nullptr);
    EXPECT_EQ(connResp->GetGetConnectInfoResp().errorCode, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// HandleGetConnectInfoReqReceiver: 正常查询（验证MOCKER_CPP_VIRTUAL拦截成功）
TEST_F(TestUbseSsuRpcHandler, HandleGetConnectInfoReqReceiver_Success)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::GetConnectInfo)
        .stubs().will(returnValue(UBSE_OK));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuGetConnectInfoReqMsg reqMsg("req_conn", "agent_node_1", "test_name", identity, nullptr);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleGetConnectInfoReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto connResp = dynamic_cast<UbseSsuGetConnectInfoRespMsg *>(resp.get());
    ASSERT_NE(connResp, nullptr);
    EXPECT_EQ(connResp->GetGetConnectInfoResp().errorCode, UBSE_OK);
}

// HandleGetConnectInfoReqReceiver: 查询失败分支
TEST_F(TestUbseSsuRpcHandler, HandleGetConnectInfoReqReceiver_Fail)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::GetConnectInfo)
        .stubs().will(returnValue(UBSE_ERROR));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuGetConnectInfoReqMsg reqMsg("req_conn_fail", "agent_node_1", "test_name", identity, nullptr);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleGetConnectInfoReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto connResp = dynamic_cast<UbseSsuGetConnectInfoRespMsg *>(resp.get());
    ASSERT_NE(connResp, nullptr);
    EXPECT_EQ(connResp->GetGetConnectInfoResp().errorCode, UBSE_ERROR);
}

// ========================================================================
// 8. HandleAllocReqReceiver — master端处理分配请求（executor异步）
// ========================================================================

// 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleAllocReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00, 0x01, 0x02};
    HandleAllocReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// 反序列化成功但executor为null → 分配失败，返回UBSE_ERROR
// executor默认为null（UT环境未初始化UbseTaskExecutorModule）
TEST_F(TestUbseSsuRpcHandler, HandleAllocReqReceiver_ExecutorNull)
{
    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuAllocSpaceReq allocReq;
    allocReq.name = "test_alloc_ns";
    allocReq.nsSize = 4096;
    allocReq.nsNum = 1;
    allocReq.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    allocReq.strategy = UbseSsuAllocStrategy::LINEAR;
    allocReq.tenant = "test_tenant";
    UbseSsuAllocReqMsg reqMsg("req_alloc_001", "agent_node_1", identity, allocReq);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAllocReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_EXECUTOR_NULL);
}

// ========================================================================
// 9. HandleFreeReqReceiver — master端处理释放请求
// ========================================================================

// 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleFreeReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleFreeReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// 反序列化成功但executor为null → 释放失败，返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleFreeReqReceiver_ExecutorNull)
{
    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuFreeReqMsg reqMsg("req_free_001", "agent_node_1", "test_free_ns", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleFreeReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_EXECUTOR_NULL);
}

// ========================================================================
// 10. HandleAddPermReqReceiver / HandleRemovePermReqReceiver — 权限请求
// ========================================================================

// HandleAddPermReqReceiver: 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleAddPermReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleAddPermReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// HandleAddPermReqReceiver: executor为null → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleAddPermReqReceiver_ExecutorNull)
{
    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuPermReqMsg reqMsg("req_add_perm_001", "agent_node_1", "test_perm_ns", "nqn.test", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAddPermReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_EXECUTOR_NULL);
}

// HandleRemovePermReqReceiver: 反序列化失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleRemovePermReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleRemovePermReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// HandleRemovePermReqReceiver: executor为null → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcHandler, HandleRemovePermReqReceiver_ExecutorNull)
{
    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuPermReqMsg reqMsg("req_rm_perm_001", "agent_node_1", "test_perm_ns", "nqn.test", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleRemovePermReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_EXECUTOR_NULL);
}

// ========================================================================
// 11. 补充已有Handler的错误分支测试（SetResult失败、Verify失败等）
// ========================================================================

// HandleAllocRespReceiver: SetResult找不到requestId
TEST_F(TestUbseSsuRpcHandler, HandleAllocRespReceiver_SetResultFail)
{
    UbseSsuAllocResp respData;
    respData.requestId = "non_existent_request_id_for_alloc";
    respData.errorCode = UBSE_OK;
    respData.state = UbseSsuNsState::CREATED;
    UbseSsuAllocRespMsg respMsg(respData);
    SerializedData sd(respMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAllocRespReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
}

// HandleFreeRespReceiver: SetResult找不到requestId
TEST_F(TestUbseSsuRpcHandler, HandleFreeRespReceiver_SetResultFail)
{
    UbseSsuFreeResp respData;
    respData.requestId = "non_existent_request_id_for_free";
    respData.errorCode = UBSE_OK;
    UbseSsuFreeRespMsg respMsg(respData);
    SerializedData sd(respMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleFreeRespReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
}

// HandlePermRespReceiver: SetResult找不到requestId
TEST_F(TestUbseSsuRpcHandler, HandlePermRespReceiver_SetResultFail)
{
    UbseSsuPermResp respData;
    respData.requestId = "non_existent_request_id_for_perm";
    respData.errorCode = UBSE_OK;
    UbseSsuPermRespMsg respMsg(respData);
    SerializedData sd(respMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandlePermRespReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
}

// HandleAttachDetachVerifyReqReceiver: Verify成功但entryPtr为null（账本中无对应entry）
TEST_F(TestUbseSsuRpcHandler, HandleAttachDetachVerifyReqReceiver_VerifySuccessEntryNotFound)
{
    MOCKER(&UbseSsuServiceImp::VerifyAttachDetachPrecondition)
        .stubs()
        .will(returnValue(UBSE_OK));

    // 不放入账本，验证entryPtr == nullptr分支
    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuAttachDetachVerifyReqMsg reqMsg("req_002", "agent_node_1", "non_existent_name", identity, {});
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAttachDetachVerifyReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto verifyResp = dynamic_cast<UbseSsuAttachDetachVerifyRespMsg *>(resp.get());
    ASSERT_NE(verifyResp, nullptr);
    EXPECT_EQ(verifyResp->GetAttachDetachVerifyResp().errorCode, UBSE_OK);
}

// HandleAttachDetachVerifyReqReceiver: Verify失败
TEST_F(TestUbseSsuRpcHandler, HandleAttachDetachVerifyReqReceiver_VerifyFail)
{
    MOCKER(&UbseSsuServiceImp::VerifyAttachDetachPrecondition)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    PutLedgerEntry("test_verify_fail", UbseSsuNsState::CREATED);

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuAttachDetachVerifyReqMsg reqMsg("req_003", "agent_node_1", "test_verify_fail", identity, {});
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleAttachDetachVerifyReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto verifyResp = dynamic_cast<UbseSsuAttachDetachVerifyRespMsg *>(resp.get());
    ASSERT_NE(verifyResp, nullptr);
    EXPECT_EQ(verifyResp->GetAttachDetachVerifyResp().errorCode, UBSE_ERROR);
}

// HandleGetNsStatsReqReceiver: 查询失败分支
TEST_F(TestUbseSsuRpcHandler, HandleGetNsStatsReqReceiver_Fail)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::GetNsStats)
        .stubs().will(returnValue(UBSE_ERROR));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuGetNsStatsReqMsg reqMsg("req_ns_stats_fail", "agent_node_1", "test_name", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleGetNsStatsReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto nsStatsResp = dynamic_cast<UbseSsuGetNsStatsRespMsg *>(resp.get());
    ASSERT_NE(nsStatsResp, nullptr);
    EXPECT_EQ(nsStatsResp->GetGetNsStatsResp().errorCode, UBSE_ERROR);
}

// HandleListAllocInfoReqReceiver: 查询失败分支
TEST_F(TestUbseSsuRpcHandler, HandleListAllocInfoReqReceiver_Fail)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::ListAllocInfo)
        .stubs().will(returnValue(UBSE_ERROR));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuListAllocInfoReqMsg reqMsg("req_list_fail", "agent_node_1", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleListAllocInfoReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto listResp = dynamic_cast<UbseSsuListAllocInfoRespMsg *>(resp.get());
    ASSERT_NE(listResp, nullptr);
    EXPECT_EQ(listResp->GetListAllocInfoResp().errorCode, UBSE_ERROR);
}

// HandleGetAllocInfoReqReceiver: 查询失败分支
TEST_F(TestUbseSsuRpcHandler, HandleGetAllocInfoReqReceiver_Fail)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::GetAllocInfoByName)
        .stubs().will(returnValue(UBSE_ERROR));

    UbseSsuAllocIdentityInfo identity;
    identity.uid = 100;
    identity.userName = "test_user";
    UbseSsuGetAllocInfoReqMsg reqMsg("req_get_fail", "agent_node_1", "test_name", identity);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleGetAllocInfoReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto getResp = dynamic_cast<UbseSsuGetAllocInfoRespMsg *>(resp.get());
    ASSERT_NE(getResp, nullptr);
    EXPECT_EQ(getResp->GetGetAllocInfoResp().errorCode, UBSE_ERROR);
}

} // namespace ubse::ssu::controller::ut
