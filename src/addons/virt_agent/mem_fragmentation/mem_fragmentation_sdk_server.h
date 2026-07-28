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

#include <ubse_api_server_def.h>
#include <ubse_def.h>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "mem_fragmentation_msg.h"
#include "mempooling_def.h"
#include "vm_error.h"

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

// Shared context for AsyncMemBorrowExec two-phase refactoring: pass successTaskIds between borrow threads and watcher thread + synchronization
struct AsyncBorrowCtx {
    std::mutex mux;
    std::condition_variable cv;
    int pendingCount{0};                     // Remaining borrow thread count, protected by lock
    std::vector<std::string> successTaskIds; // taskIds that succeeded in phase-1
};

class VirtMemFragSdk {
public:
    static VmResult QueryRegister();
    static VmResult Register();

private:
    static VmResult ConvertToLegacyResult(const MemBorrowExecuteResult& src, mem_borrow_result_c& dest);
    static uint32_t GetNodeInfo(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t GetVmInfo(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t NodeAntiAffinity(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MemBorrowStrategy(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MemBorrowExecute(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MemTaskQuery(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MemMigrateStrategy(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MemMigrateExecute(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MemReturn(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t StartMemReturnSync(const std::string& taskId, const std::string& nodeId,
                                       const UbseRequestContext& context);
    static uint32_t StartMemReturnAsync(const std::string& taskId, const std::string& nodeId,
                                        const UbseRequestContext& context);
    static void ExecuteAsyncMemReturn(const std::string& taskId, const std::string& nodeId,
                                      const UbseRequestContext& context);
    static uint32_t MemRollback(const UbseIpcMessage& req, const UbseRequestContext& context);
    static bool ValidateRequest(const UbseIpcMessage& req);
    static uint32_t UpdateAntiAffinityConfig(
        const std::map<std::string, std::vector<std::string>>& nodeAntiAffinityMap);
    static std::pair<std::string, const uint8_t*> ParseKey(const uint8_t* buffer, size_t buffer_size,
                                                           const uint8_t*& ptr);
    static std::vector<std::string> ParseValues(const uint8_t* buffer, size_t buffer_size, const uint8_t*& ptr);
    static std::pair<std::string, std::vector<std::string>> ParseEntry(const uint8_t* buffer, size_t buffer_size,
                                                                       const uint8_t*& ptr);
    static uint32_t DeserializeNodeAntiDictionary(const uint8_t* buffer, size_t buffer_size,
                                                  std::map<std::string, std::vector<std::string>>& node_dict_map);
    static uint32_t PackBorrowStrategyRsp(const MemBorrowStrategyResult& borrowStrategyResult, UbseIpcMessage& buffer);
    static uint32_t PackBorrowExecuteRsp(const MemBorrowExecuteResult& memBorrowExecuteResult, UbseIpcMessage& buffer);
    static uint32_t StartMemBorrowSync(const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                                       const UbseRequestContext& context);
    static uint32_t StartMemBorrowAsync(const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                                        const UbseRequestContext& context);
    static void ExecuteAsyncMemBorrow(const std::string& taskId, const VMMemoryBorrowParam& vmParam,
                                      const UbseRequestContext& context);
    static uint32_t PackMigrateStrategyRsp(const MigrateStrategyResult& migrateStrategyResult, UbseIpcMessage& buffer);
    static VmResult SetSrcNodeHugePage(const MemBorrowExecuteResult& borrowExecuteResult);
    static VmResult SetSrcNodeHugePageGlobal();
    /** ==============big memory virtual machine============== */
    static VmResult NodeInfoListSerialize(const std::vector<mem_fragmentation::NodeInfo>& nodeInfoList,
                                          UbseIpcMessage& resp);
    static VmResult GetNumaInfoFromRemote(const std::string& nodeId,
                                          std::vector<mem_fragmentation::NodeInfo>& numaInfoList);
    static void GetRemoteNodeInfoSrcHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    static VmResult GetRemoteNodeInfoMasterReceiver(const UbseByteBuffer& req, UbseByteBuffer& rep);
    static void GetRemoteNodeInfoMasterHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
    static VmResult GetRemoteNodeInfoDestReceiver(const UbseByteBuffer& req, UbseByteBuffer& rep);
    static VmResult GetNodeInfoList(const UbseIpcMessage& req, const UbseRequestContext& context);
    static VmResult BorrowParamDeserialize(const UbseIpcMessage& req, mem_fragmentation::BorrowParam& borrowParam,
                                           bool& isAsync);
    static VmResult MemBorrowStrategyByRMRS(const mem_fragmentation::BorrowParam& borrowParam,
                                            std::vector<MemBorrowStrategyResult>& borrowResult);
    static VmResult SyncRunBorrowExec(const std::string& taskId, const MemBorrowStrategyResult& memBorrowStrategyRst,
                                      mem_borrow_result_c& memBorrowRstC);
    static VmResult ExecBorrowCore(const std::string& taskId, const MemBorrowStrategyResult& memBorrowStrategyRst,
                                   MemBorrowExecuteResult& memBorrowExecRst);
    // AsyncMemBorrowExec two-phase: borrow threads run phase-1 + result archiving, watcher thread runs phase-2 global hugepage after all borrows complete
    static void AsyncBorrowWorker(std::shared_ptr<AsyncBorrowCtx> ctx, const std::string& taskId,
                                  const MemBorrowStrategyResult& borrowStrategyRst);
    static void AsyncBorrowWatcher(std::shared_ptr<AsyncBorrowCtx> ctx);
    static void AsyncBorrowCountDown(std::shared_ptr<AsyncBorrowCtx> ctx);
    static VmResult SyncMemBorrowExec(const std::vector<MemBorrowStrategyResult>& borrowStrategyRsts,
                                      std::vector<mem_borrow_result_c>& memBorrowRstCs);
    static VmResult AsyncMemBorrowExec(const std::vector<MemBorrowStrategyResult>& borrowStrategyRsts,
                                       std::vector<mem_borrow_result_c>& memBorrowRstCs);
    static VmResult BorrowResultSerialize(const std::vector<mem_borrow_result_c>& memBorrowRstCs, UbseIpcMessage& resp);
    static VmResult MemBorrow(const UbseIpcMessage& req, const UbseRequestContext& context);
    static VmResult PageSwapEnableParamSerialize(const UbseIpcMessage& req, pid_t& pid,
                                                 std::vector<mem_fragmentation::PageSwapPair>& pageSwapPairs);
    static VmResult PageSwapEnableByRMRS(const pid_t& pid,
                                         const std::vector<mem_fragmentation::PageSwapPair>& pageSwapPairs);
    static VmResult PageSwapEnable(const UbseIpcMessage& req, const UbseRequestContext& context);
};
} // namespace vm

#endif // MEM_FRAGMENTATION_SDK_SERVER_H