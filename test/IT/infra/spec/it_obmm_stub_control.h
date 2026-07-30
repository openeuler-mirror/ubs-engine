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

#ifndef IT_OBMM_STUB_CONTROL_H
#define IT_OBMM_STUB_CONTROL_H

#include <atomic>
#include <cstdint>

namespace ubse::it::infra {

/**
 * @brief Shared-memory control block between IT test process and obmm_stub.so.
 *
 * One instance per node, mapped via shm_open("/obmm_stub_<nodeId>").
 * The test process creates and writes the block; the daemon (with
 * LD_PRELOAD'd obmm_stub) maps it read-write and consults it on every
 * obmm_* call. Reads are atomic loads (~10ns), suitable for the high
 * call frequency of obmm interfaces.
 *
 * Layout:
 *   - magic:     sanity check for successful mmap (0xDEADBEEF when initialized)
 *   - failMask:  bit i set => operation i should fail (see OpIndex below)
 *   - errnoVal:  errno value set by stub when returning failure
 *   - count[i]:  per-op remaining failure count. 0 = persistent failure;
 *                >0 = fail N times then auto-recover. Atomically decremented.
 *   - delayMs[i]: per-op delay in milliseconds. 0 = no delay; >0 = sleep
 *                before the op returns. Runtime-adjustable via atomic store.
 *
 * Bit assignment (must match OpIndex in obmm_stub.cpp):
 *   bit 0: export
 *   bit 1: unexport
 *   bit 2: import
 *   bit 3: unimport
 *   bit 4: export_useraddr
 *   bit 5: query_pa
 *   bit 6: preimport
 *   bit 7: unpreimport
 */
struct ObmmStubControl {
    static constexpr uint32_t MAGIC = 0xDEADBEEFu;
    static constexpr uint32_t OP_COUNT = 8;

    std::atomic<uint32_t> magic{0};
    std::atomic<uint32_t> failMask{0};
    std::atomic<int32_t> errnoVal{0};
    std::atomic<uint32_t> count[OP_COUNT]{};
    std::atomic<uint32_t> delayMs[OP_COUNT]{};

    /** Bit positions for failMask. */
    enum OpBit : uint32_t
    {
        OP_EXPORT = 0,
        OP_UNEXPORT = 1,
        OP_IMPORT = 2,
        OP_UNIMPORT = 3,
        OP_EXPORT_USERADDR = 4,
        OP_QUERY_PA = 5,
        OP_PREIMPORT = 6,
        OP_UNPREIMPORT = 7,
    };
};

} // namespace ubse::it::infra

#endif // IT_OBMM_STUB_CONTROL_H
