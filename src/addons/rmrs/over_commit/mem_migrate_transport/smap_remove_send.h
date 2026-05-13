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

#ifndef SMAP_REMOVE_SEND_H
#define SMAP_REMOVE_SEND_H

#include "ubse_com.h"

#include "mempooling_interface.h"
#include "mp_error.h"
#include "smap_remove_trans_msg.h"

namespace mempooling::over_commit {

class SmapRemoveSend {
public:
    SmapRemoveSend(outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam, std::vector<pid_t> pids)
        : srcMemoryBorrowParam_(std::move(srcMemoryBorrowParam)),
          pids_(std::move(pids))
    {
    }

    ~SmapRemoveSend() = default;

    MpResult SendMsg();

    /**
     * 发送rpc消息
     * @param reqData 消息body体
     * @return
     */
    MpResult CreateRequestData(UbseByteBuffer& reqData) const;

    /**
     * 处理返回消息
     * @param ctx MemMigrateSend指针
     * @param respData 返回消息体
     * @param resCode 接口调用结果
     */
    static void RespHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);

private:
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam_;
    std::vector<pid_t> pids_;

    MpResult sendResult_{};
};

} // namespace mempooling::over_commit

#endif // SMAP_REMOVE_SEND_H
