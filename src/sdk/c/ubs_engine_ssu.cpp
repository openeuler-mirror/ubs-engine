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
#include "ubs_engine_ssu.h"
#include <securec.h>
#include <functional>

#include "ssu/ubs_ssu_pack.h"
#include "ssu/ubs_ssu_validate.h"
#include "ubs_error.h"
#include "ubse_com_op_code.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"
using namespace ubs::ssu;
// 10000以内属于ubse对外暴露的错误码
static constexpr uint32_t UBSE_DAEMON_EXTERNAL_ERROR_MAX = 10000;
ubs_error_t ubs_ssu_daemon_error(uint32_t daemon_errno)
{
    if (daemon_errno < UBSE_DAEMON_EXTERNAL_ERROR_MAX) {
        return static_cast<ubs_error_t>(daemon_errno);
    }
    return UBS_ENGINE_ERR_INTERNAL; // 内部错误
}

/**
 * 释放单个 ubs_ssu_namespace_info_t 内部的 hostNqn 列表
 * @param ns 命名空间信息结构体指针,可为 nullptr
 */
static void free_namespace_info_internal(ubs_ssu_namespace_info_t *ns)
{
    if (ns == nullptr) {
        return;
    }
    if (ns->host_nqns != nullptr) {
        for (uint32_t i = 0; i < ns->nqn_count; i++) {
            delete[] ns->host_nqns[i];
        }
        delete[] ns->host_nqns;
        ns->host_nqns = nullptr;
    }
    ns->nqn_count = 0;
}

/**
 * 释放单个 ubs_ssu_alloc_result_t 内部的 namespaces 数组(含每个 namespace 的 hostNqn)
 * @param r 分配结果结构体指针,可为 nullptr
 */
static void free_alloc_result_internal(ubs_ssu_alloc_result_t *r)
{
    if (r == nullptr) {
        return;
    }
    if (r->namespaces != nullptr) {
        for (uint32_t i = 0; i < r->namespace_cnt; i++) {
            free_namespace_info_internal(&r->namespaces[i]);
        }
        delete[] r->namespaces;
        r->namespaces = nullptr;
    }
    r->namespace_cnt = 0;
}

/**
 * SSU IPC 调用统一封装
 * @param opcode    操作码
 * @param pack_fn   请求打包回调,传 nullptr 表示无请求体(list 类查询)
 * @param unpack_fn 响应解包回调,传 nullptr 表示无响应体需要解包(free/detach 类写操作)
 */
