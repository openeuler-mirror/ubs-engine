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

#ifndef MEM_STRATEGY_ACCOUNTOBJADAPTER_H
#define MEM_STRATEGY_ACCOUNTOBJADAPTER_H
#include <variant>
#include "src/controllers/include/ubse_mem_resource.h"
namespace ubse::mem::strategy {
using namespace ubse::resource::mem;
using namespace ubse::mem::obj;
class AccountObjAdapter {
public:
    static std::string GetExportNode(const UbseMemAlgoResult &algoResult);
    static std::string GetImportNode(const UbseMemAlgoResult &algoResult);

private:
    AccountObjAdapter() = default;
};

} // namespace ubse::mem::strategy

#endif
