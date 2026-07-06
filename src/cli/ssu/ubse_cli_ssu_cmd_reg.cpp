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

#include "ubse_cli_ssu_cmd_reg.h"

#include <pwd.h>
#include <unistd.h>
#include <cctype>
#include <limits>
#include <sstream>

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_ssu_struct.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"

namespace ubse::cli::reg {
// 向 CLI 框架注册 SSU 模块：框架在加载期通过宏收集模块类，统一调度命令字分发。
// "CLI_SSU_MODULE" 为模块唯一标识，UbseCliRegSsuModule 为负责 display/create ssu 命令的实现类。
UBSE_CLI_REGISTER_MODULE("CLI_SSU_MODULE", UbseCliRegSsuModule);

namespace {
constexpr uint64_t BYTES_PER_GIB = 1024ULL * 1024ULL * 1024ULL;
constexpr size_t MIN_SIZE_ARG_LEN = 2; // 至少为 "<数字>G"，仅含后缀时拒绝

// 参数名统一以长选项形式对外，短选项由命令注册处映射；此处常量是 params 字典的键。
const std::string TYPE_OPT = "type";
const std::string NAME_OPT = "name";
const std::string SIZE_OPT = "size";
const std::string LBA_OPT = "lba";
const std::string NS_NUM_OPT = "ns_num";
const std::string STRATEGY_OPT = "strategy";

// display ssu 的 -t 取值，区分摘要与详情两类子命令。
const std::string ALLOC_SUMMARY_TYPE = "alloc_summary";
const std::string ALLOC_DETAIL_TYPE = "alloc_detail";

// 枚举的规范字符串字面量：输入校验与表格输出共用同一份，避免大小写/拼写漂移导致输入输出不一致。
const std::string STRATEGY_LINEAR = "Linear";
const std::string STRATEGY_STRIPED = "Striped";
const std::string LBA_512B = "512B";
const std::string LBA_4K = "4K";

// 固定文本的客户端错误/提示信息
const std::string ERR_NAME_REQUIRED = "ERROR: The option -n or --name is required.";
const std::string ERR_SIZE_REQUIRED = "ERROR: The option -s or --size is required.";
const std::string ERR_INVALID_NAME =
    "ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'.";
const std::string ERR_INVALID_SIZE =
    "ERROR: Invalid size. The value must be an integer number of GiB with uppercase G suffix, minimum 1G.";
const std::string ERR_INVALID_LBA = "ERROR: Invalid lba. The value must be 512B or 4K.";
const std::string ERR_INVALID_STRATEGY = "ERROR: Invalid strategy. The value must be Linear or Striped.";
const std::string ERR_SERIALIZATION = "ERROR: Serialization failed in client.";
const std::string ERR_DESERIALIZATION = "ERROR: Deserialization failed in client.";
const std::string INFO_EMPTY = "INFO: No SSU allocation information found.";

std::string InternalError(uint32_t code)
{
    return "ERROR: Internal error with error code " + std::to_string(code) + ".";
}

// ns_num 的合法区间随 SSU_CLI_DEFAULT_NS_NUM/SSU_CLI_MAX_NS_NUM 变化，故区间按常量动态拼接，
// 避免改契约时错误文案与实际校验脱节（与 ParseNsNum 的判定保持单一来源）。
std::string InvalidNsNumError()
{
    return "ERROR: Invalid ns_num. The value must be an integer in range " +
        std::to_string(SSU_CLI_DEFAULT_NS_NUM) + "-" + std::to_string(SSU_CLI_MAX_NS_NUM) + ".";
}


// 容量统一按 GiB 整数化展示：CLI 契约规定 size 只以 G 为单位，子 GiB 余数丢弃（显示 0G），
// 不再回退到 M/K/纯字节，保证输入输出单位一致。
std::string SizeToString(uint64_t sizeBytes)
{
    return std::to_string(sizeBytes / BYTES_PER_GIB) + "G";
}

// 枚举 → 规范字符串：与 Parse* 函数反向对应，输入输出共用同一份字面量，保证往返一致。
std::string StrategyToString(UbseSsuAllocStrategy strategy)
{
    return strategy == UbseSsuAllocStrategy::STRIPED ? STRATEGY_STRIPED : STRATEGY_LINEAR;
}

std::string LbaToString(UbseSsuLBAFormat lbaFormat)
{
    return lbaFormat == UbseSsuLBAFormat::LBA_FORMAT_4K ? LBA_4K : LBA_512B;
}

// 取序列化结果缓冲：序列化器内部持有缓冲所有权，此处仅将其裸指针与长度导出为 IPC 输入参数。
// 仅做 Check + 借用，不拷贝、不释放；调用方需保证 serializer 生命周期长于 ubse_invoke_call。
bool AcquireSerializedBuffer(ubse::serial::UbseSerialization &serializer, ubse_api_buffer_t &buffer)
{
    if (!serializer.Check()) {
        return false;
    }
    buffer.buffer = serializer.GetBuffer();
    buffer.length = static_cast<uint32_t>(serializer.GetLength());
    return true;
}

// 填充运行态发起人身份：服务端依据 identityInfo.uid/userName 鉴权与归属。
// getpwuid 失败时回退为 uid 数字串，避免空用户名导致服务端归属判定异常。
void FillRuntimeUser(UbseSsuAllocIdentityInfo &identity)
{
    const uid_t uid = getuid();
    identity.uid = uid;
    const passwd *user = getpwuid(uid);
    identity.userName = user == nullptr ? std::to_string(uid) : std::string(user->pw_name);
}

// 摘要表 size 列由该分配下所有命名空间 nsSize 求和得出，与服务层 UbseSsuAllocResult 模型一致：
// 摘要不再由独立的 sizeBytes 字段承载，而是从 nameSpaceList 聚合。
uint64_t SumNameSpaceSize(const UbseCliSsuAllocResult &allocation)
{
    uint64_t total = 0;
    for (const auto &nameSpace : allocation.nameSpaceList) {
        total += nameSpace.nsSize;
    }
    return total;
}

std::shared_ptr<UbseCliResultEcho> BuildSummaryTable(const UbseCliSsuAllocListRsp &response);
std::shared_ptr<UbseCliResultEcho> BuildDetailTable(const UbseCliSsuAllocResult &response);
bool IsValidName(const std::string &name);
bool ParseSize(const std::string &value, uint64_t &sizeBytes);
bool ParseNsNum(const std::string &value, uint32_t &nsNum);
bool ParseLba(const std::string &value, UbseSsuLBAFormat &lbaFormat);
bool ParseStrategy(const std::string &value, UbseSsuAllocStrategy &strategy);
} // namespace

void UbseCliRegSsuModule::UbseCliSignUp()
{
    // 注册顺序即 display/create 命令进入模块命令列表的顺序，框架据此生成补全与帮助。
    this->cmd_.emplace_back(UbseCliDisplaySsu());
    this->cmd_.emplace_back(UbseCliCreateSsu());
}

// display ssu：仅 -t 必填，按 alloc_summary / alloc_detail 分流到不同子处理。
// -n 仅 alloc_detail 需要，留待子处理自行校验，避免在摘要路径上强校验。
UbseCliCommandInfo UbseCliRegSsuModule::UbseCliDisplaySsu()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("ssu")
        .UbseCliAddOption("t", TYPE_OPT, "SSU display type: alloc_summary or alloc_detail.")
        .UbseCliAddOption("n", NAME_OPT, "SSU allocation name.")
        .UbseCliSetFunc(UbseCliDisplaySsuFunc);
    return builder.UbseCliBuild();
}

