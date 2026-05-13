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

#ifndef UBSE_CLI_MEM_ATTACH_H
#define UBSE_CLI_MEM_ATTACH_H

#include <memory>

#include "ubse_cli_res_builder.h"

namespace ubse::cli::reg {
class UbseCliMemAttach {
public:
    UbseCliMemAttach();
    ~UbseCliMemAttach() noexcept;

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliAttachMem(const std::string& name);

private:
    class UbseCliMemAttachImpl;
    std::unique_ptr<UbseCliMemAttachImpl> pImpl_;
};
} // namespace ubse::cli::reg

#endif //  UBSE_CLI_MEM_ATTACH_H
