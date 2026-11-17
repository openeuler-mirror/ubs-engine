/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_IMPORTOBJ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_IMPORTOBJ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj;
using ubse::utils::Ref;

class UbseMemAddrBorrowImportobjSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowImportObj> {
public:
    UbseMemAddrBorrowImportobjSimpo() = default;

    explicit UbseMemAddrBorrowImportobjSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowImportObj>(data, size)
    {
    }

    inline void SetUbseMemAddrBorrowImportobj(UbseMemAddrBorrowImportObj obj)
    {
        SetUbseMesgInfo(std::move(obj));
    }

    inline UbseMemAddrBorrowImportObj GetUbseMemAddrBorrowImportobj()
    {
        return GetUbseMesgInfo();
    }

    inline UbseMemAddrBorrowImportObj GetImportObj()
    {
        return GetUbseMesgInfo();
    }
};

using UbseMemAddrBorrowImportobjSimpoPtr = Ref<UbseMemAddrBorrowImportobjSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_IMPORTOBJ_SIMPO_H
