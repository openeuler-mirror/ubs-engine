/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_cli_ssu_cmd_reg.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <mockcpp/mockcpp.hpp>

#include "test_ubse_cli_ssu_mock_invoke.h"
#include "ubse_cli_ssu_cmd_reg.h"
#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"

namespace ubse::ut::cli {
using namespace ubse::cli::framework;
using namespace ubse::cli::reg;
using namespace ubse::plugin::service::ssu;
using ubse::com::UbseModuleCode;
using ubse::com::UbseSsuOpCode;

namespace {
constexpr uint64_t GIB = 1024ULL * 1024ULL * 1024ULL;
constexpr uint16_t SSU_MODULE_CODE = static_cast<uint16_t>(UbseModuleCode::UBSE_SSU);

constexpr uint16_t SsuOpCode(UbseSsuOpCode opCode)
{
    return static_cast<uint16_t>(opCode);
}

// 期望文本与 ubse_cli_ssu_cmd_reg.cpp 中固定错误/提示信息逐字一致，任何文案改动都会被用例捕获。
const std::string ERR_NAME_REQUIRED = "ERROR: The option -n or --name is required.";
const std::string ERR_SIZE_REQUIRED = "ERROR: The option -s or --size is required.";
const std::string ERR_INVALID_NAME =
    "ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'.";
const std::string ERR_INVALID_NAME_PREFIX = "ERROR: Invalid name.";
const std::string ERR_INVALID_SIZE =
    "ERROR: Invalid size. The value must be an integer number of GiB with uppercase G suffix, minimum 1G.";
const std::string ERR_INVALID_LBA = "ERROR: Invalid lba. The value must be 512B or 4K.";
const std::string ERR_INVALID_STRATEGY = "ERROR: Invalid strategy. The value must be Linear or Striped.";
const std::string ERR_DESERIALIZATION = "ERROR: Deserialization failed in client.";
const std::string INFO_EMPTY = "INFO: No SSU allocation information found.";

// 捕获 UbseCliDisplayResult 写入 stdout 的文本，便于对回显内容做子串断言。
std::string Render(const std::shared_ptr<UbseCliResultEcho> &result)
{
    testing::internal::CaptureStdout();
    result->UbseCliDisplayResult();
    return testing::internal::GetCapturedStdout();
}

// 渲染结果并断言包含指定子串，用于错误信息/表格内容的正向校验。
void ExpectRenderedContains(const std::shared_ptr<UbseCliResultEcho> &result, const std::string &needle)
{
    const std::string output = Render(result);
    EXPECT_NE(output.find(needle), std::string::npos) << output;
}

// 与生产代码同构：ns_num 区间随契约常量动态拼接，保证用例期望与实际错误文案一致。
std::string InvalidNsNumError()
{
    return "ERROR: Invalid ns_num. The value must be an integer in range " + std::to_string(SSU_CLI_DEFAULT_NS_NUM) +
           "-" + std::to_string(SSU_CLI_MAX_NS_NUM) + ".";
}

// 构造 alloc_detail 子命令的入参，name 可覆盖以测试不同 name 校验场景。
std::map<std::string, std::string> DetailParams(const std::string &name = "alloc-space-1")
{
    return {{"type", "alloc_detail"}, {"name", name}};
}

// 构造 create ssu 的最小合法入参（仅必填项），可选参数由各用例按需追加。
std::map<std::string, std::string> CreateParams()
{
    return {{"name", "alloc-space-1"}, {"size", "10G"}};
}

// 详情表正向断言：覆盖表头合并行、列标题与典型数据行，确保 create/detail 回显布局完整。
// 字段对齐服务层 UbseSsuNameSpaceInfo：不含 using_type，含 ns_dev_path/namespace_id，无 SrcNqnList 表尾。
void ExpectDetailOutput(const std::shared_ptr<UbseCliResultEcho> &result)
{
    const std::string output = Render(result);
    EXPECT_NE(output.find("Name: alloc-space-1"), std::string::npos);
    EXPECT_NE(output.find("Strategy:Linear"), std::string::npos);
    EXPECT_NE(output.find("ns_uuid"), std::string::npos);
    EXPECT_NE(output.find("tgt_eid"), std::string::npos);
    EXPECT_NE(output.find("tgt_nqn"), std::string::npos);
    EXPECT_NE(output.find("namespace_id"), std::string::npos);
    EXPECT_NE(output.find("ns_dev_path"), std::string::npos);
    EXPECT_NE(output.find("ns_size"), std::string::npos);
    EXPECT_NE(output.find("lba_format"), std::string::npos);
    EXPECT_NE(output.find("uuid-aa"), std::string::npos);
    EXPECT_NE(output.find("/dev/nvme0n1"), std::string::npos);
    EXPECT_NE(output.find("4K"), std::string::npos);
    // 旧字段不再属于当前契约：详情表不应出现 using_type 列与 SrcNqnList 表尾。
    EXPECT_EQ(output.find("using_type"), std::string::npos);
    EXPECT_EQ(output.find("SrcNqnList"), std::string::npos);
}
} // namespace

void TestUbseCliSsuCmdReg::SetUp()
{
    ResetSsuMockCapture(); // 每个用例起始清空上次捕获，避免用例间状态串扰
}

void TestUbseCliSsuCmdReg::TearDown()
{
    MOCKER(&ubse_invoke_call).reset(); // 复位 ubse_invoke_call 桩，防止影响后续用例
    GlobalMockObject::verify();        // 校验本用例 mock 期望并释放桩资源
}

// 注册校验：SignUp 后框架应登记 display/create ssu 两条命令。
TEST_F(TestUbseCliSsuCmdReg, SignUpRegistersDisplayAndCreateSsuCommands)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    module.UbseCliRegisterCmd();

    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliCommandExist("display_ssu"));
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliCommandExist("create_ssu"));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

