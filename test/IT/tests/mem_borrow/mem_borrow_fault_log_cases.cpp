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
#include <pthread.h>
#include <unistd.h>
#include <cstdint>
#include <cstdlib>
#include <future>
#include "ubse_error.h"

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
using ubse::it::infra::ObmmStubControl;

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
        EXPECT_NE(cliInvoker.CreateMemoryNuma(createInfo, name, "4M", "1/1/1-2/1/1"), UBS_SUCCESS);
    }

    // 等待并校验 fault log 中出现 BORROW_CHECK_FAILED (faultCode=1)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0001"; });

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

// BorrowNameExist-01: FD/NUMA/Share 同名重复创建 触发 BORROW_NAME_EXIST，校验 ubse_fault.log
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
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 统一等待并获取所有BORROW_NAME_EXIST类型的fault log (faultCode=2)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0002"; }, 3); // 等待3条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0002, got "
                                  << entries.size();

    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_name_exist_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 1u); //CHECK_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_name_exist_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 1u); //CHECK_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_name_exist_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, "1") << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 1u); //CHECK_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";
}

// BorrowChipNotSupport-01: 底层芯片不支持FD/NUMA借用 触发 BORROW_CHIP_NOT_SUPPORT，校验 ubse_fault.log
void RunP1FaultLogBorrowChipNotSupport(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_chip_not_support_fd";
        ubs_mem_fd_desc_t fdDesc{};

        // 创建FD失败，触发 BORROW_CHIP_NOT_SUPPORT
        IT_LOG_INFO << "[BorrowChipNotSupport-01] Creating FD with chip not support: name=" << name;
        auto ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        ubse::it::infra::ItMemCreateInfo createInfo;
        EXPECT_NE(cliInvoker.CreateMemoryFd(createInfo, name, "4M"), UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_chip_not_support_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA失败，触发 BORROW_CHIP_NOT_SUPPORT
        IT_LOG_INFO << "[BorrowChipNotSupport-01] Creating NUMA with chip not support: name=" << name;
        auto ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        ubse::it::infra::ItMemCreateInfo createInfo;
        EXPECT_NE(cliInvoker.CreateMemoryNuma(createInfo, name, "4M"), UBS_SUCCESS);
    }

    // 统一等待并获取所有BORROW_CHIP_NOT_SUPPORT类型的fault log (faultCode=3)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0003"; }, 4); // 等待4条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 4u) << "Expected 4 fault log entries with ErrorCode=ubse_borrow_0003, got "
                                  << entries.size();

    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_chip_not_support_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 11u); //UB_FEATURE_NOT_SUPPORTED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验FD类型日志
    const auto& fdEntry1 = entries[1];
    EXPECT_EQ(fdEntry1.requestName, "it_p1_fl_chip_not_support_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry1.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry1.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry1.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry1.adviceCode, 11u); //UB_FEATURE_NOT_SUPPORTED
    EXPECT_FALSE(fdEntry1.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry1.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[2];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_chip_not_support_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 11u); //UB_FEATURE_NOT_SUPPORTED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    const auto& numaEntry1 = entries[3];
    EXPECT_EQ(numaEntry1.requestName, "it_p1_fl_chip_not_support_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry1.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry1.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry1.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry1.adviceCode, 11u); //UB_FEATURE_NOT_SUPPORTED
    EXPECT_FALSE(numaEntry1.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry1.advice.empty()) << "NUMA advice is empty";
}

// BorrowScheduleFailed-01: FD/NUMA/Share 借用调度失败 触发 BORROW_SCHEDULE_FAILED，校验 ubse_fault.log
void RunP1FaultLogBorrowScheduleFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& sdk2 = cluster.GetSdkClient("2");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name_p = "it_p1_fl_br_schedule_failed";
    ubs_mem_numa_desc_t numaDesc{};

    // 创建NUMA失败，触发 BORROW_SCHEDULE_FAILED
    IT_LOG_INFO << "[BorrowScheduleFailed-01] Creating NUMA with schedule failed: name=" << name_p;
    ret = sdk.MemNumaCreate(name_p, numaSize, MEM_DISTANCE_L0, &numaDesc);
    EXPECT_IT_OK(ret);

    {
        const char* name = "it_p1_fl_br_schedule_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};

        // 创建FD失败，触发 BORROW_SCHEDULE_FAILED
        IT_LOG_INFO << "[BorrowScheduleFailed-01] Creating FD with schedule failed: name=" << name;
        ret = sdk2.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE);
    }

    {
        const char* name = "it_p1_fl_br_schedule_failed_numa";

        // 创建NUMA失败，触发 BORROW_SCHEDULE_FAILED
        IT_LOG_INFO << "[BorrowScheduleFailed-01] Creating NUMA with schedule failed: name=" << name;
        ret = sdk2.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE);
    }

    {
        const char* name = "it_p1_fl_br_schedule_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        // 创建Share失败，触发 BORROW_SCHEDULE_FAILED
        IT_LOG_INFO << "[BorrowScheduleFailed-01] Creating Share with schedule failed: name=" << name;
        ubs_mem_lender_t lender{.lender_size = shareSize, .slot_id = 1, .socket_id = 1, .numa_id = 1, .port_id = 0};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_ALLOCATE);
    }

    // 清理
    ret = sdk.MemNumaDelete(name_p);
    ASSERT_IT_OK(ret);

    // 统一等待并获取所有BORROW_SCHEDULE_FAILED类型的fault log (faultCode=4)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0004"; }, 3); // 等待3条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0004, got "
                                  << entries.size();

    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_br_schedule_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "2") << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 3u); //SCHEDULE_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_schedule_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "2") << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 3u); //SCHEDULE_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_schedule_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, "1") << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 3u); //SCHEDULE_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";
}

// BorrowMasterToExSendFailed-01: 主节点向导出节点发送借用请求失败 触发 BORROW_MASTER_TO_EX_SEND_FAILED
void RunP1FaultLogBorrowMasterToExSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "2" == masterNodeId ? 1 : 2;
    auto exportNodeIdStr = std::to_string(exportNodeId);
    auto importNodeId = masterNodeId;
    auto& comStubControl = cluster.GetComStubControl(masterNodeId);
    comStubControl.SetComSendFailed(exportNodeIdStr, true);

    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_br_master_to_export_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        IT_LOG_INFO << "Creating FD: name=" << name;
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_master_to_export_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        IT_LOG_INFO << "Creating NUMA: name=" << name;
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }

    comStubControl.SetComSendFailed(exportNodeIdStr, false);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0005"; }, 2); // 等待2条日志

    ASSERT_EQ(entries.size(), 2u) << "Expected 2 fault log entry with ErrorCode=ubse_borrow_0005, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_br_master_to_export_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(entry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(entry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(entry.requestNode, importNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(entry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(entry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_master_to_export_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, importNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";
}

// BorrowMasterToImSendFailed-01: 主节点向导入节点发送借用请求失败 触发 BORROW_MASTER_TO_IM_SEND_FAILED
void RunP1FaultLogBorrowMasterToImSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "1" == masterNodeId ? 1 : 2;
    auto importNodeId = "1" == masterNodeId ? "2" : "1";

    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(masterNodeId); // 发送方
    uint32_t count[2] = {6, 0};                                     // OP_SYNC_SEND 失败6次，OP_ASYNC_SEND 不注入

    {
        const char* name = "it_p1_fl_br_master_to_import_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        IT_LOG_INFO << "Creating FD with lender: name=" << name;
        // 在 master 节点上注入故障：向 importNodeId 发送消息失败 6 次，之后自动恢复
        comStubControl.SetComFault(0x1, UBSE_COM_ERROR_SYNC_CALL_FAIL, count, importNodeId);
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_master_to_import_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        IT_LOG_INFO << "Creating NUMA with lender: name=" << name;
        // 在 master 节点上注入故障：向 importNodeId 发送消息失败 6 次，之后自动恢复
        comStubControl.SetComFault(0x1, UBSE_COM_ERROR_SYNC_CALL_FAIL, count, importNodeId);
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_master_to_import_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        IT_LOG_INFO << "Creating Share with lender: name=" << name;
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        comStubControl.SetComFault(0x1, UBSE_COM_ERROR_SYNC_CALL_FAIL, count, importNodeId);
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0006"; }, 3); // 等待3条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entry with ErrorCode=ubse_borrow_0006, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_br_master_to_import_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(entry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(entry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(entry.requestNode, importNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(entry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(entry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_master_to_import_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, importNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_master_to_import_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, 0) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, importNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";
}

