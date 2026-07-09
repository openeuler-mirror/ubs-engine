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
 * When UBSE_IT_XALARM_FIFO_PATH is set, xalarm_get_event reads from a FIFO
 * so the IT process can inject fault events that flow through the real
 * syssentry SentryEventListen → UbseRasHandler::NodeFaultHandle chain.
 *
 * FIFO message format:  <alarmId>|<paras>\n
 * Example:              1003|12345\n
 *
 * When UBSE_IT_XALARM_FIFO_PATH is not set, falls back to the original
 * behaviour (sleep 3600 and return -1) so existing tests are unaffected.
 */

#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef ALARM_INFO_MAX_PARAS_LEN
#define ALARM_INFO_MAX_PARAS_LEN 8192
#endif

#ifndef MAX_NUM_OF_ALARM_ID
#define MAX_NUM_OF_ALARM_ID 128
#endif

/* Max single FIFO line: alarmId(6) + '|' + paras(8192) + '\n' + '\0' */
constexpr int XALARM_FIFO_LINE_MAX = ALARM_INFO_MAX_PARAS_LEN + 16;

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

/**
 * Read a delimited line from fd into buf (up to maxSize-1 bytes).
 * Returns the number of bytes placed in buf (excluding NUL), or -1 on error.
 * The delimiter is consumed but NOT stored in buf.
 */
static int ReadLine(int fd, char* buf, int maxSize)
{
    int idx = 0;
    while (idx < maxSize - 1) {
        char ch = 0;
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            if (n == 0) {
                break; /* no more data */
            }
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (ch == '\n') {
            break;
        }
        buf[idx++] = ch;
    }
    buf[idx] = '\0';
    return idx;
}

extern "C" {

int xalarm_report_event(unsigned short usAlarmId, char* pucParas, size_t len)
{
    /* RAS handler完成后的ack回调，pucParas格式为 "msgId_retCode"
     * 写入文件供IT测试进程读取RAS处理结果
     * 文件路径: CWD/xalarm_ack_<alarmId> (daemon CWD = node work dir) */
    if (pucParas != nullptr && len > 0) {
        char ackPath[256];
        snprintf(ackPath, sizeof(ackPath), "./xalarm_ack_%u", usAlarmId);
        FILE* fp = fopen(ackPath, "w");
        if (fp != nullptr) {
            fwrite(pucParas, 1, len, fp);
            fputc('\n', fp);
            fclose(fp);
        }
    }
    return 0;
}

int xalarm_register_event(struct alarm_register** register_info, struct alarm_subscription_info id_filter)
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

    const char* fifoPath = getenv("UBSE_IT_XALARM_FIFO_PATH");
    if (fifoPath != nullptr && fifoPath[0] != '\0') {
        /* mkfifo is idempotent; ignore EEXIST */
        if (mkfifo(fifoPath, 0666) != 0 && errno != EEXIST) {
            /* FIFO creation failed, fall through to no-FIFO mode */
        } else {
            /*
             * Open O_RDWR on Linux: does NOT block, and keeps a write-end
             * reference so read() never sees a premature EOF when the
             * IT helper closes its write fd.
             */
            int fd = open(fifoPath, O_RDWR);
            if (fd >= 0) {
                reg->register_fd = fd;
            }
        }
    }

    *register_info = reg;
    return 0;
}

int xalarm_get_event(struct alarm_msg* msg, struct alarm_register* register_info)
{
    if (msg == nullptr || register_info == nullptr) {
        return -1;
    }

    int fd = register_info->register_fd;
    if (fd < 0) {
        /* No FIFO — original behaviour: long sleep then return -1 */
        sleep(3600);
        return -1;
    }

    /* Blocking read with 1-second select so the thread can react to
       xalarm_unregister_event closing the fd. */
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int sel = select(fd + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sel == 0) {
            /* Timeout — check if fd was closed (unregister) */
            if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
                return -1;
            }
            continue;
        }

        /* Data available */
        char lineBuf[XALARM_FIFO_LINE_MAX];
        int lineLen = ReadLine(fd, lineBuf, sizeof(lineBuf));
        if (lineLen <= 0) {
            continue;
        }

        /* Parse: <alarmId>|<paras> */
        char* pipePos = strchr(lineBuf, '|');
        if (pipePos == nullptr) {
            continue;
        }
        *pipePos = '\0';
        const char* alarmIdStr = lineBuf;
        const char* parasStr = pipePos + 1;

        errno = 0;
        unsigned long id = strtoul(alarmIdStr, nullptr, 10);
        if (errno != 0) {
            continue;
        }

        msg->usAlarmId = static_cast<unsigned short>(id);
        gettimeofday(&msg->AlarmTime, nullptr);
        size_t parasLen = strlen(parasStr);
        if (parasLen >= ALARM_INFO_MAX_PARAS_LEN) {
            parasLen = ALARM_INFO_MAX_PARAS_LEN - 1;
        }
        memcpy(msg->pucParas, parasStr, parasLen);
        msg->pucParas[parasLen] = '\0';
        return 0;
    }
}

void xalarm_unregister_event(struct alarm_register** register_info)
{
    if (register_info == nullptr || *register_info == nullptr) {
        return;
    }
    if ((*register_info)->register_fd >= 0) {
        close((*register_info)->register_fd);
        (*register_info)->register_fd = -1;
    }
    free(*register_info);
    *register_info = nullptr;
}

} // extern "C"
