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
constexpr const char* lenderNodeId = "1"; // 借出方节点
constexpr uint32_t lenderSlotId = 1;      // 借出方槽位

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

// 通过topo链路获取借出节点的port_id/socket_id/numa_id，直接填充lender结构体
// 调用前需设置lender.slot_id；找到链路返回true；未找到或查询失败返回false
bool FillLenderTopoInfo(ubse::it::infra::ItSdkClient& sdk, uint32_t localSlotId, ubs_mem_lender_t& lender)
{
    // 从链路获取port_id和socket_id
    ubs_topo_link_t* links = nullptr;
    uint32_t linkCnt = 0;
    if (sdk.TopoLinkList(&links, &linkCnt) != UBS_SUCCESS) {
        free(links);
        return false;
    }
    bool found = false;
    for (uint32_t i = 0; i < linkCnt; i++) {
        if (links[i].slot_id == localSlotId && links[i].peer_slot_id == lender.slot_id) {
            lender.port_id = links[i].peer_port_id;
            lender.socket_id = links[i].peer_socket_id;
            found = true;
            break;
        }
    }
    free(links);
    if (!found) {
        return false;
    }

    // 从节点列表获取numa_id: 在借出节点中找到匹配socket_id的socket，取其第一个numa
    ubs_topo_node_t* nodes = nullptr;
    uint32_t nodeCnt = 0;
    if (sdk.TopoNodeList(&nodes, &nodeCnt) != UBS_SUCCESS) {
        free(nodes);
        return false;
    }
    bool numaFound = false;
    for (uint32_t i = 0; i < nodeCnt; i++) {
        if (nodes[i].slot_id != lender.slot_id) {
            continue;
        }
        for (uint32_t s = 0; s < UBS_TOPO_SOCKET_NUM; s++) {
            if (nodes[i].socket_id[s] == lender.socket_id) {
                lender.numa_id = nodes[i].numa_ids[s][0];
                numaFound = true;
                break;
            }
        }
        break;
    }
    free(nodes);
    return numaFound;
}
} // namespace

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

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 129ULL * 1024ULL * 1024ULL;

    const char* name = "it_p0_fd_lender01";

    ubs_mem_fd_desc_t fdDesc{};
    IT_LOG_INFO << "Creating FD with lender: name=" << name << ", size=" << lender.lender_size
                << ", lender slot=" << lenderSlotId;
    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    ASSERT_IT_OK(ret);

    // ====== 出参 fdDesc 字段校验 ======
    // 1. mem_stage ∈ {UBSE_CREATING, UBSE_EXIST}
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST)
        << "mem_stage should be CREATING or EXIST, actual: " << fdDesc.mem_stage;
    // 2. mem_size == lender_size
    EXPECT_EQ(fdDesc.mem_size, lender.lender_size) << "mem_size should equal input size";
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
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = fdSize;

    const char* name = "it_p0_fd_ldup01";

    ubs_mem_fd_desc_t fdDesc{};
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

    ret = sdk.MemFdDelete(name);
    ASSERT_IT_OK(ret);
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
    uint32_t slotIds[4]{};
    uint32_t slotCnt = 0;
    if (nodeIds.size() >= 4) {
        slotIds[slotCnt++] = cluster.GetNode("3").GetSpec().slotId;
        slotIds[slotCnt++] = cluster.GetNode("4").GetSpec().slotId;
    } else {
        slotIds[slotCnt++] = cluster.GetNode("2").GetSpec().slotId;
    }

    const char* name = "it_p0_fd_cand_dup01";
    ubs_mem_fd_desc_t fdDesc{};

    ASSERT_IT_OK(ubs_mem_fd_create_with_candidate(name, fdSize, nullptr, 0, slotIds, slotCnt, &fdDesc));

    ubs_mem_fd_desc_t dupDesc{};
    int32_t ret = ubs_mem_fd_create_with_candidate(name, fdSize, nullptr, 0, slotIds, slotCnt, &dupDesc);
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

// P0-FdGet-OverLen-01: name超长 (双节点)
void RunP0FdGetOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string overLenName(48, 'a');
    ubs_mem_fd_desc_t fdDesc{};

    IT_LOG_INFO << "Getting FD with over-length name: length=" << overLenName.length();
    int32_t ret = sdk.MemFdGet(overLenName.c_str(), &fdDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-FdGet-OverLen-01 done";
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
        EXPECT_IT_OK(sdk.MemFdDelete(names[i]));
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

// P0-FdMemidByImport-OverLen-01: name超长 (双节点)
void RunP0FdMemidByImportOverLen01(ubse::it::infra::ItCluster& cluster)
{
    std::string overLenName(48, 'a');
    ubs_mem_export_memid_t memInfo{};

    IT_LOG_INFO << "Querying export memid with over-length name: length=" << overLenName.length();
    int32_t ret = ubs_mem_fd_get_memid_by_import(overLenName.c_str(), 1, &memInfo);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-FdMemidByImport-OverLen-01 done";
}

// P0-FdMemidByImport-Ok-01: 正常查询 (双节点)
void RunP0FdMemidByImportOk01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 129ULL * 1024ULL * 1024ULL;

    const char* name = "it_p0_fd_memid_ok";
    ubs_mem_fd_desc_t fdDesc{};
    IT_LOG_INFO << "Creating FD with lender for memid query: name=" << name;
    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    ASSERT_IT_OK(ret);
    ASSERT_GT(fdDesc.memid_cnt, 0u) << "memid_cnt should be > 0";
    ASSERT_GT(fdDesc.memids[0], 0ull) << "memids[0] should be > 0";

    // 用 fdDesc.memids[0] 作为 import_memid 查询
    ubs_mem_export_memid_t memInfo{};
    IT_LOG_INFO << "Querying export memid: import_memid=" << fdDesc.memids[0];
    ret = ubs_mem_fd_get_memid_by_import(name, fdDesc.memids[0], &memInfo);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(memInfo.export_slot_id, lenderSlotId) << "export_slot_id should equal lender slot_id";
    EXPECT_GT(memInfo.export_memid, 0ull) << "export_memid should be > 0";

    // 清理
    EXPECT_IT_OK(sdk.MemFdDelete(name));
    IT_LOG_INFO << "P0-FdMemidByImport-Ok-01 done";
}

// P0-FdMemidByImport-NotExistMemId-01: 不存在的memId (双节点)
void RunP0FdMemidByImportNotExistMemId01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 129ULL * 1024ULL * 1024ULL;

    const char* name = "it_p0_fd_memid_nmid";
    ubs_mem_fd_desc_t fdDesc{};
    IT_LOG_INFO << "Creating FD with lender for not-exist-memid test: name=" << name;
    int32_t ret = ubs_mem_fd_create_with_lender(name, nullptr, 0, &lender, 1, &fdDesc);
    ASSERT_IT_OK(ret);

    // 使用不存在的import_memid查询
    constexpr uint64_t notExistMemId = 999999;
    ubs_mem_export_memid_t memInfo{};
    IT_LOG_INFO << "Querying export memid with non-existent memId: " << notExistMemId;
    ret = ubs_mem_fd_get_memid_by_import(name, notExistMemId, &memInfo);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "不存在的memId查询应返回失败";

    // 清理
    EXPECT_IT_OK(sdk.MemFdDelete(name));
    IT_LOG_INFO << "P0-FdMemidByImport-NotExistMemId-01 done";
}

// ==================== ubs_mem_fd_fault_register P0 测试 ====================

