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

#include "test_ubse_com_mock.h"

namespace ubse::ut::com {
ubs_hcom_channel g_mockContextChannel = 0;
} // namespace ubse::ut::com

/*
 * 以下C函数覆盖libhcom.so同名符号（可执行文件符号优先），channel/context级接口
 * 按假句柄行为返回，UT无需真实建链；service级create/destroy/start/bind以及各set、register类函数
 * 不在此覆盖，仍走libhcom.so真实实现（SetUp/TearDown真实创建/销毁service）。
 */
extern "C" {
static char g_mockRspData[] = "";

void ubs_hcom_channel_refer(ubs_hcom_channel channel)
{
    (void)channel;
}

void ubs_hcom_channel_derefer(ubs_hcom_channel channel)
{
    (void)channel;
}

int ubs_hcom_channel_send(ubs_hcom_channel channel, ubs_hcom_channel_request req, ubs_hcom_channel_callback* cb)
{
    (void)channel;
    (void)req;
    (void)cb;
    return 0;
}

int ubs_hcom_channel_call(ubs_hcom_channel channel, ubs_hcom_channel_request req, ubs_hcom_channel_response* rsp,
                          ubs_hcom_channel_callback* cb)
{
    (void)channel;
    (void)req;
    (void)cb;
    if (rsp != nullptr) {
        rsp->address = g_mockRspData;
        rsp->size = 0;
        rsp->errorCode = 0;
    }
    return 0;
}

int ubs_hcom_channel_reply(ubs_hcom_channel channel, ubs_hcom_channel_request req, ubs_hcom_channel_reply_context ctx,
                           ubs_hcom_channel_callback* cb)
{
    (void)channel;
    (void)req;
    (void)ctx;
    (void)cb;
    return 0;
}

uint64_t ubs_hcom_channel_get_id(ubs_hcom_channel channel)
{
    (void)channel;
    return 1;
}

int ubs_hcom_channel_get_remote_uds_id_info(ubs_hcom_channel channel, ubs_hcom_uds_id_info* idInfo)
{
    (void)channel;
    if (idInfo != nullptr) {
        idInfo->pid = 1;
        idInfo->uid = 1;
        idInfo->gid = 1;
    }
    return 0;
}

void ubs_hcom_channel_set_timeout(ubs_hcom_channel channel, int16_t oneSideTimeout, int16_t twoSideTimeout)
{
    (void)channel;
    (void)oneSideTimeout;
    (void)twoSideTimeout;
}

int ubs_hcom_channel_set_twoside_threshold(ubs_hcom_channel channel, ubs_hcom_twoside_threshold threshold)
{
    (void)channel;
    (void)threshold;
    return 0;
}

void ubs_hcom_channel_set_trace_id(ubs_hcom_channel channel, const char* traceId)
{
    (void)channel;
    (void)traceId;
}

int ubs_hcom_channel_get_peer_connect_payload(ubs_hcom_channel channel, char* payload, uint32_t* len)
{
    (void)channel;
    if (len != nullptr && *len > 0 && payload != nullptr) {
        payload[0] = '\0';
    }
    if (len != nullptr) {
        *len = 0;
    }
    return 0;
}

int ubs_hcom_context_get_channel(ubs_hcom_service_context context, ubs_hcom_channel* channel)
{
    (void)context;
    if (channel != nullptr) {
        *channel = ubse::ut::com::g_mockContextChannel;
    }
    return 0;
}

int ubs_hcom_context_get_rspctx(ubs_hcom_service_context context, ubs_hcom_channel_reply_context* rspCtx)
{
    (void)context;
    if (rspCtx != nullptr) {
        rspCtx->rspCtx = nullptr;
        rspCtx->errorCode = 0;
    }
    return 0;
}

int ubs_hcom_context_get_result(ubs_hcom_service_context context, int* result)
{
    (void)context;
    if (result != nullptr) {
        *result = 0;
    }
    return 0;
}

uint16_t ubs_hcom_context_get_opcode(ubs_hcom_service_context context)
{
    (void)context;
    return 0;
}

void* ubs_hcom_context_get_data(ubs_hcom_service_context context)
{
    (void)context;
    return nullptr;
}

uint32_t ubs_hcom_context_get_datalen(ubs_hcom_service_context context)
{
    (void)context;
    return 0;
}

int ubs_hcom_service_connect(ubs_hcom_service service, const char* serverUrl, ubs_hcom_channel* channel,
                             ubs_hcom_service_connect_options options)
{
    (void)service;
    (void)serverUrl;
    (void)options;
    if (channel != nullptr) {
        *channel = 0;
    }
    return -1;
}

int ubs_hcom_service_disconnect(ubs_hcom_service service, ubs_hcom_channel channel)
{
    (void)service;
    (void)channel;
    return 0;
}
}
