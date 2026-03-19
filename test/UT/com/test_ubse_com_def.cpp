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

#include "test_ubse_com_def.h"
#include "test_ubse_com_mock.h"
#include "ubse_com_module.h"
#include "ubse_conf_manager.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"

namespace ubse::ut::com {
using namespace ubse::com;
using VOS_CHAR = char;
std::string g_data = "test";
void TestUbseComDef::SetUp()
{
    Test::SetUp();
}

void TestUbseComDef::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

UbseResult TestRpcMessage::Serialize()
{
    mOutputRawDataSize = g_data.size(); // 4是data空间大小
    mOutputRawData = std::make_unique<uint8_t[]>(g_data.size());
    if (auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, g_data.data(), mOutputRawDataSize); ret != 0) {
        return UBSE_ERROR;
    }
    return SerializeRet;
}

UbseResult TestRpcMessage::Deserialize()
{
    return DeserializeRet;
}

/*
 * 用例描述：
 * 设置和获取通信引擎类型
 * 测试步骤：
 * 1.设置为客户端
 * 2.查询通信引擎类型
 * 预期结果：
 * 1.设置为客户端成功
 * 2.查询通信引擎类型为客户端
 */
TEST_F(TestUbseComDef, TestSetGetEngineType)
{
    UbseComEngineInfo ubseComEngineInfo;
    EXPECT_NO_THROW(ubseComEngineInfo.SetEngineType(UbseEngineType::CLIENT));
    EXPECT_EQ(UbseEngineType::CLIENT, ubseComEngineInfo.GetEngineType());
}

/*
 * 用例描述：
 * 设置和获取通信引擎协议
 * 测试步骤：
 * 1.设置为TCP协议
 * 2.查询通信引擎协议
 * 预期结果：
 * 1.设置为TCP协议成功
 * 2.查询通信引擎为TCP协议
 */
TEST_F(TestUbseComDef, TestSetGetProtocol)
{
    UbseComEngineInfo ubseComEngineInfo;
    EXPECT_NO_THROW(ubseComEngineInfo.SetProtocol(UbseProtocol::TCP));
    EXPECT_EQ(UbseProtocol::TCP, ubseComEngineInfo.GetProtocol());
}

/*
 * 用例描述：
 * 设置和获取通信引擎poll方式
 * 测试步骤：
 * 1.设置为busy poll
 * 2.查询通信引擎poll方式
 * 预期结果：
 * 1.设置为busy poll成功
 * 2.查询通信引擎为busy poll方式
 */
TEST_F(TestUbseComDef, TestSetGetWorkerMode)
{
    UbseComEngineInfo ubseComEngineInfo;
    EXPECT_NO_THROW(ubseComEngineInfo.SetWorkerMode(UbseWorkerMode::NET_BUSY_POLLING));
    EXPECT_EQ(UbseWorkerMode::NET_BUSY_POLLING, ubseComEngineInfo.GetWorkerMode());
}

/*
 * 用例描述：
 * 设置和获取通信引擎节点id
 * 测试步骤：
 * 1.设置节点id为1
 * 2.查询通信引擎节点id
 * 预期结果：
 * 1.设置节点id为1成功
 * 2.查询通信引擎节点id为1
 */
TEST_F(TestUbseComDef, TestSetGetNodeId)
{
    UbseComEngineInfo ubseComEngineInfo;
    std::string nodeId = "1";
    EXPECT_NO_THROW(ubseComEngineInfo.SetNodeId(nodeId));
    EXPECT_EQ(nodeId, ubseComEngineInfo.GetNodeId());
}

/*
 * 用例描述：
 * 设置和获取心跳超时时间
 * 测试步骤：
 * 1.设置心跳超时时间为1
 * 2.查询心跳超时时间
 * 预期结果：
 * 1.设置心跳超时时间为1成功
 * 2.查询心跳超时时间为1
 */