// P0-FdFaultReg-NullPtr-01: NULL handler (单节点)
void RunP0FdFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    IT_LOG_INFO << "Registering FD fault handler with NULL";
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
    EXPECT_EQ(ret, UBSE_ERR_NODE_NOT_EXIST) << "NumastatGet with non-existent slot_id should return NODE_NOT_EXISTS";

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
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;
    const char* name = "it_p0_numa_create_ok";
    constexpr uint64_t borrowSize = 129ULL * 1024ULL * 1024ULL; // 129MB

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, borrowSize, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 直接验证出参字段
    EXPECT_STREQ(numaDesc.name, name);
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);
    EXPECT_EQ(numaDesc.size, borrowSize);
    EXPECT_GE(numaDesc.numaid, 0);
    EXPECT_EQ(numaDesc.import_node.slot_id, localSlotId);
    EXPECT_GT(numaDesc.export_node.slot_id, 0u);
    EXPECT_NE(numaDesc.export_node.slot_id, numaDesc.import_node.slot_id);

    // 清理
    ret = sdk.MemNumaDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreate-Ok-01 passed";
}

// P0-NumaCreate-OverLen-01: name超长 (双节点)
void RunP0NumaCreateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    const char* longName = "it_p0_numa_overlen_name_aaaaaaaaaaaaaaaaaaaaaaaaaa";
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(longName, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";

    IT_LOG_INFO << "P0-NumaCreate-OverLen-01 passed: ret=" << ret;
}

// P0-NumaCreate-InvalidVal-01: size < 4MB (双节点)
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

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 同名重复创建
    ubs_mem_numa_desc_t dupDesc{};
    ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &dupDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_EXISTED) << "同名重复创建应返回EXISTED";

    // 清理
    ret = sdk.MemNumaDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreate-Dup-01 passed";
}

// P0-NumaCreate-NullPtr-01: 空指针 (双节点)
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
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;
    const char* name = "it_p0_numa_bound_min";

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 直接验证出参字段
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);
    EXPECT_EQ(numaDesc.size, static_cast<uint64_t>(UBS_MEM_MIN_SIZE));
    EXPECT_EQ(numaDesc.import_node.slot_id, localSlotId);

    // 清理
    ret = sdk.MemNumaDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreate-BoundMin-01 passed";
}

// P0-NumaCreate-BoundMax-01: name=47字节 (双节点)
void RunP0NumaCreateBoundMax01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;
    std::string boundName(47, 'x');

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(boundName.c_str(), UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 直接验证出参字段
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);
    EXPECT_STREQ(numaDesc.name, boundName.c_str());
    EXPECT_EQ(numaDesc.import_node.slot_id, localSlotId);

    // 清理
    ret = sdk.MemNumaDelete(boundName.c_str());
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreate-BoundMax-01 passed";
}

// ==================== ubs_mem_numa_create_with_lender ====================

// P0-NumaCreateLender-Ok-01: 指定借出节点 (双节点/四节点)
void RunP0NumaCreateLenderOk01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取借出节点的port_id/socket_id/numa_id
    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 129ULL * 1024ULL * 1024ULL;

    const char* name = "it_p0_numa_lender_ok";

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = ubs_mem_numa_create_with_lender(name, &lender, 1, &numaDesc);
    ASSERT_IT_OK(ret);

    // 直接验证出参字段
    EXPECT_STREQ(numaDesc.name, name);
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);
    EXPECT_EQ(numaDesc.size, lender.lender_size);
    EXPECT_GE(numaDesc.numaid, 0);
    EXPECT_EQ(numaDesc.export_node.slot_id, lenderSlotId);
    // 校验导出节点的socket_id与topo指定一致
    bool socketFound = false;
    for (uint32_t s = 0; s < UBS_TOPO_SOCKET_NUM; s++) {
        if (numaDesc.export_node.socket_id[s] == lender.socket_id) {
            socketFound = true;
            break;
        }
    }
    EXPECT_TRUE(socketFound) << "export_node.socket_id should contain " << lender.socket_id;
    EXPECT_EQ(numaDesc.import_node.slot_id, localSlotId);
    EXPECT_NE(numaDesc.export_node.slot_id, numaDesc.import_node.slot_id);

    // 清理
    ret = sdk.MemNumaDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreateLender-Ok-01 passed";
}

// P0-NumaCreateLender-OverLen-01: name超长 (双节点)
void RunP0NumaCreateLenderOverLen01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取借出节点的port_id/socket_id/numa_id
    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = fdSize;

    std::string overLenName(48, 'a');
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_lender(overLenName.c_str(), &lender, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG) << "name超长应返回INVALID_ARG";
    IT_LOG_INFO << "P0-NumaCreateLender-OverLen-01 passed";
}

// P0-NumaCreateLender-InvalidVal-01: lender_size < 4MB (双节点)
void RunP0NumaCreateLenderInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取合法的socket_id/numa_id/port_id
    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 1;
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_lender("it_p0_numa_lender_inv01", &lender, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "lender_size < 4MB应返回OUT_OF_RANGE";
    IT_LOG_INFO << "P0-NumaCreateLender-InvalidVal-01 passed";
}

// P0-NumaCreateLender-NullPtr-01: lender=NULL (双节点)
void RunP0NumaCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_lender("it_p0_numa_lender_null", nullptr, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER) << "lender=NULL with lender_cnt>0应返回NULL_POINTER";
    IT_LOG_INFO << "P0-NumaCreateLender-NullPtr-01 passed";
}

// P0-NumaCreateLender-NullPtr-02: name=NULL 或 numa_desc=NULL (双节点)
void RunP0NumaCreateLenderNullPtr02(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取合法的socket_id/numa_id/port_id
    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = fdSize;
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_lender(nullptr, &lender, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
    ret = ubs_mem_numa_create_with_lender("it_p0_numa_lender_null02", &lender, 1, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-NumaCreateLender-NullPtr-02 passed";
}

// P0-NumaCreateLender-BadParam-01: 不存在的slot_id (双节点)
void RunP0NumaCreateLenderBadParam01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取合法的socket_id/numa_id/port_id
    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = fdSize;
    lender.slot_id = 999;
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_lender("it_p0_numa_lender_bad01", &lender, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_LINK_NOT_EXIST) << "不存在的slot_id应返回UBS_ENGINE_ERR_LINK_NOT_EXIST";
    IT_LOG_INFO << "P0-NumaCreateLender-BadParam-01 passed";
}

// P0-NumaCreateLender-Dup-01: 同名重复 (双节点)
void RunP0NumaCreateLenderDup01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取借出节点的port_id/socket_id/numa_id
    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = fdSize;

    const char* name = "it_p0_numa_lender_dup";

    ubs_mem_numa_desc_t numaDesc{};
    ASSERT_IT_OK(ubs_mem_numa_create_with_lender(name, &lender, 1, &numaDesc));

    ubs_mem_numa_desc_t dupDesc{};
    int32_t ret = ubs_mem_numa_create_with_lender(name, &lender, 1, &dupDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_EXISTED);

    EXPECT_IT_OK(sdk.MemNumaDelete(name));
    IT_LOG_INFO << "P0-NumaCreateLender-Dup-01 passed";
}

