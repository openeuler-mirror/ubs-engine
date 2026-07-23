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

#include "scenario.h"
#include "tests/client/client_cases.h"
#include "tests/election/election_cases.h"
#include "tests/fault/fault_cases.h"
#include "tests/mem_borrow/mem_borrow_cases.h"
#include "tests/smoke/smoke_cases.h"
#include "tests/topo/topo_cases.h"
#include "tests/urma/urma_qos_cases.h"

using ubse::it::infra::Tongsuan1dFullMeshSingleNodeScenario;

// ====================================================================
// P0 测试 — 接口自身正确性
// ====================================================================

// ==================== Client P0 ====================

// P0-Init-Ok-01: 默认路径初始化
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0InitOk01)
{
    ubse::it::tests::client::RunP0InitOk01(Cluster());
}

// P0-Init-Ok-02: 指定路径初始化
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0InitOk02)
{
    ubse::it::tests::client::RunP0InitOk02(Cluster());
}

// P0-Init-OverLen-01: 路径超长
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0InitOverLen01)
{
    ubse::it::tests::client::RunP0InitOverLen01(Cluster());
}

// P0-Init-BadParam-01: 路径不存在
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0InitBadParam01)
{
    ubse::it::tests::client::RunP0InitBadParam01(Cluster());
}

// P0-Fin-Ok-01: 正常销毁后SDK不可用
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0FinOk01)
{
    ubse::it::tests::client::RunP0FinOk01(Cluster());
}

// ==================== Topo P0 (单节点) ====================

// P0-NodeList-Ok-01: 单节点查询
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0NodeListOk01)
{
    ubse::it::tests::topo::RunP0NodeListOk01(Cluster());
}

// P0-NodeList-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0NodeListNullPtr01)
{
    ubse::it::tests::topo::RunP0NodeListNullPtr01(Cluster());
}

// P0-LocalGet-Ok-01: 查询本节点
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0LocalGetOk01)
{
    ubse::it::tests::topo::RunP0LocalGetOk01(Cluster());
}

// P0-LocalGet-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0LocalGetNullPtr01)
{
    ubse::it::tests::topo::RunP0LocalGetNullPtr01(Cluster());
}

// ==================== URMA QoS SDK P0 ====================

// P0-UrmaQosCreate-Ok-01: 创建单优先级配置
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0UrmaQosCreateOk01)
{
    ubse::it::tests::urma_qos::RunP0UrmaQosCreateOk01(Cluster());
}

// ==================== CLI URMA QoS P0 ====================

// P0-CliCreateQos-Ok-01: 创建成功
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateQosOk01)
{
    ubse::it::tests::urma_qos::RunP0CliCreateQosOk01(Cluster());
}

// P0-CliCreateQos-BadParam-01: 缺少必要参数
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateQosBadParam01)
{
    ubse::it::tests::urma_qos::RunP0CliCreateQosBadParam01(Cluster());
}

// P0-CliCreateQos-InvalidVal-01: 无效priority
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateQosInvalidVal01)
{
    ubse::it::tests::urma_qos::RunP0CliCreateQosInvalidVal01(Cluster());
}

// P0-CliCreateQos-Dup-01: 重复priority
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateQosDup01)
{
    ubse::it::tests::urma_qos::RunP0CliCreateQosDup01(Cluster());
}

// P0-CliCreateQos-OverCnt-01: count超过上限
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateQosOverCnt01)
{
    ubse::it::tests::urma_qos::RunP0CliCreateQosOverCnt01(Cluster());
}

// P0-CliCreateQos-BadParam-02: pri/cir数量不匹配
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateQosBadParam02)
{
    ubse::it::tests::urma_qos::RunP0CliCreateQosBadParam02(Cluster());
}

// P0-CliDisplayQos-Ok-01: 查询空(未创建)
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliDisplayQosOk01)
{
    ubse::it::tests::urma_qos::RunP0CliDisplayQosOk01(Cluster());
}

// ==================== Mem SHM P0 (单节点) ====================

// P0-ShmFaultReg-NullPtr-01: NULL handler
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0ShmFaultRegNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0ShmFaultRegNullPtr01(Cluster());
}

// P0-FdFaultReg-NullPtr-01: NULL handler
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0FdFaultRegNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0FdFaultRegNullPtr01(Cluster());
}

// ==================== Mem NUMA P0 (单节点) ====================

// P0-NumaStatGet-Fld-01: 本节点字段校验
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0NumaStatGetFld01)
{
    ubse::it::tests::mem_borrow::RunP0NumaStatGetFld01(Cluster());
}

// P0-NumaStatGet-NotExist-01: 不存在的slot_id
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0NumaStatGetNotExist01)
{
    ubse::it::tests::mem_borrow::RunP0NumaStatGetNotExist01(Cluster());
}

