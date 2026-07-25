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

#include "mem_borrow_fault_log_cases.h"
#include <cstdint>

#include "it_assertion.h"
#include "it_console_log.h"
#include "it_fault_log_helper.h"
#include "it_node.h"
#include "it_sdk_client.h"
#include "ubs_engine_mem.h"
#include "ubs_error.h"

namespace ubse::it::tests::mem_borrow {

using ubse::it::infra::FaultLogEntry;
using ubse::it::infra::ItFaultLogHelper;

namespace {
constexpr uint64_t fdSize = UBS_MEM_MIN_SIZE; // 4MB
constexpr uint64_t numaSize = UBS_MEM_MIN_SIZE;
constexpr uint64_t shareSize = UBS_MEM_MIN_SIZE;
} // namespace

// BorrowCheckFailed-01: NUMA借用传入的链路不存在 触发 BORROW_CHECK_FAILED
void RunP1FaultLogBorrowCheckFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_check_failed";
    {
        IT_LOG_INFO << "[BorrowCheckFailed-01] Creating NUMA with error link";
        // 使用numa cli传入不存在的链路，触发BORROW_CHECK_FAILED
        ubse::it::infra::ItMemCreateInfo createInfo;
        // 传入不存在的链路，触发链路检查失败
        auto ret = cliInvoker.CreateMemoryNuma(createInfo, name, "128M", "1/1/1-2/1/1");
    }

    // 等待并校验 fault log 中出现 BORROW_CHECK_FAILED (faultCode=1)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0001"; }, 5000);

    ASSERT_FALSE(entries.empty()) << "Expected fault log entry with ErrorCode=ubse_borrow_0001 not found";
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(entry.requestSize, numaSize);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 1u); // CHECK_FAILED
    EXPECT_FALSE(entry.errorInfo.empty());
    EXPECT_FALSE(entry.advice.empty());
}

// BorrowNameExist-01: FD/NUMA/Share 同名重复创建触发 BORROW_NAME_EXIST，校验 ubse_fault.log
void RunP1FaultLogBorrowNameExist(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    // 测试FD同名重复创建
    {
        const char* name = "it_p1_fl_name_exist_fd";
        ubs_mem_fd_desc_t fdDesc{};

        // 第一次创建成功
        IT_LOG_INFO << "[NameExist] Creating FD first time: name=" << name;
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        ASSERT_IT_OK(ret);

        // 同名重复创建，触发 BORROW_NAME_EXIST
        IT_LOG_INFO << "[NameExist] Creating FD with duplicate name: name=" << name;
        ubs_mem_fd_desc_t fdDesc2{};
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc2);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

        // 清理
        ret = sdk.MemFdDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 测试NUMA同名重复创建
    {
        const char* name = "it_p1_fl_name_exist_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 第一次创建成功
        IT_LOG_INFO << "[NameExist] Creating NUMA first time: name=" << name;
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        ASSERT_IT_OK(ret);

        // 同名重复创建，触发 BORROW_NAME_EXIST
        IT_LOG_INFO << "[NameExist] Creating NUMA with duplicate name: name=" << name;
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

        // 清理
        ret = sdk.MemNumaDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 测试Share同名重复创建
    {
        const char* name = "it_p1_fl_name_exist_share";
        ubs_mem_shm_desc_t shareDesc{};
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        // 第一次创建成功
        IT_LOG_INFO << "[NameExist] Creating Share first time: name=" << name;
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        // create + attach
        IT_LOG_INFO << "Creating SHM for memid query: name=" << name;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);

        // 同名重复创建，触发 BORROW_NAME_EXIST
        IT_LOG_INFO << "[NameExist] Creating Share with duplicate name: name=" << name;
        ubs_mem_shm_desc_t shareDesc2{};
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 统一等待并获取所有BORROW_NAME_EXIST类型的fault log (faultCode=2)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0002"; }, 5000, 3); // 等待3条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0002, got "
                                  << entries.size();

    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_name_exist_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_name_exist_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_name_exist_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, "1") << "Share requestNode mismatch";
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";

}

// BorrowChipNotSupport-01: 底层芯片不支持FD/NUMA借用 触发 BORROW_CHIP_NOT_SUPPORT，校验 ubse_fault.log
void RunP1FaultLogBorrowChipNotSupport(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    // 测试FD借用不支持
    {
        const char* name = "it_p1_fl_chip_not_support_fd";
        ubs_mem_fd_desc_t fdDesc{};

        // 创建FD失败，触发 BORROW_CHIP_NOT_SUPPORT
        IT_LOG_INFO << "[BorrowChipNotSupport-01] Creating FD with chip not support: name=" << name;
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // 清理
        ret = sdk.MemFdDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 测试NUMA借用不支持
    {
        const char* name = "it_p1_fl_chip_not_support_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA失败，触发 BORROW_CHIP_NOT_SUPPORT
        IT_LOG_INFO << "[BorrowChipNotSupport-01] Creating NUMA with chip not support: name=" << name;
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // 清理
        ret = sdk.MemNumaDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 统一等待并获取所有BORROW_CHIP_NOT_SUPPORT类型的fault log (faultCode=3)
    auto entries = ItFaultLogHelper::WaitForFaultLog(faultLogPath,
        [](const FaultLogEntry& e) {return e.errorCode == "ubse_borrow_0003";}, 5000, 2); // 等待2条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 2u) << "Expected 2 fault log entries with ErrorCode=ubse_borrow_0003, got " << entries.size();

    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_chip_not_support_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_chip_not_support_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";
}

// BorrowScheduleFailed-01: 借用调度失败 触发 BORROW_SCHEDULE_FAILED，校验 ubse_fault.log
void RunP1FaultLogBorrowScheduleFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);


}

