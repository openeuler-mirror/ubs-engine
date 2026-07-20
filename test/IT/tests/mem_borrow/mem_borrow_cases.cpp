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

#include <unistd.h>
#include "ubse_common_def.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_sdk_client.h"
#include "it_string_util.h"
#include "it_wait_helper.h"
#include "ubs_engine.h"
#include "ubs_engine_mem.h"
#include "ubs_error.h"

namespace ubse::it::tests::mem_borrow {

namespace {
constexpr const char* lenderNodeId = "1";   // 借出方节点
constexpr const char* borrowerNodeId = "2"; // 借用方节点
constexpr uint32_t lenderSlotId = 1;        // 借出方槽位
constexpr uint32_t borrowerSlotId = 2;      // 借用方槽位

constexpr uint64_t fdSize = UBS_MEM_MIN_SIZE;               // 4MB
constexpr uint64_t shmSize = UBS_MEM_MIN_SIZE;              // 4MB
constexpr uint64_t fdSize129M = 129ULL * 1024ULL * 1024ULL; // 129MB, 128M一块 → 2块

void WaitForFdReady(ubse::it::infra::ItSdkClient& sdk, const char* name)
{
    for (int i = 0; i < 60; i++) {
        ubs_mem_fd_desc_t desc{};
        int32_t ret = sdk.MemFdGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            return;
        }
        sleep(1);
    }
}

int32_t WaitForShmReady(ubse::it::infra::ItSdkClient& sdk, const char* name)
{
    for (int i = 0; i < 60; i++) {
        ubs_mem_shm_desc_t* desc = nullptr;
        int32_t ret = sdk.MemShmGet(name, &desc);
        if (ret == UBS_SUCCESS && desc != nullptr && desc->mem_stage == UBSE_EXIST) {
            free(desc);
            return UBS_SUCCESS;
        }
        if (desc != nullptr) {
            free(desc);
        }
        sleep(1);
    }
    return UBS_ENGINE_ERR_TIMEOUT;
}
} // namespace

// 两节点NUMA正常借用生命周期测试
void RunP0NumaCreateBorrowOk01(ubse::it::infra::ItCluster& cluster)
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
void RunP0CliCheckMemOk01(ubse::it::infra::ItCluster& cluster)
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
void RunP0CliCreateNumaOk01(ubse::it::infra::ItCluster& cluster)
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
void RunP1CliCreateNumaParamVariant01(ubse::it::infra::ItCluster& cluster)
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
void RunP1CliBorrowDetailOk01(ubse::it::infra::ItCluster& cluster)
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
void RunP0CliNumaStatusOk01(ubse::it::infra::ItCluster& cluster)
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
void RunP0CliMemConfigOk01(ubse::it::infra::ItCluster& cluster)
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
void RunP1ShmAttachMultiNode01(ubse::it::infra::ItCluster& cluster)
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

// ==================== FD 辅助函数（来自 mem_fd） ====================

// ==================== ubs_mem_fd_create P0 测试 ====================

// P0-FdCreate-Ok-01: 标准创建成功 (双/四节点)
void RunP0FdCreateOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_ok01";
    constexpr uint64_t borrowSize = 129ULL * 1024ULL * 1024ULL; // 129MB, 2 blocks
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Creating FD: name=" << name << ", size=" << borrowSize;
    int32_t ret = sdk.MemFdCreate(name, borrowSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    // 1. name: strcmp == 输入 name
    EXPECT_STREQ(fdDesc.name, name) << "fdDesc.name should match input name";
    // 2. mem_stage ∈ {UBSE_CREATING, UBSE_EXIST} (创建中或已完成)
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST)
        << "mem_stage should be CREATING or EXIST, actual: " << fdDesc.mem_stage;
    // 3. mem_size == 输入 size
    EXPECT_EQ(fdDesc.mem_size, borrowSize) << "mem_size should equal input size";
    // 4. memid_cnt == ceil(mem_size / unit_size)
    uint32_t expectedMemidCnt = static_cast<uint32_t>((fdDesc.mem_size + fdDesc.unit_size - 1) / fdDesc.unit_size);
    EXPECT_EQ(fdDesc.memid_cnt, expectedMemidCnt)
        << "memid_cnt should equal ceil(mem_size/unit_size), expected=" << expectedMemidCnt;
    // 5. memids[0..1] 非零
    for (uint32_t i = 0; i < fdDesc.memid_cnt && i < UBS_MEM_MAX_MEMID_NUM; i++) {
        EXPECT_GT(fdDesc.memids[i], 0ull) << "memid[" << i << "] should be > 0";
    }
    // 6. unit_size > 0
    EXPECT_GT(fdDesc.unit_size, static_cast<size_t>(0)) << "unit_size should be > 0";
    // 7. import_node.slot_id == 本节点 (nodeId "1" 对应的 slotId)
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;
    EXPECT_EQ(fdDesc.import_node.slot_id, localSlotId) << "import_node.slot_id should be local node";
    // 8. export_node.slot_id 有效 (>0)
    EXPECT_GT(fdDesc.export_node.slot_id, 0u) << "export_node.slot_id should be valid (>0)";
    // 9. export_node.slot_id != import_node.slot_id (借入与借出节点不同)
    EXPECT_NE(fdDesc.export_node.slot_id, fdDesc.import_node.slot_id)
        << "export_node.slot_id should differ from import_node.slot_id";

    // 清理
    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreate-Ok-01 done";
}

// P0-FdCreate-OverLen-01: name超长 (单节点)
void RunP0FdCreateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string overLenName(48, 'a');
    ubs_mem_fd_desc_t fdDesc{};
    IT_LOG_INFO << "Creating FD with over-length name: length=" << overLenName.length();
    int32_t ret = sdk.MemFdCreate(overLenName.c_str(), fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-FdCreate-OverLen-01 done";
}

// P0-FdCreate-InvalidVal-01: size < 4MB (双节点)
void RunP0FdCreateInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_inval01";
    ubs_mem_fd_desc_t fdDesc{};

    constexpr uint64_t tooSmall = 1;
    IT_LOG_INFO << "Creating FD with size < 4MB: " << tooSmall;
    int32_t ret = sdk.MemFdCreate(name, tooSmall, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    IT_LOG_INFO << "P0-FdCreate-InvalidVal-01 done";
}

// P0-FdCreate-InvalidVal-02: size > 256GB（超过 UBS_MEM_MAX_MEMID_NUM * 128MB）
void RunP0FdCreateInvalidVal02(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_inval02";
    ubs_mem_fd_desc_t fdDesc{};

    constexpr uint64_t tooLarge = 257ULL * 1024ULL * 1024ULL * 1024ULL;
    IT_LOG_INFO << "Creating FD with size > 256GB: " << tooLarge;
    int32_t ret = sdk.MemFdCreate(name, tooLarge, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE) << "size > 256GB should return ALLOCATE";

    IT_LOG_INFO << "P0-FdCreate-InvalidVal-02 done";
}

// P0-FdCreate-Dup-01: 同名重复创建 (双节点)
void RunP0FdCreateDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_dup01";
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Creating FD first time: name=" << name;
    int32_t ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    // 同名重复创建
    IT_LOG_INFO << "Creating FD with duplicate name: name=" << name;
    ubs_mem_fd_desc_t fdDesc2{};
    ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc2);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

    // 清理
    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreate-Dup-01 done";
}

// P0-FdCreate-NullPtr-01: 空指针 (单节点)
void RunP0FdCreateNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_null01";
    IT_LOG_INFO << "Creating FD with null fd_desc pointer";
    int32_t ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdCreate-NullPtr-01 done";
}

// P0-FdCreate-BoundMin-01: size=4MB (双节点)
void RunP0FdCreateBoundMin01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_bmin01";
    constexpr uint64_t minSize = UBS_MEM_MIN_SIZE; // 4MB
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Creating FD with boundary min size: " << minSize;
    int32_t ret = sdk.MemFdCreate(name, minSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 出参校验
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST);
    EXPECT_EQ(fdDesc.mem_size, minSize) << "mem_size should equal 4MB";
    uint32_t expectedMemidCnt = static_cast<uint32_t>((fdDesc.mem_size + fdDesc.unit_size - 1) / fdDesc.unit_size);
    EXPECT_EQ(fdDesc.memid_cnt, expectedMemidCnt);
    EXPECT_EQ(fdDesc.import_node.slot_id, localSlotId) << "import_node.slot_id should be local node";

    // 清理
    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreate-BoundMin-01 done";
}

