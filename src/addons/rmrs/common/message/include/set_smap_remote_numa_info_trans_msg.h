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

#ifndef SET_SMAP_REMOTE_NUMA_INFO_SIMPO_H
#define SET_SMAP_REMOTE_NUMA_INFO_SIMPO_H

#include "rmrs_serialize.h"
#include "over_commit_def.h"
#include "mempooling_interface.h"
#include "common_delete_func.h"
namespace mempooling {
using namespace rmrs::serialize;
using namespace mempooling::over_commit;

struct SetSmapRemoteNumaInfoTrans {
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam;
    std::vector<MemBorrowInfo> memBorrowInfos;
};

class SetSmapRemoteNumaInfoTransMsg {
public:
    SetSmapRemoteNumaInfoTransMsg() = default;

    explicit SetSmapRemoteNumaInfoTransMsg(SetSmapRemoteNumaInfoTrans setSmapRemoteNumaInfoTrans)
        : _setSmapRemoteNumaInfoTrans(std::move(setSmapRemoteNumaInfoTrans))
    {
    }

    inline SetSmapRemoteNumaInfoTrans GetSetSmapRemoteNumaInfoTrans() const
    {
        return _setSmapRemoteNumaInfoTrans;
    }
    
    SetSmapRemoteNumaInfoTrans _setSmapRemoteNumaInfoTrans{};
};
}
#endif  // SET_SMAP_REMOTE_NUMA_INFO_SIMPO_H
