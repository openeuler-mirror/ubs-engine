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
#include "ubse_conf_error.h"
#include "ubse_serial_util.h"

namespace ubse::mem::serial::ut {
void TesUbseMemControllerSerial::SetUp()
{
    Test::SetUp();
}

void TesUbseMemControllerSerial::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试 UbseMemDebtNumaInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：方法正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemDebtNumaInfoSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemDebtNumaInfoDeserialization)
{
    UbseDeSerialization in;
    UbseMemDebtNumaInfo debtNumaInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemDebtNumaInfoDeserialization(in, debtNumaInfo));
    EXPECT_TRUE(UBSE_OK == UbseMemDebtNumaInfoDeserialization(in, debtNumaInfo));
}

/*
 * 用例描述： 测试 UbseMemAlgoResultSerialization 方法
 * 测试步骤： 略
 * 预期结果： 程序正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemAlgoResultSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemAlgoResultDeserialization_WhenCheckFailed)
{
    UbseDeSerialization in;
    UbseMemAlgoResult algoResult;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false));
    EXPECT_TRUE(UBSE_ERROR == UbseMemAlgoResultDeserialization(in, algoResult));
}

/*
 * 用例描述：
 * 测试UbseMemAlgoResultDeserialization在一切功能正常时的行为
 * 测试步骤：略
 * 预期结果：返回UBSE_ERROR
 */
TEST_F(TesUbseMemControllerSerial, UbseMemAlgoResultDeserialization_WhenEverythingIsOk)
{
    UbseDeSerialization in;
    UbseMemAlgoResult algoResult;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(true));
    EXPECT_TRUE(UBSE_OK == UbseMemAlgoResultDeserialization(in, algoResult));
}

