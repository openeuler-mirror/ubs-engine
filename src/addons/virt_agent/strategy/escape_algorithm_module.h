/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef VM_ESCAPE_ALGORITHM_MODULE_H
#define VM_ESCAPE_ALGORITHM_MODULE_H

#include <cmath>
#include "vm_error.h"
#include "vm_struct.h"

namespace vm {
struct StrategyConfig {
    std::string borrowWatermark{};
    std::string lowWatermark{};
    std::string highWatermark{};

    float_t maxMemBorrow{};
    uint64_t maxMemPerBorrowBytes{};
    uint64_t minMemPerBorrowBytes{};
    uint64_t maxPerTotalMemBorrowBytes{};
    uint64_t oomEventBorrowBytes{};
};

using EscapeAlgorithmInitFunc = int (*)(const StrategyConfig &, void(int, const char *));
using EscapeAlgorithmFunc = int (*)(const StrategyConfig &, AlarmNumaInfo &, GlobalNumaInfoMap &, EscapeAction &);

class EscapeAlgorithmModule {
public:
    static VmResult Init();

    static EscapeAlgorithmInitFunc GetEscapeAlgorithmInit();

    static EscapeAlgorithmFunc GetStrategyAlgorithm();

    static void CloseStrategyHandle();

    static void VmEscapeLog(int level, const char *str);

private:
    static void *algorithmHandle;
    static EscapeAlgorithmInitFunc escapeAlgorithmInitFunc;
    static EscapeAlgorithmFunc escapeAlgorithmFunc;
};
} // namespace vm
#endif // VM_ESCAPE_ALGORITHM_MODULE_H