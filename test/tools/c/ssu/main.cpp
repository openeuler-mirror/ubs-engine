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

// UBS-Engine SSU 命令行测试工具 (ubse_ssu_test_c)
// 面向 ubs_engine_ssu.h 的交互式命令行工具, 仅调用接口, 不做性能测试
//   - 交互式 REPL (prompt: ubse_ssu_test_c>)
//   - 单次执行模式: 命令跟在程序名后, 执行完即退出
//   - 参数风格: 纯位置参数, 无可选/可省略; 空字符串用 "", 数值零用 0
//   - 枚举值填写 SDK 底层十进制数值 (lba_format/strategy/level/chunk_size)
//   - SDK 调用结果以 JSON 应答输出; help/prompt 为普通文本
// 编译示例 (见 build.sh):
//   bash build.sh          # 运行时 dlopen 真实 libubse-ssu-client.so

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ubs_engine.h"
#include "ubs_engine_ssu.h"

namespace ubse_dynamic {

inline void* LoadLibrary(const char* environment, const char* fallback)
{
    const char* configured = std::getenv(environment);
    const char* library = configured && *configured ? configured : fallback;
    return dlopen(library, RTLD_NOW | RTLD_LOCAL);
}

inline void* SsuLibraryHandle()
{
    static void* handle = LoadLibrary("UBSE_SSU_CLIENT_LIBRARY", "libubse-ssu-client.so");
    return handle;
}

inline void* ClientLibraryHandle()
{
    static void* handle = LoadLibrary("UBSE_CLIENT_LIBRARY", "libubse-client.so");
    return handle;
}

template<typename T>
T LoadSymbol(const char* name)
{
    // 新版将 SSU API 拆到 libubse-ssu-client.so，通用初始化和错误码 API
    // 仍在 libubse-client.so。先查 SSU 库再查主库，同时兼容旧版单库布局。
    void* handles[] = {SsuLibraryHandle(), ClientLibraryHandle()};
    for (void* handle : handles) {
        if (handle == nullptr) {
            continue;
        }
        dlerror();
        void* symbol = dlsym(handle, name);
        if (dlerror() == nullptr) {
            return reinterpret_cast<T>(symbol);
        }
    }
    std::cerr << "ERROR: 无法从 libubse-ssu-client.so 或 libubse-client.so 解析符号 " << name << std::endl;
    std::exit(EXIT_FAILURE);
}

} // namespace ubse_dynamic

#define UBSE_DYNAMIC_CALL(function) (*[] { \
    static auto symbol = ubse_dynamic::LoadSymbol<decltype(&function)>(#function); \
    return symbol; \
}())
#define ubs_engine_client_initialize UBSE_DYNAMIC_CALL(ubs_engine_client_initialize)
#define ubs_engine_client_finalize UBSE_DYNAMIC_CALL(ubs_engine_client_finalize)
#define ubs_error_name UBSE_DYNAMIC_CALL(ubs_error_name)
#define ubs_error_string UBSE_DYNAMIC_CALL(ubs_error_string)
#define ubs_ssu_access_permission_add UBSE_DYNAMIC_CALL(ubs_ssu_access_permission_add)
#define ubs_ssu_access_permission_remove UBSE_DYNAMIC_CALL(ubs_ssu_access_permission_remove)
#define ubs_ssu_alloc_info_free UBSE_DYNAMIC_CALL(ubs_ssu_alloc_info_free)
#define ubs_ssu_alloc_info_list UBSE_DYNAMIC_CALL(ubs_ssu_alloc_info_list)
#define ubs_ssu_alloc_info_list_free UBSE_DYNAMIC_CALL(ubs_ssu_alloc_info_list_free)
#define ubs_ssu_connect_info_free UBSE_DYNAMIC_CALL(ubs_ssu_connect_info_free)
#define ubs_ssu_connect_info_get UBSE_DYNAMIC_CALL(ubs_ssu_connect_info_get)
#define ubs_ssu_fe_device_alloc UBSE_DYNAMIC_CALL(ubs_ssu_fe_device_alloc)
#define ubs_ssu_fe_device_free UBSE_DYNAMIC_CALL(ubs_ssu_fe_device_free)
#define ubs_ssu_fe_device_list UBSE_DYNAMIC_CALL(ubs_ssu_fe_device_list)
#define ubs_ssu_fe_device_list_free UBSE_DYNAMIC_CALL(ubs_ssu_fe_device_list_free)
#define ubs_ssu_linear_space_attach UBSE_DYNAMIC_CALL(ubs_ssu_linear_space_attach)
#define ubs_ssu_linear_space_detach UBSE_DYNAMIC_CALL(ubs_ssu_linear_space_detach)
#define ubs_ssu_ns_dev_paths_free UBSE_DYNAMIC_CALL(ubs_ssu_ns_dev_paths_free)
#define ubs_ssu_ns_stats_free UBSE_DYNAMIC_CALL(ubs_ssu_ns_stats_free)
#define ubs_ssu_ns_stats_get UBSE_DYNAMIC_CALL(ubs_ssu_ns_stats_get)
#define ubs_ssu_space_alloc UBSE_DYNAMIC_CALL(ubs_ssu_space_alloc)
#define ubs_ssu_space_attach UBSE_DYNAMIC_CALL(ubs_ssu_space_attach)
#define ubs_ssu_space_detach UBSE_DYNAMIC_CALL(ubs_ssu_space_detach)
#define ubs_ssu_space_free UBSE_DYNAMIC_CALL(ubs_ssu_space_free)
#define ubs_ssu_striped_space_attach UBSE_DYNAMIC_CALL(ubs_ssu_striped_space_attach)
#define ubs_ssu_striped_space_detach UBSE_DYNAMIC_CALL(ubs_ssu_striped_space_detach)

