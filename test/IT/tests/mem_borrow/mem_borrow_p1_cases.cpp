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

#include <algorithm>
#include <future>
#include <string>
#include <vector>

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

// P1-CliSdkMemOk-01: 测试CLI创建后调用SDK接口正常
void RunP1CliSdkMemOK01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto& sdk = cluster.GetSdkClient("1");
    using ubse::it::infra::util::ExtractNodeId;

    // 使用CLI创建FD内存
    ubse::it::infra::ItMemCreateInfo fdCreateInfo;
    const std::string fdName = "it_cli_sdk_fd_test";
    ASSERT_IT_OK(cliInvoker.CreateMemoryFd(fdCreateInfo, fdName, "128M"));
    EXPECT_EQ(fdCreateInfo.name, fdName);
    EXPECT_EQ(fdCreateInfo.size, "128MB");

    // SDK查询FD内存
    ubs_mem_fd_desc_t fdDesc{};
    EXPECT_IT_OK(sdk.MemFdGet(fdName.c_str(), &fdDesc));
    EXPECT_STREQ(fdDesc.name, fdName.c_str());
    EXPECT_TRUE(fdDesc.mem_stage == UBSE_CREATING || fdDesc.mem_stage == UBSE_EXIST);

    // 使用CLI创建NUMA内存
    ubse::it::infra::ItMemCreateInfo numaCreateInfo;
    const std::string numaName = "it_cli_sdk_numa_test";
    ASSERT_IT_OK(cliInvoker.CreateMemoryNuma(numaCreateInfo, numaName, "128M"))
        << "create numa " << numaName << " failed";
    EXPECT_EQ(numaCreateInfo.name, numaName);
    EXPECT_EQ(numaCreateInfo.size, "128MB");

    // SDK查询NUMA内存
    ubs_mem_numa_desc_t numaDesc{};
    EXPECT_IT_OK(sdk.MemNumaGet(numaName.c_str(), &numaDesc)) << "get numa " << numaName << " failed";
    EXPECT_STREQ(numaDesc.name, numaName.c_str());
    EXPECT_TRUE(numaDesc.mem_stage == UBSE_CREATING || numaDesc.mem_stage == UBSE_EXIST);

    // 使用CLI创建SHARE内存
    ubse::it::infra::ItMemCreateInfo shareCreateInfo;
    const std::string shareName = "it_cli_sdk_share_test";
    ASSERT_IT_OK(cliInvoker.CreateMemoryShare(shareCreateInfo, shareName, "128M", "1,2"))
        << "create share " << shareName << " failed";
    EXPECT_EQ(shareCreateInfo.name, shareName);
    EXPECT_EQ(shareCreateInfo.size, "128MB");

    // SDK查询SHARE内存
    ubs_mem_shm_desc_t* shareDesc = nullptr;
    EXPECT_IT_OK(sdk.MemShmGet(shareName.c_str(), &shareDesc)) << "get share " << shareName << " failed";
    ASSERT_NE(shareDesc, nullptr);
    EXPECT_STREQ(shareDesc->name, shareName.c_str());
    EXPECT_EQ(shareDesc->mem_size, 128ULL * 1024 * 1024); // 128MB
    free(shareDesc);

    // 清理资源
    ASSERT_IT_OK(sdk.MemFdDelete(fdName.c_str())) << "delete fd " << fdName << " failed";
    ASSERT_IT_OK(sdk.MemNumaDelete(numaName.c_str())) << "delete numa " << numaName << " failed";
    ASSERT_IT_OK(sdk.MemShmDelete(shareName.c_str())) << "delete share " << shareName << " failed";
}