// P0-NumaCreateLender-BoundMax-01: lender_cnt=4 (双节点)
void RunP0NumaCreateLenderBoundMax01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 通过topo链路获取借出节点的port_id/socket_id/numa_id
    ubs_mem_lender_t tmpl{};
    tmpl.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, tmpl))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;

    const char* name = "it_p0_numa_lender_max";
    ubs_mem_lender_t lenders[UBS_MEM_MAX_LENDER_CNT]{};
    for (uint32_t i = 0; i < UBS_MEM_MAX_LENDER_CNT; i++) {
        lenders[i].slot_id = lenderSlotId;
        lenders[i].socket_id = tmpl.socket_id;
        lenders[i].numa_id = tmpl.numa_id;
        lenders[i].lender_size = fdSize;
        lenders[i].port_id = tmpl.port_id;
    }

    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = ubs_mem_numa_create_with_lender(name, lenders, UBS_MEM_MAX_LENDER_CNT, &numaDesc);
    ASSERT_IT_OK(ret);

    // 直接验证出参字段
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);
    EXPECT_EQ(numaDesc.export_node.slot_id, lenderSlotId);
    EXPECT_EQ(numaDesc.import_node.slot_id, localSlotId);

    // 清理
    ret = sdk.MemNumaDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreateLender-BoundMax-01 passed";
}

// ==================== ubs_mem_numa_create_with_candidate ====================

// P0-NumaCreateCandidate-Ok-01: 指定候选节点 (双节点)
void RunP0NumaCreateCandidateOk01(ubse::it::infra::ItCluster& cluster)
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

    const char* name = "it_p0_numa_candidate_ok";
    constexpr uint64_t borrowSize = 129ULL * 1024ULL * 1024ULL;
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_candidate(name, borrowSize, slotIds, slotCnt, &numaDesc);
    ASSERT_IT_OK(ret);

    // 直接验证出参字段
    EXPECT_STREQ(numaDesc.name, name);
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);
    EXPECT_EQ(numaDesc.size, borrowSize);
    EXPECT_GE(numaDesc.numaid, 0);
    bool inSlotIds = false;
    for (uint32_t i = 0; i < slotCnt; i++) {
        if (numaDesc.export_node.slot_id == slotIds[i]) {
            inSlotIds = true;
            break;
        }
    }
    EXPECT_TRUE(inSlotIds);
    EXPECT_EQ(numaDesc.import_node.slot_id, localSlotId);
    EXPECT_NE(numaDesc.export_node.slot_id, numaDesc.import_node.slot_id);

    // 清理
    EXPECT_IT_OK(sdk.MemNumaDelete(name));
    IT_LOG_INFO << "P0-NumaCreateCandidate-Ok-01 passed";
}

// P0-NumaCreateCandidate-OverLen-01: name超长 (双节点)
void RunP0NumaCreateCandidateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    std::string overLenName(48, 'a');
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_candidate(overLenName.c_str(), fdSize, slotIds, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG) << "name超长应返回INVALID_ARG";
    IT_LOG_INFO << "P0-NumaCreateCandidate-OverLen-01 passed";
}

// P0-NumaCreateCandidate-InvalidVal-01: size < 4MB (双节点)
void RunP0NumaCreateCandidateInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_candidate("it_p0_numa_cand_inv01", 1, slotIds, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "size < 4MB应返回OUT_OF_RANGE";
    IT_LOG_INFO << "P0-NumaCreateCandidate-InvalidVal-01 passed";
}

// P0-NumaCreateCandidate-NullPtr-01: 空指针 (双节点)
void RunP0NumaCreateCandidateNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string candidateNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t slotIds[1] = {cluster.GetNode(candidateNodeId).GetSpec().slotId};

    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_candidate(nullptr, fdSize, slotIds, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
    ret = ubs_mem_numa_create_with_candidate("it_p0_numa_cand_null01", fdSize, slotIds, 1, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER);
    IT_LOG_INFO << "P0-NumaCreateCandidate-NullPtr-01 passed";
}

// P0-NumaCreateCandidate-BadParam-01: 不存在的slot_id (双节点)
void RunP0NumaCreateCandidateBadParam01(ubse::it::infra::ItCluster& cluster)
{
    uint32_t slotIds[1] = {999};
    ubs_mem_numa_desc_t numaDesc{};

    int32_t ret = ubs_mem_numa_create_with_candidate("it_p0_numa_cand_bad01", fdSize, slotIds, 1, &numaDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_ALLOCATE) << "不存在的slot_id应返回ALLOCATE";
    IT_LOG_INFO << "P0-NumaCreateCandidate-BadParam-01 passed";
}

// P0-NumaCreateCandidate-Dup-01: 同名重复 (双节点)
void RunP0NumaCreateCandidateDup01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    uint32_t slotIds[4]{};
    uint32_t slotCnt = 0;
    if (nodeIds.size() >= 4) {
        slotIds[slotCnt++] = cluster.GetNode("3").GetSpec().slotId;
        slotIds[slotCnt++] = cluster.GetNode("4").GetSpec().slotId;
    } else {
        slotIds[slotCnt++] = cluster.GetNode("2").GetSpec().slotId;
    }

    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_cand_dup";
    ubs_mem_numa_desc_t numaDesc{};

    ASSERT_IT_OK(ubs_mem_numa_create_with_candidate(name, fdSize, slotIds, slotCnt, &numaDesc));

    ubs_mem_numa_desc_t dupDesc{};
    int32_t ret = ubs_mem_numa_create_with_candidate(name, fdSize, slotIds, slotCnt, &dupDesc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_EXISTED);

    ret = sdk.MemNumaDelete(name);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "P0-NumaCreateCandidate-Dup-01 passed";
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

// P0-NumaGet-OverLen-01: name超长 (双节点)
void RunP0NumaGetOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string overLenName(48, 'a');
    ubs_mem_numa_desc_t numaDesc{};

    IT_LOG_INFO << "Getting NUMA with over-length name: length=" << overLenName.length();
    int32_t ret = sdk.MemNumaGet(overLenName.c_str(), &numaDesc);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-NumaGet-OverLen-01 done";
}

// ==================== ubs_mem_numa_list ====================

// P0-NumaList-Ok-01: 空时查询 + 创建后查询验证 (双节点)
void RunP0NumaListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    // 第一步：空时调用NumaList，验证接口可用
    ubs_mem_numa_desc_t* numaDescs = nullptr;
    uint32_t numaDescCnt = 0;
    int32_t ret = ubs_mem_numa_list(&numaDescs, &numaDescCnt);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "Empty NumaList ok, cnt=" << numaDescCnt;
    if (numaDescs != nullptr) {
        free(numaDescs);
        numaDescs = nullptr;
    }

    // 第二步：创建2个NUMA
    const char* names[] = {"it_p0_numalist_01", "it_p0_numalist_02"};
    constexpr uint64_t sizes[] = {fdSize, fdSize129M};
    ubs_mem_numa_desc_t createDescs[2]{};

    for (int i = 0; i < 2; i++) {
        IT_LOG_INFO << "Creating NUMA: name=" << names[i] << ", size=" << sizes[i];
        ret = sdk.MemNumaCreate(names[i], sizes[i], MEM_DISTANCE_L0, &createDescs[i]);
        ASSERT_IT_OK(ret);
    }

    // 第三步：再次调用NumaList
    ret = ubs_mem_numa_list(&numaDescs, &numaDescCnt);
    ASSERT_IT_OK(ret);
    ASSERT_GE(numaDescCnt, 2u);
    IT_LOG_INFO << "NumaList after create returned " << numaDescCnt << " entries";

    // 逐条校验字段
    for (uint32_t i = 0; i < numaDescCnt; i++) {
        IT_LOG_INFO << "NumaList[" << i << "]: name=" << numaDescs[i].name << ", mem_stage=" << numaDescs[i].mem_stage
                    << ", size=" << numaDescs[i].size << ", import_slot=" << numaDescs[i].import_node.slot_id
                    << ", export_slot=" << numaDescs[i].export_node.slot_id;

        EXPECT_TRUE(numaDescs[i].mem_stage == UBSE_CREATING || numaDescs[i].mem_stage == UBSE_EXIST);
        EXPECT_GT(numaDescs[i].size, 0ull);
        EXPECT_GE(numaDescs[i].numaid, 0);
        EXPECT_EQ(numaDescs[i].import_node.slot_id, localSlotId);
        EXPECT_GT(numaDescs[i].export_node.slot_id, 0u);
        EXPECT_NE(numaDescs[i].export_node.slot_id, numaDescs[i].import_node.slot_id);
    }

    // 验证创建的2个NUMA都在列表中且size一致
    for (int j = 0; j < 2; j++) {
        bool found = false;
        for (uint32_t i = 0; i < numaDescCnt; i++) {
            if (strcmp(numaDescs[i].name, names[j]) == 0) {
                found = true;
                EXPECT_EQ(numaDescs[i].size, sizes[j]);
                break;
            }
        }
        EXPECT_TRUE(found) << "NUMA " << names[j] << " not found in list";
    }

    if (numaDescs != nullptr) {
        free(numaDescs);
    }

    // 清理
    for (int i = 0; i < 2; i++) {
        EXPECT_IT_OK(sdk.MemNumaDelete(names[i]));
    }
    IT_LOG_INFO << "P0-NumaList-Ok-01 done";
}

