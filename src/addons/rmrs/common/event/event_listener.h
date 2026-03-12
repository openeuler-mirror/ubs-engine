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

#ifndef MEMPOOLING_EVENT_H
#define MEMPOOLING_EVENT_H

#include <string>

#include "event_handler.h"
#include "ubse_logger.h"
#include "mp_module.h"

namespace mempooling::event {
class EventListener {
public:
    static MpResult Init();
};

class MpEventSubModule : public mempooling::MpSubModule {
public:
    MpResult Init() override
    {
        auto ret = EventListener::Init();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return ;
    }
    const std::string Name() override
    {
        return "Event";
    }
};

}
#endif