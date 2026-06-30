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

#ifndef UBSE_SSU_DEBT_LEDGER_H
#define UBSE_SSU_DEBT_LEDGER_H

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "ubse_ssu_def.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::debt {
using namespace ubse::plugin::service::ssu;
using namespace ubse::adapter_plugins::ssu::def;

struct UbseSsuLedgerEntry {
    std::string name;
    UbseSsuAllocSpaceReq allocReq;
    UbseSsuNsState state{UbseSsuNsState::IDLE}; // 后续可能改为每个namespace对应状态，先不改
    UbseSsuAllocResult allocResult;
};

class UbseSsuDebtLedger {
public:
    UbseSsuDebtLedger(const UbseSsuDebtLedger &) = delete;
    UbseSsuDebtLedger &operator=(const UbseSsuDebtLedger &) = delete;

    static UbseSsuDebtLedger &GetInstance();

    bool Put(const std::string &name, std::shared_ptr<const UbseSsuLedgerEntry> entry);

    std::shared_ptr<const UbseSsuLedgerEntry> Get(const std::string &name) const;

    bool Modify(const std::string &name, std::function<void(UbseSsuLedgerEntry &)> modify);

    bool Remove(const std::string &name);

    std::vector<std::shared_ptr<const UbseSsuLedgerEntry>> GetAll() const;

    void Rebuild(const std::vector<UbseSsuDevInfo> &devices);

private:
    UbseSsuDebtLedger() = default;
    ~UbseSsuDebtLedger() = default;
    
    mutable std::shared_mutex mtx_;
    std::unordered_map<std::string, std::shared_ptr<const UbseSsuLedgerEntry>> ledger_;
};

} // namespace ubse::ssu::debt

#endif // UBSE_SSU_DEBT_LEDGER_H
