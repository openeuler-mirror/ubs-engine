/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_mem_api.h"

#include <arpa/inet.h>
#include <securec.h>

#include "src/sdk/include/ubs_engine.h"
#include "ubse_api_server_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_logger_inner.h"
#include "ubse_mem_api_convert.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"
#include "ubse_mgr_configuration.h"
#include "ubse_node_module.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"

namespace usbe::mem::api {
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::node;
using namespace ::api::server;
using namespace ubse::serial;
using namespace ubse::resource::mem;
using namespace ubse::election;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_API_SERVER_MID)

UbseResult UbseMemApi::Register()
{
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubse_api_server_module->RegisterIpcHandler(UBSE_NODE, UBSE_NODE_NUMA_MEM_GET, UbseSeverNodeNumaGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_FD_GET, UbseServerFdGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_FD_LIST, UbseServerFdList);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_NUMA_GET, UbseServerNumaGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, UBSE_MEM_NUMA_LIST, UbseServerNumaList);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, USBE_MEM_CLI_NODE_BORROW, UbseNodeBorrowInfoHandle);
    ret |=
        ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, USBE_MEM_CLI_BORROW_DETAIL, UbseBorrowDetailsInfoHandle);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_MEM, USBE_MEM_CLI_CHECK_STATUS, UbseCheckMemoryStatus);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of mem IPC-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseSeverNodeNumaGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.length < sizeof(uint32_t)) {
        return UBS_ERR_BUFFER_TOO_SMALL;
    }
    uint32_t slotId = 0;
    auto res = memcpy_s(&slotId, sizeof(uint32_t), req.buffer, sizeof(uint32_t));
    if (res != EOK) {
        return res;
    }
    std::vector<ubse::nodeController::UbseNumaInfo> nodeNumaMemList{};
    ubse::mem::controller::UbseNodeNumaMemGet(std::to_string(slotId), nodeNumaMemList);
    uint32_t nodeNumaMemCnt = nodeNumaMemList.size();
    UbseIpcMessage resp{};
    resp.length = sizeof(uint32_t) + nodeNumaMemCnt * UBSE_NODE_NUMA_MEM_SIZE;
    resp.buffer = static_cast<uint8_t *>(malloc(resp.length));
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t ret = UbseNumaInfoListPack(nodeNumaMemList, resp.buffer);
    if (ret != EOK) {
        UBSE_LOG_ERROR << "UbseNumeInfoList pack failed," << FormatRetCode(ret);
        free(resp.buffer);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        free(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    free(resp.buffer);
    return ret;
}

uint32_t UbseMemApi::UbseServerFdGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::string name = std::string(reinterpret_cast<char *>(req.buffer));
    ubse::mem::def::UbseMemFdDesc fdDesc;
    UbseIpcMessage resp{};
    uint32_t ret = ubse::mem::controller::UbseMemFdGet(name, fdDesc);
    if (ret != UBSE_OK) {
        return ret;
    }
    uint32_t memIdAccount = fdDesc.memIds.size();
    // 计算总需求长度
    const size_t requiredLength = UBS_MEM_MAX_NAME_LENGTH +         // name
                                  sizeof(uint32_t) +                // memid_cnt
                                  sizeof(uint64_t) * memIdAccount + // memids有效部分
                                  sizeof(uint64_t) +                // mem_size
                                  sizeof(size_t);                   // unit_size
    resp.length = requiredLength;
    resp.buffer = static_cast<uint8_t *>(malloc(resp.length));
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = UbseMemFdDescPack(fdDesc, resp.buffer);
    if (ret != UBSE_OK) {
        free(resp.buffer);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        free(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    free(resp.buffer);
    return ret;
}
uint32_t UbseMemApi::UbseServerFdList(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::vector<ubse::mem::def::UbseMemFdDesc> fdList{};
    uint32_t ret = ubse::mem::controller::UbseMemFdList(fdList);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseIpcMessage resp{};
    // 计算size
    size_t requiredLength{};
    ret = UbseMemFdDescListPackedSize(fdList, requiredLength);
    if (ret != IPC_SUCCESS) {
        return ret;
    }
    resp.length = requiredLength;
    resp.buffer = static_cast<uint8_t *>(malloc(resp.length));
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = UbseMemFdDescListPack(fdList, resp.buffer);
    if (ret != IPC_SUCCESS) {
        free(resp.buffer);
        return ret;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        free(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    free(resp.buffer);
    return ret;
}

uint32_t UbseMemApi::UbseServerNumaGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::string name = std::string(reinterpret_cast<char *>(req.buffer));
    UbseIpcMessage resp{};
    // 计算总长度
    const size_t requiredLength = UBS_MEM_MAX_NAME_LENGTH + // name
                                  sizeof(int64_t);          // numaid
    resp.length = requiredLength;
    resp.buffer = static_cast<uint8_t *>(malloc(resp.length));
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::mem::def::UbseMemNumaDesc memNumaDesc{};
    uint32_t ret = ubse::mem::controller::UbseMemNumaGet(name, memNumaDesc);
    if (ret != UBSE_OK) {
        free(resp.buffer);
        return ret;
    }
    ret = UbseMemNumaDescPack(memNumaDesc, resp.buffer);
    if (ret != UBSE_OK) {
        free(resp.buffer);
        return ret;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        free(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    free(resp.buffer);
    return ret;
}
uint32_t UbseMemApi::UbseServerNumaList(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    std::vector<ubse::mem::def::UbseMemNumaDesc> cMemNumaDescList{};
    uint32_t ret = ubse::mem::controller::UbseMemNumaList(cMemNumaDescList);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseIpcMessage resp{};
    // 计算总长度
    const size_t memNumaDescLength = UBS_MEM_MAX_NAME_LENGTH + // name
                                     sizeof(int64_t);          // numaid
    resp.length = sizeof(uint32_t) + memNumaDescLength * cMemNumaDescList.size();
    resp.buffer = static_cast<uint8_t *>(malloc(resp.length));
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = UbseMemNumaDescListPack(cMemNumaDescList, resp.buffer);
    if (ret != UBSE_OK) {
        free(resp.buffer);
        return ret;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        free(resp.buffer);
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
    }
    free(resp.buffer);
    return ret;
}

struct UbseNodeBorrow {
    std::string ubseBorrowNodeId;
    std::string ubseLendNodeId;
    std::string ubseSize;
    UbseMemState ubseLendStatue{ UBSE_MEM_STATE_INIT };
    UbseMemState ubseBorrowStatue{ UBSE_MEM_STATE_INIT };
};

struct MemoryInfo {
    std::string ubseName;
    std::string ubseType;
    std::string ubseBorrowNode;
    std::string ubseBorrowSize;
    std::string ubseLendNode;
    std::string ubseLendSize;
    std::string ubseStatus;
};

std::unordered_map<UbseMemState, std::string> UbseMemStateMap = {
    { UBSE_MEM_EXPORT_RUNNING, "exporting" },      { UBSE_MEM_EXPORT_SUCCESS, "done" },
    { UBSE_MEM_EXPORT_DESTROYING, "unexporting" }, { UBSE_MEM_EXPORT_DESTROYED, "done" },
    { UBSE_MEM_IMPORT_RUNNING, "importing" },      { UBSE_MEM_IMPORT_SUCCESS, "done" },
    { UBSE_MEM_IMPORT_DESTROYING, "unimporting" }, { UBSE_MEM_IMPORT_DESTROYED, "done" }
};

static const int ONE_M = 1024 * 1024;

static std::string ConvertStrToMbStr(const char *data)
{
    if (data == nullptr) {
        return std::to_string(0);
    }
    char *remain = nullptr;
    uint64_t num = std::strtoull(data, &remain, 10L);
    if (remain == data) {
        return std::to_string(0);
    }
    return std::to_string(num / ONE_M);
}

std::string UbseAddStringsNumber(const std::string &num1, const std::string &num2)
{
    std::string result;
    std::string reversed_num1(num1.rbegin(), num1.rend());
    std::string reversed_num2(num2.rbegin(), num2.rend());

    int carry = 0;
    size_t max_length = std::max(reversed_num1.size(), reversed_num2.size());

    for (size_t i = 0; i < max_length; ++i) {
        int digit1 = (i < reversed_num1.size()) ? reversed_num1[i] - '0' : 0;
        int digit2 = (i < reversed_num2.size()) ? reversed_num2[i] - '0' : 0;

        int sum = digit1 + digit2 + carry;
        carry = sum / 10;
        result += (sum % 10) + '0';
    }

    if (carry > 0) {
        result += carry + '0';
    }

    std::reverse(result.begin(), result.end());

    while (result.size() > 1 && result[0] == '0') {
        result.erase(0, 1);
    }
    return result;
}

static void ProcessCollectNumaExportSize(
    const std::pair<std::string, ubse::mem::obj::UbseMemNumaBorrowExportObj> &numa_export_obj_map,
    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> &ubse_node_borrows)
{
    UBSE_LOG_DEBUG << "name=" << numa_export_obj_map.first << ",numa_export_status=" <<
        numa_export_obj_map.second.status.state << ",borrow_relation=" <<
        numa_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        numa_export_obj_map.second.algoResult.importNumaInfos[0].nodeId;
    const std::string borrow_name = numa_export_obj_map.first + "numa";
    if (numa_export_obj_map.second.algoResult.exportNumaInfos.empty() ||
        numa_export_obj_map.second.algoResult.importNumaInfos.empty()) {
        return;
    }
    const std::string borrow_relation = numa_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        numa_export_obj_map.second.algoResult.importNumaInfos[0].nodeId;

    if (ubse_node_borrows.find(borrow_relation) == ubse_node_borrows.end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ numa_export_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            numa_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(numa_export_obj_map.second.algoResult.exportNumaInfos[0].size),
            numa_export_obj_map.second.status.state, UBSE_MEM_STATE_INIT };
    } else if (ubse_node_borrows[borrow_relation].find(borrow_name) == ubse_node_borrows[borrow_relation].end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ numa_export_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            numa_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(numa_export_obj_map.second.algoResult.exportNumaInfos[0].size),
            numa_export_obj_map.second.status.state, UBSE_MEM_STATE_INIT };
    } else {
        ubse_node_borrows[borrow_relation][borrow_name].ubseLendStatue = numa_export_obj_map.second.status.state;
    }
}

static void ProcessCollectNumaImportSize(
    const std::pair<std::string, ubse::mem::obj::UbseMemNumaBorrowImportObj> &numa_import_obj_map,
    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> &ubse_node_borrows)
{
    UBSE_LOG_DEBUG << "name=" << numa_import_obj_map.first << ",numa_import_status=" <<
        numa_import_obj_map.second.status.state << ",borrow_relation=" <<
        numa_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        numa_import_obj_map.second.algoResult.importNumaInfos[0].nodeId;
    const std::string borrow_name = numa_import_obj_map.first + "numa";
    if (numa_import_obj_map.second.algoResult.exportNumaInfos.empty() ||
        numa_import_obj_map.second.algoResult.importNumaInfos.empty()) {
        return;
    }
    const std::string borrow_relation = numa_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        numa_import_obj_map.second.algoResult.importNumaInfos[0].nodeId;

    if (ubse_node_borrows.find(borrow_relation) == ubse_node_borrows.end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ numa_import_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            numa_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(numa_import_obj_map.second.algoResult.importNumaInfos[0].size), UBSE_MEM_STATE_INIT,
            numa_import_obj_map.second.status.state };
    } else if (ubse_node_borrows[borrow_relation].find(borrow_name) == ubse_node_borrows[borrow_relation].end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ numa_import_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            numa_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(numa_import_obj_map.second.algoResult.importNumaInfos[0].size), UBSE_MEM_STATE_INIT,
            numa_import_obj_map.second.status.state };
    } else {
        ubse_node_borrows[borrow_relation][borrow_name].ubseBorrowStatue = numa_import_obj_map.second.status.state;
    }
}

static void ProcessCollectFdExportSize(
    const std::pair<std::string, ubse::mem::obj::UbseMemFdBorrowExportObj> &fd_export_obj_map,
    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> &ubse_node_borrows)
{
    UBSE_LOG_DEBUG << "name=" << fd_export_obj_map.first << ",fd_export_status=" <<
        fd_export_obj_map.second.status.state << ",borrow_relation=" <<
        fd_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        fd_export_obj_map.second.algoResult.importNumaInfos[0].nodeId;
    const std::string borrow_name = fd_export_obj_map.first + "fd";
    if (fd_export_obj_map.second.algoResult.exportNumaInfos.empty() ||
        fd_export_obj_map.second.algoResult.importNumaInfos.empty()) {
        return;
    }
    const std::string borrow_relation = fd_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        fd_export_obj_map.second.algoResult.importNumaInfos[0].nodeId;

    if (ubse_node_borrows.find(borrow_relation) == ubse_node_borrows.end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ fd_export_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            fd_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(fd_export_obj_map.second.algoResult.exportNumaInfos[0].size),
            fd_export_obj_map.second.status.state, UBSE_MEM_STATE_INIT };
    } else if (ubse_node_borrows[borrow_relation].find(borrow_name) == ubse_node_borrows[borrow_relation].end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ fd_export_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            fd_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(fd_export_obj_map.second.algoResult.exportNumaInfos[0].size),
            fd_export_obj_map.second.status.state, UBSE_MEM_STATE_INIT };
    } else {
        ubse_node_borrows[borrow_relation][borrow_name].ubseLendStatue = fd_export_obj_map.second.status.state;
    }
}

