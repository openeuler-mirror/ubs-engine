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

#include "ubse_ssu_vm_cache.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <sstream>
#include "ubse_error.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ssu::controller {

using rapidjson::Document;
using rapidjson::PrettyWriter;
using rapidjson::SizeType;
using rapidjson::StringBuffer;
using rapidjson::Value;

using ubse::utils::UbseJsonUtil;

// 缓存文件版本(兼容性校验)
constexpr uint32_t CACHE_FILE_VERSION = 1;
constexpr const char* KEY_VERSION = "version";
constexpr const char* KEY_LAST_SYNC_TS = "lastSyncTs";
constexpr const char* KEY_ALLOC_LIST = "allocList";
constexpr const char* KEY_CONNECT_LIST = "connectList";

namespace {

// ========== AllocResult 序列化辅助 ==========

// 序列化单个 UbseSsuNameSpaceInfo 到 JSON Value
static void SerializeNsInfo(const UbseSsuNameSpaceInfo& ns, Value& out, Document::AllocatorType& alloc)
{
    out.SetObject();
    out.AddMember("tgtEid", Value(ns.tgtEid.c_str(), alloc), alloc);
    out.AddMember("tgtNqn", Value(ns.tgtNqn.c_str(), alloc), alloc);
    out.AddMember("nsUuid", Value(ns.nsUuid.c_str(), alloc), alloc);
    out.AddMember("namespaceId", Value(ns.namespaceId), alloc);
    out.AddMember("nsDevPath", Value(ns.nsDevPath.c_str(), alloc), alloc);
    out.AddMember("nsSize", Value(ns.nsSize), alloc);
    out.AddMember("lbaFormat", Value(static_cast<uint32_t>(ns.lbaFormat)), alloc);
    Value allowList(rapidjson::kArrayType);
    for (const auto& nqn : ns.allowHostNqnList) {
        allowList.PushBack(Value(nqn.c_str(), alloc), alloc);
    }
    out.AddMember("allowHostNqnList", allowList, alloc);
}

// 反序列化单个 UbseSsuNameSpaceInfo
static bool DeserializeNsInfo(const Value& in, UbseSsuNameSpaceInfo& ns)
{
    if (!in.IsObject()) {
        return false;
    }
    uint32_t lba = 0;
    if (UbseJsonUtil::GetStrFromJsonPtr(in, "tgtEid", ns.tgtEid) != UBSE_OK ||
        UbseJsonUtil::GetStrFromJsonPtr(in, "tgtNqn", ns.tgtNqn) != UBSE_OK ||
        UbseJsonUtil::GetStrFromJsonPtr(in, "nsUuid", ns.nsUuid) != UBSE_OK ||
        UbseJsonUtil::GetUintFromJsonPtr(in, "namespaceId", ns.namespaceId) != UBSE_OK ||
        UbseJsonUtil::GetStrFromJsonPtr(in, "nsDevPath", ns.nsDevPath) != UBSE_OK ||
        UbseJsonUtil::GetUint64FromJsonPtr(in, "nsSize", ns.nsSize) != UBSE_OK ||
        UbseJsonUtil::GetUintFromJsonPtr(in, "lbaFormat", lba) != UBSE_OK) {
        return false;
    }
    if (lba != static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512) &&
        lba != static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K)) {
        return false;
    }
    ns.lbaFormat = static_cast<UbseSsuLBAFormat>(lba);
    auto allowIt = in.FindMember("allowHostNqnList");
    if (allowIt == in.MemberEnd() || !allowIt->value.IsArray()) {
        return false;
    }
    ns.allowHostNqnList.clear();
    for (SizeType i = 0; i < allowIt->value.Size(); ++i) {
        if (!allowIt->value[i].IsString()) {
            return false;
        }
        ns.allowHostNqnList.emplace_back(allowIt->value[i].GetString());
    }
    return true;
}

// 序列化 UbseSsuAllocResult 到 JSON Value
static void SerializeAllocResult(const UbseSsuAllocResult& ar, Value& out, Document::AllocatorType& alloc)
{
    out.SetObject();
    out.AddMember("name", Value(ar.name.c_str(), alloc), alloc);
    out.AddMember("strategy", Value(static_cast<uint32_t>(ar.strategy)), alloc);
    Value nsList(rapidjson::kArrayType);
    for (const auto& ns : ar.nameSpaceList) {
        Value nsVal;
        SerializeNsInfo(ns, nsVal, alloc);
        nsList.PushBack(nsVal, alloc);
    }
    out.AddMember("nameSpaceList", nsList, alloc);
}

