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

#include "test_ubse_mem_controller_common.h"
#include <ubse_com_module.h>
#include <ubse_error.h>
#include "message/ubse_mem_operation_resp_simpo.h"
#include "ubse_election.h"
#include "ubse_mem_account.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_helper.h"
#include "ubse_mmi_def.h"

namespace ubse::mem_controller::ut {
using namespace mem::controller;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::decoder::utils;
using namespace ubse::election;
static constexpr uint32_t UBSE_MAX_USR_INFO_LEN = 32;

void BuildOperationMockSet()
{
    std::shared_ptr<com::UbseComModule> module = std::make_shared<com::UbseComModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<com::UbseComModule>).stubs().will(returnValue(module));
    const auto func =
        &com::UbseComModule::RpcSend<mem::controller::message::UbseMemOperationRespSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
}
void TestUbseMemControllerCommonHelper::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerCommonHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerCommonHelper, SendParamSwitcherTest)
{
    UbseMemOperationResp resp;
    auto errorCode = UBSE_OK;
    MOCKER_CPP(&IsSdkRequest).stubs().will(returnValue(true));
    UbseRoleInfo currentNode;
    currentNode.nodeRole = election::ELECTION_ROLE_MASTER;
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentNode)).will(returnValue(UBSE_OK));
    BuildOperationMockSet();
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::SHARED_BORROW));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::SHARED_ATTACH));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::SHARED_DETACH));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::FD_BORROW));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::SHARED_RETURN));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::NUMA_BORROW));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::NUMA_RETURN));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::FD_RETURN));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::COMMON_RETURN));
    EXPECT_EQ(UBSE_OK, BuildOperationRespWhenSuccess(resp, errorCode, MemOperationType::COMMON_RETURN));
}

TEST_F(TestUbseMemControllerCommonHelper, InitializeResponseTest)
{
    UbseMemReturnReq req;
    UbseMemOperationResp resp;
    InitializeResponse(req, resp);
    EXPECT_EQ(req.name, resp.name);
    EXPECT_EQ(req.requestNodeId, resp.requestNodeId);
    EXPECT_EQ(req.requestId, resp.requestId);
}

TEST_F(TestUbseMemControllerCommonHelper, ImportToAddDecoderEntryTest)
{
    std::pair<uint32_t, uint32_t> chipDiePair;
    UbseMemNumaBorrowImportObj importObj;
    UbseMemObmmInfo ubseMemObmmInfo;
    ubseMemObmmInfo.memId = 1;
    importObj.exportObmmInfo.push_back(ubseMemObmmInfo);
    ImportDecoderParam importParam;
    chipDiePair = std::make_pair(0x1234, 0x5678);
    importParam = {.importType = 0x01,
                   .decoderIdx = 0x02,
                   .portSet = 0x12345678,
                   .flag = 0x87654321,
                   .handle = 0xABCDEF0123456789};
    auto res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
    EXPECT_EQ(UBSE_ERROR, res);
}

TEST_F(TestUbseMemControllerCommonHelper, ImportToAddDecoderEntryNotPreOnlineTest)
{
    std::pair<uint32_t, uint32_t> chipDiePair;
    UbseMemNumaBorrowImportObj importObj;
    UbseMemObmmInfo ubseMemObmmInfo;
    ubseMemObmmInfo.memId = 1;
    importObj.exportObmmInfo.push_back(ubseMemObmmInfo);
    ImportDecoderParam importParam;
    chipDiePair = std::make_pair(0x1234, 0x5678);
    importParam = {.importType = 0x01,
                   .decoderIdx = 0x02,
                   .portSet = 0x12345678,
                   .flag = 0x87654321,
                   .handle = 0xABCDEF0123456789};
    auto res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
    EXPECT_EQ(UBSE_ERROR, res);
}

TEST_F(TestUbseMemControllerCommonHelper, UnImportToAddDecoderEntryTest)
{
    std::pair<uint32_t, uint32_t> chipDiePair;
    UbseMemNumaBorrowImportObj importObj;
    UbseMemObmmInfo ubseMemObmmInfo;
    ubseMemObmmInfo.memId = 1;
    importObj.exportObmmInfo.push_back(ubseMemObmmInfo);
    UbseMamiMemImportResult importResult;
    importResult.marId = 0;
    importObj.status.decoderResult.push_back(importResult);
    UnimportToDelDecoderEntry(chipDiePair, importObj.status, 0);
    EXPECT_EQ(0, importObj.status.importResults.size());
}