// P0-NumaList-NullPtr-01: 空指针 (双节点)
void RunP0NumaListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    ubs_mem_numa_desc_t* numaDescs = nullptr;
    uint32_t numaDescCnt = 0;

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
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";

    IT_LOG_INFO << "P0-NumaDel-OverLen-01 passed";
}

// ==================== ubs_mem_numa_get_memid_by_import ====================

// P0-NumaMemidByImport-NotExist-01: name不存在 (双节点)
void RunP0NumaMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster)
{
    ubs_mem_export_memid_t memInfo{};

    int32_t ret = ubs_mem_numa_get_memid_by_import("it_p0_numa_memid_notexist", 0, &memInfo);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "name不存在应返回NOT_EXIST";

    IT_LOG_INFO << "P0-NumaMemidByImport-NotExist-01 passed";
}

// P0-NumaMemidByImport-OverLen-01: name超长 (双节点)
void RunP0NumaMemidByImportOverLen01(ubse::it::infra::ItCluster& cluster)
{
    std::string overLenName(48, 'a');
    ubs_mem_export_memid_t memInfo{};

    IT_LOG_INFO << "Querying NUMA export memid with over-length name: length=" << overLenName.length();
    int32_t ret = ubs_mem_numa_get_memid_by_import(overLenName.c_str(), 1, &memInfo);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-NumaMemidByImport-OverLen-01 done";
}

// P0-NumaMemidByImport-NotExistMemId-01: 不存在的memId (双节点)
void RunP0NumaMemidByImportNotExistMemId01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_numa_memid_nmid";
    ubs_mem_export_memid_t memInfo{};

    // 创建NUMA
    ubs_mem_numa_desc_t numaDesc{};
    int32_t ret = sdk.MemNumaCreate(name, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(ret);

    // 使用不存在的import_memid查询
    constexpr uint64_t notExistMemId = 999999;
    IT_LOG_INFO << "Querying NUMA export memid with non-existent memId: " << notExistMemId;
    ret = ubs_mem_numa_get_memid_by_import(name, notExistMemId, &memInfo);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "不存在的memId查询应返回失败";

    // 清理
    EXPECT_IT_OK(sdk.MemNumaDelete(name));
    IT_LOG_INFO << "P0-NumaMemidByImport-NotExistMemId-01 done";
}

// ==================== ubs_mem_numa_fault_register ====================

// P0-NumaFaultReg-NullPtr-01: NULL handler (单节点)
void RunP0NumaFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    int32_t ret = ubs_mem_numa_fault_register(nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);

    IT_LOG_INFO << "P0-NumaFaultReg-NullPtr-01 passed: ret=" << ret;
}

// ==================== SHM 函数（来自 mem_shm，排除 shm_fault_get） ====================

// ==================== ubs_mem_shm_create ====================

// 标准创建 (双节点/四节点，region参数化)
void RunP0ShmCreateOk01(ubse::it::infra::ItCluster& cluster, const std::vector<std::string>& regionNodeIds)
{
    // 使用region中第一个节点作为SDK调用节点
    auto& sdk = cluster.GetSdkClient(regionNodeIds[0]);
    const char* name = "it_p0_shm_create_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = static_cast<uint32_t>(regionNodeIds.size());
    for (size_t i = 0; i < regionNodeIds.size(); ++i) {
        region.slot_ids[i] = cluster.GetNode(regionNodeIds[i]).GetSpec().slotId;
    }

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)
    IT_LOG_INFO << "Creating SHM: name=" << name << ", size=" << shmSize128M;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    // 验证属性
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
            << "mem_stage should be CREATING or EXIST, actual: " << desc->mem_stage;
        EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size should equal input size";
        EXPECT_GT(desc->unit_size, static_cast<size_t>(0)) << "unit_size should be > 0";
        EXPECT_STREQ(desc->name, name);
        EXPECT_GT(desc->export_node.slot_id, 0u) << "export_node.slot_id should be > 0";
        auto inRegion = std::any_of(region.slot_ids, region.slot_ids + region.node_cnt,
                                    [&](uint32_t id) { return id == desc->export_node.slot_id; });
        EXPECT_TRUE(inRegion) << "export_node.slot_id=" << desc->export_node.slot_id << " should be in region nodes";
        free(desc);
    }

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmCreateOk01 done";
}

// 指定provider创建 (双节点/四节点，验证export_node在provider集合中)
// 指定provider创建 (双节点，region={1,2}, provider={2})
void RunP0ShmCreateWithProviderOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_create_provider";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    ubs_mem_nodes_t provider{};
    provider.node_cnt = 1;
    provider.slot_ids[0] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;
    IT_LOG_INFO << "Creating SHM with provider: name=" << name << ", size=" << shmSize128M;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, &provider);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    // 验证属性
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
            << "mem_stage should be CREATING or EXIST, actual: " << desc->mem_stage;
        EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size should equal input size";
        EXPECT_GT(desc->unit_size, static_cast<size_t>(0)) << "unit_size should be > 0";
        EXPECT_STREQ(desc->name, name);
        EXPECT_GT(desc->export_node.slot_id, 0u) << "export_node.slot_id should be > 0";
        // 导出节点必须在provider集合中
        auto inProvider = std::any_of(provider.slot_ids, provider.slot_ids + provider.node_cnt,
                                      [&](uint32_t id) { return id == desc->export_node.slot_id; });
        EXPECT_TRUE(inProvider) << "export_node.slot_id=" << desc->export_node.slot_id
                                << " should be in provider nodes";
        free(desc);
    }

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmCreateWithProviderOk01 done";
}

// name超长 (双节点)
void RunP0ShmCreateOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    // 48字符的name，超出UBS_MEM_MAX_NAME_LENGTH限制
    const char* name = "it_p0_shm_name_way_too_long_for_the_max_limit_x_";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    IT_LOG_INFO << "Creating SHM with overlen name: " << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize, usrInfo, 0, nullptr, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG);

    IT_LOG_INFO << "RunP0ShmCreateOverLen01 done";
}

// size < 4MB (双节点)
void RunP0ShmCreateInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_inv_val";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint64_t invalidSize = 1ULL * 1024ULL * 1024ULL; // 1MB，小于4MB

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
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

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

