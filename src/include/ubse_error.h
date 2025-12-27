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

#ifndef UBSE_ERROR_H
#define UBSE_ERROR_H
#include <cstdint>

/**
 * 描述:生成模块内的私有错误码。(模块ID + 模块内具体错误码 = 完整的错误码)
 * MID（高两个字节）＋ERRNO（低两个字节）
 * 如错误码：0x10081000
 * 高字节0x1008表示模块ID：即0x1000 + 8，表示communication子系统的net模块，可在本头文件找到UBSE_COMMUNICATION_MID_NET
 * 低字节0x1000表示错误类型：0x1000 + 0，表示模块内的私有错误码基础值 + 错误码，可在头文件ubse_net_error.h中找到错误类型
 */
#define UBSE_ERROR_BEGIN_USER 0X1000                                 /* 模块内的私有错误码基础值 */
#define UBSE_ERROR_USERNO(n) (uint32_t(UBSE_ERROR_BEGIN_USER + (n))) /* 计算模块内的私有错误码加基础值 */
#define UBSE_MID_HI16(MID) (uint32_t((MID) << 16))                   /* 模块ID左移到高字节 */

/* 各个子系统，MID分段起始ID定义，各个模块定义时选择相应的起始ID */
#define UBSE_MID_BEGIN0 0x0000      /* common     */
#define UBSE_MID_BEGIN1 0x1000      /* Ubs_Engine */
#define UBSE_MID_BEGIN2 0x2000      /* Ubse CLI   */
#define UBSE_MID_BEGIN3 0x3000      /* Plugin     */

/* 宏定义各个子模块MID计算方法 */
#define UBSE_COMMON_ERROR(n) (uint32_t(UBSE_MID_BEGIN0 + (n)))       /* common     */
#define UBSE_MID_MAKE_MANAGER(n) (uint32_t(UBSE_MID_BEGIN1 + (n)))   /* Ubs_Engine */
#define UBSE_MID_MAKE_CLI(n) (uint32_t(UBSE_MID_BEGIN2 + (n)))       /* Ubse CLI   */
#define UBSE_MID_PLUGIN(n) (uint32_t(UBSE_MID_BEGIN3 + (n)))         /* Plugin     */

/* ********************************************** */
/* common错误码定义，全局唯一，记录系统的标准错误返回 */
/* ********************************************** */
#define UBSE_OK UBSE_COMMON_ERROR(0)                         /* 正确 */
#define UBSE_ERROR UBSE_COMMON_ERROR(1)                      /* 错误 */
#define UBSE_ERROR_NOENT UBSE_COMMON_ERROR(2)                /* No such file or directory */
#define UBSE_ERROR_NOMEM UBSE_COMMON_ERROR(3)                /* Out of memory */
#define UBSE_ERROR_ACCES UBSE_COMMON_ERROR(4)                /* Permission denied */
#define UBSE_ERROR_SRCH UBSE_COMMON_ERROR(5)                 /* No such process */
#define UBSE_ERROR_EXIST UBSE_COMMON_ERROR(6)                /* File exists */
#define UBSE_ERROR_NOSPC UBSE_COMMON_ERROR(7)                /* No space left on device */
#define UBSE_ERROR_AGAIN UBSE_COMMON_ERROR(8)                /* Try again */
#define UBSE_ERROR_IO UBSE_COMMON_ERROR(9)                   /* I/O error */
#define UBSE_ERROR_BADF UBSE_COMMON_ERROR(10)                /* Bad file descriptor */
#define UBSE_ERROR_CONF_INVALID UBSE_COMMON_ERROR(11)        /* 非法的配置文件 */
#define UBSE_ERROR_NULLPTR UBSE_COMMON_ERROR(12)             /* 空指针 */
#define UBSE_MASTER_EMPTY_VECTOR_ERROR UBSE_COMMON_ERROR(13) /* 空数组 */
#define UBSE_ERROR_INVAL UBSE_COMMON_ERROR(14)               /* Invalid argument */
#define UBSE_ERROR_MODULE_LOAD_FAILED UBSE_COMMON_ERROR(15)  /* 模块加载失败 */
#define UBSE_ERROR_CLI_ARGS_FAILED UBSE_COMMON_ERROR(16)     /* 解析参数失败 */
#define UBSE_ERROR_SERIALIZE_FAILED UBSE_COMMON_ERROR(17)    /* 序列化失败 */
#define UBSE_ERROR_DESERIALIZE_FAILED UBSE_COMMON_ERROR(18)  /* 反序列化失败 */
#define UBSE_ERROR_NULL_INFO UBSE_COMMON_ERROR(19)           /* 业务信息为空 */

/* *************************************** */
/* 各个模块MID定义                          */
/* 0:基础模块                               */
/* 1:com模块                                */
/* 2:config模块                             */
/* 3:event模块                              */
/* 4:http模块                               */
/* 5:log模块                                */
/* 6:message模块                            */
/* 7:plugin模块                             */
/* 8:role模块                               */
/* 9:storage模块                            */
/* 11:资源管理框架                           */
/* 30:kmc加解密模块                          */
/* **************************************** */
#define UBSE_MANAGER_MID_BASE UBSE_MID_MAKE_MANAGER(0)       /* 0X1000 基础模块 */

