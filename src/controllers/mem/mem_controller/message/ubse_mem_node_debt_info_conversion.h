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
#include "ubse_logger_module.h"
#include "ubse_mem_controller_serial.h"

namespace ubse::mem::serial {
#define MODULE_LOG_NAME "ubse"
using namespace ubse::log;

inline void UbseMemFdImportObjMapSerialize(UbseSerialization& out, const UbseMemFdImportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemFdBorrowImportObjSerialization(out, item.second);
    }
}
inline bool UbseMemFdImportObjMapDeserialize(UbseDeSerialization& in, UbseMemFdImportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        UbseMemFdBorrowImportObj obj;
        bool ret = UbseMemFdBorrowImportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemFdExportObjMapSerialize(UbseSerialization& out, const UbseMemFdExportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemFdBorrowExportObjSerialization(out, item.second);
    }
}
inline bool UbseMemFdExportObjMapDeserialize(UbseDeSerialization& in, UbseMemFdExportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        UbseMemFdBorrowExportObj obj;
        bool ret = UbseMemFdBorrowExportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "UbseMemFdBorrowExportObjDeserialization failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemNumaImportObjMapSerialize(UbseSerialization& out, const UbseMemNumaImportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemNumaBorrowImportObjSerialization(out, item.second);
    }
}
inline bool UbseMemNumaImportObjMapDeserialize(UbseDeSerialization& in, UbseMemNumaImportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        UbseMemNumaBorrowImportObj obj;
        bool ret = UbseMemNumaBorrowImportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemNumaExportObjMapSerialize(UbseSerialization& out, const UbseMemNumaExportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemNumaBorrowExportObjSerialization(out, item.second);
    }
}
inline bool UbseMemNumaExportObjMapDeserialize(UbseDeSerialization& in, UbseMemNumaExportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "Check failed;";
            return false;
        }
        UbseMemNumaBorrowExportObj obj;
        bool ret = UbseMemNumaBorrowExportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "Check failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}
inline void UbseMemShareImportObjMapSerialize(UbseSerialization& out, const UbseMemShareImportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemShareBorrowImportObjSerialization(out, item.second);
    }
}
inline bool UbseMemShareImportObjMapDeserialize(UbseDeSerialization& in, UbseMemShareImportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        UbseMemShareBorrowImportObj obj;
        bool ret = UbseMemShareBorrowImportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}
inline void UbseMemShareExportObjMapSerialize(UbseSerialization& out, const UbseMemShareExportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemShareBorrowExportObjSerialization(out, item.second);
    }
}
inline bool UbseMemShareExportObjMapDeserialize(UbseDeSerialization& in, UbseMemShareExportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        UbseMemShareBorrowExportObj obj;
        bool ret = UbseMemShareBorrowExportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemAddrImportObjMapSerialize(UbseSerialization& out, const UbseMemAddrImportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemAddrBorrowImportObjSerialization(out, item.second);
    }
}
inline bool UbseMemAddrImportObjMapDeserialize(UbseDeSerialization& in, UbseMemAddrImportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }
        UbseMemAddrBorrowImportObj obj;
        bool ret = UbseMemAddrBorrowImportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "UbseMemAddrBorrowImportObjDeserialization failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}

inline void UbseMemAddrExportObjMapSerialize(UbseSerialization& out, const UbseMemAddrExportObjMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        UbseMemAddrBorrowExportObjSerialization(out, item.second);
    }
}
inline bool UbseMemAddrExportObjMapDeserialize(UbseDeSerialization& in, UbseMemAddrExportObjMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string key{};
        in >> key;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "Check failed;";
            return false;
        }
        UbseMemAddrBorrowExportObj obj;
        bool ret = UbseMemAddrBorrowExportObjDeserialization(in, obj);
        if (!ret) {
            UBSE_LOG_ERROR << "UbseMemAddrBorrowExportObjDeserialization failed;";
            return false;
        }
        data[key] = obj;
    }
    return true;
}
inline void NodeMemDebtInfoSerialize(UbseSerialization& out, NodeMemDebtInfoMap& data)
{
    out << array_len_insert(data.size());
    for (const auto& item : data) {
        out << item.first;
        // 序列化UbseMemFdImportObjMap
        UbseMemFdImportObjMapSerialize(out, item.second.fdImportObjMap);
        // 序列化 UbseMemFdExportObjMap
        UbseMemFdExportObjMapSerialize(out, item.second.fdExportObjMap);
        // 序列化 UbseMemNumaImportObjMap
        UbseMemNumaImportObjMapSerialize(out, item.second.numaImportObjMap);
        // 序列化 UbseMemNumaExportObjMap
        UbseMemNumaExportObjMapSerialize(out, item.second.numaExportObjMap);
        // 序列化 UbseMemShareImportObjMap
        UbseMemShareImportObjMapSerialize(out, item.second.shareImportObjMap);
        // 序列化 UbseMemShareExportObjMap
        UbseMemShareExportObjMapSerialize(out, item.second.shareExportObjMap);
        // 序列化 UbseMemAddrImportObjMap
        UbseMemAddrImportObjMapSerialize(out, item.second.addrImportObjMap);
        // 序列化 UbseMemAddrExportObjMap
        UbseMemAddrExportObjMapSerialize(out, item.second.addrExportObjMap);
    }
}
inline bool DoDeserialization(UbseDeSerialization& in, NodeMemDebtInfo& nodeMemDebtInfo)
{
    // 反序列化UbseMemFdImportObjMap
    UbseMemFdImportObjMapDeserialize(in, nodeMemDebtInfo.fdImportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemFdExportObjMap
    UbseMemFdExportObjMapDeserialize(in, nodeMemDebtInfo.fdExportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemNumaImportObjMap
    UbseMemNumaImportObjMapDeserialize(in, nodeMemDebtInfo.numaImportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemNumaExportObjMap
    UbseMemNumaExportObjMapDeserialize(in, nodeMemDebtInfo.numaExportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemShareImportObjMap
    UbseMemShareImportObjMapDeserialize(in, nodeMemDebtInfo.shareImportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemShareExportObjMap
    UbseMemShareExportObjMapDeserialize(in, nodeMemDebtInfo.shareExportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemAddrImportObjMap
    UbseMemAddrImportObjMapDeserialize(in, nodeMemDebtInfo.addrImportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    // 反序列化 UbseMemAddrExportObjMap
    UbseMemAddrExportObjMapDeserialize(in, nodeMemDebtInfo.addrExportObjMap);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    return true;
}
inline bool NodeMemDebtInfoDeserialize(UbseDeSerialization& in, NodeMemDebtInfoMap& data)
{
    uint64_t dataSize;
    in >> array_len_capture(dataSize);
    if (!in.Check()) {
        UBSE_LOG_ERROR << "check failed;";
        return false;
    }
    for (size_t i = 0; i < dataSize; i++) {
        std::string debtInfoMapKey{};
        in >> debtInfoMapKey;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "check failed;";
            return false;
        }

        NodeMemDebtInfo nodeMemDebtInfo;
        if (!DoDeserialization(in, nodeMemDebtInfo)) {
            return false;
        }
        data.emplace(debtInfoMapKey, nodeMemDebtInfo);
    }
    return true;
}
#undef MODULE_LOG_NAME
} // namespace ubse::mem::serial

#endif // UBSE_MANAGER_UBSE_MEM_NODE_DEBT_INFO_CONVERSION_H