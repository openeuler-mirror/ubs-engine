/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * DT Fuzz harness: virt_agent JSON 解析攻击面（配置参数从 JSON 字符串反序列化）
 *
 * 攻击面说明：
 *   CaseConfParam::FromJson() 直接解析来自北向/云端的 JSON 字符串
 *   （case_conf_set 的参数本体），畸形 JSON、超长字符串、深层嵌套、
 *   类型混淆（caseType 传对象/数组、overCommitmentRatio 传字符串）都可能触发解析层缺陷。
 *   同时覆盖 VMJsonUtil 的通用 JSON <-> map/vector 转换（配置解析公共路径）。
 *
 * 输入布局：
 *   byte[0]    目标函数选择器
 *   byte[1..]  JSON 文本
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "vm_json_util.h"
#include "vm_string_util.h"

using namespace vm;

namespace {

volatile uint64_t g_sink = 0;

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) {
        return 0;
    }
    const uint8_t selector = data[0];
    const std::string jsonStr(reinterpret_cast<const char*>(data + 1), size - 1);

    switch (selector % 3) {
        case 0: {
            // CaseConfParam::FromJson 的实际解析路径：
            // rapidjson Document::Parse + VMJsonUtil 取值 + SafeStof 浮点转换
            //（FromJson 本体位于 virtagent 插件库，依赖过重不编入，此处覆盖等价路径）
            Document doc;
            doc.Parse(jsonStr.c_str(), jsonStr.size());
            if (!doc.HasParseError() && doc.IsObject()) {
                std::string caseType;
                (void)VMJsonUtil::GetString(doc, "caseType", caseType);
                g_sink ^= caseType.size();
                // SafeStof：str -> float 边界（"1e999"、"-nan"、超长数字串等）
                try {
                    g_sink ^= static_cast<uint64_t>(VmStringUtil::SafeStof(caseType));
                } catch (const std::exception&) {
                }
                double ratio = 0.0;
                (void)VMJsonUtil::GetNumber(doc, "overCommitment", ratio);
                g_sink ^= static_cast<uint64_t>(ratio);
            }
            break;
        }
        case 1: {
            // 通用 JSON 字符串 -> map（配置解析公共路径）
            JSON_MAP strMap;
            (void)VMJsonUtil::VMConvertJsonStr2Map(jsonStr, strMap);
            g_sink ^= strMap.size();
            break;
        }
        default: {
            // 通用 JSON 字符串 -> vector
            JSON_VEC strVec;
            (void)VMJsonUtil::VMConvertJsonStr2Vec(jsonStr, strVec);
            g_sink ^= strVec.size();
            break;
        }
    }
    return 0;
}

// 语料目录为空时生成种子：合法/畸形/边界 JSON 模板
extern "C" int FtGenSeeds(const char* corpusDir)
{
    static const struct {
        const char* name;
        std::string text;
    } seeds[] = {
        {"j00_valid", "{\"caseType\":\"general\",\"overCommitmentRatio\":1.5}"},
        {"j01_empty_obj", "{}"},
        {"j02_empty_arr", "[]"},
        {"j03_bare", "null"},
        {"j04_truncated", "{\"caseType\":\"gen"},
        {"j05_type_confuse", "{\"caseType\":123,\"overCommitmentRatio\":\"str\"}"},
        {"j06_arr_body", "[{\"caseType\":\"a\"},{\"caseType\":\"b\"}]"},
        {"j07_nested", "{\"a\":{\"b\":{\"c\":{\"d\":[1,2,{\"e\":\"f\"}]}}}}"},
        {"j08_long_str", "{\"caseType\":\"" + std::string(512, 'x') + "\"}"},
        {"j09_unicode_esc", "{\"caseType\":\"\\u4e2d\\u6587\\uD83D\\uDE00\"}"},
        {"j10_dup_keys", "{\"caseType\":\"a\",\"caseType\":\"b\",\"caseType\":\"c\"}"},
        {"j11_depth", std::string(128, '[') + std::string(128, ']')},
        {"j12_numbers", "{\"overCommitmentRatio\":-1e308,\"x\":1e999,\"y\":0.1e-999}"},
    };
    int written = 0;
    for (const auto& s : seeds) {
        const std::string path = std::string(corpusDir) + "/" + s.name;
        FILE* f = fopen(path.c_str(), "wb");
        if (f == nullptr) {
            continue;
        }
        // 首字节为选择器（fuzz 输入布局），其后为 JSON 文本
        const unsigned char sel = 0;
        if (fwrite(&sel, 1, 1, f) == 1 && fwrite(s.text.data(), 1, s.text.size(), f) == s.text.size()) {
            ++written;
        }
        fclose(f);
    }
    return written;
}
