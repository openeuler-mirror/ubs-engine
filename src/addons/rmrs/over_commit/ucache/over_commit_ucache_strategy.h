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

#ifndef OVER_COMMIT_UCACHE_STRATEGY_H
#define OVER_COMMIT_UCACHE_STRATEGY_H

#include <string>
#include <vector>
#include "ubse_def.h"
#include "turbo_rmrs_interface.h"

namespace mempooling::over_commit {
using namespace turbo::rmrs;
/*
* @brief 生成UCache迁移策略
* @param localNumaId [in] 本地迁出numaId，-1为不绑numa场景
* @param pids [in] 迁出pid列表，通过pid列表确认容器列表
* @param remoteNumaIds [in] 远端迁入numaId
* @param nodeId [in] 迁移策略执行节点
* @param ucacheStrategy [out] 生成的UCache迁移策略
*/
void GenUCacheMigrationStrategy(int16_t localNumaId, const std::vector<pid_t>& pids,
                                const std::vector<uint16_t>& remoteNumaIds, const std::string& nodeId,
                                UCacheMigrationStrategyParam& ucacheStrategy);

/*
* @brief 发送UCache迁移策略到执行节点
* @param ucacheStrategy [in] UCache迁移策略
* @param srcNid [in] 策略执行节点
* @return #MEM_POOLING_OK 策略执行成功
* @return #RMRS_ERROR 策略执行失败
*/
uint32_t SendUCacheMigrationStrategy(UCacheMigrationStrategyParam& ucacheStrategy, const std::string& srcNid);

// 停止迁移策略
void CheckAndStopUCacheMigration(const std::string& srcNid);

// （从节点调用）查询本节点UCache迁移比例
float GetLocalUcacheUsageRatio();

// 更新UCache迁移比例
void UpdateUcacheUsageRatio(const std::vector<pid_t>& pids, uint64_t borrowMemKB, const std::string& nodeId);

// (从节点调用)记录本节点UCache迁移比例
void UpdateLocalUcacheUsageRatio(float ratio);

// 获取指定节点UCache迁移比例
float GetUcacheUsageRatio(const std::string& nodeId);

uint32_t InitUCacheOverCommitReg();

void UCacheSendMigrationStrategyRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void UCacheSendMigrationStrategyResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void UCacheStopMigrationRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void UCacheStopMigrationResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void UpdateUCacheRatioRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void UpdateUCacheRatioResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

} // namespace mempooling::over_commit

#endif