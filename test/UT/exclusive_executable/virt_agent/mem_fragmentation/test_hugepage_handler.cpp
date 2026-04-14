/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "test_hugepage_handler.h"

#include <mockcpp/mockcpp.hpp>

#include "hugepage_handler.h"
#include "vm_file_util.h"
#include "vm_def.h"

using namespace vm;
namespace ubse::ut::vm {
const uint64_t BORROW_SIZE = 1073741824;
// 设置测试环境
void TestHugepageHandler::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestHugepageHandler::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestHugepageHandler, SetHugePages)
{
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::SetHugePages(0, 1024);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, SetHugePagesFail)
{
    MOCKER(HugePageHandler::AllocateHugePages).stubs().will(returnValue(VM_ERROR));
    auto ret = HugePageHandler::SetHugePages(0, 1024);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePages)
{
    HugePageInfo info{0, 1024};
    uint64_t hugePages = (info.borrowedSize + (2 * MB_TO_BYTES - 1)) / (2 * MB_TO_BYTES);
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::GetCurrentHugePages).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::RewriteHugePages).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(HugePageHandler::GetCurrentHugePages).reset();
    MOCKER(HugePageHandler::GetCurrentHugePages).stubs().with(_, outBound(hugePages)).will(returnValue(VM_OK));
    ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetCurrentHugePagesFail2ndTime)
{
    HugePageInfo info{0, 1024};
    uint64_t hugePages = (info.borrowedSize + (2 * MB_TO_BYTES - 1)) / (2 * MB_TO_BYTES);
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_OK));
    MOCKER(HugePageHandler::GetCurrentHugePages)
        .stubs()
        .with(_, outBound(hugePages))
        .will(returnValue(VM_OK))
        .then(returnValue(VM_ERROR));
    MOCKER(HugePageHandler::RewriteHugePages).stubs().will(returnValue(VM_OK));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, AllocateHugePagesGetHugePageCanonicalPathFail)
{
    HugePageInfo info{0, 1024};
    uint64_t hugePages = (info.borrowedSize + (2 * MB_TO_BYTES - 1)) / (2 * MB_TO_BYTES);
    MOCKER(HugePageHandler::GetHugePageCanonicalPath).stubs().will(returnValue(VM_ERROR));
    auto ret = HugePageHandler::AllocateHugePages(info);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetHugePageCanonicalPath)
{
    std::string path;
    MOCKER(VmFileUtil::CanonicalPath).stubs().will(returnValue(true));
    auto ret = HugePageHandler::GetHugePageCanonicalPath("1", path);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, RewriteHugePages)
{
    std::string path;
    MOCKER(VmFileUtil::CanonicalPath).stubs().will(returnValue(true));
    auto ret = HugePageHandler::RewriteHugePages("1", 1);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestHugepageHandler, GetCurrentHugePages_FileOpenFailed1)
{
    std::string realPath = "/tmp/nonexistent_file";
    uint64_t currentHugePages = 0;

    VmResult ret = HugePageHandler::GetCurrentHugePages(realPath, currentHugePages);

    EXPECT_EQ(ret, VM_ERROR);
    EXPECT_EQ(currentHugePages, 0);
}

}