/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
*/

#include "ubse_conf_manager.h"

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <regex>
#include <sstream>

#include "ubse_conf_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_audit.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"

namespace ubse::config {
using namespace ubse::log;
using namespace ubse::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

const uint8_t SUFFIX_SIZE = 5; // .conf后缀长度
const std::string DELIMITER = "/";
const std::regex NON_VAL_CHARS(R"(^[a-zA-Z0-9\.\_\-]+$)");
const std::regex VAL_CHARS(R"(^[a-zA-Z0-9\.\_\-\:\,\/\;]+$)");
const uint32_t MAX_GROUP_SIZE = 64 * 64 + 64; // hostname最大长度64字节，最多支持64个节点，包含64个间隔符
const uint32_t MAX_PROVIDER_SIZE = 64 * 64 + 64; // hostname最大长度64字节，最多支持64个节点，包含64个间隔符
const std::map<std::string, uint32_t> whiteList = {{"group", MAX_GROUP_SIZE}, {"provider", MAX_PROVIDER_SIZE}};

UbseResult TravelDepthLimitedFiles(std::vector<std::string>& filePaths, const std::string& path, int depth);

std::string PathJoin(const std::string& baseDir, const std::string& baseName); // 获取路径

bool IsConfFile(const std::string& filename); // 是否conf文件

std::string CatString(const std::vector<std::string>& infoVec, const std::string& delimiter = " ");

UbseResult CheckParamValidation(const std::string& section, const std::string& configKey, const std::string& configVal,
                                bool checkValue = false);

bool CheckNoIllegalChars(const std::string& str, bool isConfigVal = false);

std::string FormatErrorMessage(const std::string& message, size_t lineCount, const std::string& section = "",
                               const std::string& configKey = "", const std::string& configVal = "");

UbseConfigManager::UbseConfigManager() : format_() {}

UbseResult UbseConfigManager::Init(const std::string& confDir, const std::string& filePrefix)
{
    std::vector<std::string> filePaths;
    UbseResult ret = TravelDepthLimitedFiles(filePaths, confDir, 0);
    if (ret != UBSE_OK) {
        return ret;
    }

    std::unique_lock<std::shared_mutex> guard(rwLock_);
    for (const auto& filePath : filePaths) {
        // 文件已加载, 不可重复加载
        if (fileSet_.count(filePath)) {
            std::cerr << "Warning: File=" << filePath << "has already been loaded." << std::endl;
            continue;
        }
        // 文件前缀非空且与文件名不匹配
        if (!filePrefix.empty() && filePath.substr(filePath.rfind('/') + 1).find(filePrefix) != 0) {
            continue;
        }
        ret = ParseFile(filePath);
        // 解析文件失败
        if (ret != UBSE_OK) {
            std::cerr << "Warning: Unable to parse file=" << filePath << "." << std::endl;
        } else {
            fileSet_.emplace(filePath);
        }
    }
    if (!parseErrors_.empty()) {
        std::cerr << "Unable to parse the configuration." << std::endl;
        for (const auto& pair : parseErrors_) {
            std::cerr << "fileName=" << pair.first << ",warnings:\n" << CatString(pair.second, "\n") << std::endl;
        }
    }
    parseErrors_.clear();
    return ret;
}

UbseResult UbseConfigManager::ParseFile(const std::string& filePath)
{
    char* canonicalPath = new (std::nothrow) char[PATH_MAX];
    if (canonicalPath == nullptr) {
        std::cerr << "Warning: Memory allocation failed for canonicalPath" << std::endl;
        return UBSE_CONF_ERROR_KEY_OFFSETMEMORY_ALLOCATION_FAILED;
    }
    if (realpath(filePath.c_str(), canonicalPath) == nullptr) {
        std::cerr << "Warning: Could not canonicalize file path " << filePath << " ,err=" << std::strerror(errno)
                  << std::endl;
        delete[] canonicalPath;
        return UBSE_CONF_ERROR_KEY_OFFSETPATH_CANONICALIZATION_FAILED;
    }
    delete[] canonicalPath;
    return ReadConfFile(filePath);
}

UbseResult UbseConfigManager::ReadConfFile(const std::string& filePath)
{
    std::ifstream fileStream(filePath);
    // 文件无法打开
    if (!fileStream.is_open()) {
        std::cerr << "Warning: Can not open file=" << filePath << " to read." << std::endl;
        return UBSE_CONF_ERROR_KEY_OFFSETFILE_OPEN_ERROR;
    }
    std::string defaultSection = filePath.substr(filePath.find_last_of("/\\") + 1,
                                                 filePath.size() - filePath.find_last_of("/\\") - 1 - SUFFIX_SIZE);
    std::string tempSection = Trim(defaultSection);

    // 逐行读取
    std::string line; // 存储当前行内容
    size_t lineCount = 1;
    while (std::getline(fileStream, line) && (lineCount <= (CONFIG_MAX_LINES + 1))) {
        ParseLine(filePath, line, lineCount++, tempSection);
    }
    fileStream.close();
    return UBSE_OK;
}

void UbseConfigManager::ParseLine(const std::string& filePath, std::string line, const size_t& lineCount,
                                  std::string& tempSection)
{
    if (lineCount > CONFIG_MAX_LINES) {
        parseErrors_[filePath].emplace_back("Warning: Maximum line count exceeded.");
        return;
    }

    line = Trim(line);
    size_t equalPos = line.find('=');
    // 处理注释
    if (line.empty() || format_.IsComment(std::string(1, line.front()))) {
        return;
    }
    if (format_.IsSectionStart(std::string(1, line.front())) && format_.IsSectionEnd(std::string(1, line.back()))) {
        ParseSection(filePath, line, lineCount, tempSection);
    } else if (equalPos != std::string::npos && equalPos > 0 && equalPos < line.size() - 1) {
        ParseConf(filePath, line, lineCount, tempSection);
    } else {
        parseErrors_[filePath].emplace_back("Warning: Invalid line content. Line=" + std::to_string(lineCount) + ".");
    }
}

void UbseConfigManager::ParseSection(const std::string& filePath, const std::string& line, const size_t& lineCount,
                                     std::string& tempSection)
{
    std::string section = line;
    section.erase(section.length() - 1, 1);
    section.erase(0, 1);
    section = Trim(section, std::locale{"C"});
    // 长度不合法
    if (section.size() < CONFIG_MIN_FIELD_LENGTH || section.size() > CONFIG_SECTION_MAX_FIELD_LENGTH) {
        std::string message = "Warning: The length of section is out of range( " +
                              std::to_string(CONFIG_MIN_FIELD_LENGTH) + " to " +
                              std::to_string(CONFIG_SECTION_MAX_FIELD_LENGTH) + ").";
        parseErrors_[filePath].emplace_back(FormatErrorMessage(message, lineCount, section));
        return;
    }
    // 含有非法字符
    if (!CheckNoIllegalChars(section)) {
        std::string message = "Warning: Section has illegal character.";
        parseErrors_[filePath].emplace_back(FormatErrorMessage(message, lineCount, section));
        return;
    }
    tempSection = section;
    // 首次遇到该section
    if (configMap_.find(tempSection) == configMap_.end()) {
        configMap_[tempSection];
    }
}

void UbseConfigManager::ParseConf(const std::string& filePath, const std::string& line, const size_t& lineCount,
                                  std::string& tempSection)
{
    size_t pos = line.find('=');
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    key = Trim(key);
    value = Trim(value);
    // 长度不合法
    if (key.size() > CONFIG_KEY_MAX_FIELD_LENGTH || key.size() < CONFIG_MIN_FIELD_LENGTH ||
        value.size() > CONFIG_VALUE_MAX_FIELD_LENGTH) {
        bool flag = false;
        auto it = whiteList.find(key);
        flag = (it == whiteList.end()) ? true : (value.size() >= it->second);
        if (flag) {
            std::string message =
                "Warning: The length of key is out of range(key=" + std::to_string(CONFIG_MIN_FIELD_LENGTH) + " to " +
                std::to_string(CONFIG_KEY_MAX_FIELD_LENGTH) + " ,value=" + std::to_string(CONFIG_MIN_FIELD_LENGTH) +
                " to " + std::to_string(CONFIG_VALUE_MAX_FIELD_LENGTH) + ").";
            parseErrors_[filePath].emplace_back(FormatErrorMessage(message, lineCount, "", key, value));
            return;
        }
    }
    // 含有非法字符
    if (!CheckNoIllegalChars(key)) {
        std::string message = "Warning: Section's key has illegal character.";
        parseErrors_[filePath].emplace_back(FormatErrorMessage(message, lineCount, tempSection, key));
        return;
    }
    if (!CheckNoIllegalChars(value, true)) {
        std::string message = "Warning: The configuration value contains illegal chars.";
        parseErrors_[filePath].emplace_back(FormatErrorMessage(message, lineCount, tempSection, key, value));
        return;
    }
    // 重复配置项
    if (configMap_[tempSection].find(key) != configMap_[tempSection].end()) {
        std::string message = "Warning: Duplicate key in section.";
        parseErrors_[filePath].emplace_back(FormatErrorMessage(message, lineCount, tempSection, key));
        return;
    }
    configMap_[tempSection][key] = value;
}

UbseResult UbseConfigManager::Start()
{
    return UBSE_OK;
}

void UbseConfigManager::Stop()
{
    std::unique_lock<std::shared_mutex> guard(rwLock_);
    parseErrors_.clear();
    configMap_.clear();
    fileSet_.clear();
}

UbseResult UbseConfigManager::GetConf(const std::string& section, const std::string& configKey, std::string& configVal)
{
    UbseResult ret = CheckParamValidation(section, configKey, configVal, false);
    if (ret != UBSE_OK) {
        return ret;
    }

    // 不存在该section
    if (configMap_.find(section) == configMap_.end()) {
        UBSE_LOG_WARN << "Unable to find section=" << section << ", "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_SECTION);
        return UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_SECTION;
    }

