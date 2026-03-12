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

#ifndef MEMPOOLING_NUMA_MEMINFO_SEND_H
#define MEMPOOLING_NUMA_MEMINFO_SEND_H

#include "over_commit_serializer.h"
#include "mempooling_interface.h"
#include "ubse_def.h"


namespace mempooling::over_commit {
class NumaMemInfoSend {
public:
    NumaMemInfoSend(std::string nid, int numaId) : nid_(std::move(nid)), numaId_(std::move(numaId)) {}

    ~NumaMemInfoSend() = default;

    MpResult SendMsg();

    /**
     * 发送rpc消息
     * @param reqData 消息body体
     * @return
     */
    MpResult CreateRequestData(UbseByteBuffer &reqData) const;

    /**
     * 处理返回消息
     * @param ctx MemMigrateSend指针
     * @param respData 返回消息体
     * @param resCode 接口调用结果
     */
    static void RespHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);

    std::string resJson_;

private:
    std::string nid_;

    int numaId_;

    MpResult sendResult_{};
};
}
#endif  // MEMPOOLING_NUMA_MEMINFO_SEND_H
