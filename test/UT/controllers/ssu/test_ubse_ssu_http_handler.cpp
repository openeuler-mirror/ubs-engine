/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <memory>
#include <string>

#include "ubse_error.h"
#include "ubse_http_common.h"
#include "ubse_service_registry.h"
#include "ubse_ssu_service.h"
#include "ubse_ssu_service_imp.h"
// 包含实现文件以访问匿名命名空间内的handler，并使被测代码纳入UT覆盖率统计
#include "ubse_ssu_http_handler.cpp"

namespace ubse::ut::ssu {
using namespace ubse::http;
using namespace ubse::common::def;
using namespace ubse::ssu::http_handler;
using ubse::ssu::service::UbseSsuServiceImp;
using ubse::plugin::service::ssu::UbseSsuService;
using ubse::plugin::service::ssu::UbseSsuAllocSpaceReq;
using ubse::plugin::service::ssu::UbseSsuAllocIdentityInfo;
using ubse::plugin::service::ssu::UbseSsuAllocResult;
using ubse::plugin::service::ssu::UbseSsuNameSpaceInfo;
using ubse::plugin::service::ssu::UbseSsuLBAFormat;
using ubse::plugin::service::ssu::UbseSsuAllocStrategy;
using ubse::service::UbseServiceRegistry;

namespace {
constexpr const char *SSU_SPACES_URL = "/ubse/v1/ssu/spaces";
constexpr const char *SSU_SPACES_PREFIX_URL = "/ubse/v1/ssu/spaces/";

// 构造带有效mTLS身份的请求
UbseHttpRequest MakeAuthedRequest(const std::string &path, const std::string &body, const std::string &cn = "user-01",
                                  const std::string &ou = "1000")
{
    UbseHttpRequest req;
    req.method = "POST";
    req.path = path;
    req.body = body;
    req.peerCert.present = true;
    req.peerCert.cn = cn;
    req.peerCert.ou = ou;
    return req;
}

// 有效的AllocSpace请求体
const char *VALID_ALLOC_BODY =
    R"({"name":"space1","nsSize":1073741824,"nsNum":2,"lbaFormat":4096,"strategy":0})";

// 向服务注册表注册SSU服务单例（用空deleter避免释放单例），返回shared_ptr用于注册
std::shared_ptr<UbseSsuService> InstallSsuService()
{
    auto svc = std::shared_ptr<UbseSsuService>(&UbseSsuServiceImp::GetInstance(), [](UbseSsuService *) {});
    UbseServiceRegistry::GetInstance().RegisterService<UbseSsuService>(svc);
    return svc;
}

// 清除注册表中的SSU服务条目（fno-access-control直接操作内部map，保证用例间隔离）
void ClearSsuServiceInRegistry()
{
    UbseServiceRegistry::GetInstance().services_.erase(UbseSsuService::kServiceName);
}
} // namespace

class TestUbseSsuHttpHandler : public testing::Test {
public:
    TestUbseSsuHttpHandler() = default;
    void SetUp() override
    {
        Test::SetUp();
        ClearSsuServiceInRegistry();
        InstallSsuService();
    }
    void TearDown() override
    {
        ClearSsuServiceInRegistry();
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

// ===================== AllocSpaceHandler 成功路径 =====================

/*
 * 用例描述：AllocSpaceHandler 成功路径
 * 测试步骤：构造有效mTLS身份与请求体，mock AllocSpace返回OK并填充result
 * 预期结果：返回UBSE_OK，响应状态201，响应体包含success与name
 */
TEST_F(TestUbseSsuHttpHandler, AllocSpace_Success)
{
    UbseSsuNameSpaceInfo ns;
    ns.tgtEid = "eid-1";
    ns.tgtNqn = "nqn-1";
    ns.nsUuid = "uuid-1";
    ns.namespaceId = 1;
    ns.nsDevPath = "/dev/nvme1n1";
    ns.nsSize = 536870912;
    ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    ns.allowHostNqnList = {"host-nqn-1"};
    UbseSsuAllocResult result{};
    result.name = "space1";
    result.strategy = UbseSsuAllocStrategy::STRIPED;
    result.nameSpaceList.push_back(ns);

    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::AllocSpace)
        .expects(once())
        .with(any(), any(), outBound(result))
        .will(returnValue(UBSE_OK));

    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_OK);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED));
    EXPECT_NE(resp.body.find("success"), std::string::npos);
    EXPECT_NE(resp.body.find("space1"), std::string::npos);
}