    // section中不存在该key
    if (configMap_[section].find(configKey) == configMap_[section].end()) {
        UBSE_LOG_WARN << "Unable to find key=" << configKey << " in section=" << section << ", "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_KEY);
        return UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_KEY;
    }
    configVal = configMap_[section][configKey];
    return UBSE_OK;
}

UbseResult UbseConfigManager::GetAllConf(const std::string& seactionPrefix,
                                         std::map<std::string, std::map<std::string, std::string>>& configVals)
{
    if (seactionPrefix.size() > CONFIG_KEY_MAX_FIELD_LENGTH) {
        UBSE_LOG_WARN << "Prefix too long, " << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETPREFIX_TOO_LONG);
        return UBSE_CONF_ERROR_KEY_OFFSETPREFIX_TOO_LONG;
    }
    if (!CheckNoIllegalChars(seactionPrefix)) {
        UBSE_LOG_WARN << "Prefix has illegal character, "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETPREFIX_ILLEGAL_CHAR);
        return UBSE_CONF_ERROR_KEY_OFFSETPREFIX_ILLEGAL_CHAR;
    }

    configVals.clear();
    if (configMap_.empty()) {
        UBSE_LOG_WARN << "Config Module has not been loaded, "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL);
        return UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL;
    }

    size_t configNum = 0;
    for (const auto& pair : configMap_) {
        // 前缀不符
        if (pair.first.find(seactionPrefix) != 0) {
            continue;
        }

        configVals[pair.first]; // 适配空section
        for (const auto& config : pair.second) {
            configVals[pair.first][config.first] = config.second;
            configNum++;
        }
    }

    if (configVals.empty()) {
        UBSE_LOG_INFO << "No configuration items in the section with prefix " << seactionPrefix << ".";
        return UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_PREFIX;
    }
    if (configNum == 0) {
        UBSE_LOG_INFO << "No configuration items in the section with prefix " << seactionPrefix << ".";
        return UBSE_CONF_ERROR_KEY_OFFSETCONFIG_PREFIX_NO_CONTENT;
    }

    return UBSE_OK;
}

