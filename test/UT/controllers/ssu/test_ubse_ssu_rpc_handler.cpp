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

#include <fcntl.h>
#include <cstdlib>
#include <memory>
#include <optional>
#include <unistd.h>
#include "framework/misc/ubse_future_mgr.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_free_msg.h"
#include "message/ubse_ssu_mount_reconcile_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "test_ubse_ssu_service_imp_fixture.h"
#include "trace_context.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ssu_attach_record_store.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_ssu_utils.h"
#include "ubse_thread_pool_module.h"

// ====== 直接包含production源码，static保证内部链接不冲突 ======
#include "ubse_ssu_rpc_processor.cpp"

namespace ubse::ssu::controller::ut {

using namespace ubse::com;
using namespace ubse::misc::future;
using namespace ubse::ssu::message;
using namespace ubse::ssu::service;
using namespace ubse::task_executor;
using namespace ubse::adapter_plugins::ssu::def;

namespace {
// 本套件挂载记录临时文件序号（reconcile handler测试操作store；per-test独立文件隔离，
// 防止单例残留filePath_跨套件/跨运行串扰，同service_imp fixture模式）
uint32_t g_rpcRecordFileSeq = 0;
} // namespace

// ====== Test fixture ======

void TestUbseSsuRpcHandler::SetUp()
{
    Test::SetUp();
    // 账本就绪门禁默认置位：HandleStatusReceiver在ledgerReady_为假时返回MASTER_NOT_READY，
    // 既有测试均假设账本就绪（等价于Rebuild完成后的事件序）；未就绪场景由
    // HandleStatusReceiver_LedgerNotReady单测显式置假覆盖
    UbseSsuDebtLedger::GetInstance().SetReady(true);
    // 挂载记录store重定向到本测试独立临时文件并清空内存记录（Init重读env覆盖旧filePath_）
    const std::string recordFile = "/tmp/ubse_ssut_rpc_records_" + std::to_string(++g_rpcRecordFileSeq) + ".json";
    setenv("UBSE_SSU_ATTACH_RECORD_FILE", recordFile.c_str(), 1);
    ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().Init();
}


void TestUbseSsuRpcHandler::TearDown()
{
    UbseSsuDebtLedger::GetInstance().SetReady(false);
    CleanupLedger();
    // 清理本测试的挂载记录临时文件并复位env，杜绝后续测试经单例残留filePath_续写
    if (const char *envPath = getenv("UBSE_SSU_ATTACH_RECORD_FILE"); envPath != nullptr) {
        unlink(envPath);
        unlink((std::string(envPath) + ".tmp").c_str());
        unsetenv("UBSE_SSU_ATTACH_RECORD_FILE");
    }
    Test::TearDown();
    GlobalMockObject::verify();
}

void TestUbseSsuRpcHandler::PutLedgerEntry(const std::string &name, UbseSsuNsState state)
{
    auto entry = std::make_shared<const UbseSsuLedgerEntry>(UbseSsuLedgerEntry{name, {}, {}, state, {}});
    UbseSsuDebtLedger::GetInstance().Put(name, entry);
}

void TestUbseSsuRpcHandler::PutLedgerEntry(const std::string &name, UbseSsuNsState state, UbseSsuAllocStrategy strategy,
                                           const std::unordered_set<std::string> &attachNodes)
{
    UbseSsuLedgerEntry entry{};
    entry.name = name;
    entry.allocReq.strategy = strategy;
    entry.state = state;
    entry.attachNodes = attachNodes;
    UbseSsuDebtLedger::GetInstance().Put(name, std::make_shared<const UbseSsuLedgerEntry>(entry));
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

// ====== 同状态幂等（重启恢复重报场景，reliable_design.md §5.7）======

// ATTACHED → ATTACHED：恢复期agent重报，幂等消化
TEST_F(TestUbseSsuRpcHandler, StatusTransition_SameStateAttached)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::ATTACHED, UbseSsuNsState::ATTACHED));
}

// CREATED → CREATED：恢复期agent重报（DetachDevNameSpace幂等重试后），幂等消化
TEST_F(TestUbseSsuRpcHandler, StatusTransition_SameStateCreated)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::CREATED, UbseSsuNsState::CREATED));
}

