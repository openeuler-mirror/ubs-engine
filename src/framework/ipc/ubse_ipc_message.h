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

#ifndef UBSE_IPC_MESSAGE_H
#define UBSE_IPC_MESSAGE_H

#include <cstdint>

const uint32_t UBSE_MESSAGE_SIZE = 20 * 1024; // 最大消息长度 10k

#pragma pack(push, 1) // Ensure byte-aligned packing for cross-platform compatibility

/**
 * @brief Request header structure for network communication
 */
struct UbseRequestHeader {
    uint16_t moduleCode; // Module identifier code
    uint16_t opCode;     // Operation command code
    uint32_t bodyLen;    // Length of request body
};

/**
 * @brief Response header structure for network communication
 */
struct UbseResponseHeader {
    uint32_t statusCode; // Operation status code (0 = Success, non-zero = Error code)
    uint32_t bodyLen;    // Length of response body in bytes (0 if no body exists)
};

#pragma pack(pop) // Restore original packing alignment

/**
 * @brief Complete request message container
 * @warning Call freeFunc() to release body memory after processing
 */
struct UbseRequestMessage {
    UbseRequestHeader header; // Request header metadata
    uint8_t *body;            // Pointer to request payload (nullable if bodyLen=0)
    void (*freeFunc)(void *); /* Memory deallocator for body (MUST match allocation method)
                               * - Use free() for malloc/calloc
                               * - Use delete[] for C++ new[]
                               */
};

/**
 * @brief Complete response message container
 * @warning Call freeFunc() to release body memory after processing
 */
struct UbseResponseMessage {
    UbseResponseHeader header; // Response header metadata
    uint8_t *body;             // Pointer to response payload (nullable if bodyLen=0)
    void (*freeFunc)(void *);  /* Memory deallocator for body (MUST match allocation method)
                                * - Use free() for malloc/calloc
                                * - Use delete[] for C++ new[]
                                */
};

struct UbseClientInfo {
    uid_t uid; // user id
    gid_t gid; // group id
    pid_t pid; // process id
};

struct UbseRequestContext {
    UbseClientInfo clientInfo;
    uint64_t requestId;
    uint64_t timestamp;
};
#endif // UBSE_IPC_MESSAGE_H