// size > 256GB (双节点)
void RunP0ShmCreateInvalidVal02(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_inval02";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint64_t tooLarge = 257ULL * 1024ULL * 1024ULL * 1024ULL;

    IT_LOG_INFO << "Creating SHM with size > 256GB: " << tooLarge;
    int32_t ret = sdk.MemShmCreate(name, tooLarge, usrInfo, 0, nullptr, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE) << "size > 256GB should return ALLOCATE";

    IT_LOG_INFO << "RunP0ShmCreateInvalidVal02 done";
}

// name=NULL (双节点)
void RunP0ShmCreateNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    IT_LOG_INFO << "Creating SHM with name=NULL";
    int32_t ret = sdk.MemShmCreate(nullptr, shmSize, usrInfo, 0, nullptr, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);

    IT_LOG_INFO << "RunP0ShmCreateNullPtr01 done";
}

// size=4MB 边界值 (双节点)
void RunP0ShmCreateBoundMin01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_bound_min";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    IT_LOG_INFO << "Creating SHM with boundary min size: " << shmSize;
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
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST);
        EXPECT_EQ(desc->mem_size, shmSize) << "mem_size should equal 4MB";
        EXPECT_GT(desc->unit_size, static_cast<size_t>(0));
        free(desc);
    }

    // 清理
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmCreateBoundMin01 done";
}

// name=47字节 (双节点)
void RunP0ShmCreateBoundMax01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string boundName(47, 'x');
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;
    IT_LOG_INFO << "Creating SHM with boundary max name length: " << boundName.length();
    int32_t ret = sdk.MemShmCreate(boundName.c_str(), shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    ret = WaitForShmReady(sdk, boundName.c_str());
    EXPECT_IT_OK(ret);

    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(boundName.c_str(), &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST);
        EXPECT_STREQ(desc->name, boundName.c_str()) << "name should be fully preserved";
        EXPECT_EQ(desc->mem_size, shmSize128M);
        EXPECT_GT(desc->unit_size, static_cast<size_t>(0));
        free(desc);
    }

    ret = sdk.MemShmDelete(boundName.c_str());
    EXPECT_IT_OK(ret);
    IT_LOG_INFO << "RunP0ShmCreateBoundMax01 done";
}

// ==================== ubs_mem_shm_create_with_affinity ====================

// P0-ShmCreateAffinity-Ok-01: 标准创建 (双节点)
void RunP0ShmCreateAffinityOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_affinity_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    // 从拓扑获取请求节点的合法socket_id
    ubs_topo_node_t localNode{};
    int32_t ret = sdk.TopoNodeLocalGet(&localNode);
    ASSERT_IT_OK(ret);
    ASSERT_GT(localNode.socket_id[0], 0u) << "socket_id[0] should be > 0";
    uint32_t affinitySocketId = localNode.socket_id[0];

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;
    IT_LOG_INFO << "Creating SHM with affinity: name=" << name
                << ", size=128MB, affinity_socket_id=" << affinitySocketId;
    ret = ubs_mem_shm_create_with_affinity(name, shmSize128M, affinitySocketId, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 等待就绪
    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    // 验证属性
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
            << "mem_stage should be CREATING or EXIST, actual: " << desc->mem_stage;
        EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size should equal input size";
        EXPECT_GT(desc->unit_size, static_cast<size_t>(0)) << "unit_size should be > 0";
        EXPECT_STREQ(desc->name, name);
        EXPECT_GT(desc->export_node.slot_id, 0u) << "export_node.slot_id should be > 0";
        auto inRegion = std::any_of(region.slot_ids, region.slot_ids + region.node_cnt,
                                    [&](uint32_t id) { return id == desc->export_node.slot_id; });
        EXPECT_TRUE(inRegion) << "export_node.slot_id=" << desc->export_node.slot_id << " should be in region nodes";
        free(desc);
    }

    // 清理
    EXPECT_IT_OK(sdk.MemShmDelete(name));
    IT_LOG_INFO << "P0-ShmCreateAffinity-Ok-01 done";
}

// P0-ShmCreateAffinity-OverLen-01: name超长 (双节点)
void RunP0ShmCreateAffinityOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string overLenName(48, 'a');
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    ubs_topo_node_t localNode{};
    int32_t ret = sdk.TopoNodeLocalGet(&localNode);
    ASSERT_IT_OK(ret);

    IT_LOG_INFO << "Creating SHM with affinity over-length name: length=" << overLenName.length();
    ret = ubs_mem_shm_create_with_affinity(overLenName.c_str(), shmSize, localNode.socket_id[0], usrInfo, 0, &region,
                                           nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-ShmCreateAffinity-OverLen-01 done";
}

// P0-ShmCreateAffinity-InvalidVal-01: size < 4MB (双节点)
void RunP0ShmCreateAffinityInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_affinity_inv";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint64_t size1M = 1ULL * 1024ULL * 1024ULL;

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    ubs_topo_node_t localNode{};
    int32_t ret = sdk.TopoNodeLocalGet(&localNode);
    ASSERT_IT_OK(ret);

    IT_LOG_INFO << "Creating SHM with affinity invalid size: size=1MB";
    ret = ubs_mem_shm_create_with_affinity(name, size1M, localNode.socket_id[0], usrInfo, 0, &region, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "size<4MB应返回UBS_ENGINE_ERR_OUT_OF_RANGE";
    IT_LOG_INFO << "P0-ShmCreateAffinity-InvalidVal-01 done";
}

// P0-ShmCreateAffinity-NullPtr-01: 空指针 (双节点)
void RunP0ShmCreateAffinityNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    ubs_topo_node_t localNode{};
    int32_t ret = sdk.TopoNodeLocalGet(&localNode);
    ASSERT_IT_OK(ret);

    IT_LOG_INFO << "Creating SHM with affinity NULL name";
    ret = ubs_mem_shm_create_with_affinity(nullptr, shmSize, localNode.socket_id[0], usrInfo, 0, &region, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER) << "name=NULL应返回UBS_ERR_NULL_POINTER";
    IT_LOG_INFO << "P0-ShmCreateAffinity-NullPtr-01 done";
}

// P0-ShmCreateAffinity-BadParam-01: 不存在的socket_id (双节点)
void RunP0ShmCreateAffinityBadParam01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_shm_affinity_bp";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    constexpr uint32_t badSocketId = 9999;

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    IT_LOG_INFO << "Creating SHM with affinity bad param: socket_id=" << badSocketId;
    int32_t ret = ubs_mem_shm_create_with_affinity(name, shmSize, badSocketId, usrInfo, 0, &region, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL);
    IT_LOG_INFO << "P0-ShmCreateAffinity-BadParam-01 done";
}

// P0-ShmCreateAffinity-Dup-01: 同名重复 (双节点)
void RunP0ShmCreateAffinityDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_affinity_dup";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    ubs_topo_node_t localNode{};
    int32_t ret = sdk.TopoNodeLocalGet(&localNode);
    ASSERT_IT_OK(ret);

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;
    IT_LOG_INFO << "Creating SHM with affinity (first): name=" << name;
    ret = ubs_mem_shm_create_with_affinity(name, shmSize128M, localNode.socket_id[0], usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 同名再调一次
    IT_LOG_INFO << "Creating SHM with affinity (duplicate): name=" << name;
    ret = ubs_mem_shm_create_with_affinity(name, shmSize128M, localNode.socket_id[0], usrInfo, 0, &region, nullptr);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED) << "同名重复应返回UBS_ENGINE_ERR_EXISTED";

    // 清理
    EXPECT_IT_OK(sdk.MemShmDelete(name));
    IT_LOG_INFO << "P0-ShmCreateAffinity-Dup-01 done";
}