// P0-NumaStatGet-NullPtr-01: 空指针
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0NumaStatGetNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0NumaStatGetNullPtr01(Cluster());
}

// P0-NumaFaultReg-NullPtr-01: NULL handler
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0NumaFaultRegNullPtr01)
{
    ubse::it::tests::mem_borrow::RunP0NumaFaultRegNullPtr01(Cluster());
}

// ==================== CLI Node P0 ====================

// P0-CliNode-Ok-01: 查询本节点
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliNodeOk01)
{
    ubse::it::tests::topo::RunP0CliNodeOk01(Cluster());
}

// P0-CliNode-BadParam-01: 不存在的 nodeId
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliNodeBadParam01)
{
    ubse::it::tests::topo::RunP0CliNodeBadParam01(Cluster());
}

// ==================== CLI Mem P0 (单节点) ====================

// P0-CliCreateNuma-InvalidChar-01: CLI create numa 非法name
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateNumaInvalidChar01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateNumaInvalidChar01(Cluster());
}

// P0-CliCreateFd-InvalidVal-01: CLI create fd size=0
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateFdInvalidVal01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateFdInvalidVal01(Cluster());
}

// P0-CliCreateShare-OverLen-01: CLI create share name超长
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliCreateShareOverLen01)
{
    ubse::it::tests::mem_borrow::RunP0CliCreateShareOverLen01(Cluster());
}

// P0-CliDelMem-NotExist-01: CLI delete 不存在的内存
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliDelMemNotExist01)
{
    ubse::it::tests::mem_borrow::RunP0CliDelMemNotExist01(Cluster());
}

// P0-CliBorrowDetail-Ok-01: CLI borrow_detail 无借用时为空
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliBorrowDetailEmpty01)
{
    ubse::it::tests::mem_borrow::RunP0CliBorrowDetailEmpty01(Cluster());
}

// P0-CliDetachMem-NotReady-01: CLI detach 未attach
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliDetachMemNotReady01)
{
    ubse::it::tests::mem_borrow::RunP0CliDetachMemNotReady01(Cluster());
}

// ==================== CLI URMA QoS P0 补充 ====================

// P0-CliDelQos-NotReady-01: CLI delete urma-qos 未创建
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P0CliDelQosNotReady01)
{
    ubse::it::tests::urma_qos::RunP0CliDelQosNotReady01(Cluster());
}

// ====================================================================
// P1 测试 — 对外行为语义
// ====================================================================

// ==================== URMA QoS SDK P1 ====================

// P1-UrmaQosCreate-CreateGetMatch-01: 创建+查询双优先级配置
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P1UrmaQosCreateGetMatch01)
{
    ubse::it::tests::urma_qos::RunP1UrmaQosCreateGetMatch01(Cluster());
}

// P1-UrmaQosCreate-Lifecycle-01: 创建→查询→删除→查询空
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P1UrmaQosLifecycle01)
{
    ubse::it::tests::urma_qos::RunP1UrmaQosLifecycle01(Cluster());
}

// ==================== CLI URMA QoS P1 ====================

// P1-CliCreateQos-ParamVariant-01: 双优先级创建
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P1CliCreateQosParamVariant01)
{
    ubse::it::tests::urma_qos::RunP1CliCreateQosParamVariant01(Cluster());
}

// P1-CliDelQos-DeleteGetFail-01: 删除成功
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P1CliDelQosOk01)
{
    ubse::it::tests::urma_qos::RunP1CliDelQosOk01(Cluster());
}

// P1-CliDisplayQos-CrossConsist-01: 全生命周期
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P1CliDisplayQosCrossConsist01)
{
    ubse::it::tests::urma_qos::RunP1CliDisplayQosCrossConsist01(Cluster());
}

// ====================================================================
// P2 测试 — 鲁棒性
// ====================================================================

// ==================== CLI URMA QoS P2 ====================

// P2-CliCreateQos-InvalidVal-01: bandwidth=0
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, P2CliCreateQosInvalidVal01)
{
    ubse::it::tests::urma_qos::RunP2CliCreateQosInvalidVal01(Cluster());
}

// ====================================================================
// 冒烟 / 选举 / 故障
// ====================================================================

// 冒烟测试：验证单节点启动后进程存活
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, SmokeStartupAndSurvive)
{
    ubse::it::tests::smoke::RunStartupTest(Cluster());
}

// 选举测试：验证单节点集群中节点"1"成为主节点
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunSingleNodeElectionTest(Cluster());
}

// 单节点BMC故障：注入BMC下电事件，验证单节点场景下RAS直接ack成功
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, BmcFaultSingleNode)
{
    ubse::it::tests::fault::RunBmcFaultSingleNodeTest(Cluster(), "1");
}