// BorrowMasterToReqSendFailed-01: 主节点向请求节点发送借用请求失败 触发 BORROW_MASTER_TO_REQ_SEND_FAILED
void RunP1FaultLogBorrowMasterToReqSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "1" == masterNodeId ? 1 : 2;
    auto importNodeId = "1" == masterNodeId ? "2" : "1";

    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(masterNodeId);
    auto& obmmStubControl = cluster.GetObmmStubControl(importNodeId);
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_IMPORT, 100);

    {
        const char* name = "it_p1_fl_br_master_to_req_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        IT_LOG_INFO << "Creating FD with lender: name=" << name;
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) {
                p.set_value(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc));
            },
            std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(importNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
    }
    {
        const char* name = "it_p1_fl_br_master_to_req_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        IT_LOG_INFO << "Creating NUMA with lender: name=" << name;
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) { p.set_value(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc)); },
            std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(importNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
    }
    {
        const char* name = "it_p1_fl_br_master_to_req_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        IT_LOG_INFO << "Creating Share with lender: name=" << name;
        comStubControl.SetComSendFailed(importNodeId, true);
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) { p.set_value(sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender)); },
            std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
    }
    {
        const char* name = "it_p1_fl_br_master_to_req_send_failed_attach";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Attaching SHM: name=" << name;
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemShmAttach(name, nullptr, 0, &attachDesc)); },
                      std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(importNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }
    }
    {
        // 清理创建的资源
        ret = sdk.MemShmDetach("it_p1_fl_br_master_to_req_send_failed_attach");
        EXPECT_IT_OK(ret);
        ret = sdk.MemShmDelete("it_p1_fl_br_master_to_req_send_failed_attach");
        EXPECT_IT_OK(ret);
        ret = sdk.MemShmDelete("it_p1_fl_br_master_to_req_send_failed_share");
        EXPECT_IT_OK(ret);
        ret = sdk.MemNumaDelete("it_p1_fl_br_master_to_req_send_failed_numa");
        EXPECT_IT_OK(ret);
        ret = sdk.MemFdDelete("it_p1_fl_br_master_to_req_send_failed_fd");
        EXPECT_IT_OK(ret);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0007"; }, 4); // 等待4条日志

    ASSERT_EQ(entries.size(), 4u) << "Expected 4 fault log entries with ErrorCode=ubse_borrow_0007, got "
                                  << entries.size();

    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_br_master_to_req_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, importNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_master_to_req_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, importNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_master_to_req_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, importNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";

    const auto& attachEntry = entries[3];
    EXPECT_EQ(attachEntry.requestName, "it_p1_fl_br_master_to_req_send_failed_attach") << "Attach requestName mismatch";
    EXPECT_EQ(attachEntry.borrowType, "SHARE_BORROW") << "Attach borrowType mismatch";
    EXPECT_EQ(attachEntry.requestSize, shareSize) << "Attach requestSize mismatch";
    EXPECT_EQ(attachEntry.requestNode, importNodeId) << "Attach requestNode mismatch";
    EXPECT_EQ(attachEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(attachEntry.errorInfo.empty()) << "Attach errorInfo is empty";
    EXPECT_FALSE(attachEntry.advice.empty()) << "Attach advice is empty";
}

// BorrowExportSendFailed-01: 导出节点向主节点发送借用响应失败 触发 BORROW_EXPORT_SEND_FAILED
void RunP1FaultLogBorrowExportSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto importNodeId = masterNodeId;
    uint32_t exportNodeId = "1" == masterNodeId ? 2 : 1;
    auto exportNodeIdStr = std::to_string(exportNodeId);
    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(exportNodeIdStr).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(exportNodeIdStr);
    comStubControl.SetComSendFailed(masterNodeId, true);
    comStubControl.SetMemApiWaitTimeOut(1);

    {
        const char* name = "it_p1_fl_br_export_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        IT_LOG_INFO << "Creating FD with lender: name=" << name;
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) {
                p.set_value(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc));
            },
            std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
        } else {
            ret = future.get();
        }
        t.join();
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_br_export_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        IT_LOG_INFO << "Creating NUMA with lender: name=" << name;
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) { p.set_value(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc)); },
            std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
        } else {
            ret = future.get();
        }
        t.join();
        EXPECT_NE(ret, UBS_SUCCESS);
    }

    comStubControl.SetComSendFailed(masterNodeId, false);
    comStubControl.RestoreMemApiWaitTimeOut();

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0008"; }, 2); // 等待2条日志

    ASSERT_EQ(entries.size(), 2u) << "Expected 2 fault log entry with ErrorCode=ubse_borrow_0008, got "
                                  << entries.size();
    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_br_export_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(entry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(entry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(entry.requestNode, importNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(entry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(entry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "FD advice is empty";
    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_export_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, importNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 后置，当前情况下重启主节点和导出节点，资源自动清理
    cluster.RestartNode(masterNodeId, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(masterNodeId));
    cluster.RestartNode(exportNodeIdStr, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(exportNodeIdStr));
}

// BorrowImportSendFailed-01: 导入节点向主节点发送借用响应失败 触发 BORROW_IMPORT_SEND_FAILED
void RunP1FaultLogBorrowImportSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "1" == masterNodeId ? 1 : 2;
    auto exportNodeIdStr = std::to_string(exportNodeId);
    auto importNodeId = "1" == masterNodeId ? "2" : "1";
    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(importNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(importNodeId);
    comStubControl.SetMemApiWaitTimeOut(1);

    auto& obmmStubControl = cluster.GetObmmStubControl(importNodeId);
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_IMPORT, 100);

    {
        const char* name = "it_p1_fl_br_import_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        IT_LOG_INFO << "Creating FD: name=" << name;
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) {
                p.set_value(sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc));
            },
            std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(masterNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        t.join();
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_br_import_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        IT_LOG_INFO << "Creating NUMA: name=" << name;
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t(
            [&](std::promise<int> p) { p.set_value(sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc)); },
            std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(masterNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        t.join();
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_br_import_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);
        IT_LOG_INFO << "Attaching SHM: name=" << name;
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemShmAttach(name, nullptr, 0, &attachDesc)); },
                      std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(masterNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }
    }
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_IMPORT);
    comStubControl.RestoreMemApiWaitTimeOut();

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0009"; }, 3); // 等待3条日志

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entry with ErrorCode=ubse_borrow_0009, got "
                                  << entries.size();
    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_br_import_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(entry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(entry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(entry.requestNode, importNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(entry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(entry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "FD advice is empty";
    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_import_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, importNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";
    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_import_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, importNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";

    // 后置，当前情况下重启主节点和导入节点，资源自动清理
    cluster.RestartNode(masterNodeId, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(masterNodeId));
    cluster.RestartNode(importNodeId, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(importNodeId));
}

// BorrowReqSendFailed-01: 请求节点向主节点发送借用请求失败 触发 BORROW_REQ_SEND_FAILED
void RunP1FaultLogBorrowReqSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto exportNodeId = masterNodeId;
    auto reqNodeId = "1" == masterNodeId ? "2" : "1";
    auto& comStubControl = cluster.GetComStubControl(reqNodeId);
    comStubControl.SetComSendFailed(masterNodeId, true);

    auto& sdk = cluster.GetSdkClient(reqNodeId);
    auto& cliInvoker = cluster.GetCliInvoker(reqNodeId);
    auto faultLogPath = cluster.GetNode(reqNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);
    {
        const char* name = "it_p1_fl_br_req_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};

        // 创建FD失败，触发 BORROW_REQ_SEND_FAILED
        IT_LOG_INFO << "[BorrowReqSendFailed-01] Creating FD with req send failed: name=" << name;
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        ubse::it::infra::ItMemCreateInfo createInfo;
        EXPECT_NE(cliInvoker.CreateMemoryFd(createInfo, name, "4M"), UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_br_req_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA失败，触发 BORROW_REQ_SEND_FAILED
        IT_LOG_INFO << "[BorrowReqSendFailed-01] Creating NUMA with req send failed: name=" << name;
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        ubse::it::infra::ItMemCreateInfo createInfo;
        EXPECT_NE(cliInvoker.CreateMemoryNuma(createInfo, name, "4M"), UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_br_req_send_failed_share";
        // 创建Share失败，触发 BORROW_REQ_SEND_FAILED
        IT_LOG_INFO << "[BorrowReqSendFailed-01] Creating Share with req send failed: name=" << name;
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        ubse::it::infra::ItMemCreateInfo createInfo;
        EXPECT_NE(cliInvoker.CreateMemoryShare(createInfo, name, "4M", "1,2"), UBS_SUCCESS);

        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        ubse::it::infra::ItMemCreateInfo attachInfo;
        EXPECT_NE(cliInvoker.AttachMemory(attachInfo, name), UBS_SUCCESS);
    }
    comStubControl.SetComSendFailed(masterNodeId, false);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0010"; }, 8); // 等待8条日志

    ASSERT_EQ(entries.size(), 8u) << "Expected 8 fault log entry with ErrorCode=ubse_borrow_0010, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_br_req_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, reqNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    const auto& fdEntry1 = entries[1];
    EXPECT_EQ(fdEntry1.requestName, "it_p1_fl_br_req_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry1.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry1.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry1.requestNode, reqNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry1.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry1.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[2];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_req_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, reqNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    const auto& numaEntry1 = entries[3];
    EXPECT_EQ(numaEntry1.requestName, "it_p1_fl_br_req_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry1.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry1.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry1.requestNode, reqNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry1.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry1.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[4];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_req_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, reqNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";

    const auto& shareEntry1 = entries[5];
    EXPECT_EQ(shareEntry1.requestName, "it_p1_fl_br_req_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry1.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry1.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry1.requestNode, reqNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry1.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry1.advice.empty()) << "Share advice is empty";

    // 校验Share Attach类型日志
    const auto& shareAttachEntry = entries[6];
    EXPECT_EQ(shareAttachEntry.requestName, "it_p1_fl_br_req_send_failed_share") << "Share Attach requestName mismatch";
    EXPECT_EQ(shareAttachEntry.borrowType, "SHARE_BORROW") << "Share Attach borrowType mismatch";
    EXPECT_EQ(shareAttachEntry.requestSize, 0) << "Share Attach requestSize mismatch";
    EXPECT_EQ(shareAttachEntry.requestNode, reqNodeId) << "Share Attach requestNode mismatch";
    EXPECT_EQ(shareAttachEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareAttachEntry.errorInfo.empty()) << "Share Attach errorInfo is empty";
    EXPECT_FALSE(shareAttachEntry.advice.empty()) << "Share Attach advice is empty";

    const auto& shareAttachEntry1 = entries[7];
    EXPECT_EQ(shareAttachEntry1.requestName, "it_p1_fl_br_req_send_failed_share")
        << "Share Attach requestName mismatch";
    EXPECT_EQ(shareAttachEntry1.borrowType, "SHARE_BORROW") << "Share Attach borrowType mismatch";
    EXPECT_EQ(shareAttachEntry1.requestSize, 0) << "Share Attach requestSize mismatch";
    EXPECT_EQ(shareAttachEntry1.requestNode, reqNodeId) << "Share Attach requestNode mismatch";
    EXPECT_EQ(shareAttachEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareAttachEntry1.errorInfo.empty()) << "Share Attach errorInfo is empty";
    EXPECT_FALSE(shareAttachEntry1.advice.empty()) << "Share Attach advice is empty";
}

// BorrowObmmExportFailed-01: OBMM导出失败 触发 BORROW_OBMM_EXPORT_FAILED
void RunP1FaultLogBorrowObmmExportFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& obmmStubControl = cluster.GetObmmStubControl("2");
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_EXPORT, true);

    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("2").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_br_obmm_export_failed";
        ubs_mem_fd_desc_t fdDesc{};

        // 创建FD失败，触发 BORROW_OBMM_EXPORT_FAILED
        IT_LOG_INFO << "[BorrowObmmExportFailed-01] Creating FD with OBMM export failed: name=" << name;
        auto ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_obmm_export_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA失败，触发 BORROW_OBMM_EXPORT_FAILED
        IT_LOG_INFO << "[BorrowObmmExportFailed-01] Creating NUMA with OBMM export failed: name=" << name;
        auto ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_obmm_export_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        // 创建Share失败，触发 BORROW_OBMM_EXPORT_FAILED
        IT_LOG_INFO << "[BorrowObmmExportFailed-01] Creating Share with OBMM export failed: name=" << name;
        ubs_mem_lender_t lender{
            .lender_size = shareSize, .slot_id = 2, .socket_id = UINT32_MAX, .numa_id = 0, .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        auto ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_ERROR(ret, UBSE_MMI_OBMM_OP_FAILED);
    }

    obmmStubControl.SetOpFailed(ObmmStubControl::OP_EXPORT, false);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0013"; }, 3); // 等待3条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0013, got "
                                  << entries.size();
    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_br_obmm_export_failed") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_obmm_export_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_obmm_export_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, "1") << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";
}

// BorrowObmmImportFailed-01: OBMM导入失败 触发 BORROW_OBMM_IMPORT_FAILED
void RunP1FaultLogBorrowObmmImportFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& obmmStubControl = cluster.GetObmmStubControl("1");
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_IMPORT, true);

    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_br_obmm_import_failed";
        ubs_mem_fd_desc_t fdDesc{};

        // 创建FD失败，触发 BORROW_OBMM_IMPORT_FAILED
        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating FD with OBMM import failed: name=" << name;
        auto ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_obmm_import_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA失败，触发 BORROW_OBMM_IMPORT_FAILED
        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating NUMA with OBMM import failed: name=" << name;
        auto ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_br_obmm_import_failed_share";
        // 创建Share成功
        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating SHM: name=" << name;
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{
            .lender_size = shareSize, .slot_id = 2, .socket_id = UINT32_MAX, .numa_id = 0, .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        auto ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        //导入失败，触发 BORROW_OBMM_IMPORT_FAILED
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_IMPORT, false);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0014"; }, 3); // 等待3条日志

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0014, got "
                                  << entries.size();
    // 校验FD类型日志
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_br_obmm_import_failed") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, fdSize) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, "1") << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_br_obmm_import_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, numaSize) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, "1") << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_br_obmm_import_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, shareSize) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, "1") << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";
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
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        uint64_t affinity_socket_id = 0;

        // 第一次创建成功
        IT_LOG_INFO << "[NameExist] Creating Share first time: name=" << name;
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        // create + attach
        IT_LOG_INFO << "Creating SHM for memid query: name=" << name;
        ret = sdk.MemShmCreateWithAffinity(name, shareSize, affinity_socket_id, usrInfo, 0, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL);
    }

    // 等待并校验 fault log 中出现 SHARE_BORROW_CHECK_FAILED (faultCode=16)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0016"; });

    ASSERT_FALSE(entries.empty()) << "Expected fault log entry with ErrorCode=ubse_borrow_0016 not found";
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_share_br_check_failed");
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, shareSize);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 1u); // CHECK_FAILED
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
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        auto& attachSdk = cluster.GetSdkClient(nodeId);
        // 节点3尝试attach共享内存，触发SHARE_ATTACH_CHECK_FAILED
        IT_LOG_INFO << "[ShareAttachCheckFailed-01] Attaching share from node not in region: name=" << name;
        ubs_mem_shm_desc_t* attachDesc2 = nullptr;
        ret = attachSdk.MemShmAttach(name, nullptr, 0, &attachDesc2);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
        if (attachDesc2 != nullptr) {
            free(attachDesc2);
        }

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验fault log中出现SHARE_ATTACH_CHECK_FAILED (faultCode=17)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0017"; }, 2);

    // 校验日志总数
    ASSERT_EQ(entries.size(), 2u) << "Expected 2 fault log entries with ErrorCode=ubse_borrow_0017, got "
                                  << entries.size();

    // 校验节点1尝试attach不存在的共享内存，触发SHARE_ATTACH_CHECK_FAILED的日志
    const auto& entry1 = entries[0];
    EXPECT_EQ(entry1.requestName, "it_p1_fl_share_att_check_failed_1");
    EXPECT_EQ(entry1.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry1.requestSize, 0);
    EXPECT_EQ(entry1.requestNode, "1"); // attach请求共享内存不存在
    EXPECT_EQ(entry1.adviceCode, 1u);   // CHECK_FAILED
    EXPECT_FALSE(entry1.errorInfo.empty());
    EXPECT_FALSE(entry1.advice.empty());

    // 校验节点3/4尝试attach共享内存，触发SHARE_ATTACH_CHECK_FAILED的日志
    const auto& entry3 = entries[1];
    EXPECT_EQ(entry3.requestName, "it_p1_fl_share_att_check_failed");
    EXPECT_EQ(entry3.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry3.requestSize, 0);
    EXPECT_EQ(entry3.requestNode, nodeId); // attach请求节点为节点3/4(非主节点且不在共享域)
    EXPECT_EQ(entry3.adviceCode, 1u);      // CHECK_FAILED
    EXPECT_FALSE(entry3.errorInfo.empty());
    EXPECT_FALSE(entry3.advice.empty());
}

void RunP1FaultLogShareAttachAuthFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_share_att_auth_failed";
    {
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        // 节点1创建共享内存
        IT_LOG_INFO << "[ShareAttachAuthFailed-01] Creating SHM with node 1: name=" << name;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);

        // 节点2尝试attach，用户身份与创建时不一致，触发SHARE_ATTACH_AUTH_FAILED
        auto& attachSdk = cluster.GetSdkClient("2");
        IT_LOG_INFO << "[ShareAttachAuthFailed-01] Attaching SHM with node 2 (different identity): name=" << name;
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = attachSdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_AUTH_FAILED);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验 fault log 中出现 SHARE_ATTACH_AUTH_FAILED (faultCode=18)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0018"; });

    ASSERT_FALSE(entries.empty()) << "Expected fault log entry with ErrorCode=ubse_borrow_0018 not found";
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, shareSize);
    EXPECT_EQ(entry.requestNode, "2");
    EXPECT_EQ(entry.adviceCode, 10u); // NO_OP_PERMISSION
    EXPECT_FALSE(entry.errorInfo.empty());
    EXPECT_FALSE(entry.advice.empty());
}

