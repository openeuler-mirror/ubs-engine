/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"
#include "ubse_base_message.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_obmm_def.h"
namespace ubse::mem::obj::ut {
using namespace ubse::utils;
using namespace ubse::mem::serial;
class TestUbseMemObj : public testing::Test {
public:
    void SetUp() override;

    void TearDown() override;
};

void TestUbseMemObj::SetUp()
{
    Test::SetUp();
}

void TestUbseMemObj::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemObj, UbseMemFdBorrowExportObj_ToBuffer)
{
    auto obj = new UbseMemFdBorrowExportObj();
    EXPECT_NO_THROW(obj->ToBuffer());
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemFdBorrowExportObj_FromBuff)
{
    auto obj = new UbseMemFdBorrowExportObj();
    UbseObjByteBuffer buffer;
    EXPECT_NO_THROW(obj->FromBuff(buffer));
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemFdBorrowImportObj_ToBuffer)
{
    auto obj = new UbseMemFdBorrowImportObj();
    EXPECT_NO_THROW(obj->ToBuffer());
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemFdBorrowImportObj_FromBuff)
{
    auto obj = new UbseMemFdBorrowImportObj();
    UbseObjByteBuffer buffer;
    EXPECT_NO_THROW(obj->FromBuff(buffer));
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemNumaBorrowExportObj_ToBuffer)
{
    auto obj = new UbseMemNumaBorrowExportObj();
    EXPECT_NO_THROW(obj->ToBuffer());
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemNumaBorrowExportObj_FromBuff)
{
    auto obj = new UbseMemNumaBorrowExportObj();
    UbseObjByteBuffer buffer;
    EXPECT_NO_THROW(obj->FromBuff(buffer));
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemNumaBorrowImportObj_ToBuffer)
{
    auto obj = new UbseMemNumaBorrowImportObj();
    EXPECT_NO_THROW(obj->ToBuffer());
    delete obj;
}

TEST_F(TestUbseMemObj, UbseMemNumaBorrowImportObj_FromBuff)
{
    auto obj = new UbseMemNumaBorrowImportObj();
    EXPECT_NO_THROW(obj->ToBuffer());
    delete obj;
}

} // namespace ubse::mem::obj::ut