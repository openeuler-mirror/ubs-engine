/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_OPERATION_RESP_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_OPERATION_RESP_SIMPO_H
#include "ubse_base_message.h"
#include "ubse_mem_obj.h"
namespace ubse::mem::controller::message {
using namespace ubse::message;
using namespace ubse::mem::obj;
class UbseMemOperationRespSimpo : public UbseBaseMessage {
public:
    UbseMemOperationRespSimpo() = default;
    explicit UbseMemOperationRespSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseMemOperationResp(UbseMemOperationResp response)
    {
        resp = std::move(response);
    }
    inline UbseMemOperationResp GetUbseMemOperationResp()
    {
        return resp;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UbseMemOperationResp resp;
};
using UbseMemOperationRespSimpoPtr = Ref<UbseMemOperationRespSimpo>;
}

#endif // UBSE_MANAGER_UBSE_MEM_OPERATION_RESP_SIMPO_H
