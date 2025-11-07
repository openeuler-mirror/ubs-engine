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

#include "ubse_mem_common_utils.h"
#include "ubse_common_def.h"
#include "ubse_mem_def.h"
#include "ubse_logger_inner.h"

namespace ubse::mmi {

UbseResult ubse::mmi::RmCommonUtils::GetFileFirstLine(const std::string &path, std::string &line)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "Open file " << path << " failed.";
        return E_CODE_OPEN_FILE;
    }
    if (!std::getline(file, line)) {
        UBSE_LOG_ERROR << "Get file context failed.";
        return E_CODE_OPEN_FILE;
    }
    file.close();
    return UBSE_OK;
}

// 映射到 [4, 17] 之间，确保唯一性,生成远端numa
int RmCommonUtils::HashStringToByte(const std::string &input)
{
    static std::hash<std::string> hasher;
    const size_t hashValue = hasher(input);
    return 4 + (hashValue % 14);
}

void RmCommonUtils::GenerateNodeSocketStr(const NodeId &lendNodeId, const SocketId &lendSocketId,
                                          std::string &nodeSocketStr)
{
    std::ostringstream str;
    str << lendNodeId << "-" << lendSocketId;
    nodeSocketStr = str.str();
    UBSE_LOG_DEBUG << "GenerateNodeSocketStr: " << nodeSocketStr;
}
} // namespace ubse::mmi