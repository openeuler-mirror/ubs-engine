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

#ifndef UBSE_SSU_HTTP_HANDLER_H
#define UBSE_SSU_HTTP_HANDLER_H

#include "ubse_common_def.h"

namespace ubse::ssu::http_handler {
using namespace ubse::common::def;

// 注册SSU北向HTTP接口路由
// POST   /ubse/v1/ssu/spaces       -> AllocSpace
// DELETE /ubse/v1/ssu/spaces/{name} -> FreeSpace
UbseResult RegisterSsuHttpHandlers();
} // namespace ubse::ssu::http_handler

#endif // UBSE_SSU_HTTP_HANDLER_H