// ShareBorrowCheckFailed-01: Share借用传入的亲和的socket_id不存在 触发 SHARE_BORROW_CHECK_FAILED
void RunP1FaultLogShareBorrowCheckFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_share_br_check_failed";
        ubs_mem_shm_desc_t shareDesc{};
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        uint64_t affinity_socket_id = 0;

        // 第一次创建成功
        IT_LOG_INFO << "[NameExist] Creating Share first time: name=" << name;
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        constexpr uint64_t shmSize128M = 128ULL * 1024ULL * 1024ULL;

        // create + attach
        IT_LOG_INFO << "Creating SHM for memid query: name=" << name;
        ret = sdk.MemShmCreateWithAffinity(name, shareSize, affinity_socket_id, usrInfo, 0, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL);
    }
    
    // 等待并校验 fault log 中出现 SHARE_BORROW_CHECK_FAILED (faultCode=16)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0016"; }, 5000);

    ASSERT_FALSE(entries.empty()) << "Expected fault log entry with ErrorCode=ubse_borrow_0016 not found";
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_share_br_check_failed");
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, shareSize);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 16u); // SHARE_BORROW_CHECK_FAILED
    EXPECT_FALSE(entry.errorInfo.empty());
    EXPECT_FALSE(entry.advice.empty());
}

// ShareAttachCheckFailed-01（四节点）: Share attach不存在的共享内存 或 attach请求节点不在共享域 触发 SHARE_ATTACH_CHECK_FAILED
void RunP1FaultLogShareAttachCheckFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    // 判断节点3是否为主节点，若不是则使用节点3，若是则使用节点4
    std::string nodeId = (masterNodeId != "3") ? "3" : "4";
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    // 测试Share attach尝试attach不存在的共享内存，或请求节点不在共享域，且attach节点不是主节点
    {
        const char* name = "it_p1_fl_share_att_check_failed";
        ubs_mem_shm_desc_t shareDesc{};
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        // 创建共享内存，共享域包含节点1和节点2
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);
        
        // 节点1尝试attach不存在的共享内存，触发SHARE_ATTACH_CHECK_FAILED
        IT_LOG_INFO << "[ShareAttachCheckFailed-01] Attaching share name not exist: name=" << name;
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach("it_p1_fl_share_att_check_failed_1", nullptr, 0, &attachDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);

        auto& attachSdk = cluster.GetSdkClient(nodeId);
        // 节点3尝试attach共享内存，触发SHARE_ATTACH_CHECK_FAILED
        IT_LOG_INFO << "[ShareAttachCheckFailed-01] Attaching share from node not in region: name=" << name;
        ubs_mem_shm_desc_t* attachDesc2 = nullptr;
        ret = attachSdk.MemShmAttach(name, nullptr, 0, &attachDesc2);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验fault log中出现SHARE_ATTACH_CHECK_FAILED (faultCode=17)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0017"; }, 5000);

    // 校验日志总数
    ASSERT_EQ(entries.size(), 2u) << "Expected 2 fault log entries with ErrorCode=ubse_borrow_0017, got " << entries.size();

    // 校验节点1尝试attach不存在的共享内存，触发SHARE_ATTACH_CHECK_FAILED的日志
    const auto& entry1 = entries[0];
    EXPECT_EQ(entry1.requestName, "it_p1_fl_share_att_check_failed_1");
    EXPECT_EQ(entry1.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry1.requestSize, 0);
    EXPECT_EQ(entry1.requestNode, "1"); // attach请求共享内存不存在
    EXPECT_EQ(entry1.adviceCode, 17u); // SHARE_ATTACH_CHECK_FAILED
    EXPECT_FALSE(entry1.errorInfo.empty());
    EXPECT_FALSE(entry1.advice.empty());

    // 校验节点3/4尝试attach共享内存，触发SHARE_ATTACH_CHECK_FAILED的日志
    const auto& entry3 = entries[1];
    EXPECT_EQ(entry3.requestName, "it_p1_fl_share_att_check_failed");
    EXPECT_EQ(entry3.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry3.requestSize, 0);
    EXPECT_EQ(entry3.requestNode, nodeId); // attach请求节点为节点3/4(非主节点且不在共享域)
    EXPECT_EQ(entry3.adviceCode, 17u); // SHARE_ATTACH_CHECK_FAILED
    EXPECT_FALSE(entry3.errorInfo.empty());
    EXPECT_FALSE(entry3.advice.empty());
}

// ShareAttachAuthFailed-01: Share attach与create时用户身份不一致 触发 SHARE_ATTACH_AUTH_FAILED
void RunP1FaultLogShareAttachAuthFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);
}

// ShareAttachExist-01: Share attach请求节点重复attach 触发 SHARE_ATTACH_EXIST
void RunP1FaultLogShareAttachExist(ubse::it::infra::ItCluster& cluster);
} // namespace ubse::it::tests::mem_borrow