// ShareAttachExist-01: Share attach请求节点重复attach 触发 SHARE_ATTACH_EXIST
void RunP1FaultLogShareAttachExist(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_share_att_exist";
    {
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        // 节点1创建共享内存
        IT_LOG_INFO << "[ShareAttachExist-01] Creating SHM with node 1: name=" << name;
        auto ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);

        // 节点1尝试attach
        IT_LOG_INFO << "[ShareAttachExist-01] Attaching SHM : name=" << name;
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_OK(ret);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        // 节点1再次attach，触发SHARE_ATTACH_EXIST
        IT_LOG_INFO << "[ShareAttachExist-01] Attaching SHM duplicated: name=" << name;
        ubs_mem_shm_desc_t* attachDesc2 = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc2);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);
        if (attachDesc2 != nullptr) {
            free(attachDesc2);
        }

        // 清理
        ret = sdk.MemShmDetach(name);
        ASSERT_IT_OK(ret);
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验 fault log 中出现 SHARE_ATTACH_EXIST (faultCode=19)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0019"; });

    // 校验日志总数
    ASSERT_EQ(entries.size(), 1u) << "Expected 1 fault log entry with ErrorCode=ubse_borrow_0019, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, 0);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 5u); // RESOURCE_EXIST
    EXPECT_FALSE(entry.errorInfo.empty());
    EXPECT_FALSE(entry.advice.empty());
}

