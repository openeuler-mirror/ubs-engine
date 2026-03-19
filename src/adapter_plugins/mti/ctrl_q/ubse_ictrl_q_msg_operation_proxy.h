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

#ifndef UBSE_ICTRL_Q_MSG_OPERATION_PROXY_H
#define UBSE_ICTRL_Q_MSG_OPERATION_PROXY_H
#include "ubse_common_def.h"
#include "ubse_ctrl_q_message.h"
#include "ubse_error.h"
#include "ubse_ictrl_q_req_msg.h"
#include "ubse_pointer_process.h"

namespace ubse::mti::ctrl_q {
using namespace ubse::common::def;
template <typename Response>
class ICtrlQMsgOperationProxy {
public:
    virtual ~ICtrlQMsgOperationProxy() = default;

    UbseResult SendRequest(ICtrlQReqMsg &reqMsg)
    {
        CtrlQReqMessage msg;
        auto ret = reqMsg.GetReqMsg(msg);
        if (ret != UBSE_OK) {
            return ret;
        }
        if (!CheckReqValidation(msg)) {
            return UBSE_ERROR;
        }
        auto respMsg = SafeMakeUniqueWithFreeFunc<CtrlQRespMessage>([](CtrlQRespMessage *resp) {
            if (resp->blocks != nullptr) {
                delete[] resp->blocks;
                resp->blockNums = 0;
            }
            delete resp;
        });
        if (respMsg == nullptr) {
            return UBSE_ERROR_NOMEM;
        }
        auto res = SendMsg(msg, *respMsg);
        if (res != UBSE_OK) {
            return res;
        }
        return ConvertRespMsgToUserData(reqMsg, *respMsg);
    }

    const Response &GetResponse() const
    {
        return resp_;
    }

protected:
    Response resp_;

private:
    /**
     * @brief 请求参数合法性校验，由子类实现
     * @param msg [in] 请求结构体
     *
     * @return true代表校验通过，false代表校验失败
     */
    virtual bool CheckReqValidation(const CtrlQReqMessage &msg) = 0;

    /**
     * @brief 将返回参数转化为上层业务需要的数据结构
     * @param msg [in] 返回消息结构体
     *
     * @return CtrlQErrorCode
     */
    virtual UbseResult ConvertRespMsgToUserData(const ICtrlQReqMsg &reqMsg, const CtrlQRespMessage &msg) = 0;
};
} // namespace ubse::mti::ctrl_q
#endif // UBSE_ICTRL_Q_MSG_OPERATION_PROXY_H