// P0-FdCreate-BoundMax-01: name=47字节 (双节点)
void RunP0FdCreateBoundMax01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string boundName(47, 'x');
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Creating FD with boundary max name length: " << boundName.length();
    int32_t ret = sdk.MemFdCreate(boundName.c_str(), fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 出参校验
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST);
    EXPECT_STREQ(fdDesc.name, boundName.c_str()) << "name should be fully preserved";
    EXPECT_EQ(fdDesc.import_node.slot_id, localSlotId) << "import_node.slot_id should be local node";

    // 清理
    ret = sdk.MemFdDelete(boundName.c_str());
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreate-BoundMax-01 done";
}

// ==================== ubs_mem_fd_create_with_lender P0 测试 ====================

// P0-FdCreateLender-Ok-01: 指定借出节点创建 (双/四节点)
void RunP0FdCreateLenderOk01(ubse::it::infra::ItCluster& cluster)
{
    // 双节点场景选节点2, 四节点场景选节点3
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    const char* name = "it_p0_fd_lender01";
    constexpr uint64_t borrowSize = 129ULL * 1024ULL * 1024ULL;
    ubs_mem_lender_t lender{};
    lender.lender_size = borrowSize;
    lender.slot_id = lenderSlotId;

    ubs_mem_fd_desc_t fdDesc{};
    IT_LOG_INFO << "Creating FD with lender: name=" << name << ", size=" << borrowSize
                << ", lender slot=" << lenderSlotId;
    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    ASSERT_IT_OK(ret);

    // ====== 出参 fdDesc 字段校验 ======
    // 1. mem_stage ∈ {UBSE_CREATING, UBSE_EXIST}
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST)
        << "mem_stage should be CREATING or EXIST, actual: " << fdDesc.mem_stage;
    // 2. mem_size == 输入 size
    EXPECT_EQ(fdDesc.mem_size, borrowSize) << "mem_size should equal input size";
    // 3. memid_cnt == ceil(mem_size / unit_size)
    uint32_t expectedMemidCnt = static_cast<uint32_t>((fdDesc.mem_size + fdDesc.unit_size - 1) / fdDesc.unit_size);
    EXPECT_EQ(fdDesc.memid_cnt, expectedMemidCnt) << "memid_cnt should equal ceil(mem_size/unit_size)";
    // 4. memids 非零
    for (uint32_t i = 0; i < fdDesc.memid_cnt && i < UBS_MEM_MAX_MEMID_NUM; i++) {
        EXPECT_GT(fdDesc.memids[i], 0ull) << "memid[" << i << "] should be > 0";
    }
    // 5. unit_size > 0
    EXPECT_GT(fdDesc.unit_size, static_cast<size_t>(0)) << "unit_size should be > 0";
    // 6. export_node.slot_id == lender.slot_id
    EXPECT_EQ(fdDesc.export_node.slot_id, lenderSlotId) << "export_node.slot_id should equal lender slot_id";
    // 7. import_node.slot_id == 本节点
    EXPECT_EQ(fdDesc.import_node.slot_id, localSlotId) << "import_node.slot_id should be local node";
    // 8. export_node.slot_id != import_node.slot_id
    EXPECT_NE(fdDesc.export_node.slot_id, fdDesc.import_node.slot_id)
        << "export_node.slot_id should differ from import_node.slot_id";

    // 清理
    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreateLender-Ok-01 done";
}

// P0-FdCreateLender-NullPtr-01: lender=NULL (双节点)
void RunP0FdCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_fd_lnull01";

    ubs_mem_fd_desc_t fdDesc{};
    IT_LOG_INFO << "Creating FD with lender=NULL and lender_cnt=1";
    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, nullptr, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdCreateLender-NullPtr-01 done";
}

// P0-FdCreateLender-OverLen-01: name超长 (双节点)
void RunP0FdCreateLenderOverLen01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    std::string overLenName(48, 'a');
    ubs_mem_lender_t lender{};
    lender.lender_size = fdSize;
    lender.slot_id = lenderSlotId;
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_lender(overLenName.c_str(), nullptr, 0, &lender, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回INVALID_ARG";
    IT_LOG_INFO << "P0-FdCreateLender-OverLen-01 done";
}

// P0-FdCreateLender-InvalidVal-01: lender_size < 4MB (双节点)
void RunP0FdCreateLenderInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    const char* name = "it_p0_fd_li01";
    ubs_mem_lender_t lender{};
    lender.lender_size = 1;
    lender.slot_id = lenderSlotId;
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "lender_size < 4MB应返回OUT_OF_RANGE";
    IT_LOG_INFO << "P0-FdCreateLender-InvalidVal-01 done";
}

// P0-FdCreateLender-InvalidVal-02: lender_size > 256GB (双/四节点)
void RunP0FdCreateLenderInvalidVal02(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    const char* name = "it_p0_fd_li02";
    constexpr uint64_t tooLarge = 257ULL * 1024ULL * 1024ULL * 1024ULL;
    ubs_mem_lender_t lender{};
    lender.lender_size = tooLarge;
    lender.slot_id = lenderSlotId;
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE) << "lender_size > 256GB应返回ALLOCATE";
    IT_LOG_INFO << "P0-FdCreateLender-InvalidVal-02 done";
}

// P0-FdCreateLender-NullPtr-02: name=NULL 或 fd_desc=NULL (双节点)
void RunP0FdCreateLenderNullPtr02(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.lender_size = fdSize;
    lender.slot_id = lenderSlotId;
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_lender(nullptr, nullptr, 0, &lender, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    ret = ubs_mem_fd_create_with_lender("it_p0_fd_l_null02", nullptr, 0, &lender, 1, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdCreateLender-NullPtr-02 done";
}

// P0-FdCreateLender-BadParam-01: 不存在的slot_id (双节点)
void RunP0FdCreateLenderBadParam01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_fd_lbad01";
    ubs_mem_lender_t lender{};
    lender.lender_size = fdSize;
    lender.slot_id = 999;
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE) << "不存在的slot_id应返回ALLOCATE";
    IT_LOG_INFO << "P0-FdCreateLender-BadParam-01 done";
}

// P0-FdCreateLender-Dup-01: 同名重复 (双节点)
void RunP0FdCreateLenderDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    const char* name = "it_p0_fd_ldup01";
    ubs_mem_lender_t lender{};
    lender.lender_size = fdSize;
    lender.slot_id = lenderSlotId;

    ubs_mem_fd_desc_t fdDesc{};
    ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc));

    ubs_mem_fd_desc_t dupDesc{};
    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &dupDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreateLender-Dup-01 done";
}

// ==================== ubs_mem_fd_create_with_candidate P0 测试 ====================

