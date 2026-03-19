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

#include "mp_heartbeat_monitor.h"

#include <chrono>
#include <thread>

#include "securec.h"
#include "ubse_election.h"
#include "ubse_com.h"
#include "mp_error.h"
#include "mp_configuration.h"
#include "mp_json_util.h"
#include "mempooling_message.h"

namespace mempooling::heart {
using namespace ubse::election;
using namespace ubse::com;
using namespace mempooling::message;

MpHeartBeatMonitor& MpHeartBeatMonitor::Instance()
{
    static MpHeartBeatMonitor instance;
    return instance;
}

MpResult MpHeartBeatMonitor::Init()
{
    UbseRoleInfo currentNodeInfo;
    auto ret = UbseGetCurrentNodeInfo(currentNodeInfo);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpHeartBeat][MpHeartBeat] UbseGetCurrentNodeInfo failed.";
        return MEM_POOLING_ERROR;
    }
    this->currentNode = currentNodeInfo.nodeId;
    running = true;
    pThread = new std::thread([this]() {
        this->ListenLoop();
    });
    pThread->detach();
    return MEM_POOLING_OK;
}

static const int HEARTBEAT_CYCLE = 30;
static const int RETRY_CYCLE = 5;

void MpHeartBeatMonitor::GetHeartBeat()
{
    TurboByteBuffer params;
    TurboByteBuffer result;
    params.len = 1;
    params.data = new (std::nothrow) uint8_t[params.len];
    if (!params.data) { // 判空
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat] Memory allocation failed for params.";
        return;
    }
    // 防止野指针
    result.data = nullptr;
    result.len = 0;

    MpResult isConnect = MempoolingMessage::osturboFunctionCaller("HeartBeatRecvHandler", params, result);
    if (result.data) {
        delete[] result.data;
        result.data = nullptr;
    }
    if (isConnect != 0 && !fault) {
        int times = 0;
        for (; times < retry; times++) {
            std::this_thread::sleep_for(std::chrono::seconds(RETRY_CYCLE));
            // 每次调用前清空
            result.data = nullptr;
            result.len = 0;
            if (MempoolingMessage::osturboFunctionCaller("HeartBeatRecvHandler", params, result) == 0) {
                if (result.data) {
                    delete[] result.data;
                    result.data = nullptr;
                }
                break;
            }
            if (result.data) {
                delete[] result.data;
                result.data = nullptr;
            }
        }
        if (times == retry) {
            CurrentNodeFault();
        }
    }
    if (isConnect == 0 && fault) {
        CurrentNodeRecover();
    }
    delete[] params.data;
    params.data = nullptr;
    return;
}

void MpHeartBeatMonitor::ListenLoop()
{
    while (running) {
        GetHeartBeat();
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_CYCLE));
    }
    return ;
}

MpResult MpHeartBeatMonitor::AddFaultNode(const std::string &nodeId)
{
    std::unique_lock<std::mutex> locker(mutex);
    for (auto it = faultNodeVec.begin(); it != faultNodeVec.end(); it++) {
        if (*it == nodeId) {
            return MEM_POOLING_ERROR;
        }
    }
    this->faultNodeVec.push_back(nodeId);
    return MEM_POOLING_OK;
}

MpResult MpHeartBeatMonitor::DelFaultNode(const std::string &nodeId)
{
    std::unique_lock<std::mutex> locker(mutex);
    for (auto it = faultNodeVec.begin(); it != faultNodeVec.end(); it++) {
        if (*it == nodeId) {
            faultNodeVec.erase(it);
            break;
        }
    }
    return MEM_POOLING_OK;
}

void MpHeartBeatMonitor::CurrentNodeFault()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat]" << currentNode << " error.";
    fault = true;

    UbseRoleInfo roleInfo;
    if (UbseGetMasterInfo(roleInfo) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat] UbseGetMasterInfo failed.";
        return ;
    }

    UbseComEndpoint endpoint_ms = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_ADD_FAULT_NODE, .address = roleInfo.nodeId};
    UbseByteBuffer reqData;

    std::vector<std::string> vec;
    vec.push_back(currentNode);
    std::string jsonStr;
    if (!JsonUtil::RackMemConvertVector2JsonStr(vec, jsonStr)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpHeartBeat][MpHeartBeat] RackMemConvertVector2JsonStr failed.";
        return ;
    }
    reqData.len = jsonStr.length();
    reqData.data = new (std::nothrow) uint8_t[reqData.len];
    if (!reqData.data) { // 判空
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat] Memory allocation failed.";
        return;
    }
    if (memcpy_s(reqData.data, reqData.len, jsonStr.c_str(), reqData.len) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat] Memcpy_s failed.";
        delete[] reqData.data;
        return ;
    }
    void *ctx;
    auto ret = UbseRpcSend(endpoint_ms, reqData, ctx, AddFaultNodeResHandler);
    delete[] reqData.data;
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat] UbseRpcSend failed.";
    }
    return ;
}

