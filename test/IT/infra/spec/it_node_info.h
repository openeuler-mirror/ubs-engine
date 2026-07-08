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

#ifndef IT_NODE_INFO_H
#define IT_NODE_INFO_H

#include <string>

namespace ubse::it::infra {

/**
 * @brief Node information parsed from CLI display output.
 *
 * This is a plain data structure used by ItCliInvoker for query results
 * and by ItSdkClient for election convenience methods.
 */
struct ItNodeInfo {
    std::string node;
    std::string role;
    std::string bondingEid;
    std::string guid;
};

} // namespace ubse::it::infra

#endif // IT_NODE_INFO_H
