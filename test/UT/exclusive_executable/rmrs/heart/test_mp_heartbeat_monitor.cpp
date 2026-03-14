#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "mp_heartbeat_monitor.h"
#undef private

#include "mp_json_util.h"
#include "mp_sync_data_helper.h"
#include "ubse_election.h"
#include "ubse_com.h"
#include "turbo_def.h"
#include "mempooling_message.h"
#include "securec.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling {
using namespace ubse::election;
using namespace ubse::com;
using namespace mempooling::heart;
using namespace mempooling::message;
using namespace mempooling::sync;

class TestMpHeartBeatMonitor : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto &monitor = MpHeartBeatMonitor::Instance();
        monitor.running = false;
        monitor.faultNodeVec.clear();
    }

    void TearDown() override
    {
        auto &monitor = MpHeartBeatMonitor::Instance();
        monitor.running = false;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        GlobalMockObject::verify();
    }
};

TEST_F(TestMpHeartBeatMonitor, InitSucceed)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(&MpHeartBeatMonitor::GetHeartBeat, void (*)()).stubs().will(returnValue(0));

    auto &monitor = MpHeartBeatMonitor::Instance();
    auto ret = monitor.Init();
    EXPECT_EQ(ret, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    monitor.running = false;
}

TEST_F(TestMpHeartBeatMonitor, InitFailed1)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(1));

    auto &monitor = MpHeartBeatMonitor::Instance();
    auto ret = monitor.Init();
    EXPECT_EQ(ret, 1);
    monitor.running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

uint32_t FunctionCallerForTestSucceed(const std::string &function, const TurboByteBuffer &params,
                                      TurboByteBuffer &result)
{
    return 0;
}

TEST_F(TestMpHeartBeatMonitor, GetHeartBeatSucceed1)
{
    OSTurboFunctionCaller temp = MempoolingMessage::osturboFunctionCaller;
    MempoolingMessage::osturboFunctionCaller = FunctionCallerForTestSucceed;

    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    monitor.GetHeartBeat();

    MempoolingMessage::osturboFunctionCaller = temp;
}

uint32_t FunctionCallerForTestFailed(const std::string &function, const TurboByteBuffer &params,
                                     TurboByteBuffer &result)
{
    return 1;
}

TEST_F(TestMpHeartBeatMonitor, GetHeartBeatFailed1)
{
    OSTurboFunctionCaller temp = MempoolingMessage::osturboFunctionCaller;
    MempoolingMessage::osturboFunctionCaller = FunctionCallerForTestFailed;

    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    monitor.GetHeartBeat();

    MempoolingMessage::osturboFunctionCaller = temp;
}

TEST_F(TestMpHeartBeatMonitor, AddFaultNodeSucceed)
{
    auto &monitor = MpHeartBeatMonitor::Instance();
    auto ret = monitor.AddFaultNode("Node0");
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, 0);
}

TEST_F(TestMpHeartBeatMonitor, AddFaultNodeFailed1)
{
    auto &monitor = MpHeartBeatMonitor::Instance();
    auto ret = monitor.AddFaultNode("Node0");
    ret = monitor.AddFaultNode("Node0");
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, 1);
}

TEST_F(TestMpHeartBeatMonitor, DelFaultNodeSucceed)
{
    auto &monitor = MpHeartBeatMonitor::Instance();
    auto ret = monitor.AddFaultNode("Node0");
    ret = monitor.DelFaultNode("Node0");
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, 0);
}

bool RackMemConvertVector2JsonStrForTest(const JSON_VEC &strVec, JSON_STR &jsonStr)
{
    jsonStr = "0";
    return true;
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeFaultFailed1)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(1));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeFault());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeFaultFailed2)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(returnValue(false));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeFault());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeFaultFailed3)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(invoke(RackMemConvertVector2JsonStrForTest));
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
                                        const UbseComRespHandler &handler))
        .stubs()
        .will(returnValue(1));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeFault());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeFaultSucceed)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(invoke(RackMemConvertVector2JsonStrForTest));
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
                                        const UbseComRespHandler &handler))
        .stubs()
        .will(returnValue(0));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeFault());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeRecoverFailed1)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(1));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeRecover());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeRecoverFailed2)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(returnValue(false));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeRecover());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeRecoverFailed3)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(invoke(RackMemConvertVector2JsonStrForTest));
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
                                        const UbseComRespHandler &handler))
        .stubs()
        .will(returnValue(1));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeRecover());
}

