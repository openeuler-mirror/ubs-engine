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

#ifndef MYCPP_REQUEST_HELPER_H
#define MYCPP_REQUEST_HELPER_H

#include <any>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_pointer_process.h"

namespace ubse::mem_controller {
#define MODULE_LOG_NAME "ubse"
using namespace ubse::log;
using namespace ubse::common::def;
using RequestID = std::string;

class ObjPromiseBase {
public:
    virtual ~ObjPromiseBase() = default;
    virtual bool SetResult(const std::any& result) = 0;
};

template <typename T>
class ObjPromise : public ObjPromiseBase {
public:
    std::promise<T> promise;

    bool SetResult(const std::any& result) override
    {
        if (result.type() != typeid(T)) {
            return false;
        }
        try {
            T value = std::any_cast<T>(result);
            promise.set_value(value);
            return true;
        } catch (const std::bad_any_cast& e) {
            UBSE_LOG_ERROR << "bad cast:" << e.what();
            return false;
        } catch (const std::future_error& e) {
            // promise状态错误，比如重复设置，记录日志或忽略
            UBSE_LOG_ERROR << "promise status error:" << e.what();
            ;
            return false;
        }
    }
};

class FutureMgr {
public:
    /* *
     * 同一个requestId，多个请求并发时，该方法会阻塞等待上一个完成;
     * 不同的requestId之间没有约束.
     * 返回的FutureObject，当FutureObject退出作用域时，自动从FutureObjectHelper中删除
     * @param requestId
     * @return
     */
    static std::shared_ptr<FutureMgr> CreateInstance(const std::string& requestId) __attribute__((warn_unused_result));

    /* *
     * 根据requestId，给当前的Future通知result
     * @param requestId
     * @param result
     * @return
     */
    static bool SetResult(const std::string& requestId, const std::any& result);

    /* *
     * 查找是否存在请求
     * @param requestId
     * @return
     */
    static bool Find(const std::string& requestId);

    /* *
     * 返回当前的请求数量，便于测试用例开发.
     * @return
     */
    static size_t GetSize();

    explicit FutureMgr(std::string requestId) : requestIdInner_(std::move(requestId)) {}

    ~FutureMgr();

    template <typename ResultType>
    std::future<ResultType> GetFuture()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        auto objPromise = SafeMakeShared<ObjPromise<ResultType>>();
        if (!objPromise) {
            return std::future<ResultType>{};
        }
        auto future = objPromise->promise.get_future();
        curObjPromise_ = objPromise;
        return future;
    }

    bool SetResult(const std::any& result);

private:
    std::mutex mtx_;
    std::string requestIdInner_;
    std::shared_ptr<ObjPromiseBase> curObjPromise_;

    // 全局管理
    static std::mutex mapMutex;
    static std::condition_variable waitRequestFinishedCv;
    static std::unordered_map<std::string, std::weak_ptr<FutureMgr>> mgrInstanceMap;
};
#undef MODULE_LOG_NAME
} // namespace ubse::mem_controller

#endif // MYCPP_REQUEST_HELPER_H
