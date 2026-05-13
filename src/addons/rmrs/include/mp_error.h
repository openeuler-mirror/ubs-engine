/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_ERROR_H
#define MP_ERROR_H

#include <cstdint>

using MEM_POOLING_RES = uint32_t;
using MpResult = uint32_t;
using VmResult = uint32_t;

/**
 * 描述:生成模块内的私有错误码。(模块ID + 模块内具体错误码 = 完整的错误码)
 * MID（高两个字节）＋ERRNO（低两个字节）
 * 如错误码：0x10081000
 * 高字节0x1008表示模块ID：即0x1000 + 8，表示communication子系统的net模块，可在本头文件找到MEM_POOLING_COMMUNICATION_MID_NET
 * 低字节0x1000表示错误类型：0x1000 + 0，表示模块内的私有错误码基础值 + 错误码，可在头文件MEM_POOLING_net_error.h中找到错误类型
 */
#define MEM_POOLING_ERROR_BEGIN_USER 0X1000 /* 模块内的私有错误码基础值 */
#define MEM_POOLING_ERROR_USERNO(n) (uint32_t(MEM_POOLING_ERROR_BEGIN_USER + (n))) /* 计算模块内的私有错误码加基础值 */
#define MEM_POOLING_MID_HI16(MID) (uint32_t((MID) << 16))                          /* 模块ID左移到高字节 */

/* 各个子系统，MID分段起始ID定义，各个模块定义时选择相应的起始ID */
#define MEM_POOLING_MID_BEGIN0 0x0000 /* common        */
#define MEM_POOLING_MID_BEGIN1 0x1000 /* Rack Manager  */

/* 宏定义各个子模块MID计算方法 */
#define MEM_POOLING_COMMON_ERROR(n) (uint32_t(MEM_POOLING_MID_BEGIN0 + (n)))     /* common        */
#define MEM_POOLING_MID_MAKE_MANAGER(n) (uint32_t(MEM_POOLING_MID_BEGIN1 + (n))) /* Rack Manager  */

#define MEM_POOLING_MASTER_MID_BASE MEM_POOLING_MID_MAKE_MANAGER(1) /* 0X1001 rack master base */
#define MEM_POOLING_MASTER_MID_VM MEM_POOLING_MID_MAKE_MANAGER(2)   /* 0X1002 rack master vm */
#define MEM_POOLING_MASTER_MID_MEM MEM_POOLING_MID_MAKE_MANAGER(3)  /* 0X1003 rack master mem */

#define MEM_POOLING_AGENT_MID_BASE MEM_POOLING_MID_MAKE_MANAGER(4) /* 0X1004 rack agent base */
#define MEM_POOLING_AGENT_MID_VM MEM_POOLING_MID_MAKE_MANAGER(5)   /* 0X1005 rack agent vm */
#define MEM_POOLING_AGENT_MID_MEM MEM_POOLING_MID_MAKE_MANAGER(6)  /* 0X1006 rack agent mem */

#define MEM_POOLING_CLOUD_ADAPTER_MID MEM_POOLING_MID_MAKE_MANAGER(7) /* 0X1007 cloud adapter */

#define MEM_POOLING_COMMUNICATION_MID_NET MEM_POOLING_MID_MAKE_MANAGER(8)  /* 0X1008 通信的net模块 */
#define MEM_POOLING_COMMUNICATION_MID_HTTP MEM_POOLING_MID_MAKE_MANAGER(9) /* 0X1009 通信的http模块 */

#define MEM_POOLING_DB_MID_LDC MEM_POOLING_MID_MAKE_MANAGER(10) /* 0X100A LDC模块 */

#define MEM_POOLING_SERIALIZE_MID_BASE MEM_POOLING_MID_MAKE_MANAGER(11) /* 0X100B 序列化模块 */

#define MEM_POOLING_MONITOR_MID_BASE MEM_POOLING_MID_MAKE_MANAGER(12) /* 0X100C 设备监听模块 */

/* ********************************************* */
/* common错误码定义，全局唯一，记录系统的标准错误返回 */
/* ********************************************* */

