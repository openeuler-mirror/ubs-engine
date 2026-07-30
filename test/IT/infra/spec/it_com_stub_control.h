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

#ifndef IT_COM_STUB_CONTROL_H
#define IT_COM_STUB_CONTROL_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace ubse::it::infra {

/**
 * @brief Shared-memory control block between IT test process and com_engine_it_mock.
 *
 * One instance per node, mapped via shm_open("/com_stub_<nodeId>").
 * The test process (ItNode) creates and writes the block; the daemon (with
 * ubse_com_engine_it_mock linked via --whole-archive) maps it read-write and
 * consults it at the entry of UbseCommunication::UbseComMsgSend.
 *
 * Default state: failMask=0 (no fault injected). Cluster startup and normal
 * operation are completely unaffected. Fault is only injected when a test
 * case explicitly calls ItNode::SetComFault(); RestoreComFault() clears it
 * immediately (atomic store, no daemon restart needed).
 *
 * Layout (mirrors ObmmStubControl):
 *   - magic:     sanity check for successful mmap (0xC0BEEF01 when initialized)
 *   - failMask:  bit i set => operation i should fail (see OpBit below)
 *   - errnoVal:  UbseResult value returned by stub on failure
 *                (default UBSE_COM_ERROR_SYNC_CALL_FAIL)
 *   - count[i]:  per-op remaining failure count. 0 = persistent failure;
 *                >0 = fail N times then auto-recover. Atomically decremented.
 *   - delayMs[i]: per-op delay in milliseconds. 0 = no delay; >0 = sleep
 *                before the op returns. Runtime-adjustable via atomic store.
 *   - dstSeq:    seqlock guarding dstNodeId. Even = stable; odd = write in
 *                progress. Readers retry on odd or seq change.
 *   - dstNodeId: destination node ID filter. Empty ('\0' at [0]) means fault
 *                applies to ALL destinations; non-empty means fault only
 *                triggers when the RPC's destination node matches. This lets
 *                a test case inject "send to node X fails" without breaking
 *                communication with other nodes.
 *
 * Bit assignment:
 *   bit 0: OP_SYNC_SEND   - UbseCommunication::UbseComMsgSend (synchronous RPC)
 *   bit 1: OP_ASYNC_SEND  - reserved for UbseComMsgAsyncSend (not yet implemented)
 */
struct ComStubControl {
    static constexpr uint32_t MAGIC = 0xC0BEEF01u;
    static constexpr uint32_t OP_COUNT = 2;
    static constexpr uint32_t DST_NODE_ID_MAX = 32; // node IDs are short ("1", "2", ...)

    std::atomic<uint32_t> magic{0};
    std::atomic<uint32_t> failMask{0};
    std::atomic<uint32_t> errnoVal{0};
    std::atomic<uint32_t> count[OP_COUNT]{};
    std::atomic<uint32_t> delayMs[OP_COUNT]{};

    // Destination filter (seqlock-protected, see SetComDstNodeId/ComDstNodeIdMatches)
    std::atomic<uint32_t> dstSeq{0};
    char dstNodeId[DST_NODE_ID_MAX]{};

    /** Bit positions for failMask. */
    enum OpBit : uint32_t
    {
        OP_SYNC_SEND = 0,
        OP_ASYNC_SEND = 1, // 预留，后续扩展异步发送故障注入
    };
};

/**
 * @brief Set the destination node ID filter (writer side, test process).
 *
 * Uses seqlock: increments dstSeq to odd before writing, to even after.
 * Readers observing odd (or a seq change across the read) retry.
 * Pass empty string to clear the filter (match all destinations).
 */
inline void SetComDstNodeId(ComStubControl& ctrl, const std::string& dstNodeId)
{
    uint32_t seq = ctrl.dstSeq.load(std::memory_order_relaxed);
    ctrl.dstSeq.store(seq + 1, std::memory_order_release); // begin write (odd)
    std::memset(ctrl.dstNodeId, 0, ComStubControl::DST_NODE_ID_MAX);
    if (!dstNodeId.empty()) {
        size_t n = dstNodeId.size();
        if (n > ComStubControl::DST_NODE_ID_MAX - 1) {
            n = ComStubControl::DST_NODE_ID_MAX - 1;
        }
        std::memcpy(ctrl.dstNodeId, dstNodeId.data(), n);
    }
    ctrl.dstSeq.store(seq + 2, std::memory_order_release); // end write (even)
}

/**
 * @brief Check whether @p dstId matches the destination filter (reader side, daemon).
 *
 * Seqlock read loop: retries if dstSeq is odd (write in progress) or changed
 * between the two loads. Returns true if the filter is empty (match all) or
 * if @p dstId equals the configured filter.
 */
inline bool ComDstNodeIdMatches(const ComStubControl& ctrl, const std::string& dstId)
{
    uint32_t seq1, seq2;
    char buf[ComStubControl::DST_NODE_ID_MAX];
    do {
        seq1 = ctrl.dstSeq.load(std::memory_order_acquire);
        std::memcpy(buf, ctrl.dstNodeId, ComStubControl::DST_NODE_ID_MAX);
        seq2 = ctrl.dstSeq.load(std::memory_order_acquire);
    } while (seq1 != seq2 || (seq1 & 1u) != 0);
    // Empty filter means match all destinations
    if (buf[0] == '\0') {
        return true;
    }
    return dstId == std::string(buf);
}

} // namespace ubse::it::infra

#endif // IT_COM_STUB_CONTROL_H