static void ProcessCollectFdImportSize(
    const std::pair<std::string, ubse::mem::obj::UbseMemFdBorrowImportObj> &fd_import_obj_map,
    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> &ubse_node_borrows)
{
    UBSE_LOG_DEBUG << "name=" << fd_import_obj_map.first << ",fd_import_status=" <<
        fd_import_obj_map.second.status.state << ",borrow_relation=" <<
        fd_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        fd_import_obj_map.second.algoResult.importNumaInfos[0].nodeId;

    const std::string borrow_name = fd_import_obj_map.first + "fd";
    if (fd_import_obj_map.second.algoResult.exportNumaInfos.empty() ||
        fd_import_obj_map.second.algoResult.importNumaInfos.empty()) {
        return;
    }
    const std::string borrow_relation = fd_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId + "_" +
        fd_import_obj_map.second.algoResult.importNumaInfos[0].nodeId;

    // It is unusual to have imports without exports.
    if (ubse_node_borrows.find(borrow_relation) == ubse_node_borrows.end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ fd_import_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            fd_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(fd_import_obj_map.second.algoResult.importNumaInfos[0].size), UBSE_MEM_STATE_INIT,
            fd_import_obj_map.second.status.state };
    } else if (ubse_node_borrows[borrow_relation].find(borrow_name) == ubse_node_borrows[borrow_relation].end()) {
        ubse_node_borrows[borrow_relation][borrow_name] =
            UbseNodeBorrow{ fd_import_obj_map.second.algoResult.importNumaInfos[0].nodeId,
            fd_import_obj_map.second.algoResult.exportNumaInfos[0].nodeId,
            std::to_string(fd_import_obj_map.second.algoResult.importNumaInfos[0].size), UBSE_MEM_STATE_INIT,
            fd_import_obj_map.second.status.state };
    } else {
        ubse_node_borrows[borrow_relation][borrow_name].ubseBorrowStatue = fd_import_obj_map.second.status.state;
    }
}

