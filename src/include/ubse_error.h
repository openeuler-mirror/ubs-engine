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

/*
 * UBSE错误码转换规则：
 * 小于10000的错误码为对外暴露的错误码，新增需同步至ubs_error.h
 * 内部错误码从10000起始，对外统一返回内部错误。
 */
#define UBSE_INTERNAL_ERROR_BASE 10000
#define UBSE_ERROR_DEF(n) (uint32_t(n))
#define UBSE_INTERNAL_ERROR_DEF(n) (uint32_t(UBSE_INTERNAL_ERROR_BASE + (n)))

/* ************************************************************* */
/*                  成功场景统一返回UBSE_OK，值为0                   */
/* ************************************************************* */
#define UBSE_OK UBSE_ERROR_DEF(0) /* 正确 */

/* ************************************************************* */
/* 对外暴露的错误码定义，全局唯一，范围1~9999，需要暴露给sdk时使用以下错误码 */
/* ************************************************************* */

/* ====================== 参数错误 (1-9) ====================== */
#define UBSE_ERR_INVALID_ARG UBSE_ERROR_DEF(1)      /* 无效参数 */
#define UBSE_ERR_BUFFER_TOO_SMALL UBSE_ERROR_DEF(4) /* 缓冲区不足 */

/* ====================== 资源错误 (10-19) ====================== */
#define UBSE_ERR_OUT_OF_MEMORY UBSE_ERROR_DEF(10)      /* 内存不足 */
#define UBSE_ERR_RESOURCE_BUSY UBSE_ERROR_DEF(11)      /* 资源忙 */
#define UBSE_ERR_RESOURCE_EXHAUSTED UBSE_ERROR_DEF(12) /* 资源耗尽 */
#define UBSE_ERR_QUOTA_EXCEEDED UBSE_ERROR_DEF(13)     /* 配额超出 */

/* ====================== IPC通信错误 (20-29) ====================== */
#define UBSE_ERR_IPC_CONNECTION_FAILED UBSE_ERROR_DEF(20)             /* IPC连接失败 */
#define UBSE_ERR_IPC_TIMEOUT UBSE_ERROR_DEF(21)                       /* IPC超时 */
#define UBSE_ERR_IPC_SERVICE_UNAVAILABLE UBSE_ERROR_DEF(22)           /* 服务不可用 */
#define UBSE_ERR_IPC_CONNECTION_FAILED_PATH_LENGTH UBSE_ERROR_DEF(23) /* 由于socket path长度导致IPC连接失败 */

/* ====================== 权限错误 (30-39) ====================== */
#define UBSE_ERR_PERMISSION_DENIED UBSE_ERROR_DEF(30)     /* 权限不足 */
#define UBSE_ERR_AUTHENTICATION_FAILED UBSE_ERROR_DEF(31) /* 认证失败 */
#define UBSE_ERR_ACCESS_DENIED UBSE_ERROR_DEF(32)         /* 访问被拒 */

/* ====================== 操作错误 (40-49) ====================== */
#define UBSE_ERR_NOT_IMPLEMENTED UBSE_ERROR_DEF(40)  /* 功能未实现 */
#define UBSE_ERR_NOT_SUPPORTED UBSE_ERROR_DEF(41)    /* 不支持的操作 */
#define UBSE_ERR_OPERATION_FAILED UBSE_ERROR_DEF(42) /* 操作失败 */
#define UBSE_ERR_TIMED_OUT UBSE_ERROR_DEF(43)        /* 操作超时 */

/* ====================== 守护进程错误 (50-59) ====================== */
#define UBSE_ERR_DAEMON_UNREACHABLE UBSE_ERROR_DEF(50) /* 守护进程不可达 */
#define UBSE_ERR_DAEMON_BUSY UBSE_ERROR_DEF(51)        /* 守护进程忙 */
#define UBSE_ERR_DAEMON_CRASHED UBSE_ERROR_DEF(52)     /* 守护进程崩溃 */
#define UBSE_ERR_DAEMON_INTERNEL UBSE_ERROR_DEF(53)    /* 守护进程内部错误 */

