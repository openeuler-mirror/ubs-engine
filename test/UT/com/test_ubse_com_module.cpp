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

#include "test_ubse_com_module.h"
#include "intercom/ubse_inter_com.h"
#include "ubse_com_module.cpp"
#include "ubse_lcne_module.h"
#include "test_ubse_com.h"
namespace ubse::ut::com {
const std::string IP = "127.0.0.1";
const uint16_t PORT = 1901;
const std::string NODE_ID_MASTER = "Node0";
std::map<std::string, std::pair<std::string, uint16_t>> SERVER_LIST;

void TestUbseComModule::SetUp()
{
    Test::SetUp();
    ubseComModulePtr = std::make_shared<UbseComModule>();
    SERVER_LIST.insert({NODE_ID_MASTER, {IP, PORT}});
}

void TestUbseComModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

UbseResult MockSplitIpPort(const std::string &ipPortStr, std::pair<std::string, uint16_t> &address)
{
    address.first = "127.0.0.1";
    address.second = 8080; // 8080端口号
}

UbseResult MockSplitIdIpStr(const std::string &ipStr, std::map<std::string, std::pair<std::string, uint16_t>> &addrMap)
{
    addrMap.emplace("Node0", std::make_pair("127.0.0.1", 8080)); // 8080端口号
}

std::string CUR_ROLE_STR = ELECTION_ROLE_MASTER;
std::string MockGetCurRole()
{
    return CUR_ROLE_STR;
}

uint32_t MockGetRole(ubse::election::UbseRoleInfo &role)
{
    role.nodeRole = ELECTION_ROLE_MASTER;
    return UBSE_OK;
}

UbseResult MockInitUbseCom(UbseComModule *ubseComModule)
{
    ubseComModule->rpcServer_ = new (std::nothrow) UbseRpcServer(IP, PORT, MASTER_RPC_SERVER_NAME, NODE_ID_MASTER);

    return UBSE_OK;
}

UbseResult MockReturnUbseError()
{
    return UBSE_ERROR;
}

/*
 * 用例描述：
 * 通信模块初始化
 * 测试步骤：
 * 1.启动Initialize
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseComModule, InitializeSuccess)
{
    UbseComModule ubseComModule;
    EXPECT_EQ(UBSE_OK, ubseComModule.Initialize());
}

/*
 * 用例描述：
 * 通信模块UnInitialize
 * 测试步骤：
 * 1.启动UnInitialize
 * 预期结果：
 * 1.成功执行
 */
TEST_F(TestUbseComModule, UnInitializeSuccess)
{
    UbseComModule ubseComModule;
    EXPECT_NO_THROW(ubseComModule.UnInitialize());
}

/*
 * 用例描述：
 * 通信模块启动
 * 测试步骤：
 * 1.CLI场景启动通信模块
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseComModule, CliStartFail)
{
    UbseComModule ubseComModule;
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::CLI));
    auto ret = ubseComModule.Start();
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 通信模块启动
 * 测试步骤：
 * 1.CLI场景启动通信模块
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseComModule, CliStartSuccess)
{
    UbseComModule ubseComModule;
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::CLI));
    auto ret = ubseComModule.Start();
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 通信模块启动rpcServer失败
 * 测试步骤：
 * 1.MANAGER场景启动通信模块
 * 2.Mock函数rpcServer->Start，返回UBSE_ERROR
 * 预期结果：
 * 1.通信模块启动成功失败，返回UBSE_ERROR_INVAL
 */
TEST_F(TestUbseComModule, TestManagerStartRpcServerFailed)
{
    UbseComModule ubseComModule;
    UbseRpcServer rpcServer(IP, PORT, MASTER_RPC_SERVER_NAME, NODE_ID_MASTER);
    MOCKER(&UbseComModule::InitUbseCom).stubs().will(invoke(MockInitUbseCom));
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::MANAGER));
    MOCKER(&UbseComModule::GetCurRoleStr).stubs().will(returnValue(ELECTION_ROLE_MASTER));
    MOCKER_CPP_VIRTUAL(&rpcServer, &UbseRpcServer::Start)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    auto ret = ubseComModule.Start();
    EXPECT_EQ(ret, UBSE_ERROR_CONF_INVALID);
}

/*
 * 用例描述：
 * 通信模块启动ipcServer失败
 * 测试步骤：
 * 1.MANAGER场景启动通信模块
 * 2.Mock函数ipcServer->Start，返回UBSE_ERROR
 * 预期结果：
 * 1.通信模块启动成功失败，返回UBSE_ERROR_INVAL
 */