// display ssu 缺少 -t 应返回必选参数错误，不进入 IPC。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryRejectsMissingType)
{
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({}),
                           "ERROR: The option -t or --type is required.");
}

// display ssu 的 -t 取值非 alloc_summary/alloc_detail 应返回非法类型错误。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryRejectsUnsupportedType)
{
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "unknown"}}),
                           "ERROR: Invalid type. The value must be alloc_summary or alloc_detail.");
}

// 摘要查询返回空列表时应输出 INFO 提示，并验证实际发出的 module/op code 正确。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryReturnsInfoWhenEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_summary_invoke_call_empty));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}}), INFO_EMPTY);
    EXPECT_EQ(g_ssuMockLastModuleCode, SSU_MODULE_CODE);
    EXPECT_EQ(g_ssuMockLastOpCode, SsuOpCode(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ));
}

// 摘要表正向内容校验：表头列名与各分配的 name/size/strategy 均应出现在输出中。
// size 由 nameSpaceList[*].nsSize 求和得出，mock 提供 10G/20G 两条分配。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryPrintsNameSizeStrategy)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_summary_invoke_call_normal));
    const std::string output = Render(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}}));
    EXPECT_NE(output.find("name"), std::string::npos);
    EXPECT_NE(output.find("size"), std::string::npos);
    EXPECT_NE(output.find("strategy"), std::string::npos);
    EXPECT_NE(output.find("alloc-space-1"), std::string::npos);
    EXPECT_NE(output.find("10G"), std::string::npos);
    EXPECT_NE(output.find("Linear"), std::string::npos);
    EXPECT_NE(output.find("alloc-space-2"), std::string::npos);
    EXPECT_NE(output.find("20G"), std::string::npos);
    EXPECT_NE(output.find("Striped"), std::string::npos);
}

// 查询输出统一以 G 为单位，不再输出 M / K / 纯字节数
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryNeverEmitsMOrKOrBareBytes)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_summary_invoke_call_subgib));
    const std::string output = Render(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}}));
    // 512MiB 在统一为 G 后应显示为 0G，而非 512M
    EXPECT_NE(output.find("0G"), std::string::npos);
    EXPECT_EQ(output.find("512M"), std::string::npos);
    EXPECT_EQ(output.find("512K"), std::string::npos);
    EXPECT_EQ(output.find("524288"), std::string::npos);
}

// 详情表格 size 列同样只以 G 展示
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailNeverEmitsMOrKOrBareBytes)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_subgib));
    const std::string output = Render(UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams()));
    EXPECT_EQ(output.find("512M"), std::string::npos);
    EXPECT_EQ(output.find("524288"), std::string::npos);
}

