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

#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ut_dir.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "src/adapter_plugins/urma_uvs/ubse_urma_topo_config.h"

namespace ubse::ut::urma {
using namespace ubse::common::def;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::urma;

class TestUbseUrmaTopoConfig : public testing::Test {
public:
    void SetUp() override
    {
        tempDir_ = std::filesystem::temp_directory_path() / ("ubse_urma_topo_config_ut_" + std::to_string(getpid()));
        std::filesystem::create_directories(tempDir_);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tempDir_);
        GlobalMockObject::verify();
    }

protected:
    std::filesystem::path WriteConfig(const std::string& fileName, const std::string& content) const
    {
        auto filePath = tempDir_ / fileName;
        std::ofstream file(filePath);
        file << content;
        file.close();
        return filePath;
    }

    std::filesystem::path tempDir_;
};

TEST_F(TestUbseUrmaTopoConfig, GetUrmaTopoModeReturnsNonCrossWhenConfigModuleMissing)
{
    std::shared_ptr<UbseConfModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule));

    EXPECT_EQ(GetUrmaTopoMode(), UbseUrmaTopoMode::NON_CROSS);
}

TEST_F(TestUbseUrmaTopoConfig, GetUrmaTopoModeReturnsNonCrossWhenGetConfigFails)
{
    auto module = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(GetUrmaTopoMode(), UbseUrmaTopoMode::NON_CROSS);
}

TEST_F(TestUbseUrmaTopoConfig, GetUrmaTopoModeReturnsHccsCross)
{
    auto module = std::make_shared<UbseConfModule>();
    std::string topoMode = "hccs-cross";
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.urma")), eq(std::string("topo_mode")), outBound(topoMode))
        .will(returnValue(UBSE_OK));

    EXPECT_EQ(GetUrmaTopoMode(), UbseUrmaTopoMode::HCCS_CROSS);
}

TEST_F(TestUbseUrmaTopoConfig, GetUrmaTopoModeReturnsNonCrossWhenConfigInvalid)
{
    auto module = std::make_shared<UbseConfModule>();
    std::string topoMode = "invalid";
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.urma")), eq(std::string("topo_mode")), outBound(topoMode))
        .will(returnValue(UBSE_OK));

    EXPECT_EQ(GetUrmaTopoMode(), UbseUrmaTopoMode::NON_CROSS);
}

TEST_F(TestUbseUrmaTopoConfig, LoadUrmaTopoConfigReturnsParseError)
{
    MOCKER_CPP(&ParseUrmaTopoConfig)
        .stubs()
        .with(eq(std::string("/etc/ubse/topo/non-cross.json")), any())
        .will(returnValue(UBSE_ERROR_FILE_NOT_EXIST));

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(LoadUrmaTopoConfig(UbseUrmaTopoMode::NON_CROSS, topoConfig), UBSE_ERROR_FILE_NOT_EXIST);
}

TEST_F(TestUbseUrmaTopoConfig, LoadUrmaTopoConfigReturnsErrorWhenLinkTypeMismatch)
{
    UbseUrmaTopoConfig parsedConfig;
    parsedConfig.linkType = "non-cross";
    MOCKER_CPP(&ParseUrmaTopoConfig)
        .stubs()
        .with(eq(std::string("/etc/ubse/topo/hccs-cross.json")), outBound(parsedConfig))
        .will(returnValue(UBSE_OK));

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(LoadUrmaTopoConfig(UbseUrmaTopoMode::HCCS_CROSS, topoConfig), UBSE_ERROR_INVAL);
    EXPECT_EQ(topoConfig.linkType, "non-cross");
}

TEST_F(TestUbseUrmaTopoConfig, LoadUrmaTopoConfigSuccess)
{
    UbseUrmaTopoConfig parsedConfig;
    parsedConfig.version = "1.0";
    parsedConfig.linkType = "hccs-cross";
    MOCKER_CPP(&ParseUrmaTopoConfig)
        .stubs()
        .with(eq(std::string("/etc/ubse/topo/hccs-cross.json")), outBound(parsedConfig))
        .will(returnValue(UBSE_OK));

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(LoadUrmaTopoConfig(UbseUrmaTopoMode::HCCS_CROSS, topoConfig), UBSE_OK);
    EXPECT_EQ(topoConfig.version, "1.0");
    EXPECT_EQ(topoConfig.linkType, "hccs-cross");
}

