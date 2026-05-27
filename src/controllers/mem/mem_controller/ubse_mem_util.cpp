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

#include "ubse_mem_util.h"

#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_os_util.h"
#include "ubs_engine_mem.h"

namespace ubse::mem::util {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::mti;
using namespace ubse::task_executor;
using namespace ubse::utils;

std::string GetCurNodeId()
{
    auto& ubseContext = ubse::context::UbseContext::GetInstance();
    auto module = ubseContext.GetModule<UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "get the lcne module failed.";
        return "";
    }
    MtiNodeInfo info{};
    auto ret = module->UbseGetLocalNodeInfo(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "query local info failed, " << FormatRetCode(ret);
        return "";
    }
    return info.nodeId;
}

UbseTaskExecutorPtr GetExecutor(const std::string& name)
{
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return nullptr;
    }
    auto resourceExecutor = taskExecutor->Get(name);
    if (resourceExecutor == nullptr) {
        return nullptr;
    }
    return resourceExecutor;
}

bool CheckName(const std::string& name)
{
    if (name.empty() || name.size() >= UBS_MEM_MAX_NAME_LENGTH) {
        UBSE_LOG_ERROR << "Invalid name length:" << name.size();
        return false;
    }
    for (unsigned char c : name) {
        if (!isdigit(c) && !isalpha(c) && c != '_' && c != '-' && c != '.' && c != ':') {
            UBSE_LOG_ERROR << "Invalid name char: " << c;
            return false;
        }
    }
    return true;
}

bool isNumericString(const std::string& str)
{
    return !str.empty() && std::all_of(str.begin(), str.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}

UbseUdsInfo GenUdsInfo(const api::server::UbseRequestContext& context)
{
    UbseUdsInfo udsInfo = {context.clientInfo.uid, context.clientInfo.gid, context.clientInfo.pid, ""};
    auto ret = UbseOsUtil::GetUserNameById(context.clientInfo.uid, udsInfo.username);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get username, uid=" << context.clientInfo.uid;
    }
    return udsInfo;
}
} // namespace ubse::mem::util