TEST_F(TestUbseComModule, TestManagerStartIpcServerFailed)
{
    UbseComModule ubseComModule;
    UbseRpcServer rpcServer(IP, PORT, MASTER_RPC_SERVER_NAME, NODE_ID_MASTER);
    MOCKER(&UbseComModule::InitUbseCom).stubs().will(invoke(MockInitUbseCom));
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::MANAGER));
    MOCKER(&UbseComModule::GetCurRoleStr).stubs().will(returnValue(ELECTION_ROLE_MASTER));
    MOCKER(&UbseInterCom::StartQueue).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(&rpcServer, &UbseRpcServer::Start).stubs().will(returnValue(UBSE_OK));
    auto ret = ubseComModule.Start();
    EXPECT_EQ(ret, UBSE_ERROR_CONF_INVALID);
}

/*
 * 用例描述：
 * 通信模块GetCurRoleStr成功
 * 测试步骤：
 * 1.Mock函数GetRole，返回UBSE_OK
 * 2.运行GetCurRoleStr
 * 预期结果：
 * 1.返回空字符串
 */
TEST_F(TestUbseComModule, TestUbseComGetCurRoleStrSuccess)
{
    ubse::election::UbseRoleInfo roleInfo{};
    roleInfo.nodeRole = ELECTION_ROLE_MASTER;
    MOCKER(&ubse::election::UbseGetCurrentNodeInfo).stubs().will(invoke(MockGetRole));

    EXPECT_EQ(ubseComModulePtr->GetCurRoleStr(), ELECTION_ROLE_MASTER);
}

TEST_F(TestUbseComModule, TestUbseComInit)
{
    const std::string localNodeId;
    const std::string localIp;
    EXPECT_EQ(UBSE_OK, ubseComModulePtr->InitUbseCom(localNodeId, localIp));
}

/*
 * 用例描述：
 * 通信模块创建线程池
 * 测试步骤：
 * 1.创建HeartBeatExecutor失败
 * 预期结果：
 * 1.返回UBSE_ERROR_CONF_INVALID
 */
TEST_F(TestUbseComModule, TestCreateHbExecutorFailed)
{
    GTEST_SKIP();
    std::shared_ptr<UbseTaskExecutorModule> ubseTaskExecutorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&context::UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(ubseTaskExecutorModule));
    std::string hbExecutorName = "HeartBeatExecutor";
    uint16_t threadNum = 8;
    uint32_t queueCapacity = 1000;
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(hbExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_ERROR));
    std::string comExecutorName = "ComExecutor";
    threadNum = 16;
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(comExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_ERROR));
    std::string collectExecutorName = "CollectionExecutor";
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(collectExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, CreateRpcExecutor());

    ubseTaskExecutorModule->Remove(hbExecutorName);
    ubseTaskExecutorModule->Remove(comExecutorName);
    ubseTaskExecutorModule->Remove(collectExecutorName);
}

/*
 * 用例描述：
 * 通信模块创建线程池
 * 测试步骤：
 * 1.创建ComExecutor失败
 * 预期结果：
 * 1.返回UBSE_ERROR_CONF_INVALID
 */
TEST_F(TestUbseComModule, TestCreateComExecutorFailed)
{
    GTEST_SKIP();
    std::shared_ptr<UbseTaskExecutorModule> ubseTaskExecutorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&context::UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(ubseTaskExecutorModule));
    std::string hbExecutorName = "HeartBeatExecutor";
    uint16_t threadNum = 8;
    uint32_t queueCapacity = 1000;
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(hbExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_OK));
    std::string comExecutorName = "ComExecutor";
    threadNum = 16;
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(comExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_ERROR));
    std::string collectExecutorName = "CollectionExecutor";
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(collectExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, CreateRpcExecutor());

    ubseTaskExecutorModule->Remove(hbExecutorName);
    ubseTaskExecutorModule->Remove(comExecutorName);
    ubseTaskExecutorModule->Remove(collectExecutorName);
}

/*
 * 用例描述：
 * 通信模块创建线程池
 * 测试步骤：
 * 1.创建CollectionExecutor失败
 * 预期结果：
 * 1.返回UBSE_ERROR_CONF_INVALID
 */