TEST_F(TestUbseComDef, TestSetGetHeartBeatTimeOut)
{
    UbseComEngineInfo ubseComEngineInfo;
    int16_t time = 1;
    EXPECT_NO_THROW(ubseComEngineInfo.SetHeartBeatTimeOut(time));
    EXPECT_EQ(time, ubseComEngineInfo.GetHeartBeatTimeOut());
}

/*
 * 用例描述：
 * 设置和获取rpc超时时间
 * 测试步骤：
 * 1.设置rpc超时时间为1
 * 2.查询rpc超时时间
 * 预期结果：
 * 1.设置rpc超时时间为1成功
 * 2.查询rpc超时时间为1
 */
TEST_F(TestUbseComDef, TestSetGetTimeOut)
{
    UbseComEngineInfo ubseComEngineInfo;
    int16_t time = 1;
    EXPECT_NO_THROW(ubseComEngineInfo.SetTimeOut(time));
    EXPECT_EQ(time, ubseComEngineInfo.GetTimeOut());
}

/*
 * 用例描述：
 * 设置和获取日志回调函数
 * 测试步骤：
 * 1.调用SetLogFunc设置回调函数
 * 2.调用GetLogFunc获取回调函数
 * 3.调用回调函数
 * 预期结果：
 * 1.无异常
 */
TEST_F(TestUbseComDef, TestSetGetLogFunc)
{
    UbseComEngineInfo ubseComEngineInfo;
    UbseComLogFunc cb = [](int level, const char *msg) {
    };
    EXPECT_NO_THROW(ubseComEngineInfo.SetLogFunc(cb));
    EXPECT_NO_THROW(ubseComEngineInfo.GetLogFunc());
}

/*
 * 用例描述：
 * 设置和获取ip信息
 * 测试步骤：
 * 1.设置ip信息127.0.0.1:5000
 * 2.查询ip信息
 * 预期结果：
 * 1.设置ip信息成功
 * 2.查询ip信息为127.0.0.1:5000
 */
TEST_F(TestUbseComDef, TestSetGetIpInfo)
{
    UbseComEngineInfo ubseComEngineInfo;
    std::pair<std::string, uint16_t> ipInfo{"127.0.0.1", 5000};
    EXPECT_NO_THROW(ubseComEngineInfo.SetIpInfo(ipInfo));
    EXPECT_EQ(ipInfo, ubseComEngineInfo.GetIpInfo());
}

/*
 * 用例描述：
 * 设置和获取uds信息
 * 测试步骤：
 * 1.设置uds信息file:5000
 * 2.查询uds信息
 * 预期结果：
 * 1.设置uds信息成功
 * 2.查询uds信息为file:5000
 */
TEST_F(TestUbseComDef, TestSetGetUdsInfo)
{
    UbseComEngineInfo ubseComEngineInfo;
    std::pair<std::string, uint16_t> udsInfo{"file", 5000};
    EXPECT_NO_THROW(ubseComEngineInfo.SetUdsInfo(udsInfo));
    EXPECT_EQ(udsInfo, ubseComEngineInfo.GetUdsInfo());
}

/*
 * 用例描述：
 * 测试是否为uds协议
 * 测试步骤：
 * 1.设置uds协议
 * 2.查询是否为uds协议
 * 3.设置tcp协议
 * 4.查询是否为uds协议
 * 预期结果：
 * 1.设置uds协议成功
 * 2.查询到为uds协议
 * 3.设置tcp协议成功
 * 4.查询到不是uds协议
 */
TEST_F(TestUbseComDef, TestIsUds)
{
    UbseComEngineInfo ubseComEngineInfo;
    ubseComEngineInfo.protocol_ = UbseProtocol::UDS;
    EXPECT_EQ(true, ubseComEngineInfo.IsUds());
    ubseComEngineInfo.protocol_ = UbseProtocol::TCP;
    EXPECT_EQ(false, ubseComEngineInfo.IsUds());
}

