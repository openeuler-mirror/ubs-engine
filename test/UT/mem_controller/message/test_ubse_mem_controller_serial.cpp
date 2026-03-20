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

#include "test_ubse_mem_controller_serial.h"

#include "mockcpp/mockcpp.hpp"

#include "message/ubse_mem_controller_serial.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"

namespace ubse::mem::serial::ut {
void TestUbseMemControllerSerial::SetUp()
{
    Test::SetUp();
}

void TestUbseMemControllerSerial::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试 UbseMemDebtNumaInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：方法正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemDebtNumaInfoSerialization)
{
    UbseSerialization out;
    UbseMemDebtNumaInfo debtNumaInfo;
    EXPECT_NO_THROW(UbseMemDebtNumaInfoSerialization(out, debtNumaInfo));
}

/*
 * 用例描述：测试 UbseMemDebtNumaInfoDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemDebtNumaInfoDeserialization)
{
    UbseDeSerialization in;
    UbseMemDebtNumaInfo debtNumaInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemDebtNumaInfoDeserialization(in, debtNumaInfo));
    EXPECT_TRUE(UbseMemDebtNumaInfoDeserialization(in, debtNumaInfo));
}

/*
 * 用例描述： 测试 UbseMemAlgoResultSerialization 方法
 * 测试步骤： 略
 * 预期结果： 程序正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemAlgoResultSerialization)
{
    UbseSerialization out;
    UbseMemAlgoResult algoResult;
    algoResult.exportNumaInfos.emplace_back(UbseMemDebtNumaInfo());
    algoResult.importNumaInfos.emplace_back(UbseMemDebtNumaInfo());
    EXPECT_NO_THROW(UbseMemAlgoResultSerialization(out, algoResult));
}

/*
 * 用例描述： 测试UbseMemAlgoResultDeserialization 在Check函数失败时的行为
 * 测试步骤：略
 * 预期结果：返回UBSE_ERROR
 */
TEST_F(TestUbseMemControllerSerial, UbseMemAlgoResultDeserialization_WhenCheckFailed)
{
    UbseDeSerialization in;
    UbseMemAlgoResult algoResult;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false));
    EXPECT_FALSE(UbseMemAlgoResultDeserialization(in, algoResult));
}

/*
 * 用例描述：
 * 测试UbseMemAlgoResultDeserialization在一切功能正常时的行为
 * 测试步骤：略
 * 预期结果：返回UBSE_ERROR
 */
TEST_F(TestUbseMemControllerSerial, UbseMemAlgoResultDeserialization_WhenEverythingIsOk)
{
    UbseDeSerialization in;
    UbseMemAlgoResult algoResult;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(true));
    EXPECT_TRUE(UbseMemAlgoResultDeserialization(in, algoResult));
}

