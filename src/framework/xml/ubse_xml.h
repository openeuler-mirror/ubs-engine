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

#ifndef UBSE_XML_H
#define UBSE_XML_H

#include <memory>
#include <stdexcept>
#include <string>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>

namespace ubse::utils {
enum class UbseXmlError {
    OK = 0,
    NULLPTR,
    NOT_FOUND,
    ERROR_XML_PARSE,
};

class ListNode {
    // 头指针移动的链表
public:
    xmlNode *node{};
    std::shared_ptr<ListNode> next;
    int depth{};
};

class UbseXml : public std::enable_shared_from_this<UbseXml> {
public:
    explicit UbseXml(const std::string &xml);

    UbseXml();

    ~UbseXml();

    UbseXmlError Parse();

    void Printer(std::string &outputString, bool includeXmlDeclaration = true);

    // 头指针向下寻找
    std::shared_ptr<UbseXml> Next(const std::string &name, int num = 0);

    UbseXmlError Previous();

    // 将行为指针回到头指针
    void Back();

    // 运算子节点
    std::shared_ptr<UbseXml> Child(const std::string &name, int num = 0);

    // 子节点操作
    // name
    std::string Text();
    void Text(const std::string &text);

    // name
    std::string Name();
    void Name(const std::string &name);

    void SetXmlns(const std::string& uri);

    // attribute 增删改查
    std::string Attr(const std::string &key);
    void Attr(const std::string &key, const std::string &value);

    // 增加节点
    void AddNode(const std::string &name, bool isFirst = false);

    // 删除节点
    UbseXmlError DeleteNode(const std::string &name, int num = 0);

    // 获得当前节点的深度
    int GetDeepth();

private:
    xmlNode* FindNthChildByName(xmlNode* child, const std::string& name, int num) const;
    std::shared_ptr<UbseXml> UpdateToNode(xmlNode* nodePtr);

    xmlDoc *doc{};

    std::string xmlString;
    std::shared_ptr<ListNode> listNode{}; // 走过的路径
    xmlNode *rootNode{};              // 根节点
    xmlNode *curNode{};            // 当前操作的xmlelement
    xmlNode *node{};               // 当前的xmlelement
};
} // namespace ubse::utils
#endif

