/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBSE_MEM_AGENT_UPDATE_OBJ_STATE_H
#define UBSE_MEM_AGENT_UPDATE_OBJ_STATE_H
#include "ubse_com_base.h"
#include "ubse_mem_update_obj_state.simpo.h"
namespace ubse::mem::controller {
class UbseMemAgentUpdateObjState : public com::UbseComBaseMessageHandler {
public:
    common::def::UbseResult Handle(const ubse::message::UbseBaseMessagePtr &req,
                                   const ubse::message::UbseBaseMessagePtr &rsp,
                                   ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override;
    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
} // namespace ubse::mem::controller
#endif