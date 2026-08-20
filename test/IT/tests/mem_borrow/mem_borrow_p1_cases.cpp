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

#include "mem_borrow_p1_cases.h"
#include "mem_borrow_internal.h"

#include <gtest/gtest.h>

#include <pthread.h>
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
    int32_t numaId = std::stoi(createInfo.numaId);
    EXPECT_GE(numaId, 0) << "numa-id should be >= 0";
    EXPECT_EQ(createInfo.importNode, "1") << "import-node should be current node (1)";
    EXPECT_FALSE(createInfo.exportNode.empty()) << "export-node should not be empty";
    EXPECT_NE(createInfo.exportNode, "1") << "export-node should NOT be current node";

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

// P1-FdBorrow-MultiNode-Ok-01(四节点): 多节点 FD 借用用例
void RunP1FdBorrowMultiNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_fd_borrow_multi_node_ok_01";

    // 两个线程同时提交创建FD请求
    std::thread thread1([&]() {
        ubs_mem_fd_desc_t fdDesc{};
        ASSERT_IT_OK(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc))
            << "node 1 create " << name << " failed";
    });

    auto& sdk2 = cluster.GetSdkClient("3");
    const char* name2 = "it_p1_fd_borrow_multi_node_ok_01_2";
    std::thread thread2([&]() {
        ubs_mem_fd_desc_t fdDesc2{};
        ASSERT_IT_OK(sdk2.MemFdCreate(name2, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc2))
            << "node 3 create " << name2 << " failed";
    });

    thread1.join();
    thread2.join();
    // 清理
    ASSERT_IT_OK(sdk.MemFdDelete(name)) << "node 1 delete " << name << " failed";
    ASSERT_IT_OK(sdk2.MemFdDelete(name2)) << "node 3 delete " << name2 << " failed";
}

// P1-FdBorrow-MultiTime-Ok-01: FD 多次借用用例
void RunP1FdBorrowMultiTimeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_fd_borrow_multi_time_ok_01";
    ubs_mem_fd_desc_t fdDesc{};
    ASSERT_IT_OK(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc))
        << "node 1 create " << name << " failed";

    const char* name2 = "it_p1_fd_borrow_multi_time_ok_01_2";
    ubs_mem_fd_desc_t fdDesc2{};
    ASSERT_IT_OK(sdk.MemFdCreate(name2, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc2))
        << "node 1 borrow " << name2 << " failed";

    // 清理
    ASSERT_IT_OK(sdk.MemFdDelete(name)) << "node 1 delete " << name << " failed";
    ASSERT_IT_OK(sdk.MemFdDelete(name2)) << "node 1 delete " << name2 << " failed";

    // 重新创建，再归还
    ubs_mem_fd_desc_t fdDesc3{};
    ASSERT_IT_OK(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc3))
        << "node 1 create " << name << " failed";

    ASSERT_IT_OK(sdk.MemFdDelete(name)) << "node 1 delete " << name << " failed";
    IT_LOG_INFO << "P1-FdBorrow-MultiTime-Ok-01 passed";
}

// P1-FdCreate-DiffNode-Ok-01: fd创建用例，不同节点创建同名FD
void RunP1FdCreateDiffNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_fd_create_diff_node_ok_01";
    ubs_mem_fd_desc_t fdDesc{};
    ASSERT_IT_OK(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc))
        << "node 1 create " << name << " failed";

    // 清理
    ASSERT_IT_OK(sdk.MemFdDelete(name)) << "node 1 delete " << name << " failed";

    auto& sdk2 = cluster.GetSdkClient("2");
    ubs_mem_fd_desc_t fdDesc2{};
    ASSERT_IT_OK(sdk2.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc2))
        << "node 2 create " << name << " failed";

    // 清理
    ASSERT_IT_OK(sdk2.MemFdDelete(name)) << "node 2 delete " << name << " failed";
    IT_LOG_INFO << "P1-FdCreate-DiffNode-Ok-01 passed";
}

// P1-FdGet-DiffNode-Ok-01: fd获取用例，不同节点获取同名FD
void RunP1FdGetDiffNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_fd_get_diff_node_ok_01";
    ubs_mem_fd_desc_t fdDesc{};
    ASSERT_IT_OK(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc))
        << "node 1 create " << name << " failed";

    ubs_mem_fd_desc_t fdDescGet{};
    ASSERT_IT_OK(sdk.MemFdGet(name, &fdDescGet)) << "node 1 get " << name << " failed";
    EXPECT_STREQ(fdDescGet.name, name);
    EXPECT_TRUE(fdDescGet.mem_stage == UBSE_CREATING || fdDescGet.mem_stage == UBSE_EXIST);

    auto& sdk2 = cluster.GetSdkClient("2");
    ubs_mem_fd_desc_t fdDesc2{};
    ASSERT_IT_ERROR(sdk2.MemFdGet(name, &fdDesc2), UBS_ENGINE_ERR_NOT_EXIST) << "node 2 get fd failed";

    // 清理
    ASSERT_IT_OK(sdk.MemFdDelete(name)) << "node 1 delete " << name << " failed";
    IT_LOG_INFO << "P1-FdGet-DiffNode-Ok-01 passed";
}