// 反序列化 UbseSsuAllocResult
static bool DeserializeAllocResult(const Value& in, UbseSsuAllocResult& ar)
{
    if (!in.IsObject()) {
        return false;
    }
    uint32_t strat = 0;
    if (UbseJsonUtil::GetStrFromJsonPtr(in, "name", ar.name) != UBSE_OK ||
        UbseJsonUtil::GetUintFromJsonPtr(in, "strategy", strat) != UBSE_OK) {
        return false;
    }
    if (strat != static_cast<uint32_t>(UbseSsuAllocStrategy::STRIPED) &&
        strat != static_cast<uint32_t>(UbseSsuAllocStrategy::LINEAR) &&
        strat != static_cast<uint32_t>(UbseSsuAllocStrategy::NORMAL)) {
        return false;
    }
    ar.strategy = static_cast<UbseSsuAllocStrategy>(strat);
    auto nsIt = in.FindMember("nameSpaceList");
    if (nsIt == in.MemberEnd() || !nsIt->value.IsArray()) {
        return false;
    }
    ar.nameSpaceList.clear();
    for (SizeType i = 0; i < nsIt->value.Size(); ++i) {
        UbseSsuNameSpaceInfo ns{};
        if (!DeserializeNsInfo(nsIt->value[i], ns)) {
            return false;
        }
        ar.nameSpaceList.push_back(std::move(ns));
    }
    return true;
}

// 序列化 UbseSsuConnectInfo
static void SerializeConnectInfo(const std::string& name, const UbseSsuConnectInfo& ci, Value& out,
                                 Document::AllocatorType& alloc)
{
    out.SetObject();
    out.AddMember("name", Value(name.c_str(), alloc), alloc);
    out.AddMember("srcEid", Value(ci.srcEid.c_str(), alloc), alloc);
    out.AddMember("tgtEid", Value(ci.tgtEid.c_str(), alloc), alloc);
    out.AddMember("tgtNqn", Value(ci.tgtNqn.c_str(), alloc), alloc);
    out.AddMember("hostNqn", Value(ci.hostNqn.c_str(), alloc), alloc);
    out.AddMember("nsUuid", Value(ci.nsUuid.c_str(), alloc), alloc);
    out.AddMember("nsId", Value(ci.nsId), alloc);
}

// 反序列化 UbseSsuConnectInfo
static bool DeserializeConnectInfo(const Value& in, std::string& name, UbseSsuConnectInfo& ci)
{
    if (!in.IsObject()) {
        return false;
    }
    return UbseJsonUtil::GetStrFromJsonPtr(in, "name", name) == UBSE_OK &&
           UbseJsonUtil::GetStrFromJsonPtr(in, "srcEid", ci.srcEid) == UBSE_OK &&
           UbseJsonUtil::GetStrFromJsonPtr(in, "tgtEid", ci.tgtEid) == UBSE_OK &&
           UbseJsonUtil::GetStrFromJsonPtr(in, "tgtNqn", ci.tgtNqn) == UBSE_OK &&
           UbseJsonUtil::GetStrFromJsonPtr(in, "hostNqn", ci.hostNqn) == UBSE_OK &&
           UbseJsonUtil::GetStrFromJsonPtr(in, "nsUuid", ci.nsUuid) == UBSE_OK &&
           UbseJsonUtil::GetUintFromJsonPtr(in, "nsId", ci.nsId) == UBSE_OK;
}

// 确保缓存文件父目录存在(权限 0700),避免首次部署或目录被清理后落盘必然失败
static bool EnsureCacheDirExist(const std::string& filePath)
{
    const auto sep = filePath.find_last_of('/');
    if (sep == std::string::npos || sep == 0) {
        return true; // 相对路径或根目录
    }
    const std::string dir = filePath.substr(0, sep);
    struct stat st{};
    if (stat(dir.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true;
        }
        UBSE_LOG_ERROR << "cache dir path exists but is not a directory: " << dir;
        return false;
    }
    if (mkdir(dir.c_str(), S_IRWXU) != 0) {
        if (errno == EEXIST) {
            return true; // 并发场景下目录已被其他线程创建
        }
        UBSE_LOG_ERROR << "failed to create cache dir: " << dir << " errno=" << errno;
        return false;
    }
    UBSE_LOG_INFO << "cache dir created: " << dir << " mode=0700";
    return true;
}

} // namespace

