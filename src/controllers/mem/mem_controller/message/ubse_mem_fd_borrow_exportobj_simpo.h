/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MANAGER_UBSE_MEM_FD_BORROW_EXPORTOBJ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_FD_BORROW_EXPORTOBJ_SIMPO_H

#include "ubse_mem_auto_serial.h"
#include "src/framework/misc/ubse_auto_simpo.hpp"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::utils::Ref;

class UbseMemFdBorrowExportobjSimpo : public ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowExportObj> {
public:
    UbseMemFdBorrowExportobjSimpo() = default;

    explicit UbseMemFdBorrowExportobjSimpo(uint8_t* data, uint32_t size)
        : ubse::simpo::util::UbseAutoSimpo<UbseMemFdBorrowExportObj>(data, size)
    {
    }

    inline void SetUbseMemFdBorrowExportobj(UbseMemFdBorrowExportObj obj)
    {
        SetUbseMesgInfo(std::move(obj));
    }

    inline UbseMemFdBorrowExportObj GetUbseMemFdBorrowExportObj()
    {
        return GetUbseMesgInfo();
    }
};
using UbseMemFdBorrowExportobjSimpoPtr = Ref<UbseMemFdBorrowExportobjSimpo>;

} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_FD_BORROW_EXPORTOBJ_SIMPO_H
