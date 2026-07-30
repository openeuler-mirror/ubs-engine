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
 */

/**
 * @file ubse_com_engine_it_mock.cpp
 * @brief IT mock for UbseComEngine, linked via --whole-archive into ubse_it_daemon.
 *
 * Two overrides:
 *   1. RegisterTLSCallbacks — disable TLS (original behavior).
 *   2. UbseCommunication::UbseComMsgSend — link-time replacement of the real
 *      UbseComMsgSend to add runtime fault injection via shared memory
 *      (ComStubControl). Default failMask=0 (no fault); only fails when
 *      ItNode::SetComFault() has been called. RestoreComFault() clears it.
 *
 * When no fault is injected, the function executes the real send logic
 * (reimplemented below — mirror of src/framework/com/engine/ubse_com_engine.cpp
 * UbseCommunication::UbseComMsgSend). If the real function changes, sync
 * the reimplementation. The helper functions ItGetChannel/ItGetMessageLen/
 * ItCheckReplyResult mirror the file-static GetChannel/GetMessageLen/
 * CheckReplyResult in ubse_com_engine.cpp.
 *
 * Simplification: the real code distinguishes NN_URMA_ACK_TIMEOUT (returns
 * UBSE_COM_ERROR_RESOURCE_TEMPORARILY_UNAVAILABLE); this mock treats all
 * channel->Call failures uniformly as UBSE_COM_ERROR_SYNC_CALL_FAIL. This
 * does not affect business-side retry logic (RPC_RETRY_TIMES triggers on
 * any non-OK return).
 */

#include "engine/ubse_com_engine.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include "it_com_stub_control.h"
#include "trace_context.h"

using ubse::it::infra::ComStubControl;

