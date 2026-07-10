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

#include "mem_borrow_cases.h"

#include <gtest/gtest.h>

#include "ubse_common_def.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_string_util.h"
#include "it_wait_helper.h"
#include "ubs_engine_mem.h"

namespace ubse::it::tests::mem_borrow {

namespace {
constexpr const char* lenderNodeId = "1";   // 借出方节点
constexpr const char* borrowerNodeId = "2"; // 借用方节点
constexpr uint32_t lenderSlotId = 1;        // 借出方槽位
constexpr uint32_t borrowerSlotId = 2;      // 借用方槽位
} // namespace

// 两节点NUMA正常借用生命周期测试
void RunNumaNormalBorrowTest(ubse::it::infra::ItCluster& cluster)
{
    auto& borrowerClient = cluster.GetSdkClient(borrowerNodeId);
    IT_LOG_INFO << "Borrower SDK client initialized on node " << borrowerNodeId;

    // 第一步：创建NUMA借用
    constexpr uint64_t borrowSize = UBS_MEM_MIN_SIZE;
    const char* borrowName = "it_numa_normal_borrow";
    ubs_mem_numa_desc_t numaDesc{};
    IT_LOG_INFO << "Creating NUMA borrow: name=" << borrowName << ", size=" << borrowSize
                << ", distance=MEM_DISTANCE_L0";
    int32_t sdkRet = borrowerClient.MemNumaCreate(borrowName, borrowSize, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(sdkRet);
    IT_LOG_INFO << "NUMA borrow created, initial stage=" << numaDesc.mem_stage;

    // 第二步：轮询等待借用达到UBSE_EXIST就绪状态
    IT_LOG_INFO << "Waiting for borrow to reach UBSE_EXIST state...";
    auto waitRet = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() {
            ubs_mem_numa_desc_t desc{};
            int32_t getRet = borrowerClient.MemNumaGet(borrowName, &desc);
            if (getRet != UBS_SUCCESS) {
                IT_LOG_WARN << "MemNumaGet returned: " << getRet;
                return false;
            }
            IT_LOG_INFO << "Borrow stage: " << desc.mem_stage;
            return desc.mem_stage == UBSE_EXIST;
        },
        15000, 200);
    EXPECT_IT_OK(waitRet);

    // 第三步：验证借用属性（状态、大小、NUMA ID、导入导出槽位）
    ubs_mem_numa_desc_t verifyDesc{};
    sdkRet = borrowerClient.MemNumaGet(borrowName, &verifyDesc);
    EXPECT_IT_OK(sdkRet);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);
    EXPECT_EQ(verifyDesc.size, borrowSize);
    EXPECT_GE(verifyDesc.numaid, 0);
    EXPECT_EQ(verifyDesc.import_node.slot_id, borrowerSlotId);
    EXPECT_EQ(verifyDesc.export_node.slot_id, lenderSlotId);
    IT_LOG_INFO << "Borrow verified: stage=" << verifyDesc.mem_stage << ", size=" << verifyDesc.size
                << ", numaid=" << verifyDesc.numaid << ", importSlot=" << verifyDesc.import_node.slot_id
                << ", exportSlot=" << verifyDesc.export_node.slot_id;

    // 第四步：删除借用，释放资源
    IT_LOG_INFO << "Deleting NUMA borrow: " << borrowName;
    sdkRet = borrowerClient.MemNumaDelete(borrowName);
    EXPECT_IT_OK(sdkRet);
}

// CLI查询节点内存状态测试
void RunCliQueryNodesMemoryStatus001(ubse::it::infra::ItCluster& cluster)
{
    // 获取节点1的CLI调用器
    auto& cliInvoker = cluster.GetCliInvoker("1");
    IT_LOG_INFO << "Executing CLI command: check memory";

    // 执行check memory命令
    std::string output = cliInvoker.ExecCli("check memory");
    IT_LOG_INFO << "CLI output: " << output;

    // 验证输出包含表格结构（node, status, detail列）
    EXPECT_NE(output.find("node"), std::string::npos);
    EXPECT_NE(output.find("status"), std::string::npos);
    EXPECT_NE(output.find("detail"), std::string::npos);

    // 验证输出包含两个节点的信息（节点1和节点2）
    // 根据集群配置，节点ID通常为"1"和"2"
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);

    // 验证输出不包含错误信息
    EXPECT_EQ(output.find("ERROR"), std::string::npos);
    EXPECT_EQ(output.find("Failed"), std::string::npos);

    IT_LOG_INFO << "CLI check memory test passed";
}

