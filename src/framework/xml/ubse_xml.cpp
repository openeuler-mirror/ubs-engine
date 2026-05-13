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

#include "ubse_xml.h"

#include <algorithm>
#include <cctype>

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_pointer_process.h"

namespace ubse::utils {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;

UbseXml::UbseXml() {}

UbseXml::UbseXml(const std::string& xml)
{
    xmlString = xml;
}

/**
* 解析构造初始化的字符串
*
*/
UbseXmlError UbseXml::Parse()
{
    // 清理旧文档
    if (doc) {
        xmlFreeDoc(doc);
        doc = nullptr;
    }

    // 解析字符串
    doc = xmlReadMemory(xmlString.c_str(), static_cast<int>(xmlString.length()), "noname.xml", nullptr,
                        XML_PARSE_NOBLANKS);
    if (!doc) {
        return UbseXmlError::ERROR_XML_PARSE;
    }

    rootNode = xmlDocGetRootElement(doc);
    if (!rootNode) {
        return UbseXmlError::ERROR_XML_PARSE;
    }

    node = rootNode;

    listNode = SafeMakeShared<ListNode>();
    if (!listNode) {
        UBSE_LOG_ERROR << "Memory allocation failed for listNode";
        return UbseXmlError::NULLPTR;
    }
    listNode->depth = 0;
    listNode->node = node;
    listNode->next = nullptr;

    curNode = node;
    return UbseXmlError::OK;
}

/**
* 打印 xml 到内存字符串中
* @param outputString 输出字符串
* @param includeXmlDeclaration 是否包含 XML 声明（默认 true）
*/
void UbseXml::Printer(std::string& outputString, bool includeXmlDeclaration)
{
    outputString.clear();
    if (!doc) {
        return;
    }

    xmlBufferPtr buffer = nullptr;
    xmlSaveCtxtPtr saveCtxt = nullptr;

    // 创建输出内存缓冲区
    buffer = xmlBufferCreate();
    if (!buffer) {
        return;
    }

    // 创建保存上下文（save context）
    int saveOptions = XML_SAVE_FORMAT; // 格式化输出（缩进）
    if (!includeXmlDeclaration) {
        saveOptions |= XML_SAVE_NO_DECL; // 现代 libxml2 支持
    }

    saveCtxt = xmlSaveToBuffer(buffer, "UTF-8", saveOptions);
    if (saveCtxt == nullptr) {
        xmlBufferFree(buffer);
        return;
    }

    // 执行保存
    long result = xmlSaveDoc(saveCtxt, doc);
    xmlSaveClose(saveCtxt); // 自动释放 saveCtxt

    if (result >= 0 && buffer->content && buffer->use > 0) {
        outputString.assign(reinterpret_cast<char*>(buffer->content), buffer->use);
    } else {
        outputString.clear();
    }

    // 释放 buffer
    xmlBufferFree(buffer);
}

/**
* 返回下一个节点（child节点）
* @param name 节点名称
* @param num 第 num 个节点，从 0 开始
* @return shared_ptr to this (if found), else nullptr
*/
std::shared_ptr<UbseXml> UbseXml::Next(const std::string& name, int num)
{
    // 1. 输入校验
    if (!curNode || !curNode->parent || num < 0) {
        return nullptr;
    }

    // 2. 查找第 num 个匹配的子元素节点
    xmlNode* target = FindNthChildByName(curNode->children, name, num);
    if (!target) {
        return nullptr;
    }

    // 3. 更新当前节点和路径栈
    return UpdateToNode(target);
}
xmlNode* UbseXml::FindNthChildByName(xmlNode* child, const std::string& name, int num) const
{
    int matchCount = 0;
    const xmlChar* targetName = reinterpret_cast<const xmlChar*>(name.c_str());

    while (child) {
        if (child->type == XML_ELEMENT_NODE && xmlStrcmp(child->name, targetName) == 0) {
            if (matchCount == num) {
                return child;
            }
            ++matchCount;
        }
        child = child->next;
    }

    return nullptr;
}
std::shared_ptr<UbseXml> UbseXml::UpdateToNode(xmlNode* nodePtr)
{
    curNode = nodePtr;
    this->node = nodePtr; // 更新初始节点

    auto newList = SafeMakeShared<ListNode>();
    if (!newList) {
        return nullptr;
    }

    newList->node = nodePtr;
    newList->depth = listNode ? listNode->depth + 1 : 1;
    newList->next = listNode;
    listNode = newList;

    return shared_from_this();
}

/**
* 将头指针返回到上一个节点（路径回溯）
*/
UbseXmlError UbseXml::Previous()
{
    if (!listNode || !listNode->next) {
        return UbseXmlError::NULLPTR;
    }

    listNode = listNode->next; // 回退路径
    curNode = listNode->node;  // 恢复节点
    node = listNode->node;     // 恢复初始节点

    return UbseXmlError::OK;
}

/**
* 回到实际节点
* _simpleEle回到simpleEle
*/
void UbseXml::Back()
{
    curNode = node;
}

/**
* 返回第一个名为 name 的子节点（第 num 个）
* @param name 节点名称
* @param num 第 num 个匹配的子节点（从 0 开始）
* @return shared_ptr to this (if found), else nullptr
*/
std::shared_ptr<UbseXml> UbseXml::Child(const std::string& name, int num)
{
    if (!curNode) {
        return shared_from_this(); // 返回 this，但 curNode 为 nullptr
    }

    xmlNode* child = curNode->children;
    int matchCount = 0;

    while (child) {
        if (child->type == XML_ELEMENT_NODE &&
            xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>(name.c_str())) == 0) {
            if (matchCount == num) {
                curNode = child;
                return shared_from_this();
            }
            ++matchCount;
        }
        child = child->next;
    }