/*
 * 用例描述：测试 UbseMemObmmMemDescSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemObmmMemDescSerialization)
{
    UbseSerialization out;
    ubse_mem_obmm_mem_desc ubseMemObmmMemDesc;
    EXPECT_NO_THROW(UbseMemObmmMemDescSerialization(out, ubseMemObmmMemDesc));
}

/*
 * 用例描述：测试 UbseMemObmmMemDescDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemObmmMemDescDeserialization)
{
    UbseDeSerialization in;
    ubse_mem_obmm_mem_desc ubseMemObmmMemDesc;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemObmmMemDescDeserialization(in, ubseMemObmmMemDesc));
    EXPECT_TRUE(UbseMemObmmMemDescDeserialization(in, ubseMemObmmMemDesc));
}

/*
 * 用例描述：测试 UbseMemObmmInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemObmmInfoSerialization)
{
    UbseSerialization out;
    UbseMemObmmInfo ubseMemObmmInfo;
    EXPECT_NO_THROW(UbseMemObmmInfoSerialization(out, ubseMemObmmInfo));
}

/*
 * 用例描述：测试 UbseMemObmmInfoDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemObmmInfoDeserialization)
{
    UbseDeSerialization in;
    UbseMemObmmInfo ubseMemObmmInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemObmmInfoDeserialization(in, ubseMemObmmInfo));
    EXPECT_TRUE(UbseMemObmmInfoDeserialization(in, ubseMemObmmInfo));
}

/*
 * 用例描述：测试 UbseMemExportStatusSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemExportStatusSerialization)
{
    UbseSerialization out;
    UbseMemExportStatus ubseMemExportStatus;
    ubseMemExportStatus.exportObmmInfo.emplace_back(UbseMemObmmInfo());
    EXPECT_NO_THROW(UbseMemExportStatusSerialization(out, ubseMemExportStatus));
}

/*
 * 用例描述：测试 UbseMemExportStatusDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemExportStatusDeserialization)
{
    UbseDeSerialization in;
    UbseMemExportStatus ubseMemExportStatus;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemExportStatusDeserialization(in, ubseMemExportStatus));
    EXPECT_TRUE(UbseMemExportStatusDeserialization(in, ubseMemExportStatus));
}

/*
 * 用例描述：测试 UbseUdsInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseUdsInfoSerialization)
{
    UbseSerialization out;
    UbseUdsInfo udsInfo;
    EXPECT_NO_THROW(UbseUdsInfoSerialization(out, udsInfo));
}

/*
 * 用例描述：测试 UbseUdsInfoDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseUdsInfoDeserialization)
{
    UbseDeSerialization in;
    UbseUdsInfo udsInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseUdsInfoDeserialization(in, udsInfo));
    EXPECT_TRUE(UbseUdsInfoDeserialization(in, udsInfo));
}

/*
 * 用例描述：测试 UbseNumaLocationSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseNumaLocationSerialization)
{
    UbseSerialization out;
    UbseNumaLocation ubseNumaLocation;
    EXPECT_NO_THROW(UbseNumaLocationSerialization(out, ubseNumaLocation));
}

/*
 * 用例描述：测试 UbseNumaLocationDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseNumaLocationDeserialization)
{
    UbseDeSerialization in;
    UbseNumaLocation ubseNumaLocation;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseNumaLocationDeserialization(in, ubseNumaLocation));
    EXPECT_TRUE(UbseNumaLocationDeserialization(in, ubseNumaLocation));
}

/*
 * 用例描述：测试 UbseMemFdBorrowReqSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdBorrowReqSerialization)
{
    UbseSerialization out;
    UbseMemFdBorrowReq req;
    req.lenderLocs.emplace_back(UbseNumaLocation());
    uint64_t size = 1;
    req.lenderSizes.emplace_back(size);
    EXPECT_NO_THROW(UbseMemFdBorrowReqSerialization(out, req));
}

/*
 * 用例描述：测试 UbseMemFdBorrowReqDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdBorrowReqDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdBorrowReq req;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false));
    EXPECT_FALSE(UbseMemFdBorrowReqDeserialization(in, req));
    MOCKER_CPP(&UbseDeSerialization::Check).reset();
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(true));
    EXPECT_TRUE(UbseMemFdBorrowReqDeserialization(in, req));
}

/*
 * 用例描述：测试 UbseMemFdBorrowExportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdBorrowExportObjSerialization)
{
    UbseSerialization out;
    UbseMemFdBorrowExportObj fdBorrowExportObj;
    EXPECT_NO_THROW(UbseMemFdBorrowExportObjSerialization(out, fdBorrowExportObj));
}

/*
 * 用例描述：测试 UbseMemFdBorrowExportObjDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟UbseMemAlgoResultDeserialization返回UBSE_ERROR的情况
 * 3.模拟UbseMemExportStatusDeserialization返回UBSE_ERROR的情况
 * 4.模拟UbseMemFdBorrowReqDeserialization返回UBSE_ERROR的情况
 * 5.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_ERROR
 * 5.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdBorrowExportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdBorrowExportObj fdBorrowExportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
    MOCKER_CPP(UbseMemExportStatusDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
    MOCKER_CPP(UbseMemFdBorrowReqDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
    EXPECT_TRUE(UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
}

/*
 * 用例描述：测试 UbseMemImportResultSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemImportResultSerialization)
{
    UbseSerialization out;
    UbseMemImportResult ubseMemImportResult;
    EXPECT_NO_THROW(UbseMemImportResultSerialization(out, ubseMemImportResult));
}

/*
 * 用例描述：测试 UbseMemImportResultDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemImportResultDeserialization)
{
    UbseDeSerialization in;
    UbseMemImportResult ubseMemImportResult;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemImportResultDeserialization(in, ubseMemImportResult));
    EXPECT_TRUE(UbseMemImportResultDeserialization(in, ubseMemImportResult));
}

/*
 * 用例描述：测试 UbseMemImportStatusSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemImportStatusSerialization)
{
    UbseSerialization out;
    UbseMemImportStatus ubseMemImportStatus;
    ubseMemImportStatus.importResults.emplace_back(UbseMemImportResult());
    EXPECT_NO_THROW(UbseMemImportStatusSerialization(out, ubseMemImportStatus));
}

/*
 * 用例描述：测试 UbseMemImportStatusDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemImportStatusDeserialization)
{
    UbseDeSerialization in;
    UbseMemImportStatus ubseMemImportStatus;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemImportStatusDeserialization(in, ubseMemImportStatus));
    EXPECT_TRUE(UbseMemImportStatusDeserialization(in, ubseMemImportStatus));
}

/*
 * 用例描述：测试 UbseMemFdBorrowImportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdBorrowImportObjSerialization)
{
    UbseSerialization out;
    UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj;
    ubseMemFdBorrowImportObj.exportObmmInfo.emplace_back(UbseMemObmmInfo());
    EXPECT_NO_THROW(UbseMemFdBorrowImportObjSerialization(out, ubseMemFdBorrowImportObj));
}

/*
 * 用例描述：测试 UbseMemFdBorrowImportObjDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟UbseMemAlgoResultDeserialization返回UBSE_ERROR的情况
 * 3.模拟MUbseMemImportStatusDeserialization返回UBSE_ERROR的情况
 * 4.模拟UbseMemFdBorrowReqDeserialization返回UBSE_ERROR的情况
 * 5.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_ERROR
 * 5.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdBorrowImportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
    MOCKER_CPP(UbseMemImportStatusDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
    MOCKER_CPP(UbseMemFdBorrowReqDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
    EXPECT_TRUE(UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowReqSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaBorrowReqSerialization)
{
    UbseSerialization out;
    UbseMemNumaBorrowReq req;
    EXPECT_NO_THROW(UbseMemNumaBorrowReqSerialization(out, req));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowReqDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false时的情况
 * 2.模拟UbseMemFdBorrowReqDeserialization 返回UBSE_ERROR时的情况
 * 3.模拟一切功能正常时的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaBorrowReqDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaBorrowReq req;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowReqDeserialization(in, req));
    MOCKER_CPP(UbseMemFdBorrowReqDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowReqDeserialization(in, req));
    EXPECT_TRUE(UbseMemNumaBorrowReqDeserialization(in, req));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowExportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaBorrowExportObjSerialization)
{
    UbseSerialization out;
    UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj;
    EXPECT_NO_THROW(UbseMemNumaBorrowExportObjSerialization(out, ubseMemNumaBorrowExportObj));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowExportObjDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟UbseMemAlgoResultDeserialization返回UBSE_ERROR的情况
 * 3.模拟UbseMemExportStatusDeserialization返回UBSE_ERROR的情况
 * 4.模拟UbseMemNumaBorrowReqDeserialization返回UBSE_ERROR的情况
 * 5.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_ERROR
 * 5.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaBorrowExportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
    MOCKER_CPP(UbseMemExportStatusDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
    MOCKER_CPP(UbseMemNumaBorrowReqDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
    EXPECT_TRUE(UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowImportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaBorrowImportObjSerialization)
{
    UbseSerialization out;
    UbseMemNumaBorrowImportObj ubseMemNumaBorrowImportObj;
    ubseMemNumaBorrowImportObj.exportObmmInfo.emplace_back(UbseMemObmmInfo());
    EXPECT_NO_THROW(UbseMemNumaBorrowImportObjSerialization(out, ubseMemNumaBorrowImportObj));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowImportObjDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟UbseMemAlgoResultDeserialization返回UBSE_ERROR的情况
 * 3.模拟UbseMemImportStatusDeserialization返回UBSE_ERROR的情况
 * 4.模拟UbseMemNumaBorrowReqDeserialization返回UBSE_ERROR的情况
 * 5.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_ERROR
 * 5.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaBorrowImportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaBorrowImportObj ubseMemNumaBorrowImportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
    MOCKER_CPP(UbseMemImportStatusDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
    MOCKER_CPP(UbseMemNumaBorrowReqDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
    EXPECT_TRUE(UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
}

/*
 * 用例描述：测试 UbseMemAddrInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemAddrInfoSerialization)
{
    UbseSerialization out;
    UbseMemAddrInfo ubseMemAddrInfo;
    EXPECT_NO_THROW(UbseMemAddrInfoSerialization(out, ubseMemAddrInfo));
}

/*
 * 用例描述：测试 UbseMemAddrInfoDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemAddrInfoDeserialization)
{
    UbseDeSerialization in;
    UbseMemAddrInfo ubseMemAddrInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemAddrInfoDeserialization(in, ubseMemAddrInfo));
    EXPECT_TRUE(UbseMemAddrInfoDeserialization(in, ubseMemAddrInfo));
}

/*
 * 用例描述：测试 UbseMemBorrowExportBaseObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemBorrowExportBaseObjSerialization)
{
    UbseSerialization out;
    UbseMemBorrowExportBaseObj data;
    EXPECT_NO_THROW(UbseMemBorrowExportBaseObjSerialization(out, data));
}

/*
 * 用例描述：测试 UbseMemBorrowExportBaseObjDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟 UbseMemAlgoResultDeserialization 返回UBSE_ERROR的情况
 * 3.模拟 UbseMemExportStatusDeserialization 返回UBSE_ERROR的情况
 * 4.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemBorrowExportBaseObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemBorrowExportBaseObj data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemBorrowExportBaseObjDeserialization(in, data));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemBorrowExportBaseObjDeserialization(in, data));
    MOCKER_CPP(UbseMemExportStatusDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemBorrowExportBaseObjDeserialization(in, data));
    EXPECT_TRUE(UbseMemBorrowExportBaseObjDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseNodeInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseNodeInfoSerialization)
{
    UbseSerialization out;
    UbseNodeInfo data;
    EXPECT_NO_THROW(UbseNodeInfoSerialization(out, data));
}

/*
 * 用例描述：测试 UbseNodeInfoDeserialization 方法
 * 测试步骤：按顺序判断Check返回false和true时函数的返回结果。
 * 预期结果：Check返回false时，函数返回UBSE_ERROR；否则返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseNodeInfoDeserialization)
{
    UbseDeSerialization in;
    UbseNodeInfo data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseNodeInfoDeserialization(in, data));
    EXPECT_TRUE(UbseNodeInfoDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemBorrowImportBaseObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemBorrowImportBaseObjSerialization)
{
    UbseSerialization out;
    UbseMemBorrowImportBaseObj data;
    data.exportObmmInfo.push_back(UbseMemObmmInfo());
    EXPECT_NO_THROW(UbseMemBorrowImportBaseObjSerialization(out, data));
}

/*
 * 用例描述：测试 UbseMemBorrowImportBaseObjDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟 UbseMemAlgoResultDeserialization 返回UBSE_ERROR的情况
 * 3.模拟 UbseMemImportStatusDeserialization 返回UBSE_ERROR的情况
 * 4.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, UbseMemBorrowImportBaseObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemBorrowImportBaseObj data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemBorrowImportBaseObjDeserialization(in, data));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemBorrowImportBaseObjDeserialization(in, data));
    MOCKER_CPP(UbseMemImportStatusDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemBorrowImportBaseObjDeserialization(in, data));
    EXPECT_TRUE(UbseMemBorrowImportBaseObjDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemFdImportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdImportObjMapSerialization)
{
    UbseSerialization out;
    UbseMemFdImportObjMap data;
    data.insert(std::make_pair("first", UbseMemFdBorrowImportObj()));
    EXPECT_NO_THROW(UbseMemFdImportObjMapSerialization(out, data));
}

/*
 * 用例描述：测试 UbseMemFdImportObjMapDeserialization 方法。
 * 测试步骤：略
 * 预期结果：Check成功时返回UBSE_OK；失败时返回UBSE_ERROR
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdImportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdImportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdImportObjMapDeserialization(in, data));
    EXPECT_TRUE(UbseMemFdImportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemFdExportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdExportObjMapSerialization)
{
    UbseSerialization out;
    UbseMemFdExportObjMap data;
    data.insert(std::make_pair("first", UbseMemFdBorrowExportObj()));
    EXPECT_NO_THROW(UbseMemFdExportObjMapSerialization(out, data));
}

/*
 * 用例描述：测试 UbseMemFdExportObjMapDeserialization 方法。
 * 测试步骤：略
 * 预期结果：Check成功时返回UBSE_OK；失败时返回UBSE_ERROR
 */
