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

#include "ubse_ssu_attach_record_store.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <cerrno>
#include <fstream>
#include <sstream>
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "src/framework/misc/ubse_env_util.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::ssu::persist {

using namespace ubse::log;
using namespace ubse::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

namespace {

// 记录文件默认路径（env UBSE_SSU_ATTACH_RECORD_FILE 可覆盖，测试用）
constexpr const char *DEFAULT_RECORD_FILE = "/var/lib/ubse/ssu/attach_records.json";

// 记录文件格式版本
constexpr uint32_t RECORD_FILE_VERSION = 1;

// 记录总数上限：单节点挂载数受空间数约束（≤数百），取 4096 覆盖余量；
// 超限视为文件被篡改，拒载防内存耗尽
constexpr uint32_t MAX_ATTACH_RECORDS = 4096;

// 记录文件权限：属主读写，防越权访问
constexpr mode_t RECORD_FILE_MODE = S_IRUSR | S_IWUSR;

// 记录目录权限
constexpr mode_t RECORD_DIR_MODE = 0755;

std::string GetDirName(const std::string &path)
{
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return ".";
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

// 逐级创建目录（已存在则忽略），返回目录是否可用
bool MakeDirs(const std::string &dir)
{
    if (dir.empty() || dir == ".") {
        return true;
    }
    std::string current;
    current.reserve(dir.size());
    for (size_t i = 0; i < dir.size(); ++i) {
        current.push_back(dir[i]);
        if (dir[i] == '/' && i > 0) {
            mkdir(current.c_str(), RECORD_DIR_MODE);
        }
    }
    mkdir(dir.c_str(), RECORD_DIR_MODE);
    struct stat st {};
    return stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool ReadFileToString(const std::string &path, std::string &content)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    return true;
}

// 本机 nodeId；获取失败（选举未就绪等）返回空串，调用方跳过相关校验/打标
std::string GetCurrentNodeIdSafe()
{
    std::string nodeId;
    if (ubse::election::UbseGetCurrentNodeId(nodeId) != UBSE_OK) {
        return "";
    }
    return nodeId;
}

} // namespace

UbseSsuAttachRecordStore &UbseSsuAttachRecordStore::GetInstance()
{
    static UbseSsuAttachRecordStore instance;
    return instance;
}

uint32_t UbseSsuAttachRecordStore::Init()
{
    std::lock_guard<std::mutex> lock(mtx_);
    filePath_ = GetEnv<std::string>("UBSE_SSU_ATTACH_RECORD_FILE", DEFAULT_RECORD_FILE);
    if (!MakeDirs(GetDirName(filePath_))) {
        UBSE_LOG_ERROR << "SSU attach record store: create dir failed, filePath=" << filePath_
                       << ", errno=" << errno;
        return UBSE_ERROR;
    }
    const uint32_t ret = LoadLocked();
    if (ret != UBSE_OK) {
        inited_.store(false, std::memory_order_release); // 允许恢复流程后续重试
        UBSE_LOG_ERROR << "SSU attach record store: load failed, filePath=" << filePath_ << ", ret=" << ret;
        return ret;
    }
    inited_.store(true, std::memory_order_release);
    UBSE_LOG_INFO << "SSU attach record store init success, filePath=" << filePath_
                  << ", recordCount=" << records_.size();
    return UBSE_OK;
}

uint32_t UbseSsuAttachRecordStore::EnsureInited()
{
    // acquire/release与Init内的store配对：已成功初始化则no-op（不重载，保护内存视图，
    // 避免主备切换等多次触发时丢弃"落盘失败仅存内存"的记录）
    if (inited_.load(std::memory_order_acquire)) {
        return UBSE_OK;
    }
    return Init();
}

uint32_t UbseSsuAttachRecordStore::LoadLocked()
{
    // 入口 clear：保证重入 Init（env 切换/重试）时内存视图与文件状态一致，任何失败短路径均从空开始
    records_.clear();
    struct stat st {};
    if (stat(filePath_.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            UBSE_LOG_INFO << "SSU attach record file not exist, start with empty records, filePath="
                          << filePath_;
            return UBSE_OK; // 文件不存在视为空（首次部署/未挂载过）
        }
        UBSE_LOG_ERROR << "SSU attach record file stat failed, filePath=" << filePath_
                       << ", errno=" << errno;
        return UBSE_ERROR;
    }

    // 校验0：属主须为运行用户、权限 0600（捕获权限漂移，文件按不可信输入处理）
    if (st.st_uid != geteuid() || (st.st_mode & 0777) != RECORD_FILE_MODE) {
        UBSE_LOG_ERROR << "SSU attach record file owner/mode check failed, filePath=" << filePath_
                       << ", uid=" << st.st_uid << ", mode=" << (st.st_mode & 0777)
                       << ", expectMode=600"
                       << ", refuse to load";
        return UBSE_ERROR;
    }

    std::string content;
    if (!ReadFileToString(filePath_, content)) {
        UBSE_LOG_ERROR << "SSU attach record file read failed, filePath=" << filePath_;
        return UBSE_ERROR;
    }

    rapidjson::Document doc;
    if (doc.Parse(content.c_str(), content.size()).HasParseError()) {
        // 整体不可解析（异常断电半截写等）：隔离留档，从空开始，行为退化不产生错误挂载
        IsolateBadFileLocked("json parse failed");
        return UBSE_ERROR;
    }
    if (!doc.IsObject() || !doc.HasMember("version") || !doc["version"].IsUint() ||
        doc["version"].GetUint() != RECORD_FILE_VERSION) {
        IsolateBadFileLocked("invalid version field");
        return UBSE_ERROR;
    }

    // nodeId 比对：不一致（整机盘被挪用/误拷贝）则隔离拒载，避免恢复他机挂载。
    // 隔离（而非仅拒载）可防止后续 Upsert 覆写原文件丢失现场
    if (!doc.HasMember("nodeId") || !doc["nodeId"].IsString()) {
        IsolateBadFileLocked("invalid nodeId field");
        return UBSE_ERROR;
    }
    const std::string fileNodeId = doc["nodeId"].GetString();
    const std::string localNodeId = GetCurrentNodeIdSafe();
    if (!localNodeId.empty() && !fileNodeId.empty() && fileNodeId != localNodeId) {
        IsolateBadFileLocked("nodeId mismatch, file=" + fileNodeId + ", local=" + localNodeId);
        return UBSE_ERROR;
    }

    if (!doc.HasMember("records") || !doc["records"].IsArray()) {
        IsolateBadFileLocked("invalid records field");
        return UBSE_ERROR;
    }
    const auto &recordArr = doc["records"];
    // 校验1：记录总数先 cap 再 reserve（防篡改 count 内存耗尽）
    if (recordArr.Size() > MAX_ATTACH_RECORDS) {
        IsolateBadFileLocked("record count exceeds cap, count=" + std::to_string(recordArr.Size()));
        return UBSE_ERROR;
    }
    for (rapidjson::SizeType i = 0; i < recordArr.Size(); ++i) {
        UbseSsuAttachRecord record{};
        // 校验2/3：单条非法仅丢弃该条 + WARN，不整文件作废
        if (!DeserializeRecord(recordArr[i], record)) {
            UBSE_LOG_WARN << "SSU attach record: discard invalid record entry, index=" << i;
            continue;
        }
        records_[record.name] = std::move(record);
    }
    UBSE_LOG_INFO << "SSU attach record file loaded, filePath=" << filePath_
                  << ", total=" << recordArr.Size() << ", valid=" << records_.size()
                  << ", nodeId=" << fileNodeId;
    return UBSE_OK;
}

std::vector<UbseSsuAttachRecord> UbseSsuAttachRecordStore::GetAll()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<UbseSsuAttachRecord> result;
    result.reserve(records_.size());
    for (const auto &kv : records_) {
        result.emplace_back(kv.second);
    }
    return result;
}

