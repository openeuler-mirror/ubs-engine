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
#ifndef UBS_ENGINE_UBS_SSU_VALIDATE_H
#define UBS_ENGINE_UBS_SSU_VALIDATE_H
#include "ubs_engine_ssu.h"

namespace ubs::ssu {
uint32_t ubs_ssu_name_is_valid(const char *name);
uint32_t ubs_ssu_alloc_space_req_validate(const ubs_ssu_alloc_space_req_t *req);
uint32_t ubs_ssu_access_permission_add_req_validate(const char *name, const char *nqn);
uint32_t ubs_ssu_access_permission_remove_req_validate(const char *name, const char *nqn);
uint32_t ubs_ssu_space_attach_req_validate(const ubs_ssu_space_req_t *req);
uint32_t ubs_ssu_space_detach_req_validate(const ubs_ssu_space_req_t *req);
uint32_t ubs_ssu_linear_space_attach_req_validate(const ubs_ssu_linear_space_req_t *req);
uint32_t ubs_ssu_linear_space_detach_req_validate(const ubs_ssu_linear_space_req_t *req);
uint32_t ubs_ssu_striped_space_attach_req_validate(const ubs_ssu_striped_space_req_t *req);
uint32_t ubs_ssu_striped_space_detach_req_validate(const ubs_ssu_striped_space_req_t *req);
uint32_t ubs_ssu_fe_device_alloc_validate(uint32_t upi,const ubs_ub_vfe_t *vfe);
uint32_t ubs_ssu_fe_device_free_validate(uint32_t upi,const ubs_ub_vfe_t *vfe);
} // namespace ubs::ssu
#endif //UBS_ENGINE_UBS_SSU_VALIDATE_H
