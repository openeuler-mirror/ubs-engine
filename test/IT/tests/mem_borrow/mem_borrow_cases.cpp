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

// CLI内存类型过滤查询测试：NUMA/FD/SHM全生命周期
void RunCliMemoryTypeFilterOperations001(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");

    // ===== NUMA: create → display by type → display by name → delete =====
    ubse::it::infra::ItMemCreateInfo numaCreateInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryNuma(numaCreateInfo, "it_test_numa", "128M"));
    EXPECT_EQ(numaCreateInfo.name, "it_test_numa");
    EXPECT_EQ(numaCreateInfo.size, "128MB");
    EXPECT_FALSE(numaCreateInfo.numaId.empty());
    EXPECT_FALSE(numaCreateInfo.importNode.empty());
    EXPECT_FALSE(numaCreateInfo.exportNode.empty());

    // ===== FD: create → display by type → delete =====
    ubse::it::infra::ItMemCreateInfo fdCreateInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryFd(fdCreateInfo, "it_test_fd", "128M"));
    EXPECT_EQ(fdCreateInfo.name, "it_test_fd");
    EXPECT_EQ(fdCreateInfo.size, "128MB");
    EXPECT_FALSE(fdCreateInfo.memIds.empty());
    EXPECT_FALSE(fdCreateInfo.importNode.empty());
    EXPECT_FALSE(fdCreateInfo.exportNode.empty());

    // ===== SHM: create → attach → display by type → detach → delete =====
    // Phase 1: create share (在借出方创建，输出含 export-node + region)
    ubse::it::infra::ItMemCreateInfo shareCreateInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryShare(shareCreateInfo, "it_test_share", "128M", "1,2"));
    EXPECT_EQ(shareCreateInfo.name, "it_test_share");
    EXPECT_EQ(shareCreateInfo.size, "128MB");
    EXPECT_FALSE(shareCreateInfo.exportNode.empty());
    EXPECT_FALSE(shareCreateInfo.region.empty());

    // Phase 2: attach share (在借用方挂载，输出含 mem-ids + import-node + export-node + region)
    ubse::it::infra::ItMemCreateInfo attachInfo;
    EXPECT_IT_OK(cliInvoker.AttachMemory(attachInfo, "it_test_share"));
    EXPECT_EQ(attachInfo.name, "it_test_share");
    EXPECT_EQ(attachInfo.size, "128MB");
    EXPECT_FALSE(attachInfo.memIds.empty());
    EXPECT_FALSE(attachInfo.importNode.empty());
    EXPECT_FALSE(attachInfo.exportNode.empty());
    EXPECT_FALSE(attachInfo.region.empty());

    // ===== Display by type filter =====
    // NUMA type
    std::vector<ubse::it::infra::ItMemBorrowDetail> numaBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(numaBorrowDetails, "numa"));
    EXPECT_GT(numaBorrowDetails.size(), 0);
    bool foundNuma = false;
    for (const auto& detail : numaBorrowDetails) {
        if (detail.name == "it_test_numa") {
            foundNuma = true;
            EXPECT_EQ(detail.type, "numa");
            EXPECT_NE(detail.lendSize.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(foundNuma);

    // FD type
    std::vector<ubse::it::infra::ItMemBorrowDetail> fdBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(fdBorrowDetails, "fd"));
    EXPECT_GT(fdBorrowDetails.size(), 0);
    bool foundFd = false;
    for (const auto& detail : fdBorrowDetails) {
        if (detail.name == "it_test_fd") {
            foundFd = true;
            EXPECT_EQ(detail.type, "fd");
            EXPECT_NE(detail.lendSize.find("128"), std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(foundFd);

    // SHARE type
    std::vector<ubse::it::infra::ItMemBorrowDetail> shareBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(shareBorrowDetails, "share"));
    EXPECT_GT(shareBorrowDetails.size(), 0);
    bool foundShare = false;
    for (const auto& detail : shareBorrowDetails) {
        if (detail.name == "it_test_share") {
            foundShare = true;
            EXPECT_EQ(detail.type, "share");
            break;
        }
    }
    EXPECT_TRUE(foundShare);

    // ===== Display by name filter =====
    std::vector<ubse::it::infra::ItMemBorrowDetail> nameBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(nameBorrowDetails, "", "it_test_numa"));
    EXPECT_EQ(nameBorrowDetails.size(), 1);
    if (nameBorrowDetails.size() > 0) {
        EXPECT_EQ(nameBorrowDetails[0].name, "it_test_numa");
        EXPECT_EQ(nameBorrowDetails[0].type, "numa");
    }

    // Type + name filter
    std::vector<ubse::it::infra::ItMemBorrowDetail> typeAndNameBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(typeAndNameBorrowDetails, "fd", "it_test_fd"));
    EXPECT_EQ(typeAndNameBorrowDetails.size(), 1);
    if (typeAndNameBorrowDetails.size() > 0) {
        EXPECT_EQ(typeAndNameBorrowDetails[0].name, "it_test_fd");
        EXPECT_EQ(typeAndNameBorrowDetails[0].type, "fd");
    }

    // ===== Cleanup: detach → delete (SHM), delete (FD), delete (NUMA) =====
    EXPECT_IT_OK(cliInvoker.DetachMemory("it_test_share"));
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_test_share", "share"));
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_test_fd", "fd"));
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_test_numa", "numa"));

    // 验证账本为空
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetailsAfterDelete;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetailsAfterDelete));
    EXPECT_EQ(borrowDetailsAfterDelete.size(), 0);
}

