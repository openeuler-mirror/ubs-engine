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

/**
 * @file obmm_stub.cpp
 * @brief Stub implementation of OBMM kernel module library for IT testing.
 *
 * This shared library replaces libobmm.so.1 when running IT tests.
 * It simulates OBMM export/import/unexport/unimport operations using
 * environment variables for 3-layer configuration:
 *   - UBSE_OBMM_STUB_FAIL: comma-separated operations that should fail
 *   - UBSE_OBMM_STUB_DELAY_MS: delay in milliseconds for all operations
 *
 * Runtime per-op fault injection (failure/delay) is supported via shared
 * memory (ObmmStubControl) when UBSE_IT_NODE_ID is set; see apply_delay()
 * and should_fail() for details.
 *
 * mem_id generation uses an atomic counter starting from 1000.
 * Export/import records are tracked in a static map for consistency.
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "it_obmm_stub_control.h"

using ubse::it::infra::ObmmStubControl;

#ifndef UBSE_EID_LENGTH
#define UBSE_EID_LENGTH 16
#endif

#ifndef OBMM_MAX_LOCAL_NUMA_NODES
#define OBMM_MAX_LOCAL_NUMA_NODES 16
#endif

struct obmm_mem_desc {
    uint64_t addr;
    uint64_t length;
    uint8_t seid[UBSE_EID_LENGTH];
    uint8_t deid[UBSE_EID_LENGTH];
    uint32_t tokenid;
    uint32_t scna;
    uint32_t dcna;
    uint16_t priv_len;
    uint8_t priv[];
} __attribute__((aligned(8)));

struct obmm_preimport_info {
    uint64_t pa;
    uint64_t length;
    int base_dist;
    int numa_id;
    uint8_t seid[UBSE_EID_LENGTH];
    uint8_t deid[UBSE_EID_LENGTH];
    uint32_t scna;
    uint32_t dcna;
    uint16_t priv_len;
    uint8_t priv[];
};

using mem_id = uint64_t;

static std::atomic<uint64_t> g_next_mem_id{1000};
static std::mutex g_records_mutex;
static std::map<mem_id, obmm_mem_desc*> g_export_records;
static std::map<mem_id, int> g_import_numa_records;

// --- Shared-memory control block (runtime fault injection) ---
// Lazily mapped on first should_fail() call from env UBSE_IT_OBMM_SHM.
// Once mapped, reads are atomic loads (~10ns), suitable for high-frequency
// obmm calls. If mapping fails or shm is absent, falls back to env-based
// UBSE_OBMM_STUB_FAIL behavior (backward compatible).
static ObmmStubControl* g_ctrl = nullptr;

static int op_index(const char* operation)
{
    if (operation == nullptr) {
        return -1;
    }
    if (strcmp(operation, "export") == 0) {
        return ObmmStubControl::OP_EXPORT;
    }
    if (strcmp(operation, "unexport") == 0) {
        return ObmmStubControl::OP_UNEXPORT;
    }
    if (strcmp(operation, "import") == 0) {
        return ObmmStubControl::OP_IMPORT;
    }
    if (strcmp(operation, "unimport") == 0) {
        return ObmmStubControl::OP_UNIMPORT;
    }
    if (strcmp(operation, "export_useraddr") == 0) {
        return ObmmStubControl::OP_EXPORT_USERADDR;
    }
    if (strcmp(operation, "query_pa") == 0) {
        return ObmmStubControl::OP_QUERY_PA;
    }
    if (strcmp(operation, "preimport") == 0) {
        return ObmmStubControl::OP_PREIMPORT;
    }
    if (strcmp(operation, "unpreimport") == 0) {
        return ObmmStubControl::OP_UNPREIMPORT;
    }
    return -1;
}

static void ensure_ctrl_loaded()
{
    static std::once_flag once;
    std::call_once(once, []() {
        // shm name 与 ItNode 约定: /obmm_stub_<nodeId>
        const char* nodeId = getenv("UBSE_IT_NODE_ID");
        if (nodeId == nullptr || nodeId[0] == '\0') {
            return;
        }
        std::string name = "/obmm_stub_" + std::string(nodeId);
        int fd = shm_open(name.c_str(), O_RDWR, 0600);
        if (fd < 0) {
            return;
        }
        void* p = mmap(nullptr, sizeof(ObmmStubControl), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (p == MAP_FAILED) {
            return;
        }
        auto* ctrl = static_cast<ObmmStubControl*>(p);
        if (ctrl->magic.load(std::memory_order_acquire) != ObmmStubControl::MAGIC) {
            munmap(ctrl, sizeof(ObmmStubControl));
            return;
        }
        g_ctrl = ctrl;
    });
}

static bool should_fail(const char* operation)
{
    const char* fail_list = getenv("UBSE_OBMM_STUB_FAIL");
    if (fail_list != nullptr && fail_list[0] != '\0') {
        std::string list(fail_list);
        std::string op(operation);
        size_t pos = 0;
        while (pos < list.size()) {
            size_t comma = list.find(',', pos);
            std::string token = (comma == std::string::npos) ? list.substr(pos) : list.substr(pos, comma - pos);
            if (token == op) {
                errno = ENOMEM;
                return true;
            }
            if (comma == std::string::npos) {
                break;
            }
            pos = comma + 1;
        }
    }

    // 2) Shared-memory runtime fault injection
    ensure_ctrl_loaded();
    if (g_ctrl == nullptr) {
        return false;
    }
    int idx = op_index(operation);
    if (idx < 0) {
        return false;
    }
    uint32_t mask = g_ctrl->failMask.load(std::memory_order_relaxed);
    if ((mask & (1u << idx)) == 0) {
        return false;
    }
    errno = g_ctrl->errnoVal.load(std::memory_order_relaxed);
    // count>0: decrement; reaches 0 → clear bit and let this call succeed
    uint32_t remaining = g_ctrl->count[idx].load(std::memory_order_relaxed);
    if (remaining > 0) {
        uint32_t prev = g_ctrl->count[idx].fetch_sub(1, std::memory_order_relaxed);
        if (prev == 0) {
            // Another thread consumed the last failure; clear bit and succeed
            g_ctrl->failMask.fetch_and(~(1u << idx), std::memory_order_relaxed);
            return false;
        }
    }
    return true;
}

static void apply_delay(const char* operation)
{
    // 1) shm per-op 延迟（运行时控制，优先级高）
    ensure_ctrl_loaded();
    if (g_ctrl != nullptr) {
        int idx = op_index(operation);
        if (idx >= 0) {
            uint32_t ms = g_ctrl->delayMs[idx].load(std::memory_order_relaxed);
            if (ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                return;
            }
        }
    }
    // 2) env var 回退（向后兼容，全局默认延迟）
    const char* delay_str = getenv("UBSE_OBMM_STUB_DELAY_MS");
    if (delay_str != nullptr && delay_str[0] != '\0') {
        int delay_ms = atoi(delay_str);
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
}

extern "C" {

mem_id obmm_export(const size_t length[OBMM_MAX_LOCAL_NUMA_NODES], unsigned long flags, struct obmm_mem_desc* desc)
{
    apply_delay("export");
    if (should_fail("export")) {
        return 0;
    }

    mem_id id = g_next_mem_id.fetch_add(1);

    if (desc != nullptr) {
        desc->addr = id;
        desc->length = 0;
        for (size_t i = 0; i < OBMM_MAX_LOCAL_NUMA_NODES; ++i) {
            desc->length += length[i];
        }
        desc->tokenid = static_cast<uint32_t>(id);
        desc->scna = 0;
        desc->dcna = 0;
        memset(desc->seid, 0, UBSE_EID_LENGTH);
        memset(desc->deid, 0, UBSE_EID_LENGTH);
        desc->priv_len = 0;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_export_records[id] = desc;

    return id;
}

int obmm_unexport(mem_id id, unsigned long flags)
{
    apply_delay("unexport");
    if (should_fail("unexport")) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_export_records.erase(id);

    return 0;
}

mem_id obmm_import(const struct obmm_mem_desc* desc, unsigned long flags, int base_dist, int* numa)
{
    apply_delay("import");
    if (should_fail("import")) {
        return 0;
    }

    mem_id id = g_next_mem_id.fetch_add(1);

    if (numa != nullptr) {
        // 真实OBMM内核模块会写入实际的远端NUMA ID，stub保留原始值
        // 避免ObmmImport中 *numa==0 检查拦截后续迭代的import调用
        if (*numa <= 0) {
            *numa = 1; // 无有效NUMA映射时使用默认值，绕过 *numa==0 拦截
        }
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_import_numa_records[id] = (numa != nullptr) ? *numa : 0;

    return id;
}

int obmm_unimport(mem_id id, unsigned long flags)
{
    apply_delay("unimport");
    if (should_fail("unimport")) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_import_numa_records.erase(id);

    return 0;
}

mem_id obmm_export_useraddr(int pid, void* va, size_t size, unsigned long flags, struct obmm_mem_desc* desc)
{
    apply_delay("export_useraddr");
    if (should_fail("export_useraddr")) {
        return 0;
    }

    mem_id id = g_next_mem_id.fetch_add(1);

    if (desc != nullptr) {
        desc->addr = reinterpret_cast<uint64_t>(va);
        desc->length = size;
        desc->tokenid = static_cast<uint32_t>(id);
        desc->scna = 0;
        desc->dcna = 0;
        memset(desc->seid, 0, UBSE_EID_LENGTH);
        memset(desc->deid, 0, UBSE_EID_LENGTH);
        desc->priv_len = 0;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_export_records[id] = desc;

    return id;
}

int obmm_query_pa_by_memid(mem_id id, unsigned long offset, unsigned long* pa)
{
    apply_delay("query_pa");
    if (should_fail("query_pa")) {
        return -1;
    }

    if (pa != nullptr) {
        *pa = (id << 12) + offset;
    }

    return 0;
}

int obmm_preimport(struct obmm_preimport_info* preimport_info, unsigned long flags)
{
    apply_delay("preimport");
    if (should_fail("preimport")) {
        return -1;
    }

    return 0;
}

int obmm_unpreimport(const struct obmm_preimport_info* preimport_info, unsigned long flags)
{
    apply_delay("unpreimport");
    if (should_fail("unpreimport")) {
        return -1;
    }

    return 0;
}
}