    // 未找到，设置 curNode 为 nullptr
    curNode = nullptr;
    return shared_from_this();
}

/**
* 获得元素的 text
* @return 文本内容，若节点为空则返回空字符串
*/
std::string UbseXml::Text()
{
    if (!curNode) {
        return "";
    }

    xmlChar* content = xmlNodeGetContent(curNode);
    std::string text = (content != nullptr) ? reinterpret_cast<char*>(content) : "";
    if (content) {
        xmlFree(content);
    }

    Back(); // 读取后重置当前指针
    return text;
}

/**
* 设置当前节点的文本内容
* @param text 文本字符串
*/
void UbseXml::Text(const std::string& text)
{
    if (!curNode) {
        return;
    }

    // 清除所有子节点（包括旧文本）
    xmlNodePtr child;
    while ((child = curNode->children) != nullptr) {
        xmlUnlinkNode(child);
        xmlFreeNode(child);
    }

    // 设置新文本
    xmlNodeSetContent(curNode, reinterpret_cast<const xmlChar*>(text.c_str()));

    Back(); // 恢复到初始节点
}

/**
* 获取当前节点的名称（标签名）
* @return 节点名，失败返回空字符串
*/
std::string UbseXml::Name()
{
    if (!curNode || !curNode->name) {
        return "";
    }

    std::string name = reinterpret_cast<const char*>(curNode->name);
    Back(); // 读取后重置
    return name;
}

/**
* 设置当前节点的名称（标签名）
* @param name 新的节点名
*/
void UbseXml::Name(const std::string& name)
{
    if (!curNode) {
        return;
    }

    // 修改节点名
    xmlNodeSetName(curNode, reinterpret_cast<const xmlChar*>(name.c_str()));
    Back(); // 恢复到初始节点
}

/**
* 读取属性值
* @param key 属性名
* @return 属性值，不存在则返回空字符串
*/
std::string UbseXml::Attr(const std::string& key)
{
    if (!curNode) {
        return "";
    }

    // 特殊处理：xmlns
    if (key == "xmlns") {
        xmlNsPtr ns = curNode->ns;
        if (ns && ns->href) {
            std::string result = reinterpret_cast<const char*>(ns->href);
            Back(); // 读取后重置
            return result;
        }

        // 或者从 nsDef 中查找默认命名空间
        ns = curNode->nsDef;
        while (ns) {
            if (!ns->prefix || xmlStrcmp(ns->prefix, BAD_CAST "") == 0) {
                std::string result = reinterpret_cast<const char*>(ns->href);
                Back();
                return result;
            }
            ns = ns->next;
        }
        Back();
        return "";
    }

    // 普通属性
    xmlChar* value = xmlGetProp(curNode, reinterpret_cast<const xmlChar*>(key.c_str()));
    std::string result = (value != nullptr) ? reinterpret_cast<char*>(value) : "";
    if (value) {
        xmlFree(value);
    }

    Back();
    return result;
}