/*
 * 用例描述：
 * 测试是否为uds协议
 * 测试步骤：
 * 1.设置uds协议
 * 2.查询是否为uds协议
 * 3.设置tcp协议
 * 4.查询是否为uds协议
 * 预期结果：
 * 1.设置uds协议成功
 * 2.查询到为uds协议
 * 3.设置tcp协议成功
 * 4.查询到不是uds协议
 */
TEST_F(TestUbseComDef, TestIsServerSide)
{
    UbseComEngineInfo ubseComEngineInfo;
    ubseComEngineInfo.engineType_ = UbseEngineType::SERVER;
    EXPECT_EQ(true, ubseComEngineInfo.IsServerSide());
    ubseComEngineInfo.engineType_ = UbseEngineType::CLIENT;
    EXPECT_EQ(false, ubseComEngineInfo.IsServerSide());
}

/*
 * 用例描述：
 * 设置和获取查询节点eid回调函数
 * 测试步骤：
 * 1.调用SetQueryEidByNodeIdCb设置回调函数cb1
 * 2.调用GetQueryEidByNodeIdCb获取回调函数cb2
 * 3.调用回调函数cb1和cb2观察结果是否一致
 * 预期结果：
 * 1.无异常
 * 2.结果一致
 */
TEST_F(TestUbseComDef, TestSetGetQueryEidByNodeIdCb)
{
    UbseComEngineInfo ubseComEngineInfo;
    QueryEidByNodeIdCb cb1 = [](std::string nodeId, std::string &eid) {
        if (nodeId == "TestSetGetQueryEidByNodeIdCb") {
            return true;
        } else {
            return false;
        }
    };
    EXPECT_NO_THROW(ubseComEngineInfo.SetQueryEidByNodeIdCb(cb1));
    std::string nodeId = "TestSetGetQueryEidByNodeIdCb";
    std::string eid = "4245:4944:0000:0000:0000:0000:0300:0000";
    QueryEidByNodeIdCb cb2 = ubseComEngineInfo.GetQueryEidByNodeIdCb();
    EXPECT_EQ(cb1(nodeId, eid), cb2(nodeId, eid));
}

/*
 * 用例描述：
 * 设置和获取CipherSuite
 * 测试步骤：
 * 1.设置CipherSuite为AES_CCM_128
 * 2.查询CipherSuite
 * 预期结果：
 * 1.设置CipherSuite成功
 * 2.查询CipherSuite为AES_CCM_128
 */
TEST_F(TestUbseComDef, TestSetGetCipherSuite)
{
    UbseComEngineInfo ubseComEngineInfo;
    EXPECT_NO_THROW(ubseComEngineInfo.SetCipherSuite(UBSHcomNetCipherSuite::AES_CCM_128));
    EXPECT_EQ(UBSHcomNetCipherSuite::AES_CCM_128, ubseComEngineInfo.GetCipherSuite());
}

/*
 * 用例描述：
 * 设置和获取ip
 * 测试步骤：
 * 1.设置ip为127.0.0.1
 * 2.查询ip
 * 预期结果：
 * 1.设置ip为127.0.0.1成功
 * 2.查询ip为127.0.0.1
 */
TEST_F(TestUbseComDef, TestSetGetIp)
{
    UbseComChannelConnectInfo connectInfo;
    std::string ip = "127.0.0.1";
    EXPECT_NO_THROW(connectInfo.SetIp(ip));
    EXPECT_EQ(ip, connectInfo.GetIp());
}

/*
 * 用例描述：
 * 设置和获取port
 * 测试步骤：
 * 1.设置port为5000
 * 2.查询port
 * 预期结果：
 * 1.设置port为5000成功
 * 2.查询port为5000
 */
TEST_F(TestUbseComDef, TestSetGetPort)
{
    UbseComChannelConnectInfo connectInfo;
    uint16_t port = 5000;
    EXPECT_NO_THROW(connectInfo.SetPort(port));
    EXPECT_EQ(port, connectInfo.GetPort());
}