// ========== UbseSsuVmCache 实现 ==========

// 推送入口(单条/全量)落盘前的校验:name 非空、strategy 合法、所有 namespace 的 lbaFormat 合法。
// 取值域与 LoadFromDisk 的逐字段严格解码一致,避免非法条目落盘后
// 重启时整个缓存文件被拒绝并删除,导致已缓存分配信息全部丢失
bool UbseSsuVmCache::IsAllocResultPersistable(const UbseSsuAllocResult& allocResult)
{
    if (allocResult.name.empty()) {
        return false;
    }
    if (allocResult.strategy != UbseSsuAllocStrategy::STRIPED &&
        allocResult.strategy != UbseSsuAllocStrategy::LINEAR &&
        allocResult.strategy != UbseSsuAllocStrategy::NORMAL) {
        return false;
    }
    for (const auto& ns : allocResult.nameSpaceList) {
        if (ns.lbaFormat != UbseSsuLBAFormat::LBA_FORMAT_512 && ns.lbaFormat != UbseSsuLBAFormat::LBA_FORMAT_4K) {
            return false;
        }
    }
    return true;
}

void UbseSsuVmCache::SetCacheFilePath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(flushMutex_);
    cacheFilePath_ = path;
}

void UbseSsuVmCache::UpdateAllocInfo(const UbseSsuAllocResult& allocResult)
{
    std::unique_lock<std::shared_mutex> lock(rwLock_);
    allocInfoMap_[allocResult.name] = allocResult;
}

void UbseSsuVmCache::UpdateConnectInfo(const std::string& name, const std::vector<UbseSsuConnectInfo>& connectInfoList)
{
    std::unique_lock<std::shared_mutex> lock(rwLock_);
    connectInfoMap_[name] = connectInfoList;
}

void UbseSsuVmCache::ReplaceSnapshot(
    const std::vector<UbseSsuAllocResult>& allocList,
    const std::vector<std::pair<std::string, std::vector<UbseSsuConnectInfo>>>& connectList)
{
    std::unique_lock<std::shared_mutex> lock(rwLock_);
    allocInfoMap_.clear();
    for (const auto& allocResult : allocList) {
        allocInfoMap_[allocResult.name] = allocResult;
    }
    connectInfoMap_.clear();
    for (const auto& [name, connectInfoList] : connectList) {
        connectInfoMap_[name] = connectInfoList;
    }
    attachedNames_.clear();
}

void UbseSsuVmCache::Clear()
{
    std::unique_lock<std::shared_mutex> lock(rwLock_);
    allocInfoMap_.clear();
    connectInfoMap_.clear();
    attachedNames_.clear();
}

uint32_t UbseSsuVmCache::GetAllocInfo(const std::string& name, UbseSsuAllocResult& result)
{
    std::shared_lock<std::shared_mutex> lock(rwLock_);
    auto it = allocInfoMap_.find(name);
    if (it == allocInfoMap_.end()) {
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }
    result = it->second;
    return UBSE_OK;
}

uint32_t UbseSsuVmCache::ListAllocInfo(std::vector<UbseSsuAllocResult>& result)
{
    std::shared_lock<std::shared_mutex> lock(rwLock_);
    result.clear();
    result.reserve(allocInfoMap_.size());
    for (const auto& [_, val] : allocInfoMap_) {
        result.push_back(val);
    }
    return UBSE_OK;
}

uint32_t UbseSsuVmCache::GetConnectInfo(const std::string& name, std::vector<UbseSsuConnectInfo>& list)
{
    std::shared_lock<std::shared_mutex> lock(rwLock_);
    auto it = connectInfoMap_.find(name);
    if (it == connectInfoMap_.end()) {
        return UBSE_SSU_ERROR_SPACE_NOT_FOUND;
    }
    list = it->second;
    return UBSE_OK;
}

void UbseSsuVmCache::MarkAttached(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(rwLock_);
    attachedNames_.insert(name);
}

void UbseSsuVmCache::MarkDetached(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(rwLock_);
    attachedNames_.erase(name);
}

bool UbseSsuVmCache::IsAttached(const std::string& name)
{
    std::shared_lock<std::shared_mutex> lock(rwLock_);
    return attachedNames_.find(name) != attachedNames_.end();
}

