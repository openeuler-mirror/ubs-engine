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

#include <map>
#include <vector>

#include "ubse_storage.h"

namespace ubse::storage {

static uint32_t g_mockStoragePutError = 0;
static uint32_t g_mockStorageQueryError = 0;
static uint32_t g_mockStorageDeleteError = 0;
static uint32_t g_mockStorageListError = 0;
static std::vector<std::string> g_mockStorageKeys;
static std::map<std::string, std::vector<uint8_t>> g_mockStorageQueryPayloads;

void MockSetStorageQueryPayload(const std::string& keyPrefix, const std::string& key,
                                const std::vector<uint8_t>& payload)
{
    g_mockStorageQueryPayloads[keyPrefix + ":" + key] = payload;
}

void MockSetStoragePutError(uint32_t err)
{
    g_mockStoragePutError = err;
}
void MockSetStorageQueryError(uint32_t err)
{
    g_mockStorageQueryError = err;
}
void MockSetStorageDeleteError(uint32_t err)
{
    g_mockStorageDeleteError = err;
}
void MockSetStorageListError(uint32_t err)
{
    g_mockStorageListError = err;
}
void MockSetStorageKeys(const std::vector<std::string>& keys)
{
    g_mockStorageKeys = keys;
}
void MockResetStorageErrors()
{
    g_mockStoragePutError = 0;
    g_mockStorageQueryError = 0;
    g_mockStorageDeleteError = 0;
    g_mockStorageListError = 0;
    g_mockStorageKeys.clear();
    g_mockStorageQueryPayloads.clear();
}

uint32_t UbseStoragePutData(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data)
{
    return g_mockStoragePutError;
}

uint32_t UbseStorageQueryData(const std::string& keyPrefix, const std::string& key, void* ctx,
                              UbseStorageDealDataFunc func)
{
    auto it = g_mockStorageQueryPayloads.find(keyPrefix + ":" + key);
    if (it != g_mockStorageQueryPayloads.end()) {
        UbseByteBuffer buff{};
        buff.data = const_cast<uint8_t*>(it->second.data());
        buff.len = it->second.size();
        func(keyPrefix, key, buff, ctx);
    }
    return g_mockStorageQueryError;
}

uint32_t UbseStorageDeleteData(const std::string& keyPrefix, const std::string& key)
{
    return g_mockStorageDeleteError;
}

uint32_t UbseStorageListKeys(const std::string& keyPrefix, std::vector<std::string>& keys)
{
    if (g_mockStorageListError != 0) {
        return g_mockStorageListError;
    }
    keys = g_mockStorageKeys;
    return 0;
}
} // namespace ubse::storage