/*
 * 用例描述：
 * 测试是否为uds协议
 * 测试步骤：
 * 1.设置uds协议
 * 2.查询是否为uds协议
 * 3.设置非uds协议
 * 4.查询是否为uds协议
 * 预期结果：
 * 1.设置uds协议成功
 * 2.查询到为uds协议
 * 3.设置非uds协议成功
 * 4.查询到不是uds协议
 */
TEST_F(TestUbseComDef, TestConnectInfoIsUds)
{
    UbseComChannelConnectInfo connectInfo;
    EXPECT_NO_THROW(connectInfo.SetIsUds(true));
    EXPECT_EQ(true, connectInfo.IsUds());
    EXPECT_NO_THROW(connectInfo.SetIsUds(false));
    EXPECT_EQ(false, connectInfo.IsUds());
}

/*
 * 用例描述：
 * 设置和获取CurNodeId
 * 测试步骤：
 * 1.设置CurNodeId为1
 * 2.查询CurNodeId
 * 预期结果：
 * 1.设置CurNodeId成功
 * 2.查询到CurNodeId为1
 */
TEST_F(TestUbseComDef, TestSetGetCurNodeId)
{
    UbseComChannelConnectInfo connectInfo;
    std::string curNodeId = "1";
    EXPECT_NO_THROW(connectInfo.SetCurNodeId(curNodeId));
    EXPECT_EQ(curNodeId, connectInfo.GetCurNodeId());
}

/*
 * 用例描述：
 * 设置和获取RemoteNodeId
 * 测试步骤：
 * 1.设置RemoteNodeId为1
 * 2.查询RemoteNodeId
 * 预期结果：
 * 1.设置RemoteNodeId成功
 * 2.查询到RemoteNodeId为1
 */
TEST_F(TestUbseComDef, TestSetGetRemoteNodeId)
{
    UbseComChannelConnectInfo connectInfo;
    std::string remoteNodeId = "1";
    EXPECT_NO_THROW(connectInfo.SetRemoteNodeId(remoteNodeId));
    EXPECT_EQ(remoteNodeId, connectInfo.GetRemoteNodeId());
}

/*
 * 用例描述：
 * 设置和获取ConnectInfo
 * 测试步骤：
 * 1.设置ConnectInfo
 * 2.查询ConnectInfo
 * 预期结果：
 * 1.设置ConnectInfo成功
 * 2.查询到ConnectInfo为预期值
 */
TEST_F(TestUbseComDef, TestSetGetConnectInfo)
{
    UbseComChannelInfo channelInfo;
    std::string ip = "127.0.0.1";
    uint16_t port = 5000;
    std::string remoteNodeId = "1";
    std::string curNodeId = "0";
    UbseComChannelConnectInfo connectInfo(false, ip, port, remoteNodeId, curNodeId);
    EXPECT_NO_THROW(channelInfo.SetConnectInfo(connectInfo));
    EXPECT_EQ(false, channelInfo.GetConnectInfo().IsUds());
    EXPECT_EQ(ip, channelInfo.GetConnectInfo().GetIp());
    EXPECT_EQ(remoteNodeId, channelInfo.GetConnectInfo().GetRemoteNodeId());
    EXPECT_EQ(curNodeId, channelInfo.GetConnectInfo().GetCurNodeId());
}

/*
 * 用例描述：
 * channel信息转字符串
 * 测试步骤：
 * 1.设置channel信息
 * 2.将channel信息转字符串
 * 预期结果：
 * 1.转换后的字符串符合预期
 */
