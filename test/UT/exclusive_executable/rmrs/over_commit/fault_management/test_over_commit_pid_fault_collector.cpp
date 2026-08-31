/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include "ubse_com.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "mem_borrow_executor.h"
#include "mempooling_message.h"
#include "mockcpp/mokc.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_collector.h"
#include "over_commit_pid_fault_error_util.h"
#include "over_commit_pid_fault_state_store.h"
#include "over_commit_storage.h"
#include "rmrs_serialize.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace ubse::mem::controller {
// mockcpp的IsEqual对参数做==比较，为DebtInfo提供比较操作符（仅测试用，须定义在其命名空间供ADL查找）
static bool operator==(const UbseNumaMemoryDebtInfo& lhs, const UbseNumaMemoryDebtInfo& rhs)
{
    return lhs.borrowNodeId == rhs.borrowNodeId && lhs.name == rhs.name;
}
} // namespace ubse::mem::controller

namespace mempooling {

using DebtInfoList = std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo>;
using GetDebtInfosWithRetryFunc = MpResult (*)(DebtInfoList&);

class TestPidFaultErrorCodeCollector : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

// ==================== 透传规则与错误明细工具（error_util） ====================

/*
 * 用例描述：多错误并存时透传时序最早一条（先发生的往往是级联失败根因），明细可拼接检索
 * 前置条件：无
 * 步骤：构造不同时序的错误记录调用EarliestFaultErrorCode/JoinFaultErrorRecords
 * 预期：返回最早一条错误码；拼接串含关键字[OvercommitFaultErr]、总数与逐条明细
 */
TEST_F(TestPidFaultErrorCodeCollector, FaultErrorRecords_EarliestPassthroughAndDetailJoin)
{
    // 透传规则: 取时序最早一条（采集失败先于IPC失败 → 透传采集错误码）
    std::vector<FaultErrorRecord> records = {
        {MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR, "node=A peer collect failed"},
        {MEM_POOLING_FAULT_IPC_ERROR, "node=B rpc failed"},
    };
    EXPECT_EQ(EarliestFaultErrorCode(records), MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);

    // 时序反转: IPC先发生 → 透传IPC错误码
    records = {
        {MEM_POOLING_FAULT_IPC_ERROR, "node=B rpc failed"},
        {MEM_POOLING_FAULT_MIGRATE_ERROR, "task=t1 migrate failed"},
    };
    EXPECT_EQ(EarliestFaultErrorCode(records), MEM_POOLING_FAULT_IPC_ERROR);

    // 空列表=全部成功
    EXPECT_EQ(EarliestFaultErrorCode({}), MEM_POOLING_OK);

    // 故障特殊码识别: 六类故障码为已定义，普通错误码需调用方归一
    EXPECT_TRUE(IsDefinedFaultErrorCode(MEM_POOLING_FAULT_RETURN_MEM_ERROR));
    EXPECT_TRUE(IsDefinedFaultErrorCode(MEM_POOLING_FAULT_LACK_REMOTE_MEM_ERROR));
    EXPECT_FALSE(IsDefinedFaultErrorCode(MEM_POOLING_ERROR));

    // 明细拼接: 含关键字/总数/逐条明细，供grep "OvercommitFaultErr"一键检索
    std::string joined = JoinFaultErrorRecords(records);
    EXPECT_NE(joined.find("[OvercommitFaultErr]"), std::string::npos);
    EXPECT_NE(joined.find("total=2"), std::string::npos);
    EXPECT_NE(joined.find("task=t1 migrate failed"), std::string::npos);
}

// ==================== P1-1: 账本查询失败 → n=5 RESOURCE_COLLECT ====================

/*
 * 用例描述：账本查询失败时，采集阶段返回资源采集错误码
 * 前置条件：GetDebtInfosWithRetry持续失败
 * 步骤：调用CollectDebtInfo
 * 预期：返回MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR
 */
TEST_F(TestPidFaultErrorCodeCollector, CollectDebtInfo_RetryExhausted_ReturnResourceCollectError)
{
    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfosWithRetry, GetDebtInfosWithRetryFunc)
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    PidFaultCollector collector;
    OverCommitFaultContext context;
    MpResult ret = collector.CollectDebtInfo("node1", context);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
}

