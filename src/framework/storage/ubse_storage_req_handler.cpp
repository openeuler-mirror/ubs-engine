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

#include "ubse_storage_req_handler.h"
#include "ubse_storage_module.h"
#include "ubse_context.h"
#include "ubse_storage_req_simpo.h"
#include "ubse_storage_resp_simpo.h"

namespace ubse::storage {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::storage::message;
UbseResult UbseStorageReqHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                         UbseComBaseMessageHandlerCtxPtr ctx)
{
    UbseStorageReqSimpoPtr request = UbseBaseMessage::DeConvert<UbseStorageReqSimpo>(req);
    if (request == nullptr) {
        UBSE_LOG_ERROR << "UbseStorageReqSimpoPtr is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    auto id = request->ToString();
    UBSE_LOG_DEBUG << "UbseStorageGetHandler::Handle Start=" << id;
    auto storageModule = UbseStorageModule::GetStorageModule();
    if (storageModule == nullptr) {
        UBSE_LOG_ERROR << "UbseStorageGetHandler::Handle Get storageModule failed.";
        return UBSE_ERROR;
    }
    UbseStorageReq storageReq = request->GetStorageReq();
    UbseStorageRespSimpoPtr response = UbseBaseMessage::DeConvert<UbseStorageRespSimpo>(rsp);
    if (response == nullptr) {
        UBSE_LOG_ERROR << "UbseStorageRespSimpoPtr is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseStorageResp storageResp;
    auto ret = UBSE_OK;
    KV kv;
    switch (storageReq.cmdType) {
        case UbseStorageReqCmdType::GET:
            ret = storageModule->Get(storageReq.dbName, storageReq.key, kv);
            storageResp.kvs.emplace_back(std::move(kv));
            break;
        default:
            UBSE_LOG_ERROR << "UbseStorageGetHandler::Handle unknown request type.";
            return UBSE_ERROR;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseStorageGetHandler::Handle Error=" << id;
        return UBSE_ERROR;
    }
    response->SetStorageResp(storageResp);
    UBSE_LOG_DEBUG << "UbseStorageGetHandler::Handle End=" << id;
    return UBSE_OK;
}
} // namespace ubse::storage