/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_cli_urma_cmd_reg.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_urma_cmd_reg.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::common::def;
using namespace ubse::serial;

void TestUbseCliUrmaCmdReg::SetUp() {}

void TestUbseCliUrmaCmdReg::TearDown() {}

/*
 * 用例描述：
 * 注册URMA模块测试
 * 测试步骤：
 * 1. 注册CLI_URMA_MODULE模块
 * 2. 验证注册成功
 * 预期结果：
 * 1. 模块注册成功，creators大小正确
 * 2. 模块签入成功，creators清空
 */
TEST_F(TestUbseCliUrmaCmdReg, RegisterUrmaModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_URMA_MODULE", UbseCliRegUrmaModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

/*
 * 用例描述：
 * ParseCommaSeparatedDeviceList解析逗号分隔设备列表测试
 * 测试步骤：
 * 1. 测试空字符串
 * 2. 测试单个设备
 * 3. 测试多个设备
 * 4. 测试带空格的设备
 * 5. 测试只有空格的字符串
 * 预期结果：
 * 1. 空字符串返回空列表
 * 2. 单个设备正确解析
 * 3. 多个设备正确解析
 * 4. 带空格的设备正确去除空格
 * 5. 只有空格的字符串返回空列表
 */
TEST_F(TestUbseCliUrmaCmdReg, ParseCommaSeparatedDeviceList)
{
    std::vector<std::string> result;

    result = UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList("");
    EXPECT_TRUE(result.empty());

    result = UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList("dev1");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "dev1");

    result = UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList("dev1,dev2,dev3");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "dev1");
    EXPECT_EQ(result[1], "dev2");
    EXPECT_EQ(result[2], "dev3");

    result = UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList(" dev1 , dev2 , dev3 ");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "dev1");
    EXPECT_EQ(result[1], "dev2");
    EXPECT_EQ(result[2], "dev3");

    result = UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList("   ");
    EXPECT_TRUE(result.empty());

    result = UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList(" , , ");
    EXPECT_TRUE(result.empty());
}

/*
 * 用例描述：
 * ParseAndValidateUrmaParams参数验证测试
 * 测试步骤：
 * 1. 测试无参数（使用默认值）
 * 2. 测试有效的node参数
 * 3. 测试无效的node参数（非数字）
 * 4. 测试有效的node和device参数
 * 5. 测试无效的device参数（空设备列表）
 * 6. 测试UINT32_MAX的node参数
 * 预期结果：
 * 1. 无参数时nodeId为UINT32_MAX，deviceNameList为空，返回nullptr
 * 2. 有效node参数正确解析，返回nullptr
 * 3. 无效node参数返回错误信息
 * 4. 有效node和device参数正确解析，返回nullptr
 * 5. 空device参数返回错误信息
 * 6. UINT32_MAX的node参数返回错误信息
 */
TEST_F(TestUbseCliUrmaCmdReg, ParseAndValidateUrmaParams)
{
    uint32_t nodeId = 0;
    std::vector<std::string> deviceNameList;
    std::map<std::string, std::string> params;
    std::shared_ptr<UbseCliResultEcho> result;

    params.clear();
    result = UbseCliRegUrmaModule::ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(nodeId, UINT32_MAX);
    EXPECT_TRUE(deviceNameList.empty());

    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(nodeId, 1);

    params.clear();
    params["node"] = "invalid";
    result = UbseCliRegUrmaModule::ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    EXPECT_NE(result, nullptr);

    params.clear();
    params["node"] = "1";
    params["dev"] = "dev1,dev2";
    result = UbseCliRegUrmaModule::ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(nodeId, 1);
    EXPECT_EQ(deviceNameList.size(), 2);
    EXPECT_EQ(deviceNameList[0], "dev1");
    EXPECT_EQ(deviceNameList[1], "dev2");

    params.clear();
    params["dev"] = " , , ";
    result = UbseCliRegUrmaModule::ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    EXPECT_NE(result, nullptr);

    params.clear();
    params["node"] = "4294967295";
    result = UbseCliRegUrmaModule::ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    EXPECT_NE(result, nullptr);
}

/*
 * 用例描述：
 * UbseQueryUrmaDevInfoFunc查询URMA设备信息测试
 * 测试步骤：
 * 1. 测试无效参数（缺少node和dev）
 * 2. 测试序列化失败
 * 3. 测试IPC调用失败（UBSE_ERROR）
 * 4. 测试IPC调用返回节点不存在（UBSE_ERR_NOT_EXIST）
 * 5. 测试IPC调用返回节点状态异常（UBSE_ERROR_INVAL）
 * 6. 测试IPC调用返回空列表
 * 7. 测试反序列化失败
 * 8. 测试正常查询
 * 预期结果：
 * 1. 无效参数返回错误信息
 * 2. 序列化失败返回错误信息
 * 3. IPC调用失败返回错误信息
 * 4. 返回节点不存在错误信息
 * 5. 返回节点状态异常错误信息
 * 6. 返回空列表错误信息
 * 7. 返回反序列化失败错误信息
 * 8. 正常返回设备信息表格
 */
