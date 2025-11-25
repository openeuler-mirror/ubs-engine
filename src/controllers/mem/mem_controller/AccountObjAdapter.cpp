/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include "AccountObjAdapter.h"

namespace ubse::mem::strategy {
std::string AccountObjAdapter::GetExportNode(const UbseMemAlgoResult &algoResult)
{
    if (!algoResult.exportNumaInfos.empty()) {
        return algoResult.exportNumaInfos[0].nodeId;
    }
    return "";
}

std::string AccountObjAdapter::GetImportNode(const UbseMemAlgoResult &algoResult)
{
    if (!algoResult.importNumaInfos.empty()) {
        return algoResult.importNumaInfos[0].nodeId;
    }
    return "";
}
} // namespace ubse::mem::strategy