TEST_F(TestUbseComModule, TestCreateCollectExecutorFailed)
{
    GTEST_SKIP();
    std::shared_ptr<UbseTaskExecutorModule> ubseTaskExecutorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&context::UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(ubseTaskExecutorModule));
    std::string hbExecutorName = "HeartBeatExecutor";
    uint16_t threadNum = 8;
    uint32_t queueCapacity = 1000;
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(hbExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_OK));
    std::string comExecutorName = "ComExecutor";
    threadNum = 16;
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(comExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_OK));
    std::string collectExecutorName = "CollectionExecutor";
    MOCKER(&UbseTaskExecutorModule::Create)
        .stubs()
        .with(eq(collectExecutorName), eq(threadNum), eq(queueCapacity))
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, CreateRpcExecutor());

    ubseTaskExecutorModule->Remove(hbExecutorName);
    ubseTaskExecutorModule->Remove(comExecutorName);
    ubseTaskExecutorModule->Remove(collectExecutorName);
}

/*
 * 用例描述：
 * 通信模块创建线程池
 * 测试步骤：
 * 1.创建线程池
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseComModule, TestCreateRpcExecutorSuccess)
{
    std::shared_ptr<UbseTaskExecutorModule> ubseTaskExecutorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&context::UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(ubseTaskExecutorModule));
    std::string hbExecutorName = "HeartBeatExecutor";
    std::string comExecutorName = "ComExecutor";
    std::string collectExecutorName = "CollectionExecutor";
    EXPECT_EQ(UBSE_OK, CreateRpcExecutor());

    ubseTaskExecutorModule->Remove(hbExecutorName);
    ubseTaskExecutorModule->Remove(comExecutorName);
    ubseTaskExecutorModule->Remove(collectExecutorName);
}

/*
 * 用例描述：
 * 线程池执行任务
 * 测试步骤：
 * 1.获取不到线程池模块
 * 2.未创建线程池
 * 3.执行HEARTBEAT任务
 * 4.执行Collection任务
 * 5.执行Com任务
 * 预期结果：
 * 1.任务未被执行
 * 2.任务未被执行
 * 3.HEARTBEAT任务被执行
 * 4.Collection任务被执行
 * 5.Com任务被执行
 */
TEST_F(TestUbseComModule, TestUbseComHandlerExecutor)
{
    bool isExec = false;
    std::function<void()> task = [&isExec]() {
        isExec = true;
    };

    // 步骤1
    executorType type = executorType::HEARTBEAT;
    UbseComHandlerExecutor(task, type);
    EXPECT_EQ(false, isExec);
    isExec = false;

    // 步骤2
    std::shared_ptr<UbseTaskExecutorModule> ubseTaskExecutorModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&context::UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(ubseTaskExecutorModule));
    UbseComHandlerExecutor(task, type);
    EXPECT_EQ(false, isExec);
    isExec = false;

    // 创建线程池
    std::string hbExecutorName = "HeartBeatExecutor";
    std::string comExecutorName = "ComExecutor";
    std::string collectExecutorName = "CollectionExecutor";
    CreateRpcExecutor();

    // 步骤3
    type = executorType::HEARTBEAT;
    UbseComHandlerExecutor(task, type);
    usleep(100000);
    EXPECT_EQ(true, isExec);
    isExec = false;

    // 步骤4
    type = executorType::COM;
    UbseComHandlerExecutor(task, type);
    usleep(100000);
    EXPECT_EQ(true, isExec);
    isExec = false;

    // 释放线程池
    ubseTaskExecutorModule->Remove(hbExecutorName);
    ubseTaskExecutorModule->Remove(comExecutorName);
    ubseTaskExecutorModule->Remove(collectExecutorName);
}

/*
 * 用例描述：
 * 发布事件
 * 测试步骤：
 * 1.获取不到事件模块
 * 2.发布事件成功
 * 预期结果：
 * 1.无异常
 * 2.无异常
 */
TEST_F(TestUbseComModule, TestUbseLinkEventPub)
{
    std::shared_ptr<event::UbseEventModule> ubseEventModule = std::make_shared<event::UbseEventModule>();
    ubseEventModule->Initialize();
    UbseLinkInfo info("1", UbseLinkState::LINK_UP, 1, "1");
    std::vector<UbseLinkInfo> linkInfoList = {info};

    EXPECT_NO_THROW(UbseLinkEventPub(linkInfoList));

    MOCKER(&context::UbseContext::GetModule<event::UbseEventModule>).stubs().will(returnValue(ubseEventModule));
    EXPECT_NO_THROW(UbseLinkEventPub(linkInfoList));
}