TEST_F(TestMpHeartBeatMonitor, CurrentNodeRecoverSucceed)
{
    MOCKER_CPP(UbseGetMasterInfo, uint32_t(*)(UbseRoleInfo &)).stubs().will(returnValue(0));
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(invoke(RackMemConvertVector2JsonStrForTest));
    MOCKER_CPP(UbseRpcSend, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseByteBuffer &reqData, void *ctx,
                                        const UbseComRespHandler &handler))
        .stubs()
        .will(returnValue(0));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    EXPECT_NO_THROW(monitor.CurrentNodeRecover());
}

TEST_F(TestMpHeartBeatMonitor, GetFaultNodeVecSucceed)
{
    std::vector<std::string> vec;
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.running = false;
    vec = monitor.GetFaultNodeVec();
    EXPECT_EQ(vec.size(), 0);
}

TEST_F(TestMpHeartBeatMonitor, MpHeartBeatMonitorFromJsonFailed)
{
    MOCKER_CPP(JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &, JSON_VEC &))
        .stubs()
        .will(returnValue(false));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.faultNodeVec.push_back("Node0");
    auto ret = monitor.FromJson("");
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, false);
}

TEST_F(TestMpHeartBeatMonitor, MpHeartBeatMonitorFromJsonSucceed)
{
    MOCKER_CPP(JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &, JSON_VEC &))
        .stubs()
        .will(returnValue(true));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.faultNodeVec.push_back("Node0");
    auto ret = monitor.FromJson("");
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, true);
}

TEST_F(TestMpHeartBeatMonitor, MpHeartBeatMonitorToJsonFailed)
{
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(returnValue(false));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.faultNodeVec.push_back("Node0");
    auto ret = monitor.ToJson();
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, "");
}

TEST_F(TestMpHeartBeatMonitor, MpHeartBeatMonitorToJsonSucceed)
{
    MOCKER_CPP(JsonUtil::RackMemConvertVector2JsonStr, bool (*)(const JSON_VEC &, JSON_STR &))
        .stubs()
        .will(returnValue(true));
    auto &monitor = MpHeartBeatMonitor::Instance();
    monitor.faultNodeVec.push_back("Node0");
    auto ret = monitor.ToJson();
    monitor.faultNodeVec.clear();
    EXPECT_EQ(ret, "");
}

bool MockRackMemConvertJsonStr2Vec(const JSON_STR &jsonStr, JSON_VEC &strVec)
{
    strVec.emplace_back("Node0");
    return true;
}

TEST_F(TestMpHeartBeatMonitor, AddFaultNodeRecvHandlerTest)
{
    UbseByteBuffer req;
    req.len = 1;
    req.data = new uint8_t[req.len];
    UbseByteBuffer resp;

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Vec));

    MOCKER_CPP(&mempooling::heart::MpHeartBeatMonitor::AddFaultNode, MpResult(*)(const std::string &nodeId))
        .stubs()
        .will(returnValue(0));

    auto ret = AddFaultNodeRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMpHeartBeatMonitor, AddFaultNodeRecvHandlerTest2)
{
    UbseByteBuffer req;
    req.len = 1;
    req.data = new uint8_t[req.len];
    UbseByteBuffer resp;

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Vec));

    MOCKER_CPP(&mempooling::heart::MpHeartBeatMonitor::AddFaultNode, MpResult(*)(const std::string &nodeId))
        .stubs()
        .will(returnValue(0));

    MOCKER(memcpy_s).stubs().will(returnValue(MEM_POOLING_ERROR));

    auto ret = AddFaultNodeRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMpHeartBeatMonitor, AddFaultNodeResHandlerTest)
{
    void *ctx = nullptr;
    UbseByteBuffer respData;
    uint32_t resCode = MEM_POOLING_OK;
    AddFaultNodeResHandler(ctx, respData, resCode);
}

TEST_F(TestMpHeartBeatMonitor, DelFaultNodeResHandlerTest)
{
    void *ctx = nullptr;
    UbseByteBuffer respData;
    uint32_t resCode = MEM_POOLING_OK;
    DelFaultNodeResHandler(ctx, respData, resCode);
}

