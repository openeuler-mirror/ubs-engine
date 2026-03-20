/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef TURBO_DEF_H
#define TURBO_DEF_H

#define IPC_OK 0
#define IPC_ERROR 1
#define IPC_BAD_SOCKET 2
#define IPC_BAD_CONNECT 3
#define IPC_NO_FUNC 4
#define IPC_FUNC_ERROR 5
#define IPC_INVALID_RESULT 6

#include <cstdint>
#include <functional>

using TurboByteBufferFreeFunc = std::function<void(uint8_t *data)>;

struct TurboByteBuffer {
    uint8_t *data = nullptr;  // 数据指针
    size_t len = 0;   // 数据长度
    TurboByteBufferFreeFunc freeFunc;  // 非空代表接收方需要释放内存；空代表接收方不需要释放内存
};

using IpcHandlerFunc = std::function<uint32_t(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)>;

#endif