// CLI NUMA状态查询测试
void RunCliNumaStatusQuery001(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");

    // 查询NUMA状态（基本查询，不显示所有大页）
    std::string numaStatusOutput = cliInvoker.DisplayMemoryNumaStatus(false);
    EXPECT_NE(numaStatusOutput.find("node"), std::string::npos);
    EXPECT_NE(numaStatusOutput.find("numa"), std::string::npos);
    EXPECT_NE(numaStatusOutput.find("total"), std::string::npos);
    EXPECT_NE(numaStatusOutput.find("used"), std::string::npos);
    EXPECT_NE(numaStatusOutput.find("free"), std::string::npos);
    EXPECT_NE(numaStatusOutput.find("used_percent"), std::string::npos);
    EXPECT_EQ(numaStatusOutput.find("ERROR"), std::string::npos);

    // 查询NUMA状态（显示所有大页）
    std::string numaStatusAllOutput = cliInvoker.DisplayMemoryNumaStatus(true);
    EXPECT_NE(numaStatusAllOutput.find("node"), std::string::npos);
    EXPECT_NE(numaStatusAllOutput.find("numa"), std::string::npos);
    EXPECT_NE(numaStatusAllOutput.find("total"), std::string::npos);
    EXPECT_NE(numaStatusAllOutput.find("used"), std::string::npos);
    EXPECT_NE(numaStatusAllOutput.find("free"), std::string::npos);
    EXPECT_NE(numaStatusAllOutput.find("used_percent"), std::string::npos);
    // 验证包含所有大页信息（2M和1G）
    EXPECT_NE(numaStatusAllOutput.find("2M"), std::string::npos);
    EXPECT_NE(numaStatusAllOutput.find("1G"), std::string::npos);
    EXPECT_EQ(numaStatusAllOutput.find("ERROR"), std::string::npos);
}

// CLI内存配置查询测试
void RunCliMemoryConfigQuery001(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");

    // 查询内存配置信息
    std::vector<ubse::it::infra::ItMemConfigInfo> configs;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryConfig(configs));
    EXPECT_GT(configs.size(), 0);

    // 验证所有节点的 isLender 都是 "true"
    for (const auto& config : configs) {
        EXPECT_EQ(config.isLender, "true");
        EXPECT_FALSE(config.node.empty());
    }
}

