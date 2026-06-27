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
 * mem_id generation uses an atomic counter starting from 1000.
 * Export/import records are tracked in a static map for consistency.
 */

#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>

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

static bool should_fail(const char* operation)
{
    const char* fail_list = getenv("UBSE_OBMM_STUB_FAIL");
    if (fail_list == nullptr || fail_list[0] == '\0') {
        return false;
    }
    std::string list(fail_list);
    std::string op(operation);

    size_t pos = 0;
    while (pos < list.size()) {
        size_t comma = list.find(',', pos);
        std::string token = (comma == std::string::npos) ? list.substr(pos) : list.substr(pos, comma - pos);
        if (token == op) {
            return true;
        }
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    return false;
}

static void apply_delay()
{
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
    apply_delay();
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
    apply_delay();
    if (should_fail("unexport")) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_export_records.erase(id);

    return 0;
}

mem_id obmm_import(const struct obmm_mem_desc* desc, unsigned long flags, int base_dist, int* numa)
{
    apply_delay();
    if (should_fail("import")) {
        return 0;
    }

    mem_id id = g_next_mem_id.fetch_add(1);

    if (numa != nullptr) {
        *numa = 0;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_import_numa_records[id] = (numa != nullptr) ? *numa : 0;

    return id;
}

int obmm_unimport(mem_id id, unsigned long flags)
{
    apply_delay();
    if (should_fail("unimport")) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_records_mutex);
    g_import_numa_records.erase(id);

    return 0;
}

mem_id obmm_export_useraddr(int pid, void* va, size_t size, unsigned long flags, struct obmm_mem_desc* desc)
{
    apply_delay();
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
    apply_delay();
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
    apply_delay();
    if (should_fail("preimport")) {
        return -1;
    }

    return 0;
}

int obmm_unpreimport(const struct obmm_preimport_info* preimport_info, unsigned long flags)
{
    apply_delay();
    if (should_fail("unpreimport")) {
        return -1;
    }

    return 0;
}
}