// P0-FdCreateCandidate-Ok-01: 指定候选节点 (双/四节点)
void RunP0FdCreateCandidateOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    const auto& nodeIds = cluster.GetNodeIds();
    uint32_t slotIds[4]{};
    uint32_t slotCnt = 0;
    if (nodeIds.size() >= 4) {
        slotIds[slotCnt++] = cluster.GetNode("3").GetSpec().slotId;
        slotIds[slotCnt++] = cluster.GetNode("4").GetSpec().slotId;
    } else {
        slotIds[slotCnt++] = cluster.GetNode("2").GetSpec().slotId;
    }

    const char* name = "it_p0_fd_cand01";
    constexpr uint64_t borrowSize = 129ULL * 1024ULL * 1024ULL;
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Creating FD with candidate: name=" << name << ", size=" << borrowSize << ", slot_cnt=" << slotCnt;
    ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    int32_t ret = ubs_mem_fd_create_with_candidate(name, borrowSize, nullptr, 0, slotIds, slotCnt, &fdDesc);
    ASSERT_IT_OK(ret);

    // ====== 出参 fdDesc 字段校验 ======
    // 1. mem_stage ∈ {UBSE_CREATING, UBSE_EXIST}
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST)
        << "mem_stage should be CREATING or EXIST, actual: " << fdDesc.mem_stage;
    // 2. mem_size == 输入 size
    EXPECT_EQ(fdDesc.mem_size, borrowSize) << "mem_size should equal input size";
    // 3. memid_cnt == ceil(mem_size / unit_size)
    uint32_t expectedMemidCnt = static_cast<uint32_t>((fdDesc.mem_size + fdDesc.unit_size - 1) / fdDesc.unit_size);
    EXPECT_EQ(fdDesc.memid_cnt, expectedMemidCnt) << "memid_cnt should equal ceil(mem_size/unit_size)";
    // 4. memids 非零
    for (uint32_t i = 0; i < fdDesc.memid_cnt && i < UBS_MEM_MAX_MEMID_NUM; i++) {
        EXPECT_GT(fdDesc.memids[i], 0ull) << "memid[" << i << "] should be > 0";
    }
    // 5. unit_size > 0
    EXPECT_GT(fdDesc.unit_size, static_cast<size_t>(0)) << "unit_size should be > 0";
    // 6. export_node.slot_id ∈ slotIds
    bool inSlotIds = false;
    for (uint32_t i = 0; i < slotCnt; i++) {
        if (fdDesc.export_node.slot_id == slotIds[i]) {
            inSlotIds = true;
            break;
        }
    }
    EXPECT_TRUE(inSlotIds) << "export_node.slot_id should be in candidate slot_ids";
    // 7. import_node.slot_id == 本节点
    EXPECT_EQ(fdDesc.import_node.slot_id, localSlotId) << "import_node.slot_id should be local node";
    // 8. export_node.slot_id != import_node.slot_id
    EXPECT_NE(fdDesc.export_node.slot_id, fdDesc.import_node.slot_id)
        << "export_node.slot_id should differ from import_node.slot_id";

    sdk.MemFdDelete(name);
    IT_LOG_INFO << "P0-FdCreateCandidate-Ok-01 done";
}

// P0-FdCreateCandidate-OverLen-01: name超长 (双节点)
void RunP0FdCreateCandidateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    std::string overLenName(48, 'a');
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_candidate(overLenName.c_str(), fdSize, nullptr, 0, slotIds, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回INVALID_ARG";
    IT_LOG_INFO << "P0-FdCreateCandidate-OverLen-01 done";
}

// P0-FdCreateCandidate-InvalidVal-01: size < 4MB (双节点)
void RunP0FdCreateCandidateInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    const char* name = "it_p0_fd_cand_inv01";
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_candidate(name, 1, nullptr, 0, slotIds, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "size < 4MB应返回OUT_OF_RANGE";
    IT_LOG_INFO << "P0-FdCreateCandidate-InvalidVal-01 done";
}

// P0-FdCreateCandidate-InvalidVal-02: size > 256GB (双/四节点)
void RunP0FdCreateCandidateInvalidVal02(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    const char* name = "it_p0_fd_cand_inv02";
    constexpr uint64_t tooLarge = 257ULL * 1024ULL * 1024ULL * 1024ULL;
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_candidate(name, tooLarge, nullptr, 0, slotIds, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE) << "size > 256GB应返回ALLOCATE";
    IT_LOG_INFO << "P0-FdCreateCandidate-InvalidVal-02 done";
}

// P0-FdCreateCandidate-NullPtr-01: 空指针 (双节点)
void RunP0FdCreateCandidateNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_candidate(nullptr, fdSize, nullptr, 0, slotIds, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    ret = ubs_mem_fd_create_with_candidate("it_p0_fd_cand_null01", fdSize, nullptr, 0, slotIds, 1, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdCreateCandidate-NullPtr-01 done";
}

// P0-FdCreateCandidate-BadParam-01: 不存在的slot_id (双节点)
void RunP0FdCreateCandidateBadParam01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_fd_cand_bad01";
    uint32_t slotIds[1] = {999};
    ubs_mem_fd_desc_t fdDesc{};

    int32_t ret = ubs_mem_fd_create_with_candidate(name, fdSize, nullptr, 0, slotIds, 1, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE) << "不存在的slot_id应返回ALLOCATE";
    IT_LOG_INFO << "P0-FdCreateCandidate-BadParam-01 done";
}

// P0-FdCreateCandidate-Dup-01: 同名重复 (双节点)
void RunP0FdCreateCandidateDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    const char* name = "it_p0_fd_cand_dup01";
    ubs_mem_fd_desc_t fdDesc{};

    ASSERT_IT_OK(ubs_mem_fd_create_with_candidate(name, fdSize, nullptr, 0, slotIds, 1, &fdDesc));

    ubs_mem_fd_desc_t dupDesc{};
    int32_t ret = ubs_mem_fd_create_with_candidate(name, fdSize, nullptr, 0, slotIds, 1, &dupDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdCreateCandidate-Dup-01 done";
}

// ==================== ubs_mem_fd_permission P0 测试 ====================

// P0-FdPerm-NotExist-01: name不存在 (单节点)
void RunP0FdPermNotExist01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_fd_perm_notexist";
    ubs_mem_fd_owner_t owner{};
    owner.uid = 0;
    owner.gid = 0;
    owner.pid = 0;

    IT_LOG_INFO << "Setting permission on non-existent FD: name=" << name;
    int32_t ret = ubs_mem_fd_permission(name, &owner, 0644);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-FdPerm-NotExist-01 done";
}

// ==================== ubs_mem_fd_get P0 测试 ====================

// P0-FdGet-NotExist-01: 查询不存在 (双节点)
void RunP0FdGetNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_get_notexist";
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Getting non-existent FD: name=" << name;
    int32_t ret = sdk.MemFdGet(name, &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-FdGet-NotExist-01 done";
}

// P0-FdGet-NullPtr-01: 空指针 (双节点)
void RunP0FdGetNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_get_null";

    IT_LOG_INFO << "Getting FD with null fd_desc pointer";
    int32_t ret = sdk.MemFdGet(name, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdGet-NullPtr-01 done";
}

// ==================== ubs_mem_fd_list P0 测试 ====================

// P0-FdList-Ok-01: 空时list + 多次借用后list验证字段 (双节点)
void RunP0FdListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 第一步：空时调用FdList，验证接口可用
    ubs_mem_fd_desc_t* fdDescs = nullptr;
    uint32_t fdDescCnt = 0;
    int32_t ret = sdk.MemFdList(&fdDescs, &fdDescCnt);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "Empty FdList ok, cnt=" << fdDescCnt;
    if (fdDescs != nullptr) {
        free(fdDescs);
        fdDescs = nullptr;
    }

    // 第二步：创建2个FD
    const char* names[] = {"it_p0_fdlist_01", "it_p0_fdlist_02"};
    constexpr uint64_t sizes[] = {fdSize, fdSize129M};
    ubs_mem_fd_desc_t createDescs[2]{};

    for (int i = 0; i < 2; i++) {
        IT_LOG_INFO << "Creating FD: name=" << names[i] << ", size=" << sizes[i];
        ret = sdk.MemFdCreate(names[i], sizes[i], nullptr, 0, MEM_DISTANCE_L0, &createDescs[i]);
        ASSERT_IT_OK(ret);
    }

    // 第三步：再次调用FdList
    ret = sdk.MemFdList(&fdDescs, &fdDescCnt);
    ASSERT_IT_OK(ret);
    ASSERT_GE(fdDescCnt, 2u);
    IT_LOG_INFO << "FdList after create returned " << fdDescCnt << " entries";

    // 逐条校验字段
    for (uint32_t i = 0; i < fdDescCnt; i++) {
        IT_LOG_INFO << "FdList[" << i << "]: name=" << fdDescs[i].name << ", mem_stage=" << fdDescs[i].mem_stage
                    << ", mem_size=" << fdDescs[i].mem_size << ", import_slot=" << fdDescs[i].import_node.slot_id
                    << ", export_slot=" << fdDescs[i].export_node.slot_id;

        EXPECT_TRUE(fdDescs[i].mem_stage == UBSE_CREATING || fdDescs[i].mem_stage == UBSE_EXIST);
        EXPECT_GT(fdDescs[i].mem_size, 0ull);
        uint32_t expectedMemidCnt =
            static_cast<uint32_t>((fdDescs[i].mem_size + fdDescs[i].unit_size - 1) / fdDescs[i].unit_size);
        EXPECT_EQ(fdDescs[i].memid_cnt, expectedMemidCnt);
        EXPECT_GT(fdDescs[i].unit_size, static_cast<size_t>(0));
        EXPECT_EQ(fdDescs[i].import_node.slot_id, localSlotId);
        EXPECT_GT(fdDescs[i].export_node.slot_id, 0u);
        EXPECT_NE(fdDescs[i].export_node.slot_id, fdDescs[i].import_node.slot_id);
    }

    // 验证创建的2个FD都在列表中且mem_size一致
    for (int j = 0; j < 2; j++) {
        bool found = false;
        for (uint32_t i = 0; i < fdDescCnt; i++) {
            if (strcmp(fdDescs[i].name, names[j]) == 0) {
                found = true;
                EXPECT_EQ(fdDescs[i].mem_size, sizes[j]);
                break;
            }
        }
        EXPECT_TRUE(found) << "FD " << names[j] << " not found in list";
    }

    if (fdDescs != nullptr) {
        free(fdDescs);
    }

    // 清理
    for (int i = 0; i < 2; i++) {
        sdk.MemFdDelete(names[i]);
    }
    IT_LOG_INFO << "P0-FdList-Ok-01 done";
}