// 四节点SHM attach后import_desc_cnt验证：节点1创建 → 节点2/3/4分别attach(每个返回import_desc_cnt=1) → detach → delete
void RunShmFourNodesAttachImportDescCntTest(ubse::it::infra::ItCluster& cluster)
{
    constexpr const char* shmName = "it_shm_4node_attach";
    constexpr uint64_t shmSize = UBS_MEM_MIN_SIZE; // 128MB

    // Step 1: 节点1创建共享内存，region覆盖4个节点
    auto& node1Client = cluster.GetSdkClient("1");
    ubs_mem_nodes_t region{};
    region.node_cnt = 4;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    region.slot_ids[2] = 3;
    region.slot_ids[3] = 4;

    IT_LOG_INFO << "Creating SHM on node1: name=" << shmName << ", size=" << shmSize;
    ubs_mem_nodes_t provider{};
    provider.node_cnt = 1;
    provider.slot_ids[0] = 1;
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    int32_t ret = node1Client.MemShmCreate(shmName, shmSize, usrInfo, 0, &region, &provider);
    ASSERT_IT_OK(ret);

    // 等待SHM创建就绪
    auto waitRet = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() {
            ubs_mem_shm_desc_t* shmDesc = nullptr;
            int32_t getRet = node1Client.MemShmGet(shmName, &shmDesc);
            if (getRet != UBS_SUCCESS || shmDesc == nullptr) {
                return false;
            }
            bool ready = (shmDesc->mem_stage == UBSE_EXIST);
            free(shmDesc);
            return ready;
        },
        15000, 200);
    EXPECT_IT_OK(waitRet);

    // Step 2: 节点2/3/4分别attach共享内存
    std::vector<std::string> attachNodes = {"2", "3", "4"};
    for (const auto& nodeId : attachNodes) {
        auto& client = cluster.GetSdkClient(nodeId);
        ubs_mem_shm_desc_t* shmDesc = nullptr;

        IT_LOG_INFO << "Attaching SHM on node " << nodeId;
        ret = client.MemShmAttach(shmName, nullptr, 0, &shmDesc);
        ASSERT_IT_OK(ret);
        ASSERT_NE(shmDesc, nullptr);

        // 关键验证：每个节点attach返回的import_desc_cnt应为1（仅包含本节点的导入信息）
        IT_LOG_INFO << "Node " << nodeId << " attach result: import_desc_cnt=" << shmDesc->import_desc_cnt
                    << ", mem_stage=" << shmDesc->mem_stage;
        EXPECT_EQ(shmDesc->import_desc_cnt, 1u);
        if (shmDesc->import_desc_cnt > 0) {
            EXPECT_EQ(shmDesc->import_desc[0].mem_stage, UBSE_EXIST);
            EXPECT_GT(shmDesc->import_desc[0].memid_cnt, 0u);
        }
        EXPECT_STREQ(shmDesc->name, shmName);
        EXPECT_EQ(shmDesc->mem_size, shmSize);

        free(shmDesc);
    }

    // Step 3: 节点1通过MemShmGet验证共享内存状态，import_desc_cnt应为3（3个节点已attach）
    {
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        ret = node1Client.MemShmGet(shmName, &shmDesc);
        EXPECT_IT_OK(ret);
        if (shmDesc != nullptr) {
            IT_LOG_INFO << "Node1 Get after attach: import_desc_cnt=" << shmDesc->import_desc_cnt;
            EXPECT_EQ(shmDesc->import_desc_cnt, 3u);
            free(shmDesc);
        }
    }

    // Step 4: 节点2/3/4分别detach
    for (const auto& nodeId : attachNodes) {
        auto& client = cluster.GetSdkClient(nodeId);
        IT_LOG_INFO << "Detaching SHM on node " << nodeId;
        ret = client.MemShmDetach(shmName);
        EXPECT_IT_OK(ret);
    }

    // Step 5: 节点1删除共享内存
    IT_LOG_INFO << "Deleting SHM on node1: " << shmName;
    ret = node1Client.MemShmDelete(shmName);
    EXPECT_IT_OK(ret);
}

} // namespace ubse::it::tests::mem_borrow