bool UbseSsuVmCache::LoadFromDisk()
{
    std::lock_guard<std::mutex> lock(flushMutex_);
    struct stat st{};
    if (stat(cacheFilePath_.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            UBSE_LOG_INFO << "vm cache file not exist, waiting for full sync: " << cacheFilePath_;
        } else {
            UBSE_LOG_ERROR << "failed to stat vm cache file: " << cacheFilePath_ << " errno=" << errno;
        }
        return false;
    }
    auto rejectCacheFile = [&](const std::string& reason) {
        UBSE_LOG_ERROR << "vm cache file corrupted, will delete: " << cacheFilePath_ << " reason=" << reason;
        if (unlink(cacheFilePath_.c_str()) != 0 && errno != ENOENT) {
            UBSE_LOG_ERROR << "failed to delete corrupted vm cache file: " << cacheFilePath_ << " errno=" << errno;
        }
        return false;
    };

    // 读取文件内容
    std::ifstream ifs(cacheFilePath_);
    if (!ifs.is_open()) {
        UBSE_LOG_ERROR << "failed to open vm cache file: " << cacheFilePath_;
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    if (ifs.bad()) {
        return rejectCacheFile("read failed");
    }
    std::string content = ss.str();
    ifs.close();

    // 解析 JSON
    Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        return rejectCacheFile("invalid JSON, parseErr=" + std::to_string(doc.GetParseError()));
    }

    // 校验顶层必填字段和版本。
    auto verIt = doc.FindMember(KEY_VERSION);
    if (verIt == doc.MemberEnd() || !verIt->value.IsUint() || verIt->value.GetUint() != CACHE_FILE_VERSION) {
        return rejectCacheFile("version mismatch");
    }
    auto syncTsIt = doc.FindMember(KEY_LAST_SYNC_TS);
    auto allocIt = doc.FindMember(KEY_ALLOC_LIST);
    auto connIt = doc.FindMember(KEY_CONNECT_LIST);
    if (syncTsIt == doc.MemberEnd() || !syncTsIt->value.IsUint64() || allocIt == doc.MemberEnd() ||
        !allocIt->value.IsArray() || connIt == doc.MemberEnd() || !connIt->value.IsArray()) {
        return rejectCacheFile("required field missing or has invalid type");
    }

    // 先在临时容器中完成严格解析,避免失败时暴露部分缓存。
    std::unordered_map<std::string, UbseSsuAllocResult> allocInfoMap;
    std::unordered_map<std::string, std::vector<UbseSsuConnectInfo>> connectInfoMap;
    for (const auto& value : allocIt->value.GetArray()) {
        UbseSsuAllocResult ar{};
        if (!DeserializeAllocResult(value, ar) || ar.name.empty()) {
            return rejectCacheFile("invalid allocList entry");
        }
        std::string name = ar.name;
        if (!allocInfoMap.emplace(std::move(name), std::move(ar)).second) {
            return rejectCacheFile("duplicate allocList name");
        }
    }
    for (const auto& value : connIt->value.GetArray()) {
        std::string name;
        UbseSsuConnectInfo ci{};
        if (!DeserializeConnectInfo(value, name, ci) || name.empty()) {
            return rejectCacheFile("invalid connectList entry");
        }
        connectInfoMap[name].push_back(std::move(ci));
    }

    // 全部校验成功后一次性替换缓存;attachedNames_ 重置为空。
    {
        std::unique_lock<std::shared_mutex> rwLock(rwLock_);
        allocInfoMap_ = std::move(allocInfoMap);
        connectInfoMap_ = std::move(connectInfoMap);
        attachedNames_.clear();
        UBSE_LOG_INFO << "LoadFromDisk success: alloc=" << allocInfoMap_.size()
                      << " connect=" << connectInfoMap_.size();
    }
    return true;
}

