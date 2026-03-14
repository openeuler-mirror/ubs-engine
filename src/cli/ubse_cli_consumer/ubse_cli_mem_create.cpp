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

#include "ubse_cli_mem_create.h"
#include <string>
#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_constant.h"
#include "ubse_cli_mem_query.h"
#include "ubse_cli_mem_struct.h"
#include "ubse_cli_reg.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_pointer_process.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::mem::controller;

class UbseCliMemCreate::UbseCliMemCreateImpl {
public:
    UbseCliMemCreateImpl()
    {
        queryFuncMap_[static_cast<uint16_t>(UBSE_MEM_CLI_NUMA_CREATE)] = [this](const std::string &name,
            UbseCliMemQuery::WaitType b) { return this->query.UbseCliGetNumaMemByName(name, b); };
        // Fixed: Ensure the lambda matches the expected return type
        queryFuncMap_[static_cast<uint16_t>(UBSE_MEM_CLI_FD_CREATE)] = [this](const std::string &name,
            UbseCliMemQuery::WaitType b) { return this->query.UbseCliGetFdMemByName(name, b); };
    }

    template <typename T, uint16_t ModuleCode, uint16_t Opcode>
    bool UbseCliMemCreateRequest(UbseSerialization &ubse_req_serial, T &container)
    {
        ubse_api_buffer_t ubse_req_buffer{ ubse_req_serial.GetBuffer(),
            static_cast<uint32_t>(ubse_req_serial.GetLength()) };
        ubse_api_buffer_t ubse_res_buffer{};
        UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
        if (uint32_t ret = ubse_invoke_call(ModuleCode, Opcode, &ubse_req_buffer, &ubse_res_buffer); ret != UBSE_OK) {
            callErrorCode_ = ret;
            errorMsg_ = std::string("ERROR: Internal error with error code " + std::to_string(ret) + ".");
            return false;
        }
        UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
        if (!container.Deserialize(ubse_de_serial)) {
            errorMsg_ = data::error::RES_INFO_DESER_FAILED;
            return false;
        }
        return true;
    }

    template <uint16_t ModuleCode, uint16_t Opcode>
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateMemImpl(UbseSerialization &ubse_req_serial,
        const std::string &name)
    {
        UbseCliMemOperationResp resp{};
        UbseCliWaitIndicator waitIndicator("Creating memory");
        auto ret = UbseCliMemCreateRequest<UbseCliMemOperationResp, ModuleCode, Opcode>(ubse_req_serial, resp);
        if (ret || this->callErrorCode_ == UBSE_ERR_TIMED_OUT) {
            auto it = queryFuncMap_.find(Opcode);
            if (it != queryFuncMap_.end()) {
                return it->second(name, UbseCliMemQuery::WaitType::WAIT_CREATING);
            } else {
                return UbseCliRegModule::UbseCliStringPromptReply("ERROR: Unsupported opcode.");
            }
        } else {
            return UbseCliRegModule::UbseCliStringPromptReply(GetErrorMessage(this->callErrorCode_));
        }
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateShareMemImpl(const std::string &name, size_t size,
                                                                            const std::vector<uint32_t> &region)
    {
        // 序列化
        UbseSerialization serial;
        serial << size << name << array_len_insert(region.size());
        for (auto slotId : region) {
            serial << slotId;
        }
        if (!serial.Check()) {
            return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
        }
        // 接口调用
        ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
        ubse_api_buffer_t resBuffer{nullptr, 0};
        UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
        UbseCliWaitIndicator waitIndicator("Creating memory");
        uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_SHM_CREATE, &reqBuffer, &resBuffer);
        if (ret == UBSE_ERR_TIMED_OUT) {
            // 超时处理
            return query.UbseCliGetShmMemByName(name, UbseCliShmOperation::CREATE);
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
        return UbseCliRegModule::UbseCliStringPromptReply(shmDesc.GetStringResult());
    }

private: // Basic Verification
    std::string errorMsg_{};
    uint32_t callErrorCode_{ UBSE_OK };

private: // API Validation
    using queryFuncType =
        std::function<std::shared_ptr<framework::UbseCliResultEcho>(const std::string &, UbseCliMemQuery::WaitType)>;
    std::map<uint16_t, queryFuncType> queryFuncMap_;
    UbseCliMemQuery query;
};

UbseCliMemCreate::UbseCliMemCreate()
{
    pImpl_ = SafeMakeUnique<UbseCliMemCreateImpl>();
}
UbseCliMemCreate::~UbseCliMemCreate() noexcept = default;

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemCreate::UbseCliCreateNumaMem(const std::string &name,
    size_t size, const std::string &linkInfo)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    UbseSerialization ubse_req_serial;
    ubse_req_serial << name << size << linkInfo;
    if (!ubse_req_serial.Check()) {
        return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
    }
    return this->pImpl_
        ->UbseCliCreateMemImpl<static_cast<uint16_t>(UBSE_MEM), static_cast<uint16_t>(UBSE_MEM_CLI_NUMA_CREATE)>(
        ubse_req_serial, name);
}

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemCreate::UbseCliCreateFdMem(const std::string &name, size_t size)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    UbseSerialization ubse_req_serial;
    ubse_req_serial << name << size;
    if (!ubse_req_serial.Check()) {
        return UbseCliRegModule::UbseCliStringPromptReply(data::error::REQUEST_INFO_SER_FAILED);
    }
    return this->pImpl_
        ->UbseCliCreateMemImpl<static_cast<uint16_t>(UBSE_MEM), static_cast<uint16_t>(UBSE_MEM_CLI_FD_CREATE)>(
        ubse_req_serial, name);
}
std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemCreate::UbseCliCreateShareMem(
    const std::string &name, size_t size, const std::vector<uint32_t> &region)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return this->pImpl_->UbseCliCreateShareMemImpl(name, size, region);
}
} // namespace ubse::cli::reg
