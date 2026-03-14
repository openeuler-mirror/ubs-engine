
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mem_manager_base.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

#include <rapidjson/document.h>
#include "mp_error.h"
#include "mp_json_util.h"
#include "mp_mem_json_util.h"
#include "mp_parse_util.h"
#include "mp_sync_data_helper.h"
#include "ubse_pointer_process.h"
#include "rmrs_serialize.h"
#include "securec.h"
#include "ubse_def.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node.h"
#include "ubse_storage.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::storage;
using namespace ubse::nodeController;
using namespace rmrs::serialize;
using namespace ubse::mem::controller;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

uint32_t GetNodeInfoImmediatelyRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    auto ret = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "GetNumaInfoImmediately failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    if (numaInfos.empty()) {
        LOG_ERROR << "numaInfos is empty.";
        return MEM_POOLING_ERROR;
    }
    RmrsOutStream builder;
    builder << numaInfos;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    return MEM_POOLING_OK;
}

void PersistAntiNodeData(const MpUpdateAntiNodeParam &antiParam)
{
    std::string tempNodeAntiMapStr = antiParam.ToJson();
    UbseByteBuffer buffer;
    buffer.len = tempNodeAntiMapStr.length();
    buffer.data = new (std::nothrow) uint8_t[buffer.len];
    buffer.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    errno_t err = memcpy_s(buffer.data, buffer.len, tempNodeAntiMapStr.c_str(), tempNodeAntiMapStr.length());
    if (err != EOK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Memcpy_s anti buffer failed";
    }
    // 更新数据库
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode base start.";
    uint32_t ret = ubse::storage::UbseStoragePutData("mempooling", "_antiNode", &buffer);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Failed to store base antidata";
    }
    delete[] buffer.data;
}

uint32_t SyncAntiDataRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] SyncAntiDataRecvHandler start.";
    SyncUpdateAntiNodeParam param;
    RmrsInStream builder(req.data, req.len);
    builder >> param;
    MpUpdateAntiNodeParam antiParam;
    antiParam.nodeAntiAffinityMap = param.nodeAntiAffinityMap;
    std::string tempNodeAntiMapStr = antiParam.ToJson();
    UbseByteBuffer buffer;
    buffer.len = tempNodeAntiMapStr.length();
    buffer.data = new (std::nothrow) uint8_t[buffer.len];
    if (buffer.data == nullptr) {
        LOG_ERROR << "[MpElection][SyncData] New data failed.";
        return MEM_POOLING_ERROR;
    }
    buffer.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    errno_t err = memcpy_s(buffer.data, buffer.len, tempNodeAntiMapStr.c_str(), tempNodeAntiMapStr.length());
    if (err != EOK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Memcpy_s anti buffer failed";
    }

    // 更新数据库和更新缓存
    std::vector<std::string> nodeIdList;
    UbseRoleInfo standbyRole;
    uint32_t ret = UpdateDataBaseAndCache(buffer, antiParam, standbyRole);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] Upadate failed.";
        return MEM_POOLING_ERROR;
    }
    nodeIdList.push_back(standbyRole.nodeId);
    // 备节点上也要存
    ret = sync::MpSyncDataHelper::Instance().SyncDataToNode(nodeIdList, message::OPCODE_SYNC_ANTI_DATA_TO_NODE, req);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] SyncData to standBy failed.";
        return MEM_POOLING_ERROR;
    }
    SyncAntiNodeDataResult result = {ret};
    RmrsOutStream retBuilder;
    retBuilder << result;
    resp.len = retBuilder.GetSize();
    resp.data = retBuilder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Failed to store antidata";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode all finshed.";
    return MEM_POOLING_OK;
}

void GetNodeInfoImmediatelyResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr) {
        return;
    }
    std::vector<mempooling::exportV2::NumaInfo> &numaInfos =
        *(static_cast<std::vector<mempooling::exportV2::NumaInfo> *>(ctx));
    RmrsInStream builder(respData.data, respData.len);
    builder >> numaInfos;
    return;
}

void ParallelSendGetNodeInfo(const std::vector<std::string> &nodeList,
                             std::map<std::string, std::vector<mempooling::exportV2::NumaInfo>> &nodeInfoMap)
{
    std::vector<std::thread> threads;
    std::mutex mapMutex;

    for (const auto &node : nodeList) {
        (void)threads.emplace_back([&, node]() {
            std::vector<mempooling::exportV2::NumaInfo> numaInfos;
            UbseComEndpoint endpoint = {
                .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_GET_NODEINFO, .address = node};
            UbseByteBuffer reqData = {.data = new (std::nothrow) uint8_t[1], .len = 1, .freeFunc = nullptr};
            // 用同步接口，回调函数里记录结果
            uint32_t retRpc = UbseRpcSend(endpoint, reqData, &numaInfos, GetNodeInfoImmediatelyResHandler);
            delete[] reqData.data;
            if (retRpc == 0) {
                std::lock_guard<std::mutex> lock(mapMutex);
                nodeInfoMap[node] = numaInfos;
            }
        });
    }
    for (auto &t : threads) {
        t.join();
    }
}

