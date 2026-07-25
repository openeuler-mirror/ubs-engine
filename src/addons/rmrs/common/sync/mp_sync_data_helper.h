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

#ifndef MP_SYNC_DATA_HELPER_H
#define MP_SYNC_DATA_HELPER_H

#include <map>
#include <string>

#include "ubse_def.h"
#include "ubse_com.h"
#include "ubse_storage.h"
#include "mempooling_message.h"
#include "mp_anti_param_json_util.h"
#include "mp_error.h"
#include "mp_module.h"
#include "rmrs_serialize.h"

namespace mempooling::sync {
using namespace ubse::com;
using namespace mempooling::message;
using namespace rmrs::serialize;
using namespace ubse::storage;

using NotifyFunc = std::function<uint32_t(UbseByteBuffer&)>;
using GetDataFunc = std::function<uint32_t(UbseByteBuffer&)>;

struct MpSyncDataParam {
    uint64_t index{};
    NotifyFunc notify;
    GetDataFunc getData;
};

class MpSyncDataHelper {
public:
    static MpSyncDataHelper& Instance();
    MpResult Init();
    MpResult DeInit();
    MpResult SyncDataToNode(const std::vector<std::string>& nodeIdList, const uint32_t& opCode,
                            const UbseByteBuffer& data);

private:
    std::map<std::string, MpSyncDataParam> serviceTable;
};

class MpSyncDataSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        auto ret = MpSyncDataHelper::Instance().Init();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        MpSyncDataHelper::Instance().DeInit();
        return;
    }
    const std::string Name() override
    {
        return "SyncData";
    }
};

} // namespace mempooling::sync

#endif