TEST_F(TestMpHeartBeatMonitor, DelFaultNodeRecvHandlerFailed)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;
    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(returnValue(false));
    auto ret = DelFaultNodeRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMpHeartBeatMonitor, DelFaultNodeRecvHandlerTest)
{
    UbseByteBuffer req;
    req.len = 1;
    req.data = new uint8_t[req.len];
    UbseByteBuffer resp;

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Vec));

    MOCKER_CPP(&mempooling::heart::MpHeartBeatMonitor::DelFaultNode, MpResult(*)(const std::string &nodeId))
        .stubs()
        .will(returnValue(0));

    auto ret = DelFaultNodeRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMpHeartBeatMonitor, DelFaultNodeRecvHandlerTest2)
{
    UbseByteBuffer req;
    req.len = 1;
    req.data = new uint8_t[req.len];
    UbseByteBuffer resp;

    MOCKER_CPP(&JsonUtil::RackMemConvertJsonStr2Vec, bool (*)(const JSON_STR &jsonStr, JSON_VEC &strVec))
        .stubs()
        .will(invoke(MockRackMemConvertJsonStr2Vec));

    MOCKER_CPP(&mempooling::heart::MpHeartBeatMonitor::DelFaultNode, MpResult(*)(const std::string &nodeId))
        .stubs()
        .will(returnValue(0));

    auto ret = DelFaultNodeRecvHandler(req, resp);
    delete[] req.data;
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMpHeartBeatMonitor, FaultNodeNotifySucceed)
{
    MOCKER_CPP(&MpHeartBeatMonitor::FromJson, bool (*)(MpHeartBeatMonitor *, const std::string &))
        .stubs()
        .will(returnValue(false));
    UbseByteBuffer buffer;
    auto ret = heart::FaultNodeNotify(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

static const std::string EMPTY_STRING{""};
static const std::string FULL_STRING{"1"};

TEST_F(TestMpHeartBeatMonitor, FaultNodeGetDataFailed1)
{
    MOCKER_CPP(&MpHeartBeatMonitor::ToJson, std::string(*)(MpHeartBeatMonitor *))
        .stubs()
        .will(returnValue(EMPTY_STRING));
    UbseByteBuffer buffer;
    auto ret = heart::FaultNodeGetData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMpHeartBeatMonitor, FaultNodeGetDataFailed2)
{
    MOCKER_CPP(&MpHeartBeatMonitor::ToJson, std::string(*)(MpHeartBeatMonitor *))
        .stubs()
        .will(returnValue(FULL_STRING));
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    UbseByteBuffer buffer;
    auto ret = heart::FaultNodeGetData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMpHeartBeatMonitor, FaultNodeGetDataSucceed)
{
    MOCKER_CPP(&MpHeartBeatMonitor::ToJson, std::string(*)(MpHeartBeatMonitor *))
        .stubs()
        .will(returnValue(FULL_STRING));
    UbseByteBuffer buffer;
    auto ret = heart::FaultNodeGetData(buffer);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    delete[] buffer.data;
}

TEST_F(TestMpHeartBeatMonitor, SubModuleInitFailed1)
{
    MpHeartBeatSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMpHeartBeatMonitor, SubModuleInitFailed2)
{
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(1));
    MpHeartBeatSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMpHeartBeatMonitor, SubModuleInitFailed3)
{
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(0))
        .then(returnValue(1));
    MpHeartBeatSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMpHeartBeatMonitor, SubModuleInitFailed4)
{
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MpHeartBeatMonitor::Init, uint32_t(*)(MpHeartBeatMonitor *)).stubs().will(returnValue(1));
    MpHeartBeatSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMpHeartBeatMonitor, SubModuleInitSucceed)
{
    MOCKER_CPP(&UbseRegRpcService, uint32_t(*)(const UbseComEndpoint &endpoint, const UbseComServiceHandler &handler))
        .stubs()
        .will(returnValue(0));
    MOCKER_CPP(&MpHeartBeatMonitor::Init, uint32_t(*)(MpHeartBeatMonitor *)).stubs().will(returnValue(0));
    MpHeartBeatSubModule obj;
    auto ret = obj.Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}
} // namespace mempooling