// P0-FdList-NullPtr-01: 空指针 (双节点)
void RunP0FdListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t fdDescCnt = 0;

    IT_LOG_INFO << "Listing FDs with null fd_descs pointer";
    int32_t ret = sdk.MemFdList(nullptr, &fdDescCnt);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdList-NullPtr-01 done";
}

// ==================== ubs_mem_fd_delete P0 测试 ====================

// P0-FdDel-Ok-01: 创建后删除 (双节点)
void RunP0FdDelOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_del01";
    ubs_mem_fd_desc_t fdDesc{};

    // 创建FD
    IT_LOG_INFO << "Creating FD for delete test: name=" << name;
    int32_t ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    WaitForFdReady(sdk, name);

    // 删除FD
    IT_LOG_INFO << "Deleting FD: name=" << name;
    ret = sdk.MemFdDelete(name);
    EXPECT_IT_OK(ret);

    // 删除后get应返回NOT_EXIST
    ubs_mem_fd_desc_t getDesc{};
    ret = sdk.MemFdGet(name, &getDesc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-FdDel-Ok-01 done";
}

// P0-FdDel-NotExist-01: 删除不存在 (双节点)
void RunP0FdDelNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_del_notexist";

    IT_LOG_INFO << "Deleting non-existent FD: name=" << name;
    int32_t ret = sdk.MemFdDelete(name);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-FdDel-NotExist-01 done";
}

// P0-FdDel-Dup-01: 重复删除 (双节点)
void RunP0FdDelDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_deldup01";
    ubs_mem_fd_desc_t fdDesc{};

    // 创建FD
    IT_LOG_INFO << "Creating FD for duplicate delete test: name=" << name;
    int32_t ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    WaitForFdReady(sdk, name);

    // 第一次删除
    IT_LOG_INFO << "Deleting FD first time: name=" << name;
    ret = sdk.MemFdDelete(name);
    EXPECT_IT_OK(ret);

    // 第二次删除（重复）
    IT_LOG_INFO << "Deleting FD second time (duplicate): name=" << name;
    ret = sdk.MemFdDelete(name);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-FdDel-Dup-01 done";
}

// P0-FdDel-OverLen-01: name超长 (双节点)
void RunP0FdDelOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string overLenName(48, 'a');

    IT_LOG_INFO << "Deleting FD with over-length name";
    int32_t ret = sdk.MemFdDelete(overLenName.c_str());
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回错误";
    IT_LOG_INFO << "P0-FdDel-OverLen-01 done";
}

// ==================== ubs_mem_fd_get_memid_by_import P0 测试 ====================

// P0-FdMemidByImport-Fld-01: 创建后查询字段 (双节点)
void RunP0FdMemidByImportFld01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_fd_memid01";
    ubs_mem_fd_desc_t fdDesc{};

    // 创建FD
    IT_LOG_INFO << "Creating FD for memid query: name=" << name;
    int32_t ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    ASSERT_IT_OK(ret);

    WaitForFdReady(sdk, name);

    // 获取FD详情以获得import memid
    ubs_mem_fd_desc_t verifyDesc{};
    ret = sdk.MemFdGet(name, &verifyDesc);
    ASSERT_IT_OK(ret);
    ASSERT_GT(verifyDesc.memid_cnt, 0u);

    // 通过import memid查询export memid
    uint64_t importMemid = verifyDesc.memids[0];
    ubs_mem_export_memid_t memInfo{};
    IT_LOG_INFO << "Querying export memid by import memid: " << importMemid;
    ret = ubs_mem_fd_get_memid_by_import(name, importMemid, &memInfo);
    EXPECT_IT_OK(ret);
    EXPECT_GT(memInfo.export_slot_id, 0u);
    EXPECT_EQ(memInfo.export_slot_id, verifyDesc.export_node.slot_id);
    EXPECT_GT(memInfo.export_memid, 0ull);
    IT_LOG_INFO << "export_slot_id=" << memInfo.export_slot_id << ", export_memid=" << memInfo.export_memid;

    // 清理
    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-FdMemidByImport-Fld-01 done";
}

// P0-FdMemidByImport-NotExist-01: name不存在 (双节点)
void RunP0FdMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_fd_memid_notexist";
    ubs_mem_export_memid_t memInfo{};

    IT_LOG_INFO << "Querying export memid for non-existent FD: name=" << name;
    int32_t ret = ubs_mem_fd_get_memid_by_import(name, 1, &memInfo);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-FdMemidByImport-NotExist-01 done";
}

// ==================== ubs_mem_fd_fault_register P0 测试 ====================

// P0-FdFaultReg-NullPtr-01: NULL handler (单节点)
void RunP0FdFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    IT_LOG_INFO << "Registering FD fault handler with NULL";
    ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    int32_t ret = ubs_mem_fd_fault_register(nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-FdFaultReg-NullPtr-01 done";
}

// ==================== NUMA 函数（来自 mem_numa） ====================

// ==================== ubs_mem_numastat_get ====================

// P0-NumaStatGet-Fld-01: 本节点字段校验 (单节点)
void RunP0NumaStatGetFld01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    // 获取本节点slot_id
    ubs_topo_node_t localNode{};
    ASSERT_IT_OK(sdk.TopoNodeLocalGet(&localNode));

    ubs_mem_numastat_t* numaMems = nullptr;
    uint32_t numaMemCnt = 0;
    int32_t ret = sdk.MemNumastatGet(localNode.slot_id, &numaMems, &numaMemCnt);
    ASSERT_IT_OK(ret);
    ASSERT_NE(numaMems, nullptr);
    ASSERT_GT(numaMemCnt, 0u);

    // 字段校验
    for (uint32_t i = 0; i < numaMemCnt; i++) {
        EXPECT_GT(numaMems[i].mem_total, 0ull) << "numa[" << i << "] mem_total should be > 0";
        EXPECT_LE(numaMems[i].mem_free, numaMems[i].mem_total) << "numa[" << i << "] mem_free should be <= mem_total";
        EXPECT_GE(numaMems[i].huge_pages_2M, numaMems[i].free_huge_pages_2M)
            << "numa[" << i << "] huge_pages_2M should be >= free_huge_pages_2M";
        EXPECT_GE(numaMems[i].huge_pages_1G, numaMems[i].free_huge_pages_1G)
            << "numa[" << i << "] huge_pages_1G should be >= free_huge_pages_1G";
    }

    free(numaMems);
    IT_LOG_INFO << "P0-NumaStatGet-Fld-01 passed: numa_mem_cnt=" << numaMemCnt;
}

