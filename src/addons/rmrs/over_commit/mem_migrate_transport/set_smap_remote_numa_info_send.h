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

#ifndef SET_SMAP_REMOTE_NUAM_INFO_SEND_H
#define SET_SMAP_REMOTE_NUAM_INFO_SEND_H

#include "ubse_com.h"

#include "mempooling_interface.h"
#include "mp_error.h"
#include "set_smap_remote_numa_info_trans_msg.h"

namespace mempooling::over_commit {

class SetSmapRemoteNumaInfoSend {
public:
    SetSmapRemoteNumaInfoSend(outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam,
                              std::vector<MemBorrowInfo> memBorrowInfos)
        : srcMemoryBorrowParam_(std::move(srcMemoryBorrowParam)),
          memBorrowInfos_(std::move(memBorrowInfos))
    {
    }

    ~SetSmapRemoteNumaInfoSend() = default;

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

private:
    outinterface::SrcMemoryBorrowParam srcMemoryBorrowParam_;
    std::vector<MemBorrowInfo> memBorrowInfos_;

    MpResult sendResult_{};
};

} // namespace mempooling::over_commit

#endif // SET_SMAP_REMOTE_NUAM_INFO_SEND_H