/* ====================== UBSE错误码 (1000-1099) ====================== */
#define UBSE_ERR_OUT_OF_RANGE UBSE_ERROR_DEF(1000)      /* 参数超范围 */
#define UBSE_ERR_RESOURCE UBSE_ERROR_DEF(1001)          /* 资源申请错误 */
#define UBSE_ERR_CONNECTION_FAILED UBSE_ERROR_DEF(1002) /* 连接UBSE服务端错误 */
#define UBSE_ERR_AUTH_FAILED UBSE_ERROR_DEF(1003)       /* UBSE服务端认证鉴权不通过 */
#define UBSE_ERR_TIMEOUT UBSE_ERROR_DEF(1004)           /* UBSE服务端处理超时 */
#define UBSE_ERR_INTERNAL UBSE_ERROR_DEF(1005)          /* UBSE服务端内部错误 */
#define UBSE_ERR_EXISTED UBSE_ERROR_DEF(1006)           /* 实例已存在 */
#define UBSE_ERR_NOT_EXIST UBSE_ERROR_DEF(1007)         /* 实例不存在 */
#define UBSE_ERR_UDSINFO_MISMATCH UBSE_ERROR_DEF(1008)  /* UDS INFO信息不匹配 */
#define UBSE_ERR_IMPORT_ABSENT UBSE_ERROR_DEF(1009)     /* IMPORT不在位 */
#define UBSE_ERR_CREATING UBSE_ERROR_DEF(1010)          /* 正在创建过程中 */
#define UBSE_ERR_DELETING UBSE_ERROR_DEF(1011)          /* 正在删除过程中 */
#define UBSE_ERR_UNIMPORT_SUCCESS \
    UBSE_ERROR_DEF(1012) /* unimport已经成功, unexport失败, 资源没有释放完全, 后续对账能自动回收 */
#define UBSE_ERR_ALLOCATE UBSE_ERROR_DEF(1013)         /* 算法分配失败 */
#define UBSE_ERR_SHM_NO_CREATE UBSE_ERROR_DEF(1014)    /* 共享内存未创建 */
#define UBSE_ERR_SHM_NO_ATTACH UBSE_ERROR_DEF(1015)    /* 共享内存未导入 */
#define UBSE_ERR_SHM_ATTACHING UBSE_ERROR_DEF(1016)    /* 正在导入共享内存过程中 */
#define UBSE_ERR_SHM_DETACHING UBSE_ERROR_DEF(1017)    /* 正在删除导入共享内存过程中 */
#define UBSE_ERR_LINK_NOT_ALLOWED UBSE_ERROR_DEF(1018) /* 非poc组网不允许指定链路;poc组网必须指定链路 */
#define UBSE_ERR_LINK_NOT_EXIST UBSE_ERROR_DEF(1019)   /* 链路不存在 */
#define UBSE_ERR_SHM_NODE_EMPTY UBSE_ERROR_DEF(1020)   /* 参数节点信息为空 */
#define UBSE_ERR_COM_FAILED UBSE_ERROR_DEF(1021)       /* Ubse通信失败 */
#define UBSE_ERR_FIND_SRC_NUMA UBSE_ERROR_DEF(1022)    /* 指定链路借用无法填充srcNuma */
#define UBSE_ERR_SHM_DESTROYED UBSE_ERROR_DEF(1023)    /* 共享内存被主动清理 */
#define UBSE_ERR_SHM_ATTACH_USING UBSE_ERROR_DEF(1024)             /* 共享内存正在被使用无法被删除 */
#define UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL UBSE_ERROR_DEF(1025) /* 指定CPU平面参数异常 */
#define UBSE_ERR_NUMA_ID_IS_NOT_IN_SOCKET UBSE_ERROR_DEF(1026)     /* 当前的numaId不在socketId中 */
#define UBSE_ERR_NODE_NOT_EXIST UBSE_ERROR_DEF(1027)               /* 节点不存在 */
#define UBSE_ERR_NODE_FAULT UBSE_ERROR_DEF(1028)                   /* 节点故障 */
#define UBSE_ENGINE_ERR_EXPORT_LEDGERING UBSE_ERROR_DEF(1029)      /* 导出节点对账中 */

