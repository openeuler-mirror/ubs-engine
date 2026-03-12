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
#ifndef UBSE_CLI_MEM_QUERY_H
#define UBSE_CLI_MEM_QUERY_H

#include <memory>
#include "ubse_cli_mem_struct.h"
#include "ubse_cli_res_builder.h"

namespace ubse::cli::reg {
class UbseCliMemQuery {
public:
    UbseCliMemQuery();
    ~UbseCliMemQuery() noexcept;
    enum class WaitType {
        QUERY_ONLY,
        WAIT_CREATING,
    };
    // Query the NUMA memory allocation info for the unique identifier "name" of this node.
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliGetNumaMemByName(const std::string &name,
        WaitType waitType = WaitType::QUERY_ONLY);
    // Query the FD memory allocation info for the unique identifier "name" of this node.
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliGetFdMemByName(const std::string &name,
        WaitType waitType = WaitType::QUERY_ONLY);

    // Query the SHM memory allocation info for the unique identifier "name" of this node.
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliGetShmMemByName(const std::string &name,
                                                                         UbseCliShmOperation operation);

private:
    class UbseCliMemQueryImpl;
    std::unique_ptr<UbseCliMemQueryImpl> pImpl_;
};

class UbseCliMemDisplayBorrowDetail {
public:
    UbseCliMemDisplayBorrowDetail();
    ~UbseCliMemDisplayBorrowDetail() noexcept;

    struct Filter {
        std::string name; // Memory Unique Identifier
        std::string type; // Memory Type: numa/shm/fd/addr...
    };
    // Provide a full ledger query function with filtering capabilities.
    // When results are numerous, please use scrolling commands like less.
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliQueryBorrowDetail(const Filter &filter);

private:
    class UbseCliMemDisplayBorrowDetailImpl;
    std::unique_ptr<UbseCliMemDisplayBorrowDetailImpl> pImpl_;
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_MEM_QUERY_H