// ==================== 实际借入节点收集（防借用成环排除集） ====================

static DebtInfoList gCollectorMockDebtInfos;

MpResult MockCollectorGetDebtInfos(DebtInfoList& debtInfos)
{
    debtInfos = gCollectorMockDebtInfos;
    return MEM_POOLING_OK;
}

/*
 * 用例描述：CollectDebtInfo从全集群账本收集实际借入节点（含与故障节点无关的借用记录）
 * 前置条件：账本含多条记录（lentNode既有故障节点也有非故障节点）
 * 步骤：调用CollectDebtInfo
 * 预期：context.actualBorrowInNodeIds收集全量记录的borrowNodeId（去重，不做场景专属过滤）
 */
TEST_F(TestPidFaultErrorCodeCollector, CollectDebtInfo_CollectsActualBorrowInNodes)
{
    gCollectorMockDebtInfos.resize(3);
    gCollectorMockDebtInfos[0].borrowNodeId = "node1"; // 从故障节点node0借入
    gCollectorMockDebtInfos[0].lentNodeId = "node0";
    gCollectorMockDebtInfos[1].borrowNodeId = "node2"; // 与故障节点无关的存量借用也必须收集
    gCollectorMockDebtInfos[1].lentNodeId = "node5";
    gCollectorMockDebtInfos[2].borrowNodeId = "node1"; // 重复节点去重
    gCollectorMockDebtInfos[2].lentNodeId = "node0";

    MOCKER_CPP(&MemBorrowExecutor::GetDebtInfosWithRetry, GetDebtInfosWithRetryFunc)
        .stubs()
        .will(invoke(MockCollectorGetDebtInfos));

    PidFaultCollector collector;
    OverCommitFaultContext context;
    ASSERT_EQ(collector.CollectDebtInfo("node0", context), MEM_POOLING_OK);

    std::unordered_set<std::string> expected = {"node1", "node2"};
    EXPECT_EQ(context.actualBorrowInNodeIds, expected);
    // 空集群（GetNodeById查不到节点）下affected分析的节点状态过滤不采信，符合预期
    EXPECT_TRUE(context.affectedBorrowInNodeIds.empty());
}

// ==================== P1-4: PID查询全节点RPC失败 → n=1 IPC_ERROR ====================

/*
 * 用例描述：全部借入节点PID查询RPC失败（区别于部分失败降级），返回IPC错误码
 * 前置条件：UbseRpcSend失败
 * 步骤：构造单受影响节点上下文，调用QueryPidMemDistribution
 * 预期：返回MEM_POOLING_FAULT_IPC_ERROR
 */
