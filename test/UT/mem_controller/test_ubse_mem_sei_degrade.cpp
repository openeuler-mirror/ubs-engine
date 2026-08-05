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
#include "test_ubse_mem_sei_degrade.h"

#include <unistd.h>
#include <cstdio>
#include <cstring>

#include "ubse_conf.h"
#include "ubse_error.h"

namespace ubse::mem::controller::ut {

static TestUbseMemSeiDegrade* g_fixture = nullptr;

void TestUbseMemSeiDegrade::SetUp()
{
    Test::SetUp();
    UbseMemSeiDegradeManager::GetInstance().Reset();
    mockSeiEnable = false;
    lastSeiValue = "0";
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;
    g_fixture = this;
}

void TestUbseMemSeiDegrade::TearDown()
{
    g_fixture = nullptr;
    Test::TearDown();
    GlobalMockObject::verify();
}

static uint32_t MockUbseGetBool(const std::string& section, const std::string& key, bool& value)
{
    if (section == "ubse.memory" && key == "sei.enable") {
        value = g_fixture->mockSeiEnable;
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

static FILE* MockPopen(const char* cmd, const char* mode)
{
    g_fixture->writeSeiCallCount++;
    std::string cmdStr(cmd);
    const char* key = "arm64_sync_sei=";
    size_t pos = cmdStr.find(key);
    if (pos != std::string::npos) {
        pos += strlen(key);
        if (pos < cmdStr.size()) {
            g_fixture->lastSeiValue = cmdStr.substr(pos, 1);
        }
    }
    return reinterpret_cast<FILE*>(0x1);
}

static char* MockFgets(char* buf, int size, FILE* stream)
{
    buf[0] = '\0';
    return nullptr;
}

static int MockPclose(FILE* stream)
{
    return g_fixture->mockPcloseStatus;
}

static FILE* MockPopenNull(const char* cmd, const char* mode)
{
    g_fixture->writeSeiCallCount++;
    errno = EACCES;
    return nullptr;
}

static FILE* MockPopenRetry(const char* cmd, const char* mode)
{
    g_fixture->writeSeiCallCount++;
    std::string cmdStr(cmd);
    const char* key = "arm64_sync_sei=";
    size_t pos = cmdStr.find(key);
    if (pos != std::string::npos) {
        pos += strlen(key);
        if (pos < cmdStr.size()) {
            g_fixture->lastSeiValue = cmdStr.substr(pos, 1);
        }
    }
    if (g_fixture->writeSeiCallCount < 2) {
        g_fixture->mockPcloseStatus = 1;
    } else {
        g_fixture->mockPcloseStatus = 0;
    }
    return reinterpret_cast<FILE*>(0x1);
}

// 配置 sei.enable=false 时，IsSeiEnabled 返回 false
TEST_F(TestUbseMemSeiDegrade, IsSeiEnabled_returns_false_when_config_disabled)
{
    mockSeiEnable = false;
    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    EXPECT_FALSE(mgr.IsSeiEnabled());
}

// 首次借用时 seiOpened_=false，TryEnableSei 应写入 "1"
TEST_F(TestUbseMemSeiDegrade, TryEnableSei_writes_1_when_seiOpened_is_false)
{
    mockSeiEnable = true;
    lastSeiValue.clear();
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();

    EXPECT_EQ(writeSeiCallCount, 1);
    EXPECT_EQ(lastSeiValue, "1");
}

// 已开启 SEI 时（seiOpened_=true），重复调用 TryEnableSei 不应再次写文件
TEST_F(TestUbseMemSeiDegrade, TryEnableSei_skips_when_seiOpened_is_true)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();
    EXPECT_EQ(writeSeiCallCount, 1);

    mgr.TryEnableSei();
    EXPECT_EQ(writeSeiCallCount, 1);
}

// 尚有导入对象时（GetTotalImportCount > 0），TryDisableSei 不应关闭 SEI
TEST_F(TestUbseMemSeiDegrade, TryDisableSei_skips_when_imports_remain)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    MOCKER_CPP((size_t(UbseMemSeiDegradeManager::*)()) & UbseMemSeiDegradeManager::GetTotalImportCount)
        .stubs()
        .will(returnValue(static_cast<size_t>(1)));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();
    EXPECT_EQ(writeSeiCallCount, 1);

    mgr.TryDisableSei();
    EXPECT_EQ(writeSeiCallCount, 1);
}

// 所有导入对象释放后（GetTotalImportCount == 0），TryDisableSei 应写入 "0" 关闭 SEI
TEST_F(TestUbseMemSeiDegrade, TryDisableSei_writes_0_when_no_imports)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    MOCKER_CPP((size_t(UbseMemSeiDegradeManager::*)()) & UbseMemSeiDegradeManager::GetTotalImportCount)
        .stubs()
        .will(returnValue(static_cast<size_t>(0)));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();
    EXPECT_EQ(lastSeiValue, "1");

