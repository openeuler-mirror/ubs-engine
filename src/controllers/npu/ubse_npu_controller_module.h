/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_NPU_CONTROLLER_MODULE_H
#define UBSE_NPU_CONTROLLER_MODULE_H
#include <string>
#include <vector>

#include "ubse_context.h"
#include "ubse_npu_source_def.h"

namespace ubse::npu::controller {
using namespace ubse::context;

class UbseNpuControllerModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void Uninitialize() override;

    UbseResult Start() override;

    void Stop() override;

    UbseResult AllocDevices(const UbseAllocRequest &request, std::string &newBusInstanceGuid,
                            std::vector<std::shared_ptr<IResource>> &devList);

    UbseResult FreeUbDevices(std::string &busInstanceGuid);

    UbseResult QueryAllDevices(std::vector<std::shared_ptr<IResource>> &devList);

    UbseResult QueryUbaTidSize(const std::string &busInstanceGuid, Uba_Tid_Size &info);
};
} // namespace ubse::npu::controller
#endif // UBSE_NPU_CONTROLLER_MODULE_H