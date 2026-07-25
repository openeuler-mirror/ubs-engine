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

/**
 * @brief Memory creation result parsed from "create memory" CLI output.
 *
 * NUMA output format:
 *   name:<name>
 *   size:<size>MB
 *   numa-id:<numaId>
 *   import-node:<importNode>
 *   export-node:<exportNode>
 *
 * FD output format:
 *   name:<name>
 *   size:<size>MB
 *   mem-ids:<id1,id2,...>
 *   import-node:<importNode>
 *   export-node:<exportNode>
 *
 * SHM create output format:
 *   name:<name>
 *   size:<size>MB
 *   export-node:<exportNode>
 *   region:<r1,r2,...>
 *
 * SHM attach output format:
 *   name:<name>
 *   size:<size>MB
 *   mem-ids:<id1,id2,...>
 *   import-node:<importNode>
 *   export-node:<exportNode>
 *   region:<r1,r2,...>
 */
struct ItMemCreateInfo {
    std::string name;
    std::string size;
    std::string numaId;     // NUMA only
    std::string importNode; // NUMA, FD, SHM attach
    std::string exportNode; // NUMA, FD, SHM create/attach
    std::string memIds;     // FD, SHM attach (comma-separated)
    std::string region;     // SHM create/attach (comma-separated)
};

/**
 * @brief Node borrow information parsed from "display memory -t node_borrow".
 *
 * Table columns: borrow_node, lend_node, size
 */
struct ItNodeBorrowInfo {
    std::string borrowNode;
    std::string lendNode;
    std::string size;
};

/**
 * @brief Node lend information parsed from "display memory -t node_lend".
 *
 * Table columns: lend_node, borrow_node, size
 */
struct ItNodeLendInfo {
    std::string lendNode;
    std::string borrowNode;
    std::string size;
};

/**
 * @brief Memory configuration information parsed from "display memory -t config".
 *
 * Table columns: node, isLender
 */
struct ItMemConfigInfo {
    std::string node;
    std::string isLender;
};

} // namespace ubse::it::infra

#endif // IT_NODE_INFO_H
