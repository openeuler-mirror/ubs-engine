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

#ifndef UBSE_MANAGER_MANAGER_TEST_IT_H
#define UBSE_MANAGER_MANAGER_TEST_IT_H

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::it {
using namespace ubse::context;

const std::string IT_NODE_ID = "NodeIT";
const std::string IT_AGENT_NODE_ID = "Node1";
const std::string IT_MOCK_ALLNODES = "NodeIT,127.0.0.1:1901";

struct SubProcessMmap {
    bool bFlag;
    int32_t iStatus;
    uint32_t uiCmd;
    char acProcName[64];
};

struct ProcessMmap {
    bool bFlag;
    SubProcessMmap stCliInfo;
    SubProcessMmap stServerInfo;
    int32_t iStatus;
    pid_t fpid[260];
};

/* 命令类型 */
enum TagTestCmdInfo {
    CMD_INIT = 0,
    CMD_SERVER_START,
    CMD_CLI_START,
    CMD_SERVER_STOP,
    CMD_CLI_STOP,
    CMD_PUT_STORAGE,
    CMD_GET_STORAGE,
    CMD_GET_PREFIX_STORAGE,
    CMD_DELETE_PREFIX_STORAGE,
    CMD_PLUGIN_ADDPLUGIN,
    CMD_PLUGIN_ADDADMISSION,
    CMD_PLUGIN_DELETEADMISSION,
    CMD_PLUGIN_RELOAD,
    CMD_HTTP_SEND_WITH_NO_PARAMS,
    CMD_HTTP_SEND_WITH_ONLY_PARAMS,
    CMD_HTTP_SEND_WITH_ONLY_REQUEST_BODY,
    CMD_HTTP_SEND_WITH_ONLY_HEADERS,
    CMD_HTTP_SEND_WITH_ALL_PARAMS,
    CMD_HTTP_SEND_WITH_ERROR_URL,
    CMD_HTTP_SEND_WITH_ERROR_RESPONSE,
    CMD_HTTP_REGISTER_URL,
    CMD_EVENT_PUBEVENT,
    CMD_COM_INTERSEND,
    CMD_COM_INTERSENDUSECOM,
    CMD_COM_CLI_SEND,
    CMD_COM_MASTER_REGIISTER,
    CMD_DEADLOOP_SWITCH_ON,
    CMD_DEADLOOP_SWITCH_OFF,
    CMD_EXCEPTION_TEST,
    CMD_REMOTE_NODE_MOCK,
    CMD_MAX,
};

using FuncPtrp = int32_t (*)(ProcessMmap *);

struct UbseCmdAndFunc {
    TagTestCmdInfo enCmd;
    FuncPtrp pFunc;
};

using FuncPtr = void (*)(ProcessMmap *);

ProcessMmap *MemoryMapping();

void ITestResourceInit();

void ITestCliStart(ProcessMmap *pMmap);

void ITestServerStart(ProcessMmap *pMmap);

uint32_t ITestSetSubProcess(FuncPtr *opt, ProcessMmap *pMmap, pid_t *pid, const std::string &procName);

uint32_t ITestSetProcess(ProcessMmap *pMmap);

void ITestCmdAndFuncRegister(TagTestCmdInfo enCmd, FuncPtrp pFunc, ProcessMode processMode = ProcessMode::MANAGER);

void ITestGetCliResult(ProcessMmap *pMmap, uint32_t uiTime, int32_t *psiStatus);

void ITestGetServerResult(ProcessMmap *pMmap, uint32_t uiTime, int32_t *psiStatus);

int32_t ITestCmdServerStart(ProcessMmap *pMmap);

int32_t ITestCmdCliStart(ProcessMmap *pMmap);

void ITestQuitProcess(ProcessMmap *pMmap);

void ITestClearResource(ProcessMmap *pMmap);

int32_t ITestCmdServerStop(ProcessMmap *pMmap);

int32_t ITestCmdCliStop([[maybe_unused]] ProcessMmap *pMmap);
} // namespace ubse::it
#endif // UBSE_MANAGER_MANAGER_TEST_IT_H