namespace {

// ==================== 字符串工具 ====================

/**
 * 按空白切分, 支持双引号包裹含空格或空的参数
 * 示例: ssu_get_connect_info test-space 0 "" ""  → 5 个 token, 含 2 个空串
 */
std::vector<std::string> SplitTokens(const std::string& line)
{
    std::vector<std::string> tokens;
    std::string token;
    bool inQuote = false;
    bool hasToken = false; // 是否正在构建 token (含空 token)
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (!inQuote) {
                hasToken = true;
            }
            inQuote = !inQuote;
        } else if (std::isspace(static_cast<unsigned char>(c)) && !inQuote) {
            if (hasToken) {
                tokens.push_back(token);
                token.clear();
                hasToken = false;
            }
        } else {
            token += c;
            hasToken = true;
        }
    }
    if (hasToken) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Trim(const std::string& s)
{
    auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

/** 安全拷贝定长字符数组, 保证末尾 '\0' */
void CopyToFixed(char* dst, size_t dstSize, const std::string& src)
{
    if (dstSize == 0) {
        return;
    }
    size_t copyLen = std::min(src.size(), dstSize - 1);
    std::memcpy(dst, src.c_str(), copyLen);
    dst[copyLen] = '\0';
}

/** 将 32 字符 ASCII 串填入 GUID 字节数组, 不足补零, 超出截断 */
void FillGuid(uint8_t* dst, const std::string& src)
{
    std::memset(dst, 0, UBS_SSU_GUID_LENGTH);
    size_t len = std::min(src.size(), static_cast<size_t>(UBS_SSU_GUID_LENGTH));
    std::memcpy(dst, src.data(), len);
}

// ==================== JSON 辅助 ====================

/** JSON 字符串转义: 处理控制字符和非 ASCII 字节 */
std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        auto uc = static_cast<unsigned char>(c);
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (uc < 0x20 || uc >= 0x7f) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", uc);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string JStr(const std::string& s)
{
    return "\"" + JsonEscape(s) + "\"";
}

std::string JNum(uint64_t n)
{
    return std::to_string(n);
}

std::string JField(const std::string& key, const std::string& jsonVal)
{
    return JStr(key) + ":" + jsonVal;
}

std::string JArray(const std::vector<std::string>& items)
{
    std::string s = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            s += ",";
        }
        s += items[i];
    }
    return s + "]";
}

std::string JObject(const std::vector<std::string>& fields)
{
    std::string s = "{";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            s += ",";
        }
        s += fields[i];
    }
    return s + "}";
}

/** GUID (32 字节) 转 JSON 字符串, 按 ASCII 透传, 非 ASCII 转义 */
std::string GuidToJson(const uint8_t* guid)
{
    std::string s(reinterpret_cast<const char*>(guid), UBS_SSU_GUID_LENGTH);
    return JStr(s);
}

// ==================== 纯位置参数解析器 ====================

/**
 * 简易位置参数解析器
 * 所有参数均为必填位置参数, 无选项/默认值/可省略
 */
class ArgParser {
public:
    explicit ArgParser(const std::string& cmd) : cmd_(cmd) {}

    void AddPositional(const std::string& name)
    {
        positionals_.push_back(name);
    }

    /**
     * 解析参数, 严格校验数量
     * @return 成功返回 true, 失败填充 errMsg
     */
    bool Parse(const std::vector<std::string>& args, std::string& errMsg)
    {
        if (args.size() < positionals_.size()) {
            errMsg = cmd_ + " 参数不足, 期望 " + std::to_string(positionals_.size()) + " 个, 实际 " +
                     std::to_string(args.size()) + " 个";
            return false;
        }
        if (args.size() > positionals_.size()) {
            errMsg = cmd_ + " 参数过多, 期望 " + std::to_string(positionals_.size()) + " 个, 实际 " +
                     std::to_string(args.size()) + " 个";
            return false;
        }
        for (size_t i = 0; i < positionals_.size(); ++i) {
            values_[positionals_[i]] = args[i];
        }
        return true;
    }

    std::string Get(const std::string& name) const
    {
        auto it = values_.find(name);
        return it != values_.end() ? it->second : "";
    }

    uint8_t GetUint8(const std::string& name) const
    {
        return static_cast<uint8_t>(GetUint32(name));
    }

    uint16_t GetUint16(const std::string& name) const
    {
        return static_cast<uint16_t>(GetUint32(name));
    }

    uint32_t GetUint32(const std::string& name) const
    {
        return static_cast<uint32_t>(GetUint64(name));
    }