// ShareChipNotSupported-01: 底层芯片不支持Shared attach 触发 SHARED_CHIP_NOT_SUPPORTED
void RunP1FaultLogShareChipNotSupported(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_share_chip_not_supported";

        // SDK尝试attach共享内存
        IT_LOG_INFO << "[ShareAttachAuthFailed-01] Attaching SHM name=" << name;
        auto ret = sdk.MemShmAttach(name, nullptr, 0, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试attach共享内存
        IT_LOG_INFO << "[ShareChipNotSupported-01] Attaching SHM name=" << name;
        ubse::it::infra::ItMemCreateInfo attachInfo;
        EXPECT_NE(cliInvoker.AttachMemory(attachInfo, name), UBS_SUCCESS);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0020"; }, 2);

    ASSERT_EQ(entries.size(), 2u) << "Expected 2 fault log entries with ErrorCode=ubse_borrow_0020, got "
                                  << entries.size();

    // 校验SDK创建共享内存触发SHARED_CHIP_NOT_SUPPORTED的日志
    const auto& sdkEntry = entries[0];
    EXPECT_EQ(sdkEntry.requestName, "it_p1_fl_share_chip_not_supported");
    EXPECT_EQ(sdkEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(sdkEntry.requestSize, 0);
    EXPECT_EQ(sdkEntry.requestNode, "1");
    EXPECT_EQ(sdkEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(sdkEntry.errorInfo.empty());
    EXPECT_FALSE(sdkEntry.advice.empty());

    // 校验CLI创建共享内存触发SHARED_CHIP_NOT_SUPPORTED的日志
    const auto& cliEntry = entries[1];
    EXPECT_EQ(cliEntry.requestName, "it_p1_fl_share_chip_not_supported");
    EXPECT_EQ(cliEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(cliEntry.requestSize, 0);
    EXPECT_EQ(cliEntry.requestNode, "1");
    EXPECT_EQ(cliEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(cliEntry.errorInfo.empty());
    EXPECT_FALSE(cliEntry.advice.empty());
}

// ShareChipModeNotSupported-01: 底层芯片模式不支持Share借用模式 触发 SHARED_CHIP_MODE_NOT_SUPPORTED
void RunP1FaultLogShareChipModeNotSupported(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_share_chip_mode_not_supported";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        // SDK创建共享内存, NC模式不支持Share借用模式
        IT_LOG_INFO << "[ShareChipModeNotSupported-01] Creating SHM name=" << name;
        auto ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        //SDK创建共享内存, CC模式不支持Share借用模式
        IT_LOG_INFO << "[ShareChipModeNotSupported-01] Creating SHM with mode CC: name=" << name;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0x4, &region, nullptr);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试创建共享内存
        IT_LOG_INFO << "[ShareChipModeNotSupported-01] Creating SHM name=" << name;
        ubse::it::infra::ItMemCreateInfo shareCreateInfo;
        EXPECT_NE(cliInvoker.CreateMemoryShare(shareCreateInfo, name, "4M", "1,2"), UBS_SUCCESS);
    }

    // 等待并校验 fault log 中出现 SHARED_CHIP_MODE_NOT_SUPPORTED (faultCode=21)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0021"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0021, got "
                                  << entries.size();

    // 校验SDK创建共享内存触发SHARED_CHIP_MODE_NOT_SUPPORTED的日志
    const auto& sdkEntry = entries[0];
    EXPECT_EQ(sdkEntry.requestName, "it_p1_fl_share_chip_mode_not_supported");
    EXPECT_EQ(sdkEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(sdkEntry.requestSize, shareSize);
    EXPECT_EQ(sdkEntry.requestNode, "1");
    EXPECT_EQ(sdkEntry.adviceCode, 11u); // CHIP_MODE_NOT_SUPPORTED
    EXPECT_FALSE(sdkEntry.errorInfo.empty());
    EXPECT_FALSE(sdkEntry.advice.empty());

    const auto& sdkEntry1 = entries[1];
    EXPECT_EQ(sdkEntry1.requestName, "it_p1_fl_share_chip_mode_not_supported");
    EXPECT_EQ(sdkEntry1.borrowType, "SHARE_BORROW");
    EXPECT_EQ(sdkEntry1.requestSize, shareSize);
    EXPECT_EQ(sdkEntry1.requestNode, "1");
    EXPECT_EQ(sdkEntry1.adviceCode, 11u); // CHIP_MODE_NOT_SUPPORTED
    EXPECT_FALSE(sdkEntry1.errorInfo.empty());
    EXPECT_FALSE(sdkEntry1.advice.empty());

    // 校验CLI创建共享内存触发SHARED_CHIP_MODE_NOT_SUPPORTED的日志
    const auto& cliEntry = entries[2];
    EXPECT_EQ(cliEntry.requestName, "it_p1_fl_share_chip_mode_not_supported");
    EXPECT_EQ(cliEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(cliEntry.requestSize, shareSize);
    EXPECT_EQ(cliEntry.requestNode, "1");
    EXPECT_EQ(cliEntry.adviceCode, 11u); // CHIP_MODE_NOT_SUPPORTED
    EXPECT_FALSE(cliEntry.errorInfo.empty());
    EXPECT_FALSE(cliEntry.advice.empty());
}

// ReturnNameNotExist-01:  FD/NUMA/Share 归还不存在的共享内存 触发 RETURN_NAME_NOT_EXIST
void RunP1FaultLogReturnNameNotExist(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_ret_name_not_exist_fd";
        // 节点1尝试归还不存在的共享内存，触发RETURN_NAME_NOT_EXIST
        IT_LOG_INFO << "[ReturnNameNotExist-01] Returning FD name not exist: name=" << name;
        ret = sdk.MemFdDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    }
    {
        const char* name = "it_p1_fl_ret_name_not_exist_numa";
        // 节点1尝试归还不存在的共享内存，触发RETURN_NAME_NOT_EXIST
        IT_LOG_INFO << "[ReturnNameNotExist-01] Returning NUMA name not exist: name=" << name;
        ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    }
    {
        const char* name = "it_p1_fl_ret_name_not_exist_share";
        // 节点1尝试归还不存在的共享内存，触发RETURN_NAME_NOT_EXIST
        IT_LOG_INFO << "[ReturnNameNotExist-01] Returning SHM name not exist: name=" << name;
        ret = sdk.MemShmDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_NOT_EXIST);
    }

    // 等待并校验 fault log 中出现 RETURN_NAME_NOT_EXIST (faultCode=22)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0022"; }, 3);

    // 校验日志总数
    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0022, got "
                                  << entries.size(); // 校验日志内容

    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_name_not_exist_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, "1");
    EXPECT_EQ(fdEntry.adviceCode, 8u); // RESOURCE_NOT_EXIST
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_name_not_exist_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, "1");
    EXPECT_EQ(numaEntry.adviceCode, 8u); // RESOURCE_NOT_EXIST
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_name_not_exist_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, "1");
    EXPECT_EQ(shareEntry.adviceCode, 8u); // RESOURCE_NOT_EXIST
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());
}

//ReturnChipNotSupported-01: 底层芯片不支持FD/NUMA归还 触发 RETURN_CHIP_NOT_SUPPORTED
void RunP1FaultLogReturnChipNotSupported(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_ret_chip_not_supported_fd";
        // SDK尝试归还不存在的共享内存，触发RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ReturnChipNotSupported-01] Returning FD name not exist: name=" << name;
        auto ret = sdk.MemFdDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试归还不存在的共享内存，触发RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ReturnChipNotSupported-01] Returning FD name not exist: name=" << name;
        EXPECT_NE(cliInvoker.DeleteMemory(name, "fd"), UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_chip_not_supported_numa";
        // SDK尝试归还不存在的共享内存，触发RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ReturnChipNotSupported-01] Returning NUMA name not exist: name=" << name;
        auto ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试归还不存在的共享内存，触发RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ReturnChipNotSupported-01] Returning NUMA name not exist: name=" << name;
        EXPECT_NE(cliInvoker.DeleteMemory(name, "numa"), UBS_SUCCESS);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0023"; }, 4);

    ASSERT_EQ(entries.size(), 4u) << "Expected 4 fault log entries with ErrorCode=ubse_borrow_0023, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_chip_not_supported_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, "1");
    EXPECT_EQ(fdEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& cliFdEntry = entries[1];
    EXPECT_EQ(cliFdEntry.requestName, "it_p1_fl_ret_chip_not_supported_fd");
    EXPECT_EQ(cliFdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(cliFdEntry.requestSize, 0);
    EXPECT_EQ(cliFdEntry.requestNode, "1");
    EXPECT_EQ(cliFdEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(cliFdEntry.errorInfo.empty());
    EXPECT_FALSE(cliFdEntry.advice.empty());

    const auto& numaEntry = entries[2];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_chip_not_supported_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, "1");
    EXPECT_EQ(numaEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& cliNumaEntry = entries[3];
    EXPECT_EQ(cliNumaEntry.requestName, "it_p1_fl_ret_chip_not_supported_numa");
    EXPECT_EQ(cliNumaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(cliNumaEntry.requestSize, 0);
    EXPECT_EQ(cliNumaEntry.requestNode, "1");
    EXPECT_EQ(cliNumaEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(cliNumaEntry.errorInfo.empty());
    EXPECT_FALSE(cliNumaEntry.advice.empty());
}

// ReturnMasterToExSendFailed-01: 主节点向导出节点发送归还请求失败 触发 RETURN_MASTER_TO_EX_SEND_FAILED
void RunP1FaultLogReturnMasterToExSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "2" == masterNodeId ? 1 : 2;
    auto exportNodeIdStr = std::to_string(exportNodeId);
    auto importNodeId = masterNodeId;

    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();

    {
        const char* name = "it_p1_fl_ret_master_to_export_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        ubs_mem_lender_t lender{.lender_size = fdSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        IT_LOG_INFO << "Creating FD with lender: name=" << name;
        ret = sdk.MemFdCreateWithLender(name, nullptr, 0, &lender, 1, &fdDesc);
        EXPECT_IT_OK(ret);
    }
    {
        const char* name = "it_p1_fl_ret_master_to_export_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        ubs_mem_lender_t lender{.lender_size = numaSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        IT_LOG_INFO << "Creating NUMA with lender: name=" << name;
        ret = sdk.MemNumaCreateWithLender(name, &lender, 1, &numaDesc);
        EXPECT_IT_OK(ret);
    }
    {
        const char* name = "it_p1_fl_ret_master_to_export_send_failed_share";
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode(importNodeId).GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode(exportNodeIdStr).GetSpec().slotId;
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        IT_LOG_INFO << "Creating Share with lender: name=" << name;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);
    }

    auto& comStubControl = cluster.GetComStubControl(masterNodeId);
    comStubControl.SetComSendFailed(exportNodeIdStr, true);

    {
        ret = sdk.MemFdDelete("it_p1_fl_ret_master_to_export_send_failed_fd");
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_UNIMPORT_SUCCESS);

        ret = sdk.MemNumaDelete("it_p1_fl_ret_master_to_export_send_failed_numa");
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_UNIMPORT_SUCCESS);

        ret = sdk.MemShmDelete("it_p1_fl_ret_master_to_export_send_failed_share");
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_UNIMPORT_SUCCESS);
    }
    comStubControl.SetComSendFailed(exportNodeIdStr, false);

    {
        sdk.MemFdDelete("it_p1_fl_ret_master_to_export_send_failed_fd");
        sdk.MemNumaDelete("it_p1_fl_ret_master_to_export_send_failed_numa");
        sdk.MemShmDelete("it_p1_fl_ret_master_to_export_send_failed_share");
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0024"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0024, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_master_to_export_send_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, importNodeId);
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_master_to_export_send_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, importNodeId);
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_master_to_export_send_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, importNodeId);
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());
}

