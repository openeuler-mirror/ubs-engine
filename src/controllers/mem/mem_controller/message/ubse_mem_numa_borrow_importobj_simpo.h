/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_IMPORTOBJ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_IMPORTOBJ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj;
using ubse::utils::Ref;

class UbseMemNumaBorrowImportobjSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowImportObj> {
public:
    UbseMemNumaBorrowImportobjSimpo() = default;

    explicit UbseMemNumaBorrowImportobjSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemNumaBorrowImportObj>(data, size)
    {
    }

    inline void SetUbseMemNumaBorrowImportobj(UbseMemNumaBorrowImportObj obj)
    {
        SetUbseMesgInfo(std::move(obj));
    }

    inline UbseMemNumaBorrowImportObj GetUbseMemNumaBorrowImportObj()
    {
        return GetUbseMesgInfo();
    }

    inline UbseMemNumaBorrowImportObj GetImportObj()
    {
        return GetUbseMesgInfo();
    }
};

using UbseMemNumaBorrowImportobjSimpoPtr = Ref<UbseMemNumaBorrowImportobjSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_NUMA_BORROW_IMPORTOBJ_SIMPO_H