// CREATING → CREATING 同状态同样幂等（对账消化不拒收）
TEST_F(TestUbseSsuRpcHandler, StatusTransition_SameStateCreating)
{
    EXPECT_TRUE(IsStatusTransitionValid(UbseSsuNsState::CREATING, UbseSsuNsState::CREATING));
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
// 2.1 HandleStatusReceiver — NORMAL策略多节点对账
// ========================================================================

// NORMAL：节点A attach成功上报 → attachNodes记录A，空间state=ATTACHED
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalAttachNodeA)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::CREATED, UbseSsuAllocStrategy::NORMAL);
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::ATTACHED, "", "node_a");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
    EXPECT_NE(entry->attachNodes.find("node_a"), entry->attachNodes.end());
}

// NORMAL：节点B在节点A挂载时也attach → 两节点共存，空间保持ATTACHED
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalMultiNodeAttach)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::ATTACHED, UbseSsuAllocStrategy::NORMAL, {"node_a"});
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::ATTACHED, "", "node_b");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(entry->attachNodes.size(), 2u);
    EXPECT_NE(entry->attachNodes.find("node_a"), entry->attachNodes.end());
    EXPECT_NE(entry->attachNodes.find("node_b"), entry->attachNodes.end());
}

// NORMAL：节点A detach但节点B仍挂载 → 移除A，空间保持ATTACHED
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalDetachOneOfTwo)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::ATTACHED, UbseSsuAllocStrategy::NORMAL, {"node_a", "node_b"});
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::CREATED, "", "node_a");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::ATTACHED); // B仍挂载，空间保持ATTACHED
    EXPECT_EQ(entry->attachNodes.size(), 1u);
    EXPECT_EQ(entry->attachNodes.count("node_a"), 0u);
    EXPECT_NE(entry->attachNodes.find("node_b"), entry->attachNodes.end());
}

// NORMAL：最后一个节点detach → 空间回退CREATED
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalDetachLastNode)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::ATTACHED, UbseSsuAllocStrategy::NORMAL, {"node_b"});
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::CREATED, "", "node_b");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_TRUE(entry->attachNodes.empty());
}

// NORMAL：节点不在attachNodes中（Rebuild后map为空）的CREATED上报仍消化，不拒绝
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalDetachNodeNotInMap)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::ATTACHED, UbseSsuAllocStrategy::NORMAL, {});
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::CREATED, "", "node_unknown");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_TRUE(entry->attachNodes.empty());
}

// NORMAL：nodeId缺失 → 拒收（对账主键不可为空）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalEmptyNodeIdRejected)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::CREATED, UbseSsuAllocStrategy::NORMAL);
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::ATTACHED);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_STATE_INVALID);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_TRUE(entry->attachNodes.empty());
}

// NORMAL：CREATING状态拒收attach上报（分配仍在途）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_NormalCreatingRejected)
{
    PutLedgerEntry("test_normal", UbseSsuNsState::CREATING, UbseSsuAllocStrategy::NORMAL);
    UbseSsuStatusReqMsg reqMsg("test_normal", UbseSsuNsState::ATTACHED, "", "node_a");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_STATE_INVALID);
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_normal");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATING);
    EXPECT_TRUE(entry->attachNodes.empty());
}

// ====== 重启恢复扩展（reliable_design.md §5.5/§5.7）======

// 账本未就绪（master首次采集/重建未完成）→ 返回MASTER_NOT_READY而非SPACE_NOT_FOUND，
// 杜绝agent误判space已free而错误自愈清理本地挂载
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_LedgerNotReady)
{
    UbseSsuDebtLedger::GetInstance().SetReady(false);
    PutLedgerEntry("test_not_ready", UbseSsuNsState::CREATED, UbseSsuAllocStrategy::LINEAR);
    UbseSsuStatusReqMsg reqMsg("test_not_ready", UbseSsuNsState::ATTACHED, "", "node_a");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_MASTER_NOT_READY);
    // 账本状态不被修改
    auto entry = UbseSsuDebtLedger::GetInstance().Get("test_not_ready");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_TRUE(entry->attachNodes.empty());
}

