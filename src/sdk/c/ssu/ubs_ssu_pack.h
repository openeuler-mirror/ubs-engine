/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBS_SSU_PACK_H
#define UBS_ENGINE_UBS_SSU_PACK_H
#include "ubs_engine_ssu.h"
#include "ubse_ipc_client.h"

namespace ubs::ssu {
/**
 * 已分配空间信息列表解包
 * 对应服务端 SsuAllocInfoListPack: allocResultListSize(uint32) + [AllocResultPack]*allocResultListSize
 * AllocResultPack: name(string) + strategy(uint8) + nsListSize(uint32) + [NameSpaceInfo]*nsListSize
 * @param buffer 输入缓冲区
 * @param results 输出结果指针
 * @param result_cnt 输出结果数量指针
 * @return 错误码
 */
ubs_error_t ubs_ssu_alloc_info_list_unpack(ubse_api_buffer_t &buffer, ubs_ssu_alloc_result_t **results,
                                           uint32_t *result_cnt);
/**
 * @brief 获取命名空间统计请求打包
 * 对应服务端 SsuGetNsStatsUnpack: name(string)
 * @param name 空间名称
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_ns_stats_get_pack(const char *name, ubse_api_buffer_t &buffer);
/**
 * 解包命名空间状态信息结果
 * @param buffer  输入缓冲区
 * @param stats 成功时指向新分配的 ns_stats 数组，调用方负责释放
 *                      失败时为 nullptr，已分配内存已内部清理
 * @param stats_cnt  命名空间统计数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_ns_stats_get_unpack(ubse_api_buffer_t &buffer, ubs_ssu_ns_stats_t **stats, uint32_t *stats_cnt);
/**
 * @brief 获取连接信息请求打包
 * 对应服务端 SsuGetConnectInfoUnpack: name(string) + vfe(VfePack)
 * @param name 空间名称
 * @param vfe VFE信息
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_connect_info_get_pack(const char *name, const ubs_ub_vfe_t *vfe, ubse_api_buffer_t &buffer);
/**
 * @brief 获取连接信息结果解包
 * 对应服务端: listSize(uint32) + [ConnectInfo]*listSize
 * @param buffer 输入缓冲区
 * @param connect_info_list 成功时指向新分配的 connect_info 数组，调用方负责释放
 *                          失败时为 nullptr，已分配内存已内部清理
 * @param connect_info_cnt  连接信息数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_connect_info_get_unpack(ubse_api_buffer_t &buffer, ubs_ssu_connect_info_t **connect_info_list,
                                            uint32_t *connect_info_cnt);
/**
 * @brief 分配空间请求打包
 * 对应服务端 SsuAllocSpaceUnpack: name(string) + nsSize(uint64) + nsNum(uint32)
 * + lbaFormat(uint32) + strategy(uint8) + tenant(string)
 * @param req 分配空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_alloc_pack(const ubs_ssu_alloc_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * @brief 分配空间结果解包
 * 对应服务端 SsuAllocSpacePack: AllocResultPack(result)
 * @param buffer 输入缓冲区
 * @param result 成功时指向新分配的分配结果结构体指针，调用方负责释放
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_space_alloc_unpack(ubse_api_buffer_t &buffer, ubs_ssu_alloc_result_t **result);
/**
 * @brief 释放空间请求打包
 * 对应服务端 SsuFreeSpaceUnpack: name(string)
 * @param name 空间名称
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_free_pack(const char *name, ubse_api_buffer_t &buffer);
/**
 * @brief 添加访问权限请求打包
 * 对应服务端 SsuAccessPermissionAddUnpack: name(string) + hostNqn(string)
 * @param name 空间名称
 * @param nqn NVMeoF目标NQN
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_access_permission_add_pack(const char *name, const char *nqn, ubse_api_buffer_t &buffer);
/**
 * @brief 移除访问权限请求打包
 * 对应服务端 SsuAccessPermissionRemoveUnpack: name(string) + hostNqn(string)
 * @param name 空间名称
 * @param nqn NVMeoF目标NQN
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_access_permission_remove_pack(const char *name, const char *nqn, ubse_api_buffer_t &buffer);
/**
 * @brief 挂载空间请求打包
 * 对应服务端 SsuAttachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 * @param req 挂载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_attach_pack(const ubs_ssu_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * @brief 挂载空间结果解包
 * 对应服务端 SsuAttachSpacePack: nsDevPathsListSize(uint32) + [nsDevPath(string)]*nsDevPathsListSize
 * @param buffer 输入缓冲区
 * @param ns_dev_paths [OUT] 命名空间设备路径数组，成功时指向新分配的数组，调用方负责释放
 * @param ns_dev_path_cnt [OUT] 命名空间设备路径数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_space_attach_unpack(ubse_api_buffer_t &buffer, char ***ns_dev_paths,
                                        uint32_t *ns_dev_path_cnt);
/**
 * @brief 卸载空间请求打包
 * 对应服务端 SsuDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 * @param req 卸载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_space_detach_pack(const ubs_ssu_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * @brief 线性挂载空间请求打包
 * 对应服务端 SsuLinearAttachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string)
 * @param req 线性挂载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_linear_space_attach_pack(const ubs_ssu_linear_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * @brief 线性挂载空间结果解包
 * 对应服务端 SsuLinearAttachSpacePack: nsDevPathsListSize(uint32) + [nsDevPath(string)]*nsDevPathsListSize + devPath(string)
 * @param buffer 输入缓冲区
 * @param ns_dev_paths [OUT] 命名空间设备路径数组，成功时指向新分配的数组，调用方负责释放
 * @param ns_dev_path_cnt [OUT] 命名空间设备路径数量
 * @param dev_path [OUT] 挂载路径，非空
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_linear_space_attach_unpack(ubse_api_buffer_t &buffer, char ***ns_dev_paths,
                                              uint32_t *ns_dev_path_cnt, char *dev_path);
/**
 * @brief 线性卸载空间请求打包
 * 对应服务端 SsuLinearDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string)
 * @param req 线性卸载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_linear_space_detach_pack(const ubs_ssu_linear_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * @brief 条带化挂载空间请求打包
 * 对应服务端 SsuStripedAttachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string)
 *  + devName(string) + level(uint32) + chunkSize(uint32)
 * @param req 条带化挂载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_striped_space_attach_pack(const ubs_ssu_striped_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * @brief 条带化挂载空间结果解包
 * 对应服务端 SsuStripedAttachSpacePack: nsDevPathsListSize(uint32) + [nsDevPath(string)]*nsDevPathsListSize + devPath(string)
 * @param buffer 输入缓冲区
 * @param ns_dev_paths [OUT] 命名空间设备路径数组，成功时指向新分配的数组，调用方负责释放
 * @param ns_dev_path_cnt [OUT] 命名空间设备路径数量
 * @param dev_path [OUT] 挂载路径，非空
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_striped_space_attach_unpack(ubse_api_buffer_t &buffer, char ***ns_dev_paths,
                                               uint32_t *ns_dev_path_cnt, char *dev_path);
/**
 * @brief 条带化卸载空间请求打包
 * 对应服务端 SsuStripedDetachSpaceUnpack: name(string) + hostNqn(string) + srcEid(string) + devName(string) + level(uint32) + chunkSize(uint32)
 * @param req 条带化卸载空间请求参数
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_striped_space_detach_pack(const ubs_ssu_striped_space_req_t *req, ubse_api_buffer_t &buffer);
/**
 * 解包 FE 设备列表
 * 对应服务端: feListSize(uint32) + [FePack]*feListSize
 * @param buffer     解包上下文
 * @param fe_device_list 成功时指向新分配的 FE 数组，调用方负责释放
 *                          失败时为 nullptr，已分配内存已内部清理
 * @param fe_device_cnt  FE 设备数量
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_fe_device_list_unpack(ubse_api_buffer_t &buffer, ubs_ub_fe_t **fe_device_list,
                                          uint32_t *fe_device_cnt);
/**
 * @brief FE设备分配请求打包
 * 对应服务端 SsuFeDeviceAllocUnpack: upi(uint32_t) + vfe(VfePack) + busInstanceGuid(string)
 * @param upi UPID
 * @param vfe VFE信息
 * @param bus_instance_guid [IN] 设备实例GUID
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_fe_device_alloc_pack(uint32_t upi, const ubs_ub_vfe_t *vfe, uint8_t *bus_instance_guid,
                                         ubse_api_buffer_t &buffer);

/**
 * @brief FE设备分配结果解包
 * 对应服务端 SsuFeDeviceAllocPack: busInstanceGuid(string)
 * @param buffer 输入缓冲区
 * @param bus_instance_guid 分配的总线实例 GUID，非空
 * @return UBS_SUCCESS 成功，其他值为错误码
 */
ubs_error_t ubs_ssu_fe_device_alloc_unpack(ubse_api_buffer_t &buffer, uint8_t *bus_instance_guid);
/**
 * @brief FE设备释放请求打包
 * 对应服务端 SsuFeDeviceFreeUnpack: upi(uint32_t) + vfe(VfePack)
 * @param upi UPID
 * @param vfe VFE信息
 * @param buffer 输出缓冲区
 * @return ubs_error_t 错误码
 */
ubs_error_t ubs_ssu_fe_device_free_pack(uint32_t upi, const ubs_ub_vfe_t *vfe,
                                        ubse_api_buffer_t &buffer);
} // namespace ubs::ssu

#endif //UBS_ENGINE_UBS_SSU_PACK_H