void UbseConfigManager::AddConfig(const std::string& section, const std::string& key, const std::string& value)
{
    if (section.size() < CONFIG_MIN_FIELD_LENGTH || section.size() > CONFIG_SECTION_MAX_FIELD_LENGTH) {
        return;
    }

    if (key.size() > CONFIG_KEY_MAX_FIELD_LENGTH || key.size() < CONFIG_MIN_FIELD_LENGTH || value.empty() ||
        value.size() > CONFIG_VALUE_MAX_FIELD_LENGTH) {
        return;
    }

    if (!CheckNoIllegalChars(section) || !CheckNoIllegalChars(key) || !CheckNoIllegalChars(value, true)) {
        return;
    }

    auto it_section = configMap_.find(section);
    if (it_section == configMap_.end()) {
        configMap_[section][key] = value;
        return;
    }
    auto it_key = it_section->second.find(key);
    if (it_key == it_section->second.end()) {
        it_section->second[key] = value;
    }
}

UbseResult CheckParamValidation(const std::string& section, const std::string& configKey, const std::string& configVal,
                                bool checkValue)
{
    // 非法字符检查
    if (!CheckNoIllegalChars(section)) {
        UBSE_LOG_WARN << "Section has invalid character, "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETSECTION_HAVE_ILLEGAL_CHAR);
        return UBSE_CONF_ERROR_KEY_OFFSETSECTION_HAVE_ILLEGAL_CHAR;
    }
    if (!CheckNoIllegalChars(configKey)) {
        UBSE_LOG_WARN << "Key has invalid character, "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETKEY_HAVE_ILLEGAL_CHAR);
        return UBSE_CONF_ERROR_KEY_OFFSETKEY_HAVE_ILLEGAL_CHAR;
    }
    if (!CheckNoIllegalChars(configVal, true) && checkValue) {
        UBSE_LOG_WARN << "The configuration value contains illegal chars, "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETVALUE_HAVE_ILLEGAL_CHAR);
        return UBSE_CONF_ERROR_KEY_OFFSETVALUE_HAVE_ILLEGAL_CHAR;
    }

    // 长度检查
    if (section.size() > CONFIG_SECTION_MAX_FIELD_LENGTH || section.size() < CONFIG_MIN_FIELD_LENGTH) {
        UBSE_LOG_WARN << "Section length invalid, " << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETSECTION_ILLEGAL_LENGTH);
        return UBSE_CONF_ERROR_KEY_OFFSETSECTION_ILLEGAL_LENGTH;
    }
    if (configKey.size() > CONFIG_KEY_MAX_FIELD_LENGTH || section.size() < CONFIG_MIN_FIELD_LENGTH) {
        UBSE_LOG_WARN << "Key length invalid, " << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETKEY_ILLEGAL_LENGTH);
        return UBSE_CONF_ERROR_KEY_OFFSETKEY_ILLEGAL_LENGTH;
    }
    if ((configVal.empty() || configVal.size() > CONFIG_VALUE_MAX_FIELD_LENGTH) && checkValue) {
        bool flag = false;
        auto it = whiteList.find(configKey);
        flag = (it == whiteList.end()) ? true : (configVal.size() >= it->second);
        if (flag) {
            UBSE_LOG_WARN << "Value length too long or too short, "
                          << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETVALUE_ILLEGAL_LENGTH);
            return UBSE_CONF_ERROR_KEY_OFFSETVALUE_ILLEGAL_LENGTH;
        }
    }
    return UBSE_OK;
}