// create ssu：-n/-s 必填，其余选项可选并带缺省值；短选项字母与长选项一一映射，供 bash completion 使用。
UbseCliCommandInfo UbseCliRegSsuModule::UbseCliCreateSsu()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("create")
        .UbseCliSetType("ssu")
        .UbseCliAddOption("n", NAME_OPT, "SSU allocation name.")
        .UbseCliAddOption("s", SIZE_OPT, "SSU allocation size.")
        .UbseCliAddOption("l", LBA_OPT, "SSU LBA format.")
        .UbseCliAddOption("m", NS_NUM_OPT, "SSU namespace count.")
        .UbseCliAddOption("r", STRATEGY_OPT, "SSU allocation strategy.")
        .UbseCliSetFunc(UbseCliCreateSsuFunc);
    return builder.UbseCliBuild();
}

// display ssu 入口：先按 -t 分流，缺失或非法 -t 直接返回错误，不进入 IPC 路径。
std::shared_ptr<UbseCliResultEcho> UbseCliRegSsuModule::UbseCliDisplaySsuFunc(
    [[maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto type = params.find(TYPE_OPT);
    if (type == params.end()) {
        return UbseCliStringPromptReply("ERROR: The option -t or --type is required.");
    }
    if (type->second == ALLOC_SUMMARY_TYPE) {
        return DisplayAllocSummary();
    }
    if (type->second == ALLOC_DETAIL_TYPE) {
        return DisplayAllocDetail(params);
    }
    return UbseCliStringPromptReply("ERROR: Invalid type. The value must be alloc_summary or alloc_detail.");
}

// 摘要查询：请求携带当前运行用户身份；空列表返回 INFO 提示而非空表，避免用户误判为故障。
std::shared_ptr<UbseCliResultEcho> UbseCliRegSsuModule::DisplayAllocSummary()
{
    UbseCliSsuAllocSummaryReq request;
    FillRuntimeUser(request.identityInfo);
    ubse::serial::UbseSerialization serializer;
    if (!request.Serialize(serializer)) {
        return UbseCliStringPromptReply(ERR_SERIALIZATION);
    }
    ubse_api_buffer_t reqBuffer{};
    if (!AcquireSerializedBuffer(serializer, reqBuffer)) {
        return UbseCliStringPromptReply(ERR_SERIALIZATION);
    }

    ubse_api_buffer_t resBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_SSU, UBSE_SSU_CLI_ALLOC_SUMMARY, &reqBuffer, &resBuffer);
    UbseCliBufferGuard guard(resBuffer); // RAII 释放服务端返回的 resBuffer，无论后续是否提前 return
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(InternalError(ret));
    }

    ubse::serial::UbseDeSerialization deserializer(resBuffer.buffer, resBuffer.length);
    UbseCliSsuAllocListRsp response;
    if (!response.Deserialize(deserializer)) {
        return UbseCliStringPromptReply(ERR_DESERIALIZATION);
    }
    if (response.allocations.empty()) {
        return UbseCliStringPromptReply(INFO_EMPTY);
    }
    return BuildSummaryTable(response);
}