// 测试用例1: 正常情况，所有参数都有效
TEST_F(TestUbseMemControllerCommonHelper, UbseMemCreateWithLenderReqIsValidTest)
{
    UbseMemBorrower borrower = {"node1"};
    std::string name = "valid_name";
    uint64_t size = 4 * 1024 * 1024;
    UbseMemNumaLender lender{.slotId = 1, .socketId = 1, .numaId = 0, .size = size};
    std::vector<UbseMemNumaLender> lenders;
    lenders.push_back(lender);
    // 1. 测试所有参数都有效的情况
    {
        EXPECT_EQ(UBSE_OK, UbseMemCreateWithLenderReqIsValid(name, borrower, lenders));
    }
    // 2. 测试名称无效的情况
    {
        std::string invalidName = "invalid_name!";
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemCreateWithLenderReqIsValid(invalidName, borrower, lenders));
    }
    // 3. 测试 borrower 的 nodeId 为空的情况
    {
        UbseMemBorrower emptyBorrower = {""};
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemCreateWithLenderReqIsValid(name, emptyBorrower, lenders));
    }
    // 4. 测试 lenders 为空的情况
    {
        std::vector<UbseMemNumaLender> emptyLenders;
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemCreateWithLenderReqIsValid(name, borrower, emptyLenders));
    }
    // 5. 测试 lenders 数量超过最大值的情况
    {
        std::vector<UbseMemNumaLender> tooManyLenders(5, {.slotId = 1, .socketId = 1, .numaId = 0, .size = size});
        EXPECT_EQ(UBSE_ERR_INVALID_ARG,
                  UbseMemCreateWithLenderReqIsValid(name, borrower, tooManyLenders));
    }
    // 6. 测试 lender 的 size 小于最小值的情况
    {
        std::vector<UbseMemNumaLender> smallLender{{.slotId = 1, .socketId = 1, .numaId = 0, .size = 0}};
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemCreateWithLenderReqIsValid(name, borrower, smallLender));
    }
}

TEST_F(TestUbseMemControllerCommonHelper, ConvertUbseMemNumaCreateWithLenderReqTest)
{
    // 初始化测试数据
    std::string name = "test_numa";
    uint64_t size = 4 * 1024 * 1024;
    ;
    UbseMemBorrower borrower = {"1", 0, 0, name};
    std::vector<UbseMemNumaLender> lenders = {{0, 1, 0, size}, {1, 2, 1, size}};
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN] = {0}; // 假设的 usrInfo 数据
    UbseRoleInfo currentRoleInfo;
    currentRoleInfo.nodeId = "1";
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    UbseMemNumaBorrowReq numaBorrowReq;
    // 调用被测函数
    auto ret = ConvertUbseMemNumaCreateWithLenderReq(name, borrower, lenders, usrInfo, numaBorrowReq);
    // 验证返回值
    EXPECT_EQ(ret, UBSE_OK);
    // 验证 numaBorrowReq 的各个字段
    EXPECT_EQ(numaBorrowReq.name, name);
    EXPECT_EQ(numaBorrowReq.requestNodeId, currentRoleInfo.nodeId);
    EXPECT_EQ(numaBorrowReq.importNodeId, borrower.nodeId);
    EXPECT_EQ(numaBorrowReq.srcSocket, borrower.affinitySocketId);
    EXPECT_EQ(numaBorrowReq.udsInfo.uid, borrower.uid);
    EXPECT_EQ(numaBorrowReq.udsInfo.username, borrower.username);
    EXPECT_EQ(numaBorrowReq.size, lenders.size() * size); // 假设的 lenders 的 size 总和

    // 验证 lenderLocs 和 lenderSizes
    EXPECT_EQ(numaBorrowReq.lenderLocs.size(), lenders.size());
    EXPECT_EQ(numaBorrowReq.lenderSizes.size(), lenders.size());
    for (size_t i = 0; i < lenders.size(); ++i) {
        EXPECT_EQ(numaBorrowReq.lenderLocs[i].nodeId, std::to_string(lenders[i].slotId));
        EXPECT_EQ(numaBorrowReq.lenderLocs[i].numaId, static_cast<uint32_t>(lenders[i].numaId));
        EXPECT_EQ(numaBorrowReq.lenderSizes[i], lenders[i].size);
    }
    // 验证 usrInfo 是否正确复制
    EXPECT_EQ(memcmp(numaBorrowReq.usrInfo, usrInfo, UBSE_MAX_USR_INFO_LEN), 0);
}

