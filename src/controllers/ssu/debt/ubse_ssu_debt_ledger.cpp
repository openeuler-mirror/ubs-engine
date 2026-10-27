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

// 复合键拼接：'\x1f'（ASCII单元分隔符）不会出现在name合法字符集[A-Za-z0-9_-]与
// UUID格式[0-9a-f-]中，分隔无歧义；空resourceUuid（遗留数据）与name直接拼接
std::string ComposeResourceKey(const std::string &name, const std::string &resourceUuid)
{
    return name + '\x1f' + resourceUuid;
}

std::string ExtractNameFromResourceKey(const std::string &key)
{
    const auto pos = key.find('\x1f');
    return pos == std::string::npos ? key : key.substr(0, pos);
}

std::string ExtractUuidFromResourceKey(const std::string &key)
{
    const auto pos = key.find('\x1f');
    return pos == std::string::npos ? std::string() : key.substr(pos + 1);
}

// 从NS customData读取resourceUuid（遗留数据尾部补零读出为空串）
static std::string ReadCustomDataResourceUuid(const UbseSsuDevNameSpaceCustomData &customData)
{
    return std::string(customData.resourceUuid, strnlen(customData.resourceUuid, sizeof(customData.resourceUuid)));
}

bool UbseSsuDebtLedger::Put(const std::string &name, const std::string &resourceUuid,
                            std::shared_ptr<const UbseSsuLedgerEntry> entry)
{
    if (!entry) {
        UBSE_LOG_ERROR << "Ledger put: entry is null";
        return false;
    }
    auto key = ComposeResourceKey(name, resourceUuid);
    std::unique_lock<std::shared_mutex> lock(mtx_);
    bool isNew = ledger_.find(key) == ledger_.end();
    if (!isNew) {
        UBSE_LOG_ERROR << "Ledger put: entry already exists, reject overwrite: name=" << name
                       << ", resourceUuid=" << resourceUuid;
        return false;
    }
    ledger_[key] = std::move(entry);
    UBSE_LOG_INFO << "Ledger put: name=" << name << ", resourceUuid=" << resourceUuid;
    return true;
}

bool UbseSsuDebtLedger::Upsert(const std::string &name, const std::string &resourceUuid,
                               std::shared_ptr<const UbseSsuLedgerEntry> entry)
{
    if (!entry) {
        UBSE_LOG_ERROR << "Ledger upsert: entry is null";
        return false;
    }
    auto key = ComposeResourceKey(name, resourceUuid);
    std::unique_lock<std::shared_mutex> lock(mtx_);
    bool replaced = ledger_.find(key) != ledger_.end();
    ledger_[key] = std::move(entry);
    UBSE_LOG_INFO << "Ledger upsert: name=" << name << ", resourceUuid=" << resourceUuid
                  << ", replaced=" << replaced;
    return replaced;
}

std::shared_ptr<const UbseSsuLedgerEntry> UbseSsuDebtLedger::Get(const std::string &name,
                                                                 const std::string &resourceUuid) const
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = ledger_.find(ComposeResourceKey(name, resourceUuid));
    return it != ledger_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> UbseSsuDebtLedger::GetByName(const std::string &name) const
{
    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> entries;
    std::shared_lock<std::shared_mutex> lock(mtx_);
    for (const auto &[key, entry] : ledger_) {
        // 复合键以name为前缀（'\x1f'分隔），前缀匹配即属于该name
        if (key.size() > name.size() && key.compare(0, name.size(), name) == 0 && key[name.size()] == '\x1f') {
            entries.push_back(entry);
        }
    }
    return entries;
}

bool UbseSsuDebtLedger::Modify(const std::string &name, const std::string &resourceUuid,
                               std::function<void(UbseSsuLedgerEntry &)> modify)
{
    auto key = ComposeResourceKey(name, resourceUuid);
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto it = ledger_.find(key);
    if (it == ledger_.end()) {
        UBSE_LOG_WARN << "Ledger entry not found for modify: name=" << name << ", resourceUuid=" << resourceUuid;
        return false;
    }
    auto oldEntry = it->second;
    auto newEntry = std::make_shared<UbseSsuLedgerEntry>(*oldEntry);
    modify(*newEntry);
    ledger_[key] = std::move(newEntry);
    return true;
}

bool UbseSsuDebtLedger::Remove(const std::string &name, const std::string &resourceUuid)
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    auto erased = ledger_.erase(ComposeResourceKey(name, resourceUuid));
    if (erased > 0) {
        UBSE_LOG_INFO << "Ledger remove: name=" << name << ", resourceUuid=" << resourceUuid;
        return true;
    }
    UBSE_LOG_WARN << "Ledger entry not found for remove: name=" << name << ", resourceUuid=" << resourceUuid;
    return false;
}