// P1-SdkCliMemOk-01: 测试SDK创建后调用CLI接口正常
void RunP1SdkCliMemOK01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& cliInvoker = cluster.GetCliInvoker("1");
    using ubse::it::infra::util::ExtractNodeId;

    // 使用SDK创建FD内存
    const std::string fdName = "it_sdk_cli_fd_test";
    ubs_mem_fd_desc_t fdDesc{};
    ASSERT_IT_OK(sdk.MemFdCreate(fdName.c_str(), 128ULL * 1024 * 1024, nullptr, 0, MEM_DISTANCE_L0, &fdDesc))
        << "create fd " << fdName << " failed";
    EXPECT_STREQ(fdDesc.name, fdName.c_str());

    // CLI查询FD内存
    std::vector<ubse::it::infra::ItMemBorrowDetail> fdBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(fdBorrowDetails, "fd", fdName));
    EXPECT_EQ(fdBorrowDetails.size(), 1);
    EXPECT_EQ(fdBorrowDetails[0].name, fdName);

    // 使用SDK创建NUMA内存
    const std::string numaName = "it_sdk_cli_numa_test";
    ubs_mem_numa_desc_t numaDesc{};
    ASSERT_IT_OK(sdk.MemNumaCreate(numaName.c_str(), 128ULL * 1024 * 1024, MEM_DISTANCE_L0, &numaDesc))
        << "create numa " << numaName << " failed";
    EXPECT_STREQ(numaDesc.name, numaName.c_str());

    // CLI查询NUMA内存
    std::vector<ubse::it::infra::ItMemBorrowDetail> numaBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(numaBorrowDetails, "numa", numaName));
    EXPECT_EQ(numaBorrowDetails.size(), 1);
    EXPECT_EQ(numaBorrowDetails[0].name, numaName);

    // 使用SDK创建SHARE内存
    const std::string shareName = "it_sdk_cli_share_test";
    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = 1;
    region.slot_ids[1] = 2;
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
    ASSERT_IT_OK(sdk.MemShmCreate(shareName.c_str(), 128ULL * 1024 * 1024, usrInfo, 0, &region, nullptr))
        << "create share " << shareName << " failed";

    // 等待SHARE内存创建完成
    auto waitRet = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&]() {
            ubs_mem_shm_desc_t* shmDesc = nullptr;
            int32_t getRet = sdk.MemShmGet(shareName.c_str(), &shmDesc);
            if (getRet != UBS_SUCCESS || shmDesc == nullptr) {
                return false;
            }
            bool ready = (shmDesc->mem_stage == UBSE_EXIST);
            free(shmDesc);
            return ready;
        },
        15000, 200);
    EXPECT_IT_OK(waitRet);

    // CLI查询SHARE内存
    std::vector<ubse::it::infra::ItMemBorrowDetail> shareBorrowDetails;
    EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(shareBorrowDetails, "share", shareName));
    EXPECT_EQ(shareBorrowDetails.size(), 1);
    EXPECT_EQ(shareBorrowDetails[0].name, shareName);

    // 清理资源
    ASSERT_IT_OK(cliInvoker.DeleteMemory(fdName, "fd")) << "delete fd " << fdName << " failed";
    ASSERT_IT_OK(cliInvoker.DeleteMemory(numaName, "numa")) << "delete numa " << numaName << " failed";
    ASSERT_IT_OK(cliInvoker.DeleteMemory(shareName, "share")) << "delete share " << shareName << " failed";
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

