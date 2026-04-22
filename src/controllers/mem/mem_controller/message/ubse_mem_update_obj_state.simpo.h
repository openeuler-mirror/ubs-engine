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

#ifndef UBSE_MEM_UPDATE_OBJ_STATE_SIMPO_H
#define UBSE_MEM_UPDATE_OBJ_STATE_SIMPO_H
#include <variant>
#include "ubse_base_message.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mmi_def.h"
namespace ubse::mem::controller::message {
class UbseMemUpdateObjState : public ubse::message::UbseBaseMessage {
public:
    UbseMemUpdateObjState() = default;
    ~UbseMemUpdateObjState() = default;

    common::def::UbseResult Serialize() override;

    common::def::UbseResult Deserialize() override;

public:
    std::string objType; // 对象类型，如fd、numa等
    std::variant<adapter_plugins::mmi::UbseMemNumaBorrowImportObj, adapter_plugins::mmi::UbseMemAddrBorrowImportObj,
                 adapter_plugins::mmi::UbseMemFdBorrowImportObj>
        obj; // 对象信息，使用variant存储不同类型的对象,目前只包含import对象
};
using UbseMemUpdateObjStatePtr = utils::Ref<UbseMemUpdateObjState>;

class UbseMemUpdateObjStateReply : public ubse::message::UbseBaseMessage {
public:
    UbseMemUpdateObjStateReply() = default;
    ~UbseMemUpdateObjStateReply() = default;
    common::def::UbseResult Serialize() override;
    common::def::UbseResult Deserialize() override;
};
} // namespace ubse::mem::controller::message
#endif