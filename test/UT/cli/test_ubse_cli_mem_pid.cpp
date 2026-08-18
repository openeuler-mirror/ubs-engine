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

} // namespace ubse::ut::cli