std::string UbseSsuVmCache::SerializeSnapshot(size_t& allocSize, size_t& connectSize)
{
    std::shared_lock<std::shared_mutex> rwLock(rwLock_);
    allocSize = allocInfoMap_.size();
    connectSize = connectInfoMap_.size();
    Document doc;
    auto& alloc = doc.GetAllocator();
    doc.SetObject();
    doc.AddMember(Value(KEY_VERSION, alloc), Value(CACHE_FILE_VERSION), alloc);
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto lastSyncTs = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    doc.AddMember(Value(KEY_LAST_SYNC_TS, alloc), Value(static_cast<uint64_t>(lastSyncTs)), alloc);
    Value allocList(rapidjson::kArrayType);
    for (const auto& [_, ar] : allocInfoMap_) {
        Value arVal;
        SerializeAllocResult(ar, arVal, alloc);
        allocList.PushBack(arVal, alloc);
    }
    doc.AddMember(Value(KEY_ALLOC_LIST, alloc), allocList, alloc);
    Value connectList(rapidjson::kArrayType);
    for (const auto& [name, list] : connectInfoMap_) {
        for (const auto& ci : list) {
            Value ciVal;
            SerializeConnectInfo(name, ci, ciVal, alloc);
            connectList.PushBack(ciVal, alloc);
        }
    }
    doc.AddMember(Value(KEY_CONNECT_LIST, alloc), connectList, alloc);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

bool UbseSsuVmCache::WriteCacheFileAtomically(const std::string& content)
{
    if (!EnsureCacheDirExist(cacheFilePath_)) {
        return false;
    }
    std::string tmpPath = cacheFilePath_ + ".tmp";
    int fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        UBSE_LOG_ERROR << "failed to open tmp cache file: " << tmpPath << " errno=" << errno;
        return false;
    }
    // O_CREAT 的 mode 对已存在文件不生效。写入前强制收紧权限，避免遗留 .tmp 文件暴露敏感缓存内容。
    if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
        UBSE_LOG_ERROR << "chmod tmp cache file failed, errno=" << errno;
        close(fd);
        unlink(tmpPath.c_str());
        return false;
    }

    ssize_t total = 0;
    while (static_cast<size_t>(total) < content.size()) {
        ssize_t n = write(fd, content.data() + total, content.size() - total);
        if (n < 0 && errno == EINTR) {
            // 被信号中断,重试本次写入
            continue;
        }
        if (n <= 0) {
            UBSE_LOG_ERROR << "write cache file failed, errno=" << errno;
            close(fd);
            unlink(tmpPath.c_str());
            return false;
        }
        total += n;
    }
    if (fsync(fd) != 0) {
        UBSE_LOG_ERROR << "fsync cache file failed, errno=" << errno;
        close(fd);
        unlink(tmpPath.c_str());
        return false;
    }
    if (close(fd) != 0) {
        UBSE_LOG_ERROR << "close cache file failed, errno=" << errno;
        unlink(tmpPath.c_str());
        return false;
    }

    if (rename(tmpPath.c_str(), cacheFilePath_.c_str()) != 0) {
        UBSE_LOG_ERROR << "rename cache file failed, errno=" << errno;
        unlink(tmpPath.c_str());
        return false;
    }
    // rename 修改的是父目录项;同步父目录后才能保证正式文件名在掉电后仍然存在。
    // 注意:rename 已成功即代表数据与正式文件名已持久化,后续父目录同步失败仅降低
    // 掉电可靠性,不构成落盘失败,故降级为告警日志并返回 true,避免调用方误报。
    const auto separator = cacheFilePath_.find_last_of('/');
    const std::string parentDir =
        separator == std::string::npos ? "." : (separator == 0 ? "/" : cacheFilePath_.substr(0, separator));
    int dirFd = open(parentDir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dirFd < 0) {
        UBSE_LOG_WARN << "failed to open cache parent directory: " << parentDir << " errno=" << errno;
        return true;
    }
    if (fsync(dirFd) != 0) {
        UBSE_LOG_WARN << "fsync cache parent directory failed: " << parentDir << " errno=" << errno;
        close(dirFd);
        return true;
    }
    if (close(dirFd) != 0) {
        UBSE_LOG_WARN << "close cache parent directory failed: " << parentDir << " errno=" << errno;
    }
    return true;
}

bool UbseSsuVmCache::FlushToDisk()
{
    std::lock_guard<std::mutex> lock(flushMutex_);
    size_t allocSize = 0;
    size_t connectSize = 0;
    const std::string jsonStr = SerializeSnapshot(allocSize, connectSize);
    if (!WriteCacheFileAtomically(jsonStr)) {
        return false;
    }
    UBSE_LOG_INFO << "FlushToDisk success: alloc=" << allocSize << " connect=" << connectSize;
    return true;
}

} // namespace ubse::ssu::controller