/*
 * 用例描述：
 * 读rpc超时时间
 * 测试步骤：
 * 1.获取不到配置模块
 * 2.读不到配置项
 * 3.读到的值超过INT16_MAX
 * 4.成功读到正确的值
 * 预期结果：
 * 1.返回DEFAULTTIMEOUT
 * 2.返回DEFAULTTIMEOUT
 * 3.返回DEFAULTTIMEOUT
 * 4.返回2
 */
TEST_F(TestUbseComModule, TestGetTimeOutValue)
{
    // 步骤1
    EXPECT_EQ(DEFAULTTIMEOUT, GetTimeOutValue());

    // 步骤2
    std::shared_ptr<UbseConfModule> ubseConfModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(DEFAULTTIMEOUT, GetTimeOutValue());
    GlobalMockObject::verify();

    // 步骤3
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    std::string section = "ubse.rpc";
    std::string configKey = "request.timeout";
    uint32_t configVal = INT16_MAX + 1;
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(DEFAULTTIMEOUT, GetTimeOutValue());
    GlobalMockObject::verify();

    // 步骤4
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    configVal = 2;
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(configVal, GetTimeOutValue());
}

/*
 * 用例描述：
 * 读心跳超时时间
 * 测试步骤：
 * 1.获取不到配置模块
 * 2.读不到配置项
 * 3.读到的值超过128
 * 4.成功读到正确的值
 * 预期结果：
 * 1.返回2
 * 2.返回2
 * 3.返回2
 * 4.返回3
 */
TEST_F(TestUbseComModule, TestGetHeartBeatTimeOutValue)
{
    // 步骤1
    EXPECT_EQ(2, GetHeartBeatTimeOutValue());

    // 步骤2
    std::shared_ptr<UbseConfModule> ubseConfModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(2, GetHeartBeatTimeOutValue());
    GlobalMockObject::verify();

    // 步骤3
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    std::string section = "ubse.election";
    std::string configKey = "heartbeat.timeInterval";
    uint32_t configVal = 61000;
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(2, GetHeartBeatTimeOutValue());
    GlobalMockObject::verify();

    // 步骤4
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(ubseConfModule));
    configVal = 3000;
    MOCKER(&UbseConfModule::GetConf<uint32_t>)
        .expects(once())
        .with(eq(section), eq(configKey), outBound(configVal))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(configVal / 1000, GetHeartBeatTimeOutValue());
}

/*
 * 用例描述：
 * 打开Tls
 * 测试步骤：
 * 1.传入空指针
 * 2.传入正确指针
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_OK
 */
TEST_F(TestUbseComModule, TestServerTls)
{
    UbseComBasePtr rpcServer = nullptr;
    EXPECT_EQ(UBSE_ERROR, ServerTls(rpcServer));

    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    rpcServer = new UbseRpcServer(ip, port, name, nodeId);
    EXPECT_EQ(UBSE_OK, ServerTls(rpcServer));
}
/*
 * 用例描述：
 * 查询节点id回调
 * 测试步骤：
 * 1.获取不到lcne模块
 * 2.查询节点id失败
 * 3.查询节点id成功
 * 预期结果：
 * 1.返回false
 * 2.返回false
 * 3.返回true
 */
TEST_F(TestUbseComModule, TestQueryEidByNodeIdCb)
{
    // 步骤1
    std::string nodeId;
    std::string eid;
    bool ret = queryCb(nodeId, eid);
    EXPECT_EQ(false, ret);

    // 步骤2
    std::shared_ptr<ubse::mti::UbseLcneModule> ubseLcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(ubseLcneModule));
    MOCKER(GetBondingEidByNodeId).stubs().will(returnValue(UBSE_ERROR));
    ret = queryCb(nodeId, eid);
    EXPECT_EQ(false, ret);
    GlobalMockObject::verify();

    // 步骤3
    MOCKER(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(ubseLcneModule));
    MOCKER(GetBondingEidByNodeId).stubs().will(returnValue(UBSE_OK));
    ret = queryCb(nodeId, eid);
    EXPECT_EQ(true, ret);
}

/*
 * 用例描述：
 * 启动RpcServer
 * 测试步骤：
 * 1.读tls配置失败
 * 2.rpcServer启动失败
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR_INVAL
 */
