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

#ifndef UBSE_MANAGER_UBSE_RAS_ERROR_H
#define UBSE_MANAGER_UBSE_RAS_ERROR_H
#include "ubse_error.h"

/* 0x10241000 消息重复 */
#define RAS_ERROR_MSG_DUPLICATION (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x00))

/* 0x10241001 消息发送失败 */
#define RAS_ERROR_MSG_SEND_BY_JOB (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x01))

/* 0x10241002 该节点不是master或mem未就绪 */
#define RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x02))

/* 0x10241003 处理msgId/eid/cna消息失败 */
#define RAS_PANIC_REBOOT_MSG_INVALID (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x03))

/* 0x10241004 未根据eid查询到NodeId */
#define RAS_ERROR_QUERY_NODE_BY_EID (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x04))

/* 0x10241005 dlopen失败 */
#define RAS_ERROR_DLOPEN_XALARMD (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x05))

/* 0x10241006 dlsym失败 */
#define RAS_ERROR_DLSYM_XALARMD (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x06))

/* 0x10241007 dlsym失败 */
#define RAS_ERROR_REPORT_TO_XALARMD (UBSE_MID_HI16(UBSE_RAS) | UBSE_ERROR_USERNO(0x07))

#endif // UBSE_MANAGER_UBSE_RAS_ERROR_H