/*
 * 用例描述：带可选tenant字段的请求体成功
 * 预期结果：返回UBSE_OK
 */
TEST_F(TestUbseSsuHttpHandler, AllocSpace_WithTenant_Success)
{
    UbseSsuAllocResult result{};
    result.name = "space2";
    result.strategy = UbseSsuAllocStrategy::LINEAR;

    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::AllocSpace)
        .expects(once())
        .with(any(), any(), outBound(result))
        .will(returnValue(UBSE_OK));

    std::string body = R"({"name":"space2","nsSize":1048576,"nsNum":1,"lbaFormat":512,"strategy":1,"tenant":"t1"})";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_OK);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED));
}

// ===================== AllocSpaceHandler 身份认证失败 =====================

TEST_F(TestUbseSsuHttpHandler, AllocSpace_NoCert_Unauth)
{
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY);
    req.peerCert.present = false;
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_AUTHENTICATION_FAILED);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_UNAUTH));
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_EmptyCnAndOu_Unauth)
{
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY, "", "");
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_AUTHENTICATION_FAILED);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_InvalidCn_Unauth)
{
    // CN含大写字母，不满足字符集校验
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY, "User-01");
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_AUTHENTICATION_FAILED);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_InvalidOu_Unauth)
{
    // OU非数字
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY, "user01", "not-a-number");
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_AUTHENTICATION_FAILED);
}

// ===================== AllocSpaceHandler 请求体校验失败 =====================

TEST_F(TestUbseSsuHttpHandler, AllocSpace_InvalidBody_BadRequest)
{
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, "{not json");
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ));
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_MissingName_BadRequest)
{
    std::string body = R"({"nsSize":1048576,"nsNum":1,"lbaFormat":512,"strategy":0})";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_NameTooLong_BadRequest)
{
    std::string longName(48, 'a'); // 超过47字符上限
    std::string body = R"({"name":")" + longName + R"(","nsSize":1048576,"nsNum":1,"lbaFormat":512,"strategy":0})";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_NsNumZero_BadRequest)
{
    std::string body = R"({"name":"space1","nsSize":1048576,"nsNum":0,"lbaFormat":512,"strategy":0})";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_InvalidLbaFormat_BadRequest)
{
    std::string body = R"({"name":"space1","nsSize":1048576,"nsNum":1,"lbaFormat":2048,"strategy":0})";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_InvalidStrategy_BadRequest)
{
    std::string body = R"({"name":"space1","nsSize":1048576,"nsNum":1,"lbaFormat":512,"strategy":2})";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_TenantTooLong_BadRequest)
{
    std::string longTenant(17, 't'); // 超过16字符上限
    std::string body =
        R"({"name":"space1","nsSize":1048576,"nsNum":1,"lbaFormat":512,"strategy":0,"tenant":")" + longTenant + "\"}";
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, body);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

// ===================== AllocSpaceHandler 服务不可用 =====================

TEST_F(TestUbseSsuHttpHandler, AllocSpace_ServiceNull_Unavailable)
{
    ClearSsuServiceInRegistry(); // 模拟服务未注册
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_IPC_SERVICE_UNAVAILABLE);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_SERVICE_UNVAILABLE));
}

// ===================== AllocSpaceHandler 服务返回错误 =====================

TEST_F(TestUbseSsuHttpHandler, AllocSpace_ServiceExisted_Conflict)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::AllocSpace)
        .stubs()
        .will(returnValue(UBSE_ERR_EXISTED));
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERR_EXISTED);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CONFICT));
}

TEST_F(TestUbseSsuHttpHandler, AllocSpace_ServiceOtherError_InternalError)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::AllocSpace)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_URL, VALID_ALLOC_BODY);
    UbseHttpResponse resp;
    EXPECT_EQ(AllocSpaceHandler(req, resp), UBSE_ERROR);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR));
}

// ===================== FreeSpaceHandler =====================

TEST_F(TestUbseSsuHttpHandler, FreeSpace_Success)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::FreeSpace)
        .expects(once())
        .with(eq(std::string("myname")), any())
        .will(returnValue(UBSE_OK));
    UbseHttpRequest req = MakeAuthedRequest(std::string(SSU_SPACES_PREFIX_URL) + "myname", "");
    req.method = "DELETE";
    UbseHttpResponse resp;
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_OK);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK));
    EXPECT_NE(resp.body.find("success"), std::string::npos);
}

