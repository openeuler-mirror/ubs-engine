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

#ifndef UBSE_CONFIG_MANAGER_H
#define UBSE_CONFIG_MANAGER_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <shared_mutex>

#include "ubse_common_def.h"

namespace ubse::config {
using namespace ubse::common::def;
class UbseConfModule;
class Format {
public:
    const std::string charSectionStart_;  // 配置节首字符 [
    const std::string charSectionEnd_;  // 配置节尾字符 [
    const std::string charAssign_;  // 等号
    const std::string charCommentSemicolon_;  // 参数注释（分号）
    const std::string charCommentHash_;  // 参数注释（警号）

    explicit Format(std::string section_start = "[", std::string section_end = "]", std::string assign = "=",
                    std::string commentSemicolon = ";", std::string commentHash = "#")
        : charSectionStart_(std::move(section_start)),
          charSectionEnd_(std::move(section_end)),
          charAssign_(std::move(assign)),
          charCommentSemicolon_(std::move(commentSemicolon)),
          charCommentHash_(std::move(commentHash))
    {
    }

    [[nodiscard]] inline bool IsSectionStart(const std::string& ch) const
    {
        return ch == charSectionStart_;
    }

    [[nodiscard]] inline bool IsSectionEnd(const std::string& ch) const
    {
        return ch == charSectionEnd_;
    }

    [[nodiscard]] inline bool IsAssign(const std::string& ch) const
    {
        return ch == charAssign_;
    }

    [[nodiscard]] inline bool IsComment(const std::string& ch) const
    {
        return ch == charCommentSemicolon_ || ch == charCommentHash_;
    }
};

using SECTION = std::map<std::string, std::string>;
using ConfigMap = std::map<std::string, SECTION>;

class UbseConfigManager {
public:
    friend UbseConfModule;
    static UbseConfigManager& GetInstance()
    {
        static UbseConfigManager instance;
        return instance;
    }

    UbseConfigManager operator=(const UbseConfigManager&) = delete;

    UbseConfigManager(const UbseConfigManager&) = delete;

    /* *
    * @brief 从配置文件读取配置
    * @brief 对应于模块的初始化和插件动态加载配置阶段
    * @param[in] confDir: 配置文件所在的目录
    * @param[in] filePrefix: 前缀筛选配置文件
    * @return UbseResult, 成功返回0, 失败返回非0
    */
    UbseResult Init(const std::string& confDir, const std::string& filePrefix = "");

    /* *
    * @brief 注册Http和cli
    * @brief 对应于模块的启动阶段
    * @return UbseResult, 成功返回0, 失败返回非0
    */
    UbseResult Start();

    /* *
    * @brief 清理配置数据
    */
    void Stop();

    /* *
    * @brief 读取单条配置
    * @param[in] section: 配置节
    * @param[in] configKey: 配置参数key
    * @param[out] configVal: 配置参数值
    * @return UbseResult, 成功返回0, 失败返回非0
    */
    UbseResult GetConf(const std::string& section, const std::string& configKey, std::string& configVal);

    /* *
    * @brief 根据配置节前缀读取配置
    * @param[in] seactionPrefix: 配置节前缀
    * @param[out] configVals: 配置参数值表
    * @return UbseResult, 成功返回0, 失败返回非0
    */
    UbseResult GetAllConf(const std::string& seactionPrefix,
                          std::map<std::string, std::map<std::string, std::string>>& configVals);

private:
    UbseConfigManager();

    UbseResult ParseFile(const std::string& filePath);  // 解析文件, 校验文件并读入内存

    void ParseLine(const std::string& filePath, std::string line, const size_t& lineCount, std::string& tempSection);

    void ParseSection(const std::string& filePath, const std::string& line, const size_t& lineCount,
                      std::string& tempSection);

    void ParseConf(const std::string& filePath, const std::string& line, const size_t& lineCount,
                   std::string& tempSection);

    UbseResult ReadConfFile(const std::string& filePath);  // 读取文件到内存

    void AddConfig(const std::string &section, const std::string &key, const std::string &value);

    Format format_;
    std::unordered_map<std::string, std::vector<std::string>> parseErrors_;  // 解析错误信息
    std::shared_mutex rwLock_;
    ConfigMap configMap_;  // 全部配置
    std::unordered_set<std::string> fileSet_;
};

}  // namespace ubse::config

#endif  // UBSE_CONFIG_MANAGER_H