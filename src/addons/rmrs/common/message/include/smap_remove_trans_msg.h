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

#ifndef SMAP_REMOVE_SIMPO_H
#define SMAP_REMOVE_SIMPO_H

#include "over_commit_def.h"
#include "mempooling_interface.h"
#include "common_delete_func.h"
namespace mempooling {
using namespace mempooling::over_commit;
using namespace mempooling::outinterface;
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

struct SmapRemoveTrans {
    std::vector<pid_t> pids;
};

class SmapRemoveTransMsg {
public:
    SmapRemoveTransMsg() = default;

    explicit SmapRemoveTransMsg(SmapRemoveTrans smapRemoveTrans)
        : _smapRemoveTrans(std::move(smapRemoveTrans))
    {
    }

    inline SmapRemoveTrans GetSmapRemoveTrans() const
    {
        return _smapRemoveTrans;
    }
    SmapRemoveTrans _smapRemoveTrans{};
};
}
#endif // SMAP_REMOVE_SIMPO_H
