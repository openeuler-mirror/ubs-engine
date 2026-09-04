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

#include "ubse_ssu_utils.h"
#include "ubse_ssu_def.h"

#include <dlfcn.h>
#include <securec.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_thread_pool_module.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ssu::utils {

using uuid_t = unsigned char[16];
using UuidGenerateRandom = void (*)(uuid_t);
using UuidUnparse = void (*)(const uuid_t uu, char *out);

constexpr size_t UUID_STR_SIZE = 37;

static std::atomic<void *> g_uuidLib{nullptr};
static std::atomic<UuidGenerateRandom> g_uuidGenerateRandomFunc{nullptr};
static std::atomic<UuidUnparse> g_uuidUnparseFunc{nullptr};
static std::atomic<bool> g_uuidInitialized{false};

using namespace ubse::context;
using namespace ubse::task_executor;

UbseTaskExecutorPtr GetSsuExecutor()
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return nullptr;
    }
    return taskExecutor->Get("ubseSsuController");
}

std::string StrToUuid(const std::string &id)
{
    // 原始二进制UUID固定为16字节
    constexpr size_t UUID_BIN_SIZE = 16;
    if (id.size() != UUID_BIN_SIZE) {
        return "";
    }

    static const char hexDigits[] = "0123456789abcdef";
    // 标准UUID文本格式：8-4-4-4-12（十六进制），共32个十六进制字符 + 4个连字符 = 36字符
    // 按字节分组：4字节-2字节-2字节-2字节-6字节
    static const size_t groupSizes[] = {4, 2, 2, 2, 6};
    std::string uuidStr;
    uuidStr.reserve(36);

    size_t offset = 0;
    for (size_t g = 0; g < 5; ++g) {
        if (g > 0) {
            uuidStr.push_back('-');
        }
        for (size_t i = 0; i < groupSizes[g]; ++i) {
            unsigned char byte = static_cast<unsigned char>(id[offset++]);
            uuidStr.push_back(hexDigits[byte >> 4]);       // 高 4 位
            uuidStr.push_back(hexDigits[byte & 0x0F]);     // 低 4 位
        }
    }
    return uuidStr;
}

uint32_t InitUuid()
{
    bool expected = false;
    if (!g_uuidInitialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return 0;
    }

    void *lib = dlopen("libuuid.so.1", RTLD_LAZY);
    if (lib == nullptr) {
        UBSE_LOG_WARN << "InitUuid: dlopen libuuid.so.1 failed, fallback to random generation";
        return 0;
    }

    auto generateFunc = reinterpret_cast<UuidGenerateRandom>(dlsym(lib, "uuid_generate_random"));
    if (generateFunc == nullptr) {
        UBSE_LOG_WARN << "InitUuid: dlsym uuid_generate_random failed, fallback to random generation";
        dlclose(lib);
        return 0;
    }

    auto unparseFunc = reinterpret_cast<UuidUnparse>(dlsym(lib, "uuid_unparse"));
    if (unparseFunc == nullptr) {
        UBSE_LOG_WARN << "InitUuid: dlsym uuid_unparse failed, fallback to random generation";
        dlclose(lib);
        return 0;
    }

    g_uuidLib.store(lib, std::memory_order_release);
    g_uuidGenerateRandomFunc.store(generateFunc, std::memory_order_release);
    g_uuidUnparseFunc.store(unparseFunc, std::memory_order_release);
    UBSE_LOG_INFO << "InitUuid: libuuid loaded successfully";
    return 0;
}

static std::string GenerateUuidFallback()
{
    thread_local std::mt19937_64 rng(std::random_device{}() ^ std::hash<std::thread::id>{}(std::this_thread::get_id()));
    thread_local std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    uint64_t hi = static_cast<uint64_t>(now) ^ tid ^ dist(rng);
    uint64_t lo = dist(rng);

    char buf[UUID_STR_SIZE] = {0};
    errno_t ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%08lx-%04lx-%04lx-%04lx-%012lx",
                             static_cast<unsigned long>(hi >> 32), static_cast<unsigned long>((hi >> 16) & 0xFFFF),
                             static_cast<unsigned long>(hi & 0xFFFF), static_cast<unsigned long>(lo >> 48),
                             static_cast<unsigned long>(lo & 0xFFFFFFFFFFFFULL));
    if (ret < 0) {
        return "";
    }
    return buf;
}

static std::string GenerateUuid()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() { InitUuid(); });

    auto generateFunc = g_uuidGenerateRandomFunc.load(std::memory_order_relaxed);
    auto unparseFunc = g_uuidUnparseFunc.load(std::memory_order_relaxed);

    if (generateFunc && unparseFunc) {
        uuid_t uuid;
        generateFunc(uuid);
        char buf[UUID_STR_SIZE] = {0};
        unparseFunc(uuid, buf);
        buf[UUID_STR_SIZE - 1] = '\0';
        return buf;
    }

    return GenerateUuidFallback();
}