    uint64_t GetUint64(const std::string& name) const
    {
        const std::string& v = Get(name);
        try {
            return std::stoull(v);
        } catch (...) {
            parseError_ = true;
            return 0;
        }
    }

    /** 数值参数转换是否发生过失败 */
    bool HasParseError() const { return parseError_; }

private:
    std::string cmd_;
    std::vector<std::string> positionals_;
    std::map<std::string, std::string> values_;
    mutable bool parseError_ = false;
};

} // namespace

// UBS-Engine SSU 命令行应用主类
class UbseSsuApp {
public:
    UbseSsuApp() : initialized_(false)
    {
        int32_t ret = ubs_engine_client_initialize(nullptr);
        if (ret == UBS_SUCCESS) {
            initialized_ = true;
        } else {
            std::cerr << "ERROR: UBSE 客户端初始化失败: " << PrintError(ret) << std::endl;
        }
    }

    ~UbseSsuApp()
    {
        if (initialized_) {
            ubs_engine_client_finalize();
        }
    }

    /** 交互式 REPL 主循环 */
    void Run()
    {
        std::cout << "Welcome to ubse_ssu_test_c. Type help or ? to list commands." << std::endl;
        std::string line;
        while (true) {
            std::cout << "ubse_ssu_test_c> " << std::flush;
            if (!std::getline(std::cin, line)) {
                std::cout << std::endl;
                break;
            }
            std::string trimmed = Trim(line);
            if (trimmed.empty()) {
                continue;
            }
            if (!DispatchOne(SplitTokens(trimmed))) {
                break;
            }
        }
    }

    /**
     * 单次执行模式: 直接运行一条命令后退出
     * @return 退出码: 0=成功; 1=未知命令/参数错误/SDK失败; 2=quit等控制命令
     */
    int ExecuteOnce(int argc, char* argv[])
    {
        std::vector<std::string> tokens;
        tokens.reserve(argc - 1);
        for (int i = 1; i < argc; ++i) {
            tokens.emplace_back(argv[i]);
        }
        if (tokens.empty()) {
            PrintHelp();
            return 1;
        }
        lastCmdOk_ = true;
        bool cont = DispatchOne(tokens);
        if (!cont) {
            return 2;
        }
        return lastCmdOk_ ? 0 : 1;
    }

private:
    /** 分发单条命令, 返回 false 表示应退出 (quit/exit) */
    bool DispatchOne(const std::vector<std::string>& tokens)
    {
        if (tokens.empty()) {
            return true;
        }
        const std::string& cmd = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        if (cmd == "quit" || cmd == "exit") {
            return false;
        }
        if (cmd == "help" || cmd == "?") {
            if (args.empty()) {
                PrintHelp();
            } else {
                PrintHelp(args[0]);
            }
            return true;
        }

        auto it = commands_.find(cmd);
        if (it == commands_.end()) {
            std::cerr << "ERROR: 未知命令: " << cmd << " (输入 help 查看可用命令)" << std::endl;
            lastCmdOk_ = false;
            return true;
        }
        try {
            (this->*(it->second))(args);
        } catch (const std::exception& e) {
            std::cerr << "ERROR: 命令执行异常: " << e.what() << std::endl;
            lastCmdOk_ = false;
        }
        return true;
    }

    using CmdHandler = void (UbseSsuApp::*)(const std::vector<std::string>&);

    // 命令注册表, 命名规律: ssu_<动词>_<对象>
    std::map<std::string, CmdHandler> commands_ = {
        {"ssu_alloc_space", &UbseSsuApp::DoSpaceAlloc},
        {"ssu_free_space", &UbseSsuApp::DoSpaceFree},
        {"ssu_list_alloc_info", &UbseSsuApp::DoAllocInfoList},
        {"ssu_get_ns_stats", &UbseSsuApp::DoNsStatsGet},
        {"ssu_get_connect_info", &UbseSsuApp::DoConnectInfoGet},
        {"ssu_add_access_permission", &UbseSsuApp::DoAccessPermissionAdd},
        {"ssu_remove_access_permission", &UbseSsuApp::DoAccessPermissionRemove},
        {"ssu_attach_space", &UbseSsuApp::DoSpaceAttach},
        {"ssu_detach_space", &UbseSsuApp::DoSpaceDetach},
        {"ssu_attach_linear_space", &UbseSsuApp::DoLinearSpaceAttach},
        {"ssu_detach_linear_space", &UbseSsuApp::DoLinearSpaceDetach},
        {"ssu_attach_striped_space", &UbseSsuApp::DoStripedSpaceAttach},
        {"ssu_detach_striped_space", &UbseSsuApp::DoStripedSpaceDetach},
        {"ssu_get_fe_device_list", &UbseSsuApp::DoFeDeviceList},
        {"ssu_fe_device_alloc", &UbseSsuApp::DoFeDeviceAlloc},
        {"ssu_fe_device_free", &UbseSsuApp::DoFeDeviceFree},
    };

    bool initialized_;
    bool lastCmdOk_ = true;

    // ==================== 通用辅助 ====================

    std::string PrintError(int32_t errCode)
    {
        const char* name = ubs_error_name(errCode);
        const char* desc = ubs_error_string(errCode);
        std::ostringstream oss;
        oss << "[UBS Error " << errCode << "] " << (name ? name : "UNKNOWN") << ": " << (desc ? desc : "Unknown error");
        return oss.str();
    }