// ReturnMasterToImSendFailed-01: 主节点向导入节点发送归还请求失败 触发 RETURN_MASTER_TO_IM_SEND_FAILED
void RunP1FaultLogReturnMasterToImSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "1" == masterNodeId ? 1 : 2;
    auto importNodeId = "1" == masterNodeId ? "2" : "1";

    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(masterNodeId); // 发送方
    uint32_t count[2] = {6, 0};                                     // OP_SYNC_SEND 失败6次，OP_ASYNC_SEND 不注入

    {
        const char* name = "it_p1_fl_ret_master_to_import_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting FD: name=" << name;
        // 在 master 节点上注入故障：向 importNodeId 发送消息失败 6 次，之后自动恢复
        comStubControl.SetComFault(0x1, UBSE_COM_ERROR_SYNC_CALL_FAIL, count, importNodeId);

        ret = sdk.MemFdDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_ret_master_to_import_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting NUMA: name=" << name;
        // 在 master 节点上注入故障：向 importNodeId 发送消息失败 6 次，之后自动恢复
        comStubControl.SetComFault(0x1, UBSE_COM_ERROR_SYNC_CALL_FAIL, count, importNodeId);

        ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_ret_master_to_import_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        IT_LOG_INFO << "Creating Share with lender: name=" << name;
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_OK(ret);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        IT_LOG_INFO << "Detaching Share: name=" << name;
        // 在 master 节点上注入故障：向 importNodeId 发送消息失败 6 次，之后自动恢复
        comStubControl.SetComFault(0x1, UBSE_COM_ERROR_SYNC_CALL_FAIL, count, importNodeId);

        ret = sdk.MemShmDetach(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    comStubControl.RestoreComFault();
    {
        ret = sdk.MemFdDelete("it_p1_fl_ret_master_to_import_send_failed_fd");
        EXPECT_IT_OK(ret);
        ret = sdk.MemNumaDelete("it_p1_fl_ret_master_to_import_send_failed_numa");
        EXPECT_IT_OK(ret);
        ret = sdk.MemShmDetach("it_p1_fl_ret_master_to_import_send_failed_share");
        EXPECT_IT_OK(ret);
        ret = sdk.MemShmDelete("it_p1_fl_ret_master_to_import_send_failed_share");
        EXPECT_IT_OK(ret);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0025"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0025, got "
                                  << entries.size();
    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_master_to_import_send_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, importNodeId);
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_master_to_import_send_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, importNodeId);
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_master_to_import_send_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, importNodeId);
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());
}

// ReturnMasterToReqSendFailed-01: 主节点向请求节点发送归还请求失败 触发 RETURN_MASTER_TO_REQ_SEND_FAILED
void RunP1FaultLogReturnMasterToReqSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "1" == masterNodeId ? 1 : 2;
    auto importNodeId = "1" == masterNodeId ? "2" : "1";

    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(masterNodeId);
    auto& obmmStubControl = cluster.GetObmmStubControl(importNodeId);
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_UNIMPORT, 100);

    {
        const char* name = "it_p1_fl_ret_master_to_req_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting FD: name=" << name;
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemFdDelete(name)); }, std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(importNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
    }
    {
        const char* name = "it_p1_fl_ret_master_to_req_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting NUMA: name=" << name;
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemNumaDelete(name)); }, std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(importNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
    }
    {
        const char* name = "it_p1_fl_ret_master_to_req_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting Share: name=" << name;
        comStubControl.SetComSendFailed(importNodeId, true);
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemShmDelete(name)); }, std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(importNodeId, false);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_TIMEOUT);
    }
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_UNIMPORT);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0026"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0026, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_master_to_req_send_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, importNodeId);
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_master_to_req_send_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, importNodeId);
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_master_to_req_send_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, importNodeId);
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());
}

// ReturnExportSendFailed-01: 导出节点向主节点发送归还响应失败 触发 RETURN_EXPORT_SEND_FAILED
void RunP1FaultLogReturnExportSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto importNodeId = masterNodeId;
    uint32_t exportNodeId = "1" == masterNodeId ? 2 : 1;
    auto exportNodeIdStr = std::to_string(exportNodeId);
    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(exportNodeIdStr).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(exportNodeIdStr);
    comStubControl.SetMemApiWaitTimeOut(1);
    {
        const char* name = "it_p1_fl_ret_export_to_master_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting FD: name=" << name;
        comStubControl.SetComSendFailed(masterNodeId, true);
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemFdDelete(name)); }, std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_export_to_master_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting NUMA: name=" << name;
        comStubControl.SetComSendFailed(masterNodeId, true);
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemNumaDelete(name)); }, std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_export_to_master_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting Share: name=" << name;
        comStubControl.SetComSendFailed(masterNodeId, true);
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemShmDelete(name)); }, std::move(promise));
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0027"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0027, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_export_to_master_send_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, importNodeId);
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_export_to_master_send_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, importNodeId);
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_export_to_master_send_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, importNodeId);
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());

    // 后置，当前情况下重启主节点和导入节点，资源自动清理
    cluster.RestartNode(masterNodeId, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(masterNodeId));
    cluster.RestartNode(exportNodeIdStr, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(exportNodeIdStr));
}

// ReturnImportSendFailed-01: 导入节点向主节点发送归还响应失败 触发 RETURN_IMPORT_SEND_FAILED
void RunP1FaultLogReturnImportSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    uint32_t exportNodeId = "1" == masterNodeId ? 1 : 2;
    auto exportNodeIdStr = std::to_string(exportNodeId);
    auto importNodeId = "1" == masterNodeId ? "2" : "1";
    auto& sdk = cluster.GetSdkClient(importNodeId);
    auto faultLogPath = cluster.GetNode(importNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& comStubControl = cluster.GetComStubControl(importNodeId);
    comStubControl.SetMemApiWaitTimeOut(1);

    auto& obmmStubControl = cluster.GetObmmStubControl(importNodeId);
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_UNIMPORT, 100);

    {
        const char* name = "it_p1_fl_ret_import_to_master_send_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};
        ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting FD: name=" << name;

        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemFdDelete(name)); }, std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(masterNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_import_to_master_send_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};
        ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_OK(ret);

        IT_LOG_INFO << "Deleting NUMA: name=" << name;
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemNumaDelete(name)); }, std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(masterNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_import_to_master_send_failed_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{.lender_size = shareSize,
                                .slot_id = exportNodeId,
                                .socket_id = UINT32_MAX,
                                .numa_id = 0,
                                .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        EXPECT_IT_OK(ret);

        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_OK(ret);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        IT_LOG_INFO << "Detaching Share: name=" << name;
        // 使用std::thread+promise实现1秒超时，超时后pthread_cancel杀死线程
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([&](std::promise<int> p) { p.set_value(sdk.MemShmDetach(name)); }, std::move(promise));
        usleep(10000);
        comStubControl.SetComSendFailed(masterNodeId, true);
        if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
            ret = UBS_ENGINE_ERR_TIMEOUT;
            pthread_cancel(t.native_handle());
            t.join();
        } else {
            ret = future.get();
        }
        comStubControl.SetComSendFailed(masterNodeId, false);
        EXPECT_NE(ret, UBS_SUCCESS);
    }
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_UNIMPORT);
    comStubControl.SetMemApiWaitTimeOut(0);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0028"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0028, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_import_to_master_send_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, importNodeId);
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_import_to_master_send_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, importNodeId);
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_import_to_master_send_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, importNodeId);
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());

    // 后置，当前情况下重启主节点和导入节点，资源自动清理
    cluster.RestartNode(masterNodeId, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(masterNodeId));
    cluster.RestartNode(importNodeId, true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning(importNodeId));
}