TEST_F(TestUbseMemControllerCommonHelper, UbseMemCreateReqIsValidTest)
{
    // 公用的测试数据初始化
    std::string validName = "valid_name";
    std::string invalidName = "!invalid@name"; // 假设util::CheckName不接受特殊字符
    std::string emptyNodeId = "";
    std::string validNodeId = "1";
    const size_t minMemSize = 4 * 1024 * 1024;

    // 测试用例1: 所有参数有效
    {
        UbseMemBorrower borrower{validNodeId};
        UbseMemNumaCreateOpt opt{minMemSize};
        ASSERT_EQ(UbseMemCreateReqIsValid(validName, borrower, opt), UBSE_OK);
    }
    // 测试用例2: 名称无效
    {
        UbseMemBorrower borrower{validNodeId};
        UbseMemNumaCreateOpt opt{minMemSize};
        ASSERT_EQ(UbseMemCreateReqIsValid(invalidName, borrower, opt), UBSE_ERR_INVALID_ARG);
    }
    // 测试用例3: nodeId为空
    {
        UbseMemBorrower borrower{emptyNodeId};
        UbseMemNumaCreateOpt opt{minMemSize};
        ASSERT_EQ(UbseMemCreateReqIsValid(validName, borrower, opt), UBSE_ERR_INVALID_ARG);
    }
    // 测试用例4: 内存大小不足
    {
        UbseMemBorrower borrower{validNodeId};
        UbseMemNumaCreateOpt opt{minMemSize - 1}; // 故意减1使大小无效
        ASSERT_EQ(UbseMemCreateReqIsValid(validName, borrower, opt), UBSE_ERR_INVALID_ARG);
    }
}

TEST_F(TestUbseMemControllerCommonHelper, ConvertUbseMemNumaCreateReqTest)
{
    // 初始化测试数据
    std::string validName = "test_numa";
    std::string longName(100, 'a');       // 超长名称测试
    uint64_t validSize = 4 * 1024 * 1024; // 4MB
    uint64_t invalidSize = 0;             // 无效大小

    // 构造有效的borrower对象
    UbseMemBorrower validBorrower;
    validBorrower.nodeId = "1";
    validBorrower.affinitySocketId = 0;
    validBorrower.uid = 1000;
    validBorrower.username = "test_user";

    // 构造无效的borrower对象
    UbseMemBorrower invalidBorrower;
    invalidBorrower.nodeId = ""; // 空nodeId

    // 构造有效的opt对象
    UbseMemNumaCreateOpt validOpt;
    validOpt.size = validSize;
    validOpt.highWatermark = 80;
    for (auto& byte : validOpt.usrInfo) {
        byte = 'A';
    }
    // 构造无效的opt对象
    UbseMemNumaCreateOpt invalidOpt;
    invalidOpt.size = invalidSize;

    UbseRoleInfo currentRoleInfo;
    currentRoleInfo.nodeId = "1";
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
    // 测试用例1: 正常转换
    {
        UbseMemNumaBorrowReq req;
        EXPECT_EQ(ConvertUbseMemNumaCreateReq(validName, validBorrower, validOpt, req), UBSE_OK);
        // 基础字段验证
        EXPECT_EQ(req.name, validName);
        EXPECT_EQ(req.importNodeId, validBorrower.nodeId);
        EXPECT_EQ(req.size, validOpt.size);
        EXPECT_EQ(memcmp(req.usrInfo, validOpt.usrInfo, UBSE_MAX_USR_INFO_LEN), 0);

        // 新增关键字段验证
        EXPECT_EQ(req.requestNodeId, currentRoleInfo.nodeId);
        EXPECT_EQ(req.srcSocket, validBorrower.affinitySocketId);
        EXPECT_EQ(req.udsInfo.uid, validBorrower.uid);
        EXPECT_EQ(req.udsInfo.username, validBorrower.username);
        EXPECT_EQ(req.distance, static_cast<ubse::adapter_plugins::mmi::UbseMemDistance>(validOpt.distance));
        EXPECT_EQ(req.highWatermark, validOpt.highWatermark);
    }
    // 测试用例2: 用户信息拷贝失败
    {
        // 模拟memcpy_s失败
        MOCKER_CPP(&memcpy_s).stubs().will(returnValue(1));
        UbseMemNumaBorrowReq req;
        EXPECT_EQ(ConvertUbseMemNumaCreateReq(validName, validBorrower, validOpt, req),
                  UBSE_ERR_INVALID_ARG);
    }
}