static bool UbseGetNodeBorrowInfo(
    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> &ubse_node_borrows)
{
    NodeMemDebtInfoMap ubse_node_mem_debt_info{};
    auto ret = UbseGetMemDebtInfo("", ubse_node_mem_debt_info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to obtain debt information of all nodes.";
        return false;
    }
    for (const auto &debt_obj : ubse_node_mem_debt_info) {
        for (const auto &numa_export_obj_map : debt_obj.second.numaExportObjMap) {
            ProcessCollectNumaExportSize(numa_export_obj_map, ubse_node_borrows);
        }
        for (auto &numa_import_obj_map : debt_obj.second.numaImportObjMap) {
            ProcessCollectNumaImportSize(numa_import_obj_map, ubse_node_borrows);
        }
        for (const auto &fd_export_obj_map : debt_obj.second.fdExportObjMap) {
            ProcessCollectFdExportSize(fd_export_obj_map, ubse_node_borrows);
        }
        for (const auto &fd_import_obj_map : debt_obj.second.fdImportObjMap) {
            ProcessCollectFdImportSize(fd_import_obj_map, ubse_node_borrows);
        }
    }
    return true;
}

static uint32_t UbseBuildResponseFromNodeBorrowInfo(
    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> &ubse_node_borrows,
    UbseIpcMessage &res, UbseSerialization &ubse_serial)
{
    std::vector<UbseNodeBorrow> results;
    std::string size = "0";
    for (const auto &account : ubse_node_borrows) {
        UBSE_LOG_DEBUG << "ubse_node_borrows";
        UBSE_LOG_DEBUG << account.first;
        for (const auto &account_info : account.second) {
            UBSE_LOG_DEBUG << "ubseBorrowNodeId:" << account_info.second.ubseBorrowNodeId << "ubseBorrowStatue:" <<
                account_info.second.ubseBorrowStatue << "ubseLendNodeId:" << account_info.second.ubseLendNodeId <<
                "ubseLendStatue:" << account_info.second.ubseLendStatue << "ubseSize:" <<
                std::string(ConvertStrToMbStr(account_info.second.ubseSize.c_str()));
            if (account_info.second.ubseBorrowStatue == UBSE_MEM_IMPORT_SUCCESS &&
                account_info.second.ubseLendStatue == UBSE_MEM_EXPORT_SUCCESS) {
                size = UbseAddStringsNumber(size, std::string(ConvertStrToMbStr(account_info.second.ubseSize.c_str())));
            }
        }
        if (!account.second.empty() && size != "0") {
            results.emplace_back(UbseNodeBorrow{ account.second.begin()->second.ubseBorrowNodeId,
                account.second.begin()->second.ubseLendNodeId, size });
        }
        size = "0";
    }
    ubse_serial << right_v<std::size_t>(results.size());
    for (const auto &ubse_node_borrow : results) {
        ubse_serial <<
            std::string("node-" + ubse_node_borrow.ubseBorrowNodeId + "(" + ubse_node_borrow.ubseBorrowNodeId + ")");
        ubse_serial <<
            std::string("node-" + ubse_node_borrow.ubseLendNodeId + "(" + ubse_node_borrow.ubseLendNodeId + ")");
        ubse_serial << ubse_node_borrow.ubseSize;
    }
    if (!ubse_serial.Check()) {
        UBSE_LOG_ERROR << "Serialization of topo response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    res.buffer = ubse_serial.GetBuffer();
    res.length = static_cast<uint32_t>(ubse_serial.GetLength());
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseNodeBorrowInfoHandle(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Node borrow IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }

    std::unordered_map<std::string, std::unordered_map<std::string, UbseNodeBorrow>> ubse_node_borrows{};
    if (!UbseGetNodeBorrowInfo(ubse_node_borrows)) {
        return UBSE_ERROR_NULL_INFO;
    }
    UbseIpcMessage res{};
    UbseSerialization ubse_serial;
    auto ret = UbseBuildResponseFromNodeBorrowInfo(ubse_node_borrows, res, ubse_serial);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = ubse_api_server_module->SendResponse(IPC_SUCCESS, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " NodeBorrowHandle response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static void ProcessNumaImport(
    const std::pair<std::string, ubse::mem::obj::UbseMemNumaBorrowImportObj> &numa_import_obj_map,
    std::unordered_map<std::string, MemoryInfo> &ubse_borrow_details)
{
    const std::string key = numa_import_obj_map.first + "numa";
    // It is unusual to have imports without exports.
    if (ubse_borrow_details.find(key) == ubse_borrow_details.end()) {
        ubse_borrow_details.insert({ key, MemoryInfo{ numa_import_obj_map.first, "numa", "", "", "", "", "fault" } });
    } else if (UbseMemStateMap.find(numa_import_obj_map.second.status.state) != UbseMemStateMap.end()) {
        // Otherwise, fill in the status of the import node.
        ubse_borrow_details[key].ubseStatus = UbseMemStateMap[numa_import_obj_map.second.status.state];
    } else {
        // If none of these states are present, it is in an unknown state.
        ubse_borrow_details[key].ubseStatus = "unknown";
    }

    if (!numa_import_obj_map.second.algoResult.importNumaInfos.empty()) {
        auto &info = ubse_borrow_details[key];
        info.ubseBorrowNode = numa_import_obj_map.second.algoResult.importNumaInfos[0].nodeId;
        info.ubseBorrowSize = std::to_string(numa_import_obj_map.second.algoResult.importNumaInfos[0].size);
    }

    UBSE_LOG_DEBUG << "name=" << numa_import_obj_map.first << ",numa_import_status=" <<
        numa_import_obj_map.second.status.state;
}

static void ProcessNumaExport(const std::pair<std::string, UbseMemNumaBorrowExportObj> &numa_export_obj_map,
    std::unordered_map<std::string, MemoryInfo> &ubse_borrow_details)
{
    const std::string key = numa_export_obj_map.first + "numa";
    ubse_borrow_details.insert({ key, MemoryInfo{ numa_export_obj_map.first, "numa", "", "", "", "", "unknown" } });

    if (!numa_export_obj_map.second.algoResult.exportNumaInfos.empty()) {
        auto &info = ubse_borrow_details[key];
        info.ubseLendNode = numa_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId;
        info.ubseLendSize = std::to_string(numa_export_obj_map.second.algoResult.exportNumaInfos[0].size);
    }
    // Fill in the status of the export node here.
    if (UbseMemStateMap.find(numa_export_obj_map.second.status.state) != UbseMemStateMap.end()) {
        ubse_borrow_details[key].ubseStatus = UbseMemStateMap[numa_export_obj_map.second.status.state];
    }

    UBSE_LOG_DEBUG << "name=" << numa_export_obj_map.first << ",numa_export_status=" <<
        numa_export_obj_map.second.status.state;
}

static void ProcessFdImport(const std::pair<std::string, UbseMemFdBorrowImportObj> &fd_import_obj_map,
    std::unordered_map<std::string, MemoryInfo> &ubse_borrow_details)
{
    const std::string key = fd_import_obj_map.first + "fd";
    // It is unusual to have imports without exports.
    if (ubse_borrow_details.find(key) == ubse_borrow_details.end()) {
        ubse_borrow_details.insert({ key, MemoryInfo{ fd_import_obj_map.first, "fd", "", "", "", "", "fault" } });
    } else if (UbseMemStateMap.find(fd_import_obj_map.second.status.state) != UbseMemStateMap.end()) {
        // Otherwise, fill in the status of the import node.
        ubse_borrow_details[key].ubseStatus = UbseMemStateMap[fd_import_obj_map.second.status.state];
    } else {
        // If none of these states are present, it is in an unknown state.
        ubse_borrow_details[key].ubseStatus = "unknown";
    }

    if (!fd_import_obj_map.second.algoResult.importNumaInfos.empty()) {
        auto &info = ubse_borrow_details[key];
        info.ubseBorrowNode = fd_import_obj_map.second.algoResult.importNumaInfos[0].nodeId;
        info.ubseBorrowSize = std::to_string(fd_import_obj_map.second.algoResult.importNumaInfos[0].size);
    }

    UBSE_LOG_DEBUG << "name=" << fd_import_obj_map.first << ",fd_export_status=" <<
        fd_import_obj_map.second.status.state;
}

static void ProcessFdExport(const std::pair<std::string, UbseMemFdBorrowExportObj> &fd_export_obj_map,
    std::unordered_map<std::string, MemoryInfo> &ubse_borrow_details)
{
    const std::string key = fd_export_obj_map.first + "fd";
    ubse_borrow_details.insert({ key, MemoryInfo{ fd_export_obj_map.first, "fd", "", "", "", "", "unknown" } });

    if (!fd_export_obj_map.second.algoResult.exportNumaInfos.empty()) {
        auto &info = ubse_borrow_details[key];
        info.ubseLendNode = fd_export_obj_map.second.algoResult.exportNumaInfos[0].nodeId;
        info.ubseLendSize = std::to_string(fd_export_obj_map.second.algoResult.exportNumaInfos[0].size);
    }

    // Fill in the status of the export node here.
    if (UbseMemStateMap.find(fd_export_obj_map.second.status.state) != UbseMemStateMap.end()) {
        ubse_borrow_details[key].ubseStatus = UbseMemStateMap[fd_export_obj_map.second.status.state];
    }

    UBSE_LOG_DEBUG << "name=" << fd_export_obj_map.first << ",fd_export_status=" <<
        fd_export_obj_map.second.status.state;
}

static bool UbseGetBorrowDetailsInfo(std::unordered_map<std::string, MemoryInfo> &ubse_borrow_details)
{
    NodeMemDebtInfoMap ubse_node_mem_debt_info{};
    auto ret = UbseGetMemDebtInfo("", ubse_node_mem_debt_info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to obtain debt information of all nodes.";
        return false;
    }

    for (const auto &debt_obj : ubse_node_mem_debt_info) {
        for (const auto &numa_export_obj_map : debt_obj.second.numaExportObjMap) {
            ProcessNumaExport(numa_export_obj_map, ubse_borrow_details);
        }
        for (const auto &fd_export_obj_map : debt_obj.second.fdExportObjMap) {
            ProcessFdExport(fd_export_obj_map, ubse_borrow_details);
        }
    }
    for (const auto &debt_obj : ubse_node_mem_debt_info) {
        for (const auto &numa_import_obj_map : debt_obj.second.numaImportObjMap) {
            ProcessNumaImport(numa_import_obj_map, ubse_borrow_details);
        }
        for (const auto &fd_import_obj_map : debt_obj.second.fdImportObjMap) {
            ProcessFdImport(fd_import_obj_map, ubse_borrow_details);
        }
    }
    return true;
}

static uint32_t UbseBuildResponseFromBorrowDetailsInfo(std::unordered_map<std::string, MemoryInfo> &ubse_borrow_details,
    UbseIpcMessage &res, UbseSerialization &ubse_serial)
{
    ubse_serial << right_v<std::size_t>(ubse_borrow_details.size());
    for (const auto &ubse_borrows_detail : ubse_borrow_details) {
        ubse_serial << ubse_borrows_detail.second.ubseName << ubse_borrows_detail.second.ubseType <<
            (ubse_borrows_detail.second.ubseBorrowNode.empty() ? std::string("") :
                                                                 std::string("node-" +
            ubse_borrows_detail.second.ubseBorrowNode + "(" + ubse_borrows_detail.second.ubseBorrowNode + ")")) <<
            ubse_borrows_detail.second.ubseBorrowSize <<
            (ubse_borrows_detail.second.ubseLendNode.empty() ? std::string("") :
                                                               std::string("node-" +
            ubse_borrows_detail.second.ubseLendNode + "(" + ubse_borrows_detail.second.ubseLendNode + ")")) <<
            ubse_borrows_detail.second.ubseLendSize << ubse_borrows_detail.second.ubseStatus;
    }
    if (!ubse_serial.Check()) {
        UBSE_LOG_ERROR << "Serialization of topo response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    res.buffer = ubse_serial.GetBuffer();
    res.length = static_cast<uint32_t>(ubse_serial.GetLength());
    return UBSE_OK;
}

uint32_t UbseMemApi::UbseBorrowDetailsInfoHandle(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Borrow details IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }

    std::unordered_map<std::string, MemoryInfo> ubse_borrows_details{};
    if (!UbseGetBorrowDetailsInfo(ubse_borrows_details)) {
        return UBSE_ERROR_NULL_INFO;
    }
    UbseIpcMessage res{};
    UbseSerialization ubse_serial;
    auto ret = UbseBuildResponseFromBorrowDetailsInfo(ubse_borrows_details, res, ubse_serial);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = ubse_api_server_module->SendResponse(IPC_SUCCESS, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " BorrowDetailsHandle response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseClusterList(std::vector<ubse::nodeController::UbseNodeInfo> &nodeList)
{
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos =
        ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfos.empty()) {
        return;
    }
    std::vector<ubse::nodeController::UbseNodeInfo> staticNodeInfos =
        ubse::nodeController::UbseNodeController::GetInstance().GetStaticNodeInfo();
    nodeList.reserve(std::max(nodeInfos.size(), staticNodeInfos.size()));
    std::transform(nodeInfos.begin(), nodeInfos.end(), std::back_inserter(nodeList),
        [](auto &kv) { return kv.second; });
    // 加入静态节点信息
    for (auto &nodeInfo : staticNodeInfos) {
        if (nodeInfos.find(nodeInfo.nodeId) == nodeInfos.end()) {
            ConvertStrToUint32(nodeInfo.nodeId, nodeInfo.slotId);
            nodeList.emplace_back(nodeInfo);
        }
    }
    std::sort(nodeList.begin(), nodeList.end(),
        [](ubse::nodeController::UbseNodeInfo &l, ubse::nodeController::UbseNodeInfo &r) {
            return l.slotId < r.slotId;
        });
}

uint32_t UbseMemApi::UbseCheckMemoryStatus(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Cluster IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::vector<ubse::nodeController::UbseNodeInfo> nodeList;
    UbseClusterList(nodeList);
    UbseSerialization ubseSerial;
    ubseSerial << (right_v<std::size_t>(nodeList.size()));
    // hostName(slotId), status;
    for (const auto &node : nodeList) {
        UBSE_LOG_INFO << "hostname=" << node.hostName << ", slotId=" << node.slotId << ", clusterState=" <<
            static_cast<uint32_t>(node.clusterState);
        bool isOnline = node.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
            node.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
        ubseSerial <<
            ((!isOnline || node.hostName.empty() ? "-" : node.hostName) + "(" + std::to_string(node.slotId) + ")");
        // 平滑对账和正常工作的时候ok
        ubseSerial << (isOnline ? "ok" : "nok");
    }
    if (!ubseSerial.Check()) {
        UBSE_LOG_ERROR << "Serialization of cluster response info failed";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage res{ ubseSerial.GetBuffer(), static_cast<uint32_t>(ubseSerial.GetLength()) };
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, res);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "CheckMemoryStatus response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace usbe::mem::api