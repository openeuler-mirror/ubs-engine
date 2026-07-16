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

std::string StrToHex(const std::string &id)
{
    if (id.empty()) {
        return "";
    }

    // 预先精确分配空间：1 字节二进制对应 2 字节十六进制文本
    std::string hexStr;
    hexStr.resize(id.size() * 2);

    static const char hexDigits[] = "0123456789abcdef";

    for (size_t i = 0; i < id.size(); ++i) {
        // 转为 unsigned char
        unsigned char byte = static_cast<unsigned char>(id[i]);

        hexStr[i * 2] = hexDigits[byte >> 4];       // 高 4 位
        hexStr[i * 2 + 1] = hexDigits[byte & 0x0F]; // 低 4 位
    }

    return hexStr;
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

} // namespace ubse::ssu::utils
