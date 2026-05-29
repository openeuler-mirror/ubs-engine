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

#ifndef ESCAPE_ALGORITHM_HELPER_H
#define ESCAPE_ALGORITHM_HELPER_H

#include "escape_algorithm_module.h"

namespace vm {

class EscapeAlgorithmHelper {
public:
    static EscapeAlgorithmHelper& GetInstance();

    static VmResult Init();

    static void EscapeAlgorithmHandleClose();

    static void GetStrategyConf(StrategyConfig& strategyConf);

private:
    EscapeAlgorithmHelper() = default;
    ~EscapeAlgorithmHelper() = default;
};
} // namespace vm
#endif // ESCAPE_ALGORITHM_HELPER_H