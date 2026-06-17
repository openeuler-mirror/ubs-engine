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

/**
 * @file xalarm_stub.cpp
 * @brief Stub implementation of xalarm system alarm library for IT testing.
 *
 * This shared library replaces libxalarm.so when running IT tests.
 * - xalarm_register_event: allocates an empty alarm_register struct
 * - xalarm_get_event: returns -1 (no events available)
 * - xalarm_unregister_event: frees the allocated alarm_register struct
 * - xalarm_report_event: returns 0 (success, no actual alarm sent)
 */

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <stddef.h>

#ifndef ALARM_INFO_MAX_PARAS_LEN
#define ALARM_INFO_MAX_PARAS_LEN 8192
#endif

#ifndef MAX_NUM_OF_ALARM_ID
#define MAX_NUM_OF_ALARM_ID 128
#endif

struct alarm_subscription_info {
    int id_list[MAX_NUM_OF_ALARM_ID];
    unsigned int len;
};

struct alarm_msg {
    unsigned short usAlarmId;
    struct timeval AlarmTime;
    char pucParas[ALARM_INFO_MAX_PARAS_LEN];
};

struct alarm_register {
    int register_fd;
    char alarm_enable_bitmap[MAX_NUM_OF_ALARM_ID];
};

extern "C" {

int xalarm_report_event(unsigned short usAlarmId, char* pucParas, size_t len)
{
    return 0;
}

int xalarm_register_event(struct alarm_register** register_info,
                           struct alarm_subscription_info id_filter)
{
    if (register_info == nullptr) {
        return -1;
    }
    struct alarm_register* reg = static_cast<alarm_register*>(calloc(1, sizeof(alarm_register)));
    if (reg == nullptr) {
        return -1;
    }
    memset(reg, 0, sizeof(alarm_register));
    reg->register_fd = -1;
    *register_info = reg;
    return 0;
}

int xalarm_get_event(struct alarm_msg* msg, struct alarm_register* register_info)
{
    if (msg == nullptr || register_info == nullptr) {
        return -1;
    }
    // Real xalarm_get_event is blocking. Sleep here so the production
    // SentryEventListen loop does not spin and repeatedly re-register in IT.
    sleep(3600);
    return -1;
}

void xalarm_unregister_event(struct alarm_register** register_info)
{
    if (register_info == nullptr || *register_info == nullptr) {
        return;
    }
    free(*register_info);
    *register_info = nullptr;
}

}
