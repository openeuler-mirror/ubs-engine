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
#ifndef UBSE_CLI_MEM_CREATE_H
#define UBSE_CLI_MEM_CREATE_H

#include <memory>
#include "ubse_cli_res_builder.h"

namespace ubse::cli::reg {
class UbseCliMemCreate {
public:
    UbseCliMemCreate();
    ~UbseCliMemCreate() noexcept;

    // Create NUMA-type memory allocation info at this node.
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateNumaMem(const std::string& name, size_t size,
                                                                       const std::string& linkInfo);
    // Create FD-type memory allocation info at this node.
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateFdMem(const std::string& name, size_t size);
    // Create共享内存
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliCreateShareMem(const std::string& name, size_t size,
                                                                        const std::vector<uint32_t>& region);

private:
    class UbseCliMemCreateImpl;
    std::unique_ptr<UbseCliMemCreateImpl> pImpl_;
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_MEM_CREATE_H
