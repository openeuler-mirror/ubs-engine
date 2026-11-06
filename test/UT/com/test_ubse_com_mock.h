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

#ifndef UBS_ENGINE_TEST_UBSE_COM_MOCK_H
#define UBS_ENGINE_TEST_UBSE_COM_MOCK_H

#include "hcom/capi/hcom_c.h"
#include "hcom/capi/hcom_service_c.h"

namespace ubse::ut::com {
/* UT专用假channel句柄：配合test_ubse_com_mock.cpp中的同名C函数符号覆盖
 * （链接期可执行文件定义优先于libhcom.so），使假句柄在UT中安全传递 */
constexpr ubs_hcom_channel TEST_CHANNEL_HANDLE = 0x1234;

/* 模拟原UBSHcomServiceContext::mCh成员：置为TEST_CHANNEL_HANDLE后
 * ubs_hcom_context_get_channel可查出该channel，默认为0表示context无channel */
extern ubs_hcom_channel g_mockContextChannel;
} // namespace ubse::ut::com
#endif // UBS_ENGINE_TEST_UBSE_COM_MOCK_H
