/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_EXPORTOBJ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_EXPORTOBJ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj;
using ubse::utils::Ref;

class UbseMemNumaBorrowExportobjSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowExportObj> {
public:
    UbseMemNumaBorrowExportobjSimpo() = default;

    explicit UbseMemNumaBorrowExportobjSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowExportObj>(data, size)
    {
    }

    inline void SetUbseMemNumaBorrowExportobj(UbseMemNumaBorrowExportObj obj)
    {
        SetUbseMesgInfo(std::move(obj));
    }

    inline UbseMemNumaBorrowExportObj GetUbseMemNumaBorrowExportObj()
    {
        return GetUbseMesgInfo();
    }
};

using UbseMemNumaBorrowExportobjSimpoPtr = Ref<UbseMemNumaBorrowExportobjSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_EXPORTOBJ_SIMPO_H
