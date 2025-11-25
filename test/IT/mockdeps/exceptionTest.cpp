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

#include "exceptionTest.h"
#include <csignal>
#include <cstdint>
#include "ubse_common_def.h"

namespace ubse::it::exception {
using namespace ubse::common::def;
void SimulateSig(uint32_t iSigNum)
{
    raise(iSigNum);
}

void TestPrintStack()
{
    SimulateSig(PRINTSIG);
}

void TestSIGFPE()
{
    SimulateSig(SIGFPE);
}

void TestSIGSEGV()
{
    SimulateSig(SIGSEGV);
}

void TestSIGSTKFLT()
{
    SimulateSig(SIGSTKFLT);
}

void TestSIGXCPU()
{
    SimulateSig(SIGXCPU);
}

void TestSIGXFSZ()
{
    SimulateSig(SIGXFSZ);
}

void TestSIGSYS()
{
    SimulateSig(SIGSYS);
}
}