/* ****************************************************** */
/* Node Controller模块错误码定义，全局唯一，范围1100~1199，记录系统的标准错误返回 */
/* ****************************************************** */
#define UBSE_ERR_NODE_NOT_FOUND UBSE_ERROR_DEF(1100)      /* 节点不存在 */
#define UBSE_ERR_NODE_UNREACHABLE UBSE_ERROR_DEF(1101)    /* 节点不可达 */
#define UBSE_ERR_NODE_NOT_ACTIVE UBSE_ERROR_DEF(1102)     /* 节点不活跃 */
#define UBSE_ERR_NODE_NOT_RESPONDING UBSE_ERROR_DEF(1103) /* 节点无响应 */

/* ************************************************************* */
/*      内部错误码定义，范围[10000,+∞)，以下错误码对外统一返回内部错误     */
/* ************************************************************* */

/* ====================== COMMON错误码 (10000~10199) ====================== */
#define UBSE_ERROR UBSE_INTERNAL_ERROR_DEF(0)                     /* 错误 */
#define UBSE_ERROR_NULLPTR UBSE_INTERNAL_ERROR_DEF(1)             /* 空指针 */
#define UBSE_ERROR_EMPTY UBSE_INTERNAL_ERROR_DEF(2)               /* 空值 */
#define UBSE_ERROR_INVAL UBSE_INTERNAL_ERROR_DEF(3)               /* 无效参数 */
#define UBSE_ERROR_NOMEM UBSE_INTERNAL_ERROR_DEF(4)               /* 内存不足 */
#define UBSE_ERROR_ACCES UBSE_INTERNAL_ERROR_DEF(5)               /* 权限被拒绝 */
#define UBSE_ERROR_FILE_EXIST UBSE_INTERNAL_ERROR_DEF(6)          /* 文件存在 */
#define UBSE_ERROR_FILE_NOT_EXIST UBSE_INTERNAL_ERROR_DEF(7)      /* 文件不存在 */
#define UBSE_ERROR_NO_ENOUGH_SPACE UBSE_INTERNAL_ERROR_DEF(8)     /* 设备上无剩余空间 */
#define UBSE_ERROR_AGAIN UBSE_INTERNAL_ERROR_DEF(9)               /* 重试 */
#define UBSE_ERROR_IO UBSE_INTERNAL_ERROR_DEF(10)                 /* I/O错误 */
#define UBSE_ERROR_CONF_INVALID UBSE_INTERNAL_ERROR_DEF(11)       /* 非法的配置文件 */
#define UBSE_ERROR_MODULE_LOAD_FAILED UBSE_INTERNAL_ERROR_DEF(12) /* 模块加载失败 */
#define UBSE_ERROR_PARSE_ARGS_FAILED UBSE_INTERNAL_ERROR_DEF(13)  /* 解析参数失败 */
#define UBSE_ERROR_SERIALIZE_FAILED UBSE_INTERNAL_ERROR_DEF(14)   /* 序列化失败 */
#define UBSE_ERROR_DESERIALIZE_FAILED UBSE_INTERNAL_ERROR_DEF(15) /* 反序列化失败 */