TEST_F(TestUbseComDef, TestConvertUbseComChannelInfoToString)
{
    UbseComChannelInfo channelInfo;
    channelInfo.engineName_ = "RpcServer";
    channelInfo.channelType_ = UbseChannelType::NORMAL;
    UBSHcomChannelPtr chPtr = new TestChannel();
    channelInfo.channel_ = chPtr;
    UbseComChannelConnectInfo connectInfo;
    connectInfo.curNodeId_ = "0";
    connectInfo.remoteNodeId_ = "1";
    channelInfo.connectInfo_ = connectInfo;

    std::string channelInfoString = channelInfo.ConvertUbseComChannelInfoToString();
    EXPECT_EQ("engine Name: RpcServer; channel type: 0; channel id: 1; cur node id: 0; remote node id: 1; ",
        channelInfoString);
}

/*
 * 用例描述：
 * 字符串转为reply错误码
 * 测试步骤：
 * 1.设置字符串为ERR，转为错误码
 * 2.设置字符串为ERR_NO_HANDLER，转为错误码
 * 3.设置字符串为ERR_NO_REPLY，转为错误码
 * 4.设置字符串为OK，转为错误码
 * 预期结果：
 * 1.错误码为ERR
 * 2.错误码为ERR_NO_HANDLER
 * 3.错误码为ERR_NO_REPLY
 * 4.错误码为OK
 */
TEST_F(TestUbseComDef, TestStringToUbseReplyResult)
{
    std::string result = "ERR";
    EXPECT_EQ(UbseReplyResult::ERR, StringToUbseReplyResult(result));
    result = "ERR_NO_HANDLER";
    EXPECT_EQ(UbseReplyResult::ERR_NO_HANDLER, StringToUbseReplyResult(result));
    result = "ERR_NO_REPLY";
    EXPECT_EQ(UbseReplyResult::ERR_NO_REPLY, StringToUbseReplyResult(result));
    result = "OK";
    EXPECT_EQ(UbseReplyResult::OK, StringToUbseReplyResult(result));
}

/*
 * 用例描述：
 * reply错误码转为字符串
 * 测试步骤：
 * 1.设置错误码为ERR，转为字符串
 * 2.设置错误码为ERR_NO_HANDLER，转为字符串
 * 3.设置错误码为ERR_NO_REPLY，转为字符串
 * 4.设置错误码为OK，转为字符串
 * 预期结果：
 * 1.字符串为ERR
 * 2.字符串为ERR_NO_HANDLER
 * 3.字符串为ERR_NO_REPLY
 * 4.字符串为OK
 */
TEST_F(TestUbseComDef, TestUbseReplyResultToString)
{
    std::string result;
    EXPECT_EQ("ERR", UbseReplyResultToString(UbseReplyResult::ERR));
    EXPECT_EQ("ERR_NO_HANDLER", UbseReplyResultToString(UbseReplyResult::ERR_NO_HANDLER));
    EXPECT_EQ("ERR_NO_REPLY", UbseReplyResultToString(UbseReplyResult::ERR_NO_REPLY));
    EXPECT_EQ("OK", UbseReplyResultToString(UbseReplyResult::OK));
}

/*
 * 用例描述：
 * 设置和获取Crc
 * 测试步骤：
 * 1.设置Crc为1
 * 2.查询Crc
 * 预期结果：
 * 1.设置Crc成功
 * 2.查询到Crc为1
 */
TEST_F(TestUbseComDef, TestSetGetCrc)
{
    UbseComMessageHead head;
    uint32_t crc = 1;
    EXPECT_NO_THROW(head.SetCrc(crc));
    EXPECT_EQ(crc, head.GetCrc());
}

/*
 * 用例描述：
 * 设置和获取Message
 * 测试步骤：
 * 1.设置Message为"hello world!"并查询
 * 预期结果：
 * 1.查询到Message为"hello world!"
 */
TEST_F(TestUbseComDef, TestSetGetMessage)
{
    UbseComMessageCtx ctx;
    uint8_t msg[] = "hello world!";
    EXPECT_NO_THROW(ctx.SetMessage(msg));
    EXPECT_EQ(0, memcmp(msg, ctx.GetMessage(), strlen(reinterpret_cast<const char *>(msg))));
}

