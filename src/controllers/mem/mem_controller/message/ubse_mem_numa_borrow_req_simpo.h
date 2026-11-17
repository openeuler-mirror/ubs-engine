/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_REQ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_REQ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq;
using ubse::utils::Ref;

class UbseMemNumaBorrowReqSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowReq> {
public:
    UbseMemNumaBorrowReqSimpo() = default;

    explicit UbseMemNumaBorrowReqSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowReq>(data, size)
    {
    }

    inline void SetUbseMemNumaBorrowReq(UbseMemNumaBorrowReq request)
    {
        SetUbseMesgInfo(std::move(request));
    }

    inline UbseMemNumaBorrowReq GetUbseMemNumaBorrowReq()
    {
        return GetUbseMesgInfo();
    }
};

using UbseMemNumaBorrowReqSimpoPtr = Ref<UbseMemNumaBorrowReqSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_REQ_SIMPO_H