#define MEM_POOLING_ERROR_SIGN_INT (-1)                                   /* 错误, 有符号数-1 */
#define MEM_POOLING_OK MEM_POOLING_COMMON_ERROR(0)                        /* 正确 */
#define MEM_POOLING_ERROR MEM_POOLING_COMMON_ERROR(1)                     /* 错误 */
#define MEM_POOLING_MIGRATE_FAILED_VM_DELETED MEM_POOLING_COMMON_ERROR(2) /* SMAP迁移失败-迁移过程中虚机被删除 */
#define MEM_POOLING_MIGRATE_FAILED MEM_POOLING_COMMON_ERROR(3)            /* SMAP迁移失败通用错误码 */
#define MEM_POOLING_RESOURCE_DELETE MEM_POOLING_COMMON_ERROR(4)           /* UBSE删除内存资源失败 */
#define MEM_POOLING_ERROR_SRCH MEM_POOLING_COMMON_ERROR(5)                /* No such process */
#define MEM_POOLING_ERROR_EXIST MEM_POOLING_COMMON_ERROR(6)               /* File exists */
#define MEM_POOLING_ERROR_NOSPC MEM_POOLING_COMMON_ERROR(7)               /* No space left on device */
#define MEM_POOLING_ERROR_AGAIN MEM_POOLING_COMMON_ERROR(8)               /* Try again */
#define MEM_POOLING_ERROR_IO MEM_POOLING_COMMON_ERROR(9)                  /* I/O error */
#define MEM_POOLING_ERROR_BADF MEM_POOLING_COMMON_ERROR(10)               /* Bad file descriptor */
#define MEM_POOLING_ERROR_CONF_INVALID MEM_POOLING_COMMON_ERROR(11)        /* 非法的配置文件 */
#define MEM_POOLING_ERROR_NULLPTR MEM_POOLING_COMMON_ERROR(12)             /* 空指针 */
#define MEM_POOLING_MASTER_EMPTY_VECTOR_ERROR MEM_POOLING_COMMON_ERROR(13) /* 空数组 */
#define MEM_POOLING_ERROR_INVAL MEM_POOLING_COMMON_ERROR(14)               /* Invalid argument */
#define MEM_POOLING_ERROR_EXCEEDS_RANGE MEM_POOLING_COMMON_ERROR(15)       /* Out of range */
#define MEM_POOLING_WARN MEM_POOLING_COMMON_ERROR(16)                      /* 告警错误码 */
#define MEM_POOLING_TURBO_CONNECT_ERROR MEM_POOLING_COMMON_ERROR(17)       /* OSTURBO断连错误码 */
#define MEM_POOLING_FORCE_RETURN_OK MEM_POOLING_COMMON_ERROR(20)           /* 归还失败但强制归还成功 */
#define MEM_POOLING_SMAP_NOT_INIT_ERROR MEM_POOLING_COMMON_ERROR(21)       /* SMAP未初始化错误码 */
#define MEM_POOLING_SMAP_PARAM_ERROR MEM_POOLING_COMMON_ERROR(22)          /* SMAP参数错误错误码 */
#define MEM_POOLING_SMAP_PARTIAL_SUCCESS MEM_POOLING_COMMON_ERROR(23)      /* SMAP迁移时 */
#define MEM_POOLING_RMRS_MIGRATE_FAILED_VM_DELETED MEM_POOLING_COMMON_ERROR(24) /* SMAP迁移失败-迁移过程中虚机被删除 */

/* **************************************** */
/* http模块错误码定义                        */
/* **************************************** */

/* 0x10021000 rack master vm invalid strategy */
#define MEM_POOLING_INVALID_STRATEGY_ERROR \
    (MEM_POOLING_MID_HI16(MEM_POOLING_MASTER_MID_VM) | MEM_POOLING_ERROR_USERNO(0x00))

/* 0x10021001 rack master vm reclaim mem error */
#define MEM_POOLING_RECLAIM_MEMORY_ERROR \
    (MEM_POOLING_MID_HI16(MEM_POOLING_MASTER_MID_VM) | MEM_POOLING_ERROR_USERNO(0x01))

/* 0x10021002 rack master vm migrate error */
#define MEM_POOLING_MIGRATE_ERROR (MEM_POOLING_MID_HI16(MEM_POOLING_MASTER_MID_VM) | MEM_POOLING_ERROR_USERNO(0x02))

/* 0x10021003 rack master return memory invalid param error */
#define MEM_POOLING_INVALID_PARAM_ERROR \
    (MEM_POOLING_MID_HI16(MEM_POOLING_MASTER_MID_VM) | MEM_POOLING_ERROR_USERNO(0x03))

#define MEM_POOLING_RESULT_FAIL(ret) (static_cast<VmResult>(ret) != MEM_POOLING_OK)
#define MEM_POOLING_RESULT_OK(ret) (static_cast<VmResult>(ret) == MEM_POOLING_OK)

#define RETURN_FAIL_AS_NULLPTR(ptr)                                             \
    do {                                                                        \
        if (MEM_POOLING_UNLIKELY((ptr) == nullptr)) {                           \
            UBSE_LOGGER_ERROR(MEM_POOLING_MODULE_NAME, MEM_POOLING_MODULE_CODE) \
                << "Unexpected nullptr value " << std::string(#ptr).c_str();    \
            return MEM_POOLING_ERROR_NULLPTR;                                   \
        }                                                                       \
    } while (0)

/* **************************************** */
/* 序列化模块错误码定义                       */
/* **************************************** */

/* 0x100B1000  序列化错误 */
#define MEM_POOLING_ERROR_SERIALIZE_ERROR \
    (MEM_POOLING_MID_HI16(MEM_POOLING_SERIALIZE_MID_BASE) | MEM_POOLING_ERROR_USERNO(0x00))

/* 0x100B1001 反序列化错误 */
#define MEM_POOLING_ERROR_DESERIALIZE_ERROR \
    (MEM_POOLING_MID_HI16(MEM_POOLING_SERIALIZE_MID_BASE) | MEM_POOLING_ERROR_USERNO(0x01))

/* 0x100B1002 序列化反序列化公共错误 */
#define MEM_POOLING_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR \
    (MEM_POOLING_MID_HI16(MEM_POOLING_SERIALIZE_MID_BASE) | MEM_POOLING_ERROR_USERNO(0x02))

#endif // MP_ERROR_H
