/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_REQ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_REQ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq;
using ubse::utils::Ref;

class UbseMemAddrBorrowReqSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowReq> {
public:
    UbseMemAddrBorrowReqSimpo() = default;

    explicit UbseMemAddrBorrowReqSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowReq>(data, size)
    {
    }

    inline void SetUbseMemAddrBorrowReq(UbseMemAddrBorrowReq obj)
    {
        SetUbseMesgInfo(std::move(obj));
    }

    inline UbseMemAddrBorrowReq GetUbseMemAddrBorrowReq()
    {
        return GetUbseMesgInfo();
    }
};

using UbseMemAddrBorrowReqSimpoPtr = Ref<UbseMemAddrBorrowReqSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_REQ_SIMPO_H
