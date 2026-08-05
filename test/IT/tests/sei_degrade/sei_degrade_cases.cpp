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

#include "sei_degrade_cases.h"

#include <gtest/gtest.h>
#include <unistd.h>

#include <fstream>
#include <string>

#include "it_assertion.h"
#include "it_console_log.h"
#include "it_sdk_client.h"
#include "ubs_engine.h"
#include "ubs_engine_mem.h"
#include "ubs_error.h"

namespace ubse::it::tests::sei_degrade {

namespace {

constexpr const char* BORROWER_NODE = "2";
constexpr const char* FD_NAME = "it_sei_fd";
constexpr const char* NUMA_NAME = "it_sei_numa";

std::string ReadSeiFile(const std::string& workDir)
{
    std::string path = workDir + "/sei_degrade_status";
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content;
    std::getline(file, content);
    return content;
}

} // namespace

void RunITSeiDegradeLifecycle(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient(BORROWER_NODE);
    auto& node = cluster.GetNode(BORROWER_NODE);
    std::string workDir = node.GetWorkDir();

    // IT-01: 节点2借入FD内存，首次借用后SEI文件变为"1"
    {
        IT_LOG_INFO << "IT-01: borrowing FD on node " << BORROWER_NODE;
        ubs_mem_fd_desc_t fdDesc{};
        int32_t ret = sdk.MemFdCreate(FD_NAME, UBS_MEM_MIN_SIZE, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        ASSERT_IT_OK(ret);
        IT_LOG_INFO << "IT-01: FD created, export_node.slot_id=" << fdDesc.export_node.slot_id;

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_EQ(seiContent, "1") << "IT-01: SEI file should be '1' after first borrow, got '" << seiContent << "'";
        IT_LOG_INFO << "IT-01 passed: SEI file = '" << seiContent << "'";
    }

    // IT-02: 节点2继续借入NUMA内存，seiOpened_已为true，不应重复写文件
    {
        IT_LOG_INFO << "IT-02: borrowing NUMA on node " << BORROWER_NODE;
        ubs_mem_numa_desc_t numaDesc{};
        int32_t ret = sdk.MemNumaCreate(NUMA_NAME, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
        ASSERT_IT_OK(ret);
        IT_LOG_INFO << "IT-02: NUMA created";

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_EQ(seiContent, "1") << "IT-02: SEI file should still be '1', got '" << seiContent << "'";
        IT_LOG_INFO << "IT-02 passed: SEI file = '" << seiContent << "'";
    }

    // IT-03: 节点2归还FD内存（NUMA仍存在），SEI文件应保持"1"
    {
        IT_LOG_INFO << "IT-03: returning FD on node " << BORROWER_NODE;
        int32_t ret = sdk.MemFdDelete(FD_NAME);
        ASSERT_IT_OK(ret);

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_EQ(seiContent, "1") << "IT-03: SEI file should still be '1' after partial return, got '" << seiContent
                                   << "'";
        IT_LOG_INFO << "IT-03 passed: SEI file = '" << seiContent << "'";
    }

    // IT-04: 节点2归还NUMA内存（末次归还），SEI文件应变为"0"
    {
        IT_LOG_INFO << "IT-04: returning NUMA on node " << BORROWER_NODE;
        int32_t ret = sdk.MemNumaDelete(NUMA_NAME);
        ASSERT_IT_OK(ret);

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_EQ(seiContent, "0") << "IT-04: SEI file should be '0' after last return, got '" << seiContent << "'";
        IT_LOG_INFO << "IT-04 passed: SEI file = '" << seiContent << "'";
    }

    IT_LOG_INFO << "IT-01~IT-04: all SEI degradation lifecycle tests passed";
}

void RunITSeiDegradeDisabled(ubse::it::infra::ItCluster& cluster)
{
    auto& sdk = cluster.GetSdkClient(BORROWER_NODE);
    auto& node = cluster.GetNode(BORROWER_NODE);
    std::string workDir = node.GetWorkDir();
    IT_LOG_INFO << "IT-06: sei.enable=false, full flow on node " << BORROWER_NODE;
    {
        IT_LOG_INFO << "IT-06: borrowing FD";
        ubs_mem_fd_desc_t fdDesc{};
        int32_t ret = sdk.MemFdCreate(FD_NAME, UBS_MEM_MIN_SIZE, nullptr, 0, MEM_DISTANCE_L0, &fdDesc);
        ASSERT_IT_OK(ret);

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_TRUE(seiContent.empty() || seiContent == "0")
            << "IT-06: SEI file should be empty or '0' when disabled, got '" << seiContent << "'";
        IT_LOG_INFO << "IT-06 after FD borrow: SEI file = '" << seiContent << "'";
    }
    {
        IT_LOG_INFO << "IT-06: borrowing NUMA";
        ubs_mem_numa_desc_t numaDesc{};
        int32_t ret = sdk.MemNumaCreate(NUMA_NAME, UBS_MEM_MIN_SIZE, MEM_DISTANCE_L0, &numaDesc);
        ASSERT_IT_OK(ret);

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_TRUE(seiContent.empty() || seiContent == "0")
            << "IT-06: SEI file should remain unchanged when disabled, got '" << seiContent << "'";
        IT_LOG_INFO << "IT-06 after NUMA borrow: SEI file = '" << seiContent << "'";
    }
    {
        IT_LOG_INFO << "IT-06: returning NUMA";
        int32_t ret = sdk.MemNumaDelete(NUMA_NAME);
        ASSERT_IT_OK(ret);
    }
    {
        IT_LOG_INFO << "IT-06: returning FD";
        int32_t ret = sdk.MemFdDelete(FD_NAME);
        ASSERT_IT_OK(ret);

        sleep(1);
        std::string seiContent = ReadSeiFile(workDir);
        EXPECT_TRUE(seiContent.empty() || seiContent == "0")
            << "IT-06: SEI file should be empty or '0' after all returns, got '" << seiContent << "'";
        IT_LOG_INFO << "IT-06 after all returns: SEI file = '" << seiContent << "'";
    }

    IT_LOG_INFO << "IT-06: SEI degradation disabled test passed";
}

} // namespace ubse::it::tests::sei_degrade