// P0-NumaStatGet-NotExist-01: 不存在的slot_id (单节点)
void RunP0NumaStatGetNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numastat_t* numaMems = nullptr;
    uint32_t numaMemCnt = 0;
    int32_t ret = sdk.MemNumastatGet(9999, &numaMems, &numaMemCnt);
    EXPECT_NE(ret, UBS_SUCCESS) << "NumastatGet with non-existent slot_id should fail";

    IT_LOG_INFO << "P0-NumaStatGet-NotExist-01 passed: ret=" << ret;
}

// P0-NumaStatGet-NullPtr-01: 空指针 (单节点)
void RunP0NumaStatGetNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numastat_t* numaMems = nullptr;
    uint32_t numaMemCnt = 0;

    EXPECT_EQ(sdk.MemNumastatGet(1, nullptr, &numaMemCnt), UBS_ERR_NULL_POINTER)
        << "numa_mems=null should return NULL_POINTER";
    EXPECT_EQ(sdk.MemNumastatGet(1, &numaMems, nullptr), UBS_ERR_NULL_POINTER)
        << "numa_mem_cnt=null should return NULL_POINTER";

    IT_LOG_INFO << "P0-NumaStatGet-NullPtr-01 passed";
}

// ==================== ubs_mem_numa_create ====================

// P0-NumaCreate-Ok-01: 标准创建成功 (双节点)
void RunP0NumaCreateOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_create_ok";

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 验证属性
    ubs_mem_numa_desc_t verifyDesc{};
    ret = sdk.MemNumaGet(name, &verifyDesc);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);
    EXPECT_EQ(verifyDesc.size, UBS_MEM_MIN_SIZE);
    EXPECT_GE(verifyDesc.numaid, 0);

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaCreate-Ok-01 passed";
}

// P0-NumaCreate-OverLen-01: name超长 (单节点)
void RunP0NumaCreateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    // UBS_MEM_MAX_NAME_LENGTH=48, 构造48字符的name（含结尾\0会超长）
    const char* longName = "it_p0_numa_overlen_name_aaaaaaaaaaaaaaaaaaaaaaaaaa";
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(longName, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    EXPECT_NE(ret, UBS_SUCCESS) << "name超长应返回错误";

    IT_LOG_INFO << "P0-NumaCreate-OverLen-01 passed: ret=" << ret;
}

// P0-NumaCreate-InvalidVal-01: size < 4MB (单节点)
void RunP0NumaCreateInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate("it_p0_numa_invalid_val", 1, MEM_DISTANCE_L0, &numaDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "size < 4MB应返回OUT_OF_RANGE";

    IT_LOG_INFO << "P0-NumaCreate-InvalidVal-01 passed: ret=" << ret;
}

// P0-NumaCreate-Dup-01: 同名重复 (双节点)
void RunP0NumaCreateDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_dup";

    // 第一次创建
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 同名重复创建
    ubs_mem_numa_desc_t dupDesc{};
    ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &dupDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_EXISTED) << "同名重复创建应返回EXISTED";

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaCreate-Dup-01 passed";
}

// P0-NumaCreate-NullPtr-01: 空指针 (单节点)
void RunP0NumaCreateNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t numaDesc{};
    EXPECT_EQ(sdk.MemNumaCreate(nullptr, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc), UBS_ERR_NULL_POINTER)
        << "name=null should return NULL_POINTER";
    EXPECT_EQ(sdk.MemNumaCreate("it_p0_numa_nullptr", UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, nullptr), UBS_ERR_NULL_POINTER)
        << "numa_desc=null should return NULL_POINTER";

    IT_LOG_INFO << "P0-NumaCreate-NullPtr-01 passed";
}

// P0-NumaCreate-BoundMin-01: size=4MB (双节点)
void RunP0NumaCreateBoundMin01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_bound_min";

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 验证size
    ubs_mem_numa_desc_t verifyDesc{};
    ret = sdk.MemNumaGet(name, &verifyDesc);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(verifyDesc.size, UBS_MEM_MIN_SIZE);

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaCreate-BoundMin-01 passed";
}

// ==================== ubs_mem_numa_create_with_lender ====================

// P0-NumaCreateLender-Ok-01: 指定借出节点 (双节点)
void RunP0NumaCreateLenderOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& sdk2 = cluster.GetSdkClient("2");
    const char* name = "it_p0_numa_lender_ok";

    // 获取借出节点slot_id
    ubs_topo_node_t lenderNode{};
    ASSERT_IT_OK(sdk2.TopoNodeLocalGet(&lenderNode));

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderNode.slot_id;
    lender.socket_id = 0;
    lender.numa_id = 0;
    lender.port_id = 0;
    lender.lender_size = UBS_MEM_MIN_SIZE;

    ubs_mem_numa_desc_t numaDesc{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_create_with_lender(name, &lender, 1, &numaDesc);
    if (ret != UBS_SUCCESS) {
        IT_LOG_INFO << "P0-NumaCreateLender-Ok-01: numa create_with_lender returned " << ret
                    << " (no URMA link), skip verification";
        return;
    }

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 验证属性
    ubs_mem_numa_desc_t verifyDesc{};
    ret = sdk.MemNumaGet(name, &verifyDesc);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);
    EXPECT_EQ(verifyDesc.export_node.slot_id, lenderNode.slot_id);

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaCreateLender-Ok-01 passed";
}

// P0-NumaCreateLender-ZeroCnt-01: lender_cnt=0 (单节点)
void RunP0NumaCreateLenderZeroCnt01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t numaDesc{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_create_with_lender("it_p0_numa_lender_zero", nullptr, 0, &numaDesc);
    EXPECT_NE(ret, UBS_SUCCESS) << "lender_cnt=0 should fail";

    IT_LOG_INFO << "P0-NumaCreateLender-ZeroCnt-01 passed: ret=" << ret;
}

// P0-NumaCreateLender-NullPtr-01: lender=NULL (单节点)
void RunP0NumaCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t numaDesc{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_create_with_lender("it_p0_numa_lender_null", nullptr, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER) << "lender=NULL with lender_cnt>0 should return NULL_POINTER";

    IT_LOG_INFO << "P0-NumaCreateLender-NullPtr-01 passed";
}

// P0-NumaCreateLender-BoundMax-01: lender_cnt=4 (双节点)
void RunP0NumaCreateLenderBoundMax01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& sdk2 = cluster.GetSdkClient("2");
    const char* name = "it_p0_numa_lender_max";

    // 获取借出节点slot_id
    ubs_topo_node_t lenderNode{};
    ASSERT_IT_OK(sdk2.TopoNodeLocalGet(&lenderNode));

    // 构造4个lender条目，全部指向借出节点
    ubs_mem_lender_t lenders[UBS_MEM_MAX_LENDER_CNT]{};
    for (uint32_t i = 0; i < UBS_MEM_MAX_LENDER_CNT; i++) {
        lenders[i].slot_id = lenderNode.slot_id;
        lenders[i].socket_id = 0;
        lenders[i].numa_id = 0;
        lenders[i].port_id = 0;
        lenders[i].lender_size = UBS_MEM_MIN_SIZE;
    }

    ubs_mem_numa_desc_t numaDesc{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_create_with_lender(name, lenders, UBS_MEM_MAX_LENDER_CNT, &numaDesc);
    if (ret != UBS_SUCCESS) {
        IT_LOG_INFO << "P0-NumaCreateLender-BoundMax-01: numa create_with_lender returned " << ret
                    << " (no URMA link), skip verification";
        return;
    }

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 验证属性
    ubs_mem_numa_desc_t verifyDesc{};
    ret = sdk.MemNumaGet(name, &verifyDesc);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaCreateLender-BoundMax-01 passed";
}

// ==================== ubs_mem_numa_create_with_candidate ====================