// 账本就绪后账本内无该空间 → 仍返回SPACE_NOT_FOUND（space确已被free，与NOT_READY区分）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_LedgerReadyEntryNotFound)
{
    PutLedgerEntry("test_gone", UbseSsuNsState::CREATED, UbseSsuAllocStrategy::LINEAR);
    UbseSsuStatusReqMsg reqMsg("non_existent_after_ready", UbseSsuNsState::ATTACHED, "", "node_a");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// LINEAR空间ATTACHED状态收到ATTACHED重报（恢复期agent重报）→ 幂等消化，
// attachNodes/devName保持不变
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_AttachedIdempotentRereport)
{
    UbseSsuDebtLedger::GetInstance().SetReady(true);
    UbseSsuLedgerEntry entry{};
    entry.name = "test_linear_rereport";
    entry.allocReq.strategy = UbseSsuAllocStrategy::LINEAR;
    entry.state = UbseSsuNsState::ATTACHED;
    entry.attachNodes = {"node_attacher"};
    entry.devName = "linear_dev_1";
    UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry));

    UbseSsuStatusReqMsg reqMsg("test_linear_rereport", UbseSsuNsState::ATTACHED, "linear_dev_1", "node_attacher");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_OK);
    auto updated = UbseSsuDebtLedger::GetInstance().Get("test_linear_rereport");
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->state, UbseSsuNsState::ATTACHED);
    EXPECT_EQ(updated->attachNodes.size(), 1u);
    EXPECT_NE(updated->attachNodes.find("node_attacher"), updated->attachNodes.end());
    EXPECT_EQ(updated->devName, "linear_dev_1");
}

// LINEAR空间ATTACHED状态收到其他节点的ATTACHED上报（恢复窗口双重挂载污染）→ 拒收
// ATTACHER_MISMATCH，维持attacher单节点不变式（reliable_design.md §14.2-3）
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_AttacherMismatchReportRejected)
{
    UbseSsuDebtLedger::GetInstance().SetReady(true);
    UbseSsuLedgerEntry entry{};
    entry.name = "test_linear_mismatch";
    entry.allocReq.strategy = UbseSsuAllocStrategy::LINEAR;
    entry.state = UbseSsuNsState::ATTACHED;
    entry.attachNodes = {"node_attacher"};
    entry.devName = "linear_dev_1";
    UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry));

    UbseSsuStatusReqMsg reqMsg("test_linear_mismatch", UbseSsuNsState::ATTACHED, "linear_dev_1", "node_other");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_ATTACHER_MISMATCH);
    // attachNodes不被污染：仍仅含原attacher
    auto updated = UbseSsuDebtLedger::GetInstance().Get("test_linear_mismatch");
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->attachNodes.size(), 1u);
    EXPECT_NE(updated->attachNodes.find("node_attacher"), updated->attachNodes.end());
    EXPECT_EQ(updated->attachNodes.find("node_other"), updated->attachNodes.end());
}

// FAULTED空间的状态上报一律拒收（reliable_design.md §12.2）：返回SPACE_FAULTED，
// 防止故障期间上报覆盖FAULTED状态掩盖告警；设备恢复由collector对账回正后重报收敛
TEST_F(TestUbseSsuRpcHandler, HandleStatusReceiver_FaultedSpaceReportRejected)
{
    UbseSsuDebtLedger::GetInstance().SetReady(true);
    UbseSsuLedgerEntry entry{};
    entry.name = "test_space_faulted";
    entry.allocReq.strategy = UbseSsuAllocStrategy::LINEAR;
    entry.state = UbseSsuNsState::FAULTED;
    entry.attachNodes = {"node_attacher"};
    UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry));

    UbseSsuStatusReqMsg reqMsg("test_space_faulted", UbseSsuNsState::CREATED, "", "node_attacher");
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleStatusReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto syncResp = dynamic_cast<UbseSsuSyncRespMsg *>(resp.get());
    ASSERT_NE(syncResp, nullptr);
    EXPECT_EQ(syncResp->GetErrorCode(), UBSE_SSU_ERROR_SPACE_FAULTED);
    // FAULTED状态与attachNodes均不被上报篡改
    auto updated = UbseSsuDebtLedger::GetInstance().Get("test_space_faulted");
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->state, UbseSsuNsState::FAULTED);
    EXPECT_EQ(updated->attachNodes.size(), 1u);
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
    UbseSsuFreeReqMsg reqMsg("req_free_001", "agent_node_1", "test_free_ns", "", identity);
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