    /** 输出 SDK 成功应答 (JSON) */
    void EmitResponse(const std::string& jsonVal)
    {
        std::cout << "{\"response\":" << jsonVal << "}" << std::endl;
    }

    /** 输出固定成功应答 (无业务返回值时) */
    void EmitSuccess()
    {
        std::cout << "{\"response\":\"success\"}" << std::endl;
    }

    /** 输出 SDK 错误应答 (JSON), 并标记命令失败 */
    void EmitError(int32_t ret)
    {
        const char* desc = ubs_error_string(ret);
        std::cout << "{\"error\":" << JStr(desc ? desc : "unknown error") << "}" << std::endl;
        lastCmdOk_ = false;
    }

    /** 统一处理参数解析失败: 置 lastCmdOk_=false 并打印错误到 stderr */
    bool ParseArgsOrReport(ArgParser& parser, const std::vector<std::string>& args, std::string& errMsg)
    {
        if (parser.Parse(args, errMsg)) {
            return true;
        }
        lastCmdOk_ = false;
        std::cerr << "ERROR: " << errMsg << std::endl;
        return false;
    }

    /** 检查数值参数转换是否失败, 失败则打印 "输入格式不对" 并返回 false, 避免进入后续 SDK 校验流程 */
    bool CheckFormatOrReport(const ArgParser& parser)
    {
        if (parser.HasParseError()) {
            lastCmdOk_ = false;
            std::cerr << "ERROR: 输入格式不对" << std::endl;
            return false;
        }
        return true;
    }

    /** 简单成功/失败输出: 成功输出 success, 失败输出 error */
    void EmitSimpleResult(int32_t ret)
    {
        if (ret == UBS_SUCCESS) {
            EmitSuccess();
        } else {
            EmitError(ret);
        }
    }

    // ==================== 请求结构体组装辅助 ====================

    /** 从 parser 组装 ubs_ub_vfe_t (slot_id/chip_id/die_id/pfe_id/vfe_id/guid) */
    void FillVfeFromParser(ubs_ub_vfe_t& vfe, const ArgParser& parser)
    {
        std::memset(&vfe, 0, sizeof(vfe));
        vfe.slot_id = parser.GetUint8("slot_id");
        vfe.chip_id = parser.GetUint8("chip_id");
        vfe.die_id = parser.GetUint8("die_id");
        vfe.pfe_id = parser.GetUint16("pfe_id");
        vfe.vfe_id = parser.GetUint16("vfe_id");
        FillGuid(vfe.vfe_guid, parser.Get("vfe_guid"));
        FillGuid(vfe.bind_bus_instance_guid, parser.Get("bind_bus_instance_guid"));
    }

    /** 从 parser 组装 ubs_ssu_space_req_t (name/nqn/src_eid) */
    void FillSpaceReq(ubs_ssu_space_req_t& req, const ArgParser& parser)
    {
        std::memset(&req, 0, sizeof(req));
        CopyToFixed(req.name, UBS_SSU_MAX_NAME_LENGTH, parser.Get("name"));
        CopyToFixed(req.nqn, UBS_SSU_MAX_NQN_LENGTH, parser.Get("nqn"));
        CopyToFixed(req.src_eid, UBS_SSU_MAX_EID_LENGTH, parser.Get("src_eid"));
    }

    /** 从 parser 组装 ubs_ssu_linear_space_req_t (name/nqn/src_eid/dev_name) */
    void FillLinearSpaceReq(ubs_ssu_linear_space_req_t& req, const ArgParser& parser)
    {
        std::memset(&req, 0, sizeof(req));
        CopyToFixed(req.name, UBS_SSU_MAX_NAME_LENGTH, parser.Get("name"));
        CopyToFixed(req.nqn, UBS_SSU_MAX_NQN_LENGTH, parser.Get("nqn"));
        CopyToFixed(req.src_eid, UBS_SSU_MAX_EID_LENGTH, parser.Get("src_eid"));
        CopyToFixed(req.dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH, parser.Get("dev_name"));
    }

    /** 从 parser 组装 ubs_ssu_striped_space_req_t (name/nqn/src_eid/dev_name/level/chunk_size) */
    void FillStripedSpaceReq(ubs_ssu_striped_space_req_t& req, const ArgParser& parser)
    {
        std::memset(&req, 0, sizeof(req));
        CopyToFixed(req.name, UBS_SSU_MAX_NAME_LENGTH, parser.Get("name"));
        CopyToFixed(req.nqn, UBS_SSU_MAX_NQN_LENGTH, parser.Get("nqn"));
        CopyToFixed(req.src_eid, UBS_SSU_MAX_EID_LENGTH, parser.Get("src_eid"));
        CopyToFixed(req.dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH, parser.Get("dev_name"));
        req.level = static_cast<ubs_ssu_raid_level_t>(parser.GetUint32("level"));
        req.chunk_size = static_cast<ubs_ssu_chunk_size_t>(parser.GetUint32("chunk_size"));
    }

    // ==================== JSON 结构体编码 ====================

