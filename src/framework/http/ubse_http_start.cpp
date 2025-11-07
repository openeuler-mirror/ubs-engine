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

#include "ubse_http_start.h"

#include "ubse_http_tcp_server.h" // for UbseHttpTcpServer
#include "ubse_conf_module.h"     // for UbseConfModule
#include "ubse_error.h"           // for UBSE_OK, UBSE_ERROR, UBSE_ERROR_N...
#include "ubse_logger.h"          // for FormatRetCode, UbseLoggerEntry
#include "ubse_logger_inner.h"
namespace ubse::http {
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_HTTP_MID)

void StopTcpServer()
{
    try {
        UbseHttpTcpServer::GetInstance().Stop();
    } catch (const std::exception&) {
        UBSE_LOG_ERROR <<"Failed to stop TCP server.";
    }
}

bool StartTcpServer()
{
    try {
        return UbseHttpTcpServer::GetInstance().Start();
    } catch (const std::exception&) {
        UBSE_LOG_ERROR << "Failed to start TCP server.";
        return false;
    }
}
} // namespace ubse::http