// ==================== ubs_mem_shm_create_with_lender ====================

// P0-ShmCreateLender-Ok-01: 指定借出节点 (双/四节点)
void RunP0ShmCreateLenderOk01(ubse::it::infra::ItCluster& cluster, const std::vector<std::string>& regionNodeIds)
{
    const char* lenderNodeId = "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 128ULL * 1024ULL * 1024ULL;

    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = static_cast<uint32_t>(regionNodeIds.size());
    for (size_t i = 0; i < regionNodeIds.size() && i < 16; i++) {
        region.slot_ids[i] = cluster.GetNode(regionNodeIds[i]).GetSpec().slotId;
    }

    const char* name = "it_p0_shm_lender_ok";
    IT_LOG_INFO << "Creating SHM with lender: name=" << name << ", lender_slot=" << lenderSlotId;
    int32_t ret = ubs_mem_shm_create_with_lender(name, usrInfo, 0, &region, &lender);
    ASSERT_IT_OK(ret);

    ret = WaitForShmReady(sdk, name);
    EXPECT_IT_OK(ret);

    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST);
        EXPECT_EQ(desc->mem_size, lender.lender_size);
        EXPECT_GT(desc->unit_size, static_cast<size_t>(0));
        EXPECT_EQ(desc->export_node.slot_id, lenderSlotId);
        EXPECT_GT(desc->export_node.slot_id, 0u);
        free(desc);
    }

    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);
    IT_LOG_INFO << "P0-ShmCreateLender-Ok-01 passed";
}

// P0-ShmCreateLender-OverLen-01: name超长 (双节点)
void RunP0ShmCreateLenderOverLen01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = shmSize;

    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    std::string overLenName(48, 'a');

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    int32_t ret = ubs_mem_shm_create_with_lender(overLenName.c_str(), usrInfo, 0, &region, &lender);
    EXPECT_EQ(ret, UBS_ERR_INVALID_ARG) << "name超长应返回INVALID_ARG";
    IT_LOG_INFO << "P0-ShmCreateLender-OverLen-01 passed";
}

// P0-ShmCreateLender-InvalidVal-01: lender_size < 4MB (双节点)
void RunP0ShmCreateLenderInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = 1; // < 4MB

    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    int32_t ret = ubs_mem_shm_create_with_lender("it_p0_shm_lender_inval", usrInfo, 0, &region, &lender);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "lender_size<4MB应返回OUT_OF_RANGE";
    IT_LOG_INFO << "P0-ShmCreateLender-InvalidVal-01 passed";
}

// P0-ShmCreateLender-NullPtr-01: name=NULL (双节点)
void RunP0ShmCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    IT_LOG_INFO << "Creating SHM with lender: name=NULL";
    int32_t ret = ubs_mem_shm_create_with_lender(nullptr, usrInfo, 0, nullptr, nullptr);
    EXPECT_EQ(ret, UBS_ERR_NULL_POINTER) << "name=NULL应返回NULL_POINTER";
    IT_LOG_INFO << "P0-ShmCreateLender-NullPtr-01 passed";
}

// P0-ShmCreateLender-BadParam-01: 不存在的slot_id (双节点)
void RunP0ShmCreateLenderBadParam01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = shmSize;
    lender.slot_id = 999;

    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    int32_t ret = ubs_mem_shm_create_with_lender("it_p0_shm_lender_bad01", usrInfo, 0, &region, &lender);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_ALLOCATE) << "不存在的slot_id应返回ALLOCATE";
    IT_LOG_INFO << "P0-ShmCreateLender-BadParam-01 passed";
}

// P0-ShmCreateLender-Dup-01: 同名重复 (双节点)
void RunP0ShmCreateLenderDup01(ubse::it::infra::ItCluster& cluster)
{
    const auto& nodeIds = cluster.GetNodeIds();
    std::string lenderNodeId = (nodeIds.size() >= 4) ? "3" : "2";
    uint32_t lenderSlotId = cluster.GetNode(lenderNodeId).GetSpec().slotId;

    auto& sdk = cluster.GetSdkClient("1");
    uint32_t localSlotId = cluster.GetNode("1").GetSpec().slotId;

    ubs_mem_lender_t lender{};
    lender.slot_id = lenderSlotId;
    ASSERT_TRUE(FillLenderTopoInfo(sdk, localSlotId, lender))
        << "No topo link from " << localSlotId << " to " << lenderSlotId;
    lender.lender_size = shmSize;

    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    const char* name = "it_p0_shm_lender_dup";
    IT_LOG_INFO << "Creating SHM with lender first time: name=" << name;
    int32_t ret = ubs_mem_shm_create_with_lender(name, usrInfo, 0, &region, &lender);
    ASSERT_IT_OK(ret);

    int32_t dupRet = ubs_mem_shm_create_with_lender(name, usrInfo, 0, &region, &lender);
    EXPECT_EQ(dupRet, UBS_ENGINE_ERR_EXISTED);

    EXPECT_IT_OK(sdk.MemShmDelete(name));
    IT_LOG_INFO << "P0-ShmCreateLender-Dup-01 passed";
}

// ==================== ubs_mem_shm_attach ====================

// attach后校验内存块数 (双节点, 256MB)
void RunP0ShmAttachOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_attach_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize256M = 256ULL * 1024ULL * 1024ULL;

    // 创建 SHM
    IT_LOG_INFO << "Creating SHM: name=" << name << ", size=256MB";
    int32_t ret = sdk.MemShmCreate(name, shmSize256M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    ret = WaitForShmReady(sdk, name);
    ASSERT_IT_OK(ret);

    // attach
    ubs_mem_shm_desc_t* desc = nullptr;
    IT_LOG_INFO << "Attaching SHM: name=" << name;
    ret = sdk.MemShmAttach(name, nullptr, 0, &desc);
    ASSERT_IT_OK(ret);
    ASSERT_NE(desc, nullptr);

    // 校验
    EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST);
    EXPECT_EQ(desc->mem_size, shmSize256M) << "mem_size should be 256MB";
    EXPECT_GT(desc->unit_size, static_cast<size_t>(0));
    EXPECT_GE(desc->import_desc_cnt, 1u) << "import_desc_cnt should >= 1 after attach";
    if (desc->import_desc_cnt > 0) {
        uint32_t expectedMemidCnt = static_cast<uint32_t>((desc->mem_size + desc->unit_size - 1) / desc->unit_size);
        EXPECT_EQ(desc->import_desc[0].memid_cnt, expectedMemidCnt)
            << "import_desc[0].memid_cnt should equal ceil(256MB/unit_size)";
        EXPECT_GT(desc->import_desc[0].memid_cnt, 0u);
        EXPECT_GT(desc->import_desc[0].import_node.slot_id, 0u);
    }
    free(desc);

    // 清理
    ret = sdk.MemShmDetach(name);
    EXPECT_IT_OK(ret);
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    IT_LOG_INFO << "RunP0ShmAttachOk01 done";
}

// 未创建时attach (双节点)
void RunP0ShmAttachNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_not_exist_attach";
    ubs_mem_shm_desc_t* desc = nullptr;

    IT_LOG_INFO << "Attaching SHM that does not exist: " << name;
    int32_t ret = sdk.MemShmAttach(name, nullptr, 0, &desc);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "未创建attach应返回UBS_ENGINE_ERR_NOT_EXIST";
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
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

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

    // 第二次attach
    ubs_mem_shm_desc_t* desc2 = nullptr;
    IT_LOG_INFO << "Attaching SHM second time";
    ret = sdk.MemShmAttach(name, nullptr, 0, &desc2);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_EXISTED) << "重复attach应返回EXISTED";
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

