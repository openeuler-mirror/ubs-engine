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

#ifndef UBSE_SYNC_REQ_H
#define UBSE_SYNC_REQ_H

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "ubse_ipc_message.h"

namespace ubse::ipc {
/**
 * 长连接模式下 客户端和服务端均适用epoll来处理对端请求；若需要发送同步消息，对方的响应由epoll线程处理，需要记录响应 并对外提供等待函数；
 */
class UbseSyncReq {
public:
    static UbseSyncReq &GetInstance()
    {
        static UbseSyncReq instance;
        return instance;
    }

    ~UbseSyncReq() noexcept;

    /**
     * 注册等待响应的消息
     * @param reqId
     */
    void RegisterRequest(uint64_t reqId);

    /**
     * 当前消息是否需要监听；客户端没有监听的reqId的请求不要保存响应
     * @param reqId
     * @return
     */
    bool IsReqIdRegister(uint64_t reqId);

    /**
     * 同步等待请求响应
     * @param reqId 监听同步请求id
     * @param timeout 监听超时时间
     * @param msg 监听结果
     * @return
     */
    uint32_t WaitForResp(uint64_t reqId, int timeout, UbseResponseMessage &msg);

    void StoreResp(uint64_t reqId, UbseResponseMessage msg);

private:
    std::mutex mtx_;
    std::unordered_map<uint64_t, UbseResponseMessage> responses_; // 请求ID到响应的映射
    std::unordered_set<uint64_t> waitList_;                       // 请求ID到条件变量的映射
};
} // namespace ubse::ipc

#endif // UBSE_SYNC_REQ_H