TEST_F(TestUbseMemControllerCommonHelper, UbseMemCreateWithCandidateReqIsValid)
{
    // 公用的测试数据初始化
    const std::string validName = "valid_name";
    const std::string invalidName = "!invalid@name";
    const std::string validNodeId = "1";
    const size_t minMemSize = 4 * 1024 * 1024;
    const std::vector<std::string> validSlotIds = {"1", "2", "3"};
    const std::vector<uint32_t> emptySlotIds = {};

    UbseMemNumaCandidateOpt validOpt;
    validOpt.slotIds = validSlotIds;
    validOpt.size = minMemSize;
    UbseMemBorrower validBorrower{validNodeId};
    // 测试用例1: 所有参数有效
    {
        ASSERT_EQ(UbseMemCreateWithCandidateReqIsValid(validName, validBorrower, validOpt), UBSE_OK);
    }
    // 测试用例2: 名称无效
    {
        ASSERT_EQ(UbseMemCreateWithCandidateReqIsValid(invalidName, validBorrower, validOpt),
                  UBSE_ERR_INVALID_ARG);
    }
    // 测试用例3: nodeId为空
    {
        UbseMemBorrower borrower;
        ASSERT_EQ(UbseMemCreateWithCandidateReqIsValid(validName, borrower, validOpt),
                  UBSE_ERR_INVALID_ARG);
    }
    // 测试用例4: 内存大小不足
    {
        UbseMemNumaCandidateOpt opt;
        opt.size = minMemSize - 1;
        opt.slotIds = validSlotIds;
        ASSERT_EQ(UbseMemCreateWithCandidateReqIsValid(validName, validBorrower, opt),
                  UBSE_ERR_INVALID_ARG);
    }
    // 测试用例5: slotIds为空
    {
        UbseMemNumaCandidateOpt opt;
        opt.size = minMemSize;
        ASSERT_EQ(UbseMemCreateWithCandidateReqIsValid(validName, validBorrower, opt),
                  UBSE_ERR_INVALID_ARG);
    }
    // 测试用例6: 多个无效条件同时存在
    {
        UbseMemBorrower borrower;
        UbseMemNumaCandidateOpt opt;
        opt.size = minMemSize - 1;
        ASSERT_EQ(UbseMemCreateWithCandidateReqIsValid(invalidName, borrower, opt), UBSE_ERR_INVALID_ARG);
    }
}