/**
* 设置默认命名空间：xmlns="..."
*/
void UbseXml::SetXmlns(const std::string& uri)
{
    if (!curNode) {
        return;
    }

    // 如果 URI 为空，移除命名空间
    if (uri.empty()) {
        return;
    }

    // 创建新的命名空间定义（不会重复添加）
    xmlNsPtr ns = xmlNewNs(nullptr, reinterpret_cast<const xmlChar*>(uri.c_str()),
                           nullptr); // nullptr = 默认命名空间
    if (ns) {
        // 将命名空间绑定到当前节点
        xmlSetNs(curNode, ns);
    }
}

/**
* 设置属性值，如果 value 为空则删除属性
* @param key 属性名
* @param value 属性值，空字符串表示删除
*/
void UbseXml::Attr(const std::string& key, const std::string& value)
{
    if (!curNode) {
        return;
    }

    // 特殊处理：xmlns 命名空间声明
    if (key == "xmlns") {
        // 设置默认命名空间（无前缀）
        SetXmlns(value);
    }

    // 普通属性处理
    if (value.empty()) {
        // 删除普通属性
        xmlUnsetProp(curNode, reinterpret_cast<const xmlChar*>(key.c_str()));
    } else {
        // 设置普通属性
        xmlSetProp(curNode, reinterpret_cast<const xmlChar*>(key.c_str()),
                   reinterpret_cast<const xmlChar*>(value.c_str()));
    }

    Back(); // 按原逻辑，操作后回退
}

/**
* 添加节点
* @param name 节点名称
* @param isFirst 是否插入到当前节点之前（true=前，false=后）
*/
void UbseXml::AddNode(const std::string& name, bool isFirst)
{
    try {
        if (name.empty()) {
            throw std::invalid_argument("name can't be empty");
        }

        xmlNodePtr newNode = xmlNewNode(nullptr, reinterpret_cast<const xmlChar*>(name.c_str()));
        if (!newNode) {
            throw std::bad_alloc();
        }

        if (!rootNode || !node) {
            // 首次添加：设置为根节点
            rootNode = newNode;
            node = newNode;
            curNode = newNode;

            // 初始化路径栈
            listNode = std::make_shared<ListNode>();
            listNode->node = newNode;
            listNode->depth = 0;
            listNode->next = nullptr;

            // 确保 doc 存在，并将 rootNode 绑定为根元素
            if (!doc) {
                doc = xmlNewDoc(reinterpret_cast<const xmlChar*>("1.0"));
            }
            // 必须设置根元素，否则文档为空
            xmlDocSetRootElement(doc, rootNode);
        } else {
            // 添加为 curNode 的子节点
            if (isFirst && curNode->children) {
                xmlAddPrevSibling(curNode->children, newNode);
            } else {
                xmlAddChild(curNode, newNode);
            }
        }
        curNode = node;
    } catch (const std::invalid_argument& e) {
        UBSE_LOG_ERROR << "Invalid argument=" << e.what();
    } catch (const std::bad_alloc& e) {
        UBSE_LOG_ERROR << "Memory allocation failed=" << e.what();
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Unexpected error=" << e.what();
    }
}

/**
* 删除当前节点的第 num 个名为 name 的子节点
* @param name 节点名称
* @param num 第几个（从 0 开始）
* @return UbseXmlError::OK 或 NOT_FOUND
*/
UbseXmlError UbseXml::DeleteNode(const std::string& name, int num)
{
    if (!curNode) {
        return UbseXmlError::NOT_FOUND;
    }

    xmlNodePtr child = curNode->children;
    int matchCount = 0;

    while (child) {
        if (child->type == XML_ELEMENT_NODE &&
            xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>(name.c_str())) == 0) {
            if (matchCount == num) {
                // 找到目标节点，删除它
                xmlUnlinkNode(child); // 从树中移除
                xmlFreeNode(child);   // 释放内存
                return UbseXmlError::OK;
            }
            ++matchCount;
        }
        child = child->next;
    }

    return UbseXmlError::NOT_FOUND;
}

/**
* 获得当前节点的深度
* @return
*/
int UbseXml::GetDeepth()
{
    return this->listNode->depth;
}

/**
* 析构函数
*/
UbseXml::~UbseXml()
{
    xmlFreeDoc(doc);
    rootNode = nullptr;
    node = nullptr;
    curNode = nullptr;
}
} // namespace ubse::utils