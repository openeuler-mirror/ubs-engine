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
#ifndef UBSE_MEM_SCHEDULER_STRATEGY_IMPL_H
#define UBSE_MEM_SCHEDULER_STRATEGY_IMPL_H

#include <memory>
#include <mutex>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_types.h"
#include "ubse_noncopyable.h"

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_filter_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_mem_scheduler_score_manager.h"

namespace ubse::mem::scheduler {

#define MODULE_LOG_NAME "ubse_mem_scheduler"

class SchedulerImpl : public Noncopyable {
public:
    /* *
     * @brief   获取 SchedulerImpl 单例
     *
     * @return SchedulerImpl&   单例引用
     */
    static SchedulerImpl& GetInstance()
    {
        static SchedulerImpl instance;
        return instance;
    }
    ~SchedulerImpl();

    /* *
     * @brief   scheduler 模块初始化：注册集群状态回调、初始化内部管理器
     *
     * @return UbseResult   0：成功；非0：失败
     */
    UbseResult Init();

    /* *
     * @brief   主备切换后，清理算法缓存数据
     */
    void ClearCache();

    /* *
     * @brief   Node 状态变化回调处理
     *
     * @param nodeInfo  [IN] 节点信息
     * @return UbseResult  0：成功；非0：失败
     */
    UbseResult NodeObjChangeHandler(const UbseNodeInfo& nodeInfo);

    /* *
     * @brief   借用对象状态变化统一处理入口
     *          （支持 fd/numa/shm/addr 的 import/export 对象）
     *
     * @param memObj  [IN]/[OUT] 借用对象引用，类型由模板自动推导
     * @return UbseResult  0：成功；非0：失败
     */
    template <typename MemObj>
    UbseResult MemoryObjChangeHandler(MemObj& memObj)
    {
        std::unique_lock<std::mutex> guard(lock_);
        if (memObj.status.state == adapter_plugins::mmi::UBSE_MEM_SCHEDULING) {
            SchedulerRequest request = ConvertMemObjToRequest(memObj);
            if (nodeInfo_->GetLenderBalance() && request.requestMode_ == RequestMode::BORROW) {
                request.params_["lenderBalance"] = true;
            }
            auto ret = ScheduleBorrow(request, memObj.algoResult);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Memory handle borrow request failed, name=" << memObj.req.name;
                return ret;
            }
            UBSE_LOG_INFO << "Memory handle borrow request success, name=" << memObj.req.name;
        }
        return account_->UpdateSchedulerAccount(memObj.req.name, memObj.algoResult, memObj.status.state,
                                                MemObjType<MemObj>::TYPE);
    }

private:
    UbseResult FillAlgoResult(const SchedulerRequest& request, const ScoredNode& node,
                              adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    UbseResult SelectNumaByLenderInfo(const SchedulerRequest& request,
                                      const adapter_plugins::mmi::UbseMemLenderInfo& lenderInfo,
                                      adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    UbseResult SelectNumaForAddr(const SchedulerRequest& request,
                                 const adapter_plugins::mmi::UbseMemLenderInfo& lenderInfo,
                                 adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    UbseResult SelectNumaByFreeMemory(const SchedulerRequest& request, const ScoredNode& node,
                                      adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    UbseResult SelectNumaByReliable(const SchedulerRequest& request, const ScoredNode& node,
                                    adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    UbseResult ScheduleBorrow(const SchedulerRequest& request, adapter_plugins::mmi::UbseMemAlgoResult& algoResult);

    std::mutex lock_;
    SchedulerImpl();

    std::unique_ptr<SchedulerAccountManager> account_;
    std::unique_ptr<SchedulerNodeManager> nodeInfo_;
    std::unique_ptr<SchedulerFilterManager> filterManager_;
    std::unique_ptr<SchedulerScoreManager> scoreManager_;
    bool initialized_{false};
};
#undef MODULE_LOG_NAME
} // namespace ubse::mem::scheduler
#endif // UBSE_MEM_SCHEDULER_STRATEGY_IMPL_H