// 详情查询：name 必填且需通过格式校验后再序列化发起 IPC，避免非法 name 打到服务端。
std::shared_ptr<UbseCliResultEcho> UbseCliRegSsuModule::DisplayAllocDetail(
    const std::map<std::string, std::string> &params)
{
    auto name = params.find(NAME_OPT);
    if (name == params.end()) {
        return UbseCliStringPromptReply(ERR_NAME_REQUIRED);
    }
    if (!IsValidName(name->second)) {
        return UbseCliStringPromptReply(ERR_INVALID_NAME);
    }

    UbseCliSsuAllocDetailReq request;
    request.name = name->second;
    FillRuntimeUser(request.identityInfo);
    ubse::serial::UbseSerialization serializer;
    if (!request.Serialize(serializer)) {
        return UbseCliStringPromptReply(ERR_SERIALIZATION);
    }
    ubse_api_buffer_t reqBuffer{};
    if (!AcquireSerializedBuffer(serializer, reqBuffer)) {
        return UbseCliStringPromptReply(ERR_SERIALIZATION);
    }

    ubse_api_buffer_t resBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_SSU, UBSE_SSU_CLI_ALLOC_DETAIL, &reqBuffer, &resBuffer);
    UbseCliBufferGuard guard(resBuffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(InternalError(ret));
    }

    ubse::serial::UbseDeSerialization deserializer(resBuffer.buffer, resBuffer.length);
    UbseCliSsuAllocResult response;
    if (!response.Deserialize(deserializer)) {
        return UbseCliStringPromptReply(ERR_DESERIALIZATION);
    }
    if (response.nameSpaceList.empty()) {
        return UbseCliStringPromptReply(INFO_EMPTY);
    }
    return BuildDetailTable(response);
}