namespace ubse::com {

// ====== 1. TLS mock (original behavior) ======
void UbseComEngine::RegisterTLSCallbacks(UBSHcomTlsOptions& tlsOptions)
{
    tlsOptions.enableTls = false;
}

// ====== 2. RPC fault injection via shared memory ======
// Lazily mapped on first UbseComMsgSend call from env UBSE_IT_NODE_ID.
// Once mapped, reads are atomic loads (~10ns), suitable for the RPC hot path.
// If mapping fails or shm is absent, falls through to normal send (no fault).
static ComStubControl* g_comCtrl = nullptr;

static void EnsureComCtrlLoaded()
{
    static std::once_flag once;
    std::call_once(once, []() {
        // shm name 与 ItNode::InitComShm 约定: /com_stub_<nodeId>
        const char* nodeId = getenv("UBSE_IT_NODE_ID");
        if (nodeId == nullptr || nodeId[0] == '\0') {
            return;
        }
        std::string name = "/com_stub_" + std::string(nodeId);
        int fd = shm_open(name.c_str(), O_RDWR, 0600);
        if (fd < 0) {
            return;
        }
        void* p = mmap(nullptr, sizeof(ComStubControl), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (p == MAP_FAILED) {
            return;
        }
        auto* ctrl = static_cast<ComStubControl*>(p);
        if (ctrl->magic.load(std::memory_order_acquire) != ComStubControl::MAGIC) {
            munmap(ctrl, sizeof(ComStubControl));
            return;
        }
        g_comCtrl = ctrl;
    });
}

static bool ShouldComFail(uint32_t opBit, const std::string& dstNodeId)
{
    EnsureComCtrlLoaded();
    if (g_comCtrl == nullptr) {
        return false;
    }
    uint32_t mask = g_comCtrl->failMask.load(std::memory_order_relaxed);
    if ((mask & (1u << opBit)) == 0) {
        return false;
    }
    // Destination filter: empty filter matches all; non-empty must match dstNodeId.
    if (!ComDstNodeIdMatches(*g_comCtrl, dstNodeId)) {
        return false;
    }
    // count>0: decrement; reaches 0 → clear bit and let this call succeed
    uint32_t remaining = g_comCtrl->count[opBit].load(std::memory_order_relaxed);
    if (remaining > 0) {
        uint32_t prev = g_comCtrl->count[opBit].fetch_sub(1, std::memory_order_relaxed);
        if (prev == 0) {
            // Another thread consumed the last failure; clear bit and succeed
            g_comCtrl->failMask.fetch_and(~(1u << opBit), std::memory_order_relaxed);
            return false;
        }
    }
    return true;
}

// ====== 3. Helper reimplementations (mirror of ubse_com_engine.cpp) ======
// NOTE: These mirror file-static helpers in ubse_com_engine.cpp. If the real
//       implementations change, sync these copies. Prefixed with "It" to avoid
//       symbol clashes (the real symbols also exist via --allow-multiple-definition,
//       but distinct names keep the intent explicit).

// Mirror of MAX_ERROR_CODE_LENGTH in ubse_com_engine.cpp
static constexpr int IT_MAX_ERROR_CODE_LENGTH = 20;

// Mirror of CheckReplyResult in ubse_com_engine.cpp
static UbseResult ItCheckReplyResult(const UbseComDataDesc& retData)
{
    if (retData.len > IT_MAX_ERROR_CODE_LENGTH) {
        return UBSE_OK;
    }
    std::string str(reinterpret_cast<const char*>(retData.data),
                    retData.len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (StringToUbseReplyResult(str) != UbseReplyResult::OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// Mirror of GetChannel in ubse_com_engine.cpp
static UbseResult ItGetChannel(const std::string& engineName, UbseComMessageCtx& message, UBSHcomChannelPtr& channel)
{
    const std::string& nodeId = message.GetDstId();
    UbseComChannelInfo channelInfo;
    auto engine = UbseComEngineManager::GetEngine(engineName);
    if (engine == nullptr) {
        return UBSE_COM_ERROR_GET_ENGINE_FAIL;
    }
    UbseResult res = engine->GetChannelByRemoteNodeId(nodeId, message.GetChannelType(), channelInfo);
    if (UBSE_RESULT_FAIL(res)) {
        return UBSE_COM_ERROR_CHANNEL_NOT_FOUND;
    }
    channel = channelInfo.GetChannel();
    if (channel == nullptr) {
        return UBSE_COM_ERROR_CHANNEL_NULL;
    }
    return UBSE_OK;
}

// Mirror of GetMessageLen in ubse_com_engine.cpp
static UbseResult ItGetMessageLen(UbseComMessageCtx& message, uint32_t& sendLen)
{
    auto* transMsg = static_cast<UbseComMessage*>(static_cast<void*>(message.GetMessage()));
    if (transMsg == nullptr) {
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    if (transMsg->GetMessageBodyLen() > UINT32_MAX - static_cast<uint32_t>(sizeof(UbseComMessage))) {
        return UBSE_COM_ERROR_MESSAGE_INVALID;
    }
    sendLen = static_cast<uint32_t>(sizeof(UbseComMessage)) + transMsg->GetMessageBodyLen();
    return UBSE_OK;
}

// ====== 4. UbseComMsgSend link-time replacement (with fault injection) ======
// Mirrors UbseCommunication::UbseComMsgSend in ubse_com_engine.cpp (around L1435).
// Real function change → sync this copy. See file header for simplification notes.
UbseResult UbseCommunication::UbseComMsgSend(const std::string& engineName, UbseComMessageCtx& message,
                                             UbseComDataDesc& retData)
{
    // Fault injection check (default: no fault, falls through to real logic)
    // Destination filter (dstNodeId) allows failing only sends to a specific node.
    if (ShouldComFail(ComStubControl::OP_SYNC_SEND, message.GetDstId())) {
        return static_cast<UbseResult>(g_comCtrl->errnoVal.load(std::memory_order_relaxed));
    }

    // --- Real send logic (mirror of ubse_com_engine.cpp) ---
    UBSHcomChannelPtr channel;
    uint32_t sendLen;
    UbseResult channelRes = ItGetChannel(engineName, message, channel);
    if (channelRes != UBSE_OK) {
        return channelRes;
    }
    UbseResult getLenRes = ItGetMessageLen(message, sendLen);
    if (getLenRes != UBSE_OK) {
        return getLenRes;
    }
    UbseComDataDesc sendData = {message.GetMessage(), sendLen};
    UBSHcomRequest reqMsg{sendData.data, sendData.len, 0};
    UBSHcomResponse rspMsg;
    std::string traceId = TraceContext::GetTraceId();
    channel->SetTraceId(traceId);
    auto ret = channel->Call(reqMsg, rspMsg, nullptr);
    // Simplification: real code checks NN_URMA_ACK_TIMEOUT separately; this mock
    // treats all failures as SYNC_CALL_FAIL. Business retry logic still triggers.
    if (UBSE_RESULT_FAIL(ret)) {
        return UBSE_COM_ERROR_SYNC_CALL_FAIL;
    }
    retData.data = static_cast<uint8_t*>(rspMsg.address);
    retData.len = rspMsg.size;
    if (ItCheckReplyResult(retData) != UBSE_OK) {
        return UBSE_COM_ERROR_SYNC_CALL_FAIL;
    }
    return UBSE_OK;
}

} // namespace ubse::com