// P1-FdCreate-MultiThread-Ok-01: 单节点多并发FD创建用例
void RunP1FdCreateMultiThreadOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    const int threadCount = 2; // 2个线程并发创建
    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        std::string name = "it_p1_fd_create_multi_thread_ok_01_" + std::to_string(i);
        threads.emplace_back([&sdk, name]() {
            ubs_mem_fd_desc_t fdDesc{};
            ASSERT_IT_OK(sdk.MemFdCreate(name.c_str(), fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc))
                << "create " << name << " failed";
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 清理所有创建的FD
    for (int i = 0; i < threadCount; ++i) {
        std::string name = "it_p1_fd_create_multi_thread_ok_01_" + std::to_string(i);
        ASSERT_IT_OK(sdk.MemFdDelete(name.c_str())) << "delete " << name << " failed";
    }
    IT_LOG_INFO << "P1-FdCreate-MultiThread-Ok-01 passed";
}

// P1-FdBorrow-Cycle-01(四节点): 四节点并发借用成环借用失败用例
void RunP1FdBorrowCycle01(ubse::it::infra::ItCluster& cluster)
{
    // 准备四个节点的SDK客户端
    auto& sdk1 = cluster.GetSdkClient("1");
    auto& sdk2 = cluster.GetSdkClient("2");
    auto& sdk3 = cluster.GetSdkClient("3");
    auto& sdk4 = cluster.GetSdkClient("4");

    // 定义四个FD名称，形成环型依赖: 1→2→3→4→1
    const char* name1 = "it_p1_fd_cycle_01_1";
    const char* name2 = "it_p1_fd_cycle_01_2";
    const char* name3 = "it_p1_fd_cycle_01_3";
    const char* name4 = "it_p1_fd_cycle_01_4";

    // 并发创建FD，模拟成环借用场景
    std::vector<int32_t> threadResults(4, UBS_SUCCESS);
    std::thread t1([&]() {
        ubs_mem_fd_desc_t fdDesc{};
        // 节点1尝试创建name1，期望从节点2借用
        threadResults[0] = sdk1.MemFdCreate(name1, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    });

    std::thread t2([&]() {
        ubs_mem_fd_desc_t fdDesc{};
        // 节点2尝试创建name2，期望从节点3借用
        threadResults[1] = sdk2.MemFdCreate(name2, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    });

    std::thread t3([&]() {
        ubs_mem_fd_desc_t fdDesc{};
        // 节点3尝试创建name3，期望从节点4借用
        threadResults[2] = sdk3.MemFdCreate(name3, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    });

    std::thread t4([&]() {
        ubs_mem_fd_desc_t fdDesc{};
        // 节点4尝试创建name4，期望从节点1借用，形成闭环
        threadResults[3] = sdk4.MemFdCreate(name4, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
    });

    // 等待所有线程完成
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // 验证是否存在借用失败的情况（成环借用应导致至少部分请求失败）
    bool hasFailure = false;
    for (int32_t res : threadResults) {
        if (res != UBS_SUCCESS) {
            hasFailure = true;
            break;
        }
    }
    EXPECT_TRUE(hasFailure) << "borrow cycle scenario should have at least one request failed";

    // 验证所有FD均未创建成功（清理操作，防止残留影响）
    sdk1.MemFdDelete(name1);
    sdk2.MemFdDelete(name2);
    sdk3.MemFdDelete(name3);
    sdk4.MemFdDelete(name4);
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

// 双节点SHM 创建→节点1attach→borrow_detail查账(存在)→detach/delete→再查账(为空)
void RunP1CliBorrowDetailLedger01(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    const std::string shmName = "it_cli_shm_ledger";
    auto nodeIds = cluster.GetNodeIds();
    std::string region;
    for (size_t i = 0; i < nodeIds.size(); i++) {
        if (i > 0)
            region += ",";
        region += nodeIds[i];
    }

    IT_LOG_INFO << "S1: 创建共享内存, name=" << shmName << ", region=" << region;
    ubse::it::infra::ItMemCreateInfo createInfo;
    EXPECT_IT_OK(cliInvoker.CreateMemoryShare(createInfo, shmName, "128M", region));
    EXPECT_EQ(createInfo.name, shmName);
    EXPECT_EQ(createInfo.size, "128MB");
    EXPECT_FALSE(createInfo.exportNode.empty()) << "SHARE export-node should not be empty";
    EXPECT_EQ(createInfo.region, region) << "region should match input";

    IT_LOG_INFO << "S2: 节点1 attach(映射) 共享内存";
    ubse::it::infra::ItMemCreateInfo attachInfo;
    EXPECT_IT_OK(cliInvoker.AttachMemory(attachInfo, shmName));
    EXPECT_EQ(attachInfo.name, shmName);
    EXPECT_EQ(attachInfo.size, "128MB");
    EXPECT_FALSE(attachInfo.memIds.empty()) << "attach should return mem-ids";
    EXPECT_FALSE(attachInfo.importNode.empty()) << "attach should return import-node";
    EXPECT_FALSE(attachInfo.exportNode.empty()) << "attach should return export-node";

    IT_LOG_INFO << "S3: borrow_detail 查询账本, 应存在该 share 记录";
    {
        std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
        EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetails, "share", shmName));
        bool found = false;
        for (const auto& detail : borrowDetails) {
            if (detail.name == shmName) {
                found = true;
                EXPECT_EQ(detail.type, "share");
                EXPECT_NE(detail.lendSize.find("128"), std::string::npos) << "lend_size should contain 128";
                break;
            }
        }
        EXPECT_TRUE(found) << "borrow_detail 应包含 " << shmName;
    }

    IT_LOG_INFO << "S4: detach 并 delete 共享内存";
    EXPECT_IT_OK(cliInvoker.DetachMemory(shmName));
    EXPECT_IT_OK(cliInvoker.DeleteMemory(shmName, "share"));

    IT_LOG_INFO << "S5: 再次查询账本, 应无该 share 记录";
    {
        std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetailsAfter;
        EXPECT_IT_OK(cliInvoker.DisplayMemoryBorrowDetail(borrowDetailsAfter, "share", shmName));
        EXPECT_EQ(borrowDetailsAfter.size(), 0) << "detach/delete 后账本应无该共享内存记录";
    }

    IT_LOG_INFO << "P1-CliBorrowDetail-Ledger-01 passed";
}

// 双节点SHM 节点1/节点2各自创建(region覆盖双节点)→borrow_detail查账(2条share记录)→节点2删除→再查账(仅剩节点1记录)
void RunP1CliBorrowDetailMultiNode01(ubse::it::infra::ItCluster& cluster)
{
    const std::string node1Name = "it_cli_shm_mn_1";
    const std::string node2Name = "it_cli_shm_mn_2";
    const std::string region = "1,2";
    auto& cli1 = cluster.GetCliInvoker("1");
    auto& cli2 = cluster.GetCliInvoker("2");

    IT_LOG_INFO << "S1: 节点1创建共享内存 name=" << node1Name << ", region=" << region;
    ubse::it::infra::ItMemCreateInfo createInfo1;
    EXPECT_IT_OK(cli1.CreateMemoryShare(createInfo1, node1Name, "128M", region));
    EXPECT_EQ(createInfo1.name, node1Name);
    EXPECT_EQ(createInfo1.size, "128MB");
    EXPECT_FALSE(createInfo1.exportNode.empty()) << "节点1创建SHM export-node不应为空";
    EXPECT_EQ(createInfo1.region, region) << "节点1创建SHM region应一致";

    IT_LOG_INFO << "S2: 节点2创建共享内存 name=" << node2Name << ", region=" << region;
    ubse::it::infra::ItMemCreateInfo createInfo2;
    EXPECT_IT_OK(cli2.CreateMemoryShare(createInfo2, node2Name, "128M", region));
    EXPECT_EQ(createInfo2.name, node2Name);
    EXPECT_EQ(createInfo2.size, "128MB");
    EXPECT_FALSE(createInfo2.exportNode.empty()) << "节点2创建SHM export-node不应为空";
    EXPECT_EQ(createInfo2.region, region) << "节点2创建SHM region应一致";

    IT_LOG_INFO << "S3: 节点1 CLI borrow_detail 查询账本, 应含两条share记录";
    {
        std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetails;
        EXPECT_IT_OK(cli1.DisplayMemoryBorrowDetail(borrowDetails, "share"));
        bool found1 = false;
        bool found2 = false;
        for (const auto& detail : borrowDetails) {
            if (detail.type != "share") {
                continue;
            }
            if (detail.name == node1Name) {
                found1 = true;
            } else if (detail.name == node2Name) {
                found2 = true;
            }
        }
        EXPECT_TRUE(found1) << "borrow_detail 应包含 " << node1Name;
        EXPECT_TRUE(found2) << "borrow_detail 应包含 " << node2Name;
    }

    IT_LOG_INFO << "S4: 节点2删除其共享内存 name=" << node2Name;
    EXPECT_IT_OK(cli2.DeleteMemory(node2Name, "share"));

    IT_LOG_INFO << "S5: 节点1 CLI borrow_detail 再次查询账本, 应仅剩节点1的记录";
    {
        std::vector<ubse::it::infra::ItMemBorrowDetail> borrowDetailsAfter;
        EXPECT_IT_OK(cli1.DisplayMemoryBorrowDetail(borrowDetailsAfter, "share"));
        bool found1 = false;
        bool found2 = false;
        for (const auto& detail : borrowDetailsAfter) {
            if (detail.type != "share") {
                continue;
            }
            if (detail.name == node1Name) {
                found1 = true;
            } else if (detail.name == node2Name) {
                found2 = true;
            }
        }
        EXPECT_TRUE(found1) << "节点2删除后 borrow_detail 应仍包含 " << node1Name;
        EXPECT_FALSE(found2) << "节点2删除后 borrow_detail 不应包含 " << node2Name;
    }

    IT_LOG_INFO << "S6: 清理节点1的共享内存 name=" << node1Name;
    EXPECT_IT_OK(cli1.DeleteMemory(node1Name, "share"));

    IT_LOG_INFO << "P1-CliBorrowDetail-MultiNode-01 passed";
}

// P1-ShmCreate-Concurrent-01: 双节点，8并发创建共享内存，全部创建成功
void RunP1ShmCreateConcurrent01(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    constexpr uint32_t kCreateCnt = 8;                           // 并发创建数
    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)
    const char* names[kCreateCnt] = {"it_p1_shm_conc_01", "it_p1_shm_conc_02", "it_p1_shm_conc_03",
                                     "it_p1_shm_conc_04", "it_p1_shm_conc_05", "it_p1_shm_conc_06",
                                     "it_p1_shm_conc_07", "it_p1_shm_conc_08"};
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    // S1. 8并发创建共享内存：8个线程，每个线程创建不同name的SHM，region覆盖节点1/2
    IT_LOG_INFO << "S1: Concurrently creating " << kCreateCnt << " SHMs";
    int32_t rets[kCreateCnt] = {0};
    std::vector<std::future<int32_t>> futures;
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&, i]() -> int32_t {
            return sdk.MemShmCreate(names[i], shmSize128M, usrInfo, 0, &region, nullptr);
        }));
    }
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        rets[i] = futures[i].get();
        EXPECT_IT_OK(rets[i]) << "Concurrent create failed, name=" << names[i];
    }

    // S2. 等待8个共享内存全部创建就绪
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        IT_LOG_INFO << "S2: Wait SHM ready: name=" << names[i];
        int32_t ret = WaitForShmReady(sdk, names[i]);
        EXPECT_IT_OK(ret) << "WaitForShmReady failed for " << names[i];
    }

    // S3. 逐一查询，校验 name/mem_size/mem_stage
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        ubs_mem_shm_desc_t* desc = nullptr;
        int32_t ret = sdk.MemShmGet(names[i], &desc);
        EXPECT_IT_OK(ret) << "MemShmGet failed for " << names[i];
        if (desc != nullptr) {
            EXPECT_STREQ(desc->name, names[i]);
            EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size mismatch, name=" << names[i];
            EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
                << "unexpected mem_stage, name=" << names[i];
            free(desc);
        }
    }

    // S4. 清理：逐一删除8个共享内存
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        IT_LOG_INFO << "S4: Delete SHM: name=" << names[i];
        EXPECT_IT_OK(sdk.MemShmDelete(names[i])) << "MemShmDelete failed for " << names[i];
    }

    IT_LOG_INFO << "P1-ShmCreate-Concurrent-01 done";
}