/* ====================== COM错误码 (10200~10299) ====================== */
#define UBSE_COM_ERROR_CHANNEL_NULL UBSE_INTERNAL_ERROR_DEF(200)            /* Hcom channel 空指针 */
#define UBSE_COM_ERROR_MESSAGE_INVALID UBSE_INTERNAL_ERROR_DEF(201)         /* 非法的消息 */
#define UBSE_COM_ERROR_MESSAGE_CHECK_SIZE_FAIL UBSE_INTERNAL_ERROR_DEF(202) /* 消息长度校验失败 */
#define UBSE_COM_ERROR_MESSAGE_INVALID_OP_CODE UBSE_INTERNAL_ERROR_DEF(203) /* 非法的操作码 */
#define UBSE_COM_ERROR_ENGINE_CREATE_FAIL UBSE_INTERNAL_ERROR_DEF(204)      /* 创建引擎失败 */
#define UBSE_COM_ERROR_CHANNEL_NOT_FOUND UBSE_INTERNAL_ERROR_DEF(205)       /* channel不存在 */
#define UBSE_COM_ERROR_GET_PEER_IP_PORT_FAIL UBSE_INTERNAL_ERROR_DEF(206)   /* 获取对端Ip端口失败 */
#define UBSE_COM_ERROR_ENGINE_NOT_INIT UBSE_INTERNAL_ERROR_DEF(207)         /* 引擎未初始化 */
#define UBSE_COM_ERROR_SYNC_CALL_FAIL UBSE_INTERNAL_ERROR_DEF(208)          /* 同步发消息失败 */
#define UBSE_COM_ERROR_ASYNC_CALL_FAIL UBSE_INTERNAL_ERROR_DEF(209)         /* 异步发消息失败 */
#define UBSE_COM_ERROR_REPLY_FAIL UBSE_INTERNAL_ERROR_DEF(210)              /* 回复发消息失败 */
#define UBSE_COM_ERROR_NEW_NET_CALLBACK_FAIL UBSE_INTERNAL_ERROR_DEF(211)   /* callback创建失败 */
#define UBSE_COM_ERROR_GET_ENGINE_FAIL UBSE_INTERNAL_ERROR_DEF(212)         /* 引擎获取失败 */
#define UBSE_COM_ERROR_RESOURCE_TEMPORARILY_UNAVAILABLE UBSE_INTERNAL_ERROR_DEF(213) /* 发送消息失败：资源临时不可用 */

/* ====================== CONF错误码 (10300~10399) ====================== */
#define UBSE_CONF_ERROR_KEY_OFFSETDIR_OPEN_ERROR UBSE_INTERNAL_ERROR_DEF(300)  /* 目录打不开 */
#define UBSE_CONF_ERROR_KEY_OFFSETFILE_OPEN_ERROR UBSE_INTERNAL_ERROR_DEF(301) /* 文件打不开 */
#define UBSE_CONF_ERROR_KEY_OFFSETDIR_TOO_DEEP UBSE_INTERNAL_ERROR_DEF(302) /* 配置目录深度超过最大限制 */
#define UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL UBSE_INTERNAL_ERROR_DEF(303) /* 配置管理模块未加载 */
#define UBSE_CONF_ERROR_KEY_OFFSETPATH_CANONICALIZATION_FAILED UBSE_INTERNAL_ERROR_DEF(304) /* 路径规范化失败 */
#define UBSE_CONF_ERROR_KEY_OFFSETMEMORY_ALLOCATION_FAILED UBSE_INTERNAL_ERROR_DEF(305)     /* 内存分配失败 */
#define UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_SECTION UBSE_INTERNAL_ERROR_DEF(306)            /* section不存在 */
#define UBSE_CONF_ERROR_KEY_OFFSETSECTION_ILLEGAL_LENGTH UBSE_INTERNAL_ERROR_DEF(307)       /* section超长或超短 */
#define UBSE_CONF_ERROR_KEY_OFFSETSECTION_HAVE_ILLEGAL_CHAR UBSE_INTERNAL_ERROR_DEF(308) /* section存在非法字符 */
#define UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_PREFIX UBSE_INTERNAL_ERROR_DEF(309)          /* prefix不存在 */
#define UBSE_CONF_ERROR_KEY_OFFSETPREFIX_ILLEGAL_CHAR UBSE_INTERNAL_ERROR_DEF(310)       /* prefix含有非法字符 */
#define UBSE_CONF_ERROR_KEY_OFFSETPREFIX_TOO_LONG UBSE_INTERNAL_ERROR_DEF(311)           /* prefix超长 */
#define UBSE_CONF_ERROR_KEY_OFFSETCONFIG_PREFIX_NO_CONTENT UBSE_INTERNAL_ERROR_DEF(312)  /* prefix中无内容 */
#define UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_KEY UBSE_INTERNAL_ERROR_DEF(313)             /* key不存在 */
#define UBSE_CONF_ERROR_KEY_OFFSETKEY_ILLEGAL_LENGTH UBSE_INTERNAL_ERROR_DEF(314)        /* key超长或超短 */
#define UBSE_CONF_ERROR_KEY_OFFSETKEY_HAVE_ILLEGAL_CHAR UBSE_INTERNAL_ERROR_DEF(315)     /* key存在非法字符 */
#define UBSE_CONF_ERROR_KEY_OFFSETVALUE_ILLEGAL_LENGTH UBSE_INTERNAL_ERROR_DEF(316)      /* value超长或空 */
#define UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE UBSE_INTERNAL_ERROR_DEF(317)          /* 不支持的类型 */
#define UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE UBSE_INTERNAL_ERROR_DEF(318) /* 查询的值超过类型的最大值 */
#define UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR UBSE_INTERNAL_ERROR_DEF(319)      /* 类型无法转化 */
#define UBSE_CONF_ERROR_KEY_OFFSETVALUE_HAVE_ILLEGAL_CHAR UBSE_INTERNAL_ERROR_DEF(320) /* 含有非法字符 */

