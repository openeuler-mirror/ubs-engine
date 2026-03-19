/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEM_JSON_DEF_H
#define MEM_JSON_DEF_H

#include <string>
#include <vector>

#include "mp_error.h"
#include "mp_json_util.h"
#include "ubse_str_util.h"

namespace mempooling {

constexpr auto STRANDED_BORROW_REQUEST = "WATER_BORROW";

struct RackMemNumaLoc {
    std::string nodeId; // 节点id
    int socketId;       // socket id
    int64_t numaId{};   // numa id
    bool operator==(const RackMemNumaLoc &other) const
    {
        return nodeId == other.nodeId && socketId == other.socketId && numaId == other.numaId;
    }
    // 重载 < 运算符
    bool operator<(const RackMemNumaLoc &other) const
    {
        if (nodeId != other.nodeId) {
            return nodeId < other.nodeId;
        }
        if (socketId != other.socketId) {
            return socketId < other.socketId;
        }
        return numaId < other.numaId;
        ;
    }
    friend std::ostream &operator<<(std::ostream &os, const RackMemNumaLoc &obj)
    {
        return os << "nodeId: " << obj.nodeId << " socketId: " << obj.socketId << " numaId: " << obj.numaId;
    }
};

struct MemMallocAttr {
    std::string srcNid;    /* *内存申请需求方节点 */
    int srcSocket{};       /* *内存申请需求方节点socket信息 -1 无效 */
    int srcNuma{};         /* *内存申请需求方节点NUMA信息  -1 无效 */
    uid_t uid{0};          /* *内存申请需求方用户uid */
    std::string username{}; /* *内存申请需求方用户名 */
    int dstNodeNum{};      /* *借用是否从单节点还是多个节点 */
    int lenderNumaSize{0}; // 最大值为2,小于等于0，不指定借出
    std::vector<RackMemNumaLoc> lenderLocs{};
    std::vector<uint64_t> lenderSizes{}; // 内部单位，字节
};

enum PerfLevel {
    L0, /* *L0对应直连节点 */
    L1, /* *L1对应通过1跳节点，暂不支持 */
    L2  /* *L2对应过超过1跳节点 ，暂不支持 */
};

struct RackCreateResourceWaterBorrowAttr {
    std::string ToJson();

    bool FromJson(const std::string &jsonString);
    bool ParseJsonString(const std::string &jsonString, JSON_MAP &strMap);
    bool ParseBasicAttributes(JSON_MAP &strMap);
    bool ParseWaterMallocAttr(JSON_MAP &strMap, JSON_MAP &waterMallocAttrMap);
    bool ParseVecOfWaterMallocAttr(JSON_MAP &waterMallocAttrMap);

    int64_t numaPresentId{-1}; // 用户不需要设置，算法决策时会决策出
    std::string type{STRANDED_BORROW_REQUEST};
    std::string name{};
    size_t size{0};                     // 必填，单位字节
    PerfLevel perfLevel{PerfLevel::L0}; // 必填，有默认值
    size_t highWatermark{88};           // 必填，算法百分比，单位%
    size_t lowWatermark{11};            // 必填，算法比分在，单位%
    MemMallocAttr waterMallocAttr{};    // 水线借用必填
};

} // namespace mempooling

#endif // MEM_JSON_DEF_H