// P1-ShmCreate-Concurrent-MultiNode-01: 四节点，4节点各自并发创建共享内存，全部创建成功
void RunP1ShmCreateConcurrentMultiNode01(ubse::it::infra::ItCluster& cluster)
{
    constexpr uint32_t kNodeCnt = 4;                             // 节点数
    constexpr uint32_t kCreateCnt = 4;                           // 并发创建数
    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)
    const char* names[kCreateCnt] = {"it_p1_shm_conc4m_01", "it_p1_shm_conc4m_02", "it_p1_shm_conc4m_03",
                                     "it_p1_shm_conc4m_04"};
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    // 预先获取4个节点的SDK客户端引用，避免在并发线程内查询
    ubse::it::infra::ItSdkClient* sdks[kNodeCnt] = {nullptr};
    for (uint32_t i = 0; i < kNodeCnt; ++i) {
        sdks[i] = &cluster.GetSdkClient(std::to_string(i + 1));
    }

    // region 覆盖全部4节点
    ubs_mem_nodes_t region{};
    region.node_cnt = kNodeCnt;
    for (uint32_t i = 0; i < kNodeCnt; ++i) {
        region.slot_ids[i] = cluster.GetNode(std::to_string(i + 1)).GetSpec().slotId;
    }

    // S1. 4个节点各自并发创建共享内存：4个线程，每线程用不同节点的SDK创建不同name的SHM，region覆盖全部4节点
    IT_LOG_INFO << "S1: Concurrently creating " << kCreateCnt << " SHMs from 4 nodes";
    int32_t rets[kCreateCnt] = {0};
    std::vector<std::future<int32_t>> futures;
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&, i]() -> int32_t {
            return sdks[i]->MemShmCreate(names[i], shmSize128M, usrInfo, 0, &region, nullptr);
        }));
    }
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        rets[i] = futures[i].get();
        EXPECT_IT_OK(rets[i]) << "Concurrent create failed, node=" << (i + 1) << ", name=" << names[i];
    }

    // S2. 等待4个共享内存全部创建就绪
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        IT_LOG_INFO << "S2: Wait SHM ready: name=" << names[i];
        int32_t ret = WaitForShmReady(*sdks[i], names[i]);
        EXPECT_IT_OK(ret) << "WaitForShmReady failed for " << names[i];
    }

    // S3. 逐一查询，校验 name/mem_size/mem_stage
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        ubs_mem_shm_desc_t* desc = nullptr;
        int32_t ret = sdks[i]->MemShmGet(names[i], &desc);
        EXPECT_IT_OK(ret) << "MemShmGet failed for " << names[i];
        if (desc != nullptr) {
            EXPECT_STREQ(desc->name, names[i]);
            EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size mismatch, name=" << names[i];
            EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
                << "unexpected mem_stage, name=" << names[i];
            free(desc);
        }
    }

    // S4. 清理：逐一删除4个共享内存
    for (uint32_t i = 0; i < kCreateCnt; ++i) {
        IT_LOG_INFO << "S4: Delete SHM: name=" << names[i];
        EXPECT_IT_OK(sdks[i]->MemShmDelete(names[i])) << "MemShmDelete failed for " << names[i];
    }

    IT_LOG_INFO << "P1-ShmCreate-Concurrent-MultiNode-01 done";
}

