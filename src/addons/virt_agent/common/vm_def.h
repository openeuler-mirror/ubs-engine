/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_DEF_H
#define VM_DEF_H

#include <cstdint>

namespace vm {
constexpr uint32_t VM_MODIFY_HUGEPAGE_ID = 11; // VM Hugepage Server ID Change
constexpr uint32_t HAM_MIGRATE_ID = 14;        // Dedicated for socket communication with libvirt, cannot be changed
constexpr uint32_t HAM_MIGRATE_CANCEL = 15;    // Invoke the libvirt interface to cancel deterministic migration
constexpr uint32_t HAM_MIGRATE_MESSAGE_TO_MASTER = 16; // send pid check message to master
constexpr uint32_t HAM_MIGRATE_MESSAGE_TO_AGENT = 17;  // send pid check message to agent
constexpr uint32_t MAX_NUMA_NUM = 512;                 // 512 for max numa number;
constexpr uint32_t MAX_VM_NUM = 300;                   // 300 for max VM number;
constexpr uint32_t MAX_BORROW_ID_COUNT = 2000;
constexpr uint32_t MAX_BORROW_ID_LENGTH = 128;
constexpr uint32_t MAX_NODE_NUM = 32;
constexpr uint32_t MAX_DEST_PARAM_SIZE = MAX_BORROW_ID_COUNT;
constexpr uint32_t MAX_DEST_NUMA_NUM = 1;
constexpr uint32_t MAX_UUID_LENGTH = 37;
constexpr int MB_TO_BYTES = 1048576;
constexpr int GB_TO_BYTES = 1073741824;

// Numeric constant definitions representing the values 0, 1, 2, 3, etc.
constexpr int16_t NO_0 = 0;
constexpr int16_t NO_1 = 1;
constexpr int16_t NO_2 = 2;
constexpr int16_t NO_3 = 3;
constexpr int16_t NO_4 = 4;
constexpr int16_t NO_8 = 8;
constexpr int16_t NO_16 = 16;
constexpr int16_t NO_32 = 32;
constexpr int16_t NO_64 = 64;
constexpr int16_t NO_128 = 128;
constexpr int16_t NO_1000 = 1000;
constexpr int16_t NO_1024 = 1024;
constexpr int16_t NO_2048 = 2048;
constexpr int32_t NO_60000 = 60000;
constexpr int16_t NO_NEGATIVE_1 = -1;
constexpr int MAX_IPC_DATA_PACKAGE_LEN = 10485760;
constexpr int16_t DUMP_DELETE_LEN = -1;
constexpr uint64_t BYTE2KB = 10;
constexpr uint64_t BYTE2MB = 20;
constexpr uint64_t BYTE2GB = 30;
} // namespace vm

#endif // VM_DEF_H
