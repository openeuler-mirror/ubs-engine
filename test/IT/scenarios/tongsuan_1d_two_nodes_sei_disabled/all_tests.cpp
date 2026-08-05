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

#include "scenario.h"
#include "tests/sei_degrade/sei_degrade_cases.h"

using ubse::it::infra::Tongsuan1dTwoNodesSeiDisabledScenario;

// IT-06: sei.enable=false 全流程测试
// 验证配置 sei.enable=false 时，借入/归还操作不会改变 SEI 文件状态
TEST_F(Tongsuan1dTwoNodesSeiDisabledScenario, IT06SeiDegradeDisabled)
{
    ubse::it::tests::sei_degrade::RunITSeiDegradeDisabled(Cluster());
}