// P1-ShmCreate-WithProviders-MultiNode-01: 四节点，指定2个provider创建共享内存，校验export_node在provider集合内
void RunP1ShmCreateWithProvidersMultiNode01(ubse::it::infra::ItCluster& cluster)
{
    constexpr const char* shmName = "it_p1_shm_create_providers";
    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    auto& node1Client = cluster.GetSdkClient("1");

    // region 覆盖全部4节点
    ubs_mem_nodes_t region{};
    region.node_cnt = 4;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
    region.slot_ids[2] = cluster.GetNode("3").GetSpec().slotId;
    region.slot_ids[3] = cluster.GetNode("4").GetSpec().slotId;

    // 指定2个provider：节点2、节点3
    ubs_mem_nodes_t provider{};
    provider.node_cnt = 2;
    provider.slot_ids[0] = cluster.GetNode("2").GetSpec().slotId;
    provider.slot_ids[1] = cluster.GetNode("3").GetSpec().slotId;

    // S1. 节点1创建SHM，指定2个provider
    IT_LOG_INFO << "S1: Creating SHM with 2 providers: name=" << shmName << ", size=" << shmSize128M
                << ", provider_slot0=" << provider.slot_ids[0] << ", provider_slot1=" << provider.slot_ids[1];
    int32_t ret = node1Client.MemShmCreate(shmName, shmSize128M, usrInfo, 0, &region, &provider);
    ASSERT_IT_OK(ret);

    // S2. 等待创建就绪
    IT_LOG_INFO << "S2: Wait SHM ready: name=" << shmName;
    ret = WaitForShmReady(node1Client, shmName);
    EXPECT_IT_OK(ret);

    // S3. 查询并校验导出节点在provider集合内
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = node1Client.MemShmGet(shmName, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_STREQ(desc->name, shmName);
        EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size should equal input size";
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
            << "unexpected mem_stage: " << desc->mem_stage;
        EXPECT_GT(desc->export_node.slot_id, 0u) << "export_node.slot_id should be > 0";
        auto inProvider = std::any_of(provider.slot_ids, provider.slot_ids + provider.node_cnt,
                                      [&](uint32_t id) { return id == desc->export_node.slot_id; });
        EXPECT_TRUE(inProvider) << "export_node.slot_id=" << desc->export_node.slot_id
                                << " should be in provider nodes";
        free(desc);
    }

    // S4. 清理
    IT_LOG_INFO << "S4: Delete SHM: name=" << shmName;
    EXPECT_IT_OK(node1Client.MemShmDelete(shmName));

    IT_LOG_INFO << "P1-ShmCreate-WithProviders-MultiNode-01 done";
}

