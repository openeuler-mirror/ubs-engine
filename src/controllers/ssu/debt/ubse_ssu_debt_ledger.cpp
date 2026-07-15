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
#include "ubse_ssu_debt_ledger.h"

#include <set>
#include <shared_mutex>
#include <unordered_map>
#include "ubse_logger.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::debt {

using namespace ubse::log;
using namespace ubse::ssu::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseSsuDebtLedger &UbseSsuDebtLedger::GetInstance()
{
    static UbseSsuDebtLedger instance;
    return instance;
}

bool UbseSsuDebtLedger::Put(const std::string &name, std::shared_ptr<const UbseSsuLedgerEntry> entry)
{
    if (!entry) {
        UBSE_LOG_ERROR << "Ledger put: entry is null";
        return false;
    }
    std::unique_lock<std::shared_mutex> lock(mtx_);
    bool isNew = ledger_.find(name) == ledger_.end();
    if (!isNew) {
        UBSE_LOG_ERROR << "Ledger put: entry already exists, reject overwrite: name=" << name;
        return false;
    }
    ledger_[name] = std::move(entry);
    UBSE_LOG_INFO << "Ledger put: name=" << name;
    return true;
}

std::shared_ptr<const UbseSsuLedgerEntry> UbseSsuDebtLedger::Get(const std::string &name) const
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = ledger_.find(name);
    return it != ledger_.end() ? it->second : nullptr;
}

bool UbseSsuDebtLedger::Modify(const std::string &name, std::function<void(UbseSsuLedgerEntry &)> modify)
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto it = ledger_.find(name);
    if (it == ledger_.end()) {
        UBSE_LOG_WARN << "Ledger entry not found for modify: name=" << name;
        return false;
    }
    auto oldEntry = it->second;
    auto newEntry = std::make_shared<UbseSsuLedgerEntry>(*oldEntry);
    modify(*newEntry);
    ledger_[name] = std::move(newEntry);
    return true;
}

bool UbseSsuDebtLedger::Remove(const std::string &name)
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto erased = ledger_.erase(name);
    if (erased > 0) {
        UBSE_LOG_INFO << "Ledger remove: name=" << name;
        return true;
    }
    UBSE_LOG_WARN << "Ledger entry not found for remove: name=" << name;
    return false;
}

std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> UbseSsuDebtLedger::GetAll() const
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> entries;
    entries.reserve(ledger_.size());
    for (const auto &[_, entry] : ledger_) {
        entries.push_back(entry);
    }
    return entries;
}

static void RebuildEntry(const std::string &name, const std::vector<const UbseSsuDevNameSpace *> &nsList,
                         const std::set<std::pair<std::string, uint32_t>> &attachedNsSet, UbseSsuDebtLedger &ledger,
                         const std::string &tenant)
{
    if (nsList.empty()) {
        UBSE_LOG_ERROR << "Rebuild entry: nsList is empty: name=" << name;
        return;
    }
    const auto &first = *(nsList[0]);
    // 校验所有NS的customData关键字段一致性
    for (size_t i = 1; i < nsList.size(); i++) {
        if (nsList[i]->customData.nsNum != first.customData.nsNum ||
            nsList[i]->customData.totalBytes != first.customData.totalBytes) {
            UBSE_LOG_WARN << "RebuildEntry: customData inconsistency detected, name=" << name
                          << ", ns[0].nsNum=" << static_cast<int>(first.customData.nsNum) << ", ns[" << i
                          << "].nsNum=" << static_cast<int>(nsList[i]->customData.nsNum);
        }
    }

    UbseSsuAllocSpaceReq req;
    req.name = name;
    req.tenant = tenant;
    req.nsNum = first.customData.nsNum;
    req.nsSize = first.customData.totalBytes;
    req.lbaFormat = (first.nsOptions.flbas == 1) ? UbseSsuLBAFormat::LBA_FORMAT_4K : UbseSsuLBAFormat::LBA_FORMAT_512;
    req.strategy = (first.customData.allocStrategy == static_cast<uint8_t>(UbseSsuAllocStrategy::STRIPED)) ?
                       UbseSsuAllocStrategy::STRIPED :
                       UbseSsuAllocStrategy::LINEAR;
    std::string userName(first.customData.userName,
                         strnlen(first.customData.userName, sizeof(first.customData.userName)));

    UbseSsuAllocResult allocResult;
    allocResult.name = name;
    allocResult.strategy = req.strategy;
    for (const auto *ns : nsList) {
        UbseSsuNameSpaceInfo info;
        info.tgtEid = ns->subSystem.eid;
        info.tgtNqn = ns->subSystem.subNqn;
        info.nsUuid = ns->uuid;
        info.namespaceId = ns->namespaceId;
        info.nsDevPath = std::string("/dev/disk/by-id/nvme-eui.") + StrToHex(ns->guid);
        info.nsSize = ns->nsze;
        info.lbaFormat = req.lbaFormat;
        allocResult.nameSpaceList.push_back(info);
    }

    bool isAttached = false;
    for (const auto *ns : nsList) {
        if (attachedNsSet.count({ns->subSystem.eid, ns->namespaceId}) > 0) {
            isAttached = true;
            break;
        }
    }

    UbseSsuLedgerEntry entry;
    entry.name = name;
    entry.allocReq = req;
    if (nsList[0]->namespaceId == 0) {
        UBSE_LOG_ERROR << "Rebuild entry: invalid namespaceId(0) for name=" << name;
        return;
    }
    // 如果有attach的ns，状态为ATTACHED，否则为CREATED
    // namespaceId == 0 已在前面校验拦截
    entry.state = isAttached ? UbseSsuNsState::ATTACHED : UbseSsuNsState::CREATED;
    entry.allocResult = allocResult;

    if (!ledger.Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
        UBSE_LOG_WARN << "Rebuild entry: ledger entry already exists, skip: name=" << name;
    }
}

void UbseSsuDebtLedger::Rebuild(const std::vector<UbseSsuDevInfo> &devices)
{
    auto &ledger = *this;
    std::unordered_map<std::string, std::vector<const UbseSsuDevNameSpace *>> groups;
    std::set<std::pair<std::string, uint32_t>> attachedNsSet;

    for (const auto &dev : devices) {
        for (const auto &ns : dev.nameSpaces) {
            if (ns.customData.name[0] == '\0') {
                continue;
            }
            std::string nsName(ns.customData.name, strnlen(ns.customData.name, sizeof(ns.customData.name)));
            if (nsName.empty()) {
                continue;
            }
            groups[nsName].push_back(&ns);
        }
        for (const auto &attachInfo : dev.attachInfoList) {
            attachedNsSet.emplace(dev.subSystem.eid, attachInfo.namespaceId);
        }
    }

    for (const auto &[name, nsList] : groups) {
        if (nsList.empty()) {
            UBSE_LOG_WARN << "Rebuild debt ledger entry: nsList is empty: name=" << name;
            continue;
        }
        // 同一个请求name，来自同一个租户，每个namespace的tenant都应相同
        std::string tenant(nsList[0]->customData.tenant,
                           strnlen(nsList[0]->customData.tenant, sizeof(nsList[0]->customData.tenant)));
        RebuildEntry(name, nsList, attachedNsSet, ledger, tenant);
    }

    UBSE_LOG_INFO << "Ledger rebuild complete: " << groups.size() << " entries restored";
}

} // namespace ubse::ssu::debt