TEST_F(TestUbseSsuHttpHandler, FreeSpace_Idempotent_NotExist)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::FreeSpace)
        .stubs()
        .will(returnValue(UBSE_ERR_NOT_EXIST));
    UbseHttpRequest req = MakeAuthedRequest(std::string(SSU_SPACES_PREFIX_URL) + "myname", "");
    req.method = "DELETE";
    UbseHttpResponse resp;
    // 释放不存在的空间应返回成功（幂等性）
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_OK);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK));
}

TEST_F(TestUbseSsuHttpHandler, FreeSpace_EmptyName_BadRequest)
{
    // 路径恰好等于前缀，name为空
    UbseHttpRequest req = MakeAuthedRequest(SSU_SPACES_PREFIX_URL, "");
    req.method = "DELETE";
    UbseHttpResponse resp;
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ));
}

TEST_F(TestUbseSsuHttpHandler, FreeSpace_NameTooLong_BadRequest)
{
    std::string longName(48, 'a');
    UbseHttpRequest req = MakeAuthedRequest(std::string(SSU_SPACES_PREFIX_URL) + longName, "");
    req.method = "DELETE";
    UbseHttpResponse resp;
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_ERR_INVALID_ARG);
}

TEST_F(TestUbseSsuHttpHandler, FreeSpace_NoCert_Unauth)
{
    UbseHttpRequest req = MakeAuthedRequest(std::string(SSU_SPACES_PREFIX_URL) + "myname", "");
    req.method = "DELETE";
    req.peerCert.present = false;
    UbseHttpResponse resp;
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_ERR_AUTHENTICATION_FAILED);
}

TEST_F(TestUbseSsuHttpHandler, FreeSpace_ServiceNull_Unavailable)
{
    ClearSsuServiceInRegistry();
    UbseHttpRequest req = MakeAuthedRequest(std::string(SSU_SPACES_PREFIX_URL) + "myname", "");
    req.method = "DELETE";
    UbseHttpResponse resp;
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_ERR_IPC_SERVICE_UNAVAILABLE);
}

TEST_F(TestUbseSsuHttpHandler, FreeSpace_ServiceError_InternalError)
{
    MOCKER_CPP_VIRTUAL(UbseSsuServiceImp::GetInstance(), &UbseSsuServiceImp::FreeSpace)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    UbseHttpRequest req = MakeAuthedRequest(std::string(SSU_SPACES_PREFIX_URL) + "myname", "");
    req.method = "DELETE";
    UbseHttpResponse resp;
    EXPECT_EQ(FreeSpaceHandler(req, resp), UBSE_ERROR);
    EXPECT_EQ(resp.status, static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_INTERNAL_SVR_ERR));
}

// ===================== RegisterSsuHttpHandlers =====================

/*
 * 用例描述：注册SSU HTTP路由，外部依赖RegVipHttpService返回成功
 * 预期结果：返回UBSE_OK
 */
TEST_F(TestUbseSsuHttpHandler, RegisterSsuHttpHandlers_Success)
{
    MOCKER(ubse::vip::RegVipHttpService).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(RegisterSsuHttpHandlers(), UBSE_OK);
}

/*
 * 用例描述：第一次注册(POST)即失败
 * 预期结果：返回非UBSE_OK，不再进行第二次注册
 */
TEST_F(TestUbseSsuHttpHandler, RegisterSsuHttpHandlers_FirstRegFail_ReturnError)
{
    MOCKER(ubse::vip::RegVipHttpService).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(RegisterSsuHttpHandlers(), UBSE_OK);
}

/*
 * 用例描述：第一次注册(POST)成功，第二次注册(DELETE)失败
 * 预期结果：返回非UBSE_OK
 */
TEST_F(TestUbseSsuHttpHandler, RegisterSsuHttpHandlers_SecondRegFail_ReturnError)
{
    MOCKER(ubse::vip::RegVipHttpService).expects(exactly(2)).will(returnObjectList(UBSE_OK, UBSE_ERROR));
    EXPECT_NE(RegisterSsuHttpHandlers(), UBSE_OK);
}
} // namespace ubse::ut::ssu