/*
 * 用例描述：
 * 设置和获取RspCtx
 * 测试步骤：
 * 1.设置RspCtx为1并查询
 * 预期结果：
 * 1.查询到RspCtx为1
 */
TEST_F(TestUbseComDef, TestSetGetRspCtx)
{
    UbseComMessageCtx ctx;
    uintptr_t transRspCtx = 1;
    EXPECT_NO_THROW(ctx.SetRspCtx(transRspCtx));
    EXPECT_EQ(transRspCtx, ctx.GetRspCtx());
}

/*
 * 用例描述：
 * 设置和获取ChannelId
 * 测试步骤：
 * 1.设置ChannelId为1并查询
 * 预期结果：
 * 1.查询到ChannelId为1
 */
TEST_F(TestUbseComDef, TestSetGetChannelId)
{
    UbseComMessageCtx ctx;
    uintptr_t transRspCtx = 1;
    EXPECT_NO_THROW(ctx.SetChannelId(transRspCtx));
    EXPECT_EQ(transRspCtx, ctx.GetChannelId());
}

/*
 * 用例描述：
 * 设置和获取SrcId
 * 测试步骤：
 * 1.设置SrcId为1并查询
 * 预期结果：
 * 1.查询到SrcId为1
 */
TEST_F(TestUbseComDef, TestSetGetSrcId)
{
    UbseComMessageCtx ctx;
    std::string srcId = "1";
    EXPECT_NO_THROW(ctx.SetSrcId(srcId));
    EXPECT_EQ(srcId, ctx.GetSrcId());
}

/*
 * 用例描述：
 * 设置和获取DstId
 * 测试步骤：
 * 1.设置DstId为1并查询
 * 预期结果：
 * 1.查询到DstId为1
 */
TEST_F(TestUbseComDef, TestSetGetDstId)
{
    UbseComMessageCtx ctx;
    std::string dstId = "1";
    EXPECT_NO_THROW(ctx.SetDstId(dstId));
    EXPECT_EQ(dstId, ctx.GetDstId());
}

/*
 * 用例描述：
 * 设置和获取EngineName
 * 测试步骤：
 * 1.设置EngineName为RpcServer并查询
 * 预期结果：
 * 1.查询到EngineName为RpcServer
 */
TEST_F(TestUbseComDef, TestSetGetEngineName)
{
    UbseComMessageCtx ctx;
    std::string name = "RpcServer";
    EXPECT_NO_THROW(ctx.SetEngineName(name));
    EXPECT_EQ(name, ctx.GetEngineName());
}

/*
 * 用例描述：
 * 设置和获取UdsInfo
 * 测试步骤：
 * 1.设置UdsInfo为{1,1,1}并查询
 * 预期结果：
 * 1.查询到UdsInfo为{1,1,1}
 */
TEST_F(TestUbseComDef, TestSetGetUdsInfo_2)
{
    UbseComMessageCtx ctx;
    uint32_t pid = 1;
    uint32_t uid = 1;
    uint32_t gid = 1;
    UbseUdsIdInfo info{pid, uid, gid};
    EXPECT_NO_THROW(ctx.SetUdsInfo(info));
    EXPECT_EQ(info.pid, ctx.GetUdsInfo().pid);
    EXPECT_EQ(info.uid, ctx.GetUdsInfo().uid);
    EXPECT_EQ(info.gid, ctx.GetUdsInfo().gid);
}

/*
 * 用例描述：
 * 测试从context中获取UdsInfo
 * 测试步骤：
 * 1.context中设置pid uid gid均为1
 * 2.获取UdsInfo
 * 预期结果：
 * 1.获取到的pid uid gid均为1
 */