/* ====================== HTTP错误码 (10400~10499) ====================== */
#define UBSE_HTTP_ERROR_FAILURE UBSE_INTERNAL_ERROR_DEF(400)      /* HTTP公共错误 */
#define UBSE_HTTP_ERROR_MSG_OVERSIZE UBSE_INTERNAL_ERROR_DEF(401) /* HTTP消息大小错误（超出大小限制） */
#define UBSE_HTTP_ERROR_UNKNOWN UBSE_INTERNAL_ERROR_DEF(402)      /* HTTP未知错误 */
#define UBSE_HTTP_ERROR_CONNECTION UBSE_INTERNAL_ERROR_DEF(403)   /* HTTP连接错误 */
#define UBSE_HTTP_ERROR_BIND_IP_ADDRESS UBSE_INTERNAL_ERROR_DEF(404)         /* HTTP绑定IP地址错误 */
#define UBSE_HTTP_ERROR_READ UBSE_INTERNAL_ERROR_DEF(405)                    /* HTTP读取错误 */
#define UBSE_HTTP_ERROR_WRITE UBSE_INTERNAL_ERROR_DEF(406)                   /* HTTP写入错误 */
#define UBSE_HTTP_ERROR_EXCEED_REDIRECT_COUNT UBSE_INTERNAL_ERROR_DEF(407)   /* HTTP重定向次数超出限制 */
#define UBSE_HTTP_ERROR_CANCELED UBSE_INTERNAL_ERROR_DEF(408)                /* HTTP操作取消 */
#define UBSE_HTTP_ERROR_SSL_CONNECTION UBSE_INTERNAL_ERROR_DEF(409)          /* HTTP SSL连接错误 */
#define UBSE_HTTP_ERROR_SSL_LOADING_CERTS UBSE_INTERNAL_ERROR_DEF(410)       /* HTTP SSL加载证书错误 */
#define UBSE_HTTP_ERROR_SSL_SERVER_VERIFICATION UBSE_INTERNAL_ERROR_DEF(411) /* HTTP SSL服务器验证错误 */
#define UBSE_HTTP_ERROR_UNSUPPORTED_MULTIPART_BOUNDARY_CHARS \
    UBSE_INTERNAL_ERROR_DEF(412)                                        /* HTTP不支持的多部分边界字符 */
#define UBSE_HTTP_ERROR_COMPRESSION UBSE_INTERNAL_ERROR_DEF(413)        /* HTTP压缩错误 */
#define UBSE_HTTP_ERROR_CONNECTION_TIMEOUT UBSE_INTERNAL_ERROR_DEF(414) /* HTTP连接超时 */
#define UBSE_HTTP_ERROR_PROXY_CONNECTION UBSE_INTERNAL_ERROR_DEF(415)   /* HTTP代理连接错误 */
#define UBSE_HTTP_ERROR_MSG_EMPTY UBSE_INTERNAL_ERROR_DEF(416)          /* HTTP消息为空 */
#define UBSE_HTTP_ERROR_STATUS_ERROR UBSE_INTERNAL_ERROR_DEF(417)       /* HTTP响应状态非200 */

