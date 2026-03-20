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

#ifndef OVER_COMMIT_SERIALIZER_H
#define OVER_COMMIT_SERIALIZER_H

#include <string>
#include <utility>
#include <vector>

#include "rmrs_resource_query.h"

namespace mempooling::over_commit {
constexpr int SERIALIZER_OK = 0;
constexpr int SERIALIZER_ERROR = 1;

class PidNumaInfoCollectParam {
public:
    PidNumaInfoCollectParam() = default;
    explicit PidNumaInfoCollectParam(std::vector<pid_t> pidList) : pidList(std::move(pidList)) {}
    std::vector<pid_t> pidList{};
};

class PidNumaInfoCollectResult {
public:
    PidNumaInfoCollectResult() = default;
    explicit PidNumaInfoCollectResult(std::vector<mempooling::RmrsPidInfo> pidInfoList)
        : pidInfoList(std::move(pidInfoList)) {}
    std::vector<mempooling::RmrsPidInfo> pidInfoList{};
};

class NumaMemInfoCollectParam {
public:
    NumaMemInfoCollectParam() = default;
    explicit NumaMemInfoCollectParam(int numaId) : numaId(numaId) {}
    int numaId{};
};

} // namespace mempooling::over_commit

#endif