// ReturnReqSendFailed-01: 请求节点向主节点发送归还请求失败 触发 RETURN_REQ_SEND_FAILED
void RunP1FaultLogReturnReqSendFailed(ubse::it::infra::ItCluster& cluster)
{
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);

    auto exportNodeId = masterNodeId;
    auto reqNodeId = "1" == masterNodeId ? "2" : "1";
    auto& comStubControl = cluster.GetComStubControl(reqNodeId);
    comStubControl.SetComSendFailed(masterNodeId, true);

    auto& sdk = cluster.GetSdkClient(reqNodeId);
    auto& cliInvoker = cluster.GetCliInvoker(reqNodeId);
    auto faultLogPath = cluster.GetNode(reqNodeId).GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);
    {
        const char* name = "it_p1_fl_ret_req_send_failed_fd";

        // 创建FD失败，触发 RETURN_REQ_SEND_FAILED
        IT_LOG_INFO << "[ReturnReqSendFailed-01] Deleting FD with req send failed: name=" << name;
        ret = sdk.MemFdDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        EXPECT_NE(cliInvoker.DeleteMemory(name, "fd"), UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_req_send_failed_numa";

        // 创建NUMA失败，触发 RETURN_REQ_SEND_FAILED
        IT_LOG_INFO << "[ReturnReqSendFailed-01] Deleting NUMA with req send failed: name=" << name;
        ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        EXPECT_NE(cliInvoker.DeleteMemory(name, "numa"), UBS_SUCCESS);
    }
    {
        const char* name = "it_p1_fl_ret_req_send_failed_share";
        // 创建Share失败，触发 RETURN_REQ_SEND_FAILED
        IT_LOG_INFO << "[ReturnReqSendFailed-01] Deleting Share with req send failed: name=" << name;
        ret = sdk.MemShmDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        EXPECT_NE(cliInvoker.DeleteMemory(name, "share"), UBS_SUCCESS);

        ret = sdk.MemShmDetach(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);

        EXPECT_NE(cliInvoker.DetachMemory(name), UBS_SUCCESS);
    }
    comStubControl.SetComSendFailed(masterNodeId, false);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0029"; }, 8); // 等待8条日志

    ASSERT_EQ(entries.size(), 8u) << "Expected 8 fault log entry with ErrorCode=ubse_borrow_0029, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_req_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry.requestSize, 0) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry.requestNode, reqNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry.advice.empty()) << "FD advice is empty";

    const auto& fdEntry1 = entries[1];
    EXPECT_EQ(fdEntry1.requestName, "it_p1_fl_ret_req_send_failed_fd") << "FD requestName mismatch";
    EXPECT_EQ(fdEntry1.borrowType, "WATER_BORROW") << "FD borrowType mismatch";
    EXPECT_EQ(fdEntry1.requestSize, 0) << "FD requestSize mismatch";
    EXPECT_EQ(fdEntry1.requestNode, reqNodeId) << "FD requestNode mismatch";
    EXPECT_EQ(fdEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(fdEntry1.errorInfo.empty()) << "FD errorInfo is empty";
    EXPECT_FALSE(fdEntry1.advice.empty()) << "FD advice is empty";

    // 校验NUMA类型日志
    const auto& numaEntry = entries[2];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_req_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry.requestSize, 0) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry.requestNode, reqNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry.advice.empty()) << "NUMA advice is empty";

    const auto& numaEntry1 = entries[3];
    EXPECT_EQ(numaEntry1.requestName, "it_p1_fl_ret_req_send_failed_numa") << "NUMA requestName mismatch";
    EXPECT_EQ(numaEntry1.borrowType, "APP_NUMA_BORROW") << "NUMA borrowType mismatch";
    EXPECT_EQ(numaEntry1.requestSize, 0) << "NUMA requestSize mismatch";
    EXPECT_EQ(numaEntry1.requestNode, reqNodeId) << "NUMA requestNode mismatch";
    EXPECT_EQ(numaEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(numaEntry1.errorInfo.empty()) << "NUMA errorInfo is empty";
    EXPECT_FALSE(numaEntry1.advice.empty()) << "NUMA advice is empty";

    // 校验Share类型日志
    const auto& shareEntry = entries[4];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_req_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry.requestSize, 0) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry.requestNode, reqNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry.advice.empty()) << "Share advice is empty";

    const auto& shareEntry1 = entries[5];
    EXPECT_EQ(shareEntry1.requestName, "it_p1_fl_ret_req_send_failed_share") << "Share requestName mismatch";
    EXPECT_EQ(shareEntry1.borrowType, "SHARE_BORROW") << "Share borrowType mismatch";
    EXPECT_EQ(shareEntry1.requestSize, 0) << "Share requestSize mismatch";
    EXPECT_EQ(shareEntry1.requestNode, reqNodeId) << "Share requestNode mismatch";
    EXPECT_EQ(shareEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareEntry1.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(shareEntry1.advice.empty()) << "Share advice is empty";

    // 校验Share Detach类型日志
    const auto& shareDetachEntry = entries[6];
    EXPECT_EQ(shareDetachEntry.requestName, "it_p1_fl_ret_req_send_failed_share")
        << "Share Detach requestName mismatch";
    EXPECT_EQ(shareDetachEntry.borrowType, "SHARE_BORROW") << "Share Detach borrowType mismatch";
    EXPECT_EQ(shareDetachEntry.requestSize, 0) << "Share Detach requestSize mismatch";
    EXPECT_EQ(shareDetachEntry.requestNode, reqNodeId) << "Share Detach requestNode mismatch";
    EXPECT_EQ(shareDetachEntry.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareDetachEntry.errorInfo.empty()) << "Share Detach errorInfo is empty";
    EXPECT_FALSE(shareDetachEntry.advice.empty()) << "Share Detach advice is empty";

    const auto& shareDetachEntry1 = entries[7];
    EXPECT_EQ(shareDetachEntry1.requestName, "it_p1_fl_ret_req_send_failed_share")
        << "Share Attach requestName mismatch";
    EXPECT_EQ(shareDetachEntry1.borrowType, "SHARE_BORROW") << "Share Detach borrowType mismatch";
    EXPECT_EQ(shareDetachEntry1.requestSize, 0) << "Share Detach requestSize mismatch";
    EXPECT_EQ(shareDetachEntry1.requestNode, reqNodeId) << "Share Detach requestNode mismatch";
    EXPECT_EQ(shareDetachEntry1.adviceCode, 2u); // COMM_FAILED
    EXPECT_FALSE(shareDetachEntry1.errorInfo.empty()) << "Share Detach errorInfo is empty";
    EXPECT_FALSE(shareDetachEntry1.advice.empty()) << "Share Detach advice is empty";
}

// ReturnReqConflict-01:  FD/NUMA/Share 归还请求冲突
void RunP1FaultLogReturnReqConflict(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    auto& obmmStubControl = cluster.GetObmmStubControl("2");
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_EXPORT, 100);
    {
        const char* name = "it_p1_fl_ret_req_conflict_fd";

        // 同时创建FD请求和归还请求，构造请求冲突，触发RETURN_REQ_CONFLICT
        IT_LOG_INFO << "[ReturnReqConflict-01] Creating FD with request conflict: name=" << name;
        // 使用std::future获取线程返回值
        auto future1 = std::async(std::launch::async, [&]() -> int {
            ubs_mem_fd_desc_t fdDesc1{};
            return sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc1);
        });
        auto future2 = std::async(std::launch::async, [&]() -> int {
            EXPECT_NE(cliInvoker.DeleteMemory(name, "fd"), UBS_SUCCESS);
            return 0;
        });
    }
    {
        const char* name = "it_p1_fl_ret_req_conflict_numa";

        // 同时创建NUMA请求和归还请求，构造请求冲突，触发RETURN_REQ_CONFLICT
        IT_LOG_INFO << "[ReturnReqConflict-01] Creating NUMA with request conflict: name=" << name;
        // 使用std::future获取线程返回值
        auto future1 = std::async(std::launch::async, [&]() -> int {
            ubs_mem_numa_desc_t numaDesc1{};
            return sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc1);
        });
        auto future2 = std::async(std::launch::async, [&]() -> int {
            EXPECT_NE(cliInvoker.DeleteMemory(name, "numa"), UBS_SUCCESS);
            return 0;
        });
    }
    {
        const char* name = "it_p1_fl_ret_req_conflict_share";
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{
            .lender_size = shareSize, .slot_id = 2, .socket_id = UINT32_MAX, .numa_id = 0, .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;

        // 使用两个sdk同时创建Share请求，构造请求冲突，触发RETURN_REQ_CONFLICT
        IT_LOG_INFO << "[ReturnReqConflict-01] Creating SHM with request conflict: name=" << name;
        // 使用std::future获取线程返回值
        auto future1 = std::async(std::launch::async, [&]() -> int {
            return sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        });
        // ShareReturnValidate 调 FindShareBorrowObjByName → GetMaxRefCountExportObj → CollectExportObjsByName
        // CollectExportObjsByName 显式跳过非 SUCCESS 状态,拿不到 RUNNING 对象
    }
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_EXPORT);
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_UNEXPORT, 100);
    {
        // 同时创建FD CLI 和 SDK 归还请求，构造请求冲突，触发RETURN_REQ_CONFLICT
        const char* name = "it_p1_fl_ret_req_conflict_fd";
        // 使用std::future获取线程返回值
        auto future1 = std::async(std::launch::async, [&]() -> int { return sdk.MemFdDelete(name); });
        auto future2 = std::async(std::launch::async, [&]() -> int {
            EXPECT_NE(cliInvoker.DeleteMemory(name, "fd"), UBS_SUCCESS);
            return 0;
        });
    }
    {
        // 同时创建NUMA CLI 和 SDK 归还请求，构造请求冲突，触发RETURN_REQ_CONFLICT
        const char* name = "it_p1_fl_ret_req_conflict_numa";
        // 使用std::future获取线程返回值
        auto future1 = std::async(std::launch::async, [&]() -> int { return sdk.MemNumaDelete(name); });
        auto future2 = std::async(std::launch::async, [&]() -> int {
            EXPECT_NE(cliInvoker.DeleteMemory(name, "numa"), UBS_SUCCESS);
            return 0;
        });
    }
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_UNEXPORT);

    {
        // 清理
        auto ret = sdk.MemShmDelete("it_p1_fl_ret_req_conflict_share");
        EXPECT_IT_OK(ret);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0030"; }, 4);

    ASSERT_EQ(entries.size(), 4u) << "Expected 4 fault log entries with ErrorCode=ubse_borrow_0030, got "
                                  << entries.size();
    // 校验日志内容

    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_req_conflict_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, "1");
    EXPECT_EQ(fdEntry.adviceCode, 9u); // REQ_CONFLICT
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_req_conflict_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, "1");
    EXPECT_EQ(numaEntry.adviceCode, 9u); // REQ_CONFLICT
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& fdEntry1 = entries[2];
    EXPECT_EQ(fdEntry1.requestName, "it_p1_fl_ret_req_conflict_fd");
    EXPECT_EQ(fdEntry1.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry1.requestSize, 0);
    EXPECT_EQ(fdEntry1.requestNode, "1");
    EXPECT_EQ(fdEntry1.adviceCode, 9u); // REQ_CONFLICT
    EXPECT_FALSE(fdEntry1.errorInfo.empty());
    EXPECT_FALSE(fdEntry1.advice.empty());

    const auto& numaEntry1 = entries[3];
    EXPECT_EQ(numaEntry1.requestName, "it_p1_fl_ret_req_conflict_numa");
    EXPECT_EQ(numaEntry1.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry1.requestSize, 0);
    EXPECT_EQ(numaEntry1.requestNode, "1");
    EXPECT_EQ(numaEntry1.adviceCode, 9u); // REQ_CONFLICT
    EXPECT_FALSE(numaEntry1.errorInfo.empty());
    EXPECT_FALSE(numaEntry1.advice.empty());
}

