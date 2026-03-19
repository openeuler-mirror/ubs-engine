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

#ifndef SMAP_REMOTE_PROCESS_QUERY_TRANS_MSG_H
#define SMAP_REMOTE_PROCESS_QUERY_TRANS_MSG_H

#include "mempooling_interface.h"
#include "over_commit_def.h"
#include "rmrs_serialize.h"
#include "common_delete_func.h"
namespace mempooling {
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;
using namespace mempooling::over_commit;
using namespace mempooling::outinterface;

struct SmapRemoteProcessQueryTrans {
    std::vector<uint32_t> numaIds;
};

class SmapRemoteProcessQueryTransMsg {
public:
    SmapRemoteProcessQueryTransMsg() = default;

    explicit SmapRemoteProcessQueryTransMsg(SmapRemoteProcessQueryTrans smapRemoteProcessQueryTrans)
        : _smapRemoteProcessQueryTrans(std::move(smapRemoteProcessQueryTrans))
    {
    }

    inline SmapRemoteProcessQueryTrans GetSmapRemoteProcessQueryTrans() const
    {
        return _smapRemoteProcessQueryTrans;
    }

    SmapRemoteProcessQueryTrans _smapRemoteProcessQueryTrans{};
};
} // namespace mempooling
#endif // SMAP_REMOTE_PROCESS_QUERY_TRANS_MSG_H