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

#ifndef UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_EXPORTOBJ_SIMPO_H
#define UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_EXPORTOBJ_SIMPO_H

#include "ubse_base_message.h"
#include "ubse_mem_controller_serial.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller::message {
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj;
using ubse::message::UbseBaseMessage;
using ubse::utils::Ref;

class UbseMemAddrBorrowExportobjSimpo : public UbseBaseMessage {
public:
    UbseMemAddrBorrowExportobjSimpo() = default;

    explicit UbseMemAddrBorrowExportobjSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetUbseMemAddrBorrowExportobj(UbseMemAddrBorrowExportObj obj)
    {
        exportObj_ = std::move(obj);
    }
    inline UbseMemAddrBorrowExportObj GetUbseMemAddrBorrowExportObj()
    {
        return exportObj_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UbseMemAddrBorrowExportObj exportObj_;
};

using UbseMemAddrBorrowExportobjSimpoPtr = Ref<UbseMemAddrBorrowExportobjSimpo>;
} // namespace ubse::mem::controller::message

#endif // UBSE_MANAGER_UBSE_MEM_ADDR_BORROW_EXPORTOBJ_SIMPO_H
