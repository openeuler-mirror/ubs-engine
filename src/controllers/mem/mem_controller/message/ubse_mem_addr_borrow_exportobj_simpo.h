/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_EXPORTOBJ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_EXPORTOBJ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj;
using ubse::utils::Ref;

class UbseMemAddrBorrowExportobjSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowExportObj> {
public:
    UbseMemAddrBorrowExportobjSimpo() = default;

    explicit UbseMemAddrBorrowExportobjSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemAddrBorrowExportObj>(data, size)
    {
    }

    inline void SetUbseMemAddrBorrowExportobj(UbseMemAddrBorrowExportObj obj)
    {
        SetUbseMesgInfo(std::move(obj));
    }

    inline UbseMemAddrBorrowExportObj GetUbseMemAddrBorrowExportObj()
    {
        return GetUbseMesgInfo();
    }
};

using UbseMemAddrBorrowExportobjSimpoPtr = Ref<UbseMemAddrBorrowExportobjSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_EXPORTOBJ_SIMPO_H