    std::string NamespaceInfoToJson(const ubs_ssu_namespace_info_t& ns)
    {
        std::vector<std::string> hostNqns;
        for (uint32_t i = 0; i < ns.nqn_count; ++i) {
            hostNqns.push_back(JStr(ns.host_nqns && ns.host_nqns[i] ? ns.host_nqns[i] : ""));
        }
        return JObject({
            JField("TgtEid", JStr(ns.tgt_eid)),
            JField("TgtNqn", JStr(ns.tgt_nqn)),
            JField("NsUuid", JStr(ns.ns_uuid)),
            JField("NsId", JNum(ns.ns_id)),
            JField("NsDevPath", JStr(ns.ns_dev_path)),
            JField("NsSize", JNum(ns.ns_size)),
            JField("LbaFormat", JNum(static_cast<uint32_t>(ns.lba_format))),
            JField("HostNqns", JArray(hostNqns)),
        });
    }

    std::string AllocResultToJson(const ubs_ssu_alloc_result_t& r)
    {
        std::vector<std::string> nsList;
        for (uint32_t i = 0; i < r.namespace_cnt; ++i) {
            nsList.push_back(NamespaceInfoToJson(r.namespaces[i]));
        }
        return JObject({
            JField("Name", JStr(r.name)),
            JField("Strategy", JNum(static_cast<uint32_t>(r.strategy))),
            JField("Namespaces", JArray(nsList)),
        });
    }

    std::string NsStatsToJson(const ubs_ssu_ns_stats_t& s)
    {
        return JObject({
            JField("NsUuid", JStr(s.ns_uuid)),
            JField("NsId", JNum(s.ns_id)),
            JField("TotalSize", JNum(s.total_size)),
            JField("UsedSize", JNum(s.used_size)),
        });
    }

    std::string ConnectInfoToJson(const ubs_ssu_connect_info_t& c)
    {
        return JObject({
            JField("SrcEid", JStr(c.src_eid)),
            JField("TgtEid", JStr(c.tgt_eid)),
            JField("TgtNqn", JStr(c.tgt_nqn)),
            JField("HostNqn", JStr(c.host_nqn)),
            JField("NsUuid", JStr(c.ns_uuid)),
            JField("NsId", JNum(c.ns_id)),
        });
    }

    std::string VfeToJson(const ubs_ub_vfe_t& v)
    {
        return JObject({
            JField("SlotId", JNum(v.slot_id)),
            JField("ChipId", JNum(v.chip_id)),
            JField("DieId", JNum(v.die_id)),
            JField("PfeId", JNum(v.pfe_id)),
            JField("VfeId", JNum(v.vfe_id)),
            JField("VfeGuid", GuidToJson(v.vfe_guid)),
            JField("BindBusInstanceGuid", GuidToJson(v.bind_bus_instance_guid)),
        });
    }

    std::string FeToJson(const ubs_ub_fe_t& fe)
    {
        std::vector<std::string> vfeList;
        for (uint32_t i = 0; i < fe.vfe_cnt; ++i) {
            vfeList.push_back(VfeToJson(fe.vfe_list[i]));
        }
        return JObject({
            JField("SlotId", JNum(fe.slot_id)),
            JField("ChipId", JNum(fe.chip_id)),
            JField("DieId", JNum(fe.die_id)),
            JField("PfeId", JNum(fe.pfe_id)),
            JField("PfeGuid", GuidToJson(fe.pfe_guid)),
            JField("VfeCnt", JNum(fe.vfe_cnt)),
            JField("VfeList", JArray(vfeList)),
        });
    }

    /** 将 ns_dev_paths (char**) 编码为 JSON 字符串数组 */
    std::string NsDevPathsToJson(char** paths, uint32_t cnt)
    {
        std::vector<std::string> arr;
        for (uint32_t i = 0; i < cnt; ++i) {
            arr.push_back(JStr(paths && paths[i] ? paths[i] : ""));
        }
        return JArray(arr);
    }

    // ==================== help ====================

    /** 命令用法表, 供 PrintHelp 和 help <cmd> 共用 */
    static const std::map<std::string, std::string>& UsageTable()
    {
        static const std::map<std::string, std::string> table = {
            {"ssu_alloc_space", "ssu_alloc_space <name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>"},
            {"ssu_free_space", "ssu_free_space <name>"},
            {"ssu_list_alloc_info", "ssu_list_alloc_info"},
            {"ssu_get_ns_stats", "ssu_get_ns_stats <name>"},
            {"ssu_get_connect_info", "ssu_get_connect_info <name> <vfe_present> <slot_id> <chip_id> <die_id> <pfe_id> "
                                     "<vfe_id> <vfe_guid> <bind_bus_instance_guid>"},
            {"ssu_add_access_permission", "ssu_add_access_permission <name> <nqn>"},
            {"ssu_remove_access_permission", "ssu_remove_access_permission <name> <nqn>"},
            {"ssu_attach_space", "ssu_attach_space <name> <nqn> <src_eid>"},
            {"ssu_detach_space", "ssu_detach_space <name> <nqn> <src_eid>"},
            {"ssu_attach_linear_space", "ssu_attach_linear_space <name> <nqn> <src_eid> <dev_name>"},
            {"ssu_detach_linear_space", "ssu_detach_linear_space <name> <nqn> <src_eid> <dev_name>"},
            {"ssu_attach_striped_space",
             "ssu_attach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>"},
            {"ssu_detach_striped_space",
             "ssu_detach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>"},
            {"ssu_get_fe_device_list", "ssu_get_fe_device_list"},
            {"ssu_fe_device_alloc", "ssu_fe_device_alloc <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> "
                                    "<vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>"},
            {"ssu_fe_device_free", "ssu_fe_device_free <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> "
                                   "<bind_bus_instance_guid>"},
        };
        return table;
    }

