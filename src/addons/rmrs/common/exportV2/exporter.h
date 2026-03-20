/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef EXPORT_H
#define EXPORT_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
#include "ThreadPool/ThreadPool.h"
#include "export_type.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "ubse_logger.h"

namespace mempooling::exportV2 {
using namespace ubse::log;

void fillErrorVmDomainInfo(VmDomainInfo &info);
bool isVmDomainInfoError(const VmDomainInfo &info);

class Exporter {
public:
    Exporter() noexcept = default;
    struct Options {
        size_t threads{0}; // 0 => use hw_concurrency
        // 允许枚举与真正采集之间的时间差上限（仅用于日志/告警，不硬阻塞）
        std::chrono::milliseconds jitter_budget{50};
        Options() = default;
    };
    static MpResult Init();
    static MpResult Init(Options opt);
    static void Shutdown(bool wait = true);
    static MpResult GetVmInfoImmediately(std::vector<VmDomainInfo> &vmDomainInfos);
    static MpResult GetNumaInfoImmediately(std::vector<NumaInfo> &numaInfos);

private:
    static VmDomainInfo genVmDomainInfo(const std::string &vmName);
    static NumaInfo genNumaInfo(const std::uint16_t &numaId);
    static time_t NowTimeT();

    class StartGate {
    public:
        void wait();
        void release();

    private:
        std::mutex mu_;
        std::condition_variable cv_;
        bool go_ = false;
    };

    static ThreadPool &Pool();
    template <typename IdT, typename OutT, typename WorkerFn>
    static void parallel_collect(const std::vector<IdT> &ids, std::vector<OutT> &outs, WorkerFn &&worker,
                                 const char *tag_prefix = nullptr)
    {
        outs.assign(ids.size(), OutT{});
        StartGate gate;
        std::vector<std::future<void>> futs;
        std::mutex exc_mu;
        std::vector<std::exception_ptr> exceptions;
        futs.reserve(ids.size());
        for (size_t i = 0; i < ids.size(); ++i) {
            const char *tag_cstr = [&]() -> const char* {
                static thread_local std::string tag;
                if (tag_prefix) {
                    std::ostringstream oss;
                    oss << tag_prefix << "#" << i << " id=" << ids[i];
                    tag = oss.str();
                    return tag.c_str();
                }
                return nullptr;
            }();
            try {
                futs.emplace_back(Pool().submit_named(tag_cstr, [&, i]() {
                    gate.wait();
                    try {
                        outs[i] = worker(ids[i]);
                    } catch (...) {
                        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                            << "worker threw exception, tag=" << (tag_prefix ? tag_prefix : "<no-tag>") << " idx=" << i;
                        std::lock_guard<std::mutex> lk(exc_mu);
                        exceptions.push_back(std::current_exception());
                    }
                }));
            } catch (const std::exception &e) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "submit threw exception: " << e.what() << " (tag=" << (tag_cstr ? tag_cstr : "<unnamed>") << ")";
                throw;
            } catch (...) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "submit threw unknown exception (tag=" << (tag_cstr ? tag_cstr : "<unnamed>") << ")";
                throw;
            }
        }
        gate.release();
        for (auto &f : futs) {
            try {
                f.get();
            } catch (const std::exception &e) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "future.get() threw std::exception: " << e.what();
                throw;
            } catch (...) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "future.get() threw unknown exception";
                throw;
            }
        }
        if (!exceptions.empty()) {
            std::rethrow_exception(exceptions.front());
        }
    }
    static std::atomic<bool> inited_;
    static std::mutex init_mu_;
    static Options cfg_;
    static ThreadPool *pool_;
};

} // namespace mempooling::exportV2

#endif
