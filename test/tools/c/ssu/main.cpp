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
//   bash build.sh          # 运行时 dlopen 真实 libubse-client.so

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

inline void* LibraryHandle()
{
    static void* handle = [] {
        const char* configured = std::getenv("UBSE_CLIENT_LIBRARY");
        const char* library = configured && *configured ? configured : "libubse-client.so";
        void* loaded = dlopen(library, RTLD_NOW | RTLD_LOCAL);
        if (loaded == nullptr) {
            std::cerr << "ERROR: 无法加载 " << library << ": " << dlerror() << std::endl;
        }
        return loaded;
    }();
    return handle;
}

template<typename T>
T LoadSymbol(const char* name)
{
    void* handle = LibraryHandle();
    if (handle == nullptr) {
        return nullptr;
    }
    dlerror();
    void* symbol = dlsym(handle, name);
    const char* error = dlerror();
    if (error != nullptr) {
        std::cerr << "ERROR: 无法解析符号 " << name << ": " << error << std::endl;
        return nullptr;
    }
    return reinterpret_cast<T>(symbol);
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
                inQuote = true;
                hasToken = true;
            } else {
                inQuote = false;
            }
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
        const std::string& v = Get(name);
        try {
            return static_cast<uint32_t>(std::stoul(v));
        } catch (...) {
            return 0;
        }
    }

    uint64_t GetUint64(const std::string& name) const
    {
        const std::string& v = Get(name);
        try {
            return std::stoull(v);
        } catch (...) {
            return 0;
        }
    }

private:
    std::string cmd_;
    std::vector<std::string> positionals_;
    std::map<std::string, std::string> values_;
};

} // namespace