/*
 * 用例描述：解析合法的 URMA 拓扑配置文件。
 * 预期结果：返回 UBSE_OK，并正确填充版本、节点类型、链路类型、端口和链路信息。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseUrmaTopoConfigSuccess)
{
    auto filePath = WriteConfig("valid.json", R"({
        "version": "1.0",
        "node_type": "normal",
        "node_ports": [
            "1/1/1",
            "1/1/2",
            "2/1/1",
            "2/1/2"
        ],
        "link_type": "hccs-cross",
        "links": [
            {
                "local_port": "1/1/1",
                "remote_port": "2/1/1"
            },
            {
                "local_port": "1/1/2",
                "remote_port": "2/1/2"
            }
        ]
    })");

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig(filePath.string(), topoConfig), UBSE_OK);
    EXPECT_EQ(topoConfig.version, "1.0");
    EXPECT_EQ(topoConfig.nodeType, "normal");
    EXPECT_EQ(topoConfig.linkType, "hccs-cross");
    ASSERT_EQ(topoConfig.nodePorts.size(), 4);
    EXPECT_EQ(topoConfig.nodePorts[0].chipId, 1);
    EXPECT_EQ(topoConfig.nodePorts[0].dieId, 1);
    EXPECT_EQ(topoConfig.nodePorts[0].portId, 1);
    ASSERT_EQ(topoConfig.links.size(), 2);
    EXPECT_EQ(topoConfig.links[0].localPort.chipId, 1);
    EXPECT_EQ(topoConfig.links[0].remotePort.chipId, 2);
}

/*
 * 用例描述：解析不存在的 URMA 拓扑配置文件。
 * 预期结果：返回 UBSE_ERROR_FILE_NOT_EXIST。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseUrmaTopoConfigFailsWhenFileNotExist)
{
    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig((tempDir_ / "not_exist.json").string(), topoConfig), UBSE_ERROR_FILE_NOT_EXIST);
}

/*
 * 用例描述：解析 JSON 格式非法的 URMA 拓扑配置文件。
 * 预期结果：返回 UBSE_ERROR_INVAL。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseUrmaTopoConfigFailsWhenJsonInvalid)
{
    auto filePath = WriteConfig("invalid_json.json", R"({"version": "1.0",)");

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig(filePath.string(), topoConfig), UBSE_ERROR_INVAL);
}

/*
 * 用例描述：解析缺少必填链路字段的 URMA 拓扑配置文件。
 * 预期结果：返回 UBSE_ERROR_INVAL。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseUrmaTopoConfigFailsWhenRequiredFieldMissing)
{
    auto filePath = WriteConfig("missing_field.json", R"({
        "version": "1.0",
        "node_type": "normal",
        "node_ports": [
            "1/1/1"
        ],
        "link_type": "non-cross",
        "links": [
            {
                "local_port": "1/1/1"
            }
        ]
    })");

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig(filePath.string(), topoConfig), UBSE_ERROR_INVAL);
}

/*
 * 用例描述：解析端口格式不符合 chip/die/port 的 URMA 拓扑配置文件。
 * 预期结果：返回 UBSE_ERROR_INVAL。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseUrmaTopoConfigFailsWhenPortFormatInvalid)
{
    auto filePath = WriteConfig("invalid_port.json", R"({
        "version": "1.0",
        "node_type": "normal",
        "node_ports": [
            "1/1"
        ],
        "link_type": "non-cross",
        "links": []
    })");

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig(filePath.string(), topoConfig), UBSE_ERROR_INVAL);
}

/*
 * 用例描述：解析仓库内置的 non-cross 拓扑配置文件。
 * 预期结果：链路类型为 non-cross，端口数量和同 chip 同 port 的链路关系符合预置配置。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseBuiltinNonCrossTopoConfig)
{
    auto filePath =
        std::filesystem::path(UT_DIRECTORY).parent_path().parent_path() / "conf" / "topo" / "non-cross.json";

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig(filePath.string(), topoConfig), UBSE_OK);
    EXPECT_EQ(topoConfig.linkType, "non-cross");
    ASSERT_EQ(topoConfig.nodePorts.size(), 4);
    EXPECT_EQ(topoConfig.nodePorts[0].chipId, 1);
    EXPECT_EQ(topoConfig.nodePorts[0].dieId, 1);
    ASSERT_EQ(topoConfig.links.size(), 4);
    EXPECT_EQ(topoConfig.links[0].localPort.chipId, topoConfig.links[0].remotePort.chipId);
    EXPECT_EQ(topoConfig.links[0].localPort.portId, topoConfig.links[0].remotePort.portId);
}

/*
 * 用例描述：解析仓库内置的 hccs-cross 拓扑配置文件。
 * 预期结果：链路类型为 hccs-cross，端口数量和跨 chip 链路关系符合预置配置。
 */
TEST_F(TestUbseUrmaTopoConfig, ParseBuiltinHccsCrossTopoConfig)
{
    auto filePath =
        std::filesystem::path(UT_DIRECTORY).parent_path().parent_path() / "conf" / "topo" / "hccs-cross.json";

    UbseUrmaTopoConfig topoConfig;
    EXPECT_EQ(ParseUrmaTopoConfig(filePath.string(), topoConfig), UBSE_OK);
    EXPECT_EQ(topoConfig.linkType, "hccs-cross");
    ASSERT_EQ(topoConfig.nodePorts.size(), 4);
    EXPECT_EQ(topoConfig.nodePorts[0].chipId, 1);
    EXPECT_EQ(topoConfig.nodePorts[0].dieId, 1);
    ASSERT_EQ(topoConfig.links.size(), 8);
    EXPECT_EQ(topoConfig.links[1].localPort.chipId, 1);
    EXPECT_EQ(topoConfig.links[1].remotePort.chipId, 2);
}
} // namespace ubse::ut::urma
