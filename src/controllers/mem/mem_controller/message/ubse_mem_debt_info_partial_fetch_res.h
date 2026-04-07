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

#ifndef UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_RES_H
#define UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_RES_H
#include <algorithm>
#include <sstream>
#include <vector>
#include "ubse_base_message.h"
#include "ubse_serial_util.h"
namespace ubse::mem::controller::message {
enum class AccountType {
    INIT,
    NUMA,
    FD,
    SHM,
    ADDR
};

namespace AccountTypeUtil {
const std::unordered_map<AccountType, std::string> enumToStringMap = { { AccountType::NUMA, "numa" },
                                                                       { AccountType::FD, "fd" },
                                                                       { AccountType::SHM, "share" },
                                                                       { AccountType::ADDR, "addr" } };
inline std::string AccountTypeToString(AccountType type)
{
    auto it = enumToStringMap.find(type);
    if (it != enumToStringMap.end()) {
        return it->second;
    }
    return "";
}
const std::unordered_map<std::string, AccountType> stringToEnumMap = { { "numa", AccountType::NUMA },
                                                                       { "fd", AccountType::FD },
                                                                       { "share", AccountType::SHM },
                                                                       { "addr", AccountType::ADDR } };
inline AccountType StringToAccountType(const std::string &str)
{
    auto it = stringToEnumMap.find(str);
    if (it != stringToEnumMap.end()) {
        return it->second;
    }
    return AccountType::INIT;
}
} // namespace AccountTypeUtil

struct numaLendInfo {
    int socketId{ -1 };
    int64_t numaId{ -1 };
    uint64_t size{ 0 };
};

struct FlatDebtInformation {
    std::string name{};
    AccountType type{ AccountType::INIT };
    std::uint32_t status{};
    std::string importId{};
    std::string lendId{};
    std::vector<numaLendInfo> numaLendInfos;
    std::string handle;

    static bool Serialize(ubse::serial::UbseSerialization &out, const std::vector<FlatDebtInformation> &data)
    {
        out << (ubse::serial::right_v<size_t>(data.size()));

        for (const auto &item : data) {
            out << item.name << ubse::serial::enum_v(item.type) << item.status << item.importId << item.lendId <<
                item.handle;

            out << (ubse::serial::right_v<size_t>(item.numaLendInfos.size()));

            for (const auto &info : item.numaLendInfos) {
                out << info.socketId << info.numaId << info.size;
            }
        }

        return out.Check();
    }

    static bool Deserialize(ubse::serial::UbseDeSerialization &in, std::vector<FlatDebtInformation> &data)
    {
        size_t totalItems = 0;
        in >> totalItems;
        if (!in.Check()) {
            return false;
        }

        for (size_t i = 0; i < totalItems; ++i) {
            FlatDebtInformation item;

            in >> item.name >> ubse::serial::enum_v(item.type) >> item.status >> item.importId >> item.lendId >>
                item.handle;
            if (!in.Check()) {
                return false;
            }

            size_t lendCount = 0;
            in >> lendCount;
            if (!in.Check()) {
                return false;
            }

            for (size_t j = 0; j < lendCount; ++j) {
                numaLendInfo info;
                in >> info.socketId >> info.numaId >> info.size;
                if (!in.Check()) {
                    return false;
                }
                item.numaLendInfos.push_back(info);
            }

            data.push_back(std::move(item));
        }

        return in.Check();
    }

    static std::string toString(const FlatDebtInformation &info)
    {
        std::ostringstream oss;
        oss << "FlatDebtInformation{"
            << "name=\"" << info.name << "\", "
            << "type=\"" << AccountTypeUtil::AccountTypeToString(info.type) << "\", "
            << "status=" << info.status << ", "
            << "importId=\"" << info.importId << "\", "
            << "lendId=\"" << info.lendId << "\", "
            << "handle=\"" << info.handle << "\", "
            << "numaLendInfos=[ ";

        for (size_t i = 0; i < info.numaLendInfos.size(); ++i) {
            const auto &lendInfo = info.numaLendInfos[i];
            oss << "(" << lendInfo.socketId << ", " << lendInfo.numaId << ", " << lendInfo.size << ")";

            if (i != info.numaLendInfos.size() - 1) {
                oss << ", ";
            }
        }

        oss << " ]}";

        return oss.str();
    }
};

struct PartialFetchRes {
    bool NeedToContinue{ false };
    std::vector<FlatDebtInformation> flatDebt;
    static bool Serialize(ubse::serial::UbseSerialization &out, const PartialFetchRes &data)
    {
        out << data.NeedToContinue;
        return FlatDebtInformation::Serialize(out, data.flatDebt);
    }

    static bool Deserialize(ubse::serial::UbseDeSerialization &in, PartialFetchRes &data)
    {
        in >> data.NeedToContinue;
        if (!in.Check()) {
            return false;
        }
        return FlatDebtInformation::Deserialize(in, data.flatDebt);
    }

    static std::string toString(const PartialFetchRes &res)
    {
        std::ostringstream oss;
        oss << "PartialFetchRes{"
            << "NeedToContinue=" << (res.NeedToContinue ? "true" : "false") << ", "
            << "flatDebt=[ ";

        for (size_t i = 0; i < res.flatDebt.size(); ++i) {
            oss << FlatDebtInformation::toString(res.flatDebt[i]);
            if (i != res.flatDebt.size() - 1) {
                oss << ", ";
            }
        }
        oss << " ]}";
        return oss.str();
    }
};

class UbseMemDebtInfoPartialFetchRes : public ubse::message::UbseBaseMessage {
public:
    UbseMemDebtInfoPartialFetchRes() = default;
    explicit UbseMemDebtInfoPartialFetchRes(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetFlatDebtInformationVec(PartialFetchRes &info)
    {
        this->partialFetchRes = std::move(info);
    }
    inline PartialFetchRes &GetFlatDebtInformationVec()
    {
        return this->partialFetchRes;
    }
    ubse::message::UbseResult Serialize() override;

    ubse::message::UbseResult Deserialize() override;

private:
    PartialFetchRes partialFetchRes{};
};
using UbseMemDebtInfoPartialFetchResPtr = ubse::message::Ref<UbseMemDebtInfoPartialFetchRes>;
} // namespace ubse::mem::controller::message
#endif // UBS_ENGINE_UBSE_MEM_DEBT_INFO_PARTIAL_FETCH_RES_H
