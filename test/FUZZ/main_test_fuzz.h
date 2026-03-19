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

#ifndef MAIN_TEST_FUZZ_H
#define MAIN_TEST_FUZZ_H

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::fuzz {
using namespace ubse::context;

void SetArgv(char **argv);

char **GetArgv();

int GetFuzzCount();

int32_t TestServerStart();

int32_t TestServerStop();

}

#endif // MAIN_TEST_FUZZ_H
