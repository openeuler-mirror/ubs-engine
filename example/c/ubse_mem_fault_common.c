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

#include "ubse_mem_fault_common.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

volatile int g_ubse_running = 1;

const char* ubse_fault_type_to_string(ubs_mem_fault_type_t type)
{
    switch (type) {
        case UB_MEM_ATOMIC_DATA_ERR:
            return "UB_MEM_ATOMIC_DATA_ERR";
        case UB_MEM_READ_DATA_ERR:
            return "UB_MEM_READ_DATA_ERR";
        case UB_MEM_FLOW_POISON:
            return "UB_MEM_FLOW_POISON";
        case UB_MEM_FLOW_READ_AUTH_POISON:
            return "UB_MEM_FLOW_READ_AUTH_POISON";
        case UB_MEM_FLOW_READ_AUTH_RESPERR:
            return "UB_MEM_FLOW_READ_AUTH_RESPERR";
        case UB_MEM_TIMEOUT_POISON:
            return "UB_MEM_TIMEOUT_POISON";
        case UB_MEM_TIMEOUT_RESPERR:
            return "UB_MEM_TIMEOUT_RESPERR";
        case UB_MEM_READ_DATA_POISON:
            return "UB_MEM_READ_DATA_POISON";
        case UB_MEM_READ_DATA_RESPERR:
            return "UB_MEM_READ_DATA_RESPERR";
        case MAR_NOPORT_VLD_INT_ERR:
            return "MAR_NOPORT_VLD_INT_ERR";
        case MAR_FLUX_INT_ERR:
            return "MAR_FLUX_INT_ERR";
        case MAR_WITHOUT_CXT_ERR:
            return "MAR_WITHOUT_CXT_ERR";
        case RSP_BKPRE_OVER_TIMEOUT_ERR:
            return "RSP_BKPRE_OVER_TIMEOUT_ERR";
        case MAR_NEAR_AUTH_FAIL_ERR:
            return "MAR_NEAR_AUTH_FAIL_ERR";
        case MAR_FAR_AUTH_FAIL_ERR:
            return "MAR_FAR_AUTH_FAIL_ERR";
        case MAR_TIMEOUT_ERR:
            return "MAR_TIMEOUT_ERR";
        case MAR_ILLEGAL_ACCESS_ERR:
            return "MAR_ILLEGAL_ACCESS_ERR";
        case REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR:
            return "REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR";
        case MEM_EXPORT_FAULT:
            return "MEM_EXPORT_FAULT";
        case MEM_LINK_DOWN:
            return "MEM_LINK_DOWN";
        case MEM_LINK_UP:
            return "MEM_LINK_UP";
        case UB_MEM_HEALTHY:
            return "UB_MEM_HEALTHY";
        default:
            return "UNKNOWN_FAULT_TYPE";
    }
}

void ubse_start_fault_monitoring(const char* memory_type, int32_t register_result)
{
    if (register_result != 0) {
        fprintf(stderr, "[MAIN] Failed to register fault handler, error: %d\n", register_result);
        return;
    }

    printf("[MAIN] Fault handler registered successfully\n");
    printf("[MAIN] Monitoring for %s memory fault events...\n", memory_type);
    printf("[MAIN] Press Ctrl+C to exit\n\n");

    while (g_ubse_running) {
        sleep(1);
    }

    printf("[MAIN] Shutting down...\n");
}

void ubse_signal_handler(int sig)
{
    (void)sig;
    g_ubse_running = 0;
    printf("\n[MAIN] Received shutdown signal, exiting...\n");
}

void ubse_setup_signal_handlers(void)
{
    if (signal(SIGINT, ubse_signal_handler) == SIG_ERR) {
        fprintf(stderr, "[MAIN] signal=SIGINT error=register_failed\n");
    }
    if (signal(SIGTERM, ubse_signal_handler) == SIG_ERR) {
        fprintf(stderr, "[MAIN] signal=SIGTERM error=register_failed\n");
    }
}

void ubse_print_fault_info(const char* memory_type, const char* name, uint64_t memid, ubs_mem_fault_type_t type)
{
    if (name == NULL) {
        fprintf(stderr, "[FAULT] Error: name is NULL\n");
        return;
    }

    printf("[FAULT] %s memory: name=%s, handleId=%lu, type=%s(%d)\n", memory_type, name, (unsigned long)memid,
           ubse_fault_type_to_string(type), type);
}