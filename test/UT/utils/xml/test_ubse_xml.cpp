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

#include "test_ubse_xml.h"

#include <regex>

#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_pointer_process.h"
#include "src/framework/xml/ubse_xml.h"

namespace ubse::ut::utils {
using namespace ubse::utils;
void TestUbseXml::SetUp() {}

void TestUbseXml::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseXml, test_parse_ubse_xml)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
                             <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
                                 <host-bus-instance-eid>0x00412</host-bus-instance-eid>
                                 <upi>0x0041</upi>
                             </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    const auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    auto child = ubseXml->Child("upi");
    if (child) {
        EXPECT_EQ("0x0041", child->Text());
    }

    child = ubseXml->Child("host-bus-instance-eid");
    if (child) {
        EXPECT_EQ("0x00412", child->Text());
    }
}

TEST_F(TestUbseXml, test_parse_ubse_xml_twice)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
                             <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
                                 <host-bus-instance-eid>0x00412</host-bus-instance-eid>
                                 <upi>0x0041</upi>
                             </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    UbseXmlError ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);
    ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    auto child = ubseXml->Child("upi");
    if (child) {
        EXPECT_EQ("0x0041", child->Text());
    }

    child = ubseXml->Child("host-bus-instance-eid");
    if (child) {
        EXPECT_EQ("0x00412", child->Text());
    }
}

TEST_F(TestUbseXml, test_parse_not_vaild_ubse_xml_failed)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
                             <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
                                 <host-bus-instance-eid>0x00412</host-bus-instance-eid>
                                 <upi>0x0041</up>
                             </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    UbseXmlError ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::ERROR_XML_PARSE, ret);
}

TEST_F(TestUbseXml, test_parse_ubse_xml_without_declaration)
{
    // given
    std::string xmlData = R"(<guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
                                 <host-bus-instance-eid>0x00412</host-bus-instance-eid>
                                 <upi>0x0041</upi>
                            </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    const auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    auto child = ubseXml->Child("upi");
    if (child) {
        EXPECT_EQ("0x0041", child->Text());
    }

    child = ubseXml->Child("host-bus-instance-eid");
    if (child) {
        EXPECT_EQ("0x00412", child->Text());
    }
}

TEST_F(TestUbseXml, test_parse_ubse_xml_not_found_error)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    const auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    auto child = ubseXml->Child("unknow-node");

    // then
    EXPECT_EQ("", child->Name());
}

TEST_F(TestUbseXml, test_delete_node_successful)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    const auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    auto child = ubseXml->Child("host-bus-instance-eid");
    EXPECT_EQ("host-bus-instance-eid", child->Name());

    ubseXml->DeleteNode("host-bus-instance-eid");

    // then
    child = ubseXml->Child("host-bus-instance-eid");
    EXPECT_EQ("", child->Name());
}

TEST_F(TestUbseXml, test_go_to_previous_node)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);

    auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    auto next = ubseXml->Next("host-bus-instance-eid");
    EXPECT_NE(nullptr, next);

    ret = ubseXml->Previous();

    // then
    EXPECT_EQ(UbseXmlError::OK, ret);
    EXPECT_EQ(ubseXml->Name(), "guest-host-create");
    EXPECT_EQ(ubseXml->Attr("xmlns"), "urn:huawei:yang:huawei-lingqu-service");
}

TEST_F(TestUbseXml, test_set_xmlns_attr_successful)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    ubseXml->Attr("xmlns", "test");

    // then
    EXPECT_EQ(UbseXmlError::OK, ret);
    EXPECT_EQ(ubseXml->Attr("xmlns"), "test");
}

TEST_F(TestUbseXml, test_set_attr_successful)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    ubseXml->Attr("value", "test");

    // then
    EXPECT_EQ(UbseXmlError::OK, ret);
    EXPECT_EQ(ubseXml->Attr("value"), "test");
}

TEST_F(TestUbseXml, test_access_array_element)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
<logic-entity-mappings>
   <logic-entity-mapping>
       <logic-entity-bus-instance-eid>0x00412</logic-entity-bus-instance-eid>
       <physical-entity-mappings>
           <physical-entity-mapping>
               <index>1.1.1.8.1</index>
               <index>1.1.1.8.2</index>
           </physical-entity-mapping>
       </physical-entity-mappings>
   </logic-entity-mapping>
</logic-entity-mappings>
)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    ubseXml = ubseXml->Next("logic-entity-mapping");
    EXPECT_NE(nullptr, ubseXml);

    ubseXml = ubseXml->Next("physical-entity-mappings");
    EXPECT_NE(nullptr, ubseXml);

    ubseXml = ubseXml->Next("physical-entity-mapping");
    EXPECT_NE(nullptr, ubseXml);

    // then
    EXPECT_EQ(ubseXml->Child("index", 1)->Text(), "1.1.1.8.2");
}

TEST_F(TestUbseXml, test_get_deepth)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    // then
    EXPECT_EQ(UbseXmlError::OK, ret);
    EXPECT_EQ(ubseXml->GetDeepth(), 0);
}

TEST_F(TestUbseXml, test_set_name_successful)
{
    // given
    std::string xmlData = R"(<?xml version="1.0" encoding="UTF-8"?>
   <guest-host-create xmlns="urn:huawei:yang:huawei-lingqu-service">
   <host-bus-instance-eid>0x00412</host-bus-instance-eid>
   <upi>0x0041</upi>
   </guest-host-create>)";

    // when
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(xmlData);
    ASSERT_NE(ubseXml, nullptr);
    auto ret = ubseXml->Parse();
    EXPECT_EQ(UbseXmlError::OK, ret);

    ubseXml->Name("test");

    // then
    EXPECT_EQ(UbseXmlError::OK, ret);
    EXPECT_EQ(ubseXml->Name(), "test");
}
} // namespace ubse::ut::utils