// create ssu 入口：按"必填校验 → 格式校验 → 填充运行用户 → 序列化 → IPC → 回显"顺序推进，
// 任一校验失败即固定错误信息提前返回，不进入 IPC。可选参数缺省时沿用 request 的默认成员值。
std::shared_ptr<UbseCliResultEcho> UbseCliRegSsuModule::UbseCliCreateSsuFunc(
    [[maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto name = params.find(NAME_OPT);
    if (name == params.end()) {
        return UbseCliStringPromptReply(ERR_NAME_REQUIRED);
    }
    if (!IsValidName(name->second)) {
        return UbseCliStringPromptReply(ERR_INVALID_NAME);
    }

    auto size = params.find(SIZE_OPT);
    if (size == params.end()) {
        return UbseCliStringPromptReply(ERR_SIZE_REQUIRED);
    }

    UbseCliSsuAllocCreateReq request;
    request.name = name->second;
    if (!ParseSize(size->second, request.nsSize)) {
        return UbseCliStringPromptReply(ERR_INVALID_SIZE);
    }
    // 可选参数仅在用户传入时校验并覆盖默认值；find 命中后短路调用对应 Parse*，失败即返回。
    if (auto nsNum = params.find(NS_NUM_OPT); nsNum != params.end() && !ParseNsNum(nsNum->second, request.nsNum)) {
        return UbseCliStringPromptReply(InvalidNsNumError());
    }
    if (auto lba = params.find(LBA_OPT); lba != params.end() && !ParseLba(lba->second, request.lbaFormat)) {
        return UbseCliStringPromptReply(ERR_INVALID_LBA);
    }
    if (auto strategy = params.find(STRATEGY_OPT);
        strategy != params.end() && !ParseStrategy(strategy->second, request.strategy)) {
        return UbseCliStringPromptReply(ERR_INVALID_STRATEGY);
    }
    FillRuntimeUser(request.identityInfo);

    ubse::serial::UbseSerialization serializer;
    if (!request.Serialize(serializer)) {
        return UbseCliStringPromptReply(ERR_SERIALIZATION);
    }
    ubse_api_buffer_t reqBuffer{};
    if (!AcquireSerializedBuffer(serializer, reqBuffer)) {
        return UbseCliStringPromptReply(ERR_SERIALIZATION);
    }

    ubse_api_buffer_t resBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_SSU, UBSE_SSU_CLI_ALLOC_CREATE, &reqBuffer, &resBuffer);
    UbseCliBufferGuard guard(resBuffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(InternalError(ret));
    }

    ubse::serial::UbseDeSerialization deserializer(resBuffer.buffer, resBuffer.length);
    UbseCliSsuAllocResult response;
    if (!response.Deserialize(deserializer)) {
        return UbseCliStringPromptReply(ERR_DESERIALIZATION);
    }
    if (response.nameSpaceList.empty()) {
        return UbseCliStringPromptReply(INFO_EMPTY);
    }
    return BuildDetailTable(response);
}

namespace {
// 摘要表：3 列（name/size/strategy），列宽按 UBSE_CLI_NUM_8*10 固定，与 ubse_cli_ssu.md 示例输出对齐。
// size 由每个分配下 nameSpaceList[*].nsSize 求和得出，与服务层结果模型一致。
std::shared_ptr<UbseCliResultEcho> BuildSummaryTable(const UbseCliSsuAllocListRsp &response)
{
    UbseCliResBuilder builder(UBSE_CLI_NUM_3, UBSE_CLI_NUM_8 * UBSE_CLI_NUM_10);
    size_t row = builder.UbseCliAddRow();
    builder.UbseCliAddlineSeparate(row);
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "name");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "size");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "strategy");
    builder.UbseCliAddBottomlineSeparate();
    for (const auto &allocation : response.allocations) {
        row = builder.UbseCliAddRow();
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, allocation.name);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, SizeToString(SumNameSpaceSize(allocation)));
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, StrategyToString(allocation.strategy));
    }
    builder.UbseCliAddBottomlineSeparate();
    return UbseCliRegModule::UbseCliVariableCelReply(builder.UbseCliVariableCellBuild());
}

