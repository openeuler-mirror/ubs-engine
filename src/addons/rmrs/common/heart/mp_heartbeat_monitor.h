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

#ifndef MP_HEART_H
#define MP_HEART_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ubse_com.h"
#include "ubse_logger.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_module.h"
#include "mp_sync_data_helper.h"

namespace mempooling::heart {

using namespace ubse::com;
using namespace ubse::log;
using namespace mempooling::message;

class MpHeartBeatMonitor {
public:
    static MpHeartBeatMonitor& Instance();
    MpResult Init();
    MpResult AddFaultNode(const std::string& nodeId);
    MpResult DelFaultNode(const std::string& nodeId);
    void CurrentNodeFault();
    void CurrentNodeRecover();
    std::vector<std::string> GetFaultNodeVec();
    bool FromJson(const std::string& jsonStr);
    std::string ToJson();

private:
    void ListenLoop();
    void GetHeartBeat();
    std::vector<std::string> faultNodeVec;
    std::string currentNode;
    std::mutex mutex;
    int retry = 3;
    std::atomic<bool> running;
    bool fault = false;
    std::thread* pThread = nullptr;
};

uint32_t FaultNodeNotify(UbseByteBuffer& buffer);
uint32_t FaultNodeGetData(UbseByteBuffer& buffer);

uint32_t AddFaultNodeRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void AddFaultNodeResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
uint32_t DelFaultNodeRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void DelFaultNodeResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

class MpHeartBeatSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_ADD_FAULT_NODE};
        MpResult ret = UbseRegRpcService(endpoint, AddFaultNodeRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpHeartBeat][MpHeartBeat] AddFaultNodeRecvHandler reg failed res: " << ret << ".";
            return MEM_POOLING_ERROR;
        }

        endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_DEL_FAULT_NODE};
        ret = UbseRegRpcService(endpoint, DelFaultNodeRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpHeartBeat][MpHeartBeat] DelFaultNodeRecvHandler reg failed res: " << ret << ".";
            return MEM_POOLING_ERROR;
        }
        ret = MpHeartBeatMonitor::Instance().Init();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return;
    }
    const std::string Name() override
    {
        return "HeartBeat";
    }
};

} // namespace mempooling::heart

#endif