// 摘要查询 IPC 返回非零错误码时，应回显含错误码的 Internal error 文案。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryReturnsInternalErrorWithCode)
{
    MOCKER(&ubse_invoke_call).stubs().will(returnValue(1234));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}}),
                           "ERROR: Internal error with error code 1234.");
}

// 摘要查询返回损坏报文时，应返回客户端反序列化失败错误。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryReturnsDeserializationErrorForBadResponse)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_summary_invoke_call_bad_response));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}}),
                           ERR_DESERIALIZATION);
}

// IPC 返回成功但 summary 响应体为空时，不应当作空列表，而应报客户端反序列化失败。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryReturnsDeserializationErrorForEmptySuccessResponse)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_summary_invoke_call_empty_body));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}}),
                           ERR_DESERIALIZATION);
}

// alloc_detail 缺少 -n 应返回 name 必选参数错误。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailRejectsMissingName)
{
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_detail"}}), ERR_NAME_REQUIRED);
}

// alloc_detail 的 name 含非法字符应返回 name 格式错误。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailRejectsInvalidName)
{
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams("bad/name")),
                           ERR_INVALID_NAME_PREFIX);
}

// 详情查询应使用 ALLOC_DETAIL 操作码发起 IPC，且 name 正确序列化到请求体。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailSerializesNameAndUsesDetailOpcode)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_normal));
    auto result = UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams());
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_ssuMockLastModuleCode, SSU_MODULE_CODE);
    EXPECT_EQ(g_ssuMockLastOpCode, SsuOpCode(UbseSsuOpCode::UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ));
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastDetailReq.name, "alloc-space-1");
}

// 详情表正向内容校验：表头合并行、列标题与数据行均应完整呈现，且不含旧字段。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailPrintsDetailTable)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_normal));
    ExpectDetailOutput(UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams()));
}

// 详情查询返回空命名空间列表时，应输出 INFO 提示而非空表。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailReturnsInfoWhenNoNamespaces)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_empty));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams()), INFO_EMPTY);
}

// 详情查询返回损坏报文时，应返回反序列化失败错误。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailReturnsDeserializationError)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_bad_response));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams()), ERR_DESERIALIZATION);
}

// create ssu 缺少 -n 应返回 name 必选参数错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsMissingName)
{
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc({{"size", "10G"}}), ERR_NAME_REQUIRED);
}

// create ssu 缺少 -s 应返回 size 必选参数错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsMissingSize)
{
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc({{"name", "alloc-space-1"}}), ERR_SIZE_REQUIRED);
}

// name 含空格等非法字符应返回 name 格式错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsInvalidNameCharacters)
{
    auto params = CreateParams();
    params["name"] = "alloc space";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_NAME_PREFIX);
}

// name 恰好 48 字符上限应被接受（与服务层 48 字符契约一致）。
TEST_F(TestUbseCliSsuCmdReg, CreateAcceptsNameAtFortyEightCharLimit)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto params = CreateParams();
    params["name"] = std::string(SSU_CLI_MAX_NAME_LENGTH, 'a');
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.name, std::string(SSU_CLI_MAX_NAME_LENGTH, 'a'));
}

// name 超过 48 字符上限应返回 name 格式错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsNameLongerThanFortyEight)
{
    auto params = CreateParams();
    params["name"] = std::string(SSU_CLI_MAX_NAME_LENGTH + 1, 'a');
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_NAME_PREFIX);
}

// size 后缀必须为大写 G：小写 g 或缺后缀均应返回 size 格式错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsLowercaseOrMissingSizeSuffix)
{
    auto params = CreateParams();
    params["size"] = "10g";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_SIZE);
    params["size"] = "10";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_SIZE);
}

// size 为 0G 应返回 size 格式错误（低于 1G 下限）。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsZeroSize)
{
    auto params = CreateParams();
    params["size"] = "0G";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_SIZE);
}

// size 数值超过 uint64_t 可承载的 GiB 上限（UINT64_MAX / 1GiB + 1）应返回 size 格式错误，
// 不进入 IPC：固化"先判溢出上界再乘"的写法，防止无符号回绕被误放行。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsSizeOverflowingUint64Bytes)
{
    constexpr uint64_t overflowGib = std::numeric_limits<uint64_t>::max() / GIB + 1;
    auto params = CreateParams();
    params["size"] = std::to_string(overflowGib) + "G";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_SIZE);
    EXPECT_FALSE(g_ssuMockLastRequestDeserialized);
}