// ====== v1.1 §13 运营期持续对账平账 handler（reliable_design.md §13.1/§13.3，UT24） ======

namespace {

// 平账handler测试用挂载记录：nsList为空（探测语义上ProbeLocalMountReady恒命中，
// 无需真实设备文件），可选聚合块设备选项（携带devName）
UbseSsuAttachRecord MakeReconcileRecord(const std::string &name, UbseSsuAttachRecordState state,
                                        bool withBlockDevice = false)
{
    UbseSsuAttachRecord record{};
    record.name = name;
    record.strategy = withBlockDevice ? UbseSsuAllocStrategy::LINEAR : UbseSsuAllocStrategy::NORMAL;
    record.recordState = state;
    record.devName = withBlockDevice ? ("agg_" + name) : "";
    record.nqn = "";
    record.identity.uid = 100;
    record.identity.userName = "test_user";
    if (withBlockDevice) {
        record.blockDeviceOptions = UbseCreateBlockDeviceOptions{};
        record.blockDeviceOptions->addressingType = UbseSsuAddressingType::LINEAR;
        record.blockDeviceOptions->raidLevel = UbseSsuRaidLevel::RAID0;
        record.blockDeviceOptions->chunkSize = 512;
    }
    record.attachedAt = 1700000000;
    return record;
}

UbseSsuReconcileItem MakeReconcileItem(const std::string &name, UbseSsuReconcileAction action,
                                       bool hasRecord = false)
{
    UbseSsuReconcileItem item;
    item.name = name;
    item.action = action;
    item.hasRecord = hasRecord;
    return item;
}

// 从store按名查记录
std::optional<UbseSsuAttachRecord> FindRecord(const std::string &name)
{
    for (const auto &record : ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().GetAll()) {
        if (record.name == name) {
            return record;
        }
    }
    return std::nullopt;
}

} // namespace

// HandleMountSummaryReqReceiver：返回本机AttachRecordStore全量摘要
// （{name, recordState, devName}，不含nsList明细）
TEST_F(TestUbseSsuRpcHandler, HandleMountSummaryReqReceiver_ReturnsStoreSummary)
{
    auto &store = ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance();
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_sum_attached", UbseSsuAttachRecordState::ATTACHED, true)),
              UBSE_OK);
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_sum_pending", UbseSsuAttachRecordState::PENDING)), UBSE_OK);

    UbseSsuMountSummaryReqMsg reqMsg;
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleMountSummaryReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto summaryResp = dynamic_cast<UbseSsuMountSummaryRespMsg *>(resp.get());
    ASSERT_NE(summaryResp, nullptr);
    const auto &data = summaryResp->GetMountSummaryResp();
    EXPECT_EQ(data.errorCode, UBSE_OK);
    ASSERT_EQ(data.items.size(), 2U);
    // GetAll为无序map，按name查找逐项校验
    const UbseSsuMountSummaryItem *attachedItem = nullptr;
    const UbseSsuMountSummaryItem *pendingItem = nullptr;
    for (const auto &item : data.items) {
        if (item.name == "sp_sum_attached") {
            attachedItem = &item;
        } else if (item.name == "sp_sum_pending") {
            pendingItem = &item;
        }
    }
    ASSERT_NE(attachedItem, nullptr);
    EXPECT_EQ(attachedItem->recordState, UbseSsuAttachRecordState::ATTACHED);
    EXPECT_EQ(attachedItem->devName, "agg_sp_sum_attached");
    ASSERT_NE(pendingItem, nullptr);
    EXPECT_EQ(pendingItem->recordState, UbseSsuAttachRecordState::PENDING);
    EXPECT_TRUE(pendingItem->devName.empty());
}