#define UBSE_COM_MID UBSE_MID_MAKE_MANAGER(1)                /* 0X1001 */
#define UBSE_CONF_MID UBSE_MID_MAKE_MANAGER(2)               /* 0X1002 */
#define UBSE_EVENT_MID UBSE_MID_MAKE_MANAGER(3)              /* 0X1003 */
#define UBSE_HTTP_MID UBSE_MID_MAKE_MANAGER(4)               /* 0X1004 */
#define UBSE_LOG_MID UBSE_MID_MAKE_MANAGER(5)                /* 0X1005 */
#define UBSE_MESSAGE_MID UBSE_MID_MAKE_MANAGER(6)            /* 0X1006 */
#define UBSE_PLUGIN_MID UBSE_MID_MAKE_MANAGER(7)             /* 0X1007 */
#define UBSE_ROLE_MID UBSE_MID_MAKE_MANAGER(8)               /* 0X1008 */
#define UBSE_STORAGE_MID UBSE_MID_MAKE_MANAGER(9)            /* 0X1009 */
#define UBSE_RESOURCE_MGR_MID UBSE_MID_MAKE_MANAGER(11)      /* 0X100B */
#define UBSE_DEV_MID UBSE_MID_MAKE_MANAGER(12)               /* 0X100C */
#define UBSE_UTILS_MID UBSE_MID_MAKE_MANAGER(13)             /* 0X100D */
#define UBSE_PARSE_MID UBSE_MID_MAKE_MANAGER(14)             /* 0X100E */
#define UBSE_SYSTEM_MID UBSE_MID_MAKE_MANAGER(15)            /* 0X100F */
#define UBSE_TASK_EXECUTOR_MID UBSE_MID_MAKE_MANAGER(16)     /* 0X1010 */
#define UBSE_DBG_DEADLOOP_MID UBSE_MID_MAKE_MANAGER(17)      /* 0X1011 */
#define UBSE_SDK_REGISTER_MID UBSE_MID_MAKE_MANAGER(18)      /* 0X1012 */

#define UBSE_DBG_TRACE_MID UBSE_MID_MAKE_MANAGER(18)         /* 0X1012 */
#define UBSE_NODE_MID UBSE_MID_MAKE_MANAGER(19)              /* 0X1013 */
#define UBSE_DBG_EXCEPTION_MID UBSE_MID_MAKE_MANAGER(20)     /* 0X1014 */
#define UBSE_DBG_MEMORY_MID UBSE_MID_MAKE_MANAGER(21)        /* 0X1015 */
#define UBSE_FAULT_COLLECTION_MID UBSE_MID_MAKE_MANAGER(22)  /* 0X1016 */
#define UBSE_REMOTE_MID UBSE_MID_MAKE_MANAGER(23)            /* 0X1017 */
#define UBSE_LCNE_MID UBSE_MID_MAKE_MANAGER(24)              /* 0X1018 */
#define UBSE_RPC_MID UBSE_MID_MAKE_MANAGER(25)               /* 0X1019 */
#define UBSE_DRIVER_MID UBSE_MID_MAKE_MANAGER(26)            /* 0X101A */
#define UBSEK_PLUGIN_PROXY_MID UBSE_MID_MAKE_MANAGER(27)     /* 0X101B */
#define UBSE_ELECTION_MID UBSE_MID_MAKE_MANAGER(28)          /* 0X101C */
#define UBSE_DATA_SYNC_MID UBSE_MID_MAKE_MANAGER(29)         /* 0X101D */
#define UBSE_KMC_CRYPT_MID UBSE_MID_MAKE_MANAGER(30)         /* 0X101E */
#define UBSE_PSK_MID UBSE_MID_MAKE_MANAGER(30)               /* 0X101F */
#define UBSE_OBJ_MID UBSE_MID_MAKE_MANAGER(31)               /* 0X1020 */
#define UBSE_JOB_MID UBSE_MID_MAKE_MANAGER(32)               /* 0X1021 */
#define UBSE_MMI_MID UBSE_MID_MAKE_MANAGER(33)               /* 0X1022 */
#define UBSE_MTI_MID UBSE_MID_MAKE_MANAGER(34)               /* 0X1023 */
#define UBSE_CONTROLLER_MID UBSE_MID_MAKE_MANAGER(35)        /* 0X1024 */
#define UBSE_RAS UBSE_MID_MAKE_MANAGER(36)                   /* 0X1025 */
#define SYS_SENTRY UBSE_MID_MAKE_MANAGER(37)                 /* 0X1024 */
#define UBSE_NODE_CONTROLLER_MID UBSE_MID_MAKE_MANAGER(38)   /* 0X1027 */
#define UBSE_API_SERVER_MID UBSE_MID_MAKE_MANAGER(39)        /* 0X1027 */
#define UBSE_SECURITY_MID UBSE_MID_MAKE_MANAGER(40)          /* 0X1028 */
#define UBSE_URMA_CONTROLLER_MID UBSE_MID_MAKE_MANAGER(41)   /* 0X1029 */
#define UBSE_URMA_MID UBSE_MID_MAKE_MANAGER(42)              /* 0X1029 */

/* Ubse CLI */
#define UBSE_CLI_MID_BASE UBSE_MID_MAKE_CLI(0)               /* 0X2000 基础模块 */
#define UBSE_CLI_MID_PARSE UBSE_MID_MAKE_CLI(1)              /* 0X2001 */
#define UBSE_CLI_MID_SDK UBSE_MID_MAKE_CLI(2)                /* 0X2002 */
#define UBSE_CLI_MID_REG UBSE_MID_MAKE_CLI(3)                /* 0X2003 */
#define UBSE_CLI_MID_ECHO UBSE_MID_MAKE_CLI(4)               /* 0X2004 */

/* Plugin */
#define VM_MID_BASE UBSE_PLUGIN_MID(1)                       /* 0x3001 虚拟化插件 */

/* 公共方法判断错误码 */
#define UBSE_RESULT_FAIL(ret) (static_cast<UbseResult>(ret) != UBSE_OK)
#define UBSE_RESULT_OK(ret) (static_cast<UbseResult>(ret) == UBSE_OK)

#endif // UBSE_ERROR_H