// 详情表：7 列命名空间信息 + 表头合并行（Name/Strategy），列顺序与 ubse_cli_ssu.md 输出信息说明一致；
// alloc/create 两类命令共用此布局。字段对齐服务层 UbseSsuNameSpaceInfo，不再展示 using_type 与 SrcNqnList。
std::shared_ptr<UbseCliResultEcho> BuildDetailTable(const UbseCliSsuAllocResult &response)
{
    UbseCliResBuilder builder(UBSE_CLI_NUM_7, UBSE_CLI_NUM_8 * UBSE_CLI_NUM_10);
    std::map<size_t, std::string> title{
        {UBSE_CLI_NUM_7, "Name: " + response.name + "  Strategy:" + StrategyToString(response.strategy)}};
    builder.UbseCliAddlineSeparate(UBSE_CLI_NUM_1);
    builder.UbseCliAddMergeRow(title);
    builder.UbseCliAddBottomlineSeparate();

    size_t row = builder.UbseCliAddRow();
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "ns_uuid");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "tgt_eid");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "tgt_nqn");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "namespace_id");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "ns_dev_path");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "ns_size");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "lba_format");
    builder.UbseCliAddBottomlineSeparate();

    for (const auto &nameSpace : response.nameSpaceList) {
        row = builder.UbseCliAddRow();
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, nameSpace.nsUuid);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, nameSpace.tgtEid);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, nameSpace.tgtNqn);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, std::to_string(nameSpace.namespaceId));
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, nameSpace.nsDevPath);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, SizeToString(nameSpace.nsSize));
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, LbaToString(nameSpace.lbaFormat));
    }
    builder.UbseCliAddBottomlineSeparate();
    return UbseCliRegModule::UbseCliVariableCelReply(builder.UbseCliVariableCellBuild());
}

// name 格式契约：1-48 字符（与 SSU_CLI_MAX_NAME_LENGTH 一致），仅字母/数字/./:/-/_；unsigned char 转型避免 char 为负时 isalnum 未定义行为。
bool IsValidName(const std::string &name)
{
    if (name.empty() || name.size() > SSU_CLI_MAX_NAME_LENGTH) {
        return false;
    }
    for (unsigned char ch : name) {
        if (std::isalnum(ch) || ch == '.' || ch == ':' || ch == '-' || ch == '_') {
            continue;
        }
        return false;
    }
    return true;
}

// size 解析：仅接受大写 G 后缀的正整数字符串；先逐字符判数字防 stoull 隐式截断，
// 再绑定 SSU_CLI_MIN_SIZE_BYTES 下限与 BYTES_PER_GIB 溢出上限，二者均失败才拒绝。
bool ParseSize(const std::string &value, uint64_t &sizeBytes)
{
    if (value.size() < MIN_SIZE_ARG_LEN || value.back() != 'G') {
        return false;
    }
    const std::string number = value.substr(0, value.size() - 1);
    if (number.empty()) {
        return false;
    }
    for (unsigned char ch : number) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }
    try {
        const uint64_t gib = std::stoull(number);
        // 先判溢出上界再乘，避免无符号回绕干扰静态分析/sanitizer；gib == 0 由下限兜底拒绝
        if (gib > std::numeric_limits<uint64_t>::max() / BYTES_PER_GIB) {
            return false;
        }
        const uint64_t bytes = gib * BYTES_PER_GIB;
        if (bytes < SSU_CLI_MIN_SIZE_BYTES) {
            return false;
        }
        sizeBytes = bytes;
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

// ns_num 解析：仅正整数字符串，区间 [SSU_CLI_DEFAULT_NS_NUM, SSU_CLI_MAX_NS_NUM]，
// 逐字符判数字后再 stoull，超大输入由区间判定拒绝而非依赖异常。
bool ParseNsNum(const std::string &value, uint32_t &nsNum)
{
    if (value.empty()) {
        return false;
    }
    for (unsigned char ch : value) {
        if (!std::isdigit(ch)) {
            return false;
        }
    }
    try {
        uint64_t parsed = std::stoull(value);
        if (parsed < SSU_CLI_DEFAULT_NS_NUM || parsed > SSU_CLI_MAX_NS_NUM) {
            return false;
        }
        nsNum = static_cast<uint32_t>(parsed);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}


// 以下 Parse* 为字符串 → 枚举的精确匹配，仅接受规范字面量（与 *ToString 共用常量），
// 拒绝大小写变体与旧拼写，保证输入输出往返一致。
bool ParseLba(const std::string &value, UbseSsuLBAFormat &lbaFormat)
{
    if (value == LBA_512B) {
        lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
        return true;
    }
    if (value == LBA_4K) {
        lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
        return true;
    }
    return false;
}


bool ParseStrategy(const std::string &value, UbseSsuAllocStrategy &strategy)
{
    if (value == STRATEGY_LINEAR) {
        strategy = UbseSsuAllocStrategy::LINEAR;
        return true;
    }
    if (value == STRATEGY_STRIPED) {
        strategy = UbseSsuAllocStrategy::STRIPED;
        return true;
    }
    return false;
}
} // namespace
} // namespace ubse::cli::reg