TEST_F(TestUbseMemControllerCommonHelper, ConvertUbseMemNumaCreateWithCandidateReq)
{
    // 初始化测试数据
    std::string name = "test_numa";
    uint64_t size = 4 * 1024 * 1024;
    UbseMemBorrower borrower = {"1", 0, 0, "test_user"};
    std::vector<std::string> slotIds = {"1", "2", "3"};
    // 设置用户信息
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN] = {0};
    for (size_t i = 0; i < UBSE_MAX_USR_INFO_LEN; ++i) {
        usrInfo[i] = 'A';
    }
    // 设置当前节点信息
    UbseRoleInfo currentRoleInfo;
    currentRoleInfo.nodeId = "1";
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));

    // 准备被测对象
    UbseMemNumaCandidateOpt opt;
    opt.size = size;
    opt.highWatermark = 80;
    opt.slotIds = slotIds;
    for (size_t i = 0; i < UBSE_MAX_USR_INFO_LEN; ++i) {
        opt.usrInfo[i] = usrInfo[i];
    }
    // 准备输出对象
    UbseMemNumaBorrowReq numaBorrowReq;

    // 调用被测函数
    auto ret = ConvertUbseMemNumaCreateWithCandidateReq(name, borrower, opt, numaBorrowReq);

    // 验证返回值
    EXPECT_EQ(ret, UBSE_OK);

    // 验证基本字段
    EXPECT_EQ(numaBorrowReq.name, name);
    EXPECT_EQ(numaBorrowReq.requestNodeId, currentRoleInfo.nodeId);
    EXPECT_EQ(numaBorrowReq.importNodeId, borrower.nodeId);
    EXPECT_EQ(numaBorrowReq.size, opt.size);
    EXPECT_EQ(numaBorrowReq.distance, static_cast<ubse::adapter_plugins::mmi::UbseMemDistance>(opt.distance));
    EXPECT_EQ(numaBorrowReq.srcSocket, borrower.affinitySocketId);
    EXPECT_EQ(numaBorrowReq.highWatermark, opt.highWatermark);

    // 验证用户信息
    EXPECT_EQ(memcmp(numaBorrowReq.usrInfo, usrInfo, UBSE_MAX_USR_INFO_LEN), 0);

    // 验证用户认证信息
    EXPECT_EQ(numaBorrowReq.udsInfo.uid, borrower.uid);
    EXPECT_EQ(numaBorrowReq.udsInfo.username, borrower.username);

    // 验证候选节点列表
    EXPECT_EQ(numaBorrowReq.candidateNodeList.size(), slotIds.size());
    for (size_t i = 0; i < slotIds.size(); ++i) {
        EXPECT_EQ(numaBorrowReq.candidateNodeList[i], slotIds[i]);
    }

    // 测试用例2: 用户信息拷贝失败
    {
        // 模拟memcpy_s失败
        MOCKER_CPP(&memcpy_s).stubs().will(returnValue(1));
        UbseMemNumaBorrowReq req;
        EXPECT_EQ(ConvertUbseMemNumaCreateWithCandidateReq(name, borrower, opt, req),
                  UBSE_ERR_INVALID_ARG);
    }
}

TEST_F(TestUbseMemControllerCommonHelper, UbseMemDeleteReqIsValidTest)
{
    // 测试用例1: 正常情况，所有参数都有效
    {
        std::string validName = "valid_name";
        UbseMemBorrower validBorrower = {"node1"};
        EXPECT_EQ(UBSE_OK, UbseMemDeleteReqIsValid(validName, validBorrower));
    }
    // 测试用例2: 名称无效的情况
    {
        std::string invalidName = "!invalid@name";
        UbseMemBorrower validBorrower = {"node1"};
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemDeleteReqIsValid(invalidName, validBorrower));
    }
    // 测试用例3: borrower的nodeId为空的情况
    {
        std::string validName = "valid_name";
        UbseMemBorrower emptyBorrower = {""};
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemDeleteReqIsValid(validName, emptyBorrower));
    }
    // 测试用例4: 名称和nodeId同时无效
    {
        std::string invalidName = "!invalid@name";
        UbseMemBorrower emptyBorrower = {""};
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemDeleteReqIsValid(invalidName, emptyBorrower));
    }
    // 测试用例5: 空名称
    {
        std::string emptyName = "";
        UbseMemBorrower validBorrower = {"node1"};
        EXPECT_EQ(UBSE_ERR_INVALID_ARG, UbseMemDeleteReqIsValid(emptyName, validBorrower));
    }
}

