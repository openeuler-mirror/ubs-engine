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

#ifndef UBSE_SSU_CONTROLLER_MODULE_H
#define UBSE_SSU_CONTROLLER_MODULE_H

#include <memory>
#include "ubse_common_def.h"
#include "ubse_module.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::controller {

using namespace ubse::common::def;
using ubse::plugin::service::ssu::UbseSsuService;

class UbseSsuControllerModule : public ubse::module::UbseModule {
public:
    static constexpr const char *kModuleName = "UbseSsuControllerModule";
    std::string Name() const override { return kModuleName; }

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

private:
    std::shared_ptr<UbseSsuService> ssuService_;
};

} // namespace ubse::ssu::controller

#endif // UBSE_SSU_CONTROLLER_MODULE_H