// P0-ShmGet-NullPtr-01: 空指针 (双节点)
void RunP0ShmGetNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_get_null";

    IT_LOG_INFO << "Getting SHM with null shm_desc pointer";
    int32_t ret = sdk.MemShmGet(name, nullptr);
    EXPECT_IT_ERROR(ret, UBSE_ERR_NOT_EXIST);
    IT_LOG_INFO << "P0-ShmGet-NullPtr-01 done";
}

// P0-ShmGet-OverLen-01: name超长 (双节点)
void RunP0ShmGetOverLen01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string overLenName(48, 'a');
    ubs_mem_shm_desc_t* desc = nullptr;

    IT_LOG_INFO << "Getting SHM with over-length name: length=" << overLenName.length();
    int32_t ret = sdk.MemShmGet(overLenName.c_str(), &desc);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    if (desc != nullptr) {
        free(desc);
    }
    IT_LOG_INFO << "P0-ShmGet-OverLen-01 done";
}

// ==================== ubs_mem_shm_list ====================

// list + 前缀过滤 (双节点)
void RunP0ShmListOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    // 创建2个不同prefix的SHM
    const char* names[] = {"it_p0_aaa_shm1", "it_p0_bbb_shm2"};
    constexpr uint64_t sizes[] = {128ULL * 1024ULL * 1024ULL, 256ULL * 1024ULL * 1024ULL};
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    for (int i = 0; i < 2; i++) {
        IT_LOG_INFO << "Creating SHM: name=" << names[i] << ", size=" << sizes[i];
        int32_t ret = sdk.MemShmCreate(names[i], sizes[i], usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);
    }

    // === ubs_mem_shm_list ===
    ubs_mem_shm_desc_t* descs = nullptr;
    uint32_t cnt = 0;
    IT_LOG_INFO << "Listing SHM after create";
    int32_t ret = sdk.MemShmList(&descs, &cnt);
    ASSERT_IT_OK(ret);
    ASSERT_GE(cnt, 2u);
    IT_LOG_INFO << "ShmList returned " << cnt << " entries";

    // 逐条校验字段
    for (uint32_t i = 0; i < cnt; i++) {
        IT_LOG_INFO << "ShmList[" << i << "]: name=" << descs[i].name << ", mem_stage=" << descs[i].mem_stage
                    << ", mem_size=" << descs[i].mem_size << ", export_slot=" << descs[i].export_node.slot_id;
        EXPECT_TRUE(descs[i].mem_stage == UBSE_CREATING || descs[i].mem_stage == UBSE_EXIST);
        EXPECT_GT(descs[i].mem_size, 0ull);
        EXPECT_GT(descs[i].unit_size, static_cast<size_t>(0));
        EXPECT_GT(descs[i].export_node.slot_id, 0u);
        auto inRegion = std::any_of(region.slot_ids, region.slot_ids + region.node_cnt,
                                    [&](uint32_t id) { return id == descs[i].export_node.slot_id; });
        EXPECT_TRUE(inRegion) << "export_node.slot_id=" << descs[i].export_node.slot_id << " should be in region";
    }

    // 验证创建的2个SHM都在列表且mem_size一致
    for (int j = 0; j < 2; j++) {
        bool found = false;
        for (uint32_t i = 0; i < cnt; i++) {
            if (strcmp(descs[i].name, names[j]) == 0) {
                found = true;
                EXPECT_EQ(descs[i].mem_size, sizes[j]);
                break;
            }
        }
        EXPECT_TRUE(found) << "SHM " << names[j] << " not found in list";
    }
    free(descs);
    descs = nullptr;

    // === ubs_mem_shm_list_with_prefix("aaa_") ===
    cnt = 0;
    ret = ubs_mem_shm_list_with_prefix("it_p0_aaa_", &descs, &cnt);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(cnt, 1u) << "prefix 'it_p0_aaa_' should match exactly 1 SHM";
    if (cnt > 0) {
        EXPECT_EQ(descs[0].mem_size, sizes[0]);
        EXPECT_TRUE(strncmp(descs[0].name, "it_p0_aaa_", 10) == 0);
    }
    if (descs != nullptr) {
        free(descs);
        descs = nullptr;
    }

    // list_with_prefix("zzz_") 应返回0条
    cnt = 1;
    ret = ubs_mem_shm_list_with_prefix("zzz_", &descs, &cnt);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(cnt, 0u) << "prefix 'zzz_' should match 0 SHM";
    if (descs != nullptr) {
        free(descs);
    }

    // 清理
    for (int i = 0; i < 2; i++) {
        EXPECT_IT_OK(sdk.MemShmDelete(names[i]));
    }
    IT_LOG_INFO << "P0-ShmList-Ok-01 done";
}

// 空指针 (双节点)
void RunP0ShmListNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    ubs_mem_shm_desc_t* descs = nullptr;
    uint32_t cnt = 0;

    // ubs_mem_shm_list
    EXPECT_EQ(ubs_mem_shm_list(nullptr, &cnt), UBS_ERR_NULL_POINTER)
        << "shm_list: descs=null should return NULL_POINTER";
    EXPECT_EQ(ubs_mem_shm_list(&descs, nullptr), UBS_ERR_NULL_POINTER)
        << "shm_list: shm_cnt=null should return NULL_POINTER";

    // ubs_mem_shm_list_with_prefix
    EXPECT_EQ(ubs_mem_shm_list_with_prefix("test", nullptr, &cnt), UBS_ERR_NULL_POINTER)
        << "shm_list_with_prefix: descs=null should return NULL_POINTER";
    EXPECT_EQ(ubs_mem_shm_list_with_prefix("test", &descs, nullptr), UBS_ERR_NULL_POINTER)
        << "shm_list_with_prefix: shm_cnt=null should return NULL_POINTER";

    IT_LOG_INFO << "P0-ShmList-NullPtr-01 passed";
}

// P0-ShmList-OverLen-01: prefix超长 (双节点)
void RunP0ShmListOverLen01(ubse::it::infra::ItCluster& cluster)
{
    std::string overLenPrefix(48, 'a');
    ubs_mem_shm_desc_t* descs = nullptr;
    uint32_t cnt = 0;

    IT_LOG_INFO << "Listing SHM with over-length prefix: length=" << overLenPrefix.length();
    int32_t ret = ubs_mem_shm_list_with_prefix(overLenPrefix.c_str(), &descs, &cnt);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "prefix超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-ShmList-OverLen-01 done";
}

// ==================== ubs_mem_shm_detach ====================

// attach后detach (双节点)
void RunP0ShmDetachOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_detach_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;

    // create
    IT_LOG_INFO << "Creating SHM: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    ret = WaitForShmReady(sdk, name);
    ASSERT_IT_OK(ret);

    // attach
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmAttach(name, nullptr, 0, &desc);
    ASSERT_IT_OK(ret);
    uint32_t importCntBefore = 0;
    if (desc != nullptr) {
        importCntBefore = desc->import_desc_cnt;
        free(desc);
    }

    // detach
    IT_LOG_INFO << "Detaching SHM: name=" << name;
    ret = sdk.MemShmDetach(name);
    EXPECT_IT_OK(ret) << "detach after attach should succeed";

    // 验证 import_desc_cnt 变化
    desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    if (ret == UBS_SUCCESS && desc != nullptr) {
        EXPECT_LT(desc->import_desc_cnt, importCntBefore) << "import_desc_cnt should decrease after detach";
        free(desc);
    }

    // 清理
    EXPECT_IT_OK(sdk.MemShmDelete(name));
    IT_LOG_INFO << "P0-ShmDetach-Ok-01 done";
}

