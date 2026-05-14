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

#ifndef UBSE_MANAGER_MANAGER_TEST PT_H
#define UBSE_MANAGER_MANAGER_TEST PT_H

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::pt {
using namespace ubse::context;
using namespace std;

// PT测试通信需要配置项修改
const string PT_NODE_ROLE = "Agent";
const string PT_NODE_ID = "Node1";
const string PT_NODE_IDS = "Node0,Node1,Node2";
const string MASTER_ADDRESS = "Node0:7.225.78.130:1901";
const string SERVER_ADDRESS = "7.225.78.130:1901";
const string DB_SERVER_IP = "7.225.78.130";
const string DB_IS_MASTER = "false";

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
    CMD_MEM_COM,
    CMD_MAX,
};

using FuncPtrp = int32_t (*)(ProcessMmap*);

struct UbseCmdAndFunc {
    TagTestCmdInfo enCmd;
    FuncPtrp pFunc;
};

using FuncPtr = void (*)(ProcessMmap*);

ProcessMmap* MemoryMapping();

void PTestResourceInit();

void PTestCliStart(ProcessMmap* pMmap);

void PTestServerStart(ProcessMmap* pMmap);

uint32_t PTestSetSubProcess(FuncPtr* opt, ProcessMmap* pMmap, pid_t* pid, const std::string& procName);

uint32_t PTestSetProcess(ProcessMmap* pMmap);

void PTestCmdAndFuncRegister(TagTestCmdInfo enCmd, FuncPtrp pFunc, ProcessMode processMode = ProcessMode::MANAGER);

void PTestGetCliResult(ProcessMmap* pMmap, uint32_t PTime, int32_t* psiStatus);

void PTestGetServerResult(ProcessMmap* pMmap, uint32_t PTime, int32_t* psiStatus);

int32_t PTestCmdServerStart(ProcessMmap* pMmap);

int32_t PTestCmdCliStart(ProcessMmap* pMmap);

void PTestQuitProcess(ProcessMmap* pMmap);

void PTestClearResource(ProcessMmap* pMmap);

int32_t PTestCmdServerStop(ProcessMmap* pMmap);

int32_t PTestCmdCliStop([[maybe_unused]] ProcessMmap* pMmap);
} // namespace ubse::pt
#endif // UBSE_MANAGER_MANAGER_TEST PT_H