TEST_F(TestUbseComModule, TestRpcServerStart)
{
    UbseComModule ubseComModule;
    // 步骤1
    MOCKER(&UbseGetBool).stubs().will(returnValue(UBSE_ERROR));
    ubseComModule.rpcServer_ = nullptr;
    UbseResult ret = ubseComModule.RpcServerStart();
    EXPECT_EQ(UBSE_ERROR, ret);

    // 步骤2
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    ret = ubseComModule.RpcServerStart();
    EXPECT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * 启动通信模块
 * 测试步骤：
 * 1.进程为cli
 * 2.创建线程池失败
 * 3.初始化失败
 * 4.队列启动失败
 * 5.启动通信模块成功
 * 6.RpcServer启动失败
 * 预期结果：
 * 1.返回UBSE_OK
 * 2.返回UBSE_ERROR_CONF_INVALID
 * 3.返回UBSE_ERROR_MODULE_LOAD_FAILED
 * 4.返回UBSE_ERROR_CONF_INVALID
 * 5.返回UBSE_OK
 * 6.返回UBSE_ERROR
 */
TEST_F(TestUbseComModule, TestStart)
{
    GTEST_SKIP();
    UbseComModule ubseComModule;
    // 步骤1
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::CLI));
    UbseResult ret = ubseComModule.Start();
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();

    // 步骤2
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::MANAGER));
    ret = ubseComModule.Start();
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, ret);
    GlobalMockObject::verify();

    // 步骤3
    MOCKER(&UbseContext::GetProcessMode).stubs().will(returnValue(ProcessMode::MANAGER));
    MOCKER(&CreateRpcExecutor).stubs().will(returnValue(UBSE_OK));
    ret = ubseComModule.Start();
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, ret);

    // 步骤4
    MOCKER(&UbseComModule::InitUbseCom).stubs().will(returnValue(UBSE_OK));
    ret = ubseComModule.Start();
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, ret);

    // 步骤5
    MOCKER(&UbseInterCom::StartQueue).stubs().will(returnValue(UBSE_OK));
    ret = ubseComModule.Start();
    EXPECT_EQ(UBSE_OK, ret);

    // 步骤6
    MOCKER(&UbseComModule::RpcServerStart).stubs().will(returnValue(UBSE_ERROR));
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    ret = ubseComModule.Start();
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * 停止通信模块
 * 测试步骤：
 * 1.通信模块停止成功
 * 预期结果：
 * 1.无异常
 */
TEST_F(TestUbseComModule, TestStop)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    EXPECT_NO_THROW(ubseComModule.Stop());
}

/*
 * 用例描述：
 * 判断是否是当前节点
 * 测试步骤：
 * 1.获取当前节点信息失败
 * 2.获取当前节点信息成功
 * 预期结果：
 * 1.返回false
 * 2.返回true
 */
TEST_F(TestUbseComModule, TestIsCurrentNode)
{
    UbseComModule ubseComModule;
    std::string nodeId = "1";
    bool ret = ubseComModule.IsCurrentNode(nodeId);
    EXPECT_EQ(false, ret);

    const std::string id = "1";
    std::string role = "agent";
    UbseRoleInfo roleInfo(id, role);
    MOCKER(&UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    ret = ubseComModule.IsCurrentNode(nodeId);
    EXPECT_EQ(true, ret);
}

/*
 * 用例描述：
 * 获取所有服务端连接信息
 * 测试步骤：
 * 1.获取连接信息失败
 * 2.获取连接信息成功
 * 预期结果：
 * 1.连接信息数组为空
 * 2.连接信息数组非空
 */
TEST_F(TestUbseComModule, TestGetAllServerLinkInfo)
{
    UbseComModule ubseComModule;
    std::vector<UbseLinkInfo> ubseLinkInfos = ubseComModule.GetAllServerLinkInfo();
    EXPECT_EQ(0, ubseLinkInfos.size());

    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    UbseLinkInfo info("1", UbseLinkState::LINK_UP);
    std::vector<UbseLinkInfo> mockLinkInfos{info};
    MOCKER(&UbseComBase::GetAllLinkInfo).expects(once()).will(returnValue(mockLinkInfos));
    ubseLinkInfos = ubseComModule.GetAllServerLinkInfo();
    EXPECT_NE(0, ubseLinkInfos.size());
}

/*
 * 用例描述：
 * 添加服务端链路事件回调
 * 测试步骤：
 * 1.rpcServer为空
 * 2.rpcServer非空
 * 预期结果：
 * 1.无异常
 * 2.无异常
 */
TEST_F(TestUbseComModule, TestAddServerLinkNotifyFunc)
{
    UbseComModule ubseComModule;
    LinkNotifyFunction func;
    EXPECT_NO_THROW(ubseComModule.AddServerLinkNotifyFunc(func));

    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    EXPECT_NO_THROW(ubseComModule.AddServerLinkNotifyFunc(func));
}

/*
 * 用例描述：
 * 根据NodeId获取BondingEid
 * 测试步骤：
 * 1.GetClusterNodeInfoList失败
 * 2.节点列表中未找到匹配nodeId
 * 3.节点列表中找到匹配nodeId
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_OK，bondingEid正确
 */
TEST_F(TestUbseComModule, TestGetBondingEidByNodeId)
{
    std::string bondingEid;
    std::string nodeId = "Node0";
    adapter_plugins::mti::UbseMtiInterface& mtiInterface = adapter_plugins::mti::UbseMtiInterface::GetInstance();

    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetClusterNodeInfoList)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseResult ret = GetBondingEidByNodeId(bondingEid, nodeId);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();

    std::vector<adapter_plugins::mti::UbseMtiNodeInfo> nodeInfos;
    nodeInfos.push_back({"Node1", "eid1"});
    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetClusterNodeInfoList)
        .stubs()
        .with(outBound(nodeInfos))
        .will(returnValue(UBSE_OK));
    ret = GetBondingEidByNodeId(bondingEid, nodeId);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();

    nodeInfos.push_back({"Node0", "eid0"});
    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetClusterNodeInfoList)
        .stubs()
        .with(outBound(nodeInfos))
        .will(returnValue(UBSE_OK));
    ret = GetBondingEidByNodeId(bondingEid, nodeId);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("eid0", bondingEid);
}