TEST_F(TestUbseMemControllerCommonHelper, UbseMemAddrCreateReqIsValidTest)
{
    // 测试用例1: 所有参数都有效
    {
        std::string validName = "valid_name";
        UbseMemBorrower validBorrower = {"node1"};
        UbseMemProcessLender validLender;
        validLender.pid = 1234;
        validLender.vaLists = {{.size = 4 * 1024 * 1024}}; // 4MB

        EXPECT_EQ(UBSE_OK, UbseMemAddrCreateReqIsValid(validName, validBorrower, validLender));
    }

    // 测试用例2: 名称无效
    {
        std::string invalidName = "!invalid@name";
        UbseMemBorrower validBorrower = {"node1"};
        UbseMemProcessLender validLender;
        validLender.pid = 1234;
        validLender.vaLists = {{.size = 4 * 1024 * 1024}};

        EXPECT_EQ(UBSE_ERR_INVALID_ARG,
                  UbseMemAddrCreateReqIsValid(invalidName, validBorrower, validLender));
    }

    // 测试用例3: borrower的nodeId为空
    {
        std::string validName = "valid_name";
        UbseMemBorrower emptyBorrower = {""};
        UbseMemProcessLender validLender;
        validLender.pid = 1234;
        validLender.vaLists = {{.size = 4 * 1024 * 1024}};

        EXPECT_EQ(UBSE_ERR_INVALID_ARG,
                  UbseMemAddrCreateReqIsValid(validName, emptyBorrower, validLender));
    }

    // 测试用例4: lender的pid为0
    {
        std::string validName = "valid_name";
        UbseMemBorrower validBorrower = {"node1"};
        UbseMemProcessLender invalidLender;
        invalidLender.pid = 0;
        invalidLender.vaLists = {{.size = 4 * 1024 * 1024}};

        EXPECT_EQ(UBSE_ERR_INVALID_ARG,
                  UbseMemAddrCreateReqIsValid(validName, validBorrower, invalidLender));
    }

    // 测试用例5: lender的vaLists为空
    {
        std::string validName = "valid_name";
        UbseMemBorrower validBorrower = {"node1"};
        UbseMemProcessLender invalidLender;
        invalidLender.pid = 1234;
        invalidLender.vaLists = {};

        EXPECT_EQ(UBSE_OK, UbseMemAddrCreateReqIsValid(validName, validBorrower, invalidLender));
    }

    // 测试用例6: lender的vaLists中size小于最小值
    {
        std::string validName = "valid_name";
        UbseMemBorrower validBorrower = {"node1"};
        UbseMemProcessLender invalidLender;
        invalidLender.pid = 1234;
        invalidLender.vaLists = {{.size = 0}}; // 小于最小值

        EXPECT_EQ(UBSE_ERR_INVALID_ARG,
                  UbseMemAddrCreateReqIsValid(validName, validBorrower, invalidLender));
    }

    // 测试用例7: 多个无效条件同时存在
    {
        std::string invalidName = "!invalid@name";
        UbseMemBorrower emptyBorrower = {""};
        UbseMemProcessLender invalidLender;
        invalidLender.pid = 0;
        invalidLender.vaLists = {{.size = 0}};

        EXPECT_EQ(UBSE_ERR_INVALID_ARG,
                  UbseMemAddrCreateReqIsValid(invalidName, emptyBorrower, invalidLender));
    }
}

TEST_F(TestUbseMemControllerCommonHelper, ConvertUbseMemAddrCreateReqTest)
{
    // 测试用例1: 正常情况，所有参数都有效
    {
        std::string name = "valid_name";
        UbseMemBorrower borrower = {"node1", 0, 0, ""};
        UbseMemProcessLender lender;
        lender.pid = 1234;
        lender.socketId = 1;
        lender.slotId = 1;
        lender.vaLists = {{.addr = 0x1000, .size = 4 * 1024 * 1024}}; // 4MB
        uint32_t flag = 0;
        UbseMemAddrBorrowReq addrBorrowReq;
        // 设置当前节点信息
        UbseRoleInfo currentRoleInfo;
        currentRoleInfo.nodeId = "1";
        MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
        ConvertUbseMemAddrCreateReq(name, borrower, lender, flag, addrBorrowReq);
        // 验证结果
        EXPECT_EQ(addrBorrowReq.name, name);
        EXPECT_EQ(addrBorrowReq.requestNodeId, currentRoleInfo.nodeId);
        EXPECT_EQ(addrBorrowReq.importNodeId, borrower.nodeId);
        EXPECT_EQ(addrBorrowReq.srcSocket, borrower.affinitySocketId);
        EXPECT_EQ(addrBorrowReq.exportNodeId, std::to_string(lender.slotId));
        EXPECT_EQ(addrBorrowReq.dstSocket, lender.socketId);
        EXPECT_EQ(addrBorrowReq.exportPid, lender.pid);
        EXPECT_EQ(addrBorrowReq.wrDelayComp, flag);

        // 验证地址列表
        ASSERT_EQ(addrBorrowReq.exportAddrList.size(), 1);
        EXPECT_EQ(addrBorrowReq.exportAddrList[0].addr, 0x1000);
        EXPECT_EQ(addrBorrowReq.exportAddrList[0].size, 4 * 1024 * 1024);
    }
    // 测试用例2: 空名称
    {
        std::string name = "";
        UbseMemBorrower borrower = {"node1", 0, 0, ""};
        UbseMemProcessLender lender;
        lender.pid = 1234;
        lender.vaLists = {{.size = 4 * 1024 * 1024}};
        uint32_t flag = 0;
        UbseMemAddrBorrowReq addrBorrowReq;
        ConvertUbseMemAddrCreateReq(name, borrower, lender, flag, addrBorrowReq);
        EXPECT_EQ(addrBorrowReq.name, name); // 空名称应该被保留
    }
    // 测试用例3: 空vaLists
    {
        std::string name = "valid_name";
        UbseMemBorrower borrower = {"node1", 0, 0, ""};
        UbseMemProcessLender lender;
        lender.pid = 1234;
        lender.vaLists = {}; // 空列表
        uint32_t flag = 0;
        UbseMemAddrBorrowReq addrBorrowReq;
        ConvertUbseMemAddrCreateReq(name, borrower, lender, flag, addrBorrowReq);
        EXPECT_TRUE(addrBorrowReq.exportAddrList.empty());
    }
    // 测试用例4: 多个地址
    {
        std::string name = "multi_addr";
        UbseMemBorrower borrower = {"node1", 0, 0, ""};
        UbseMemProcessLender lender;
        lender.pid = 1234;
        lender.vaLists = {{.addr = 0x1000, .size = 4 * 1024 * 1024}, {.addr = 0x2000, .size = 8 * 1024 * 1024}};
        uint32_t flag = 0;
        UbseMemAddrBorrowReq addrBorrowReq;
        ConvertUbseMemAddrCreateReq(name, borrower, lender, flag, addrBorrowReq);
        ASSERT_EQ(addrBorrowReq.exportAddrList.size(), 2);
        EXPECT_EQ(addrBorrowReq.exportAddrList[0].addr, 0x1000);
        EXPECT_EQ(addrBorrowReq.exportAddrList[0].size, 4 * 1024 * 1024);
        EXPECT_EQ(addrBorrowReq.exportAddrList[1].addr, 0x2000);
        EXPECT_EQ(addrBorrowReq.exportAddrList[1].size, 8 * 1024 * 1024);
    }
}

