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

#ifndef UBSE_MODULE_H
#define UBSE_MODULE_H

#include <memory>

#include "ubse_common_def.h"

namespace ubse::module {
using ubse::common::def::UbseResult;

class UbseModule {
public:
    virtual ~UbseModule() = default;
    virtual UbseResult Initialize() = 0;

    virtual void UnInitialize() = 0;

    virtual UbseResult Start() = 0;

    virtual void Stop() = 0;

    virtual void RegArgs(){};

    template <typename T>
    static std::shared_ptr<UbseModule> CreateModule()
    {
        return std::make_shared<T>();
    }
};
} // namespace ubse::module
#endif // UBSE_MODULE_H