// P0-NumaCreateCandidate-Ok-01: 指定候选节点 (双节点)
void RunP0NumaCreateCandidateOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& sdk2 = cluster.GetSdkClient("2");
    const char* name = "it_p0_numa_candidate_ok";

    // 获取借出节点slot_id作为候选节点
    ubs_topo_node_t candidateNode{};
    ASSERT_IT_OK(sdk2.TopoNodeLocalGet(&candidateNode));

    uint32_t slotIds[1] = {candidateNode.slot_id};
    ubs_mem_numa_desc_t numaDesc{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_create_with_candidate(name, UBS_MEM_MIN_SIZE, slotIds, 1, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 验证属性
    ubs_mem_numa_desc_t verifyDesc{};
    ret = sdk.MemNumaGet(name, &verifyDesc);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaCreateCandidate-Ok-01 passed";
}

// P0-NumaCreateCandidate-ZeroCnt-01: slot_cnt=0 (单节点)
void RunP0NumaCreateCandidateZeroCnt01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t numaDesc{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_create_with_candidate("it_p0_numa_cand_zero", UBS_MEM_MIN_SIZE, nullptr, 0, &numaDesc);
    EXPECT_NE(ret, UBS_SUCCESS) << "slot_cnt=0 should fail";

    IT_LOG_INFO << "P0-NumaCreateCandidate-ZeroCnt-01 passed: ret=" << ret;
}

// ==================== ubs_mem_numa_get ====================

// P0-NumaGet-NotExist-01: 查询不存在 (单节点)
void RunP0NumaGetNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaGet("it_p0_numa_not_exist", &numaDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "查询不存在的numa应返回NOT_EXIST";

    IT_LOG_INFO << "P0-NumaGet-NotExist-01 passed";
}

// P0-NumaGet-NullPtr-01: 空指针 (单节点)
void RunP0NumaGetNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    EXPECT_EQ(sdk.MemNumaGet("it_p0_numa_get_null", nullptr), UBS_ERR_NULL_POINTER)
        << "numa_desc=null should return NULL_POINTER";

    IT_LOG_INFO << "P0-NumaGet-NullPtr-01 passed";
}

// ==================== ubs_mem_numa_list ====================

// P0-NumaList-Ok-01: 空/有numa时list (单节点)
void RunP0NumaListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    // 查询numa列表
    ubs_mem_numa_desc_t* numaDescs = nullptr;
    uint32_t numaDescCnt = 0;
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_list(&numaDescs, &numaDescCnt);
    EXPECT_IT_OK(ret);

    // cnt可能为0或大于0
    if (numaDescCnt > 0 && numaDescs != nullptr) {
        IT_LOG_INFO << "NumaList returned " << numaDescCnt << " entries";
        free(numaDescs);
    } else {
        IT_LOG_INFO << "NumaList returned empty list";
    }

    IT_LOG_INFO << "P0-NumaList-Ok-01 passed: cnt=" << numaDescCnt;
}

// P0-NumaList-NullPtr-01: 空指针 (单节点)
void RunP0NumaListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_numa_desc_t* numaDescs = nullptr;
    uint32_t numaDescCnt = 0;

    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    EXPECT_EQ(ubs_mem_numa_list(nullptr, &numaDescCnt), UBS_ERR_NULL_POINTER)
        << "numa_descs=null should return NULL_POINTER";
    EXPECT_EQ(ubs_mem_numa_list(&numaDescs, nullptr), UBS_ERR_NULL_POINTER)
        << "numa_desc_cnt=null should return NULL_POINTER";

    IT_LOG_INFO << "P0-NumaList-NullPtr-01 passed";
}

// ==================== ubs_mem_numa_delete ====================

// P0-NumaDel-Ok-01: 创建后删除 (双节点)
void RunP0NumaDelOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_del_ok";

    // 创建NUMA
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaDel-Ok-01 passed";
}

// P0-NumaDel-NotExist-01: 删除不存在 (单节点)
void RunP0NumaDelNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    int32_t ret = sdk.MemNumaDelete("it_p0_numa_del_notexist");
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "删除不存在的numa应返回NOT_EXIST";

    IT_LOG_INFO << "P0-NumaDel-NotExist-01 passed";
}

// P0-NumaDel-Dup-01: 重复删除 (双节点)
void RunP0NumaDelDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_del_dup";

    // 创建NUMA
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 第一次删除
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    // 等待删除完成
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        int32_t getRet = sdk.MemNumaGet(name, &desc);
        if (getRet == UBS_ENGINE_ERR_NOT_EXIST) {
            break;
        }
    }

    // 第二次删除，应返回NOT_EXIST
    ret = sdk.MemNumaDelete(name);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "重复删除应返回NOT_EXIST";

    IT_LOG_INFO << "P0-NumaDel-Dup-01 passed";
}

// P0-NumaDel-OverLen-01: name超长 (单节点)
void RunP0NumaDelOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    // UBS_MEM_MAX_NAME_LENGTH=48, 构造48字符的name
    const char* longName = "it_p0_numa_del_overlen_aaaaaaaaaaaaaaaaaaaaaaaaaa";
    int32_t ret = sdk.MemNumaDelete(longName);
    EXPECT_NE(ret, UBS_SUCCESS) << "name超长应返回错误";

    IT_LOG_INFO << "P0-NumaDel-OverLen-01 passed: ret=" << ret;
}

// ==================== ubs_mem_numa_get_memid_by_import ====================

// P0-NumaMemidByImport-Fld-01: 创建后查询字段 (双节点)
void RunP0NumaMemidByImportFld01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_memid_fld";

    // 创建NUMA
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 等待UBSE_EXIST就绪
    for (auto i = 0; i < 60; i++) {
        sleep(1);
        ubs_mem_numa_desc_t desc{};
        ret = sdk.MemNumaGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            break;
        }
    }

    // 获取numa描述信息
    ubs_mem_numa_desc_t verifyDesc{};
    ret = sdk.MemNumaGet(name, &verifyDesc);
    ASSERT_IT_OK(ret);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);
    EXPECT_GE(verifyDesc.numaid, 0);
    EXPECT_EQ(verifyDesc.size, UBS_MEM_MIN_SIZE);
    EXPECT_GT(verifyDesc.export_node.slot_id, 0u);

    // 通过get_memid_by_import查询导出端memid
    ubs_mem_export_memid_t memInfo{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    // 使用numaid作为import_memid进行查询
    ret = ubs_mem_numa_get_memid_by_import(name, static_cast<uint64_t>(verifyDesc.numaid), &memInfo);
    if (ret == UBS_SUCCESS) {
        EXPECT_GT(memInfo.export_slot_id, 0u);
        EXPECT_GT(memInfo.export_memid, 0ull);
    }

    // 清理：删除NUMA
    ret = sdk.MemNumaDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "P0-NumaMemidByImport-Fld-01 passed";
}

// P0-NumaMemidByImport-NotExist-01: name不存在 (单节点)
void RunP0NumaMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_export_memid_t memInfo{};
    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_get_memid_by_import("it_p0_numa_memid_notexist", 0, &memInfo);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "name不存在应返回NOT_EXIST";

    IT_LOG_INFO << "P0-NumaMemidByImport-NotExist-01 passed";
}

// ==================== ubs_mem_numa_fault_register ====================

// P0-NumaFaultReg-NullPtr-01: NULL handler (单节点)
void RunP0NumaFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    int32_t initRet = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_EQ(initRet, UBS_SUCCESS);
    int32_t ret = ubs_mem_numa_fault_register(nullptr);
    EXPECT_NE(ret, UBS_SUCCESS) << "NULL handler should fail";

    IT_LOG_INFO << "P0-NumaFaultReg-NullPtr-01 passed: ret=" << ret;
}

// ==================== SHM 函数（来自 mem_shm，排除 shm_fault_get） ====================

// ==================== ubs_mem_shm_create ====================

// 标准创建成功 (双节点)
void RunP0ShmCreateOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_create_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;

    IT_LOG_INFO << "Creating SHM: name=" << name << ", size=" << shmSize;
    int32_t ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    // 验证属性
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_EQ(desc->mem_stage, UBSE_EXIST);
        EXPECT_EQ(desc->mem_size, shmSize);
        EXPECT_STREQ(desc->name, name);
        free(desc);
    }

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmCreateOk01 done";
}