TEST_F(TestUbseMemControllerCommonHelper, ConvertUbseMemDeleteReqTest)
{
    // 测试用例1: 正常情况，所有参数都有效
    {
        std::string name = "valid_name";
        UbseMemBorrower borrower = {"node1", 0, 0, ""};
        UbseMemReturnReq returnReq;
        // 设置当前节点信息
        UbseRoleInfo currentRoleInfo;
        currentRoleInfo.nodeId = "1";
        MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
        // 调用被测函数
        ConvertUbseMemDeleteReq(name, borrower, returnReq);
        // 验证结果
        EXPECT_EQ(returnReq.name, name);
        EXPECT_EQ(returnReq.requestNodeId, currentRoleInfo.nodeId);
        EXPECT_EQ(returnReq.importNodeId, borrower.nodeId);
        // 验证requestId是否生成
        EXPECT_NE(returnReq.requestId, 0);
    }

    // 测试用例2: 空名称
    {
        std::string name = "";
        UbseMemBorrower borrower = {"node1", 0, 0, ""};
        UbseMemReturnReq returnReq;
        // 设置当前节点信息
        UbseRoleInfo currentRoleInfo;
        currentRoleInfo.nodeId = "1";
        MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
        // 调用被测函数
        ConvertUbseMemDeleteReq(name, borrower, returnReq);
        // 验证结果
        EXPECT_EQ(returnReq.name, name); // 空名称应该被保留
        EXPECT_EQ(returnReq.requestNodeId, currentRoleInfo.nodeId);
        EXPECT_EQ(returnReq.importNodeId, borrower.nodeId);
        EXPECT_NE(returnReq.requestId, 0);
    }

    // 测试用例3: 空nodeId
    {
        std::string name = "valid_name";
        UbseMemBorrower borrower = {"", 0, 0, ""};
        UbseMemReturnReq returnReq;
        // 设置当前节点信息
        UbseRoleInfo currentRoleInfo;
        currentRoleInfo.nodeId = "1";
        MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().with(outBound(currentRoleInfo)).will(returnValue(UBSE_OK));
        // 调用被测函数
        ConvertUbseMemDeleteReq(name, borrower, returnReq);
        // 验证结果
        EXPECT_EQ(returnReq.name, name);
        EXPECT_EQ(returnReq.requestNodeId, currentRoleInfo.nodeId);
        EXPECT_EQ(returnReq.importNodeId, borrower.nodeId); // 空nodeId应该被保留
        EXPECT_NE(returnReq.requestId, 0);
    }
}
} // namespace ubse::mem_controller::ut