/* ====================== PLUGIN错误码 (10500~10599) ====================== */
#define UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR UBSE_INTERNAL_ERROR_DEF(500)  /* 配置文件加载失败 */
#define UBSE_PLUGIN_ERROR_CONFIG_NOT_FOUND UBSE_INTERNAL_ERROR_DEF(501)   /* 插件配置不存在 */
#define UBSE_PLUGIN_ERROR_ADMISSION_DENIED UBSE_INTERNAL_ERROR_DEF(502)   /* 插件不准入 */
#define UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED UBSE_INTERNAL_ERROR_DEF(503) /* 插件初始化失败 */
#define UBSE_PLUGIN_ERROR_LOAD_AGAIN UBSE_INTERNAL_ERROR_DEF(504)         /* 插件已加载 */

/* ====================== RAS错误码 (10600~10699) ====================== */
#define UBSE_RAS_ERROR_MSG_DUPLICATION UBSE_INTERNAL_ERROR_DEF(600)            /* 消息重复 */
#define UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT UBSE_INTERNAL_ERROR_DEF(601) /* 该节点不是master或mem未就绪 */
#define UBSE_RAS_PANIC_REBOOT_MSG_INVALID UBSE_INTERNAL_ERROR_DEF(602)         /* 处理msgId/eid/cna消息失败 */
#define UBSE_RAS_ERROR_QUERY_NODE_BY_EID UBSE_INTERNAL_ERROR_DEF(603)          /* 未根据eid查询到NodeId */
#define UBSE_RAS_ERROR_DLOPEN_XALARMD UBSE_INTERNAL_ERROR_DEF(604)             /* dlopen失败 */
#define UBSE_RAS_ERROR_DLSYM_XALARMD UBSE_INTERNAL_ERROR_DEF(605)              /* dlsym失败 */
#define UBSE_RAS_ERROR_REPORT_TO_XALARMD UBSE_INTERNAL_ERROR_DEF(606)          /* 上报xalarm失败 */
#define UBSE_RAS_ERROR_SWITCH_ROLE UBSE_INTERNAL_ERROR_DEF(607)                /* 主备倒换 */
#define UBSE_RAS_ERROR_SWITCHING_ROLE UBSE_INTERNAL_ERROR_DEF(608)             /* 主备倒换中 */
#define UBSE_RAS_ERROR_SET_FAULT_EVENT_ON UBSE_INTERNAL_ERROR_DEF(609)         /* 打开故障事件开关失败 */
#define UBSE_RAS_ERROR_SET_SENTRY_REPORTER UBSE_INTERNAL_ERROR_DEF(610)        /* 向sysSentry配置EID失败 */

/* ====================== MMI错误码 (10700~10799) ====================== */
#define UBSE_MMI_OBMM_OP_FAILED UBSE_INTERNAL_ERROR_DEF(700)  /* OBMM 接口调用失败 */
#define UBSE_MMI_DAEMON_FAILED UBSE_INTERNAL_ERROR_DEF(701)   /* UBSE 接口调用失败 */
#define UBSE_MMI_OPEN_FAILED UBSE_INTERNAL_ERROR_DEF(702)     /* 文件打不开 */
#define UBSE_MMI_OBMM_OP_TIMEOUT UBSE_INTERNAL_ERROR_DEF(703) /* OBMM 接口调用超时 */

/* ====================== Mem Scheduler错误码 (10800~10899) ====================== */
#define UBSE_SCHEDULER_ERROR_INVAL UBSE_INTERNAL_ERROR_DEF(800)            /* 借用参数不合法错误 */
#define UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND UBSE_INTERNAL_ERROR_DEF(801) /* 无可用借出节点 */
#define UBSE_SCHEDULER_ERROR_NODE_RECONCILE UBSE_INTERNAL_ERROR_DEF(802)   /* 存在对账节点，触发重试 */

