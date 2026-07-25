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

#include "ubse_storage.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_storage_module.h"

namespace ubse::storage {
using namespace ubse::context;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseStoragePutData(const std::string& keyPrefix, const std::string& key, UbseByteBuffer* data)
{
    auto module = UbseContext::GetInstance().GetModule<UbseStorageModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "The storage module has not been initialized";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (data == nullptr) {
        UBSE_LOG_ERROR << "UbseByteBuffer is null, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult ret = module->Put(DEFAULT_DB_NAME, keyPrefix + key, data->data, static_cast<uint32_t>(data->len));
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to put data, " << FormatRetCode(ret);
    }
    return ret;
}

uint32_t UbseStorageQueryData(const std::string& keyPrefix, const std::string& key, void* ctx,
                              UbseStorageDealDataFunc func)
{
    if (func == nullptr) {
        UBSE_LOG_ERROR << "Callback func is null";
        return UBSE_ERROR_NULLPTR;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseStorageModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "The storage module has not been initialized";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    KV kv{};
    UbseResult ret = module->Get(DEFAULT_DB_NAME, keyPrefix + key, kv);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get data, " << FormatRetCode(ret);
        module->ResultFree(kv);
        return ret;
    }
    func(keyPrefix, key, {kv.value, kv.valueLen, nullptr}, ctx);
    module->ResultFree(kv);
    return UBSE_OK;
}

uint32_t UbseStorageDeleteData(const std::string& keyPrefix, const std::string& key)
{
    auto module = UbseContext::GetInstance().GetModule<UbseStorageModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "The storage module has not been initialized";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseResult ret = module->Delete(DEFAULT_DB_NAME, keyPrefix + key);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to delete data, " << FormatRetCode(ret);
    }
    return ret;
}
} // namespace ubse::storage