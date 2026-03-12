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

#ifndef MEM_FRAGMENTATION_SDK_SERVER_H
#define MEM_FRAGMENTATION_SDK_SERVER_H

#include <string>
#include <map>
#include <vector>
#include <vm_error.h>
#include <ubse_api_server_def.h>

#include "mempooling_def.h"
#include "mem_fragmentation_msg.h"

namespace vm {
using namespace api::server;
using namespace vm::mempooling;

struct VMMemoryBorrowParam {
    SrcMemoryBorrowParam srcParam{};
    std::vector<DestMemoryBorrowParam> destParam{};
};

struct VMMigrateParam {
    std::string borrowInNode{};
    std::vector<VMPresetParam> vmInfoList{};
    std::uint64_t borrowSize{};
};

class VirtMemFragSdk {
public:
    static VmResult QueryRegister();
    static VmResult Register();

private:
    static uint32_t GetNodeInfo(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t GetVmInfo(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t NodeAntiAffinity(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t MemBorrowStrategy(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t MemBorrowExecute(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t MemTaskQuery(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t MemMigrateStrategy(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t MemMigrateExecute(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t MemReturn(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t StartMemReturnSync(const std::string &taskId, const std::string &nodeId,
                                       const UbseRequestContext &context);
    static uint32_t StartMemReturnAsync(const std::string &taskId, const std::string &nodeId,
                                        const UbseRequestContext &context);
    static void ExecuteAsyncMemReturn(const std::string &taskId, const std::string &nodeId,
                                      const UbseRequestContext &context);
    static uint32_t MemRollback(const UbseIpcMessage &req, const UbseRequestContext &context);

    static bool ValidateRequest(const UbseIpcMessage &req);
    static uint32_t UpdateAntiAffinityConfig(const std::map<std::string,
                                             std::vector<std::string>> &nodeAntiAffinityMap);
    static std::pair<std::string, const uint8_t*> ParseKey(const uint8_t* buffer, size_t buffer_size,
                                                           const uint8_t*& ptr);
    static std::vector<std::string> ParseValues(const uint8_t* buffer, size_t buffer_size, const uint8_t*& ptr);
    static std::pair<std::string, std::vector<std::string>> ParseEntry(const uint8_t* buffer, size_t buffer_size,
                                                                       const uint8_t*& ptr);
    static uint32_t DeserializeNodeAntiDictionary(const uint8_t* buffer, size_t buffer_size,
                                                  std::map<std::string, std::vector<std::string>>& node_dict_map);
    static uint32_t PackBorrowStrategyRsp(const MemBorrowStrategyResult &borrowStrategyResult, UbseIpcMessage &buffer);
    static uint32_t PackBorrowExecuteRsp(const MemBorrowExecuteResult &memBorrowExecuteResult, UbseIpcMessage &buffer);
    static uint32_t StartMemBorrowSync(const std::string &taskId, const VMMemoryBorrowParam &vmParam,
                                         const UbseRequestContext &context);
    static uint32_t StartMemBorrowAsync(const std::string &taskId, const VMMemoryBorrowParam &vmParam,
                                          const UbseRequestContext &context);
    static void ExecuteAsyncMemBorrow(const std::string &taskId, const VMMemoryBorrowParam &vmParam,
                                      const UbseRequestContext &context);
    static uint32_t PackMigrateStrategyRsp(const MigrateStrategyResult &migrateStrategyResult, UbseIpcMessage &buffer);
    static VmResult SetSrcNodeHugePage(const MemBorrowExecuteResult &borrowExecuteResult);
};
} // namespace vm

#endif // MEM_FRAGMENTATION_SDK_SERVER_H