/*
 * 用例描述：测试 UbseMemObmmMemDescSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemObmmMemDescSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemObmmMemDescDeserialization)
{
    UbseDeSerialization in;
    ubse_mem_obmm_mem_desc ubseMemObmmMemDesc;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemObmmMemDescDeserialization(in, ubseMemObmmMemDesc));
    EXPECT_TRUE(UBSE_OK == UbseMemObmmMemDescDeserialization(in, ubseMemObmmMemDesc));
}

/*
 * 用例描述：测试 UbseMemObmmInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemObmmInfoSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemObmmInfoDeserialization)
{
    UbseDeSerialization in;
    UbseMemObmmInfo ubseMemObmmInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemObmmInfoDeserialization(in, ubseMemObmmInfo));
    EXPECT_TRUE(UBSE_OK == UbseMemObmmInfoDeserialization(in, ubseMemObmmInfo));
}

/*
 * 用例描述：测试 UbseMemExportStatusSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemExportStatusSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemExportStatusDeserialization)
{
    UbseDeSerialization in;
    UbseMemExportStatus ubseMemExportStatus;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemExportStatusDeserialization(in, ubseMemExportStatus));
    EXPECT_TRUE(UBSE_OK == UbseMemExportStatusDeserialization(in, ubseMemExportStatus));
}

/*
 * 用例描述：测试 UbseUdsInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseUdsInfoSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseUdsInfoDeserialization)
{
    UbseDeSerialization in;
    UbseUdsInfo udsInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseUdsInfoDeserialization(in, udsInfo));
    EXPECT_TRUE(UBSE_OK == UbseUdsInfoDeserialization(in, udsInfo));
}

/*
 * 用例描述：测试 UbseNumaLocationSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseNumaLocationSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseNumaLocationDeserialization)
{
    UbseDeSerialization in;
    UbseNumaLocation ubseNumaLocation;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseNumaLocationDeserialization(in, ubseNumaLocation));
    EXPECT_TRUE(UBSE_OK == UbseNumaLocationDeserialization(in, ubseNumaLocation));
}

/*
 * 用例描述：测试 UbseMemFdBorrowReqSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemFdBorrowReqSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemFdBorrowReqDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdBorrowReq req;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowReqDeserialization(in, req));
    EXPECT_TRUE(UBSE_OK == UbseMemFdBorrowReqDeserialization(in, req));
}

/*
 * 用例描述：测试 UbseMemFdBorrowExportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemFdBorrowExportObjSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemFdBorrowExportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdBorrowExportObj fdBorrowExportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
    MOCKER_CPP(UbseMemExportStatusDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
    MOCKER_CPP(UbseMemFdBorrowReqDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
    EXPECT_TRUE(UBSE_OK == UbseMemFdBorrowExportObjDeserialization(in, fdBorrowExportObj));
}

/*
 * 用例描述：测试 UbseMemImportResultSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemImportResultSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemImportResultDeserialization)
{
    UbseDeSerialization in;
    UbseMemImportResult ubseMemImportResult;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemImportResultDeserialization(in, ubseMemImportResult));
    EXPECT_TRUE(UBSE_OK == UbseMemImportResultDeserialization(in, ubseMemImportResult));
}

/*
 * 用例描述：测试 UbseMemImportStatusSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemImportStatusSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemImportStatusDeserialization)
{
    UbseDeSerialization in;
    UbseMemImportStatus ubseMemImportStatus;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemImportStatusDeserialization(in, ubseMemImportStatus));
    EXPECT_TRUE(UBSE_OK == UbseMemImportStatusDeserialization(in, ubseMemImportStatus));
}

/*
 * 用例描述：测试 UbseMemFdBorrowImportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemFdBorrowImportObjSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemFdBorrowImportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdBorrowImportObj ubseMemFdBorrowImportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
    MOCKER_CPP(UbseMemImportStatusDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
    MOCKER_CPP(UbseMemFdBorrowReqDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
    EXPECT_TRUE(UBSE_OK == UbseMemFdBorrowImportObjDeserialization(in, ubseMemFdBorrowImportObj));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowReqSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemNumaBorrowReqSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemNumaBorrowReqDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaBorrowReq req;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowReqDeserialization(in, req));
    MOCKER_CPP(UbseMemFdBorrowReqDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowReqDeserialization(in, req));
    EXPECT_TRUE(UBSE_OK == UbseMemNumaBorrowReqDeserialization(in, req));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowExportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemNumaBorrowExportObjSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemNumaBorrowExportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaBorrowExportObj ubseMemNumaBorrowExportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
    MOCKER_CPP(UbseMemExportStatusDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
    MOCKER_CPP(UbseMemNumaBorrowReqDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
    EXPECT_TRUE(UBSE_OK == UbseMemNumaBorrowExportObjDeserialization(in, ubseMemNumaBorrowExportObj));
}

/*
 * 用例描述：测试 UbseMemNumaBorrowImportObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemNumaBorrowImportObjSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemNumaBorrowImportObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaBorrowImportObj ubseMemNumaBorrowImportObj;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
    MOCKER_CPP(UbseMemImportStatusDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
    MOCKER_CPP(UbseMemNumaBorrowReqDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
    EXPECT_TRUE(UBSE_OK == UbseMemNumaBorrowImportObjDeserialization(in, ubseMemNumaBorrowImportObj));
}

/*
 * 用例描述：测试 UbseMemAddrInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemAddrInfoSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemAddrInfoDeserialization)
{
    UbseDeSerialization in;
    UbseMemAddrInfo ubseMemAddrInfo;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemAddrInfoDeserialization(in, ubseMemAddrInfo));
    EXPECT_TRUE(UBSE_OK == UbseMemAddrInfoDeserialization(in, ubseMemAddrInfo));
}

/*
 * 用例描述：测试 UbseMemBorrowExportBaseObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemBorrowExportBaseObjSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemBorrowExportBaseObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemBorrowExportBaseObj data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemBorrowExportBaseObjDeserialization(in, data));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemBorrowExportBaseObjDeserialization(in, data));
    MOCKER_CPP(UbseMemExportStatusDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemBorrowExportBaseObjDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseMemBorrowExportBaseObjDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseNodeInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseNodeInfoSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseNodeInfoDeserialization)
{
    UbseDeSerialization in;
    UbseNodeInfo data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseNodeInfoDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseNodeInfoDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemBorrowImportBaseObjSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemBorrowImportBaseObjSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemBorrowImportBaseObjDeserialization)
{
    UbseDeSerialization in;
    UbseMemBorrowImportBaseObj data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemBorrowImportBaseObjDeserialization(in, data));

    MOCKER_CPP(UbseMemAlgoResultDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemBorrowImportBaseObjDeserialization(in, data));
    MOCKER_CPP(UbseMemImportStatusDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == UbseMemBorrowImportBaseObjDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseMemBorrowImportBaseObjDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemFdImportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemFdImportObjMapSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemFdImportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdImportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdImportObjMapDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseMemFdImportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemFdExportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemFdExportObjMapSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemFdExportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemFdExportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemFdExportObjMapDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseMemFdExportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemNumaImportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemNumaImportObjMapSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemNumaImportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaImportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaImportObjMapDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseMemNumaImportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 UbseMemNumaExportObjMapSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, UbseMemNumaExportObjMapSerialization)
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
TEST_F(TesUbseMemControllerSerial, UbseMemNumaExportObjMapDeserialization)
{
    UbseDeSerialization in;
    UbseMemNumaExportObjMap data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == UbseMemNumaExportObjMapDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == UbseMemNumaExportObjMapDeserialization(in, data));
}

/*
 * 用例描述：测试 NodeMemDebtInfoSerialization 方法
 * 测试步骤：略
 * 预期结果：函数正常执行无异常
 */
TEST_F(TesUbseMemControllerSerial, NodeMemDebtInfoSerialization)
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
TEST_F(TesUbseMemControllerSerial, NodeMemDebtInfoDeserialization)
{
    UbseDeSerialization in;
    NodeMemDebtInfo data;
    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemFdImportObjMapDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemFdExportObjMapDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemNumaImportObjMapDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == NodeMemDebtInfoDeserialization(in, data));
    MOCKER_CPP(UbseMemNumaExportObjMapDeserialization).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == NodeMemDebtInfoDeserialization(in, data));
    EXPECT_TRUE(UBSE_OK == NodeMemDebtInfoDeserialization(in, data));
}
} // namespace ubse::mem::serial::ut