TEST_F(TestUbseComDef, TestGetUdsInfoFromNetServiceContext)
{
    UBSHcomServiceContext context;
    UBSHcomChannelPtr chPtr = new TestChannel();
    context.mCh = chPtr;
    UbseUdsIdInfo udsIdInfo;
    GetUdsInfoFromNetServiceContext(context, udsIdInfo);
    EXPECT_EQ(1, udsIdInfo.pid);
    EXPECT_EQ(1, udsIdInfo.uid);
    EXPECT_EQ(1, udsIdInfo.gid);
}

/*
 * 用例描述：
 * request转uint* 8 msg
 * 测试步骤：
 * 1.msg为空
 * 2.msg序列化失败
 * 3.转换成功
 * 预期结果：
 * 1.返回nullptr
 * 2.返回nullptr
 * 3.成功
 */
TEST_F(TestUbseComDef, TransRequestMsg)
{
    EXPECT_EQ(nullptr, TransRequestMsg(nullptr, 0, 0));
    TestRpcMessage *msg = new TestRpcMessage(UBSE_ERROR_SERIALIZE_FAILED, UBSE_OK);
    EXPECT_EQ(nullptr, TransRequestMsg(msg, 0, 0));
    TestRpcMessage *successMsg = new TestRpcMessage(UBSE_OK, UBSE_OK);
    UbseComMessagePtr result = TransRequestMsg(successMsg, 0, 0);
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(result));
    std::string str;
    str.append(reinterpret_cast<VOS_CHAR *>(ucMsg->GetMessageBody()), 4); // 4是data空间大小
    EXPECT_EQ("test", str);
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤：
 * 1.msg为空
 * 2.响应内容为空
 * 3.反序列化失败
 * 4.成功
 * 预期结果：
 * 1.返回nullptr
 * 2.UBSE_ERROR_DESERIALIZE_FAILED
 * 3.成功
 */
