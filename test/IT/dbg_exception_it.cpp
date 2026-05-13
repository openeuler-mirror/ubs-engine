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

#include "dbg_exception_it.h"

namespace ubse::it::exception {
using namespace ubse::context;
using namespace ubse::exception;

void MyHandlerBus()
{
    signal(SIGBUS, SIG_IGN);
    std::cout << "OS test SIGBUS" << std::endl;
}

void MyHandlerSegv()
{
    signal(SIGSEGV, SIG_IGN);
    std::cout << "OS test SIGSEGV" << std::endl;
}

void MyHandlerStkflt()
{
    signal(SIGSTKFLT, SIG_IGN);
    std::cout << "OS test SIGSTKFLT" << std::endl;
}

void MyHandlerXcpu()
{
    signal(SIGXCPU, SIG_IGN);
    std::cout << "OS test SIGXCPU" << std::endl;
}

void MyHandlerSys()
{
    signal(SIGSYS, SIG_IGN);
    std::cout << "OS test SIGSYS" << std::endl;
}

void MyHandlerXfsz()
{
    signal(SIGXFSZ, SIG_IGN);
    std::cout << "OS test SIGXFSZ" << std::endl;
}

void MyHandlerIll()
{
    signal(SIGILL, SIG_IGN);
    std::cout << "OS test SIGILL" << std::endl;
}

void MyHandlerFpe()
{
    signal(SIGFPE, SIG_IGN);
    std::cout << "OS test SIGFPE" << std::endl;
}

void MyHandlerPrint()
{
    signal(PRINTSIG, SIG_IGN);
    std::cout << "OS test PRINTSIG" << std::endl;
}

int32_t ITestCmdExceptionTest(ProcessMmap* pMmap)
{
    UbseContext& ctx = UbseContext::GetInstance();
    auto ubseException = ctx.GetModule<UbseExceptionModule>();
    if (ubseException == nullptr) {
        std::cout << "Cannot get ubseExceptionModule" << std::endl;
        return UBSE_ERROR;
    }
    auto ret = ubseException->SetExceptionSwitch(true);
    if (ret != UBSE_OK) {
        std::cout << "can not switch on" << std::endl;
        return UBSE_ERROR;
    }
    ret |= ubseException->RegExceptionHandler(SIGBUS, MyHandlerBus);
    ret |= ubseException->RegExceptionHandler(SIGSEGV, MyHandlerSegv);
    ret |= ubseException->RegExceptionHandler(SIGSTKFLT, MyHandlerStkflt);
    ret |= ubseException->RegExceptionHandler(SIGXCPU, MyHandlerXcpu);
    ret |= ubseException->RegExceptionHandler(SIGXFSZ, MyHandlerXfsz);
    ret |= ubseException->RegExceptionHandler(SIGSYS, MyHandlerSys);
    ret |= ubseException->RegExceptionHandler(SIGILL, MyHandlerIll);
    ret |= ubseException->RegExceptionHandler(SIGFPE, MyHandlerFpe);
    ret |= ubseException->RegExceptionHandler(PRINTSIG, MyHandlerPrint);
    if (ret != UBSE_OK) {
        std::cout << "fail to regist signal" << std::endl;
        return UBSE_ERROR;
    }

    TestPrintStack();
    TestSIGFPE();
    TestSIGSEGV();
    TestSIGSTKFLT();
    TestSIGXCPU();
    TestSIGXFSZ();
    TestSIGSYS();
    return UBSE_OK;
}
} // namespace ubse::it::exception