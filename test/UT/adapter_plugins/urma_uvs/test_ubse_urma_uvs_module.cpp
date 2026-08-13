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

#include "test_ubse_urma_uvs_module.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <mockcpp/mokc.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_security.h"
#include "ubse_urma_uvs_module.h"
#include "adapter_plugins/mti/ubse_smbios.h"

namespace ubse::urma::ut {
using namespace ubse::context;
using namespace ubse::urma;
using namespace ubse::security;
using namespace ubse::adapter_plugins::smbios;

namespace {
constexpr auto EID_SHARING_FILE = "/sys/class/ubcore/ubcore/dev_sharing";
constexpr int EID_SHARING_FD = 101;
std::atomic<uint32_t> g_openCount{0};
std::string g_openedFile;
int g_openFlags = 0;
int g_writtenFd = -1;
std::string g_writtenValue;
int g_closedFd = -1;
std::mutex g_sysfsWriteMutex;
std::condition_variable g_sysfsWriteCv;
bool g_sysfsWriteEntered = false;
bool g_releaseSysfsWrite = false;

int OpenEidSharingFile(const char* file, int flags)
{
    ++g_openCount;
    g_openedFile = file;
    g_openFlags = flags;
    return EID_SHARING_FD;
}

int OpenEidSharingFileFailsFirst(const char* file, int flags)
{
    const auto count = ++g_openCount;
    g_openedFile = file;
    g_openFlags = flags;
    if (count == 1) {
        errno = EACCES;
        return -1;
    }
    return EID_SHARING_FD;
}

ssize_t WriteEidSharingValue(int fd, const void* value, size_t count)
{
    g_writtenFd = fd;
    g_writtenValue.assign(static_cast<const char*>(value), count);
    return static_cast<ssize_t>(count);
}

ssize_t WriteEidSharingValueFails(int fd, const void* value, size_t count)
{
    (void)WriteEidSharingValue(fd, value, count);
    errno = EIO;
    return -1;
}

ssize_t WriteEidSharingValueShort(int fd, const void* value, size_t count)
{
    (void)WriteEidSharingValue(fd, value, count);
    return 0;
}

ssize_t WriteEidSharingValueBlocks(int fd, const void* value, size_t count)
{
    const auto written = WriteEidSharingValue(fd, value, count);
    std::unique_lock<std::mutex> lock(g_sysfsWriteMutex);
    g_sysfsWriteEntered = true;
    g_sysfsWriteCv.notify_all();
    g_sysfsWriteCv.wait(lock, [] { return g_releaseSysfsWrite; });
    return written;
}

int CloseEidSharingFile(int fd)
{
    g_closedFd = fd;
    return 0;
}

void* ResolveRequiredSymbols(void*, const char*)
{
    return reinterpret_cast<void*>(0x5678);
}

void ResetSysfsWriteState()
{
    g_openCount = 0;
    g_openedFile.clear();
    g_openFlags = 0;
    g_writtenFd = -1;
    g_writtenValue.clear();
    g_closedFd = -1;
    std::lock_guard<std::mutex> lock(g_sysfsWriteMutex);
    g_sysfsWriteEntered = false;
    g_releaseSysfsWrite = false;
}

void ReleaseSysfsWrite()
{
    {
        std::lock_guard<std::mutex> lock(g_sysfsWriteMutex);
        g_releaseSysfsWrite = true;
    }
    g_sysfsWriteCv.notify_all();
}
} // namespace

void TestUrmaUvsModule::SetUp()
{
    Test::SetUp();
    ResetSysfsWriteState();
}

void TestUrmaUvsModule::TearDown()
{
    ReleaseSysfsWrite();
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUrmaUvsModule, Initialize_Fail)
{
    UbseUrmaUvsModule module;
    void* mockHandle = nullptr;
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));

    EXPECT_EQ(module.Initialize(), UBSE_ERROR_FILE_NOT_EXIST);
}

TEST_F(TestUrmaUvsModule, Initialize_Success)
{
    UbseUrmaUvsModule module;
    void* mockHandle = reinterpret_cast<void*>(0x1234);
    MOCKER_CPP(&dlopen).stubs().will(returnValue(mockHandle));
    MOCKER_CPP(&dlsym).stubs().will(invoke(ResolveRequiredSymbols));

    EXPECT_EQ(module.Initialize(), UBSE_OK);

    MOCKER_CPP(&dlclose).stubs().will(returnValue(0));
    module.UnInitialize();
}

TEST_F(TestUrmaUvsModule, FailedStartupConfigurationRetriesOnRequest)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(true));
    MOCKER_CPP(ChangeOverrideCapability).expects(exactly(4)).will(returnValue(UBSE_OK));
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open))
        .expects(exactly(2))
        .will(invoke(OpenEidSharingFileFailsFirst));
    MOCKER(write).expects(once()).will(invoke(WriteEidSharingValue));
    MOCKER(close).expects(once()).will(invoke(CloseEidSharingFile));

    bool enabled = false;
    EXPECT_EQ(module.Start(), UBSE_OK);
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_OK);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(g_writtenValue, "1");
}

