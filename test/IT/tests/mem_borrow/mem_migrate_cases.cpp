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

#include "mem_borrow_cases.h"
#include "mem_borrow_internal.h"

#include <cstdlib>
#include <deque>
#include <fstream>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_wait_helper.h"

namespace ubse::it::tests::mem_borrow {
namespace {
constexpr uint64_t MIGRATE_SIZE = 256ULL * 1024 * 1024; // 两块，覆盖中间块及末块落账。

template <typename T>
using SdkResult = std::unique_ptr<T, decltype(&std::free)>;

struct MigrationLogOnFailure {
    ubse::it::infra::ItCluster& cluster;
    const char* name;
    ~MigrationLogOnFailure()
    {
        if (!testing::Test::HasFailure()) {
            return;
        }
        for (const auto* nodeId : {"1", "2", "3", "4"}) {
            std::ifstream file(cluster.GetNode(nodeId).GetLogFilePath());
            std::deque<std::string> lines;
            for (std::string line; std::getline(file, line);) {
                if (line.find(name) != std::string::npos || line.find("Migrate RPC") != std::string::npos) {
                    lines.push_back(line);
                    if (lines.size() > 60) {
                        lines.pop_front();
                    }
                }
            }
            for (const auto& line : lines) {
                IT_LOG_INFO << "migration node=" << nodeId << ": " << line;
            }
        }
    }
};

std::vector<uint64_t> ImportMemIds(const ubs_mem_shm_desc_t& desc, uint32_t nodeId)
{
    for (uint32_t i = 0; i < desc.import_desc_cnt; ++i) {
        const auto& imported = desc.import_desc[i];
        if (imported.import_node.slot_id == nodeId) {
            return {imported.memids, imported.memids + imported.memid_cnt};
        }
    }
    return {};
}

void CheckShm(ubse::it::infra::ItSdkClient& sdk, const char* name, uint32_t exportNodeId, uint32_t importNodeId,
              const std::vector<uint64_t>& expectedIds, ubs_mem_stage expectedStage = UBSE_EXIST)
{
    ubs_mem_shm_desc_t* raw = nullptr;
    const auto ret = sdk.MemShmGet(name, &raw);
    SdkResult<ubs_mem_shm_desc_t> desc(raw, &std::free);
    ASSERT_IT_OK(ret);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->mem_size, MIGRATE_SIZE);
    EXPECT_EQ(desc->export_node.slot_id, exportNodeId);
    EXPECT_EQ(desc->mem_stage, expectedStage);
    EXPECT_EQ(ImportMemIds(*desc, importNodeId), expectedIds);
}
} // namespace