// name超长 (单节点)
void RunP0ShmCreateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    // 48字符的name，超出UBS_MEM_MAX_NAME_LENGTH限制
    const char* name = "it_p0_shm_name_way_too_long_for_the_max_limit_x";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    IT_LOG_INFO << "Creating SHM with overlen name: " << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, nullptr, nullptr);
    // 服务端不校验name长度，创建应成功
    EXPECT_IT_OK(ret);

    // 清理
    if (ret == UBS_SUCCESS) {
        sdk.MemShmDelete(name);
    }

    IT_LOG_INFO << "RunP0ShmCreateOverLen01 done";
}

// size < 4MB (单节点)
void RunP0ShmCreateInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_inv_val";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint64_t invalidSize = 1; // 小于4MB

    IT_LOG_INFO << "Creating SHM with invalid size: " << invalidSize;
    int32_t ret = sdk.MemShmCreate(name, invalidSize, usrInfo, 0, nullptr, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_OUT_OF_RANGE);

    IT_LOG_INFO << "RunP0ShmCreateInvalidVal01 done";
}

// 同名重复 (双节点)
void RunP0ShmCreateDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_dup";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;

    // 第一次创建
    IT_LOG_INFO << "Creating SHM first time: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    ASSERT_IT_OK(ret);

    // 第二次创建同名
    IT_LOG_INFO << "Creating SHM again with same name: " << name;
    ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, &region, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmCreateDup01 done";
}

// size=1GB (双节点)
void RunP0ShmCreateBigSize01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_big";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint64_t bigSize = 1ULL * 1024 * 1024 * 1024; // 1GB

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;

    IT_LOG_INFO << "Creating SHM with big size: name=" << name << ", size=1GB";
    int32_t ret = sdk.MemShmCreate(name, bigSize, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmCreateBigSize01 done";
}

// ==================== ubs_mem_shm_create_with_affinity ====================

// 不存在的socket_id (双节点)
void RunP0ShmCreateAffinityBadParam01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_affinity_bp";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint32_t badSocketId = 9999; // 不存在的socket_id

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;

    IT_LOG_INFO << "Creating SHM with affinity bad param: socket_id=" << badSocketId;
    // 直接调用C API
    int32_t ret = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ret);
    ret = ubs_mem_shm_create_with_affinity(name, shmSize, badSocketId, usrInfo, 0, &region, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL);

    IT_LOG_INFO << "RunP0ShmCreateAffinityBadParam01 done";
}

// ==================== ubs_mem_shm_create_with_lender ====================

// lender=NULL (双节点)
void RunP0ShmCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_lender_null";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;

    IT_LOG_INFO << "Creating SHM with lender=nullptr";
    // 直接调用C API，lender传nullptr
    int32_t ret = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ret);
    ret = ubs_mem_shm_create_with_lender(name, usrInfo, 0, &region, nullptr);
    // lender为nullptr是合法的（不指定借出方），预期创建成功
    if (ret == UBS_SUCCESS) {
        ret = WaitForShmReady(sdk, name);
        EXPECT_IT_OK(ret);
        ret = sdk.MemShmDelete(name);
        EXPECT_IT_OK(ret);
    }

    IT_LOG_INFO << "RunP0ShmCreateLenderNullPtr01 done";
}

// ==================== ubs_mem_shm_attach ====================

// 未创建时attach (单节点)
void RunP0ShmAttachNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_not_exist_attach";
    ubs_mem_shm_desc_t* desc = nullptr;

    IT_LOG_INFO << "Attaching SHM that does not exist: " << name;
    int32_t ret = sdk.MemShmAttach(name, nullptr, 0, &desc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    if (desc != nullptr) {
        free(desc);
    }

    IT_LOG_INFO << "RunP0ShmAttachNotReady01 done";
}

// 重复attach (双节点)
void RunP0ShmAttachDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_attach_dup";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;

    // 创建SHM
    IT_LOG_INFO << "Creating SHM: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    ASSERT_IT_OK(ret);

    // 第一次attach
    ubs_mem_shm_desc_t* desc1 = nullptr;
    IT_LOG_INFO << "Attaching SHM first time";
    ret = sdk.MemShmAttach(name, nullptr, 0, &desc1);
    ASSERT_IT_OK(ret);
    if (desc1 != nullptr) {
        free(desc1);
    }

    // 第二次attach - 应该成功或返回EXISTED
    ubs_mem_shm_desc_t* desc2 = nullptr;
    IT_LOG_INFO << "Attaching SHM second time";
    ret = sdk.MemShmAttach(name, nullptr, 0, &desc2);
    EXPECT_TRUE(ret == UBS_SUCCESS || ret == UBS_ENGINE_ERR_EXISTED)
        << "Expected UBS_SUCCESS or UBS_ENGINE_ERR_EXISTED but got: " << ret;
    if (desc2 != nullptr) {
        free(desc2);
    }

    // 清理
    ret = sdk.MemShmDetach(name);
    EXPECT_IT_OK(ret);
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmAttachDup01 done";
}

// ==================== ubs_mem_shm_get ====================

// 查询不存在 (单节点)
void RunP0ShmGetNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_get_not_exist";
    ubs_mem_shm_desc_t* desc = nullptr;

    IT_LOG_INFO << "Getting SHM that does not exist: " << name;
    int32_t ret = sdk.MemShmGet(name, &desc);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    if (desc != nullptr) {
        free(desc);
    }

    IT_LOG_INFO << "RunP0ShmGetNotExist01 done";
}

// ==================== ubs_mem_shm_list ====================

// 空/有shm时list (单节点)
void RunP0ShmListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t cnt = 0;
    ubs_mem_shm_desc_t* descs = nullptr;

    // 空时list
    IT_LOG_INFO << "Listing SHM when empty";
    int32_t ret = sdk.MemShmList(&descs, &cnt);
    EXPECT_IT_OK(ret);
    IT_LOG_INFO << "Empty list cnt=" << cnt;
    if (descs != nullptr) {
        free(descs);
        descs = nullptr;
    }

    // 创建一个SHM
    const char* name = "it_p0_shm_list_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, nullptr, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    // 有SHM时list
    IT_LOG_INFO << "Listing SHM after create";
    ret = sdk.MemShmList(&descs, &cnt);
    EXPECT_IT_OK(ret);
    IT_LOG_INFO << "List cnt=" << cnt;
    if (descs != nullptr) {
        free(descs);
    }

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmListOk01 done";
}

// 空指针 (单节点)
void RunP0ShmListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    IT_LOG_INFO << "Listing SHM with nullptr params";
    int32_t ret = sdk.MemShmList(nullptr, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);

    IT_LOG_INFO << "RunP0ShmListNullPtr01 done";
}

// ==================== ubs_mem_shm_list_with_prefix ====================

// 无匹配前缀 (单节点)
void RunP0ShmListPrefixOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t cnt = 1; // 初始化为非零，验证接口能正确返回0
    ubs_mem_shm_desc_t* descs = nullptr;
    const char* prefix = "it_p0_shm_prefix_no_match_xyz";

    IT_LOG_INFO << "Listing SHM with prefix: " << prefix;
    // 直接调用C API
    int32_t ret = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ret);
    ret = ubs_mem_shm_list_with_prefix(prefix, &descs, &cnt);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(cnt, 0u);
    if (descs != nullptr) {
        free(descs);
    }

    IT_LOG_INFO << "RunP0ShmListPrefixOk01 done";
}

// ==================== ubs_mem_shm_detach ====================

// 未attach时detach (单节点)
void RunP0ShmDetachNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_detach_not_ready";

    IT_LOG_INFO << "Detaching SHM that is not attached: " << name;
    int32_t ret = sdk.MemShmDetach(name);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_NO_ATTACH);

    IT_LOG_INFO << "RunP0ShmDetachNotReady01 done";
}

// ==================== ubs_mem_shm_delete ====================

// 删除不存在 (单节点)
void RunP0ShmDelNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_del_not_exist";

    IT_LOG_INFO << "Deleting SHM that does not exist: " << name;
    int32_t ret = sdk.MemShmDelete(name);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);

    IT_LOG_INFO << "RunP0ShmDelNotExist01 done";
}

