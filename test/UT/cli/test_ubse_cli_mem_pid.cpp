#include "test_ubse_cli_mem_pid.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_mem_pid.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::serial;
using namespace process_mem::def;

void TestUbseCliMemPid::SetUp() {}

void TestUbseCliMemPid::TearDown() {}

TEST_F(TestUbseCliMemPid, SetPidThresholdInvokeFailed)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    ProcessMemPidInfo pidInfo;
    auto result = pidModule.UbseCliSetPidThreshold(pidInfo);
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, SetPidThresholdSuccess)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_op_success));
    ProcessMemPidInfo pidInfo;
    auto result = pidModule.UbseCliSetPidThreshold(pidInfo);
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, SetPidThresholdFailed)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_op_failed));
    ProcessMemPidInfo pidInfo;
    auto result = pidModule.UbseCliSetPidThreshold(pidInfo);
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, PrintPidInfoInvokeFailed)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    auto result = pidModule.UbseCliPrintPidInfo();
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, PrintPidInfoEmpty)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_print_empty));
    auto result = pidModule.UbseCliPrintPidInfo();
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, PrintPidInfoDeserializeFailed)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_print_deserialize_failed));
    auto result = pidModule.UbseCliPrintPidInfo();
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, PrintPidInfoSuccess)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_print_success));
    auto result = pidModule.UbseCliPrintPidInfo();
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, UnsetPidInvokeFailed)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    auto result = pidModule.UbseCliUnsetPid(123);
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, UnsetPidSuccess)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_op_success));
    auto result = pidModule.UbseCliUnsetPid(123);
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliMemPid, UnsetPidFailed)
{
    UbseCliMemPid pidModule;
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_pid_op_failed));
    auto result = pidModule.UbseCliUnsetPid(123);
    ASSERT_NE(result, nullptr);
    EXPECT_NO_THROW(result->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