/*
 * 用例描述：
 * 启动通信服务
 * 测试步骤：
 * 1.InitUbseCom失败
 * 2.RpcServerStart失败
 * 3.启动队列失败
 * 预期结果：
 * 1.返回UBSE_ERROR_NOMEM
 * 2.返回UBSE_ERROR_INVAL
 * 3.返回UBSE_ERROR
 */
TEST_F(TestUbseComModule, TestStartComService)
{
    std::string localNodeId = "Node0";
    std::string localIp = "127.0.0.1";
    UbseComCallBackForHA newChannelCb = [](const std::string&, const std::string&) { return UBSE_OK; };
    UbseComCallBackForHA brokenChannelCb = [](const std::string&, const std::string&) { return UBSE_OK; };

    UbseComModule ubseComModule1;
    MOCKER(&UbseComModule::InitUbseCom).stubs().will(returnValue(UBSE_ERROR_NOMEM));
    UbseResult ret = ubseComModule1.StartComService(localNodeId, localIp, newChannelCb, brokenChannelCb);
    EXPECT_EQ(UBSE_ERROR_NOMEM, ret);
    GlobalMockObject::verify();

    UbseComModule ubseComModule2;
    ubseComModule2.rpcServer_ = new UbseRpcServer(localIp, PORT, MASTER_RPC_SERVER_NAME, localNodeId);
    MOCKER(&UbseComModule::InitUbseCom).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComModule::RpcServerStart).stubs().will(returnValue(UBSE_ERROR_INVAL));
    ret = ubseComModule2.StartComService(localNodeId, localIp, newChannelCb, brokenChannelCb);
    EXPECT_EQ(UBSE_ERROR_INVAL, ret);
    GlobalMockObject::verify();

    UbseComModule ubseComModule3;
    ubseComModule3.rpcServer_ = new UbseRpcServer(localIp, PORT, MASTER_RPC_SERVER_NAME, localNodeId);
    MOCKER(&UbseComModule::InitUbseCom).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseComModule::RpcServerStart).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseInterCom::StartQueue).stubs().will(returnValue(UBSE_ERROR));
    ret = ubseComModule3.StartComService(localNodeId, localIp, newChannelCb, brokenChannelCb);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
 * 用例描述：
 * 根据IP获取NodeId
 * 测试步骤：
 * 1.rpcServer为空
 * 2.rpcServer非空
 * 预期结果：
 * 1.返回空字符串
 * 2.返回正确的nodeId
 */