TEST_F(TestUbseComDef, TransResponseWithCopy)
{
    UbseComDataDesc data{};
    data = {nullptr, 0};
    TestRpcMessage *msg = new TestRpcMessage(UBSE_OK, UBSE_OK);
    EXPECT_EQ(UBSE_ERROR, TransResponse(msg, data, true));
    std::string str = "data";
    data.data = reinterpret_cast<uint8_t *>(str.data());
    data.len = str.length();
    msg = new TestRpcMessage(UBSE_OK, UBSE_ERROR_DESERIALIZE_FAILED);
    EXPECT_EQ(UBSE_ERROR_DESERIALIZE_FAILED, TransResponse(msg, data, true));
    msg = new TestRpcMessage(UBSE_OK, UBSE_OK);
    Ref<TestRpcMessage> msgPtr = new TestRpcMessage(UBSE_OK, UBSE_OK);
    EXPECT_EQ(UBSE_OK, TransResponse(UbseBaseMessage::Convert<TestRpcMessage>(msgPtr), data, true));
    std::string respData;
    respData.append(reinterpret_cast<VOS_CHAR *>(msgPtr->InputRawData()), 4); // 4是data空间大小
    EXPECT_EQ("data", respData);
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤：
 * 1.定义对象UbseComEngineInfo egInfo
 * 预期结果：
 * 1.egInfo.GetWorkGroup()返回空字符串
 * 2.egInfo.GetUdsInfo()方法无异常
 */
TEST_F(TestUbseComDef, UbseComEngineInfo)
{
    UbseComEngineInfo egInfo;
    EXPECT_EQ("", egInfo.GetWorkGroup());
    EXPECT_NO_THROW(egInfo.GetUdsInfo());
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤：
 * 1.定义对象UbseComChannelConnectInfo connectInfo;
 * 2.调用connectInfo.SetLinkNum(2)方法
 * 预期结果：
 * 1.connectInfo.GetLinkNum()等于2
 */
TEST_F(TestUbseComDef, UbseComChannelConnectInfo)
{
    UbseComChannelConnectInfo connectInfo;
    connectInfo.SetLinkNum(2); // 2:2个链接
    EXPECT_EQ(2, connectInfo.GetLinkNum());
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤：
 * 1.定义对象UbseComChannelInfo channelInfo;
 * 2.调用channelInfo.SetIsServer(true)方法
 * 3.调用channelInfo.SetChannel(nullptr)
 * 4.调用channelInfo.SetConnectInfo(connectInfo)
 * 5.调用channelInfo.SetChannelType(UbseChannelType::NORMAL)
 * 6.调用channelInfo.SetEngineName("Node0")
 * 预期结果：
 * 1.channelInfo.IsServerSide()等于true
 * 2.channelInfo.GetChannel()等于nullptr
 * 3.调用channelInfo.SetConnectInfo(connectInfo)不抛出异常
 * 4.channelInfo.GetChannelType()等于UbseChannelType::NORMAL
 * 5.channelInfo.GetEngineName()等于Node0
 */
TEST_F(TestUbseComDef, UbseComChannelInfo)
{
    UbseComChannelInfo channelInfo;
    channelInfo.SetIsServer(true);
    EXPECT_TRUE(channelInfo.IsServerSide());
    channelInfo.SetChannel(nullptr);
    EXPECT_TRUE(channelInfo.GetChannel() == nullptr);
    UbseComChannelConnectInfo connectInfo;
    EXPECT_NO_THROW(channelInfo.SetConnectInfo(connectInfo));
    channelInfo.SetChannelType(UbseChannelType::NORMAL);
    EXPECT_TRUE(channelInfo.GetChannelType() == UbseChannelType::NORMAL);
    channelInfo.SetEngineName("Node0");
    EXPECT_EQ("Node0", channelInfo.GetEngineName());
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤： 1.调用head.SetBodyLen(10)
 * 预期结果： 1.head.GetBodyLen()等于10
 */
TEST_F(TestUbseComDef, UbseComMessageHead)
{
    UbseComMessageHead head;
    head.SetBodyLen(10); // 10:body体长度
    EXPECT_EQ(10, head.GetBodyLen());
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤： 定义UbseComMessageCtx ctx
 * 预期结果： 1.ctx.GetMessage()不抛出异常
 * 2.ctx.GetRspCtx()不抛出异常
 * 3.ctx.GetChannelId()不抛出异常
 * 4.ctx.GetSrcId()不抛出异常
 * 5.ctx.GetDstId()不抛出异常
 * 6.ctx.GetChannelType()不抛出异常
 * 7.ctx.GetEngineName()不抛出异常
 * 8.ctx.GetUdsInfo()不抛出异常
 */
TEST_F(TestUbseComDef, UbseComMessageCtx)
{
    UbseComMessageCtx ctx;
    EXPECT_NO_THROW(ctx.GetMessage());
    EXPECT_NO_THROW(ctx.GetRspCtx());
    EXPECT_NO_THROW(ctx.GetChannelId());
    EXPECT_NO_THROW(ctx.GetSrcId());
    EXPECT_NO_THROW(ctx.GetDstId());
    EXPECT_NO_THROW(ctx.GetChannelType());
    EXPECT_NO_THROW(ctx.GetEngineName());
    EXPECT_NO_THROW(ctx.GetUdsInfo());
}

/*
 * 用例描述：
 * 响应消息转换
 * 测试步骤： 1.定义UbseComMessage message
 * 预期结果： 1.message.GetMessageBodyLen()不抛出异常
 */
TEST_F(TestUbseComDef, UbseComMessage)
{
    UbseComMessage message;
    EXPECT_NO_THROW(message.GetMessageBodyLen());
}

TEST_F(TestUbseComDef, GetSendReceiveSegCount)
{
    UbseComEngineInfo engineInfo{};
    engineInfo.SetSendReceiveSegCount(1);
    EXPECT_EQ(1, engineInfo.GetSendReceiveSegCount());
}
} // namespace ubse::ut::com