TEST_F(TestUrmaUvsModule, ClosStartSkipsEidSharingConfiguration)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).expects(never());
    MOCKER_CPP(ChangeOverrideCapability).expects(never());
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(never());
    MOCKER(write).expects(never());
    MOCKER(close).expects(never());

    EXPECT_EQ(module.Start(), UBSE_OK);
}

TEST_F(TestUrmaUvsModule, UnsupportedCpuSkipsEidSharingConfiguration)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(false));
    MOCKER_CPP(ChangeOverrideCapability).expects(never());
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(never());
    MOCKER(write).expects(never());
    MOCKER(close).expects(never());

    bool enabled = true;
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_OK);
    EXPECT_FALSE(enabled);
}

TEST_F(TestUrmaUvsModule, OverrideCapabilityFailurePreventsSysfsWrite)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(true));
    MOCKER_CPP(ChangeOverrideCapability).expects(once()).with(eq(true)).will(returnValue(UBSE_ERROR));
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(never());
    MOCKER(write).expects(never());
    MOCKER(close).expects(never());

    bool enabled = true;
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_ERROR);
    EXPECT_FALSE(enabled);
}

TEST_F(TestUrmaUvsModule, SysfsWriteFailureReturnsErrorAndClosesFile)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(true));
    MOCKER_CPP(ChangeOverrideCapability).expects(exactly(2)).will(returnValue(UBSE_OK));
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(once()).will(invoke(OpenEidSharingFile));
    MOCKER(write).expects(once()).will(invoke(WriteEidSharingValueFails));
    MOCKER(close).expects(once()).will(invoke(CloseEidSharingFile));

    bool enabled = true;
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_ERROR_IO);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(g_closedFd, EID_SHARING_FD);
}

TEST_F(TestUrmaUvsModule, SysfsShortWriteReturnsErrorAndClosesFile)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(true));
    MOCKER_CPP(ChangeOverrideCapability).expects(exactly(2)).will(returnValue(UBSE_OK));
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(once()).will(invoke(OpenEidSharingFile));
    MOCKER(write).expects(once()).will(invoke(WriteEidSharingValueShort));
    MOCKER(close).expects(once()).will(invoke(CloseEidSharingFile));

    bool enabled = true;
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_ERROR_IO);
    EXPECT_FALSE(enabled);
}

TEST_F(TestUrmaUvsModule, SysfsCloseFailureDoesNotRetry)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(true));
    MOCKER_CPP(ChangeOverrideCapability).expects(exactly(2)).will(returnValue(UBSE_OK));
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(once()).will(invoke(OpenEidSharingFile));
    MOCKER(write).expects(once()).will(invoke(WriteEidSharingValue));
    MOCKER(close).expects(once()).will(returnValue(-1));

    bool enabled = false;
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_OK);
    EXPECT_TRUE(enabled);
    enabled = false;
    EXPECT_EQ(module.EnsureEidSharingConfigured(enabled), UBSE_OK);
    EXPECT_TRUE(enabled);
}

TEST_F(TestUrmaUvsModule, ConcurrentEnsureConfiguresEidSharingOnce)
{
    UbseUrmaUvsModule module;
    MOCKER_CPP(&UbseSmbios::Is1650V100Cpu).stubs().will(returnValue(true));
    MOCKER_CPP(ChangeOverrideCapability).expects(exactly(2)).will(returnValue(UBSE_OK));
    MOCKER(reinterpret_cast<int (*)(const char*, int)>(open)).expects(once()).will(invoke(OpenEidSharingFile));
    MOCKER(write).expects(once()).will(invoke(WriteEidSharingValueBlocks));
    MOCKER(close).expects(once()).will(invoke(CloseEidSharingFile));
    UbseResult firstRet = UBSE_ERROR;
    UbseResult secondRet = UBSE_ERROR;
    bool firstEnabled = false;
    bool secondEnabled = false;
    std::thread first([&] { firstRet = module.EnsureEidSharingConfigured(firstEnabled); });
    {
        std::unique_lock<std::mutex> lock(g_sysfsWriteMutex);
        g_sysfsWriteCv.wait(lock, [] { return g_sysfsWriteEntered; });
    }
    std::thread second([&] { secondRet = module.EnsureEidSharingConfigured(secondEnabled); });
    ReleaseSysfsWrite();
    first.join();
    second.join();

    EXPECT_EQ(firstRet, UBSE_OK);
    EXPECT_EQ(secondRet, UBSE_OK);
    EXPECT_TRUE(firstEnabled);
    EXPECT_TRUE(secondEnabled);
}

} // namespace ubse::urma::ut
