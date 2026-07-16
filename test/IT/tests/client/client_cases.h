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

#ifndef IT_CLIENT_CASES_H
#define IT_CLIENT_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::client {

// P0-Init-Ok-01: 默认路径初始化, path=NULL, 返回 UBS_SUCCESS
void RunP0InitOk01(ubse::it::infra::ItCluster& cluster);

// P0-Init-Ok-02: 指定路径初始化, path=有效 UDS, 返回 UBS_SUCCESS
void RunP0InitOk02(ubse::it::infra::ItCluster& cluster);

// P0-Init-OverLen-01: 路径超长, path 长度 > 107, 返回 OUT_OF_RANGE
void RunP0InitOverLen01(ubse::it::infra::ItCluster& cluster);

// P0-Init-BadParam-01: 路径不存在, 返回 CONNECTION_FAILED
void RunP0InitBadParam01(ubse::it::infra::ItCluster& cluster);

// P0-Fin-Ok-01: 正常销毁, 调用后 SDK 接口不可用
void RunP0FinOk01(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::client

#endif // IT_CLIENT_CASES_H
