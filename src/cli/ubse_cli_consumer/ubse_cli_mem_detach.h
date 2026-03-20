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

#ifndef RACK_MANAGER_UBSE_CLI_MEM_DETACH_H
#define RACK_MANAGER_UBSE_CLI_MEM_DETACH_H

#include <memory>

#include "ubse_cli_res_builder.h"

namespace ubse::cli::reg {
class UbseCliMemDetach {
public:
    UbseCliMemDetach();
    ~UbseCliMemDetach() noexcept;

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliDetachMem(const std::string &name);

private:
    class UbseCliMemDetachImpl;
    std::unique_ptr<UbseCliMemDetachImpl> pImpl_;
};
} // namespace ubse::cli::reg
#endif // RACK_MANAGER_UBSE_CLI_MEM_DETACH_H
