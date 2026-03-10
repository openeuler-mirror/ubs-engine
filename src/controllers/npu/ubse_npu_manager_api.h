/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* ubs-engine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#ifndef UBSE_NPU_MANAGE_API_H
#define UBSE_NPU_MANAGE_API_H

#include "ubse_common_def.h"
#include "ubse_npu_source_def.h"

namespace ubse::npu::controller {
using ubse::common::def::UbseResult;

void StartCollect();

UbseResult AllocDevicesImpl(const UbseAllocRequest &requestInfo, std::string &newBusInstanceGuid,
                            std::vector<std::shared_ptr<IResource>> &devList);

UbseResult FreeUbDeviceImpl(const UbseAllocRequest &requestInfo);

UbseResult QueryAllDevicesImpl(std::vector<std::shared_ptr<IResource>> &devList);

} // namespace ubse::npu::controller

#endif // UBSE_NPU_MANAGE_API_H