/* ====================== IPC错误码 (10900~10999) ====================== */
#define UBSE_IPC_ERROR_SOCKET_LISTEN_FAILED UBSE_INTERNAL_ERROR_DEF(900) /* 监听socket失败 */
#define UBSE_IPC_ERROR_SEND_FAILED UBSE_INTERNAL_ERROR_DEF(901)          /* 发送失败 */
#define UBSE_IPC_ERROR_RECV_FAILED UBSE_INTERNAL_ERROR_DEF(902)          /* 接收失败 */
#define UBSE_IPC_ERROR_RESP_NOT_FOUND UBSE_INTERNAL_ERROR_DEF(903)       /* 未找到响应 */
#define UBSE_IPC_ERROR_QUERY_STATE_FAILED UBSE_INTERNAL_ERROR_DEF(904)   /* 查询状态失败 */
#define UBSE_IPC_ERROR_QUERY_NUMA_NOT_EXIST UBSE_INTERNAL_ERROR_DEF(905) /* numa不存在 */

/* ====================== Mem Controller错误码 (11000~11099) ====================== */
#define UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED UBSE_INTERNAL_ERROR_DEF(1000) /* 归还时导入归还失败 */
#define UBSE_MEMCONTROLLER_ERROR_COMP_ERROR UBSE_INTERNAL_ERROR_DEF(1001)      /* 确定性迁移模式参数错误 */
#define UBSE_MEMCONTROLLER_ERROR_GET_INFO_FAIL UBSE_INTERNAL_ERROR_DEF(1002)   /* 从内部获取数据失败 */
#define UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS UBSE_INTERNAL_ERROR_DEF(1003) /* 对账未完成，查询账本返回部分成功 */

/* ====================== URMA Controller错误码 (11100~11199) ====================== */
#define UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED UBSE_INTERNAL_ERROR_DEF(1100)        /* 访问MTI接口失败 */
#define UBSE_URMACONTRL_ERROR_PRIO_GROUP_EXIST UBSE_INTERNAL_ERROR_DEF(1101)         /* 优先级组已存在 */
#define UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_EXISTED UBSE_INTERNAL_ERROR_DEF(1102) /* ETS模板未创建 */
#define UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_APPLIED UBSE_INTERNAL_ERROR_DEF(1103) /* ETS模板未应用 */

#define UBSE_URMACONTRL_ERROR_QUERY_PORTS_STATUS_FAILED UBSE_INTERNAL_ERROR_DEF(1104) /* 查询端口状态失败 */
#define UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED UBSE_INTERNAL_ERROR_DEF(1105)      /* 查询节点信息失败 */
#define UBSE_URMACONTRL_ERROR_CREATE_DEV_FAILED UBSE_INTERNAL_ERROR_DEF(1106)         /* 创建URMA设备失败 */
#define UBSE_URMACONTRL_ERROR_DEV_NOT_INACTIVE UBSE_INTERNAL_ERROR_DEF(1107) /* URMA设备状态异常，无法分配 */
#define UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST UBSE_INTERNAL_ERROR_DEF(1108)    /* URMA设备在内存中不存在 */
#define UBSE_URMACONTRL_ERROR_DEV_NAME_INVALID UBSE_INTERNAL_ERROR_DEF(1109) /* URMA设备名称无效 */

/* ====================== MTI错误码 (11200~11299) ====================== */
#define UBSE_MTI_ERROR_NOT_EXIST UBSE_INTERNAL_ERROR_DEF(1200) /* MTI查询对象不存在 */

/* 公共方法判断错误码 */
#define UBSE_RESULT_FAIL(ret) (static_cast<uint32_t>(ret) != UBSE_OK)
#define UBSE_RESULT_OK(ret) (static_cast<uint32_t>(ret) == UBSE_OK)

#endif // UBSE_ERROR_H