// ====== reconcile handler受理即回改造的executor mock基础设施 ======
// UT环境默认未初始化UbseTaskExecutorModule（GetSsuExecutor返回null，executor-null用例
// 直接依赖此默认态）。受理/单飞用例经mock提供真实module+executor对象，Execute捕获
// 任务不实际入队（executor未Start）；测试同步回放捕获任务，确定性地验证异步执行路径
namespace {

std::vector<std::function<void()>> g_capturedReconcileTasks;

// mock UbseContext::GetModule<UbseTaskExecutorModule>：返回带ubseSsuController executor的真实module

std::shared_ptr<task_executor::UbseTaskExecutorModule> MockGetTaskExecutorModule(ubse::context::UbseContext *self)
{
    (void)self;
    static auto module = std::make_shared<task_executor::UbseTaskExecutorModule>();
    static bool executorCreated = false;
    if (!executorCreated) {
        // 真实executor（仅构造未Start）：handler经GetSsuExecutor拿到非空对象即可走到受理分支
        (void)module->Create("ubseSsuController", 1, 10);
        executorCreated = true;
    }
    return module;
}

// mock UbseTaskExecutor::Execute(std::function)：捕获任务不入队
bool MockCaptureReconcileExecute(task_executor::UbseTaskExecutor *self, const std::function<void()> &task)
{
    (void)self;
    g_capturedReconcileTasks.push_back(task);
    return true;
}

} // namespace

// 启用executor捕获mock（GetModule返回真实module + Execute捕获任务）
static void SetupSsuExecutorCapture()
{
    g_capturedReconcileTasks.clear();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<task_executor::UbseTaskExecutorModule>).reset();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(invoke(MockGetTaskExecutorModule));
    bool (task_executor::UbseTaskExecutor::*executeFunc)(const std::function<void()> &) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER_CPP(executeFunc).reset();
    MOCKER_CPP(executeFunc).stubs().will(invoke(MockCaptureReconcileExecute));
}

// 复位executor捕获mock（防残留GetModule mock影响同进程其他依赖executor为null的用例）
static void ResetSsuExecutorCapture()
{
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<task_executor::UbseTaskExecutorModule>).reset();
    bool (task_executor::UbseTaskExecutor::*executeFunc)(const std::function<void()> &) =
        &task_executor::UbseTaskExecutor::Execute;
    MOCKER_CPP(executeFunc).reset();
    g_capturedReconcileTasks.clear();
    // 防御性复位单飞守卫：捕获的任务未被回放时guard残留running会串扰后续用例
    g_reconcileBatchGuard.Exit();
}

// RAII守卫：ASSERT提前返回也能可靠清理mock，避免跨用例串扰
class SsuExecutorCaptureGuard {
public:
    SsuExecutorCaptureGuard()
    {
        SetupSsuExecutorCapture();
    }
    ~SsuExecutorCaptureGuard()
    {
        ResetSsuExecutorCapture();
    }
};

// 同步回放捕获的executor任务（等价executor线程执行），验证异步路径副作用
static void RunCapturedReconcileTasks()
{
    auto tasks = std::move(g_capturedReconcileTasks);
    g_capturedReconcileTasks.clear();
    for (auto &task : tasks) {
        task();
    }
}

// HandleReconcileReqReceiver：反序列化失败 → 响应携带DESERIALIZE_FAILED
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_DeserializeFail)
{
    std::unique_ptr<UbseRpcMessage> resp;
    const uint8_t invalidData[] = {0x00};
    HandleReconcileReqReceiver(invalidData, sizeof(invalidData), resp);
    ASSERT_NE(resp, nullptr);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().errorCode, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
}

// executor为null（UT默认态，未初始化UbseTaskExecutorModule）→ 受理失败，errorCode=EXECUTOR_NULL
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_ExecutorNull)
{
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_exec_null", UbseSsuReconcileAction::CLEANUP));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().errorCode, UBSE_SSU_ERROR_EXECUTOR_NULL);
}

// 单飞守卫：上批仍在执行（guard被占用）→ 拒绝受理，errorCode=RESOURCE_BUSY，任务不提交
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_SingleFlightReject)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    // 预占单飞守卫，模拟上批异步执行未完成
    ASSERT_TRUE(g_reconcileBatchGuard.TryEnter());

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_busy", UbseSsuReconcileAction::CLEANUP));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    ASSERT_NE(resp, nullptr);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().errorCode, UBSE_ERR_RESOURCE_BUSY);
    EXPECT_TRUE(g_capturedReconcileTasks.empty()); // 未提交任务到executor

    g_reconcileBatchGuard.Exit(); // 释放预占，交由RAII统一复位mock
}