TEST_F(TestUbseMemControllerSerial, UbseMemFdExportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdExportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemFdExportObjMapDeserialization(in, data));
    EXPECT_TRUE(UbseMemFdExportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemNumaImportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaImportObjMapSerialization)
{
    UbseSerialization out;
    UbseMemNumaImportObjMap data;
    data.insert(std::make_pair("first", UbseMemNumaBorrowImportObj()));
    EXPECT_NO_THROW(UbseMemNumaImportObjMapSerialization(out, data));
}

/*
 * 用例描述：测试 UbseMemNumaImportObjMapDeserialization 方法。
 * 测试步骤：略
 * 预期结果：Check成功时返回UBSE_OK；失败时返回UBSE_ERROR
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaImportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaImportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaImportObjMapDeserialization(in, data));
    EXPECT_TRUE(UbseMemNumaImportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemNumaExportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaExportObjMapSerialization)
{
    UbseSerialization out;
    UbseMemNumaExportObjMap data;
    data.insert(std::make_pair("first", UbseMemNumaBorrowExportObj()));
    EXPECT_NO_THROW(UbseMemNumaExportObjMapSerialization(out, data));
}

/*
 * 用例描述：测试 UbseMemNumaExportObjMapDeserialization 方法。
 * 测试步骤：略
 * 预期结果：Check成功时返回UBSE_OK；失败时返回UBSE_ERROR
 */
