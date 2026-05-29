/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MSG_SERIALIZE_H
#define MSG_SERIALIZE_H

#include <cstdint>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
#include "ubse_mem_controller.h"
#include "turbo_ucache_interface.h"
#include "ucache_json_util.h"

namespace ucache {
namespace deserialize {

using namespace ubse::mem::controller;
using namespace turbo::ucache;

struct LentNumaInfo {
    int16_t numaId;
    uint64_t lentSize;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaId:[" << numaId << "],";
        oss << "lentSize:[" << lentSize << "],";
        oss << "}";
        return oss.str();
    }
};

struct BorrowMemInfo {
    std::string name;
    uint64_t size;
    std::string lentNodeId;
    std::vector<LentNumaInfo> lentNumaInfos;
    std::string borrowNodeId;
    int16_t borrowLocalNuma;
    int64_t borrowRemoteNuma;
    std::vector<uint64_t> lentMemId{};   // 借出节点mem_id
    std::vector<uint64_t> borrowMemId{}; // 借入节点mem_id

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "name:[" << name << "],";
        oss << "size:[" << size << "],";
        oss << "lentNodeId:[" << lentNodeId << "],";
        oss << "lentNumaInfos:[";
        for (const auto& numaInfo : lentNumaInfos) {
            oss << numaInfo.ToString() << ",";
        }
        oss << "],";
        oss << "borrowNodeId:[" << borrowNodeId << "],";
        oss << "borrowLocalNuma:[" << borrowLocalNuma << "],";
        oss << "borrowRemoteNuma:[" << borrowRemoteNuma << "],";
        oss << "lentMemId:[";
        for (const auto& memid : lentMemId) {
            oss << memid << ",";
        }
        oss << "],";
        oss << "borrowMemId:[";
        for (const auto& memid : borrowMemId) {
            oss << memid << ",";
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

class Deserialize {
public:
    static uint32_t GetBorrowMemInfo(const std::string& nodeId, std::vector<BorrowMemInfo>& borrowMem);
    static uint32_t GetNodeMemInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList);
    static uint32_t GetNumaInfos(std::map<std::string, std::map<std::string, NodeInfo>>& nodeInfos);
    static uint32_t GetCgroupInfos(
        std::queue<std::map<std::string, uint64_t>>& timeStampsCgroup,
        std::queue<std::map<std::string, std::map<std::string, CgroupInfos>>>& cgroupInfosQueue);
    static uint32_t GetMemWaterMark(std::map<std::string, uint64_t>& memWaterMarkInfos);

private:
    static uint32_t ParseBorrowLendMemInfo(const std::vector<UbseNumaMemoryDebtInfo>& extInfos);
    static void ParseNodeNumaMemInfo(const std::vector<UbseNodeNumaInfo>& extInfos);

    static std::vector<BorrowMemInfo> borrowMemInfos;
};

} // namespace deserialize
} // namespace ucache

#endif /* MSG_SERIALIZE_H */