TEST_F(TestUbseCliUrmaCmdReg, UbseQueryUrmaDevInfoFunc)
{
    std::map<std::string, std::string> params;
    std::shared_ptr<UbseCliResultEcho> result;

    params.clear();
    params["node"] = "invalid";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_NOT_EXIST));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_INVAL));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_dev_info_invoke_call_empty));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_dev_info_invoke_call_deserialize_failed));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_dev_info_invoke_call_normal));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * UbseActivateUrmaDevInfoFunc激活URMA设备信息测试
 * 测试步骤：
 * 1. 测试缺少node参数
 * 2. 测试无效的node参数
 * 3. 测试缺少dev参数
 * 4. 测试序列化失败
 * 5. 测试IPC调用失败（UBSE_ERROR_INVAL）
 * 6. 测试IPC调用失败（UBSE_ERROR_AGAIN）
 * 7. 测试IPC调用失败（UBSE_ERROR_DESERIALIZE_FAILED）
 * 8. 测试IPC调用失败（其他错误）
 * 9. 测试正常激活
 * 预期结果：
 * 1. 缺少node参数返回错误信息
 * 2. 无效node参数返回错误信息
 * 3. 缺少dev参数返回错误信息
 * 4. 序列化失败返回错误信息
 * 5. 返回节点状态异常错误信息
 * 6. 返回重试错误信息
 * 7. 返回反序列化失败错误信息
 * 8. 返回内部错误信息
 * 9. 返回激活成功信息
 */
TEST_F(TestUbseCliUrmaCmdReg, UbseActivateUrmaDevInfoFunc)
{
    std::map<std::string, std::string> params;
    std::shared_ptr<UbseCliResultEcho> result;

    params.clear();
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);

    params.clear();
    params["node"] = "invalid";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);

    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_INVAL));
    params.clear();
    params["node"] = "1";
    params["dev"] = "dev1";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_AGAIN));
    params.clear();
    params["node"] = "1";
    params["dev"] = "dev1";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR_DESERIALIZE_FAILED));
    params.clear();
    params["node"] = "1";
    params["dev"] = "dev1";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR));
    params.clear();
    params["node"] = "1";
    params["dev"] = "dev1";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    params.clear();
    params["node"] = "1";
    params["dev"] = "dev1";
    result = UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * UbseQueryUrmaQosFunc查询URMA QoS信息测试
 * 测试步骤：
 * 1. 测试缺少node参数
 * 2. 测试无效的node参数
 * 3. 测试IPC调用失败
 * 4. 测试IPC调用返回空列表
 * 5. 测试反序列化失败
 * 6. 测试正常查询
 * 预期结果：
 * 1. 缺少node参数返回错误信息
 * 2. 无效node参数返回错误信息
 * 3. IPC调用失败返回错误信息
 * 4. 返回空列表错误信息
 * 5. 返回反序列化失败错误信息
 * 6. 正常返回QoS信息表格
 */
TEST_F(TestUbseCliUrmaCmdReg, UbseQueryUrmaQosFunc)
{
    std::map<std::string, std::string> params;
    std::shared_ptr<UbseCliResultEcho> result;

    params.clear();
    result = UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params);
    EXPECT_NE(result, nullptr);

    params.clear();
    params["node"] = "invalid";
    result = UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params);
    EXPECT_NE(result, nullptr);

    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_qos_invoke_call_empty));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_qos_invoke_call_deserialize_failed));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();

    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_urma_qos_invoke_call_normal));
    params.clear();
    params["node"] = "1";
    result = UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(params);
    EXPECT_NE(result, nullptr);
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * UbseCliProcessUrmaDevInfoTable处理URMA设备信息表格测试
 * 测试步骤：
 * 1. 测试反序列化失败
 * 2. 测试状态值超出范围
 * 3. 测试正常处理
 * 预期结果：
 * 1. 反序列化失败返回错误信息
 * 2. 状态值超出范围返回错误信息
 * 3. 正常返回表格信息
 */
TEST_F(TestUbseCliUrmaCmdReg, UbseCliProcessUrmaDevInfoTable)
{
    uint8_t buffer[1024];
    uint32_t length = 0;

    UbseSerialization ser;
    uint32_t urmaSize = 1;
    ser << urmaSize;
    std::string urmaName = "urma1";
    uint32_t urmaType = 0;
    std::string devEid = "eid1";
    std::vector<std::string> feNames = {"dev1", "dev2"};
    std::vector<std::string> feEids = {"eid1", "eid2"};
    uint32_t urmaStatus = 0;
    ser << urmaName << urmaType << devEid << feNames << feEids << urmaStatus;

    length = static_cast<uint32_t>(ser.GetLength());
    if (memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), length) != EOK) {
        FAIL() << "memcpy_s failed";
    }

    UbseDeSerialization deser(buffer, length);
    auto result = UbseCliRegUrmaModule::UbseCliProcessUrmaDevInfoTable(deser, urmaSize);
    EXPECT_NE(result, nullptr);
}

/*
 * 用例描述：
 * UbseCliProcessUrmaQosTable处理URMA QoS表格测试
 * 测试步骤：
 * 1. 测试反序列化失败
 * 2. 测试正常处理
 * 预期结果：
 * 1. 反序列化失败返回错误信息
 * 2. 正常返回表格信息
 */
TEST_F(TestUbseCliUrmaCmdReg, UbseCliProcessUrmaQosTable)
{
    uint8_t buffer[1024];
    uint32_t length = 0;

    UbseSerialization ser;
    uint32_t urmaSize = 1;
    ser << urmaSize;
    std::string urmaName = "urma1";
    std::vector<std::string> feNames = {"dev1", "dev2"};
    uint32_t minBandWidth = 100;
    uint32_t maxBandWidth = 1000;
    ser << urmaName << feNames << minBandWidth << maxBandWidth;

    length = static_cast<uint32_t>(ser.GetLength());
    if (memcpy_s(buffer, sizeof(buffer), ser.GetBuffer(), length) != EOK) {
        FAIL() << "memcpy_s failed";
    }

    UbseDeSerialization deser(buffer, length);
    auto result = UbseCliRegUrmaModule::UbseCliProcessUrmaQosTable(deser, urmaSize);
    EXPECT_NE(result, nullptr);
}
} // namespace ubse::ut::cli