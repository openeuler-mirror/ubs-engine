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

#ifndef UBSE_MANAGER_UBSE_MEM_NODE_DEBT_INFO_CONVERSION_H
#define UBSE_MANAGER_UBSE_MEM_NODE_DEBT_INFO_CONVERSION_H
#include <algorithm>
#include <iostream>
#include "ubse_def.h"
#include "ubse_error.h"
#include "ubse_mem_resource.h"
#include "ubse_mem_controller_serial.h"
namespace ubse::mem::serial {
using namespace ubse::resource::mem;
inline void UbseMemFdImportObjMapSerialize(UbseSerialization &out, const UbseMemFdImportObjMap &data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto &item : data) {
        out << item.first;
        UbseMemFdBorrowImportObjSerialization(out, item.second);
    }
}
inline bool UbseMemFdImportObjMapDeserialize(UbseDeSerialization &in, UbseMemFdImportObjMap &data)
{
    uint64_t dataSize;
    in >> ubse::serial::array_len_capture(dataSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            return false;
        }
        UbseMemFdBorrowImportObj obj;
        bool ret = UbseMemFdBorrowImportObjDeserialization(in, obj);
        if (ret != UBSE_OK) {
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemFdExportObjMapSerialize(UbseSerialization &out, const UbseMemFdExportObjMap &data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto &item : data) {
        out << item.first;
        UbseMemFdBorrowExportObjSerialization(out, item.second);
    }
}
inline bool UbseMemFdExportObjMapDeserialize(UbseDeSerialization &in, UbseMemFdExportObjMap &data)
{
    uint64_t dataSize;
    in >> ubse::serial::array_len_capture(dataSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            return false;
        }
        UbseMemFdBorrowExportObj obj;
        bool ret = UbseMemFdBorrowExportObjDeserialization(in, obj);
        if (ret != UBSE_OK) {
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemNumaImportObjMapSerialize(UbseSerialization &out, const UbseMemNumaImportObjMap &data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto &item : data) {
        out << item.first;
        UbseMemNumaBorrowImportObjSerialization(out, item.second);
    }
}
inline bool UbseMemNumaImportObjMapDeserialize(UbseDeSerialization &in, UbseMemNumaImportObjMap &data)
{
    uint64_t dataSize;
    in >> ubse::serial::array_len_capture(dataSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            return false;
        }
        UbseMemNumaBorrowImportObj obj;
        bool ret = UbseMemNumaBorrowImportObjDeserialization(in, obj);
        if (ret != UBSE_OK) {
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemNumaExportObjMapSerialize(UbseSerialization &out, const UbseMemNumaExportObjMap &data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto &item : data) {
        out << item.first;
        UbseMemNumaBorrowExportObjSerialization(out, item.second);
    }
}
inline bool UbseMemNumaExportObjMapDeserialize(UbseDeSerialization &in, UbseMemNumaExportObjMap &data)
{
    uint64_t dataSize;
    in >> ubse::serial::array_len_capture(dataSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            return false;
        }
        UbseMemNumaBorrowExportObj obj;
        bool ret = UbseMemNumaBorrowExportObjDeserialization(in, obj);
        if (ret != UBSE_OK) {
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void NodeMemDebtInfoSerialize(UbseSerialization &out, resource::mem::NodeMemDebtInfoMap &data)
{
    out << ubse::serial::array_len_insert(data.size());
    for (const auto &item : data) {
        out << item.first;
        // 序列化UbseMemFdImportObjMap
        UbseMemFdImportObjMapSerialize(out, item.second.fdImportObjMap);
        // 序列化 UbseMemFdExportObjMap
        UbseMemFdExportObjMapSerialize(out, item.second.fdExportObjMap);
        // 序列化 UbseMemNumaImportObjMap
        UbseMemNumaImportObjMapSerialize(out, item.second.numaImportObjMap);
        // 序列化 UbseMemNumaExportObjMap
        UbseMemNumaExportObjMapSerialize(out, item.second.numaExportObjMap);
    }
}
inline bool NodeMemDebtInfoDeserialize(UbseDeSerialization &in, resource::mem::NodeMemDebtInfoMap &data)
{
    uint64_t dataSize;
    in >> ubse::serial::array_len_capture(dataSize);
    if (!in.Check()) {
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string debtInfoMapKey{};
        in >> debtInfoMapKey;
        if (!in.Check()) {
            return false;
        }
        resource::mem::NodeMemDebtInfo nodeMemDebtInfo;
        // 反序列化UbseMemFdImportObjMap
        UbseMemFdImportObjMapDeserialize(in, nodeMemDebtInfo.fdImportObjMap);
        if (!in.Check()) {
            return false;
        }
        // 反序列化 UbseMemFdExportObjMap
        UbseMemFdExportObjMapDeserialize(in, nodeMemDebtInfo.fdExportObjMap);
        if (!in.Check()) {
            return false;
        }
        // 反序列化 UbseMemNumaImportObjMap
        UbseMemNumaImportObjMapDeserialize(in, nodeMemDebtInfo.numaImportObjMap);
        if (!in.Check()) {
            return false;
        }
        // 反序列化 UbseMemNumaExportObjMap
        UbseMemNumaExportObjMapDeserialize(in, nodeMemDebtInfo.numaExportObjMap);
        if (!in.Check()) {
            return false;
        }
        data.emplace(debtInfoMapKey, nodeMemDebtInfo);
    }
    return true;
}
} // namespace ubse::mem::serial

#endif // UBSE_MANAGER_UBSE_MEM_NODE_DEBT_INFO_CONVERSION_H