// CLI内存操作测试（短选项）
void RunCliMemoryOperationsShortOpt001(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    using ubse::it::infra::util::ExtractNodeId;

    // 创建NUMA内存（使用短选项）
    ubse::it::infra::ItMemCreateInfo createInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryNuma(createInfo, "it_test_short_opt", "128M"));
    EXPECT_EQ(createInfo.name, "it_test_short_opt");
    EXPECT_EQ(createInfo.size, "128MB");

    // 查询内存借用详情（验证包含刚创建的记录）
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetails));
    EXPECT_GT(borrowDetails.size(), 0);
    bool found = false;
    for (const auto& detail : borrowDetails) {
        if (detail.name == "it_test_short_opt") {
            found = true;
            EXPECT_EQ(detail.type, "numa");
            EXPECT_EQ(ExtractNodeId(detail.borrowNode), createInfo.importNode);
            EXPECT_EQ(ExtractNodeId(detail.lendNode), createInfo.exportNode);
            EXPECT_NE(detail.lendSize.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 查询节点借用内存（使用短选项）
    std::vector<ubse::it::infra::ItNodeBorrowInfo> nodeBorrows;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryNodeBorrow(nodeBorrows));
    EXPECT_GT(nodeBorrows.size(), 0);
    found = false;
    for (const auto& info : nodeBorrows) {
        if (ExtractNodeId(info.borrowNode) == createInfo.importNode) {
            found = true;
            EXPECT_EQ(ExtractNodeId(info.lendNode), createInfo.exportNode);
            EXPECT_NE(info.size.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 查询节点借出内存（使用短选项）
    std::vector<ubse::it::infra::ItNodeLendInfo> nodeLends;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryNodeLend(nodeLends));
    EXPECT_GT(nodeLends.size(), 0);
    found = false;
    for (const auto& info : nodeLends) {
        if (ExtractNodeId(info.lendNode) == createInfo.exportNode) {
            found = true;
            EXPECT_EQ(ExtractNodeId(info.borrowNode), createInfo.importNode);
            EXPECT_NE(info.size.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 删除NUMA内存（使用短选项）
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_test_short_opt", "numa"));

    // 删除后再次查询，验证账本为空
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetailsAfterDelete;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetailsAfterDelete));
    EXPECT_EQ(borrowDetailsAfterDelete.size(), 0);
}

// CLI内存操作测试（长选项）
void RunCliMemoryOperationsLongOpt001(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    using ubse::it::infra::util::ExtractNodeId;

    // 创建NUMA内存（使用长选项）
    ubse::it::infra::ItMemCreateInfo createInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryNuma(createInfo, "it_test_long_opt", "128M", "", true));
    EXPECT_EQ(createInfo.name, "it_test_long_opt");
    EXPECT_EQ(createInfo.size, "128MB");

    // 查询内存借用详情（验证包含刚创建的记录）
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetails, "", "", true));
    EXPECT_GT(borrowDetails.size(), 0);
    bool found = false;
    for (const auto& detail : borrowDetails) {
        if (detail.name == "it_test_long_opt") {
            found = true;
            EXPECT_EQ(detail.type, "numa");
            EXPECT_EQ(ExtractNodeId(detail.borrowNode), createInfo.importNode);
            EXPECT_EQ(ExtractNodeId(detail.lendNode), createInfo.exportNode);
            EXPECT_NE(detail.lendSize.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 查询节点借用内存（使用长选项）
    std::vector<ubse::it::infra::ItNodeBorrowInfo> nodeBorrows;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryNodeBorrow(nodeBorrows, true));
    EXPECT_GT(nodeBorrows.size(), 0);
    found = false;
    for (const auto& info : nodeBorrows) {
        if (ExtractNodeId(info.borrowNode) == createInfo.importNode) {
            found = true;
            EXPECT_EQ(ExtractNodeId(info.lendNode), createInfo.exportNode);
            EXPECT_NE(info.size.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 查询节点借出内存（使用长选项）
    std::vector<ubse::it::infra::ItNodeLendInfo> nodeLends;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryNodeLend(nodeLends, true));
    EXPECT_GT(nodeLends.size(), 0);
    found = false;
    for (const auto& info : nodeLends) {
        if (ExtractNodeId(info.lendNode) == createInfo.exportNode) {
            found = true;
            EXPECT_EQ(ExtractNodeId(info.borrowNode), createInfo.importNode);
            EXPECT_NE(info.size.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found);

    // 删除NUMA内存（使用长选项）
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_test_long_opt", "numa", true));

    // 删除后再次查询，验证账本为空
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetailsAfterDelete;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetailsAfterDelete, "", "", true));
    EXPECT_EQ(borrowDetailsAfterDelete.size(), 0);
}

} // namespace ubse::it::tests::mem_borrow
