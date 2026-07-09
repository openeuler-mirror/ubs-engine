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

/**
 * @brief CPU topology link information parsed from "display topo -t cpu".
 */
struct ItTopoCpuLink {
    std::string linkId;
    std::string node;
    std::string socket;
    std::string port;
    std::string peerNode;
    std::string peerSocket;
    std::string peerPort;
    std::string status;
};

/**
 * @brief Memory borrow detail information parsed from "display memory -t borrow_detail".
 */
struct ItMemBorrowDetail {
    std::string name;
    std::string type;
    std::string borrowNode;
    std::string lendNode;
    std::string lendNuma;
    std::string lendSize;
    std::string status;
    std::string handle;
};

} // namespace ubse::it::infra

#endif // IT_NODE_INFO_H
