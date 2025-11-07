/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_mem_controller_msg.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/node_mem_debt_info_simpo.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_utils.h"
namespace ubse::mem::controller {
using namespace ubse::mem::controller::message;

void RegUbseMemControllerHandler()
{
    const ubse::com::UbseComEndpoint collectEndpoint = {static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                                        static_cast<uint32_t>(UbseOpCode::UBSE_MEM_LEDGER)};
    const ubse::com::UbseComEndpoint queryFdImportEndpoint = {static_cast<uint16_t>(UbseModuleCode::UBSE_MEM),
                                                              static_cast<uint32_t>(UbseOpCode::UBSE_MEM_FD_IMPORT)};
    const ubse::com::UbseComEndpoint queryNumaImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM), static_cast<uint32_t>(UbseOpCode::UBSE_MEM_NUMA_IMPORT)};
    const ubse::com::UbseComEndpoint queryAddrImportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::UBSE_MEM), static_cast<uint32_t>(UbseOpCode::UBSE_MEM_ADDR_IMPORT)};
    UbseRegRpcService(collectEndpoint, CollectLedgeHandler);
    UbseRegRpcService(queryFdImportEndpoint, QueryFdImportObjHandler);
    UbseRegRpcService(queryNumaImportEndpoint, QueryNumaImportObjHandler);
}

UbseResult CollectLedge(const std::string &nodeId, NodeMemDebtInfo &info)
{
    NodeMemDebtInfoSimpo simpo{};
    simpo.Serialize();

    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM),
        .serviceId = static_cast<uint32_t>(UbseOpCode::UBSE_MEM_LEDGER),
        .address = nodeId,
    };
    UbseResult collectRet = UBSE_OK;
    size_t size = simpo.SerializedDataSize();
    UbseByteBuffer reqBuffer{simpo.SerializedData(), size, nullptr};
    auto ret = UbseRpcSend(
        endpoint, reqBuffer, nullptr,
        [&info, &collectRet, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
            if (resCode != UBSE_OK) {
                UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " node info failed, " << FormatRetCode(resCode);
                collectRet = resCode;
                return;
            }
            if (respData.data == nullptr || respData.len == 0) {
                UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " resp null";
                collectRet = UBSE_ERROR_NULLPTR;
                return;
            }
            NodeMemDebtInfoSimpo resultSimpo{respData.data, static_cast<uint32_t>(respData.len)};
            collectRet = resultSimpo.Deserialize();
            if (collectRet != UBSE_OK) {
                UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " deserialize failed, " << FormatRetCode(collectRet);
            } else {
                if (resultSimpo.GetNodeMemDebtInfoMap().find(nodeId) != resultSimpo.GetNodeMemDebtInfoMap().end()) {
                    info = resultSimpo.GetNodeMemDebtInfoMap()[nodeId];
                }
            }
        });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send collect nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return collectRet;
}

UbseResult CollectLedgeHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    std::string nodeId = utils::GetCurNodeId();
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        return UBSE_ERROR;
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        return UBSE_ERROR;
    }
    NodeMemDebtInfo info{};
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        info = nodeMemDebtInfoMap[nodeId];
    }
    size_t size = 0;
    NodeMemDebtInfoSimpo simpo{};
    NodeMemDebtInfoMap infoMap{};
    infoMap[nodeId] = info;
    simpo.SetNodeMemDebtInfoSimpo(infoMap);
    auto ret = simpo.Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem debt info deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    size = simpo.SerializedDataSize();
    resp = {new uint8_t[size], size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    ret = memcpy_s(resp.data, resp.len, simpo.SerializedData(), size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "memcpy_s failed, ErrorCode=" << ret;
    }
    return ret;
}

UbseResult QueryFdImportObj(const std::string &nodeId, const std::string &name, UbseMemFdBorrowImportObj &info)
{
    UbseMemFdBorrowImportObj obj{};
    obj.req.name = name;
    UbseMemFdBorrowImportobjSimpo simpo{};
    simpo.SetUbseMemFdBorrowImportobj(obj);
    simpo.Serialize();

    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM),
        .serviceId = static_cast<uint32_t>(UbseOpCode::UBSE_MEM_FD_IMPORT),
        .address = nodeId,
    };
    UbseResult collectRet = UBSE_OK;
    size_t size = simpo.SerializedDataSize();
    UbseByteBuffer reqBuffer{simpo.SerializedData(), size, nullptr};
    auto ret = UbseRpcSend(
        endpoint, reqBuffer, nullptr,
        [&info, &collectRet, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
            if (resCode != UBSE_OK) {
                UBSE_LOG_ERROR << "query nodeId=" << nodeId << " fd import info failed, " << FormatRetCode(resCode);
                collectRet = resCode;
                return;
            }
            if (respData.data == nullptr || respData.len == 0) {
                UBSE_LOG_ERROR << "query nodeId=" << nodeId << " fd import resp null";
                collectRet = UBSE_ERROR_NULLPTR;
                return;
            }
            UbseMemFdBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow)
                UbseMemFdBorrowImportobjSimpo(respData.data, static_cast<uint32_t>(respData.len));
            if (resultSimpo == nullptr) {
                UBSE_LOG_ERROR << "Malloc failed, resultSimpo is nullptr.";
                collectRet = UBSE_ERROR_NULLPTR;
                return;
            }
            collectRet = resultSimpo->Deserialize();
            if (collectRet != UBSE_OK) {
                UBSE_LOG_ERROR << "query nodeId=" << nodeId << " fd import deserialize failed, "
                             << FormatRetCode(collectRet);
            } else {
                info = resultSimpo->GetImportObj();
            }
        });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send query nodeId=" << nodeId << " fd import msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return collectRet;
}

