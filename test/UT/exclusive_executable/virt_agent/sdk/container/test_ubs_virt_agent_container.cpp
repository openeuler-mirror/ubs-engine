/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubs_virt_agent_container.h"
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include <securec.h>
#include "mem_container_msg.h"
#include "ubs_virt_agent_container.h"
#include "ubse_api_server.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "vm_error.h"

using namespace vm;
namespace ubse::vm::ut {
TestContainerSDKService::TestContainerSDKService() = default;

void TestContainerSDKService::SetUp()
{
    Test::SetUp();
}

void TestContainerSDKService::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t test_ubse_invoke_call_pidInfos(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data,
                                        ubse_api_buffer_t *response_data)
{
    std::vector<PidInfo> pidInfos{};
    PidInfo pidInfo;
    pidInfo.pid = 71685;
    pidInfo.localUsedMem = 1024 * 1024 * 50;
    pidInfo.localNumaIds = {0, 1, 2};
    pidInfo.remoteUsedMem = 1024 * 1024 * 30;
    pidInfo.remoteNumaId = 3;
    pidInfos.push_back(pidInfo);

    MemContainerPidMemInfoOutputMsg outputInfo(pidInfos);
    outputInfo.Serialize();
    response_data->buffer = outputInfo.SerializedData();
    response_data->length = outputInfo.SerializedDataSize();
    return VM_OK;
}

uint32_t test_ubse_invoke_call_pidInfos_failed(uint16_t module_code, uint16_t op_code,
                                               const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    std::vector<PidInfo> pidInfos{};
    PidInfo pidInfo;
    pidInfo.pid = 71685;
    pidInfo.localUsedMem = 1024 * 1024 * 50;
    pidInfo.localNumaIds = {0, 1, 2};
    pidInfo.remoteUsedMem = 1024 * 1024 * 30;
    pidInfo.remoteNumaId = 3;
    pidInfos.push_back(pidInfo);

    MemContainerPidMemInfoOutputMsg outputInfo(pidInfos);
    outputInfo.Serialize();
    response_data->buffer = outputInfo.SerializedData();
    response_data->length = outputInfo.SerializedDataSize();
    return VM_ERROR;
}

uint32_t test_ubse_invoke_call_inject_waterLine_failed(uint16_t module_code, uint16_t op_code,
                                                       const ubse_api_buffer_t *request_data,
                                                       ubse_api_buffer_t *response_data)
{
    response_data->buffer = nullptr;
    response_data->length = 0;
    return VM_ERROR;
}

uint32_t test_ubse_invoke_call_inject_waterLine(uint16_t module_code, uint16_t op_code,
                                                const ubse_api_buffer_t *request_data, ubse_api_buffer_t *response_data)
{
    response_data->buffer = nullptr;
    response_data->length = 0;
    return VM_OK;
}

uint32_t test_ubse_invoke_call_get_container_pids(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data)
{
    std::vector<container_pid_info_for_c> responseData;
    container_pid_info_for_c pidInfo;
    pidInfo.pids[0] = 71685;
    pidInfo.pidsCount = 1;
    pidInfo.containerId = "container123";
    responseData.push_back(pidInfo);

    ContainerPidsForCInputMsg outputInfo(responseData);
    outputInfo.Serialize();
    response_data->buffer = outputInfo.SerializedData();
    response_data->length = outputInfo.SerializedDataSize();
    return VM_OK;
}

uint32_t test_ubse_invoke_call_get_container_pids_failed(uint16_t module_code, uint16_t op_code,
                                                         const ubse_api_buffer_t *request_data,
                                                         ubse_api_buffer_t *response_data)
{
    std::vector<container_pid_info_for_c> responseData;
    container_pid_info_for_c pidInfo;
    pidInfo.pids[0] = 71685;
    pidInfo.pidsCount = 1;
    pidInfo.containerId = "container123";
    responseData.push_back(pidInfo);

    ContainerPidsForCInputMsg outputInfo(responseData);
    outputInfo.Serialize();
    response_data->buffer = outputInfo.SerializedData();
    response_data->length = outputInfo.SerializedDataSize();
    return VM_ERROR;
}

TEST_F(TestContainerSDKService, testConvertPidParamFromCToGoSDK)
{
    pid_param param = {0};
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "2");
    param.pids[0] = 71685;
    param.pids_size = 1;
    pid_mem_info *infos = NULL;
    uint32_t infoSize = 0;

    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_pidInfos));
    auto ret = ubs_container_info_query(&param, &infos, &infoSize);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestContainerSDKService, testConvertPidParamFromCToGoSDK2)
{
    pid_param param = {0};
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "2");
    param.pids[0] = 71685;
    param.pids_size = 1;
    pid_mem_info *infos = NULL;
    uint32_t infoSize = 0;
    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_pidInfos_failed));
    auto ret = ubs_container_info_query(&param, &infos, &infoSize);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSDKService, testConvertPidParamFromCToGoSDK3)
{
    pid_param param = {0};
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "2");
    param.pids[0] = 71685;
    param.pids_size = 1;
    pid_mem_info *infos = NULL;
    uint32_t infoSize = 0;
    auto ret = ubs_container_info_query(&param, &infos, &infoSize);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSDKService, testUbsContainerInjectWaterLine)
{
    watermark_t param;
    param.highWaterMark = 123;
    param.lowWaterMark = 456;
    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_inject_waterLine_failed));
    auto ret = ubs_container_inject_waterLine(&param);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSDKService, testUbsContainerInjectWaterLine2)
{
    watermark_t param;
    param.highWaterMark = 123;
    param.lowWaterMark = 456;
    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_inject_waterLine));
    auto ret = ubs_container_inject_waterLine(&param);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestContainerSDKService, testUbsContainerGetContainerPids)
{
    container_id_list container_list;
    std::string data = "b29b7dbe53e8362090d66e4324a7b1732947f14b5dcdbb17507659bb358d1ea4";
    const std::size_t maxSize = data.size() + 1;
    strcpy_s(container_list.containerId[0], maxSize, data.c_str());
    container_list.containerIdSize = 1;

    container_pid_info *infos = NULL;
    uint32_t infoSize = 0;
    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_get_container_pids));
    auto ret = ubs_container_get_container_pids(&container_list, &infos, &infoSize);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestContainerSDKService, testUbsContainerGetContainerPids2)
{
    container_id_list container_list;
    std::string data = "b29b7dbe53e8362090d66e4324a7b1732947f14b5dcdbb17507659bb358d1ea4";
    const std::size_t maxSize = data.size() + 1;
    strcpy_s(container_list.containerId[0], maxSize, data.c_str());
    container_list.containerIdSize = 1;

    container_pid_info *infos = NULL;
    uint32_t infoSize = 0;
    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_get_container_pids_failed));
    auto ret = ubs_container_get_container_pids(&container_list, &infos, &infoSize);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSDKService, ubs_virt_agent_waterline_mem_borrow_InvalidParameters)
{
    char **borrowIds = nullptr;
    uint32_t idsSize = 0;
    int32_t ret = ubs_virt_agent_waterline_mem_borrow(nullptr, &borrowIds, &idsSize);
    EXPECT_EQ(ret, VM_ERROR);
}