TEST_F(TestUbseComModule, TestGetNodeIdByIp)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";

    std::string nodeId = ubseComModule.GetNodeIdByIp(ip);
    EXPECT_EQ("", nodeId);

    ubseComModule.rpcServer_ = new UbseRpcServer(ip, PORT, MASTER_RPC_SERVER_NAME, NODE_ID_MASTER);
    std::string mockNodeId = "Node1";
    MOCKER(&UbseComBase::GetNodeIdByIp).stubs().will(returnValue(mockNodeId));
    nodeId = ubseComModule.GetNodeIdByIp(ip);
    EXPECT_EQ(mockNodeId, nodeId);
}
/*
* 用例描述：
* RegRpcService(uint16_t, uint16_t)通过rpcServer注册成功
* 测试步骤：
* 1.rpcServer_非空，queueRef_为空
* 2.调用RegRpcService
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseComModule, RegRpcServiceWithCodesRpcServerSuccess)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseCommunication::RegUbseComMsgHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = ubseComModule.RegRpcService(1, 2);
    EXPECT_EQ(UBSE_OK, ret);
}

/*

* 用例描述：
* RegRpcService(uint16_t, uint16_t)通过rpcServer注册失败
* 测试步骤：
* 1.rpcServer_非空，queueRef_为空
* 2.调用RegRpcService，RegUbseComMsgHandler返回失败
* 预期结果：
* 1.函数返回UBSE_ERROR
*/
TEST_F(TestUbseComModule, RegRpcServiceWithCodesRpcServerFail)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseCommunication::RegUbseComMsgHandler).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ubseComModule.RegRpcService(1, 2);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
* 用例描述：
* RegRpcService(uint16_t, uint16_t)通过queueRef注册失败
* 测试步骤：
* 1.queueRef_非空，rpcServer_非空
* 2.调用RegRpcService，queueRef注册失败
* 预期结果：
* 1.函数返回queueRef的错误码
*/
TEST_F(TestUbseComModule, RegRpcServiceWithCodesQueueRefFail)
{
    UbseComModule ubseComModule;
    ubseComModule.queueRef_ = std::make_shared<UbseInterCom>();
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    auto ret = ubseComModule.RegRpcService(1001, 2);
    EXPECT_NE(UBSE_OK, ret);
}

/*
* 用例描述：
* RegRpcService(uint16_t, uint16_t)queueRef和rpcServer都注册成功
* 测试步骤：
* 1.queueRef_非空，rpcServer_非空
* 2.调用RegRpcService
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseComModule, RegRpcServiceWithCodesBothSuccess)
{
    UbseComModule ubseComModule;
    ubseComModule.queueRef_ = std::make_shared<UbseInterCom>();
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseCommunication::RegUbseComMsgHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = ubseComModule.RegRpcService(1, 2);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
* 用例描述：
* RpcSend(UbseRpcMessage)通过rpcServer发送成功
* 测试步骤：
* 1.rpcServer_非空，queueRef_为空，IsCurrentNode返回false
* 2.mock EncodeRequestMsg返回非空数据，UbseComMsgSend返回UBSE_OK
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseComModule, RpcSendRpcMessageViaRpcServerSuccess)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(false));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_OK;
    MockUbseRpcMessage resp;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    MOCKER(&UbseCommunication::UbseComMsgSend).stubs().will(returnValue(UBSE_OK));
    auto ret = ubseComModule.RpcSend("Node1", 1, 2, req, resp);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
* 用例描述：
* RpcSend(UbseRpcMessage)通过rpcServer发送失败
* 测试步骤：
* 1.rpcServer_非空，queueRef_为空，IsCurrentNode返回false
* 2.调用RpcSend，UbseComMsgSend返回失败
* 预期结果：
* 1.函数返回UBSE_ERROR
*/
TEST_F(TestUbseComModule, RpcSendRpcMessageViaRpcServerFail)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(false));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_OK;
    MockUbseRpcMessage resp;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    MOCKER(&UbseCommunication::UbseComMsgSend).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ubseComModule.RpcSend("Node1", 1, 2, req, resp);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
* 用例描述：
* RpcSend(UbseRpcMessage)通过queueRef发送失败
* 测试步骤：
* 1.queueRef_非空，IsCurrentNode返回true
* 2.调用RpcSend，queueRef内部GetHandler失败（handlerMap_需要RegMessageHandler）
* 预期结果：
* 1.函数返回非UBSE_OK
*/
TEST_F(TestUbseComModule, RpcSendRpcMessageViaQueueRefFail)

{
    UbseComModule ubseComModule;
    ubseComModule.queueRef_ = std::make_shared<UbseInterCom>();
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(true));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_OK;
    MockUbseRpcMessage resp;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    auto ret = ubseComModule.RpcSend("Node0", 1, 2, req, resp);
    EXPECT_NE(UBSE_OK, ret);
}