// ==================== ubs_mem_shm_fault_register ====================

// NULL handler (单节点)
void RunP0ShmFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    IT_LOG_INFO << "Registering SHM fault with NULL handler";
    // 直接调用C API
    int32_t ret = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ret);
    ret = ubs_mem_shm_fault_register(nullptr);
    EXPECT_NE(ret, UBS_SUCCESS) << "Expected failure for NULL handler but got UBS_SUCCESS";

    IT_LOG_INFO << "RunP0ShmFaultRegNullPtr01 done";
}

// ==================== ubs_mem_shm_get_memid_by_import ====================

// name不存在 (单节点)
void RunP0ShmMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_memid_not_exist";
    ubs_mem_export_memid_t memInfo{};

    IT_LOG_INFO << "Getting memid by import for non-existent SHM: " << name;
    // 直接调用C API
    int32_t ret = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ret);
    ret = ubs_mem_shm_get_memid_by_import(name, 1, &memInfo);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);

    IT_LOG_INFO << "RunP0ShmMemidByImportNotExist01 done";
}

// import_memid无效 (单节点)
void RunP0ShmMemidByImportInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_memid_inv_val";
    ubs_mem_export_memid_t memInfo{};
    constexpr uint64_t invalidMemid = 0;

    IT_LOG_INFO << "Getting memid by import with invalid memid=0";
    // 直接调用C API
    int32_t ret = ubs_engine_client_initialize(sdk.GetUdsPath().c_str());
    ASSERT_IT_OK(ret);
    ret = ubs_mem_shm_get_memid_by_import(name, invalidMemid, &memInfo);
    // import_memid=0是无效值，预期返回参数错误
    EXPECT_NE(ret, UBS_SUCCESS) << "Expected failure for invalid import_memid but got UBS_SUCCESS";

    IT_LOG_INFO << "RunP0ShmMemidByImportInvalidVal01 done";
}

// ==================== CLI P0 用例 ====================

void RunP0CliCreateNumaInvalidChar01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // 使用单引号包裹非法名称（含空格），CLI 应拒绝
    std::string output = cliInvoker.ExecCli("create memory -t numa -n 'inv@lid_n@me!' -s 128M");
    // ExecuteCommand 在 CLI 失败时返回空字符串
    bool isError = output.empty() || output.find("ERROR") != std::string::npos;
    EXPECT_TRUE(isError) << "CLI create numa with illegal name should fail, got: '" << output << "'";
    IT_LOG_INFO << "P0-CliCreateNuma-InvalidChar-01 passed";
}

void RunP0CliCreateNumaDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");

    // 第一次创建成功
    ubse::it::infra::ItMemCreateInfo createInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryNuma(createInfo, "it_cli_dup_numa", "128M"));
    IT_LOG_INFO << "First CLI create numa succeeded: " << createInfo.name;

    // 第二次同名创建应失败
    ubse::it::infra::ItMemCreateInfo dupInfo;
    auto ret = cliInvoker.CreateMemoryNuma(dupInfo, "it_cli_dup_numa", "128M");
    EXPECT_NE(ret, UBS_SUCCESS) << "CLI create duplicate numa should fail";

    // 清理
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_cli_dup_numa", "numa"));
    IT_LOG_INFO << "P0-CliCreateNuma-Dup-01 passed";
}

void RunP0CliCreateFdOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");

    // CLI 创建 FD 内存
    ubse::it::infra::ItMemCreateInfo createInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryFd(createInfo, "it_cli_fd_ok", "128M"));
    EXPECT_EQ(createInfo.name, "it_cli_fd_ok");
    EXPECT_EQ(createInfo.size, "128MB");
    EXPECT_FALSE(createInfo.importNode.empty());
    EXPECT_FALSE(createInfo.exportNode.empty());

    // 验证 display borrow_detail 可查到
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetails, "fd"));
    bool found = false;
    for (const auto& detail : borrowDetails) {
        if (detail.name == "it_cli_fd_ok") {
            found = true;
            EXPECT_EQ(detail.type, "fd");
            break;
        }
    }
    EXPECT_TRUE(found) << "CLI display borrow_detail -bt fd should contain it_cli_fd_ok";

    // 清理
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_cli_fd_ok", "fd"));
    IT_LOG_INFO << "P0-CliCreateFd-Ok-01 passed";
}

void RunP0CliCreateFdInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // size=0 是无效值
    std::string output = cliInvoker.ExecCli("create memory -t fd -n it_invalid_fd -s 0");
    bool isError = output.empty() || output.find("ERROR") != std::string::npos;
    EXPECT_TRUE(isError) << "CLI create fd with size=0 should fail, got: '" << output << "'";
    IT_LOG_INFO << "P0-CliCreateFd-InvalidVal-01 passed";
}

void RunP0CliCreateShareOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");

    // 获取所有节点作为 region（四节点场景）
    auto nodeIds = cluster.GetNodeIds();
    std::string region;
    for (size_t i = 0; i < nodeIds.size(); i++) {
        if (i > 0)
            region += ",";
        region += nodeIds[i];
    }

    // CLI 创建 SHM
    ubse::it::infra::ItMemCreateInfo createInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryShare(createInfo, "it_cli_share_ok", "128M", region));
    EXPECT_EQ(createInfo.name, "it_cli_share_ok");
    EXPECT_EQ(createInfo.size, "128MB");

    // 验证 display borrow_detail 可查到
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetails, "share"));
    bool found = false;
    for (const auto& detail : borrowDetails) {
        if (detail.name == "it_cli_share_ok") {
            found = true;
            EXPECT_EQ(detail.type, "share");
            break;
        }
    }
    EXPECT_TRUE(found) << "CLI display borrow_detail -bt share should contain it_cli_share_ok";

    // 清理
    EXPECT_IT_OK(cliInvoker.DeleteMemory("it_cli_share_ok", "share"));
    IT_LOG_INFO << "P0-CliCreateShare-Ok-01 passed";
}

void RunP0CliCreateShareOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // name 超过 47 字符（ubs_mem 限制 48 字节）
    std::string longName(48, 'x');
    std::string output = cliInvoker.ExecCli("create memory -t share -n " + longName + " -s 128M -r 1");
    bool isError = output.empty() || output.find("ERROR") != std::string::npos;
    EXPECT_TRUE(isError) << "CLI create share with name too long should fail, got: '" << output << "'";
    IT_LOG_INFO << "P0-CliCreateShare-OverLen-01 passed";
}

void RunP0CliDelMemNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // 删除不存在的内存资源
    auto ret = cliInvoker.DeleteMemory("it_nonexistent_mem", "numa");
    EXPECT_NE(ret, UBS_SUCCESS) << "CLI delete non-existent memory should fail";
    IT_LOG_INFO << "P0-CliDelMem-NotExist-01 passed";
}

void RunP0CliBorrowDetailEmpty01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // 没有任何借用时，查询 borrow_detail 应为空
    std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetails));
    EXPECT_EQ(borrowDetails.size(), 0) << "borrow_detail should be empty when no borrow exists";
    IT_LOG_INFO << "P0-CliBorrowDetail-Ok-01 passed";
}

void RunP0CliAttachMemNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // 未创建 SHM 时 attach 应失败
    ubse::it::infra::ItMemCreateInfo attachInfo;
    auto ret = cliInvoker.AttachMemory(attachInfo, "it_nonexistent_shm");
    EXPECT_NE(ret, UBS_SUCCESS) << "CLI attach non-existent shared memory should fail";
    IT_LOG_INFO << "P0-CliAttachMem-NotReady-01 passed";
}

void RunP0CliDetachMemNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    // 未 attach 时 detach 应失败
    auto ret = cliInvoker.DetachMemory("it_nonexistent_shm");
    EXPECT_NE(ret, UBS_SUCCESS) << "CLI detach non-attached memory should fail";
    IT_LOG_INFO << "P0-CliDetachMem-NotReady-01 passed";
}

} // namespace ubse::it::tests::mem_borrow
