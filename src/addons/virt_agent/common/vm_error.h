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

#ifndef VM_ERROR_H
#define VM_ERROR_H

#include <cstdint>

namespace vm {
using VmResult = uint32_t;

/**
 * @brief Generate private error codes within a module.
 * The complete error code is formed by combining the Module ID and the specific error code within the module
 * Format: MID (high two bytes) + ERRNO (low two bytes)
 * Example: Error code 0x10081000
 * - High bytes 0x1008 represent the module ID: 0x1000 + 8, where 8 is the module index of 'migrate' subsystem
 *   This can be found in this header file as vm_error.h
 * - Low bytes 0x1000 represent the error code: 0x1000 + 0, where 0 is the specific error code within the module
 *   This can be found in the header file vm_error.h
 */
// Define the base value for private error codes within the module
constexpr uint32_t VM_ERROR_BEGIN_USER = 0x1000;

// Generate the specific error code within the module (base_value + n)
inline uint32_t VmErrorUserNo(uint32_t n)
{
    return VM_ERROR_BEGIN_USER + n;
}

// Shift the module ID left to the high 16 bits
constexpr uint32_t BYTE_NUM16 = 16;
inline uint32_t VmMidHi16(uint32_t mid)
{
    return mid << BYTE_NUM16;
}

// For each subsystem, define the starting MID segment ID;
// when defining each module, select the corresponding starting ID.
constexpr int VM_MID_BEGIN0 = 0x0000; // common
constexpr int VM_MID_BEGIN1 = 0x1000; // Ubse Manager

/* 函数定义各个子模块MID计算方法 */
inline uint32_t VmCommonError(uint32_t n)
{
    return VM_MID_BEGIN0 + n;
}

inline uint32_t VmMidMakeManager(uint32_t n)
{
    return VM_MID_BEGIN1 + n;
}

#define VM_MASTER_MID_BASE VmMidMakeManager(1) /* 0X1001 ubse master base */
#define VM_MASTER_MID_VM VmMidMakeManager(2)   /* 0X1002 ubse master vm */
#define VM_MASTER_MID_MEM VmMidMakeManager(3)  /* 0X1003 ubse master mem */

#define VM_AGENT_MID_BASE VmMidMakeManager(4) /* 0X1004 ubse agent base */
#define VM_AGENT_MID_VM VmMidMakeManager(5)   /* 0X1005 ubse agent vm */
#define VM_AGENT_MID_MEM VmMidMakeManager(6)  /* 0X1006 ubse agent mem */

#define VM_CLOUD_ADAPTER_MID VmMidMakeManager(7) /* 0X1007 cloud adapter */

#define VM_COMMUNICATION_MID_NET VmMidMakeManager(8)  /* 0X1008 net module for communication */
#define VM_COMMUNICATION_MID_HTTP VmMidMakeManager(9) /* 0X1009 http module for communication */

#define VM_DB_MID_LDC VmMidMakeManager(10) /* 0X100A LDC module */

#define VM_SERIALIZE_MID_BASE VmMidMakeManager(11) /* 0X100B serialization module */

#define VM_MONITOR_MID_BASE VmMidMakeManager(12) /* 0X100C device listening module */

/* *********************************************************************************************** */
/* Common error code definition, globally unique, records the standard error returns of the system */
/* *********************************************************************************************** */

constexpr int VM_ERROR_SIGN_INT = -1;                  /* Error, signed number -1 */
#define VM_OK VmCommonError(0)                         /* correct */
#define VM_ERROR VmCommonError(1)                      /* error */
#define VM_ERROR_NOENT VmCommonError(2)                /* No such file or directory */
#define VM_ERROR_NOMEM VmCommonError(3)                /* Out of memory */
#define VM_ERROR_ACCES VmCommonError(4)                /* Permission denied */
#define VM_ERROR_SRCH VmCommonError(5)                 /* No such process */
#define VM_ERROR_EXIST VmCommonError(6)                /* File exists */
#define VM_ERROR_NOSPC VmCommonError(7)                /* No space left on device */
#define VM_ERROR_AGAIN VmCommonError(8)                /* Try again */
#define VM_ERROR_IO VmCommonError(9)                   /* I/O error */
#define VM_ERROR_BADF VmCommonError(10)                /* Bad file descriptor */
#define VM_ERROR_CONF_INVALID VmCommonError(11)        /* Invalid configuration file */
#define VM_ERROR_NULLPTR VmCommonError(12)             /* Nullptr */
#define VM_MASTER_EMPTY_VECTOR_ERROR VmCommonError(13) /* Empty vector */
#define VM_ERROR_INVAL VmCommonError(14)               /* Invalid argument */
#define VM_ERROR_EXCEEDS_RANGE VmCommonError(15)       /* Out of range */
#define VM_WARN VmCommonError(16)                      /* Alarm error code */

/* **************************************** */
/* HTTP Module Error Code Definitions       */
/* **************************************** */

/* 0x10021000 ubse master vm invalid strategy */
#define VM_INVALID_STRATEGY_ERROR (VmMidHi16(VM_MASTER_MID_VM) | VmErrorUserNo(0x00))

/* 0x10021001 ubse master vm reclaim mem error */
#define VM_RECLAIM_MEMORY_ERROR (VmMidHi16(VM_MASTER_MID_VM) | VmErrorUserNo(0x01))

/* 0x10021002 ubse master vm migrate error */
#define VM_MIGRATE_ERROR (VmMidHi16(VM_MASTER_MID_VM) | VmErrorUserNo(0x02))

/* 0x10021003 ubse master return memory invalid param error */
#define VM_INVALID_PARAM_ERROR (VmMidHi16(VM_MASTER_MID_VM) | VmErrorUserNo(0x03))

inline bool VmResultFail(uint32_t ret)
{
    return static_cast<VmResult>(ret) != VM_OK;
}

inline bool VmResultOk(uint32_t ret)
{
    return static_cast<VmResult>(ret) == VM_OK;
}

/* **************************************** */
/* Serialization Module Error Code Definitions */
/* **************************************** */

// 0x100B1000  Serialization error
#define VM_ERROR_SERIALIZE_ERROR (VmMidHi16(VM_SERIALIZE_MID_BASE) | VmErrorUserNo(0x00))

// 0x100B1001 Deserialization error
#define VM_ERROR_DESERIALIZE_ERROR (VmMidHi16(VM_SERIALIZE_MID_BASE) | VmErrorUserNo(0x01))

// 0x100B1002 common error for serialization and deserialization
#define VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR (VmMidHi16(VM_SERIALIZE_MID_BASE) | VmErrorUserNo(0x02))

} // namespace vm
#endif // VM_ERROR_H