    void PrintHelp()
    {
        std::cout << "可用命令 (命名规律: ssu_<动词>_<对象>):" << std::endl;
        for (const auto& kv : UsageTable()) {
            std::cout << "  " << kv.second << std::endl;
        }
        std::cout << "  help | ? [command]" << std::endl;
        std::cout << "  quit | exit" << std::endl;
    }

    void PrintHelp(const std::string& cmd)
    {
        const auto& table = UsageTable();
        auto it = table.find(cmd);
        if (it != table.end()) {
            std::cout << "  " << it->second << std::endl;
        } else {
            std::cerr << "ERROR: 未知命令: " << cmd << std::endl;
        }
    }

    // ==================== 命令实现 ====================

    // ssu_alloc_space <name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>
    void DoSpaceAlloc(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_alloc_space");
        parser.AddPositional("name");
        parser.AddPositional("ns_size");
        parser.AddPositional("ns_num");
        parser.AddPositional("lba_format");
        parser.AddPositional("strategy");
        parser.AddPositional("tenant");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }

        ubs_ssu_alloc_space_req_t req;
        std::memset(&req, 0, sizeof(req));
        CopyToFixed(req.name, UBS_SSU_MAX_NAME_LENGTH, parser.Get("name"));
        req.ns_size = parser.GetUint64("ns_size");
        req.ns_num = parser.GetUint32("ns_num");
        req.lba_format = static_cast<ubs_ssu_lba_format_t>(parser.GetUint32("lba_format"));
        req.strategy = static_cast<ubs_ssu_alloc_strategy_t>(parser.GetUint32("strategy"));
        CopyToFixed(req.tenant, UBS_SSU_MAX_TENANT_LENGTH, parser.Get("tenant"));

        if (!CheckFormatOrReport(parser)) {
            return;
        }