void MpHeartBeatMonitor::CurrentNodeRecover()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat]" << currentNode << " recover.";
    fault = false;

    UbseRoleInfo roleInfo;
    if (UbseGetMasterInfo(roleInfo) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat] UbseGetMasterInfo failed.";
        return ;
    }

    UbseComEndpoint endpoint_ms = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_DEL_FAULT_NODE, .address = roleInfo.nodeId};
    UbseByteBuffer reqData;

    std::vector<std::string> vec;
    vec.push_back(currentNode);
    std::string jsonStr;
    if (!JsonUtil::RackMemConvertVector2JsonStr(vec, jsonStr)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat][MpHeartBeat] UbseRpcSend failed.";
        return ;
    }
    reqData.len = jsonStr.length();
    reqData.data = new (std::nothrow) uint8_t[reqData.len];
    if (!reqData.data) { // 判空
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpHeartBeat] Memory allocation failed for raqData.";
        return;
    }
    if (memcpy_s(reqData.data, reqData.len, jsonStr.c_str(), reqData.len) != 0) {
        delete[] reqData.data;
        return ;
    }
    void *ctx;
    auto ret = UbseRpcSend(endpoint_ms, reqData, ctx, DelFaultNodeResHandler);
    delete[] reqData.data;
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpHeartBeat][MpHeartBeat] UbseRpcSend failed.";
    }
    return ;
}

std::vector<std::string> MpHeartBeatMonitor::GetFaultNodeVec()
{
    std::unique_lock<std::mutex> locker(mutex);
    return faultNodeVec;
}

bool MpHeartBeatMonitor::FromJson(const std::string& jsonStr)
{
    std::unique_lock<std::mutex> locker(mutex);
    std::vector<std::string> vec;
    if (!JsonUtil::RackMemConvertJsonStr2Vec(jsonStr, vec)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpHeartBeat][MpHeartBeat] RackMemConvertJsonStr2Vec failed.";
        return false;
    }
    this->faultNodeVec = vec;
    return true;
}

std::string MpHeartBeatMonitor::ToJson()
{
    std::unique_lock<std::mutex> locker(mutex);
    std::string jsonStr;
    if (!JsonUtil::RackMemConvertVector2JsonStr(this->faultNodeVec, jsonStr)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpHeartBeat][MpHeartBeat] RackMemConvertVector2JsonStr failed.";
        return "";
    }
    return jsonStr;
}

uint32_t FaultNodeNotify(UbseByteBuffer &buffer)
{
    std::string jsonStr(reinterpret_cast<char *>(buffer.data), buffer.len);
    return MpHeartBeatMonitor::Instance().FromJson(jsonStr);
}

uint32_t FaultNodeGetData(UbseByteBuffer &buffer)
{
    std::string jsonStr = MpHeartBeatMonitor::Instance().ToJson();
    if (jsonStr.length() == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][SyncData] GetData failed, MpHeartBeatMonitor ToJson failed.";
        return MEM_POOLING_ERROR;
    }
    buffer.len = jsonStr.length();
    buffer.data = new (std::nothrow) uint8_t[buffer.len];
    if (buffer.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][SyncData] Failed to allocate memory, size=" << buffer.len << ".";
        return MEM_POOLING_ERROR;
    }

    buffer.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (memcpy_s(buffer.data, buffer.len, jsonStr.c_str(), buffer.len) != EOK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MpElection][SyncData] Memcpy_s failed.";
        buffer.freeFunc(buffer.data);
        buffer.data = nullptr;
        buffer.len = 0;
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

uint32_t AddFaultNodeRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    std::string jsonStr((char *)req.data, req.len);
    std::vector<std::string> vec;
    if (!JsonUtil::RackMemConvertJsonStr2Vec(jsonStr, vec)) {
        return MEM_POOLING_ERROR;
    }
    for (const auto &nodeId : vec) {
        mempooling::heart::MpHeartBeatMonitor::Instance().AddFaultNode(nodeId);
    }
    jsonStr = mempooling::heart::MpHeartBeatMonitor::Instance().ToJson();

    resp.len = 1;
    resp.data = new (std::nothrow) uint8_t[resp.len];
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Failed to allocate memory, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    return MEM_POOLING_OK;
}

void AddFaultNodeResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    return;
}

uint32_t DelFaultNodeRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    std::string jsonStr((char *)req.data, req.len);
    std::vector<std::string> vec;
    if (!JsonUtil::RackMemConvertJsonStr2Vec(jsonStr, vec)) {
        return MEM_POOLING_ERROR;
    }
    for (const auto &nodeId : vec) {
        mempooling::heart::MpHeartBeatMonitor::Instance().DelFaultNode(nodeId);
    }
    jsonStr = mempooling::heart::MpHeartBeatMonitor::Instance().ToJson();
    resp.len = 1;
    resp.data = new (std::nothrow) uint8_t[resp.len];
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Failed to allocate memory, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    return MEM_POOLING_OK;
}

void DelFaultNodeResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    return;
}

}