void RunShmMigrateLedger(ubse::it::infra::ItCluster& cluster, bool targetAttached, bool failBegin)
{
    using namespace ubse::it::infra;
    auto& sdk = cluster.GetSdkClient("3");
    auto& targetSdk = cluster.GetSdkClient("2");
    const char* name = failBegin ? "it_migrate_b_fail" : (targetAttached ? "it_migrate_b" : "it_migrate_a");
    MigrationLogOnFailure diagnostics{cluster, name};
    ubs_mem_nodes_t region{};
    region.node_cnt = 4;
    for (uint32_t i = 0; i < 4; ++i) {
        region.slot_ids[i] = i + 1;
    }
    ubs_mem_lender_t lender{};
    lender.slot_id = 1;
    lender.lender_size = MIGRATE_SIZE;
    lender.socket_id = UINT32_MAX;
    lender.numa_id = UINT32_MAX;
    lender.port_id = UINT32_MAX;
    uint8_t userInfo[UBS_MEM_MAX_USR_INFO_LEN]{};
    ASSERT_IT_OK(sdk.MemShmCreateWithLender(name, userInfo, 0, &region, &lender));
    ASSERT_IT_OK(WaitForShmReady(sdk, name));

    ubs_mem_shm_desc_t* raw = nullptr;
    auto ret = sdk.MemShmAttach(name, nullptr, 0, &raw);
    SdkResult<ubs_mem_shm_desc_t> initial(raw, &std::free);
    ASSERT_IT_OK(ret);
    ASSERT_NE(initial, nullptr);
    ASSERT_EQ(initial->export_node.slot_id, 1u);
    const auto originalIds = ImportMemIds(*initial, 3);
    ASSERT_EQ(originalIds.size(), 2u);
    std::vector<uint64_t> targetIds;
    if (targetAttached) {
        raw = nullptr;
        ret = targetSdk.MemShmAttach(name, nullptr, 0, &raw);
        SdkResult<ubs_mem_shm_desc_t> attached(raw, &std::free);
        ASSERT_IT_OK(ret);
        ASSERT_NE(attached, nullptr);
        targetIds = ImportMemIds(*attached, 2);
        ASSERT_EQ(targetIds.size(), 2u);
    }

    // 故障仅注入外部 obmm，真实 SDK/RPC、迁移编排、回调和账本逻辑保持执行。
    auto& targetNode = cluster.GetNode("2");
    if (failBegin) {
        targetNode.SetOpFailed(ObmmStubControl::OP_SWITCH_BEGIN_EXPORT, true);
    }
    ret = sdk.MemShmMigrate(name, 2);
    ubs_mem_shm_migrate_desc_t terminal{};
    bool gotTerminal = false;
    auto waitRet = ItWaitHelper::WaitForCondition(
        [&]() {
            if (ret != UBS_SUCCESS) {
                return true;
            }
            ubs_mem_shm_migrate_desc_t* progressRaw = nullptr;
            const auto queryRet = sdk.MemShmMigrateGet(name, &progressRaw);
            SdkResult<ubs_mem_shm_migrate_desc_t> progress(progressRaw, &std::free);
            if (queryRet != UBS_SUCCESS || !progress) {
                return false;
            }
            terminal = *progress;
            gotTerminal = terminal.state == UBS_MEM_MIGRATE_STATE_SUCCESS ||
                          terminal.state == UBS_MEM_MIGRATE_STATE_FAILED ||
                          terminal.state == UBS_MEM_MIGRATE_STATE_CANCELED;
            return gotTerminal;
        },
        60000);
    targetNode.RestoreObmmFault();
    ASSERT_IT_OK(ret);
    ASSERT_IT_OK(waitRet);
    ASSERT_TRUE(gotTerminal);
    EXPECT_EQ(terminal.src_slot_id, 1u);
    EXPECT_EQ(terminal.dst_slot_id, 2u);
    EXPECT_EQ(terminal.total_memids, 2u);
    EXPECT_EQ(terminal.total_size, MIGRATE_SIZE);

    if (failBegin) {
        ASSERT_EQ(terminal.state, UBS_MEM_MIGRATE_STATE_FAILED);
        EXPECT_NE(terminal.error_code, 0);
        EXPECT_EQ(terminal.migrated_memids, 0u);
        CheckShm(sdk, name, 1, 3, originalIds, UBSE_ERR_ABNORMAL);
        CheckShm(targetSdk, name, 1, 2, targetIds, UBSE_ERR_ABNORMAL);
    } else {
        ASSERT_EQ(terminal.state, UBS_MEM_MIGRATE_STATE_SUCCESS) << terminal.error_msg;
        EXPECT_EQ(terminal.error_code, 0);
        EXPECT_EQ(terminal.migrated_memids, 2u);
        // 按名称查询必须仍能找到目标导出；既有导入 memId 不变。
        CheckShm(sdk, name, 2, 3, originalIds);
        if (targetAttached) {
            CheckShm(targetSdk, name, 2, 2, targetIds);
        }
        // 新节点 CLI attach：精确覆盖现场 size:0MB/export-node:0 的回显路径。
        ItMemCreateInfo cliInfo{};
        ASSERT_IT_OK(cluster.GetCliInvoker("4").AttachMemory(cliInfo, name));
        EXPECT_EQ(cliInfo.size, "256MB");
        EXPECT_EQ(cliInfo.exportNode, "2");
        EXPECT_EQ(cliInfo.importNode, "4");
        EXPECT_FALSE(cliInfo.memIds.empty());
        EXPECT_NE(cliInfo.memIds, "-");
        EXPECT_IT_OK(cluster.GetSdkClient("4").MemShmDetach(name));
        // 原导入节点解除后再次 attach，检查 SDK 返回同样完整。
        ASSERT_IT_OK(sdk.MemShmDetach(name));
        raw = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &raw);
        SdkResult<ubs_mem_shm_desc_t> reattached(raw, &std::free);
        ASSERT_IT_OK(ret);
        ASSERT_NE(reattached, nullptr);
        EXPECT_EQ(reattached->mem_size, MIGRATE_SIZE);
        EXPECT_EQ(reattached->export_node.slot_id, 2u);
        EXPECT_EQ(ImportMemIds(*reattached, 3).size(), 2u);
    }
    EXPECT_IT_OK(sdk.MemShmDetach(name));
    if (targetAttached) {
        EXPECT_IT_OK(targetSdk.MemShmDetach(name));
    }
    EXPECT_IT_OK(sdk.MemShmDelete(name));
}
} // namespace ubse::it::tests::mem_borrow