UbseResult QueryFdImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    std::string nodeId = utils::GetCurNodeId();
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        return UBSE_ERROR;
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        return UBSE_ERROR;
    }
    UbseMemFdBorrowImportobjSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem query fd import serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    std::string fdImportName = simpo.GetImportObj().req.name;
    UbseMemFdBorrowImportObj obj{};
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[nodeId].fdImportObjMap.find(fdImportName) !=
            nodeMemDebtInfoMap[nodeId].fdImportObjMap.end()) {
            obj = nodeMemDebtInfoMap[nodeId].fdImportObjMap[fdImportName];
        }
    }
    UbseMemFdBorrowImportobjSimpo resultSimpo{};
    resultSimpo.SetUbseMemFdBorrowImportobj(obj);
    ret = resultSimpo.Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem fd import deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    size = resultSimpo.SerializedDataSize();
    resp = {new uint8_t[size], size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    ret = memcpy_s(resp.data, resp.len, resultSimpo.SerializedData(), size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "memcpy_s failed, ErrorCode=" << ret;
    }
    return ret;
}

UbseResult QueryNumaImportObj(const std::string &nodeId, const std::string &name, UbseMemNumaBorrowImportObj &info)
{
    UbseMemNumaBorrowImportObj obj{};
    obj.req.name = name;
    UbseMemNumaBorrowImportobjSimpo simpo{};
    simpo.SetUbseMemNumaBorrowImportobj(obj);
    simpo.Serialize();

    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::UBSE_MEM),
        .serviceId = static_cast<uint32_t>(UbseOpCode::UBSE_MEM_NUMA_IMPORT),
        .address = nodeId,
    };
    UbseResult collectRet = UBSE_OK;
    size_t size = simpo.SerializedDataSize();
    UbseByteBuffer reqBuffer{simpo.SerializedData(), size, nullptr};
    auto ret = UbseRpcSend(
        endpoint, reqBuffer, nullptr,
        [&info, &collectRet, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
            if (resCode != UBSE_OK) {
                UBSE_LOG_ERROR << "query nodeId=" << nodeId << " numa import info failed, " << FormatRetCode(resCode);
                collectRet = resCode;
                return;
            }
            if (respData.data == nullptr || respData.len == 0) {
                UBSE_LOG_ERROR << "query nodeId=" << nodeId << " numa import resp null";
                collectRet = UBSE_ERROR_NULLPTR;
                return;
            }
            UbseMemNumaBorrowImportobjSimpo resultSimpo{respData.data, static_cast<uint32_t>(respData.len)};
            collectRet = resultSimpo.Deserialize();
            if (collectRet != UBSE_OK) {
                UBSE_LOG_ERROR << "query nodeId=" << nodeId << " numa import deserialize failed, "
                             << FormatRetCode(collectRet);
            } else {
                info = resultSimpo.GetImportObj();
            }
        });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send query nodeId=" << nodeId << " query numa import msg failed, " << FormatRetCode(ret);
        return ret;
    }
    return collectRet;
}

static UbseResult CheckNodeParam()
{
    auto node = UbseNodeController::GetInstance().GetCurNode();
    if (node.nodeId.empty()) {
        UBSE_LOG_ERROR << "current node not collect.";
        return UBSE_ERROR;
    }
    if (node.localState != UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_ERROR << "current node not ready.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult QueryNumaImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    std::string nodeId = utils::GetCurNodeId();
    if (CheckNodeParam() != UBSE_OK) {
        return UBSE_ERROR;
    }
    UbseMemNumaBorrowImportobjSimpo simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    size_t size = 0;
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem query numa import serialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    std::string numaImportName = simpo.GetImportObj().req.name;
    UbseMemNumaBorrowImportObj obj{};
    auto nodeMemDebtInfoMap = GetNodeMemDebtInfoMap();
    if (nodeMemDebtInfoMap.find(nodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[nodeId].numaImportObjMap.find(numaImportName) !=
            nodeMemDebtInfoMap[nodeId].numaImportObjMap.end()) {
            obj = nodeMemDebtInfoMap[nodeId].numaImportObjMap[numaImportName];
        }
    }
    UbseMemNumaBorrowImportobjSimpoPtr resultSimpo = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo();
    if (resultSimpo == nullptr) {
        UBSE_LOG_ERROR << "Malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    resultSimpo->SetUbseMemNumaBorrowImportobj(obj);
    ret = resultSimpo->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "mem numa import deserialize failed, " << FormatRetCode(ret);
        resp = {nullptr, 0, [size](uint8_t *p) noexcept {
                    SafeDeleteArray(p, size);
                }};
        return ret;
    }
    size = resultSimpo->SerializedDataSize();
    resp = {new uint8_t[size], size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    ret = memcpy_s(resp.data, resp.len, resultSimpo->SerializedData(), size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "memcpy_s failed, ErrorCode=" << ret;
    }
    return ret;
}
} // namespace ubse::mem::controller