// ReturnObmmExportFailed-01: OBMM导出失败 触发 RETURN_OBMM_EXPORT_FAILED
void RunP1FaultLogReturnObmmExportFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& obmmStubControl = cluster.GetObmmStubControl("2");
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_UNEXPORT, true);

    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("2").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_ret_obmm_unexport_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};

        IT_LOG_INFO << "[NameExist] Creating FD: name=" << name;
        auto ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        ASSERT_IT_OK(ret);

        // 节点1尝试归还共享内存，触发RETURN_OBMM_EXPORT_FAILED
        IT_LOG_INFO << "[ReturnObmmExportFailed-01] Returning OBMM export failed: name=" << name;
        ret = sdk.MemFdDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_ret_obmm_unexport_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};

        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating NUMA: name=" << name;
        auto ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        ASSERT_IT_OK(ret);

        // 节点1尝试归还共享内存，触发RETURN_OBMM_EXPORT_FAILED
        IT_LOG_INFO << "[ReturnObmmExportFailed-01] Returning OBMM export failed: name=" << name;
        ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_ret_obmm_unexport_failed_share";

        // 创建共享内存
        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating SHM: name=" << name;
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{
            .lender_size = shareSize, .slot_id = 2, .socket_id = UINT32_MAX, .numa_id = 0, .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        auto ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        ASSERT_IT_OK(ret);

        // 节点1尝试归还共享内存，触发RETURN_OBMM_EXPORT_FAILED
        IT_LOG_INFO << "[ReturnObmmExportFailed-01] Returning OBMM export failed: name=" << name;
        ret = sdk.MemShmDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_UNEXPORT, false);

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0034"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0034, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_obmm_unexport_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, "1");
    EXPECT_EQ(fdEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_obmm_unexport_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, "1");
    EXPECT_EQ(numaEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_obmm_unexport_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, "1");
    EXPECT_EQ(shareEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());

    // 后置，当前情况下重启主节点和导入节点，资源自动清理
    cluster.RestartNode("1", true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning("1"));
    cluster.RestartNode("2", true, 30000);
    EXPECT_TRUE(cluster.IsNodeRunning("2"));
}

