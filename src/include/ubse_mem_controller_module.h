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

#ifndef UBSE_MEM_CONTROLLER_MODULE_H
#define UBSE_MEM_CONTROLLER_MODULE_H

#include "ubse_context.h" // for context
#include "ubse_error.h"
#include "ubse_mmi_interface.h"
namespace ubse::mem::controller {
using namespace ubse::context;
using namespace ubse::adapter_plugins::mmi;

class UbseMemControllerModule : public UbseModule {
public:
    static constexpr const char *kModuleName = "UbseMemControllerModule";
    std::string Name() const override
    {
        return kModuleName;
    }
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;
};

/**
* @brief 查询中心节点账本信息
* @param nodeId [in] 节点Id，为空则查询所有节点
* @param memDebtInfoMap [out] 借用账本对象集合
* @return 成功返回0, 失败返回非0
*/
uint32_t UbseGetMemDebtInfo(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap);

} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_MODULE_H