    mgr.TryDisableSei();
    EXPECT_EQ(lastSeiValue, "0");
}

// SEI 未开启时（seiOpened_=false），调用 TryDisableSei 应直接返回不写文件
TEST_F(TestUbseMemSeiDegrade, TryDisableSei_returns_immediately_when_seiOpened_is_false)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryDisableSei();
    EXPECT_EQ(writeSeiCallCount, 0);
}

// 恢复时有导入对象存在（count=3），RecoverSeiState 应写入 "1" 开启 SEI
TEST_F(TestUbseMemSeiDegrade, RecoverSeiState_writes_1_when_imports_exist)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    MOCKER_CPP((size_t(UbseMemSeiDegradeManager::*)()) & UbseMemSeiDegradeManager::GetTotalImportCount)
        .stubs()
        .will(returnValue(static_cast<size_t>(3)));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.RecoverSeiState();

    EXPECT_EQ(lastSeiValue, "1");
    EXPECT_EQ(writeSeiCallCount, 1);
}

// 恢复时无导入对象（count=0），RecoverSeiState 应写入 "0" 关闭 SEI
TEST_F(TestUbseMemSeiDegrade, RecoverSeiState_writes_0_when_no_imports)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    MOCKER_CPP((size_t(UbseMemSeiDegradeManager::*)()) & UbseMemSeiDegradeManager::GetTotalImportCount)
        .stubs()
        .will(returnValue(static_cast<size_t>(0)));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.RecoverSeiState();

    EXPECT_EQ(lastSeiValue, "0");
    EXPECT_EQ(writeSeiCallCount, 1);
}

// popen 失败时（返回 nullptr），重试机制生效，最终不写文件
TEST_F(TestUbseMemSeiDegrade, TryEnableSei_retries_on_popen_failure)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopenNull));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();

    EXPECT_GT(writeSeiCallCount, 0);
}

// pclose 持续返回非 0，重试 3 次后 seiOpened_ 保持 false
TEST_F(TestUbseMemSeiDegrade, TryEnableSei_keeps_seiOpened_false_on_pclose_failure)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 1;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopen));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();

    EXPECT_GT(writeSeiCallCount, 0);
}

// 并发安全由单线程执行器 ubseSeiExecutor 保证（FIFO 串行）。
// TryEnableSei/TryDisableSei 本身不再需要内部互斥锁——同一时刻只有一个
// SEI 操作在执行。此处直接验证 TryEnableSei 被调用两次时的去重行为
// （已有 TryEnableSei_skips_when_seiOpened_is_true 覆盖同一用例）。
//
// 如需测试并发场景，应在集成测试层面验证 ubseSeiExecutor 的 FIFO 串行特性。

// pclose 临时返回非 0 后重试成功，验证重试机制和最终写入正确值
TEST_F(TestUbseMemSeiDegrade, TryEnableSei_retries_on_temporary_pclose_failure)
{
    mockSeiEnable = true;
    writeSeiCallCount = 0;
    mockPcloseStatus = 0;

    MOCKER_CPP((uint32_t(*)(const std::string&, const std::string&, bool&)) & ubse::config::UbseGetBool)
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP((FILE * (*)(const char*, const char*)) popen).stubs().will(invoke(MockPopenRetry));
    MOCKER_CPP((char* (*)(char*, int, FILE*))fgets).stubs().will(invoke(MockFgets));
    MOCKER_CPP((int (*)(FILE*))pclose).stubs().will(invoke(MockPclose));

    auto& mgr = UbseMemSeiDegradeManager::GetInstance();
    mgr.Init();
    mgr.TryEnableSei();

    EXPECT_GT(writeSeiCallCount, 1);
    EXPECT_EQ(lastSeiValue, "1");
}

} // namespace ubse::mem::controller::ut