// P1-ShmRecreate-AfterDelete-01: 双节点，节点1创建SHM→删除→节点2创建同名SHM成功（验证删除释放同名）
void RunP1ShmRecreateAfterDelete01(ubse::it::infra::ItCluster& cluster)
{
    constexpr const char* shmName = "it_p1_shm_recreate";
    constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    auto& node1Client = cluster.GetSdkClient("1");
    auto& node2Client = cluster.GetSdkClient("2");

    // region 覆盖节点1/2
    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

    // S1. 节点1创建共享内存，region覆盖节点1/2
    IT_LOG_INFO << "S1: Node1 creating SHM: name=" << shmName << ", size=" << shmSize128M;
    int32_t ret = node1Client.MemShmCreate(shmName, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret);

    // S2. 等待节点1创建就绪
    IT_LOG_INFO << "S2: Wait SHM ready: name=" << shmName;
    ret = WaitForShmReady(node1Client, shmName);
    EXPECT_IT_OK(ret);

    // S3. 节点2删除共享内存
    IT_LOG_INFO << "S3: Node1 deleting SHM: name=" << shmName;
    ret = node2Client.MemShmDelete(shmName);
    EXPECT_IT_OK(ret) << "delete after create should succeed";

    // S4. 验证已不存在（删除后同名已释放）
    {
        ubs_mem_shm_desc_t* desc = nullptr;
        ret = node1Client.MemShmGet(shmName, &desc);
        EXPECT_NE(ret, UBS_SUCCESS) << "SHM should not exist after delete";
        if (desc != nullptr) {
            free(desc);
        }
    }

    // S5. 节点2创建同名共享内存，region覆盖节点1/2
    IT_LOG_INFO << "S5: Node2 recreating same-name SHM: name=" << shmName << ", size=" << shmSize128M;
    ret = node2Client.MemShmCreate(shmName, shmSize128M, usrInfo, 0, &region, nullptr);
    ASSERT_IT_OK(ret) << "node2 should recreate same-name SHM after delete";

    // S6. 等待节点2创建就绪
    IT_LOG_INFO << "S6: Wait SHM ready: name=" << shmName;
    ret = WaitForShmReady(node2Client, shmName);
    EXPECT_IT_OK(ret);

    // S7. 节点2查询并校验出参
    ubs_mem_shm_desc_t* desc = nullptr;
    ret = node2Client.MemShmGet(shmName, &desc);
    EXPECT_IT_OK(ret);
    if (desc != nullptr) {
        EXPECT_STREQ(desc->name, shmName);
        EXPECT_EQ(desc->mem_size, shmSize128M) << "mem_size should equal input size";
        EXPECT_TRUE(desc->mem_stage == UBSE_CREATING || desc->mem_stage == UBSE_EXIST)
            << "unexpected mem_stage: " << desc->mem_stage;
        free(desc);
    }

    // S8. 清理：删除重建的共享内存
    IT_LOG_INFO << "S8: Cleanup: delete SHM: name=" << shmName;
    EXPECT_IT_OK(node2Client.MemShmDelete(shmName));

    IT_LOG_INFO << "P1-ShmRecreate-AfterDelete-01 done";
}