/*
* 用例描述：
* RpcAsyncSend(UbseRpcMessage)通过rpcServer发送成功
* 测试步骤：
* 1.rpcServer_非空，queueRef_为空，IsCurrentNode返回false
* 2.mock EncodeRequestMsg返回非空数据，UbseComMsgAsyncSend返回UBSE_OK
* 预期结果：
* 1.函数返回UBSE_OK
*/
TEST_F(TestUbseComModule, RpcAsyncSendRpcMessageViaRpcServerSuccess)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(false));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_OK;
    UbseComCallback cb;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    MOCKER(&UbseCommunication::UbseComMsgAsyncSend).stubs().will(returnValue(UBSE_OK));
    auto ret = ubseComModule.RpcAsyncSend("Node1", 1, 2, req, cb);
    EXPECT_EQ(UBSE_OK, ret);
}

/*
* 用例描述：
* RpcAsyncSend(UbseRpcMessage)通过rpcServer发送失败
* 测试步骤：
* 1.rpcServer_非空，queueRef_为空，IsCurrentNode返回false
* 2.调用RpcAsyncSend，UbseComMsgAsyncSend返回失败
* 预期结果：
* 1.函数返回UBSE_ERROR
*/
TEST_F(TestUbseComModule, RpcAsyncSendRpcMessageViaRpcServerFail)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(false));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_OK;
    UbseComCallback cb;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    MOCKER(&UbseCommunication::UbseComMsgAsyncSend).stubs().will(returnValue(UBSE_ERROR));
    auto ret = ubseComModule.RpcAsyncSend("Node1", 1, 2, req, cb);
    EXPECT_EQ(UBSE_ERROR, ret);
}

/*
* 用例描述：
* RpcAsyncSend(UbseRpcMessage)通过queueRef发送失败
* 测试步骤：
* 1.queueRef_非空，IsCurrentNode返回true
* 2.调用RpcAsyncSend,缺少注册等步骤
* 预期结果：
* 1.函数返回非UBSE_OK（queueRef内部处理）
*/
TEST_F(TestUbseComModule, RpcAsyncSendRpcMessageViaQueueRefFail)
{
    UbseComModule ubseComModule;
    ubseComModule.queueRef_ = std::make_shared<UbseInterCom>();
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(true));
    MockUbseRpcMessage req;
    UbseComCallback cb;
    req.serializeRet = UBSE_OK;
    std::vector<uint8_t> mockBuffer(sizeof(UbseComMessageHead) + 4, 0);
    MOCKER(EncodeRequestMsg).stubs().will(returnValue(std::make_shared<std::vector<uint8_t>>(mockBuffer)));
    auto ret = ubseComModule.RpcAsyncSend("Node0", 1, 2, req, cb);
    EXPECT_NE(UBSE_OK, ret);
}

/*
* 用例描述：
* RpcSend(UbseRpcMessage)序列化失败
* 测试步骤：
* 1.rpcServer_非空，IsCurrentNode返回false
* 2.调用RpcSend，request.Serialize返回失败
* 预期结果：
* 1.函数返回UBSE_ERROR
*/
TEST_F(TestUbseComModule, RpcSendRpcMessageSerializeFail)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(false));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_ERROR;
    MockUbseRpcMessage resp;
    auto ret = ubseComModule.RpcSend("Node1", 1, 2, req, resp);
    EXPECT_NE(UBSE_OK, ret);
}

/*
* 用例描述：
* RpcAsyncSend(UbseRpcMessage)序列化失败
* 测试步骤：
* 1.rpcServer_非空，IsCurrentNode返回false
* 2.调用RpcAsyncSend，request.Serialize返回失败
* 预期结果：
* 1.函数返回UBSE_ERROR
*/
TEST_F(TestUbseComModule, RpcAsyncSendRpcMessageSerializeFail)
{
    UbseComModule ubseComModule;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string nodeId = "1";
    std::string name = "RpcServer";
    ubseComModule.rpcServer_ = new UbseRpcServer(ip, port, name, nodeId);
    MOCKER(&UbseComModule::IsCurrentNode).stubs().will(returnValue(false));
    MockUbseRpcMessage req;
    req.serializeRet = UBSE_ERROR;
    UbseComCallback cb;
    auto ret = ubseComModule.RpcAsyncSend("Node1", 1, 2, req, cb);
    EXPECT_NE(UBSE_OK, ret);
}
} // namespace ubse::ut::com