std::optional<UbseSsuAttachRecord> UbseSsuAttachRecordStore::Get(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = records_.find(name);
    if (it == records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

uint32_t UbseSsuAttachRecordStore::Upsert(const UbseSsuAttachRecord &record)
{
    if (record.name.empty()) {
        return UBSE_ERROR;
    }
    std::lock_guard<std::mutex> lock(mtx_);
    records_[record.name] = record;
    const uint32_t ret = PersistLocked();
    if (ret != UBSE_OK) {
        // 内存视图保留更新，下次成功落盘自愈重写；挂载主流程不因记录失败而失败
        UBSE_LOG_ERROR << "SSU attach record persist failed on upsert, name=" << record.name
                       << ", recordState=" << static_cast<uint32_t>(record.recordState) << ", ret=" << ret;
        return ret;
    }
    UBSE_LOG_INFO << "SSU attach record upsert success, name=" << record.name
                  << ", recordState=" << static_cast<uint32_t>(record.recordState);
    return UBSE_OK;
}

uint32_t UbseSsuAttachRecordStore::Remove(const std::string &name)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (records_.find(name) == records_.end()) {
        return UBSE_OK; // 幂等：记录不存在视为成功
    }
    records_.erase(name);
    const uint32_t ret = PersistLocked();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SSU attach record persist failed on remove, name=" << name << ", ret=" << ret;
        return ret;
    }
    UBSE_LOG_INFO << "SSU attach record remove success, name=" << name;
    return UBSE_OK;
}