// P1-NumaCreate-MultiNode-Ok-01(四节点): 多节点 NUMA 创建用例
void RunP1NumaCreateMultiNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_numa_create_multi_node_ok_01";

    // 两个线程同时提交创建FD请求
    std::thread thread1([&]() {
        ubs_mem_numa_desc_t numaDesc{};
        ASSERT_IT_OK(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc))
            << "node 1 create " << name << " failed";
    });

    auto& sdk2 = cluster.GetSdkClient("3");
    const char* name2 = "it_p1_numa_create_multi_node_ok_01_2";
    std::thread thread2([&]() {
        ubs_mem_numa_desc_t numaDesc2{};
        ASSERT_IT_OK(sdk2.MemNumaCreate(name2, numaSize, MEM_DISTANCE_L0, &numaDesc2))
            << "node 2 create " << name2 << " failed";
    });

    thread1.join();
    thread2.join();

    // 清理
    ASSERT_IT_OK(sdk.MemNumaDelete(name)) << "node 1 delete " << name << " failed";
    ASSERT_IT_OK(sdk2.MemNumaDelete(name2)) << "node 3 delete " << name2 << " failed";
    IT_LOG_INFO << "P1-NumaCreate-MultiNode-Ok-01 passed";
}

// P1-NumaCreate-MultiTime-Ok-01: numa 创建用例，多轮创建后归还
void RunP1NumaCreateMultiTimeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_numa_create_multi_ok_01";
    ubs_mem_numa_desc_t numaDesc{};
    ASSERT_IT_OK(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc))
        << "node 1 create " << name << " failed";

    const char* name2 = "it_p1_numa_create_multi_ok_01_2";
    ubs_mem_numa_desc_t numaDesc2{};
    ASSERT_IT_OK(sdk.MemNumaCreate(name2, numaSize, MEM_DISTANCE_L0, &numaDesc2))
        << "node 2 create " << name2 << " failed";

    // 清理
    ASSERT_IT_OK(sdk.MemNumaDelete(name)) << "node 1 delete " << name << " failed";
    ASSERT_IT_OK(sdk.MemNumaDelete(name2)) << "node 2 delete " << name2 << " failed";

    // 重新创建，再归还
    ubs_mem_numa_desc_t numaDesc3{};
    ASSERT_IT_OK(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc3))
        << "node 1 create " << name << " failed";

    ASSERT_IT_OK(sdk.MemNumaDelete(name)) << "node 1 delete " << name << " failed";
    IT_LOG_INFO << "P1-NumaCreate-MultiTime-Ok-01 passed";
}

// P1-NumaCreate-DiffNode-Ok-01: numa 创建用例，不同节点创建同名NUMA
void RunP1NumaCreateDiffNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_numa_create_diff_node_ok_01";
    ubs_mem_numa_desc_t numaDesc{};
    ASSERT_IT_OK(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc))
        << "node 1 create " << name << " failed";

    // 清理
    ASSERT_IT_OK(sdk.MemNumaDelete(name)) << "node 1 delete " << name << " failed";

    auto& sdk2 = cluster.GetSdkClient("2");
    ubs_mem_numa_desc_t numaDesc2{};
    ASSERT_IT_OK(sdk2.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc2))
        << "node 2 create " << name << " failed";

    // 清理
    ASSERT_IT_OK(sdk2.MemNumaDelete(name)) << "node 2 delete " << name << " failed";
    IT_LOG_INFO << "P1-NumaCreate-DiffNode-Ok-01 passed";
}

// P1-NumaGet-DiffNode-Ok-01: numa 获取用例，不同节点获取同名NUMA
void RunP1NumaGetDiffNodeOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const char* name = "it_p1_numa_get_diff_node_ok_01";
    ubs_mem_numa_desc_t numaDesc{};
    ASSERT_IT_OK(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc))
        << "node 1 create " << name << " failed";

    ubs_mem_numa_desc_t numaDescGet{};
    ASSERT_IT_OK(sdk.MemNumaGet(name, &numaDescGet)) << "node 1 get numa failed";
    EXPECT_STREQ(numaDescGet.name, name);
    EXPECT_TRUE(numaDescGet.mem_stage == UBSE_CREATING || numaDescGet.mem_stage == UBSE_EXIST);

    auto& sdk2 = cluster.GetSdkClient("2");
    ubs_mem_numa_desc_t numaDesc2{};
    ASSERT_IT_ERROR(sdk2.MemNumaGet(name, &numaDesc2), UBS_ENGINE_ERR_NOT_EXIST) << "node 2 get numa failed";

    // 清理
    ASSERT_IT_OK(sdk.MemNumaDelete(name)) << "node 1 delete numa failed";
    IT_LOG_INFO << "P1-NumaGet-DiffNode-Ok-01 passed";
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
} // namespace ubse::it::tests::mem_borrow