uint32_t test_ubse_invoke_call_waterline_mem_borrow(uint16_t module_code, uint16_t op_code,
                                                    const ubse_api_buffer_t *request_data,
                                                    ubse_api_buffer_t *response_data)
{
    std::vector<std::string> borrowIds{"0", "1"};
    MemContainerWaterLineMemBorrowOutputMsg outputInfo{borrowIds};
    outputInfo.Serialize();
    response_data->buffer = outputInfo.SerializedData();
    response_data->length = outputInfo.SerializedDataSize();
    return VM_OK;
}

TEST_F(TestContainerSDKService, ubs_virt_agent_waterline_mem_borrow_Success)
{
    GTEST_SKIP();
    mem_borrow_request_t request{{"0", {{0, 0}, {1, 1}}, 2}, {100, 200}, 2, {80, 60}};
    char **borrowIds = nullptr;
    uint32_t idsSize = 0;
    testing::internal::CaptureStdout();
    MOCKER(ubse_invoke_call).stubs().will(invoke(test_ubse_invoke_call_waterline_mem_borrow));
    int32_t ret = ubs_virt_agent_waterline_mem_borrow(&request, &borrowIds, &idsSize);
    EXPECT_EQ(ret, VM_OK);

    free(borrowIds);
}

} // namespace ubse::vm::ut