std::string CatString(const std::vector<std::string>& infoVec, const std::string& delimiter)
{
    if (infoVec.empty()) {
        return "";
    }
    std::string outputStr;
    auto it = infoVec.begin();
    outputStr += *it;
    it++;
    for (; it != infoVec.end(); it++) {
        outputStr += delimiter + (*it);
    }
    return std::move(outputStr);
}

bool CheckNoIllegalChars(const std::string& str, bool isConfigVal)
{
    const std::regex& legalChars = isConfigVal ? VAL_CHARS : NON_VAL_CHARS;

    if (str.empty()) {
        return true;
    }
    return std::regex_match(str, legalChars);
}

UbseResult TravelDepthLimitedFiles(std::vector<std::string>& filePaths, const std::string& path, int depth)
{
    if (depth > CONFIG_DIR_MAX_DEPTH) {
        return UBSE_CONF_ERROR_KEY_OFFSETDIR_TOO_DEEP;
    }

    DIR* pd = opendir(path.c_str());
    if (pd == nullptr) {
        UBSE_LOG_WARN << "Unable to open dir " << path << ", "
                      << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETDIR_OPEN_ERROR);
        return UBSE_CONF_ERROR_KEY_OFFSETDIR_OPEN_ERROR;
    }
    const dirent* dir;
    struct stat statBuf {};
    while ((dir = readdir(pd)) != nullptr) {
        std::string dName = dir->d_name;
        if (dName == "." || dName == "..") {
            continue;
        }
        std::string subFile = PathJoin(path, dName);
        if (lstat(subFile.c_str(), &statBuf)) {
            UBSE_LOG_WARN << "Unable to get file status " << subFile << ".";
            continue;
        }

        if (S_ISDIR(statBuf.st_mode)) {
            UbseResult ret = TravelDepthLimitedFiles(filePaths, subFile, depth + 1);
            if (ret != UBSE_OK) {
                closedir(pd);
                return ret;
            }
        } else if (S_ISREG(statBuf.st_mode) && IsConfFile(dName)) {
            filePaths.emplace_back(subFile);
        }
    }
    closedir(pd);
    return UBSE_OK;
}

std::string PathJoin(const std::string& baseDir, const std::string& baseName)
{
    if (baseDir.empty()) {
        return baseName;
    }

    const char delimiter = '/';
    std::string result = baseDir;
    if (result.back() != delimiter) {
        result.append(1, delimiter);
    }
    result += baseName;
    return result;
}

std::string FormatErrorMessage(const std::string& message, size_t lineCount, const std::string& section,
                               const std::string& configKey, const std::string& configVal)
{
    std::ostringstream oss;
    oss << message << " Line=" << std::to_string(lineCount) << ".";
    if (!section.empty()) {
        oss << " Section=" << section << ".";
    }
    if (!configKey.empty()) {
        oss << " Key=" << configKey << ".";
    }
    if (!configVal.empty()) {
        oss << " Value=" << configVal << ".";
    }
    return oss.str();
}

bool IsConfFile(const std::string& filename)
{
    return filename.size() > SUFFIX_SIZE && filename.compare(filename.size() - SUFFIX_SIZE, SUFFIX_SIZE, ".conf") == 0;
}
} // namespace ubse::config