std::string GenerateHostNqn()
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << "nqn." << std::put_time(&tm, "%Y-%m") << ".org.nvmexpress:uuid:" << GenerateUuid();
    return oss.str();
}

bool IsValidHostNqn(const std::string &hostNqn)
{
    // NVMe 规范 NQN 格式：nqn.YYYY-MM.<reverse-domain>[:subdomain...]
    // 其中 reverse-domain 为反向域名，仅含 [a-zA-Z0-9.-]
    // 若子域中包含 :uuid:，则后续部分须为 8-4-4-4-12 UUID 十六进制格式
    static const std::string PREFIX = "nqn.";
    static const size_t DATE_SEG_LEN = 7; // YYYY-MM
    static constexpr size_t YEAR_LEN = 4;
    static constexpr size_t MONTH_LEN = 2;
    static constexpr int YEAR_MIN = 2010; // NVMe 规范于 2010 年发布
    static constexpr int MONTH_MIN = 1;
    static constexpr int MONTH_MAX = 12;
    static const std::string UUID_SUBDOMAIN = ":uuid:";
    static const size_t UUID_TOTAL = 36;  // 32 hex + 4 '-'
    static const size_t UUID_GROUP_NUM = 5;
    static const size_t UUID_GROUP_SIZES[] = {8, 4, 4, 4, 12};

    if (hostNqn.empty()) {
        return false;
    }

    // 长度上限校验：NQN 总长度不得超过序列化层上限 UBSE_SSU_MAX_NQN_LENGTH（含 \0），
    // 有效字符上限为 UBSE_SSU_MAX_NQN_LENGTH - 1，超出会在 IPC 序列化层被截断或失败
    if (hostNqn.size() >= ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_NQN_LENGTH) {
        return false;
    }

    // 校验前缀 nqn.
    if (hostNqn.size() < PREFIX.size() || hostNqn.compare(0, PREFIX.size(), PREFIX) != 0) {
        return false;
    }

    size_t pos = PREFIX.size();
    if (hostNqn.size() <= pos + DATE_SEG_LEN) {
        return false;
    }

    // 校验年份 YYYY
    for (size_t i = 0; i < YEAR_LEN; ++i) {
        char c = hostNqn[pos + i];
        if (c < '0' || c > '9') {
            return false;
        }
    }
    // 年份 ≥ YEAR_MIN（NVMe 规范于 2010 年发布）
    int year = (hostNqn[pos + 0] - '0') * 1000 + (hostNqn[pos + 1] - '0') * 100 + (hostNqn[pos + 2] - '0') * 10 +
               (hostNqn[pos + 3] - '0');
    if (year < YEAR_MIN) {
        return false;
    }
    
    if (hostNqn[pos + YEAR_LEN] != '-') {
        return false;
    }

    // 校验月份段 MM
    for (size_t i = 0; i < MONTH_LEN; ++i) {
        char c = hostNqn[pos + YEAR_LEN + 1 + i];
        if (c < '0' || c > '9') {
            return false;
        }
    }
    // 月份范围 MONTH_MIN-MONTH_MAX
    int month = (hostNqn[pos + YEAR_LEN + 1 + 0] - '0') * 10
              + (hostNqn[pos + YEAR_LEN + 1 + 1] - '0');
    if (month < MONTH_MIN || month > MONTH_MAX) {
        return false;
    }
    pos += DATE_SEG_LEN;

    // 校验 . 分隔符
    if (hostNqn[pos] != '.') {
        return false;
    }
    ++pos;

    // 通用 reverse-domain 校验：允许 [a-zA-Z0-9.-:]
    for (size_t i = pos; i < hostNqn.size(); ++i) {
        char c = hostNqn[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':')) {
            return false;
        }
    }

    // 若包含 :uuid: 子域，额外校验 UUID 格式
    auto uuidPos = hostNqn.find(UUID_SUBDOMAIN, pos);
    if (uuidPos != std::string::npos) {
        uuidPos += UUID_SUBDOMAIN.size();
        if (hostNqn.size() != uuidPos + UUID_TOTAL) {
            return false;
        }
        auto isHex = [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        };
        for (size_t g = 0; g < UUID_GROUP_NUM; ++g) {
            for (size_t i = 0; i < UUID_GROUP_SIZES[g]; ++i) {
                if (!isHex(hostNqn[uuidPos])) {
                    return false;
                }
                ++uuidPos;
            }
            if (g < UUID_GROUP_NUM - 1) {
                if (hostNqn[uuidPos] != '-') {
                    return false;
                }
                ++uuidPos;
            }
        }
    }
    return true;
}

bool IsValidDevName(const std::string &devName)
{
    constexpr size_t MAX_DEV_NAME_LEN = 33; // 与ubse_ssu_obj_message.h中协议上限保持一致，含结尾'\0'
    if (devName.empty() || devName.size() >= MAX_DEV_NAME_LEN) {
        return false;
    }
    for (char c : devName) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool IsOptionalNqnValid(const std::string &nqn)
{
    return nqn.empty() || IsValidHostNqn(nqn);
}

} // namespace ubse::ssu::utils
