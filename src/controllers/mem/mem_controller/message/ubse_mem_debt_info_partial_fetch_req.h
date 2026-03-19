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

#ifndef UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_REQ_H
#define UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_REQ_H
#include <algorithm>
#include <sstream>
#include "ubse_base_message.h"
#include "ubse_serial_util.h"
#include "ubse_mem_debt_info_partial_fetch_res.h"
namespace ubse::mem::controller::message {
enum class DebtFetchType {
    INIT = 1,
    EXPORT = 2,
    IMPORT = 3
};

constexpr int MAX_PAGES = 4000;
constexpr int EACH_PAGE_SIZE = 150;

constexpr int MAX_PAGE_NUM = (MAX_PAGES + EACH_PAGE_SIZE - 1) / EACH_PAGE_SIZE;

struct DebtFetchInfo {
    std::string nodeId{};
    std::string name{};
    AccountType borrowType{ AccountType::INIT };
    int pageNum{};
    int pageSize{};
    DebtFetchType type{ DebtFetchType::INIT };

    bool CheckFilter(std::string &error)
    {
        if (name.length() > 47) { // max length is 47
            error = std::string("The length of the name," + std::to_string(name.length()) + ", has exceeded 47.");
            return false;
        }

        if (!std::all_of(name.begin(), name.end(), [](char c) {
                return std::isdigit(c) || std::isalpha(c) || c == '_' || c == '-' || c == '.' || c == ':';
            })) {
            error = std::string("The format of the name does not meet the requirements.");
            return false;
        }
        return true;
    }

    static bool Serialize(ubse::serial::UbseSerialization &in, DebtFetchInfo &data)
    {
        in << data.nodeId << data.name << ubse::serial::enum_v<AccountType>(data.borrowType) << data.pageNum <<
            data.pageSize << ubse::serial::enum_v<DebtFetchType>(data.type);
        return in.Check();
    }
    static bool Deserialize(ubse::serial::UbseDeSerialization &out, DebtFetchInfo &data)
    {
        out >> data.nodeId >> data.name >> ubse::serial::enum_v<AccountType>(data.borrowType) >> data.pageNum >>
            data.pageSize >> ubse::serial::enum_v<DebtFetchType>(data.type);
        return out.Check();
    }
    static std::string DebtFetchTypeToString(DebtFetchType fetchType)
    {
        std::string result;
        switch (fetchType) {
            case DebtFetchType::INIT:
                result = "INIT";
                break;
            case DebtFetchType::EXPORT:
                result = "EXPORT";
                break;
            case DebtFetchType::IMPORT:
                result = "IMPORT";
                break;
            default:
                result = "UNKNOWN";
                break;
        }
        return result;
    }
    [[nodiscard]] std::string toString() const
    {
        std::ostringstream oss;
        oss << "nodeId: " << nodeId << ", name: " << name << ", borrowType: " <<
            AccountTypeUtil::AccountTypeToString(borrowType) << ", pageNum: " << pageNum << ", pageSize: " <<
            pageSize << ", type: " << DebtFetchTypeToString(type);
        return oss.str();
    }
};

class UbseMemDebtInfoPartialFetchReq : public ubse::message::UbseBaseMessage {
public:
    UbseMemDebtInfoPartialFetchReq() = default;
    explicit UbseMemDebtInfoPartialFetchReq(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseMemDebtFetchInfo(DebtFetchInfo &info)
    {
        this->debtFetchInfo_ = std::move(info);
    }

    [[nodiscard]] inline const DebtFetchInfo &GetUbseMemDebtFetchInfo() const
    {
        return this->debtFetchInfo_;
    }

    ubse::message::UbseResult Serialize() override;

    ubse::message::UbseResult Deserialize() override;

private:
    DebtFetchInfo debtFetchInfo_{};
};
using UbseMemDebtInfoPartialFetchReqPtr = ubse::message::Ref<UbseMemDebtInfoPartialFetchReq>;
} // namespace ubse::mem::controller::message
#endif // UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_REQ_H