// size 数值超出 std::stoull 可表示范围（位数多于 uint64_t）应返回 size 格式错误，
// 由逐字符数字校验 + stoull 异常捕获共同拒绝，不进入 IPC。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsSizeBeyondStoullRange)
{
    auto params = CreateParams();
    params["size"] = "18446744073709551616G"; // UINT64_MAX + 1，超出 stoull 范围
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_SIZE);
    EXPECT_FALSE(g_ssuMockLastRequestDeserialized);
}

// 1G 为最小下限，恰好 1G 应被接受；下限绑定 SSU_CLI_MIN_SIZE_BYTES 契约常量
TEST_F(TestUbseCliSsuCmdReg, CreateAcceptsMinimumOneGibSize)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto params = CreateParams();
    params["size"] = "1G";
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.nsSize, SSU_CLI_MIN_SIZE_BYTES);
}

// ns_num 超出 [1, MAX_NS_NUM] 区间（含 0 与越界值）应返回区间错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsInvalidNsNum)
{
    auto params = CreateParams();
    params["ns_num"] = "129";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), InvalidNsNumError());
    params["ns_num"] = "0";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), InvalidNsNumError());
}

// ns_num 数值超出 std::stoull 可表示范围（位数多于 uint64_t）应返回区间错误：
// 逐字符数字校验通过后由 stoull 异常捕获拒绝，不进入 IPC。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsNsNumBeyondStoullRange)
{
    auto params = CreateParams();
    params["ns_num"] = "18446744073709551616"; // UINT64_MAX + 1，超出 stoull 范围
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), InvalidNsNumError());
    EXPECT_FALSE(g_ssuMockLastRequestDeserialized);
}

// lba 取非规范值（如纯数字 4096）应返回 lba 格式错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsInvalidLba)
{
    auto params = CreateParams();
    params["lba"] = "4096";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_LBA);
}

// lba 输入与输出统一为 512B / 4K
TEST_F(TestUbseCliSsuCmdReg, CreateAcceptsCanonicalLbaInput)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto params = CreateParams();
    params["lba"] = "512B";
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_512);

    params["lba"] = "4K";
    result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_4K);
}

// 旧的纯数字 "512" 输入不再被接受（统一为 512B）
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsBareLba512Input)
{
    auto params = CreateParams();
    params["lba"] = "512";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_LBA);
}

// strategy 取非规范值（如 roundrobin）应返回 strategy 格式错误。
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsInvalidStrategy)
{
    auto params = CreateParams();
    params["strategy"] = "roundrobin";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_STRATEGY);
}

// strategy 输入与输出统一为 Linear / Striped（首字母大写）
TEST_F(TestUbseCliSsuCmdReg, CreateAcceptsCanonicalStrategyInput)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto params = CreateParams();
    params["ns_num"] = "2";
    params["strategy"] = "Linear";
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.strategy, UbseSsuAllocStrategy::LINEAR);

    params["strategy"] = "Striped";
    result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.strategy, UbseSsuAllocStrategy::STRIPED);
}

// 纯小写 "linear"/"striped" 输入不再被接受
TEST_F(TestUbseCliSsuCmdReg, CreateRejectsLowercaseStrategyInput)
{
    auto params = CreateParams();
    params["ns_num"] = "2";
    params["strategy"] = "striped";
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(params), ERR_INVALID_STRATEGY);
}

// --using_type 已从命令契约移除：若解析阶段透传未知参数，处理函数不再识别该参数，
// 不会触发 using_type 校验错误，命令按其余合法参数正常进入 IPC。
TEST_F(TestUbseCliSsuCmdReg, CreateIgnoresLegacyUsingTypeOption)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto params = CreateParams();
    params["using_type"] = "shared";
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    // using_type 不再进入请求体，create 仍按默认路径成功发起 IPC。
}