static int32_t ssu_call(uint32_t opcode, const std::function<int32_t(ubse_api_buffer_t &)> &pack_fn,
                        const std::function<int32_t(ubse_api_buffer_t &)> &unpack_fn)
{
    ubse_api_buffer_t request_buffer = {.buffer = nullptr, .length = 0};
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};

    if (pack_fn != nullptr) {
        auto ret = pack_fn(request_buffer);
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to pack request buffer, error: " << ret;
            return ret;
        }
    }
    auto ret = static_cast<ubs_error_t>(ubse_invoke_call(UBSE_SSU,
        opcode, &request_buffer, &response_buffer));
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubs_ssu_daemon_error(ret);
    }

    if (unpack_fn != nullptr) {
        ret = static_cast<ubs_error_t>(unpack_fn(response_buffer));
        if (ret != UBS_SUCCESS) {
            IPC_LOG_ERROR << "Failed to unpack response buffer, error: " << ret;
        }
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_ssu_alloc_info_list(ubs_ssu_alloc_result_t **results, uint32_t *result_cnt)
{
    if (results == nullptr || result_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: results or result_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    return ssu_call(UBSE_IPC_SSU_LIST_ALLOC_INFO, nullptr, [results, result_cnt](ubse_api_buffer_t &buf) {
        return ubs_ssu_alloc_info_list_unpack(buf, results, result_cnt);
    });
}

int32_t ubs_ssu_ns_stats_get(const char *name, ubs_ssu_ns_stats_t **ns_stats_list, uint32_t *ns_stats_cnt)
{
    auto ret = ubs_ssu_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid parameters: name: " << name;
        return ret;
    }
    if (ns_stats_list == nullptr || ns_stats_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: ns_stats_list or ns_stats_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    return ssu_call(
        UBSE_IPC_SSU_GET_NS_STATS, [name](ubse_api_buffer_t &buf) { return ubs_ssu_ns_stats_get_pack(name, buf); },
        [ns_stats_list, ns_stats_cnt](ubse_api_buffer_t &buf) {
            return ubs_ssu_ns_stats_get_unpack(buf, ns_stats_list, ns_stats_cnt);
        });
}

int32_t ubs_ssu_connect_info_get(const char *name, ubs_ub_vfe_t *vfe, ubs_ssu_connect_info_t **connect_info_list,
                                 uint32_t *connect_info_cnt)
{
    auto ret = ubs_ssu_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid parameters: name: " << name;
        return ret;
    }
    if (connect_info_list == nullptr || connect_info_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: connect_info_list or connect_info_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    return ssu_call(
        UBSE_IPC_SSU_GET_CONNECT_INFO,
        [name, vfe](ubse_api_buffer_t &buf) { return ubs_ssu_connect_info_get_pack(name, vfe, buf); },
        [connect_info_list, connect_info_cnt](ubse_api_buffer_t &buf) {
            return ubs_ssu_connect_info_get_unpack(buf, connect_info_list, connect_info_cnt);
        });
}

int32_t ubs_ssu_fe_device_list(ubs_ub_fe_t **fe_list, uint32_t *fe_cnt)
{
    if (fe_list == nullptr || fe_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: fe_list or fe_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    return ssu_call(UBSE_IPC_SSU_GET_FE_DEVICE_LIST, nullptr, [fe_list, fe_cnt](ubse_api_buffer_t &buf) {
        return ubs_ssu_fe_device_list_unpack(buf, fe_list, fe_cnt);
    });
}

int32_t ubs_ssu_space_alloc(const ubs_ssu_alloc_space_req_t *req, ubs_ssu_alloc_result_t **result)
{
    if (result == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: result is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    auto ret = ubs_ssu_alloc_space_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid parameters, ret: " << ret;
        return ret;
    }
    *result = nullptr; // 预置空,确保失败路径下出参不会是野指针
    return ssu_call(
        UBSE_IPC_SSU_ALLOC_SPACE, [req](ubse_api_buffer_t &buf) { return ubs_ssu_space_alloc_pack(req, buf); },
        [result](ubse_api_buffer_t &buf) { return ubs_ssu_space_alloc_unpack(buf, result); });
}

int32_t ubs_ssu_space_free(const char *name)
{
    auto ret = ubs_ssu_name_is_valid(name);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Invalid parameters: name: " << name;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_FREE_SPACE, [name](ubse_api_buffer_t &buf) { return ubs_ssu_space_free_pack(name, buf); },
        nullptr);
}

int32_t ubs_ssu_space_attach(const ubs_ssu_space_req_t *req, char ***ns_dev_paths,
                             uint32_t *ns_dev_path_cnt)
{
    auto ret = ubs_ssu_space_attach_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_ATTACH_SPACE, [req](ubse_api_buffer_t &buf) { return ubs_ssu_space_attach_pack(req, buf); },
        [ns_dev_paths, ns_dev_path_cnt](ubse_api_buffer_t &buf) {
            return ubs_ssu_space_attach_unpack(buf, ns_dev_paths, ns_dev_path_cnt);
        });
}

int32_t ubs_ssu_space_detach(const ubs_ssu_space_req_t *req)
{
    auto ret = ubs_ssu_space_detach_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_DETACH_SPACE, [req](ubse_api_buffer_t &buf) { return ubs_ssu_space_detach_pack(req, buf); },
        nullptr);
}

int32_t ubs_ssu_linear_space_attach(const ubs_ssu_linear_space_req_t *req, char ***ns_dev_paths,
                                    uint32_t *ns_dev_path_cnt, char dev_path[UBS_SSU_MAX_DEV_PATH_LENGTH])
{
    auto ret = ubs_ssu_linear_space_attach_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_ATTACH_LINEAR_SPACE,
        [req](ubse_api_buffer_t &buf) { return ubs_ssu_linear_space_attach_pack(req, buf); },
        [ns_dev_paths, ns_dev_path_cnt, dev_path](ubse_api_buffer_t &buf) {
            return ubs_ssu_linear_space_attach_unpack(buf, ns_dev_paths, ns_dev_path_cnt, dev_path);
        });
}

int32_t ubs_ssu_linear_space_detach(const ubs_ssu_linear_space_req_t *req)
{
    auto ret = ubs_ssu_linear_space_detach_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_DETACH_LINEAR_SPACE,
        [req](ubse_api_buffer_t &buf) { return ubs_ssu_linear_space_detach_pack(req, buf); }, nullptr);
}

int32_t ubs_ssu_striped_space_attach(const ubs_ssu_striped_space_req_t *req, char ***ns_dev_paths,
                                     uint32_t *ns_dev_path_cnt, char dev_path[UBS_SSU_MAX_DEV_PATH_LENGTH])
{
    auto ret = ubs_ssu_striped_space_attach_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_ATTACH_STRIPED_SPACE,
        [req](ubse_api_buffer_t &buf) { return ubs_ssu_striped_space_attach_pack(req, buf); },
        [ns_dev_paths, ns_dev_path_cnt, dev_path](ubse_api_buffer_t &buf) {
            return ubs_ssu_striped_space_attach_unpack(buf, ns_dev_paths, ns_dev_path_cnt, dev_path);
        });
}