// 四节点SHM detach后内存账本consumer归空校验：
// 节点1创建(name=detach001, region={1,2,3,4}) → 节点1/2 attach → 查询账本consumer包含1/2
// → 节点1/2 detach → 查询账本consumer为空 → 删除
void RunP1ShmDetachMultiNode01(ubse::it::infra::ItCluster& cluster)
{
    constexpr const char* shmName = "detach001";
    constexpr uint64_t shmSize = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)

    auto& node1Client = cluster.GetSdkClient("1");

    // S1. 节点1创建共享内存，name=detach001，region覆盖四节点
    ubs_mem_nodes_t region{};
    region.node_cnt = 4;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
    region.slot_ids[2] = cluster.GetNode("3").GetSpec().slotId;
    region.slot_ids[3] = cluster.GetNode("4").GetSpec().slotId;
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    IT_LOG_INFO << "S1: Creating SHM on node1: name=" << shmName << ", size=" << shmSize;
    int32_t ret = node1Client.MemShmCreate(shmName, shmSize, usrInfo, 0, &region, nullptr);
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

    // S2. 节点1、2均调用ubse_mem_shm_attach，传入name=detach001
    std::vector<std::string> attachNodes = {"1", "2"};
    for (const auto& nodeId : attachNodes) {
        auto& client = cluster.GetSdkClient(nodeId);
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        IT_LOG_INFO << "S2: Attaching SHM on node " << nodeId;
        ret = client.MemShmAttach(shmName, nullptr, 0, &shmDesc);
        ASSERT_IT_OK(ret);
        ASSERT_NE(shmDesc, nullptr);
        IT_LOG_INFO << "Node " << nodeId << " attach result: import_desc_cnt=" << shmDesc->import_desc_cnt;
        if (shmDesc != nullptr) {
            free(shmDesc);
        }
    }

    // S3. 查看内存账本：MemShmGet返回的import_desc_cnt应包含节点1、2的导入信息
    {
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        ret = node1Client.MemShmGet(shmName, &shmDesc);
        EXPECT_IT_OK(ret);
        if (shmDesc != nullptr) {
            IT_LOG_INFO << "S3: Node1 Get after attach: import_desc_cnt=" << shmDesc->import_desc_cnt;
            // 账本consumer应包含节点1、2
            EXPECT_EQ(shmDesc->import_desc_cnt, 2u) << "import_desc_cnt should be 2 (node1 and node2 attached)";
            const uint32_t slot1 = cluster.GetNode("1").GetSpec().slotId;
            const uint32_t slot2 = cluster.GetNode("2").GetSpec().slotId;
            bool hasNode1 = false;
            bool hasNode2 = false;
            for (uint32_t i = 0; i < shmDesc->import_desc_cnt; ++i) {
                if (shmDesc->import_desc[i].import_node.slot_id == slot1) {
                    hasNode1 = true;
                }
                if (shmDesc->import_desc[i].import_node.slot_id == slot2) {
                    hasNode2 = true;
                }
            }
            EXPECT_TRUE(hasNode1) << "ledger consumer should contain node1";
            EXPECT_TRUE(hasNode2) << "ledger consumer should contain node2";
            free(shmDesc);
        }
    }

    // S4. 节点1、2均调用ubse_mem_shm_detach，传入name=detach001
    for (const auto& nodeId : attachNodes) {
        auto& client = cluster.GetSdkClient(nodeId);
        IT_LOG_INFO << "S4: Detaching SHM on node " << nodeId;
        ret = client.MemShmDetach(shmName);
        EXPECT_IT_OK(ret);
    }

    // S5. 查看内存账本信息：detach后查询，账本consumer为空
    {
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        ret = node1Client.MemShmGet(shmName, &shmDesc);
        EXPECT_IT_OK(ret) << "S5: MemShmGet after detach should succeed";
        if (shmDesc != nullptr) {
            IT_LOG_INFO << "S5: Node1 Get after detach: import_desc_cnt=" << shmDesc->import_desc_cnt;
            EXPECT_EQ(shmDesc->import_desc_cnt, 0u) << "ledger consumer should be empty after detach";
            free(shmDesc);
        }
    }

    // 清理：节点1删除共享内存
    IT_LOG_INFO << "Cleaning up: deleting SHM " << shmName;
    EXPECT_IT_OK(node1Client.MemShmDelete(shmName));

    IT_LOG_INFO << "P1-ShmDetach-MultiNode-01 done";
}