bool UbseSsuDebtLedger::RemoveByName(const std::string &name)
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    bool erasedAny = false;
    for (auto it = ledger_.begin(); it != ledger_.end();) {
        if (it->first.size() > name.size() && it->first.compare(0, name.size(), name) == 0 &&
            it->first[name.size()] == '\x1f') {
            it = ledger_.erase(it);
            erasedAny = true;
        } else {
            ++it;
        }
    }
    if (erasedAny) {
        UBSE_LOG_INFO << "Ledger remove by name: name=" << name;
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

// ---- name维度便捷重载：测试与name唯一场景的便利入口，生产路径应优先复合键接口 ----

std::shared_ptr<const UbseSsuLedgerEntry> UbseSsuDebtLedger::Get(const std::string &name) const
{
    auto entries = GetByName(name);
    return entries.size() == 1 ? entries[0] : nullptr;
}

bool UbseSsuDebtLedger::Put(const std::string &name, std::shared_ptr<const UbseSsuLedgerEntry> entry)
{
    if (!entry) {
        return false;
    }
    // 先取uuid再move：实参求值顺序不确定，move后entry为空，再解引用会段错误
    const std::string &resourceUuid = entry->allocResult.resourceUuid;
    return Put(name, resourceUuid, std::move(entry));
}

bool UbseSsuDebtLedger::Modify(const std::string &name, std::function<void(UbseSsuLedgerEntry &)> modify)
{
    auto entries = GetByName(name);
    if (entries.size() != 1 || entries[0] == nullptr) {
        return false;
    }
    return Modify(name, entries[0]->allocResult.resourceUuid, std::move(modify));
}

bool UbseSsuDebtLedger::Remove(const std::string &name)
{
    return RemoveByName(name);
}

static void RebuildEntry(const std::string &name, const std::string &resourceUuid,
                         const std::vector<const UbseSsuDevNameSpace *> &nsList,
                         UbseSsuDebtLedger &ledger, const std::string &tenant, bool overwrite = true)
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
    // 三态恢复分配策略（STRIPED/LINEAR/NORMAL），与CreateDevNameSpaces写入customData.allocStrategy一致；
    // 非法值防御性回退NORMAL（最保守默认值，不假定存在聚合块设备）
    if (first.customData.allocStrategy > static_cast<uint8_t>(UbseSsuAllocStrategy::NORMAL)) {
        UBSE_LOG_ERROR << "RebuildEntry: invalid allocStrategy=" << static_cast<int>(first.customData.allocStrategy)
                       << ", fallback to NORMAL, name=" << name;
        req.strategy = UbseSsuAllocStrategy::NORMAL;
    } else {
        req.strategy = static_cast<UbseSsuAllocStrategy>(first.customData.allocStrategy);
    }
    std::string userName(first.customData.userName,
                         strnlen(first.customData.userName, sizeof(first.customData.userName)));

    UbseSsuAllocResult allocResult;
    allocResult.name = name;
    allocResult.resourceUuid = resourceUuid;
    allocResult.strategy = req.strategy;
    for (const auto *ns : nsList) {
        UbseSsuNameSpaceInfo info;
        info.tgtEid = ns->subSystem.eid;
        info.tgtNqn = ns->subSystem.subNqn;
        auto uuidStr = StrToUuid(ns->uuid);
        if (uuidStr.empty()) {
            UBSE_LOG_ERROR << "RebuildEntry: failed to convert uuid, skip this ns, name=" << name
                           << ", nsId=" << ns->namespaceId;
            continue; // 只跳过当前异常 namespace
        }
        info.nsUuid = uuidStr;
        info.namespaceId = ns->namespaceId;
        // 与CreateDevNameSpaces生成的持久化路径保持一致：/dev/disk/by-id/nvme-uuid.<标准连字符UUID>
        info.nsDevPath = std::string("/dev/disk/by-id/nvme-uuid.") + uuidStr;
        // nsze单位为LBA数量，乘LBA大小换算为字节，与创建路径的nsSize口径保持一致
        info.nsSize = ns->nsze * static_cast<uint64_t>(req.lbaFormat);
        info.lbaFormat = req.lbaFormat;
        allocResult.nameSpaceList.push_back(info);
    }

    UbseSsuLedgerEntry entry;
    entry.name = name;
    entry.allocReq = req;
    if (nsList[0]->namespaceId == 0) {
        UBSE_LOG_ERROR << "Rebuild entry: invalid namespaceId(0) for name=" << name;
        return;
    }
    // 挂载状态由后续将由本地持久化文件记录，不依赖此Rebuild结果，后续添加实现
    entry.state = UbseSsuNsState::CREATED;
    entry.allocResult = allocResult;
    // 硬件恢复标记：恢复收敛窗口守卫据此区分重启前已存在的空间与窗口内新分配的空间
    entry.restoredFromHardware = true;

    // Upsert覆盖而非跳过：主备回切时进程级单例仍驻留上一任期条目，以当前硬件清单刷新
    // （state=CREATED、attachNodes空），陈旧attachNodes会让对账跳过本应触发的REMOUNT；
    // 不在扫描中的条目维持不清（防部分扫描损伤，存亡由FAULTED增量对账裁决）。
    // 增量收编（overwrite=false）走Put只插不覆盖：账本已有同键条目（如分配流程ledger-first
    // 刚Put的CREATING在途条目）不可被硬件扫描重置
    if (overwrite) {
        const bool replaced = ledger.Upsert(entry.name, resourceUuid, std::make_shared<const UbseSsuLedgerEntry>(entry));
        if (replaced) {
            UBSE_LOG_INFO << "Rebuild entry: replaced stale entry from previous tenure: name=" << name
                          << ", resourceUuid=" << resourceUuid;
        }
    } else if (!ledger.Put(entry.name, resourceUuid, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
        UBSE_LOG_INFO << "Absorb entry: ledger key already exists, skip: name=" << name
                      << ", resourceUuid=" << resourceUuid;
    }
}

// 硬件设备按(customData.name, customData.resourceUuid)二级分组（value为NS指针列表）。
// 同name不同resourceUuid（脑裂合并遗留）各自成组；resourceUuid为空（遗留数据）同name归并。
// Rebuild与AbsorbHardwareGroups共用此分组口径
static std::unordered_map<std::string, std::unordered_map<std::string, std::vector<const UbseSsuDevNameSpace *>>>
GroupHardwareDevices(const std::vector<UbseSsuDevInfo> &devices)
{
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<const UbseSsuDevNameSpace *>>> groups;
    for (const auto &dev : devices) {
        for (const auto &ns : dev.nameSpaces) {
            if (ns.customData.name[0] == '\0') {
                continue;
            }
            std::string nsName(ns.customData.name, strnlen(ns.customData.name, sizeof(ns.customData.name)));
            if (nsName.empty()) {
                continue;
            }
            groups[nsName][ReadCustomDataResourceUuid(ns.customData)].push_back(&ns);
        }
    }
    return groups;
}

void UbseSsuDebtLedger::Rebuild(const std::vector<UbseSsuDevInfo> &devices)
{
    // 重建期间置未就绪：主备回切再次触发Rebuild时，上一任期的就绪态仍残留，
    // 不清位则verify/attach/对账可并发消费半新半旧的混合账本（对账窗口判定用旧rebuildTime_
    // 早已超窗）；置位后统一走MASTER_NOT_READY/跳过本轮/继续等待的保守路径，完成后置回
    SetReady(false);
    auto &ledger = *this;
    auto groups = GroupHardwareDevices(devices);
    size_t entryCount = 0;

    for (const auto &[name, uuidGroups] : groups) {
        for (const auto &[resourceUuid, nsList] : uuidGroups) {
            if (nsList.empty()) {
                UBSE_LOG_WARN << "Rebuild debt ledger entry: nsList is empty: name=" << name
                              << ", resourceUuid=" << resourceUuid;
                continue;
            }
            // 同一个请求name，来自同一个租户，每个namespace的tenant都应相同
            std::string tenant(nsList[0]->customData.tenant,
                               strnlen(nsList[0]->customData.tenant, sizeof(nsList[0]->customData.tenant)));
            RebuildEntry(name, resourceUuid, nsList, ledger, tenant);
            entryCount++;
        }
    }

    // 同名多份（脑裂合并遗留的典型特征）：入账后显式告警并列出各组uuid与NS数，
    // 供运维直接取uuid执行detach/free消歧，无需再查alloc_detail。对账3a不覆盖此场景
    // （两批均为合法账本条目，硬件无孤儿），此处是感知脑裂同名冲突的唯一时机
    for (const auto &[name, uuidGroups] : groups) {
        if (uuidGroups.size() <= 1) {
            continue;
        }
        std::string uuidList;
        for (const auto &[resourceUuid, nsList] : uuidGroups) {
            if (!uuidList.empty()) {
                uuidList += ",";
            }
            uuidList += resourceUuid + "(" + std::to_string(nsList.size()) + "ns)";
        }
        UBSE_LOG_WARN << "Ledger rebuild: duplicated name with multiple resourceUuid (suspected split-brain "
                         "residue), name=" << name << ", count=" << uuidGroups.size()
                      << ", groups=[" << uuidList << "], hint: detach/free with resourceUuid to disambiguate";
    }

    // Rebuild完成即视为账本就绪（空账本也是真实状态：硬件上确实无已分配空间）
    SetReady(true);
    // 记录Rebuild完成时间，启动恢复收敛窗口（跨线程可见，与ready_同经同步原语保护）
    {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        rebuildTime_ = std::chrono::steady_clock::now();
    }

    UBSE_LOG_INFO << "Ledger rebuild complete: " << entryCount << " entries restored";
}

// NODE_UP差异场景的增量收编（与Rebuild的本质差异）：
// - 账本缺失的硬件组才入账（Put只插不覆盖），既有条目state/attachNodes/devName一律保留——
//   本master账本持续维护、视图可信，不可像主备回切那样全量清空重置（否则free"需先卸载"
//   门禁失效可误删挂载中空间，且agent需重报收敛）
// - 不SetReady(false)：收编是纯新增写（账本mtx_保证原子），不产生半新半旧账本
// - 仅在收编新条目时刷新rebuildTime_开收敛窗口（新条目attachNodes空，防他节点抢挂独占空间）
// - 真孤儿（同名无任何账本条目，疑free部分失败残留）与脑裂对端新分配的组在硬件面不可区分，
//   统一收编（脑裂资源agent重报需命中条目），真孤儿额外打LedgerOrphanAbsorbedAlarm留痕转人工
// 返回本次新收编的条目个数（0表示无新增，供调用方记日志）
size_t UbseSsuDebtLedger::AbsorbHardwareGroups(const std::vector<UbseSsuDevInfo> &devices)
{
    auto groups = GroupHardwareDevices(devices);
    size_t absorbedCount = 0;
    for (const auto &[name, uuidGroups] : groups) {
        for (const auto &[resourceUuid, nsList] : uuidGroups) {
            if (nsList.empty()) {
                continue;
            }
            // 同名条目一次查出：既有键判定与真孤儿判定共用
            const auto sameNameEntries = GetByName(name);
            bool tracked = false;
            for (const auto &entry : sameNameEntries) {
                if (entry->allocResult.resourceUuid == resourceUuid) {
                    tracked = true;
                    break;
                }
            }
            if (tracked) {
                continue; // 既有条目不动：state/attachNodes由周期对账与agent重报维护
            }
            // 同一个请求name来自同一个租户，每个namespace的tenant都应相同
            std::string tenant(nsList[0]->customData.tenant,
                               strnlen(nsList[0]->customData.tenant, sizeof(nsList[0]->customData.tenant)));
            RebuildEntry(name, resourceUuid, nsList, *this, tenant, false);
            absorbedCount++;
            if (sameNameEntries.empty()) {
                // 同名无任何账本条目=真孤儿特征（疑free部分失败残留），与脑裂对端新分配的组
                // 在硬件面不可区分，统一收编但留痕转人工
                UBSE_LOG_WARN << "LedgerOrphanAbsorbedAlarm: hardware group without any same-name ledger entry "
                              << "absorbed, possible free partial-failure residue, manual check required, name="
                              << name << ", resourceUuid=" << resourceUuid << ", nsCount=" << nsList.size();
            }
            UBSE_LOG_INFO << "Absorb entry: hardware group without ledger entry absorbed, name=" << name
                          << ", resourceUuid=" << resourceUuid << ", nsCount=" << nsList.size();
        }
    }
    if (absorbedCount > 0) {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        rebuildTime_ = std::chrono::steady_clock::now();
    }
    return absorbedCount;
}

bool UbseSsuDebtLedger::IsInReconcileWindow() const
{
    std::chrono::steady_clock::time_point rebuildTime;
    {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        rebuildTime = rebuildTime_;
    }
    // 从未Rebuild（首启采集成功前）不构成收敛窗口（该阶段由IsReady门禁兜底）
    if (rebuildTime.time_since_epoch().count() == 0) {
        return false;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - rebuildTime).count();
    return elapsed < SSU_RECONCILE_WINDOW_SECONDS;
}

void UbseSsuDebtLedger::ResetRebuildTime()
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    rebuildTime_ = {};
}

bool UbseSsuDebtLedger::IsReady() const
{
    return ready_.load(std::memory_order_acquire);
}

void UbseSsuDebtLedger::SetReady(bool ready)
{
    ready_.store(ready, std::memory_order_release);
}

} // namespace ubse::ssu::debt