TEST_F(TestUbseMemControllerSerial, UbseMemNumaExportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaExportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(UbseMemNumaExportObjMapDeserialization(in, data));
    EXPECT_TRUE(UbseMemNumaExportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 NodeMemDebtInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TestUbseMemControllerSerial, NodeMemDebtInfoSerialization)
{
    UbseSerialization out;
    NodeMemDebtInfo data;
    EXPECT_NO_THROW(NodeMemDebtInfoSerialization(out, data));
}

/*
 * 用例描述：测试 NodeMemDebtInfoDeserialization 方法
 * 测试步骤：
 * 1.模拟Check返回false的情况
 * 2.模拟 UbseMemFdImportObjMapDeserialization 返回UBSE_ERROR的情况
 * 3.模拟 UbseMemFdExportObjMapDeserialization 返回UBSE_ERROR的情况
 * 4.模拟 UbseMemNumaImportObjMapDeserialization 返回UBSE_ERROR的情况
 * 5.模拟 UbseMemNumaExportObjMapDeserialization 返回UBSE_ERROR的情况
 * 6.模拟一切功能正常的情况
 * 预期结果：
 * 1.返回UBSE_ERROR
 * 2.返回UBSE_ERROR
 * 3.返回UBSE_ERROR
 * 4.返回UBSE_ERROR
 * 5.返回UBSE_ERROR
 * 6.返回UBSE_OK
 */
TEST_F(TestUbseMemControllerSerial, NodeMemDebtInfoDeserialization)
{
    UbseDeSerialization in;
    NodeMemDebtInfo data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemFdImportObjMapDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemFdExportObjMapDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemNumaImportObjMapDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemNumaExportObjMapDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_FALSE(NodeMemDebtInfoDeserialization(in, data));
    EXPECT_TRUE(NodeMemDebtInfoDeserialization(in, data));
}
} // namespace ubse::mem::serial::ut