// 未attach时detach (双节点)
void RunP0ShmDetachNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_detach_not_ready";

    IT_LOG_INFO << "Detaching SHM that is not attached: " << name;
    int32_t ret = sdk.MemShmDetach(name);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_SHM_NO_ATTACH) << "未attach时detach应返回SHM_NO_ATTACH";

    IT_LOG_INFO << "P0-ShmDetach-NotReady-01 done";
}

// ==================== ubs_mem_shm_delete ====================

// 创建后删除 (双节点)
void RunP0ShmDelOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_del_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;

    // create
    IT_LOG_INFO << "Creating SHM: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // delete
    IT_LOG_INFO << "Deleting SHM: name=" << name;
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret) << "delete after create should succeed";

    // 验证已不存在
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = sdk.MemShmGet(name, &desc);
    EXPECT_NE(ret, UBS_SUCCESS) << "SHM should not exist after delete";
    if (desc != nullptr) {
        free(desc);
    }

    IT_LOG_INFO << "P0-ShmDel-Ok-01 done";
}

// 删除不存在 (双节点)
void RunP0ShmDelNotExist01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_del_not_exist";

    IT_LOG_INFO << "Deleting SHM that does not exist: " << name;
    int32_t ret = sdk.MemShmDelete(name);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "删除不存在的SHM应返回NOT_EXIST";

    IT_LOG_INFO << "P0-ShmDel-NotExist-01 done";
}

// P0-ShmDel-Dup-01: 重复删除 (双节点)
void RunP0ShmDelDup01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_del_dup";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;

    // create
    IT_LOG_INFO << "Creating SHM for duplicate delete test: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // 第一次删除
    IT_LOG_INFO << "Deleting SHM first time: name=" << name;
    ret = sdk.MemShmDelete(name);
    EXPECT_IT_OK(ret);

    // 第二次删除（重复）
    IT_LOG_INFO << "Deleting SHM second time (duplicate): name=" << name;
    ret = sdk.MemShmDelete(name);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "重复删除应返回NOT_EXIST";

    IT_LOG_INFO << "P0-ShmDel-Dup-01 done";
}

// P0-ShmDel-OverLen-01: name超长 (双节点)
void RunP0ShmDelOverLen01(ubse::it::infra::ItCluster& cluster)
{
    std::string overLenName(48, 'a');

    IT_LOG_INFO << "Deleting SHM with over-length name";
    int32_t ret = ubs_mem_shm_delete(overLenName.c_str());
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-ShmDel-OverLen-01 done";
}

// ==================== ubs_mem_shm_fault_register ====================

// NULL handler (双节点)
void RunP0ShmFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster)
{
    IT_LOG_INFO << "Registering SHM fault with NULL handler";
    int32_t ret = ubs_mem_shm_fault_register(nullptr);
    EXPECT_IT_ERROR(ret, UBS_ERR_NULL_POINTER);

    IT_LOG_INFO << "P0-ShmFaultReg-NullPtr-01 done";
}

// ==================== ubs_mem_shm_get_memid_by_import ====================

// P0-ShmMemidByImport-Ok-01: 正常查询 (双节点)
void RunP0ShmMemidByImportOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_memid_ok";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;

    // create + attach
    IT_LOG_INFO << "Creating SHM for memid query: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    ret = WaitForShmReady(sdk, name);
    ASSERT_IT_OK(ret);

    ubs_mem_shm_desc_t* shmDesc = nullptr;
    ret = sdk.MemShmAttach(name, nullptr, 0, &shmDesc);
    ASSERT_IT_OK(ret);
    ASSERT_NE(shmDesc, nullptr);
    ASSERT_GE(shmDesc->import_desc_cnt, 1u) << "import_desc_cnt should >= 1 after attach";
    ASSERT_GT(shmDesc->import_desc[0].memid_cnt, 0u) << "import_desc[0].memid_cnt should > 0";
    ASSERT_GT(shmDesc->import_desc[0].memids[0], 0ull) << "import_desc[0].memids[0] should > 0";

    // 用 import_desc[0].memids[0] 作为 import_memid 查询
    uint64_t importMemId = shmDesc->import_desc[0].memids[0];
    ubs_mem_export_memid_t memInfo{};
    IT_LOG_INFO << "Querying SHM export memid: import_memid=" << importMemId;
    ret = ubs_mem_shm_get_memid_by_import(name, importMemId, &memInfo);
    EXPECT_IT_OK(ret);
    EXPECT_GT(memInfo.export_slot_id, 0u) << "export_slot_id should be > 0";
    EXPECT_GT(memInfo.export_memid, 0ull) << "export_memid should be > 0";

    if (shmDesc != nullptr) {
        free(shmDesc);
    }

    // 清理
    EXPECT_IT_OK(sdk.MemShmDetach(name));
    EXPECT_IT_OK(sdk.MemShmDelete(name));
    IT_LOG_INFO << "P0-ShmMemidByImport-Ok-01 done";
}

// name不存在 (单节点)
void RunP0ShmMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster)
{
    const char* name = "it_p0_shm_memid_not_exist";
    ubs_mem_export_memid_t memInfo{};

    IT_LOG_INFO << "Getting memid by import for non-existent SHM: " << name;
    // 直接调用C API
    int32_t ret = ubs_mem_shm_get_memid_by_import(name, 1, &memInfo);
    EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);

    IT_LOG_INFO << "RunP0ShmMemidByImportNotExist01 done";
}

// P0-ShmMemidByImport-OverLen-01: name超长 (双节点)
void RunP0ShmMemidByImportOverLen01(ubse::it::infra::ItCluster& cluster)
{
    std::string overLenName(48, 'a');
    ubs_mem_export_memid_t memInfo{};

    IT_LOG_INFO << "Querying SHM export memid with over-length name: length=" << overLenName.length();
    int32_t ret = ubs_mem_shm_get_memid_by_import(overLenName.c_str(), 1, &memInfo);
    EXPECT_IT_ERROR(ret, UBS_ERR_INVALID_ARG) << "name超长应返回UBS_ERR_INVALID_ARG";
    IT_LOG_INFO << "P0-ShmMemidByImport-OverLen-01 done";
}

// P0-ShmMemidByImport-NotExistMemId-01: attach后不存在的memId (双节点)
void RunP0ShmMemidByImportNotExistMemId01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p0_shm_memid_nmid";
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;

    // create + attach
    IT_LOG_INFO << "Creating SHM for not-exist-memid test: name=" << name;
    int32_t ret = sdk.MemShmCreate(name, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    ubs_mem_shm_desc_t* shmDesc = nullptr;
    ret = sdk.MemShmAttach(name, nullptr, 0, &shmDesc);
    ASSERT_IT_OK(ret);

    // 使用不存在的import_memid查询
    constexpr uint64_t notExistMemId = 999999;
    ubs_mem_export_memid_t memInfo{};
    IT_LOG_INFO << "Querying SHM export memid with non-existent memId after attach: " << notExistMemId;
    ret = ubs_mem_shm_get_memid_by_import(name, notExistMemId, &memInfo);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_NOT_EXIST) << "不存在的memId查询应返回失败";
    if (shmDesc != nullptr) {
        free(shmDesc);
    }

    // 清理
    EXPECT_IT_OK(sdk.MemShmDetach(name));
    EXPECT_IT_OK(sdk.MemShmDelete(name));
    IT_LOG_INFO << "P0-ShmMemidByImport-NotExistMemId-01 done";
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