TEST_F(TestPidFaultErrorCodeCollector, QueryPidMemDistribution_AllNodesRpcFail_ReturnIpcError)
{
    MOCKER_CPP(&MpConfiguration::GetMpSceneType, MpSceneType(*)(MpConfiguration*))
        .stubs()
        .will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(&ubse::com::UbseRpcSend, uint32_t(*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*,
                                                    const ubse::com::UbseComRespHandler&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    PidFaultCollector collector;
    OverCommitFaultContext context;
    context.affectedBorrowInNodeIds = {"nodeA"};
    context.nodeToFaultNumaIds["nodeA"] = {0};
    MpResult ret = collector.QueryPidMemDistribution("node1", context);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_IPC_ERROR);
    EXPECT_FALSE(context.nodeReachability["nodeA"].ubseReachable);
}

// RPC成功但对端采集失败: 模拟借入节点libvirt停等采集依赖异常，响应携带采集错误码
static uint32_t MockRpcSendWithPeerCollectError(const ubse::com::UbseComEndpoint& endpoint, const UbseByteBuffer& req,
                                                void* ctx, const ubse::com::UbseComRespHandler& handler)
{
    (void)endpoint;
    (void)req;
    FaultPidQueryResponse resp;
    resp.retCode = MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    RmrsOutStream out;
    FaultPidQueryResponseSerialization(out, resp);
    UbseByteBuffer buf = {.data = out.GetBufferPointer(), .len = out.GetSize(), .freeFunc = nullptr};
    handler(ctx, buf, MEM_POOLING_OK);
    return MEM_POOLING_OK;
}

// ==================== P1-5: RPC通但对端采集失败 → 透传对端采集错误码 ====================

/*
 * 用例描述：RPC通信正常但借入节点采集失败（如libvirt服务停），全节点失败时透传对端错误码
 * 前置条件：UbseRpcSend成功，响应retCode为采集错误
 * 步骤：构造单受影响节点上下文，调用QueryPidMemDistribution
 * 预期：透传MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR（而非归为IPC错误）
 */
TEST_F(TestPidFaultErrorCodeCollector, QueryPidMemDistribution_PeerCollectFail_PassThroughError)
{
    MOCKER_CPP(&MpConfiguration::GetMpSceneType, MpSceneType(*)(MpConfiguration*))
        .stubs()
        .will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(&ubse::com::UbseRpcSend, uint32_t(*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*,
                                                    const ubse::com::UbseComRespHandler&))
        .stubs()
        .will(invoke(MockRpcSendWithPeerCollectError));

    PidFaultCollector collector;
    OverCommitFaultContext context;
    context.affectedBorrowInNodeIds = {"nodeA"};
    context.nodeToFaultNumaIds["nodeA"] = {0};
    MpResult ret = collector.QueryPidMemDistribution("node1", context);

    EXPECT_EQ(ret, MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR);
    EXPECT_FALSE(context.nodeReachability["nodeA"].ubseReachable);
}

// RPC成功且响应携带待恢复numa: 模拟借入节点smap纳管查询失败且采集占用非0
static uint32_t MockRpcSendWithPendingNuma(const ubse::com::UbseComEndpoint& endpoint, const UbseByteBuffer& req,
                                           void* ctx, const ubse::com::UbseComRespHandler& handler)
{
    (void)endpoint;
    (void)req;
    FaultPidQueryResponse resp;
    resp.retCode = MEM_POOLING_OK;
    resp.pendingFaultNumaIds = {5};
    RmrsOutStream out;
    FaultPidQueryResponseSerialization(out, resp);
    UbseByteBuffer buf = {.data = out.GetBufferPointer(), .len = out.GetSize(), .freeFunc = nullptr};
    handler(ctx, buf, MEM_POOLING_OK);
    return MEM_POOLING_OK;
}

/*
 * 用例描述：借入节点响应携带待恢复故障numa（pendingFaultNumaIds随序列化透传）
 * 预期：master存入nodeToPendingFaultNumaIds，可达性不受影响（task_builder据此跳过该numa）
 */
TEST_F(TestPidFaultErrorCodeCollector, QueryPidMemDistribution_PendingNumaStored)
{
    MOCKER_CPP(&MpConfiguration::GetMpSceneType, MpSceneType(*)(MpConfiguration*))
        .stubs()
        .will(returnValue(MpSceneType::VIRTUAL_SCENE));
    MOCKER_CPP(&ubse::com::UbseRpcSend, uint32_t(*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*,
                                                    const ubse::com::UbseComRespHandler&))
        .stubs()
        .will(invoke(MockRpcSendWithPendingNuma));

    PidFaultCollector collector;
    OverCommitFaultContext context;
    context.affectedBorrowInNodeIds = {"nodeA"};
    context.nodeToFaultNumaIds["nodeA"] = {5};
    MpResult ret = collector.QueryPidMemDistribution("node1", context);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    EXPECT_TRUE(context.nodeReachability["nodeA"].ubseReachable);
    ASSERT_EQ(context.nodeToPendingFaultNumaIds["nodeA"].size(), 1U);
    EXPECT_EQ(context.nodeToPendingFaultNumaIds["nodeA"].count(5), 1U);
}

} // namespace mempooling
