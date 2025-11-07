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

#ifndef UBSE_MEM_CONTROLLER_MSG_H
#define UBSE_MEM_CONTROLLER_MSG_H

#include "ubse_com_module.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"

namespace ubse::mem::controller {
using namespace ubse::com;
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;

void RegUbseMemControllerHandler();

/**
 * 同步消息，主节点采集节点拓扑
 * @param nodeId [in] 采集节点id
 * @param info [out] agent节点拓扑
 * @return
 */
UbseResult CollectLedge(const std::string &nodeId, NodeMemDebtInfo &info);

UbseResult CollectLedgeHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryFdImportObj(const std::string &nodeId, const std::string &name, UbseMemFdBorrowImportObj &info);

UbseResult QueryFdImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryNumaImportObj(const std::string &nodeId, const std::string &name, UbseMemNumaBorrowImportObj &info);

UbseResult QueryNumaImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_MSG_H
