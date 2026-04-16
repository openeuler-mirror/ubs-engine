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

#include "ubse_mem_debt_info_query_handler.h"
#include "debt/ubse_mem_debt_info_query.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_controller_def_simpo.h"
#include "message/ubse_mem_debt_info_query_req_simpo.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_debt_info.h"
#include "ubse_mem_debt_info_partial_fetch.h"
#include "ubse_mmi_interface.h"
#include "ubse_mem_util.h"

namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::context;
using namespace ubse::mem;
using namespace ubse::mem::controller::message;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::util;
using namespace ubse::mem::controller;
UbseResult UbseMemDebtInfoQueryHandler::RegUbseMemDebtInfoQueryHandler()
{
    UbseComBaseMessageHandlerPtr ubseComBaseMessageHandler = new (std::nothrow) UbseMemDebtInfoQueryHandler();
    if (ubseComBaseMessageHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseMemDebtInfoQueryHandler failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "get UbseComModule failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    auto retCode =
        ubseComModule->RegRpcService<NodeMemDebtInfoQueryReqSimpo, NodeMemDebtInfoSimpo>(ubseComBaseMessageHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "ubseComBaseMessageHandler register fail," << FormatRetCode(retCode);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoQueryHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req和resp
    auto reqPtr = UbseBaseMessage::DeConvert<NodeMemDebtInfoQueryReqSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoQueryReqSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据;
    NodeMemDebtInfoMap data;
    if (reqPtr->GetNodeId().empty()) {
        // 全量数据
        data = mem::controller::GetNodeMemDebtInfoMap();
    } else {
        // 指定节点数据
        auto debtInfoMap = mem::controller::GetNodeMemDebtInfoMap();
        auto it = debtInfoMap.find(reqPtr->GetNodeId());
        if (it != debtInfoMap.end()) {
            data[reqPtr->GetNodeId()] = it->second;
        }
    }
    // 检查数据是否有效
    if (data.empty()) {
        UBSE_LOG_WARN << "node mem debt info is empty!";
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<NodeMemDebtInfoSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetNodeMemDebtInfoSimpo(data);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoFdGetHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    def::UbseMemFdDesc desc{};
    if (const auto ret = debt::UbseMemFdGet(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemFdDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemFdDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemFdDesc(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoFdListHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                UbseComBaseMessageHandlerCtxPtr ctx)
{
    UBSE_LOG_INFO << "UbseMemDebtInfoFdListHandler Handle start";
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    std::vector<def::UbseMemFdDesc> desc{};
    if (const auto ret = debt::UbseMemFdList(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd list failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemFdDescListSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemFdDescListSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemFdDescList(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoNumaGetHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    def::UbseMemNumaDesc desc{};
    if (const auto ret = debt::UbseMemNumaGet(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "numa get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<DefUbseMemNumaDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new DefUbseMemNumaDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemNumaDesc(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoNumaGetWithImportNodeHandler::Handle(const UbseBaseMessagePtr &req,
                                                               const UbseBaseMessagePtr &rsp,
                                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    UbseMemNumaDesc desc{};
    if (const auto ret = debt::UbseMemNumaGetWithImportNode(reqPtr->GetUbseMemDebtQueryRequest(), desc);
        ret != UBSE_OK) {
        UBSE_LOG_ERROR << "numa get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemNumaDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemNumaDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemNumaDesc(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoNumaListHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    std::vector<def::UbseMemNumaDesc> desc{};
    if (const auto ret = debt::UbseMemNumaList(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd list failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<DefUbseMemNumaDescListSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new DefUbseMemNumaDescListSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemNumaDescList(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoShmGetHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    def::UbseMemShmDesc desc{};
    if (const auto ret = debt::UbseMemShmGet(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemShmDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemShmDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemShmDesc(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoShmListHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    std::vector<def::UbseMemShmDesc> desc{};
    if (const auto ret = debt::UbseMemShmList(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemShmDescListSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemShmDescListSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemShmDescList(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoShmStatusGetHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                      UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    def::UbseMemShmMemStatusDesc desc{};
    if (const auto ret = debt::UbseMemShmStatusGet(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemShmMemStatusDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemShmMemStatusDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemShmMemStatusDesc(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoAddrGetHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 解析req 和resp
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }

    // 取数据
    UbseMemAddrDesc desc{};
    if (const auto ret = debt::UbseMemAddrGet(reqPtr->GetUbseMemDebtQueryRequest(), desc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fd get failed, ret: " << FormatRetCode(ret);
        return ret;
    }
    // 封装响应;
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemAddrDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemAddrDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemAddrDesc(desc);
    return UBSE_OK;
}

UbseResult UbseMemDebtInfoPartialFetchHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                      UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemDebtInfoPartialFetchReq>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtInfoPartialFetchReq failed!";
        return UBSE_ERROR_NULLPTR;
    }
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemDebtInfoPartialFetchRes>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemDebtInfoPartialFetchRes failed!";
        return UBSE_ERROR_NULLPTR;
    }
    if (reqPtr->InputRawData() == nullptr || reqPtr->InputRawDataSize() == 0) {
        UBSE_LOG_ERROR << "account fetch info is null!";
        return UBSE_ERROR_EMPTY;
    }

    PartialFetchRes partialFetchRes{};
    const auto debtFetchInfo = reqPtr->GetUbseMemDebtFetchInfo();
    if (debt::ValidateDebtFetchInfo(debtFetchInfo) != UBSE_OK) {
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << "debtFetchInfo: " << debtFetchInfo.toString();
    UbseResult result = debt::FetchDebtInfoByTypeAndPage(debtFetchInfo, partialFetchRes);
    if (result != UBSE_OK) {
        return result;
    }

    respPtr->SetFlatDebtInformationVec(partialFetchRes);
    return UBSE_OK;
}

UbseResult UbseMemNodeBorrowQueryHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    // 无入参
    std::vector<def::UbseNodeBorrowInfo> nodeBorrowInfo{};
    auto ret = mem::controller::debt::UbseMemNodeBorrowQuery(nodeBorrowInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Query node borrow info failed!";
        return ret;
    }
    auto respPtr = UbseBaseMessage::DeConvert<mem::controller::message::UbseMemNodeBorrowInfoMessage>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new NodeMemDebtInfoSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseNodeBorrowInfos(nodeBorrowInfo);
    return UBSE_OK;
}

UbseResult UbseMemIdInfoGetHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                           UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto reqPtr = UbseBaseMessage::DeConvert<UbseMemIdQueryRequestSimpo>(req);
    if (reqPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemIdQueryRequestSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    def::UbseMemIdQueryRequest request = reqPtr->GetUbseMemIdQueryRequest();
    def::UbseExportMemDesc exportMemDesc{};
    if (const auto ret = mem::controller::debt::UbseMemGetMemIdByImport(request, exportMemDesc); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "mem id get failed, " << FormatRetCode(ret);
        return ret;
    }
    auto respPtr = UbseBaseMessage::DeConvert<UbseMemExportMemDescSimpo>(rsp);
    if (respPtr == nullptr) {
        UBSE_LOG_ERROR << "new UbseMemExportMemDescSimpo failed!";
        return UBSE_ERROR_NULLPTR;
    }
    respPtr->SetUbseMemExportMemDesc(exportMemDesc);
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc