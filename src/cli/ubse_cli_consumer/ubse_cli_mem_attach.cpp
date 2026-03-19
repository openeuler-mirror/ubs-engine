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

#include "ubse_cli_mem_attach.h"

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_constant.h"
#include "ubse_cli_mem_query.h"
#include "ubse_cli_mem_struct.h"
#include "ubse_cli_reg.h"
#include "ubse_cli_res_builder.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_pointer_process.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
using namespace ubse::serial;
using namespace ubse::mem::controller;

class UbseCliMemAttach::UbseCliMemAttachImpl {
public:
    UbseCliMemAttachImpl() {}

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliAttachMem(const std::string &name)
    {
        // 序列化
        UbseSerialization serial;
        serial << name;
        if (!serial.Check()) {
            return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
        }
        // 调用接口
        ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
        ubse_api_buffer_t resBuffer{};
        UbseCliWaitIndicator waitIndicator("Attaching memory");
        uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_SHM_ATTACH, &reqBuffer, &resBuffer);
        UbseCliBufferGuard cliBufferGuard(resBuffer);
        if (ret == UBSE_ERR_TIMED_OUT) {
            return query.UbseCliGetShmMemByName(name, UbseCliShmOperation::ATTACH);
        } else if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(GetErrorMessage(ret));
        }
        // 解序列化
        UbseMemShmInfo shmDesc;
        UbseDeSerialization deSerialization(resBuffer.buffer, resBuffer.length);
        if (!shmDesc.Deserialize(deSerialization)) {
            return UbseCliRegModule::UbseCliStringPromptReply(data::error::RES_INFO_DESER_FAILED);
        }
        // 成功回显
        return UbseCliRegModule::UbseCliStringPromptReply(shmDesc.GetStringResult(true));
    }

private:
    UbseCliMemQuery query;
};

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemAttach::UbseCliAttachMem(const std::string &name)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return this->pImpl_->UbseCliAttachMem(name);
}

UbseCliMemAttach::UbseCliMemAttach()
{
    pImpl_ = SafeMakeUnique<UbseCliMemAttachImpl>();
}
UbseCliMemAttach::~UbseCliMemAttach() noexcept = default;
} // namespace ubse::cli::reg
