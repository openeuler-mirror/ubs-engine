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

#ifndef FAULT_MEMID_HELPER_H
#define FAULT_MEMID_HELPER_H

#include <iostream>
#include <string>
#include "mp_error.h"
#include "fault_memid_module.h"
#include "ubse_logger.h"
#include "ubse_com.h"
#include "ubse_common_def.h"
#include "mp_module.h"
#include "mempooling_message.h"


namespace mempooling {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::message;

struct FaultMemIdManageParam {
    std::string importNodeId;
    uint64_t importMemId;
};

class FaultMemIdHelper {
public:
    static FaultMemIdHelper &Instance()
    {
        static FaultMemIdHelper instance;
        return instance;
    }

    MpResult Init();

    MpResult FaultMemIdManageHelper(std::string importNodeId, uint64_t importMemId, bool isForce, bool byNodeFault);

private:
    FaultMemIdHelper() = default;
    ~FaultMemIdHelper() = default;
    FaultMemIdHelper(const FaultMemIdHelper &) = delete;
    FaultMemIdHelper &operator = (const FaultMemIdHelper &) = delete;
};

class MpFaultMemIdSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        // 注册memId级别不同nid节点故障处理
        UbseComEndpoint endpoint_fm_not = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_FM_NOT};
        uint32_t ret = UbseRegRpcService(endpoint_fm_not, MemIdFaultNotSameNidRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager][MemId] MemIdFaultNotSameNidRecvHandler reg failed ret = " << ret << ".";
            return ret;
        }

        // 注册memId级别相同nid节点故障处理
        UbseComEndpoint endpoint_fm_same = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_FM_SAME};
        ret = UbseRegRpcService(endpoint_fm_same, MemIdFaultSameNidRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager][MemId] MemIdFaultSameNidRecvHandler reg failed ret = " << ret << ".";
            return ret;
        }

        // 注册memId级别不同nid节点故障处理 vm和借用内存查询
        UbseComEndpoint endpoint_fm_vm = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_FM_VM};
        ret = UbseRegRpcService(endpoint_fm_vm, MemIdFaultNotSameNidVmInfoRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager][MemId] MemIdFaultNotSameNidVmInfoRecvHandler reg failed ret = " << ret << ".";
            return ret;
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return ;
    }
    const std::string Name() override
    {
        return "FaultMemId";
    }
};

} // namespace mempooling
#endif // FAULT_MEMID_HELPER_H