// ReturnObmmImportFailed-01: OBMM导入失败 触发 RETURN_OBMM_IMPORT_FAILED
void RunP1FaultLogReturnObmmImportFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& obmmStubControl = cluster.GetObmmStubControl("1");
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_UNIMPORT, true);

    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_ret_obmm_unimport_failed_fd";
        ubs_mem_fd_desc_t fdDesc{};

        IT_LOG_INFO << "[NameExist] Creating FD: name=" << name;
        auto ret = sdk.MemFdCreate(name, fdSize, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        ASSERT_IT_OK(ret);

        // 节点1尝试归还共享内存，触发RETURN_OBMM_IMPORT_FAILED
        IT_LOG_INFO << "[ReturnObmmImportFailed-01] Returning OBMM import failed: name=" << name;
        ret = sdk.MemFdDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_ret_obmm_unimport_failed_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA成功
        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating NUMA: name=" << name;
        auto ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        ASSERT_IT_OK(ret);

        // 节点1尝试归还共享内存，触发RETURN_OBMM_IMPORT_FAILED
        IT_LOG_INFO << "[ReturnObmmImportFailed-01] Returning OBMM import failed: name=" << name;
        ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    {
        const char* name = "it_p1_fl_ret_obmm_unimport_failed_share";

        // 创建共享内存成功
        IT_LOG_INFO << "[BorrowObmmImportFailed-01] Creating SHM: name=" << name;
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};
        ubs_mem_lender_t lender{
            .lender_size = shareSize, .slot_id = 2, .socket_id = UINT32_MAX, .numa_id = 0, .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        auto ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        ASSERT_IT_OK(ret);

        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
        ASSERT_IT_OK(ret);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        // 节点1尝试Detach共享内存，触发RETURN_OBMM_IMPORT_FAILED
        IT_LOG_INFO << "[ReturnObmmImportFailed-01] Detaching OBMM import failed: name=" << name;
        ret = sdk.MemShmDetach(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_INTERNAL);
    }
    obmmStubControl.SetOpFailed(ObmmStubControl::OP_UNIMPORT, false);

    {
        auto ret = sdk.MemFdDelete("it_p1_fl_ret_obmm_unimport_failed_fd");
        EXPECT_IT_OK(ret);

        ret = sdk.MemNumaDelete("it_p1_fl_ret_obmm_unimport_failed_numa");
        EXPECT_IT_OK(ret);

        ret = sdk.MemShmDetach("it_p1_fl_ret_obmm_unimport_failed_share");
        EXPECT_IT_OK(ret);

        ret = sdk.MemShmDelete("it_p1_fl_ret_obmm_unimport_failed_share");
        EXPECT_IT_OK(ret);
    }

    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0035"; }, 3);

    ASSERT_EQ(entries.size(), 3u) << "Expected 3 fault log entries with ErrorCode=ubse_borrow_0035, got "
                                  << entries.size();

    // 校验日志内容
    const auto& fdEntry = entries[0];
    EXPECT_EQ(fdEntry.requestName, "it_p1_fl_ret_obmm_unimport_failed_fd");
    EXPECT_EQ(fdEntry.borrowType, "WATER_BORROW");
    EXPECT_EQ(fdEntry.requestSize, 0);
    EXPECT_EQ(fdEntry.requestNode, "1");
    EXPECT_EQ(fdEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(fdEntry.errorInfo.empty());
    EXPECT_FALSE(fdEntry.advice.empty());

    const auto& numaEntry = entries[1];
    EXPECT_EQ(numaEntry.requestName, "it_p1_fl_ret_obmm_unimport_failed_numa");
    EXPECT_EQ(numaEntry.borrowType, "APP_NUMA_BORROW");
    EXPECT_EQ(numaEntry.requestSize, 0);
    EXPECT_EQ(numaEntry.requestNode, "1");
    EXPECT_EQ(numaEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(numaEntry.errorInfo.empty());
    EXPECT_FALSE(numaEntry.advice.empty());

    const auto& shareEntry = entries[2];
    EXPECT_EQ(shareEntry.requestName, "it_p1_fl_ret_obmm_unimport_failed_share");
    EXPECT_EQ(shareEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(shareEntry.requestSize, 0);
    EXPECT_EQ(shareEntry.requestNode, "1");
    EXPECT_EQ(shareEntry.adviceCode, 4u); //OBMM_FAILED
    EXPECT_FALSE(shareEntry.errorInfo.empty());
    EXPECT_FALSE(shareEntry.advice.empty());
}

// ShareReturnInAttached-01: Share 归还节点存在attach 触发 SHARED_RETURN_IN_ATTACHED
void RunP1FaultLogShareReturnInAttached(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& sdk2 = cluster.GetSdkClient("2");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_share_ret_in_attached";
    {
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        // 创建共享内存，共享域包含节点1和节点2
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);

        // 节点2 attach共享内存
        IT_LOG_INFO << "[ShareReturnInAttached-01] Attaching share from node 2: name=" << name;
        ubs_mem_shm_desc_t* attachDesc = nullptr;
        ret = sdk2.MemShmAttach(name, nullptr, 0, &attachDesc);
        EXPECT_IT_OK(ret);
        if (attachDesc != nullptr) {
            free(attachDesc);
        }

        // 节点1尝试归还存在attach的共享内存，触发SHARED_RETURN_IN_ATTACHED
        IT_LOG_INFO << "[ShareReturnInAttached-01] Returning share with attached node: name=" << name;
        ret = sdk.MemShmDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_ATTACH_USING);

        // 清理attach
        ret = sdk2.MemShmDetach(name);
        ASSERT_IT_OK(ret);
        // 清理共享内存
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验fault log中出现SHARED_RETURN_IN_ATTACHED (faultCode=37)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0037"; });

    // 校验日志总数
    ASSERT_EQ(entries.size(), 1u) << "Expected 1 fault log entry with ErrorCode=ubse_borrow_0037, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, 0);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 9u); // RES_OP_CONFLICT
    EXPECT_FALSE(entry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "Share advice is empty";
}

// ShareReturnRegionFailed-01(四节点): Share 归还节点不在共享域 触发 SHARED_RETURN_REGION_FAILED
void RunP1FaultLogShareReturnRegionFailed(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    std::string masterNodeId;
    auto ret = cluster.GetMasterNodeId(masterNodeId);
    EXPECT_IT_OK(ret);
    // 判断节点3是否为主节点，若不是则使用节点3，若是则使用节点4
    std::string nodeId = (masterNodeId != "3") ? "3" : "4";
    auto faultLogPath = cluster.GetNode(masterNodeId).GetLogFaultFilePath();
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_share_ret_region_failed";
    {
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        // 创建共享内存，共享域包含节点1和节点2
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        ret = sdk.MemShmCreate(name, shareSize, usrInfo, 0, &region, nullptr);
        ASSERT_IT_OK(ret);

        // 节点3尝试归还共享内存，触发SHARED_RETURN_REGION_FAILED
        auto& returnSdk = cluster.GetSdkClient(nodeId);
        IT_LOG_INFO << "[ShareReturnRegionFailed-01] Returning share from node not in region: name=" << name;
        ret = returnSdk.MemShmDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_AUTH_FAILED);

        // 清理
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验 fault log 中出现 SHARED_RETURN_REGION_FAILED (faultCode=38)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0038"; });

    // 校验日志总数
    ASSERT_EQ(entries.size(), 1u) << "Expected 1 fault log entry with ErrorCode=ubse_borrow_0038, got "
                                  << entries.size(); // 校验日志内容

    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, 0);
    EXPECT_EQ(entry.requestNode, nodeId);
    EXPECT_EQ(entry.adviceCode, 1u); // CHECK_FAILED
    EXPECT_FALSE(entry.errorInfo.empty());
    EXPECT_FALSE(entry.advice.empty());
}

// ShareDetachReqConflict-01: Share detach请求冲突 触发 SHARED_DETACH_REQ_CONFLICT
void RunP1FaultLogShareDetachReqConflict(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_share_detach_req_conflict";
    {
        uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN] = {0};

        // 创建共享内存，共享域包含节点1和节点2
        ubs_mem_lender_t lender{
            .lender_size = shareSize, .slot_id = 2, .socket_id = UINT32_MAX, .numa_id = 0, .port_id = UINT32_MAX};
        ubs_mem_nodes_t region{};
        region.node_cnt = 2;
        region.slot_ids[0] = cluster.GetNode("1").GetSpec().slotId;
        region.slot_ids[1] = cluster.GetNode("2").GetSpec().slotId;
        auto ret = sdk.MemShmCreateWithLender(name, usrInfo, 0, &region, &lender);
        ASSERT_IT_OK(ret);
    }

    auto& obmmStubControl = cluster.GetObmmStubControl("1");
    obmmStubControl.SetOpDelay(ObmmStubControl::OP_IMPORT, 100);

    {
        IT_LOG_INFO << "[ReturnReqConflict-01] Creating SHM with request conflict: name=" << name;
        // 使用std::future获取线程返回值
        auto future1 = std::async(std::launch::async, [&]() {
            ubs_mem_shm_desc_t* attachDesc = nullptr;
            auto attachRet = sdk.MemShmAttach(name, nullptr, 0, &attachDesc);
            if (attachDesc != nullptr) {
                free(attachDesc);
            }
            return attachRet;
        });
        auto future2 = std::async(std::launch::async, [&]() -> int {
            EXPECT_NE(cliInvoker.DetachMemory(name), UBS_SUCCESS);
            return 0;
        });
    }

    obmmStubControl.SetOpDelay(ObmmStubControl::OP_IMPORT);

    {
        // 清理
        auto ret = sdk.MemShmDetach(name);
        ASSERT_IT_OK(ret);
        ret = sdk.MemShmDelete(name);
        ASSERT_IT_OK(ret);
    }

    // 等待并校验 fault log 中出现 SHARED_DETACH_REQ_CONFLICT (faultCode=40)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0040"; });
    // 校验日志总数
    ASSERT_EQ(entries.size(), 1u) << "Expected 1 fault log entry with ErrorCode=ubse_borrow_0040, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, 0);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 9u); // RES_OP_CONFLICT
    EXPECT_FALSE(entry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "Share advice is empty";
}

// ShareDetachNotExist-01: Share detach不存在的共享内存 触发 SHARED_DETACH_NOT_EXIST
void RunP1FaultLogShareDetachNotExist(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    const char* name = "it_p1_fl_share_detach_not_exist";
    {
        // 节点1尝试detach不存在的共享内存，触发SHARED_DETACH_NOT_EXIST
        IT_LOG_INFO << "[ShareDetachNotExist-01] Detaching share name not exist: name=" << name;
        auto ret = sdk.MemShmDetach(name);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_SHM_NO_ATTACH);
    }

    // 等待并校验 fault log 中出现 SHARED_DETACH_NOT_EXIST (faultCode=41)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0041"; });

    // 校验日志总数
    ASSERT_EQ(entries.size(), 1u) << "Expected 1 fault log entry with ErrorCode=ubse_borrow_0041, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, name);
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, 0);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 8u); // RESOURCE_NOT_EXIST
    EXPECT_FALSE(entry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "Share advice is empty";
}

// ShareReturnChipNotSupported-01: 底层芯片不支持Share归还 触发 SHARED_RETURN_CHIP_NOT_SUPPORTED
void RunP1FaultLogShareReturnChipNotSupported(ubse::it::infra::ItCluster& cluster)
{
    auto& cliInvoker = cluster.GetCliInvoker("1");
    auto& sdk = cluster.GetSdkClient("1");
    auto faultLogPath = cluster.GetNode("1").GetLogFaultFilePath();
    // 清空 fault log，避免前序用例干扰
    ItFaultLogHelper::ClearFaultLog(faultLogPath);

    {
        const char* name = "it_p1_fl_share_ret_chip_not_supported";
        // SDK尝试归还共享内存，触发SHARED_RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ShareReturnChipNotSupported-01] Returning share chip not supported: name=" << name;
        auto ret = sdk.MemShmDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试归还共享内存，触发SHARED_RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ShareReturnChipNotSupported-01] Returning share chip not supported: name=" << name;
        cliInvoker.DeleteMemory(name, "share");
    }
    {
        const char* name = "it_p1_fl_share_detach_ret_chip_not_supported";
        // SDK尝试detach共享内存，触发SHARED_RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ShareReturnChipNotSupported-01] Detaching share chip not supported: name=" << name;
        auto ret = sdk.MemShmDetach(name);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试detach共享内存，触发SHARED_RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ShareReturnChipNotSupported-01] Detaching share chip not supported: name=" << name;
        cliInvoker.DetachMemory(name);
    }

    // 等待并校验 fault log 中出现 SHARED_RETURN_CHIP_NOT_SUPPORTED (faultCode=42)
    auto entries = ItFaultLogHelper::WaitForFaultLog(
        faultLogPath, [](const FaultLogEntry& e) { return e.errorCode == "ubse_borrow_0042"; }, 4);

    // 校验日志总数
    ASSERT_EQ(entries.size(), 4u) << "Expected 4 fault log entries with ErrorCode=ubse_borrow_0042, got "
                                  << entries.size();

    // 校验日志内容
    const auto& entry = entries[0];
    EXPECT_EQ(entry.requestName, "it_p1_fl_share_ret_chip_not_supported");
    EXPECT_EQ(entry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(entry.requestSize, 0);
    EXPECT_EQ(entry.requestNode, "1");
    EXPECT_EQ(entry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(entry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(entry.advice.empty()) << "Share advice is empty";

    const auto& cliEntry = entries[1];
    EXPECT_EQ(cliEntry.requestName, "it_p1_fl_share_ret_chip_not_supported");
    EXPECT_EQ(cliEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(cliEntry.requestSize, 0);
    EXPECT_EQ(cliEntry.requestNode, "1");
    EXPECT_EQ(cliEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(cliEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(cliEntry.advice.empty()) << "Share advice is empty";

    const auto& detachEntry = entries[2];
    EXPECT_EQ(detachEntry.requestName, "it_p1_fl_share_detach_ret_chip_not_supported");
    EXPECT_EQ(detachEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(detachEntry.requestSize, 0);
    EXPECT_EQ(detachEntry.requestNode, "1");
    EXPECT_EQ(detachEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(detachEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(detachEntry.advice.empty()) << "Share advice is empty";

    const auto& cliDetachEntry = entries[3];
    EXPECT_EQ(cliDetachEntry.requestName, "it_p1_fl_share_detach_ret_chip_not_supported");
    EXPECT_EQ(cliDetachEntry.borrowType, "SHARE_BORROW");
    EXPECT_EQ(cliDetachEntry.requestSize, 0);
    EXPECT_EQ(cliDetachEntry.requestNode, "1");
    EXPECT_EQ(cliDetachEntry.adviceCode, 11u); // CHIP_NOT_SUPPORTED
    EXPECT_FALSE(cliDetachEntry.errorInfo.empty()) << "Share errorInfo is empty";
    EXPECT_FALSE(cliDetachEntry.advice.empty()) << "Share advice is empty";
}
} // namespace ubse::it::tests::mem_borrow