// REMOUNT本地记录命中（探测就绪）：handler受理即回（result=UBSE_OK），
// 异步执行后记录收敛ATTACHED（SUSPECT随之解除）并回报
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_RemountProbeHitConvergeAttached)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();
    auto &store = ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance();
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_remount_hit", UbseSsuAttachRecordState::SUSPECT)), UBSE_OK);

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_remount_hit", UbseSsuReconcileAction::REMOUNT));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    const auto &data = reconcileResp->GetReconcileResp();
    ASSERT_EQ(data.items.size(), 1U);
    EXPECT_EQ(data.items[0].result, UBSE_OK); // 受理即回
    // 受理后未回放前不产生副作用
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 0U);

    // 回放捕获任务（等价executor线程执行）
    ASSERT_EQ(g_capturedReconcileTasks.size(), 1U);
    RunCapturedReconcileTasks();
    // SUSPECT经master裁决重挂后收敛为ATTACHED
    auto record = FindRecord("sp_remount_hit");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->recordState, UbseSsuAttachRecordState::ATTACHED);
    // 执行结果经SendStatusUpdate回报（§13.3闭环）
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 1U);
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateName, "sp_remount_hit");
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateState, UbseSsuNsState::ATTACHED);
}

// CLEANUP：账本无该空间（已free）agent有残留记录 → 受理后异步执行：清理+删记录+上报CREATED
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_CleanupRemovesResidueRecord)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();
    auto &store = ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance();
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_residue", UbseSsuAttachRecordState::ATTACHED)), UBSE_OK);

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_residue", UbseSsuReconcileAction::CLEANUP));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    const auto &data = reconcileResp->GetReconcileResp();
    ASSERT_EQ(data.items.size(), 1U);
    EXPECT_EQ(data.items[0].result, UBSE_OK);
    // 受理后未回放前记录仍在（异步执行尚未发生）
    EXPECT_TRUE(FindRecord("sp_residue").has_value());

    RunCapturedReconcileTasks();
    EXPECT_FALSE(FindRecord("sp_residue").has_value()); // 记录删除
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateState, UbseSsuNsState::CREATED);
}

// CLEANUP：本地记录已不存在（主流程已清理）→ 受理后异步执行幂等成功
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_CleanupIdempotentWhenRecordGone)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_gone", UbseSsuReconcileAction::CLEANUP));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().items[0].result, UBSE_OK);

    RunCapturedReconcileTasks(); // 幂等成功不崩溃
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateState, UbseSsuNsState::CREATED);
}

// REPORT：本地记录缺失 → 受理后异步执行跳过补报（master侧diff下轮再裁决）
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_ReportWithoutRecordSkipped)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();
    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_report_miss", UbseSsuReconcileAction::REPORT));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().items[0].result, UBSE_OK);

    RunCapturedReconcileTasks();
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 0U); // 无记录可报，跳过
    EXPECT_FALSE(FindRecord("sp_report_miss").has_value());
}

// REPORT：ATTACHED记录 → 受理后异步执行经SendStatusUpdate补报，记录保留
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_ReportAttachedRereports)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();
    auto &store = ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance();
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_rereport", UbseSsuAttachRecordState::ATTACHED, true)), UBSE_OK);

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_rereport", UbseSsuReconcileAction::REPORT));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    const auto &data = reconcileResp->GetReconcileResp();
    ASSERT_EQ(data.items.size(), 1U);
    EXPECT_EQ(data.items[0].result, UBSE_OK);

    RunCapturedReconcileTasks();
    EXPECT_TRUE(FindRecord("sp_rereport").has_value()); // 记录保留
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 1U);
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateState, UbseSsuNsState::ATTACHED);
}

// REPORT：PENDING（在途挂载）→ 受理后异步执行跳过不补报（主流程收敛），下轮再收敛
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_ReportPendingSkipped)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();
    auto &store = ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance();
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_report_pending", UbseSsuAttachRecordState::PENDING)), UBSE_OK);

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_report_pending", UbseSsuReconcileAction::REPORT));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().items[0].result, UBSE_OK);

    RunCapturedReconcileTasks();
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 0U); // 不补报
    EXPECT_TRUE(FindRecord("sp_report_pending").has_value());       // 记录保留
}