// 仅传必填项时，可选字段应取默认值（nsNum=1、512B、Linear、tenant 为空），并使用 ALLOC_CREATE 操作码。
TEST_F(TestUbseCliSsuCmdReg, CreateAppliesDefaults)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(CreateParams());
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(g_ssuMockLastModuleCode, SSU_MODULE_CODE);
    EXPECT_EQ(g_ssuMockLastOpCode, SsuOpCode(UbseSsuOpCode::UBSE_SSU_ALLOC_REQ));
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.name, "alloc-space-1");
    EXPECT_EQ(g_ssuMockLastCreateReq.nsSize, 10ULL * GIB);
    EXPECT_EQ(g_ssuMockLastCreateReq.nsNum, 1);
    EXPECT_EQ(g_ssuMockLastCreateReq.lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_512);
    EXPECT_EQ(g_ssuMockLastCreateReq.strategy, UbseSsuAllocStrategy::LINEAR);
    EXPECT_EQ(g_ssuMockLastCreateReq.tenant, "");
}

// 显式传入全部可选参数时，各值应被正确解析并序列化到请求体（覆盖默认值）。
TEST_F(TestUbseCliSsuCmdReg, CreateSerializesAllExplicitOptions)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    auto params = CreateParams();
    params["size"] = "20G";
    params["ns_num"] = "2";
    params["lba"] = "4K";
    params["strategy"] = "Striped";
    auto result = UbseCliRegSsuModule::UbseCliCreateSsuFunc(params);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastCreateReq.nsSize, 20ULL * GIB);
    EXPECT_EQ(g_ssuMockLastCreateReq.nsNum, 2);
    EXPECT_EQ(g_ssuMockLastCreateReq.lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_4K);
    EXPECT_EQ(g_ssuMockLastCreateReq.strategy, UbseSsuAllocStrategy::STRIPED);
}

// 分配成功应回显详情表，内容与详情查询布局一致。
TEST_F(TestUbseCliSsuCmdReg, CreatePrintsAllocatedDetailTable)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_create_invoke_call_normal));
    ExpectDetailOutput(UbseCliRegSsuModule::UbseCliCreateSsuFunc(CreateParams()));
}

// create 返回损坏报文时，应返回反序列化失败错误。
TEST_F(TestUbseCliSsuCmdReg, CreateReturnsDeserializationError)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_bad_response));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(CreateParams()), ERR_DESERIALIZATION);
}

// create 的 IPC 返回非零错误码时，应回显含错误码的 Internal error 文案。
TEST_F(TestUbseCliSsuCmdReg, CreateReturnsInternalErrorWithCode)
{
    MOCKER(&ubse_invoke_call).stubs().will(returnValue(1234));
    ExpectRenderedContains(UbseCliRegSsuModule::UbseCliCreateSsuFunc(CreateParams()),
                           "ERROR: Internal error with error code 1234.");
}

// 摘要查询使用合法的 0 字节请求体，mock 应显式捕获成功。
TEST_F(TestUbseCliSsuCmdReg, DisplaySummaryRequestUsesEmptyBody)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_summary_invoke_call_empty));
    UbseCliRegSsuModule::UbseCliDisplaySsuFunc({{"type", "alloc_summary"}});
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);

    ResetSsuMockCapture();
    uint8_t nonEmptyRequest = 0;
    ubse_api_buffer_t reqBuffer{&nonEmptyRequest, sizeof(nonEmptyRequest)};
    ubse_api_buffer_t resBuffer{};
    EXPECT_EQ(mock_ssu_alloc_summary_invoke_call_empty(
                  SSU_MODULE_CODE, SsuOpCode(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ), &reqBuffer, &resBuffer),
              UBSE_OK);
    EXPECT_FALSE(g_ssuMockLastRequestDeserialized);
    std::free(resBuffer.buffer);
}

// 详情查询请求体只携带 name。
TEST_F(TestUbseCliSsuCmdReg, DisplayDetailRequestCarriesName)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ssu_alloc_detail_invoke_call_normal));
    UbseCliRegSsuModule::UbseCliDisplaySsuFunc(DetailParams());
    EXPECT_TRUE(g_ssuMockLastRequestDeserialized);
    EXPECT_EQ(g_ssuMockLastDetailReq.name, "alloc-space-1");
    ASSERT_GE(g_ssuMockLastRequestPayload.size(), sizeof(uint32_t));
    uint32_t firstField = 0;
    std::memcpy(&firstField, g_ssuMockLastRequestPayload.data(), sizeof(firstField));
    EXPECT_EQ(firstField, std::string("alloc-space-1").size());
    EXPECT_EQ(g_ssuMockLastRequestPayload.size(), sizeof(uint32_t) + std::string("alloc-space-1").size());
}

} // namespace ubse::ut::cli
