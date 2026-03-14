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

#ifndef MEMPOOLING_OVER_COMMIT_STORAGE_H
#define MEMPOOLING_OVER_COMMIT_STORAGE_H
#include <mutex>
#include <string>
#include <unordered_map>
#include "ubse_def.h"
#include "mp_error.h"
namespace mempooling {

enum class NumaBindType {
    BIND_INVALID,
    BIND_SINGLE,
    BIND_MULTIPLE
};
const std::unordered_map<NumaBindType, std::string> BIND_TYPE_TO_STR_MAP{
    {NumaBindType::BIND_INVALID, "BIND_INVALID"},
    {NumaBindType::BIND_SINGLE, "BIND_SINGLE"},
    {NumaBindType::BIND_MULTIPLE, "BIND_MULTIPLE"}};

const std::unordered_map<std::string, NumaBindType> STR_TO_BIND_TYPE_MAP{
    {"BIND_INVALID", NumaBindType::BIND_INVALID},
    {"BIND_SINGLE", NumaBindType::BIND_SINGLE},
    {"BIND_MULTIPLE", NumaBindType::BIND_MULTIPLE}};

std::string BindTypeToStr(const NumaBindType value);

class OverCommitStorage {
public:
    static OverCommitStorage &Instance()
    {
        static OverCommitStorage instance;
        return instance;
    }
    MpResult UpdateBindTypeDB(const std::string &nodeId, const NumaBindType value);
    MpResult SelectBindTypeDB();
    MpResult GetNumaBindType(const std::string &nodeId, NumaBindType &value);
    MpResult UpdateWaterMark(const uint16_t highWaterMark, const uint16_t lowWaterMark);
    MpResult GetWaterMark(uint16_t &highWaterMark, uint16_t &lowWaterMark);

    MpResult UpdateUint16(const std::string &keyPrefix, const std::string &key, const uint16_t &value);

    MpResult GetNumaBindTypeRawData(UbseByteBuffer &data, bool needLock);
    MpResult PutNumaBindTypeRawData(UbseByteBuffer &data);
    std::unordered_map<std::string, NumaBindType>& getNodeNumaBindMap();

private:
    OverCommitStorage() = default;
    OverCommitStorage(const OverCommitStorage &) = delete;
    OverCommitStorage &operator=(const OverCommitStorage &) = delete;
    ~OverCommitStorage() = default;
    MpResult BindTypeMapToStr(std::string &str);

    std::mutex mtx;
    // 每个节点的绑定类型
    std::unordered_map<std::string, NumaBindType> mNodeNumaBindMap;
};
} // namespace mempooling

#endif // MEMPOOLING_OVER_COMMIT_STORAGE_H