uint32_t GetAllNodeInfoImmediatelyRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    // 采取在主节点查询所有节点，而不是传入，非working的还要过滤
    // 遇到smoothing状态则进行重试，间隔1s，最多重试30次
    int maxRetryTimes = 30; // 最大重试次数
    int sleepTime = 1;      // 单位秒
    int curRetryTimes = 0;
    std::vector<std::string> nodeList;
    while (curRetryTimes < maxRetryTimes) {
        bool hasSmoothingNodes = false;
        nodeList.clear();
        std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> allNodes =
            UbseNodeController::GetInstance().GetAllNodes();
        if (allNodes.empty()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get all nodes empty.";
            return MEM_POOLING_ERROR;
        }
        for (const auto &[nodeId, roleInfo] : allNodes) {
            if (roleInfo.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING) {
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "NodeId=" << nodeId << " is working.";
                nodeList.push_back(nodeId);
            } else if (roleInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "NodeId=" << nodeId << " is smoothing, retry.";
                hasSmoothingNodes = true;
                break;
            } else if (roleInfo.clusterState == UbseNodeClusterState::UBSE_NODE_INIT) {
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "NodeId=" << nodeId << " is initing, retry.";
                hasSmoothingNodes = true;
                break;
            }
        }
        if (!hasSmoothingNodes) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
        curRetryTimes++;
    }
    if (curRetryTimes >= maxRetryTimes) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "There are abnormal nodes after max retry, ubse is wrong.";
        return MEM_POOLING_ERROR;
    }
    std::map<std::string, std::vector<mempooling::exportV2::NumaInfo>> nodeInfoMap;
    ParallelSendGetNodeInfo(nodeList, nodeInfoMap);
    RmrsOutStream builder;
    builder << nodeInfoMap;
    resp.data = builder.GetBufferPointer();
    resp.len = builder.GetSize();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    return MEM_POOLING_OK;
}

uint32_t SyncAntiDataStandByRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] SyncAntiDataStandBy start.";
    SyncUpdateAntiNodeParam param;
    RmrsInStream builder(req.data, req.len);
    builder >> param;
    MpUpdateAntiNodeParam antiParam;
    antiParam.nodeAntiAffinityMap = param.nodeAntiAffinityMap;
    std::string tempNodeAntiMapStr = antiParam.ToJson();
    UbseByteBuffer buffer;
    buffer.len = tempNodeAntiMapStr.length();
    buffer.data = new (std::nothrow) uint8_t[buffer.len];
    if (buffer.data == nullptr) {
        LOG_ERROR << "[MpElection][SyncData] The nothrow failed.";
        buffer.len = 0;
        return MEM_POOLING_ERROR;
    }
    buffer.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    errno_t err = memcpy_s(buffer.data, buffer.len, tempNodeAntiMapStr.c_str(), tempNodeAntiMapStr.length());
    if (err != EOK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Memcpy_s anti buffer failed";
    }
    // 更新数据库
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode base start.";
    uint32_t ret = ubse::storage::UbseStoragePutData("mempooling", "_antiNode", &buffer);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Failed to store base antidata";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode base finshed.";
    delete[] buffer.data;
    // 更新缓存
    AntiNode::Instance().SetAntiNodeParam(antiParam);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode mem finshed.";
    SyncAntiNodeDataResult result = {ret};
    RmrsOutStream retBuilder;
    retBuilder << result;
    resp.len = retBuilder.GetSize();
    resp.data = retBuilder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Failed to store antidata";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode all finshed.";
    return MEM_POOLING_OK;
}

uint32_t UpdateDataBaseAndCache(UbseByteBuffer &buffer, const MpUpdateAntiNodeParam &antiParam,
                                ubse::election::UbseRoleInfo &standbyRole)
{
    // 更新数据库
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode base start.";
    uint32_t ret = ubse::storage::UbseStoragePutData("mempooling", "_antiNode", &buffer);
    delete[] buffer.data;
    buffer.data = nullptr;
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Failed to store base antidata";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode base finshed.";
    // 更新缓存
    AntiNode::Instance().SetAntiNodeParam(antiParam);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Update antinode mem finshed.";

    ret = ubse::election::UbseGetStandbyInfo(standbyRole);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] UbseGetStandbyInfo failed.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}
}