// P1-ShmDetachReattach-MultiNode-01: 双节点SHM 节点1创建→节点1attach→节点1detach→节点2attach，全部成功
void RunP1ShmDetachReattachMultiNode01(ubse::it::infra::ItCluster& cluster)
{
    constexpr const char* shmName = "reattach001";
    constexpr uint64_t shmSize = 128ULL * 1024ULL * 1024ULL; // SHM要求size对齐unit_size(128MB)

    auto& node1Client = cluster.GetSdkClient("1");

    // S1. 节点1创建共享内存，name=reattach001，region覆盖双节点
    ubs_mem_nodes_t region{};
    region.node_cnt = 2;
    region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
    region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

    IT_LOG_INFO << "S1: Creating SHM on node1: name=" << shmName << ", size=" << shmSize;
    int32_t ret = node1Client.MemShmCreate(shmName, shmSize, usrInfo, 0, &region, nullptr);
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

    // S2. 节点1调用ubse_mem_shm_attach映射
    {
        auto& client = cluster.GetSdkClient("1");
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        IT_LOG_INFO << "S2: Attaching SHM on node1: name=" << shmName;
        ret = client.MemShmAttach(shmName, nullptr, 0, &shmDesc);
        ASSERT_IT_OK(ret);
        ASSERT_NE(shmDesc, nullptr);
        IT_LOG_INFO << "Node1 attach result: import_desc_cnt=" << shmDesc->import_desc_cnt;
        if (shmDesc != nullptr) {
            free(shmDesc);
        }
    }

    // S3. 节点1调用ubse_mem_shm_detach解除映射
    IT_LOG_INFO << "S3: Detaching SHM on node1: name=" << shmName;
    ret = node1Client.MemShmDetach(shmName);
    EXPECT_IT_OK(ret);

    // S4. 节点2调用ubse_mem_shm_attach映射
    {
        auto& client = cluster.GetSdkClient("2");
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        IT_LOG_INFO << "S4: Attaching SHM on node2: name=" << shmName;
        ret = client.MemShmAttach(shmName, nullptr, 0, &shmDesc);
        ASSERT_IT_OK(ret);
        ASSERT_NE(shmDesc, nullptr);
        IT_LOG_INFO << "Node2 attach result: import_desc_cnt=" << shmDesc->import_desc_cnt;
        if (shmDesc != nullptr) {
            free(shmDesc);
        }
    }

    // S5. 查看内存账本：detach后再attach，账本仅含节点2
    {
        ubs_mem_shm_desc_t* shmDesc = nullptr;
        ret = node1Client.MemShmGet(shmName, &shmDesc);
        EXPECT_IT_OK(ret);
        if (shmDesc != nullptr) {
            IT_LOG_INFO << "S5: Node1 Get after reattach: import_desc_cnt=" << shmDesc->import_desc_cnt;
            EXPECT_EQ(shmDesc->import_desc_cnt, 1u) << "ledger consumer should only contain node2";
            const uint32_t slot2 = cluster.GetNode("2").GetSpec().slotId;
            const uint32_t slot1 = cluster.GetNode("1").GetSpec().slotId;
            bool hasNode1 = false;
            bool hasNode2 = false;
            for (uint32_t i = 0; i < shmDesc->import_desc_cnt; ++i) {
                if (shmDesc->import_desc[i].import_node.slot_id == slot1) {
                    hasNode1 = true;
                }
                if (shmDesc->import_desc[i].import_node.slot_id == slot2) {
                    hasNode2 = true;
                }
            }
            EXPECT_FALSE(hasNode1) << "ledger consumer should not contain node1 after detach";
            EXPECT_TRUE(hasNode2) << "ledger consumer should contain node2";
            free(shmDesc);
        }
    }

    // S6. 清理：节点2解除映射，节点1删除共享内存
    IT_LOG_INFO << "S6: Cleaning up: node2 detach then node1 delete SHM " << shmName;
    EXPECT_IT_OK(cluster.GetSdkClient("2").MemShmDetach(shmName));
    EXPECT_IT_OK(node1Client.MemShmDelete(shmName));

    IT_LOG_INFO << "P1-ShmDetachReattach-MultiNode-01 done";
}
} // namespace ubse::it::tests::mem_borrow
