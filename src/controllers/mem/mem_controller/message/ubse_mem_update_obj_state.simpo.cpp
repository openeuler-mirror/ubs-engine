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
#include "ubse_mem_update_obj_state.simpo.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_serial.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message {
UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
template <typename ObjType>
common::def::UbseResult SerializeObjType(serial::UbseSerialization &out,
                                         const std::variant<adapter_plugins::mmi::UbseMemNumaBorrowImportObj,
                                                            adapter_plugins::mmi::UbseMemAddrBorrowImportObj,
                                                            adapter_plugins::mmi::UbseMemFdBorrowImportObj> &obj,
                                         const std::string &expectedType)
{
    if (auto importObj = std::get_if<ObjType>(&obj)) {
        bool success = false;
        if constexpr (std::is_same_v<ObjType, adapter_plugins::mmi::UbseMemFdBorrowImportObj>) {
            success = serial::UbseMemFdBorrowImportObjSerialization(out, *importObj);
        } else if constexpr (std::is_same_v<ObjType, adapter_plugins::mmi::UbseMemNumaBorrowImportObj>) {
            success = serial::UbseMemNumaBorrowImportObjSerialization(out, *importObj);
        } else if constexpr (std::is_same_v<ObjType, adapter_plugins::mmi::UbseMemAddrBorrowImportObj>) {
            success = serial::UbseMemAddrBorrowImportObjSerialization(out, *importObj);
        }
        if (!success) {
            UBSE_LOG_ERROR << "Failed to serialize.";
            return UBSE_ERROR;
        }
    } else {
        UBSE_LOG_ERROR << "objType=" << expectedType << " but variant holds different type.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace

common::def::UbseResult UbseMemUpdateObjState::Serialize()
{
    UBSE_LOG_INFO << "Serialize objType=" << objType;
    serial::UbseSerialization out;
    out << objType;
    if (objType == "fd") {
        if (auto ret = SerializeObjType<adapter_plugins::mmi::UbseMemFdBorrowImportObj>(out, obj, "fd");
            ret != UBSE_OK) {
            return ret;
        }
    } else if (objType == "numa") {
        if (auto ret = SerializeObjType<adapter_plugins::mmi::UbseMemNumaBorrowImportObj>(out, obj, "numa");
            ret != UBSE_OK) {
            return ret;
        }
    } else if (objType == "addr") {
        if (auto ret = SerializeObjType<adapter_plugins::mmi::UbseMemAddrBorrowImportObj>(out, obj, "addr");
            ret != UBSE_OK) {
            return ret;
        }
    } else {
        UBSE_LOG_ERROR << "Unsupported objType=" << objType;
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

common::def::UbseResult UbseMemUpdateObjState::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> objType;
    UBSE_LOG_INFO << "Deserialize objType=" << objType;
    if (objType == "fd") {
        adapter_plugins::mmi::UbseMemFdBorrowImportObj importObj;
        if (!serial::UbseMemFdBorrowImportObjDeserialization(in, importObj)) {
            UBSE_LOG_ERROR << "Failed to deserialize.";
            return UBSE_ERROR;
        }
        obj = importObj;
    } else if (objType == "numa") {
        adapter_plugins::mmi::UbseMemNumaBorrowImportObj importObj;
        if (!serial::UbseMemNumaBorrowImportObjDeserialization(in, importObj)) {
            UBSE_LOG_ERROR << "Failed to deserialize.";
            return UBSE_ERROR;
        }
        obj = importObj;
    } else if (objType == "addr") {
        adapter_plugins::mmi::UbseMemAddrBorrowImportObj importObj;
        if (!serial::UbseMemAddrBorrowImportObjDeserialization(in, importObj)) {
            UBSE_LOG_ERROR << "Failed to deserialize.";
            return UBSE_ERROR;
        }
        obj = importObj;
    } else {
        UBSE_LOG_ERROR << "Unsupported objType=" << objType;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

common::def::UbseResult UbseMemUpdateObjStateReply::Serialize()
{
    serial::UbseSerialization out;
    auto errorCode = UBSE_OK;
    out << errorCode;
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

common::def::UbseResult UbseMemUpdateObjStateReply::Deserialize()
{
    return UBSE_OK;
}

} // namespace ubse::mem::controller::message