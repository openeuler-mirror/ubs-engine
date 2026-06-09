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

#ifndef MP_SMAP_CONTROLLER_H
#define MP_SMAP_CONTROLLER_H

#include <sys/types.h>
#include <iostream>
#include <string>
#include <vector>
#include "ubse_com.h"
#include "ubse_logger.h"
#include "mempooling_message.h"
#include "mp_smap_helper.h"

namespace mempooling::smap {
using namespace ubse::com;
using namespace ubse::log;
using namespace mempooling::message;

class SmapAddProcessTrackingParam {
public:
    std::vector<pid_t> pidVec;
    std::vector<uint32_t> scanTimeVec;
    int scanType;
    std::vector<uint32_t> durationVec;
};

class SmapAddProcessTrackingResult {
public:
    int errCode;
};

class SmapQueryVmFreqParam {
public:
    pid_t pid;
    uint16_t lengthIn;
    int dataSource;
};

class SmapQueryVmFreqResult {
public:
    int errCode;
    std::vector<uint16_t> dataVec;
    uint16_t lengthOut;
};

class SmapRemoveProcessTrackingParam {
public:
    std::vector<pid_t> pidVec;
    int flags;
};

class SmapRemoveProcessTrackingResult {
public:
    int errCode;
};

class SmapEnableProcessMigrateParam {
public:
    std::vector<pid_t> pidVec;
    int enable;
    int flags;
};

class SmapEnableProcessMigrateResult {
public:
    int errCode;
};

uint32_t SmapMigrateBackProcess(MigrateBackMsg migrateBackMsg);
uint32_t SmapEnableNumaProcess(EnableNodeMsg enableMsg);
uint32_t SmapEnablePidsProcess(std::vector<pid_t> pids);

class MpSmapSubModule : public mempooling::MpSubModule {
public:
    MpResult Init() override
    {
        auto ret = MpSmapHelper::Init();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }

        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        MpSmapHelper::VmSmapClose();
    }

    const std::string Name() override
    {
        return "Smap";
    }
};

} // namespace mempooling::smap

#endif // MP_SMAP_CONTROLLER_H