// REPORT：master回SPACE_NOT_FOUND（diff后又被free）→ 受理后异步执行残留自愈清理+记录删除
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_ReportSpaceNotFoundSelfHeals)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();
    ubse::ssu::service::ut::g_mockStatusUpdateResult = UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    auto &store = ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance();
    ASSERT_EQ(store.Upsert(MakeReconcileRecord("sp_self_heal", UbseSsuAttachRecordState::ATTACHED)), UBSE_OK);

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_self_heal", UbseSsuReconcileAction::REPORT));
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    EXPECT_EQ(reconcileResp->GetReconcileResp().items[0].result, UBSE_OK);

    RunCapturedReconcileTasks();
    EXPECT_FALSE(FindRecord("sp_self_heal").has_value()); // 残留自愈清理
}

// ExecuteReconcileBatch直测（防御性守卫）：REMOUNT无本地记录且无master补齐记录 →
// 跳过不执行重挂（正常不可达：master首发必带hasRecord=true）
TEST_F(TestUbseSsuRpcHandler, ExecuteReconcileBatch_RemountWithoutRecordDefensiveSkip)
{
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();

    std::vector<UbseSsuReconcileItem> items;
    items.push_back(MakeReconcileItem("sp_defensive", UbseSsuReconcileAction::REMOUNT));
    ExecuteReconcileBatch(std::move(items));

    EXPECT_FALSE(FindRecord("sp_defensive").has_value()); // 不用空壳记录执行重挂
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 0U);
}

// 混合平账指令批量下发 → 受理即回：响应项与请求一一对应，逐项result=UBSE_OK（已受理）；
// 回放后逐项异步执行：无记录REMOUNT/REPORT跳过、无记录CLEANUP幂等上报CREATED
TEST_F(TestUbseSsuRpcHandler, HandleReconcileReqReceiver_MultiItemsResultsMappedByIndex)
{
    SsuExecutorCaptureGuard executorGuard;
    ubse::ssu::service::ut::SetupAgentRoleAndRpcSuccess();
    ubse::ssu::service::ut::ResetStatusUpdateMockState();

    UbseSsuReconcileReq req;
    req.items.push_back(MakeReconcileItem("sp_m1", UbseSsuReconcileAction::REMOUNT)); // 无记录→防御性跳过
    req.items.push_back(MakeReconcileItem("sp_m2", UbseSsuReconcileAction::REPORT));  // 无记录→跳过
    req.items.push_back(MakeReconcileItem("sp_m3", UbseSsuReconcileAction::CLEANUP)); // 无记录→幂等OK
    UbseSsuReconcileReqMsg reqMsg(req);
    SerializedData sd(reqMsg);
    std::unique_ptr<UbseRpcMessage> resp;
    HandleReconcileReqReceiver(sd.data.get(), sd.size, resp);
    auto reconcileResp = dynamic_cast<UbseSsuReconcileRespMsg *>(resp.get());
    ASSERT_NE(reconcileResp, nullptr);
    const auto &data = reconcileResp->GetReconcileResp();
    ASSERT_EQ(data.items.size(), 3U);
    EXPECT_EQ(data.items[0].name, "sp_m1");
    EXPECT_EQ(data.items[0].result, UBSE_OK);
    EXPECT_EQ(data.items[1].name, "sp_m2");
    EXPECT_EQ(data.items[1].result, UBSE_OK);
    EXPECT_EQ(data.items[2].name, "sp_m3");
    EXPECT_EQ(data.items[2].result, UBSE_OK);

    RunCapturedReconcileTasks();
    EXPECT_EQ(ubse::ssu::service::ut::g_statusUpdateCallCount, 1U); // 仅sp_m3的CLEANUP上报
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateName, "sp_m3");
    EXPECT_EQ(ubse::ssu::service::ut::g_lastStatusUpdateState, UbseSsuNsState::CREATED);
}

} // namespace ubse::ssu::controller::ut
