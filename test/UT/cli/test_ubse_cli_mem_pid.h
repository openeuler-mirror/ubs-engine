#ifndef TEST_UBSE_CLI_MEM_PID_H
#define TEST_UBSE_CLI_MEM_PID_H

#include <gtest/gtest.h>

namespace ubse::ut::cli {
class TestUbseCliMemPid : public testing::Test {
public:
    TestUbseCliMemPid() = default;
    void SetUp() override;
    void TearDown() override;
};
} // namespace ubse::ut::cli
#endif // TEST_UBSE_CLI_MEM_PID_H