        ubs_ssu_alloc_result_t* result = nullptr;
        int32_t ret = ubs_ssu_space_alloc(&req, &result);
        if (ret == UBS_SUCCESS) {
            std::string json = result ? AllocResultToJson(*result) : JObject({});
            EmitResponse(json);
            if (result != nullptr) {
                ubs_ssu_alloc_info_free(&result);
            }
        } else {
            EmitError(ret);
        }
    }

    // ssu_free_space <name>
    void DoSpaceFree(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_free_space");
        parser.AddPositional("name");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        int32_t ret = ubs_ssu_space_free(parser.Get("name").c_str());
        EmitSimpleResult(ret);
    }

    // ssu_list_alloc_info
    void DoAllocInfoList(const std::vector<std::string>& args)
    {
        (void)args;
        ubs_ssu_alloc_result_t* results = nullptr;
        uint32_t cnt = 0;
        int32_t ret = ubs_ssu_alloc_info_list(&results, &cnt);
        if (ret == UBS_SUCCESS) {
            std::vector<std::string> arr;
            for (uint32_t i = 0; i < cnt; ++i) {
                arr.push_back(AllocResultToJson(results[i]));
            }
            EmitResponse(JArray(arr));
            ubs_ssu_alloc_info_list_free(&results, cnt);
        } else {
            EmitError(ret);
        }
    }

    // ssu_get_ns_stats <name>
    void DoNsStatsGet(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_get_ns_stats");
        parser.AddPositional("name");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_ns_stats_t* list = nullptr;
        uint32_t cnt = 0;
        int32_t ret = ubs_ssu_ns_stats_get(parser.Get("name").c_str(), &list, &cnt);
        if (ret == UBS_SUCCESS) {
            std::vector<std::string> arr;
            for (uint32_t i = 0; i < cnt; ++i) {
                arr.push_back(NsStatsToJson(list[i]));
            }
            EmitResponse(JArray(arr));
            ubs_ssu_ns_stats_free(&list);
        } else {
            EmitError(ret);
        }
    }

    // ssu_get_connect_info <name> <vfe_present> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>
    // vfe_present=0: 传 nil; vfe_present=1: 用后续字段组装非空 VFE (即使全零)
    void DoConnectInfoGet(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_get_connect_info");
        parser.AddPositional("name");
        parser.AddPositional("vfe_present");
        parser.AddPositional("slot_id");
        parser.AddPositional("chip_id");
        parser.AddPositional("die_id");
        parser.AddPositional("pfe_id");
        parser.AddPositional("vfe_id");
        parser.AddPositional("vfe_guid");
        parser.AddPositional("bind_bus_instance_guid");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }

        ubs_ub_vfe_t vfe;
        ubs_ub_vfe_t* vfePtr = nullptr;
        if (parser.GetUint32("vfe_present") != 0) {
            FillVfeFromParser(vfe, parser);
            vfePtr = &vfe;
        }

        if (!CheckFormatOrReport(parser)) {
            return;
        }

        ubs_ssu_connect_info_t* list = nullptr;
        uint32_t cnt = 0;
        int32_t ret = ubs_ssu_connect_info_get(parser.Get("name").c_str(), vfePtr, &list, &cnt);
        if (ret == UBS_SUCCESS) {
            std::vector<std::string> arr;
            for (uint32_t i = 0; i < cnt; ++i) {
                arr.push_back(ConnectInfoToJson(list[i]));
            }
            EmitResponse(JArray(arr));
            ubs_ssu_connect_info_free(&list);
        } else {
            EmitError(ret);
        }
    }

    // ssu_add_access_permission <name> <nqn>
    void DoAccessPermissionAdd(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_add_access_permission");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        int32_t ret = ubs_ssu_access_permission_add(parser.Get("name").c_str(), parser.Get("nqn").c_str());
        if (ret == UBS_SUCCESS) {
            EmitSuccess();
        } else {
            EmitError(ret);
        }
    }

    // ssu_remove_access_permission <name> <nqn>
    void DoAccessPermissionRemove(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_remove_access_permission");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        int32_t ret = ubs_ssu_access_permission_remove(parser.Get("name").c_str(), parser.Get("nqn").c_str());
        EmitSimpleResult(ret);
    }

    // ssu_attach_space <name> <nqn> <src_eid>
    void DoSpaceAttach(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_attach_space");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        parser.AddPositional("src_eid");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_space_req_t req;
        FillSpaceReq(req, parser);

        char** nsDevPaths = nullptr;
        uint32_t nsDevPathCnt = 0;
        int32_t ret = ubs_ssu_space_attach(&req, &nsDevPaths, &nsDevPathCnt);
        // UBS_ENGINE_ERR_ALREADY_ATTACHED 时服务端依然返回已挂载的设备路径, 需输出并释放
        if (ret == UBS_SUCCESS || ret == UBS_ENGINE_ERR_ALREADY_ATTACHED) {
            EmitResponse(NsDevPathsToJson(nsDevPaths, nsDevPathCnt));
            ubs_ssu_ns_dev_paths_free(&nsDevPaths, nsDevPathCnt);
        } else {
            EmitError(ret);
        }
    }

    // ssu_detach_space <name> <nqn> <src_eid>
    void DoSpaceDetach(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_detach_space");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        parser.AddPositional("src_eid");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_space_req_t req;
        FillSpaceReq(req, parser);

        int32_t ret = ubs_ssu_space_detach(&req);
        EmitSimpleResult(ret);
    }

    // ssu_attach_linear_space <name> <nqn> <src_eid> <dev_name>
    void DoLinearSpaceAttach(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_attach_linear_space");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        parser.AddPositional("src_eid");
        parser.AddPositional("dev_name");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_linear_space_req_t req;
        FillLinearSpaceReq(req, parser);

        char** nsDevPaths = nullptr;
        uint32_t nsDevPathCnt = 0;
        char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {0};
        int32_t ret = ubs_ssu_linear_space_attach(&req, &nsDevPaths, &nsDevPathCnt, devPath);
        if (ret == UBS_SUCCESS || ret == UBS_ENGINE_ERR_ALREADY_ATTACHED) {
            // 两个业务返回值: [ns_dev_paths 数组, dev_path 字符串]
            EmitResponse(JArray({NsDevPathsToJson(nsDevPaths, nsDevPathCnt), JStr(devPath)}));
            ubs_ssu_ns_dev_paths_free(&nsDevPaths, nsDevPathCnt);
        } else {
            EmitError(ret);
        }
    }

    // ssu_detach_linear_space <name> <nqn> <src_eid> <dev_name>
    void DoLinearSpaceDetach(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_detach_linear_space");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        parser.AddPositional("src_eid");
        parser.AddPositional("dev_name");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_linear_space_req_t req;
        FillLinearSpaceReq(req, parser);

        int32_t ret = ubs_ssu_linear_space_detach(&req);
        EmitSimpleResult(ret);
    }

    // ssu_attach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>
    void DoStripedSpaceAttach(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_attach_striped_space");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        parser.AddPositional("src_eid");
        parser.AddPositional("dev_name");
        parser.AddPositional("level");
        parser.AddPositional("chunk_size");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_striped_space_req_t req;
        std::memset(&req, 0, sizeof(req));
        CopyToFixed(req.name, UBS_SSU_MAX_NAME_LENGTH, parser.Get("name"));
        CopyToFixed(req.nqn, UBS_SSU_MAX_NQN_LENGTH, parser.Get("nqn"));
        CopyToFixed(req.src_eid, UBS_SSU_MAX_EID_LENGTH, parser.Get("src_eid"));
        CopyToFixed(req.dev_name, UBS_SSU_MAX_DEV_NAME_LENGTH, parser.Get("dev_name"));
        req.level = static_cast<ubs_ssu_raid_level_t>(parser.GetUint32("level"));
        req.chunk_size = static_cast<ubs_ssu_chunk_size_t>(parser.GetUint32("chunk_size"));

        if (!CheckFormatOrReport(parser)) {
            return;
        }

        char** nsDevPaths = nullptr;
        uint32_t nsDevPathCnt = 0;
        char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {0};
        int32_t ret = ubs_ssu_striped_space_attach(&req, &nsDevPaths, &nsDevPathCnt, devPath);
        if (ret == UBS_SUCCESS || ret == UBS_ENGINE_ERR_ALREADY_ATTACHED) {
            EmitResponse(JArray({NsDevPathsToJson(nsDevPaths, nsDevPathCnt), JStr(devPath)}));
            ubs_ssu_ns_dev_paths_free(&nsDevPaths, nsDevPathCnt);
        } else {
            EmitError(ret);
        }
    }

    // ssu_detach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>
    void DoStripedSpaceDetach(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_detach_striped_space");
        parser.AddPositional("name");
        parser.AddPositional("nqn");
        parser.AddPositional("src_eid");
        parser.AddPositional("dev_name");
        parser.AddPositional("level");
        parser.AddPositional("chunk_size");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }
        ubs_ssu_striped_space_req_t req;
        FillStripedSpaceReq(req, parser);

        if (!CheckFormatOrReport(parser)) {
            return;
        }

        int32_t ret = ubs_ssu_striped_space_detach(&req);
        EmitSimpleResult(ret);
    }

    // ssu_get_fe_device_list
    void DoFeDeviceList(const std::vector<std::string>& args)
    {
        (void)args;
        ubs_ub_fe_t* list = nullptr;
        uint32_t cnt = 0;
        int32_t ret = ubs_ssu_fe_device_list(&list, &cnt);
        if (ret == UBS_SUCCESS) {
            std::vector<std::string> arr;
            for (uint32_t i = 0; i < cnt; ++i) {
                arr.push_back(FeToJson(list[i]));
            }
            EmitResponse(JArray(arr));
            ubs_ssu_fe_device_list_free(&list, cnt);
        } else {
            EmitError(ret);
        }
    }

    // ssu_fe_device_alloc <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>
    void DoFeDeviceAlloc(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_fe_device_alloc");
        parser.AddPositional("upi");
        parser.AddPositional("slot_id");
        parser.AddPositional("chip_id");
        parser.AddPositional("die_id");
        parser.AddPositional("pfe_id");
        parser.AddPositional("vfe_id");
        parser.AddPositional("vfe_guid");
        parser.AddPositional("bind_bus_instance_guid");
        parser.AddPositional("bus_instance_guid");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }

        ubs_ub_vfe_t vfe;
        FillVfeFromParser(vfe, parser);

        uint8_t busInstanceGuid[UBS_SSU_GUID_LENGTH] = {0};
        FillGuid(busInstanceGuid, parser.Get("bus_instance_guid"));

        uint32_t upi = parser.GetUint32("upi");
        if (!CheckFormatOrReport(parser)) {
            return;
        }

        int32_t ret = ubs_ssu_fe_device_alloc(upi, &vfe, busInstanceGuid);
        if (ret == UBS_SUCCESS) {
            EmitResponse(GuidToJson(busInstanceGuid));
        } else {
            EmitError(ret);
        }
    }

    // ssu_fe_device_free <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>
    void DoFeDeviceFree(const std::vector<std::string>& args)
    {
        ArgParser parser("ssu_fe_device_free");
        parser.AddPositional("upi");
        parser.AddPositional("slot_id");
        parser.AddPositional("chip_id");
        parser.AddPositional("die_id");
        parser.AddPositional("pfe_id");
        parser.AddPositional("vfe_id");
        parser.AddPositional("vfe_guid");
        parser.AddPositional("bind_bus_instance_guid");
        std::string errMsg;
        if (!ParseArgsOrReport(parser, args, errMsg)) {
            return;
        }

        ubs_ub_vfe_t vfe;
        FillVfeFromParser(vfe, parser);

        uint32_t upi = parser.GetUint32("upi");
        if (!CheckFormatOrReport(parser)) {
            return;
        }

        int32_t ret = ubs_ssu_fe_device_free(upi, &vfe);
        EmitSimpleResult(ret);
    }
};

// 程序入口
// 用法:
//   1) 交互式 REPL (无命令行参数):
//        ./ubse_ssu_test_c
//        ubse_ssu_test_c> ssu_list_alloc_info
//        ubse_ssu_test_c> quit
//   2) 单次执行模式 (命令跟在程序名后, 执行完即退出):
//        ./ubse_ssu_test_c ssu_list_alloc_info
//        ./ubse_ssu_test_c ssu_alloc_space test-space 1073741824 2 4096 0 tenant-a
//        echo $?  # 退出码: 0=成功, 1=未知命令/参数错误/SDK失败, 2=quit等控制命令
// 注: argv 中的参数按空白分隔, 含空格的参数可用双引号包裹;
//     空字符串用 "" 占位, 数值零用 0 占位, 枚举值填 SDK 底层十进制数值
int main(int argc, char* argv[])
{
    UbseSsuApp app;
    if (argc > 1) {
        return app.ExecuteOnce(argc, argv);
    }
    app.Run();
    return 0;
}