uint32_t UbseSsuAttachRecordStore::PersistLocked()
{
    // 全量序列化
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    doc.AddMember("version", RECORD_FILE_VERSION, allocator);
    const std::string nodeId = GetCurrentNodeIdSafe();
    doc.AddMember("nodeId", rapidjson::Value(nodeId.c_str(), allocator), allocator);
    rapidjson::Value recordArr(rapidjson::kArrayType);
    for (const auto &kv : records_) {
        rapidjson::Value item(rapidjson::kObjectType);
        SerializeRecord(kv.second, item, allocator);
        recordArr.PushBack(item, allocator);
    }
    doc.AddMember("records", recordArr, allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!doc.Accept(writer)) {
        UBSE_LOG_ERROR << "SSU attach record serialize failed.";
        return UBSE_ERROR;
    }

    // 原子写：tmp + write + fsync(文件) + rename + fsync(目录)，任一环节失败返回错误
    // （fsync 目录必须在 rename 之后，才能把 rename 产生的目录项变更刷盘保证其持久性）
    const std::string tmpPath = filePath_ + ".tmp";
    const auto cleanupTmp = [&tmpPath]() { unlink(tmpPath.c_str()); };
    const int fd = open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, RECORD_FILE_MODE);
    if (fd < 0) {
        UBSE_LOG_ERROR << "SSU attach record: open tmp file failed, path=" << tmpPath
                       << ", errno=" << errno;
        return UBSE_ERROR;
    }
    const char *data = buffer.GetString();
    size_t remaining = buffer.GetSize();
    while (remaining > 0) {
        const ssize_t written = write(fd, data, remaining);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            cleanupTmp();
            UBSE_LOG_ERROR << "SSU attach record: write tmp file failed, errno=" << errno;
            return UBSE_ERROR;
        }
        data += written;
        remaining -= static_cast<size_t>(written);
    }
    if (fsync(fd) != 0) {
        close(fd);
        cleanupTmp();
        UBSE_LOG_ERROR << "SSU attach record: fsync file failed, errno=" << errno;
        return UBSE_ERROR;
    }
    close(fd);

    // 先 rename 原子替换；目录已由 Init 的 MakeDirs 创建且 tmp 写入成功，open/fsync 目录
    // 失败属异常，按错误处理（rename 已发生无法回滚，但须让调用方感知持久性存疑，
    // 内存视图保留、下次成功落盘自愈重写）。rename 成功后 tmp 已不存在，无需清理
    if (rename(tmpPath.c_str(), filePath_.c_str()) != 0) {
        cleanupTmp();
        UBSE_LOG_ERROR << "SSU attach record: rename failed, errno=" << errno;
        return UBSE_ERROR;
    }
    const int dirFd = open(GetDirName(filePath_).c_str(), O_RDONLY | O_DIRECTORY);
    if (dirFd < 0) {
        UBSE_LOG_ERROR << "SSU attach record: open dir for fsync failed, errno=" << errno;
        return UBSE_ERROR;
    }
    const int syncRet = fsync(dirFd);
    close(dirFd);
    if (syncRet != 0) {
        UBSE_LOG_ERROR << "SSU attach record: fsync dir failed, errno=" << errno;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseSsuAttachRecordStore::IsolateBadFileLocked(const std::string &reason)
{
    // system_clock 无失败返回值，规避 time() 返回值需检查的告警
    const auto nowSecs = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
    const std::string basePath = filePath_ + ".bad." + std::to_string(nowSecs);
    // rename 对已存在目标会静默覆盖（不返回 EEXIST），须先探测存在性递增后缀，
    // 防同秒二次隔离（如 Init 重试）覆盖前次隔离现场
    std::string badPath = basePath;
    for (int i = 1; access(badPath.c_str(), F_OK) == 0; ++i) {
        badPath = basePath + "." + std::to_string(i);
    }
    if (rename(filePath_.c_str(), badPath.c_str()) != 0) {
        UBSE_LOG_ERROR << "SSU attach record: isolate bad file failed, errno=" << errno;
        return;
    }
    // 坏文件可能权限异常，重置为 0600 留档
    chmod(badPath.c_str(), RECORD_FILE_MODE);
    UBSE_LOG_ERROR << "SSU attach record file corrupted and isolated, filePath=" << filePath_
                   << ", backupPath=" << badPath << ", reason=" << reason;
}

} // namespace ubse::ssu::persist