int32_t ubs_ssu_striped_space_detach(const ubs_ssu_striped_space_req_t *req)
{
    auto ret = ubs_ssu_striped_space_detach_req_validate(req);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_DETACH_STRIPED_SPACE,
        [req](ubse_api_buffer_t &buf) { return ubs_ssu_striped_space_detach_pack(req, buf); }, nullptr);
}

int32_t ubs_ssu_access_permission_add(const char *name, const char *nqn)
{
    auto ret = ubs_ssu_access_permission_add_req_validate(name, nqn);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_ADD_ACCESS_PERMISSION,
        [name, nqn](ubse_api_buffer_t &buf) { return ubs_ssu_access_permission_add_pack(name, nqn, buf); }, nullptr);
}

int32_t ubs_ssu_access_permission_remove(const char *name, const char *nqn)
{
    auto ret = ubs_ssu_access_permission_remove_req_validate(name, nqn);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_REMOVE_ACCESS_PERMISSION,
        [name, nqn](ubse_api_buffer_t &buf) { return ubs_ssu_access_permission_remove_pack(name, nqn, buf); }, nullptr);
}

int32_t ubs_ssu_fe_device_alloc(uint32_t upi,const ubs_ub_vfe_t *vfe, uint8_t *bus_instance_guid)
{
    auto ret = ubs_ssu_fe_device_alloc_validate(vfe, bus_instance_guid);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_FE_DEVICE_ALLOC,
        [upi, vfe, bus_instance_guid](ubse_api_buffer_t &buf) {
            return ubs_ssu_fe_device_alloc_pack(upi, vfe, bus_instance_guid, buf);
        },
        [bus_instance_guid](ubse_api_buffer_t &buf) { return ubs_ssu_fe_device_alloc_unpack(buf, bus_instance_guid); });
}

int32_t ubs_ssu_fe_device_free(uint32_t upi,const ubs_ub_vfe_t *vfe)
{
    auto ret = ubs_ssu_fe_device_free_validate(vfe);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "param validate failed, ret: " << ret;
        return ret;
    }
    return ssu_call(
        UBSE_IPC_SSU_FE_DEVICE_FREE,
        [upi, vfe](ubse_api_buffer_t &buf) {
            return ubs_ssu_fe_device_free_pack(upi, vfe, buf);
        },
        nullptr);
}

void ubs_ssu_alloc_info_list_free(ubs_ssu_alloc_result_t **results, uint32_t result_cnt)
{
    if (results == nullptr || *results == nullptr) {
        return;
    }
    ubs_ssu_alloc_result_t *arr = *results;
    for (uint32_t i = 0; i < result_cnt; i++) {
        free_alloc_result_internal(&arr[i]); // 修复:原代码 results[i] 越界访问,应为 (*results)[i]
    }
    delete[] arr; // 修复:arr 由 new[] 分配,必须 delete[]
    *results = nullptr;
}

void ubs_ssu_ns_dev_paths_free(char ***ns_dev_paths, uint32_t ns_dev_path_cnt)
{
    if (ns_dev_paths == nullptr || *ns_dev_paths == nullptr) {

        IPC_LOG_ERROR << "Invalid parameters: ns_dev_paths or *ns_dev_paths is nullptr";
        return;
    }
    char **arr = *ns_dev_paths;
    for (uint32_t i = 0; i < ns_dev_path_cnt; i++) {
        delete[] arr[i];
    }
    delete[] arr;
    *ns_dev_paths = nullptr;
}

void ubs_ssu_ns_stats_free(ubs_ssu_ns_stats_t **ns_stats_list)
{
    if (ns_stats_list == nullptr || *ns_stats_list == nullptr) {
        return;
    }
    delete[] *ns_stats_list;
    *ns_stats_list = nullptr;
}

void ubs_ssu_connect_info_free(ubs_ssu_connect_info_t **connect_info_list)
{
    if (connect_info_list == nullptr || *connect_info_list == nullptr) {
        return;
    }
    delete[] *connect_info_list;
    *connect_info_list = nullptr;
}

void ubs_ssu_alloc_info_free(ubs_ssu_alloc_result_t **result)
{
    if (result == nullptr || *result == nullptr) {
        return;
    }
    free_alloc_result_internal(*result);
    delete *result;
    *result = nullptr; // 置空调用方指针,防止悬挂指针
}

void ubs_ssu_fe_device_list_free(ubs_ub_fe_t **fe_list, uint32_t fe_cnt)
{
    if (fe_list == nullptr || *fe_list == nullptr) {
        return;
    }
    ubs_ub_fe_t *arr = *fe_list;
    for (uint32_t i = 0; i < fe_cnt; i++) {
        if (arr[i].vfe_list != nullptr) {
            delete[] arr[i].vfe_list;
            arr[i].vfe_list = nullptr;
        }
    }
    delete[] arr;
    *fe_list = nullptr;
}
