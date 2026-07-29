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
#include <unistd.h>
#include <cstdint>
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
        cliInvoker.CreateMemoryNuma(createInfo, name, "4M", "1/1/1-2/1/1");
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
        cliInvoker.CreateMemoryFd(createInfo, name, "4M");
    }
    {
        const char* name = "it_p1_fl_chip_not_support_numa";
        ubs_mem_numa_desc_t numaDesc{};

        // 创建NUMA失败，触发 BORROW_CHIP_NOT_SUPPORT
        IT_LOG_INFO << "[BorrowChipNotSupport-01] Creating NUMA with chip not support: name=" << name;
        auto ret = sdk.MemNumaCreate(name, numaSize, MEM_DISTANCE_L0, &numaDesc);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        ubse::it::infra::ItMemCreateInfo createInfo;
        cliInvoker.CreateMemoryNuma(createInfo, name, "4M");
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
        ubs_mem_lender_t lender{.lender_size = fdSize, .slot_id = 1, .socket_id = 1, .numa_id = 1, .port_id = 0};
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

// P1-FaultLog-BorrowObmmImportFailed-01: OBMM导入失败 触发 BORROW_OBMM_IMPORT_FAILED
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

        // 节点1再次attach，触发SHARE_ATTACH_EXIST
        IT_LOG_INFO << "[ShareAttachExist-01] Attaching SHM duplicated: name=" << name;
        ubs_mem_shm_desc_t* attachDesc2 = nullptr;
        ret = sdk.MemShmAttach(name, nullptr, 0, &attachDesc2);
        EXPECT_IT_ERROR(ret, UBS_ENGINE_ERR_EXISTED);

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

// P1-FaultLog-ShareChipNotSupported-01: 底层芯片不支持Shared attach 触发 SHARED_CHIP_NOT_SUPPORTED
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
        cliInvoker.AttachMemory(attachInfo, name);
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

// P1-FaultLog-ShareChipModeNotSupported-01: 底层芯片模式不支持Share借用模式 触发 SHARED_CHIP_MODE_NOT_SUPPORTED
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
        cliInvoker.CreateMemoryShare(shareCreateInfo, name, "4M", "1,2");
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

// P1-FaultLog-ReturnNameNotExist-01:  FD/NUMA/Share 归还不存在的共享内存 触发 RETURN_NAME_NOT_EXIST
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

//P1-FaultLog-ReturnChipNotSupported-01: 底层芯片不支持FD/NUMA归还 触发 RETURN_CHIP_NOT_SUPPORTED
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
        cliInvoker.DeleteMemory(name, "fd");
    }
    {
        const char* name = "it_p1_fl_ret_chip_not_supported_numa";
        // SDK尝试归还不存在的共享内存，触发RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ReturnChipNotSupported-01] Returning NUMA name not exist: name=" << name;
        auto ret = sdk.MemNumaDelete(name);
        EXPECT_IT_ERROR(ret, UBS_ERR_NOT_SUPPORTED);

        // CLI尝试归还不存在的共享内存，触发RETURN_CHIP_NOT_SUPPORTED
        IT_LOG_INFO << "[ReturnChipNotSupported-01] Returning NUMA name not exist: name=" << name;
        cliInvoker.DeleteMemory(name, "numa");
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
        ubs_mem_shm_desc_t shmDesc{};

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
        ubs_mem_shm_desc_t shmDesc{};

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

// P1-FaultLog-ShareReturnRegionFailed-01(四节点): Share 归还节点不在共享域 触发 SHARED_RETURN_REGION_FAILED
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

// P1-FaultLog-ShareDetachNotExist-01: Share detach不存在的共享内存 触发 SHARED_DETACH_NOT_EXIST
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

// P1-FaultLog-ShareReturnChipNotSupported-01: 底层芯片不支持Share归还 触发 SHARED_RETURN_CHIP_NOT_SUPPORTED
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
