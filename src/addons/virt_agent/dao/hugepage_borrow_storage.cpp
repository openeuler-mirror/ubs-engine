/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "hugepage_borrow_storage.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <ubse_error.h>
#include <ubse_logger.h>
#include <ubse_storage.h>

#include "hugepage_borrow_record_message.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;
static const std::string HUGE_PAGE_BORROW_KEY_PREFIX = "ubse_";
static const std::string HUGE_PAGE_BORROW_KEY = "vm_hugepage_borrow";
ReadWriteLock HugePageBorrowStorage::hugePageBorrowLock{};

VmResult HugePageBorrowStorage::SaveBorrowPageSizes(const std::vector<std::string>& borrowIds, const uint64_t& pageSize)
{
    if (borrowIds.empty()) {
        return VM_OK;
    }
    WriteLocker<ReadWriteLock> lock(&hugePageBorrowLock);
    std::vector<HugePageBorrowRecord> records;
    auto ret = LoadRecords(records);
    if (ret != VM_OK) {
        return ret;
    }
    for (const auto& borrowId : borrowIds) {
        if (borrowId.empty()) {
            continue;
        }
        records.erase(
            std::remove_if(records.begin(), records.end(),
                           [&borrowId](const HugePageBorrowRecord& record) { return record.borrowId == borrowId; }),
            records.end());
        HugePageBorrowRecord record{};
        record.borrowId = borrowId;
        record.pageSize = pageSize;
        records.emplace_back(std::move(record));
    }
    return SaveRecords(records);
}

VmResult HugePageBorrowStorage::DeleteBorrowPageSizes(const std::vector<std::string>& borrowIds)
{
    if (borrowIds.empty()) {
        return VM_OK;
    }
    const std::unordered_set<std::string> deletedBorrowIds(borrowIds.begin(), borrowIds.end());
    WriteLocker<ReadWriteLock> lock(&hugePageBorrowLock);
    std::vector<HugePageBorrowRecord> records;
    auto ret = LoadRecords(records);
    if (ret != VM_OK) {
        return ret;
    }
    records.erase(std::remove_if(records.begin(), records.end(),
                                 [&deletedBorrowIds](const HugePageBorrowRecord& record) {
                                     return deletedBorrowIds.count(record.borrowId) > 0;
                                 }),
                  records.end());
    return SaveRecords(records);
}

VmResult HugePageBorrowStorage::GetAllBorrowPageSizes(std::unordered_map<std::string, uint64_t>& borrowPageSizes)
{
    ReadLocker<ReadWriteLock> lock(&hugePageBorrowLock);
    std::vector<HugePageBorrowRecord> records;
    auto ret = LoadRecords(records);
    if (ret != VM_OK) {
        return ret;
    }
    for (const auto& record : records) {
        borrowPageSizes[record.borrowId] = record.pageSize;
    }
    UBSE_LOG_INFO << "[GetAllBorrowPageSizes] get " << borrowPageSizes.size() << " record(s).";
    return VM_OK;
}

void HugePageBorrowStorage::QueryHandler(const std::string& keyPrefix, const std::string& key,
                                         const UbseByteBuffer& buff, void* ctx)
{
    auto* records = static_cast<std::vector<HugePageBorrowRecord>*>(ctx);
    if (records == nullptr || buff.data == nullptr || buff.len == 0) {
        UBSE_LOG_DEBUG << "[GetAllBorrowPageSizes] no hugepage borrow record.";
        return;
    }
    HugePageBorrowRecordMessage dataMessage{};
    auto ret = dataMessage.SetInputRawData(buff.data, buff.len);
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[GetAllBorrowPageSizes] message set input failed.";
        return;
    }
    ret = dataMessage.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_WARN << "[GetAllBorrowPageSizes] message deserialize failed.";
        return;
    }
    auto data = dataMessage.GetData();
    records->insert(records->end(), data.begin(), data.end());
}

VmResult HugePageBorrowStorage::LoadRecords(std::vector<HugePageBorrowRecord>& records)
{
    auto ret = UbseStorageQueryData(HUGE_PAGE_BORROW_KEY_PREFIX, HUGE_PAGE_BORROW_KEY, &records, QueryHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[LoadRecords] query failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    return VM_OK;
}

VmResult HugePageBorrowStorage::SaveRecords(const std::vector<HugePageBorrowRecord>& records)
{
    HugePageBorrowRecordMessage dataMessage{};
    dataMessage.SetData(records);
    auto ret = dataMessage.Serialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "[SaveRecords] serialize failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UbseByteBuffer buffer{
        .data = dataMessage.SerializedData(), .len = dataMessage.SerializedDataSize(), .freeFunc = nullptr};
    ret = UbseStoragePutData(HUGE_PAGE_BORROW_KEY_PREFIX, HUGE_PAGE_BORROW_KEY, &buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[SaveRecords] put failed, " << FormatRetCode(ret);
        return VM_ERROR;
    }
    UBSE_LOG_DEBUG << "[SaveRecords] put success, record count=" << records.size();
    return VM_OK;
}
} // namespace vm
