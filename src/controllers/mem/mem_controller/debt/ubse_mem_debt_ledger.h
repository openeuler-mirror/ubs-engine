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

#ifndef UBSE_MEM_DEBT_LEDGER_H
#define UBSE_MEM_DEBT_LEDGER_H

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <unordered_map>

#include "adapter_plugins/mmi/ubse_mmi_def.h"

namespace ubse::mem::controller::debt {
using namespace ubse::adapter_plugins::mmi;

/**
 * @brief 类型特征模板，用于判断T是否为ExportObj类型
 * @tparam T 待判断的类型
 * @note 默认为false_type，通过特化支持具体的ExportObj类型
 */
template <typename T>
struct IsExportObjType : std::false_type {};

template <>
struct IsExportObjType<UbseMemFdBorrowExportObj> : std::true_type {};

template <>
struct IsExportObjType<UbseMemNumaBorrowExportObj> : std::true_type {};

template <>
struct IsExportObjType<UbseMemShareBorrowExportObj> : std::true_type {};

template <>
struct IsExportObjType<UbseMemAddrBorrowExportObj> : std::true_type {};

/**
 * @brief IsExportObjType的辅助变量模板
 * @tparam T 待判断的类型
 * @return 如果T是ExportObj类型返回true，否则返回false
 */
template <typename T>
inline constexpr bool IsExportObjTypeV = IsExportObjType<T>::value;

/**
 * @brief 类型特征模板，用于判断T是否为地址借用类型
 * @tparam T 待判断的类型
 * @note 默认为false_type，通过特化支持地址借用相关的ImportObj和ExportObj类型
 */
template <typename T>
struct IsAddrType : std::false_type {};

template <>
struct IsAddrType<UbseMemAddrBorrowImportObj> : std::true_type {};

template <>
struct IsAddrType<UbseMemAddrBorrowExportObj> : std::true_type {};

/**
 * @brief IsAddrType的辅助变量模板
 * @tparam T 待判断的类型
 * @return 如果T是地址借用类型返回true，否则返回false
 */
template <typename T>
inline constexpr bool IsAddrTypeV = IsAddrType<T>::value;

/**
 * @brief 共享指针映射类型别名
 * @tparam T 映射值的类型
 * @note 键为资源ID字符串，值为指向常量T对象的共享指针
 */
template <typename T>
using UbseSharedPtrMap = std::unordered_map<std::string, std::shared_ptr<const T>>;

/**
 * @brief 单节点内存债务信息结构体
 * @note 使用智能指针管理资源对象的生命周期，支持8种借用类型的资源映射
 */
struct UbseNodeMemDebtInfo {
    UbseSharedPtrMap<UbseMemFdBorrowImportObj> fdImportObjMap;       ///< FD类型借入资源映射
    UbseSharedPtrMap<UbseMemFdBorrowExportObj> fdExportObjMap;       ///< FD类型借出资源映射
    UbseSharedPtrMap<UbseMemNumaBorrowImportObj> numaImportObjMap;   ///< NUMA类型借入资源映射
    UbseSharedPtrMap<UbseMemNumaBorrowExportObj> numaExportObjMap;   ///< NUMA类型借出资源映射
    UbseSharedPtrMap<UbseMemShareBorrowImportObj> shareImportObjMap; ///< 共享内存类型借入资源映射
    UbseSharedPtrMap<UbseMemShareBorrowExportObj> shareExportObjMap; ///< 共享内存类型借出资源映射
    UbseSharedPtrMap<UbseMemAddrBorrowImportObj> addrImportObjMap;   ///< 地址类型借入资源映射
    UbseSharedPtrMap<UbseMemAddrBorrowExportObj> addrExportObjMap;   ///< 地址类型借出资源映射
};

/**
 * @brief 所有节点内存债务信息映射类型
 * @note 键为节点ID字符串，值为该节点的内存债务信息结构体
 */
using UbseNodeMemDebtInfoMap = std::unordered_map<std::string, UbseNodeMemDebtInfo>;

/**
 * @brief 单节点账本模板类
 * @tparam T 资源对象类型，支持各种BorrowImportObj和BorrowExportObj类型
 * @note 提供线程安全的资源管理能力，使用读写锁支持并发读写操作
 *       采用写时复制策略优化修改操作，适合读多写少的场景
 */
template <typename T>
class UbseMemNodeDebtMap {
public:
    UbseMemNodeDebtMap() = default;
    ~UbseMemNodeDebtMap() = default;

    /**
     * @brief 添加或更新资源对象
     * @param[in] resId 资源ID，作为映射的键
     * @param[in] obj 资源对象的共享指针
     * @note 线程安全，使用写锁保护
     */
    void Put(const std::string &resId, std::shared_ptr<T> obj)
    {
        if (!obj) {
            return;
        }
        std::unique_lock<std::shared_mutex> lock(mutex_);
        map_[resId] = obj;
    }

    /**
     * @brief 获取资源对象
     * @param[in] resId 资源ID
     * @return 指向常量资源对象的共享指针，如果资源不存在则返回nullptr
     * @note 线程安全，使用读锁保护
     */
    std::shared_ptr<const T> Get(const std::string &resId) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(resId);
        return it != map_.end() ? it->second : nullptr;
    }

    /**
     * @brief 删除资源对象
     * @param[in] resId 资源ID
     * @return 成功删除返回true，资源不存在返回false
     * @note 线程安全，使用写锁保护
     */
    bool Remove(const std::string &resId)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return map_.erase(resId) > 0;
    }

    /**
     * @brief 修改资源对象
     * @param[in] resId 资源ID
     * @param[in] modify 修改函数对象，接收资源对象的引用作为参数
     * @note 线程安全，采用写时复制策略：
     *       1. 先通过读锁获取旧对象
     *       2. 复制旧对象创建新对象
     *       3. 对新对象应用修改
     *       4. 通过写锁替换旧对象
     *       如果资源不存在，则不执行任何操作
     */
    void Modify(const std::string &resId, std::function<void(T &)> modify)
    {
        std::shared_ptr<const T> oldObj;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = map_.find(resId);
            if (it != map_.end()) {
                oldObj = it->second;
            }
        }
        if (!oldObj) {
            return;
        }
        auto newObj = std::make_shared<T>(*oldObj);
        modify(*newObj);
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            map_[resId] = std::const_pointer_cast<const T>(newObj);
        }
    }

    /**
     * @brief 获取所有资源对象的映射副本
     * @return 包含所有资源对象的映射副本，键为资源ID，值为资源对象的共享指针
     * @note 线程安全，使用读锁保护
     *       返回的是映射的副本，对返回值的修改不会影响内部数据
     */
    UbseSharedPtrMap<T> GetAll() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return map_;
    }

private:
    mutable std::shared_mutex mutex_;
    UbseSharedPtrMap<T> map_;
};

/**
 * @brief 单类型账本模板类
 * @tparam T 资源对象类型，支持各种BorrowImportObj和BorrowExportObj类型
 * @note 管理所有节点上某一特定类型的资源，提供节点级别的资源管理能力
 *       对于ExportObj类型，额外维护资源ID到节点ID的索引以支持跨节点资源查询
 */
template <typename T>
class UbseMemTypeDebtMap {
public:
    UbseMemTypeDebtMap() = default;
    ~UbseMemTypeDebtMap() = default;

    using NodeMap = std::unordered_map<std::string, std::shared_ptr<UbseMemNodeDebtMap<T>>>;
    using NodeMapConst = std::unordered_map<std::string, std::shared_ptr<const UbseMemNodeDebtMap<T>>>;

    /**
     * @brief 获取所有节点账本
     * @return 包含所有节点账本的映射，键为节点ID，值为节点账本的共享指针
     * @note 线程安全，使用读锁保护
     */
    NodeMapConst GetAllNodeMaps() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return {nodeMap_.begin(), nodeMap_.end()};
    }

    /**
     * @brief 获取或创建节点账本
     * @param[in] nodeId 节点ID
     * @return 节点账本的共享指针，如果不存在则创建新的节点账本
     * @note 线程安全，采用双重检查锁定模式优化性能
     */
    std::shared_ptr<UbseMemNodeDebtMap<T>> GetOrCreateNodeMap(const std::string &nodeId)
    {
        {
            // 先检查是否存在节点账本
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = nodeMap_.find(nodeId);
            if (it != nodeMap_.end()) {
                return it->second;
            }
        }
        // 未找到节点账本，创建新的
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto &nodePtr = nodeMap_[nodeId];
        if (!nodePtr) {
            nodePtr = std::make_shared<UbseMemNodeDebtMap<T>>();
        }
        return nodePtr;
    }

    /**
     * @brief 查找节点账本（非const版本）
     * @param[in] nodeId 节点ID
     * @return 节点账本的共享指针，如果不存在则返回nullptr
     * @note 线程安全，使用读锁保护
     */
    std::shared_ptr<UbseMemNodeDebtMap<T>> FindNodeMap(const std::string &nodeId)
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = nodeMap_.find(nodeId);
        return it != nodeMap_.end() ? it->second : nullptr;
    }

    /**
     * @brief 查找节点账本（const版本）
     * @param[in] nodeId 节点ID
     * @return 指向常量节点账本的共享指针，如果不存在则返回nullptr
     * @note 线程安全，使用读锁保护
     */
    std::shared_ptr<const UbseMemNodeDebtMap<T>> FindNodeMap(const std::string &nodeId) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = nodeMap_.find(nodeId);
        return it != nodeMap_.end() ? it->second : nullptr;
    }

    /**
     * @brief 删除节点账本
     * @param[in] nodeId 节点ID
     * @return 成功删除返回true，节点不存在返回false
     * @note 线程安全，使用写锁保护
     *       对于ExportObj类型，会同步清理资源ID索引中的相关条目
     */
    bool RemoveNodeMap(const std::string &nodeId)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if constexpr (IsExportObjTypeV<T>) {
            auto it = nodeMap_.find(nodeId);
            if (it != nodeMap_.end()) {
                auto allResources = it->second->GetAll();
                for (const auto &[resId, _] : allResources) {
                    exportResIdToNodeId_.erase(resId);
                }
            }
        }
        return nodeMap_.erase(nodeId) > 0;
    }

    /**
     * @brief 删除除指定节点外的所有节点账本
     * @param[in] excludeNodeId 要保留的节点ID
     * @note 线程安全，使用写锁保护
     *       对于ExportObj类型，会同步清理资源ID索引中的相关条目
     */
    void RemoveOtherNodeMaps(const std::string &excludeNodeId)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto it = nodeMap_.begin(); it != nodeMap_.end();) {
            if (it->first != excludeNodeId) {
                if constexpr (IsExportObjTypeV<T>) {
                    auto allResources = it->second->GetAll();
                    for (const auto &[resId, _] : allResources) {
                        exportResIdToNodeId_.erase(resId);
                    }
                }
                it = nodeMap_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief 获取某个节点的某个资源
     * @param[in] nodeId 节点ID
     * @param[in] resId 资源ID
     * @return 指向常量资源对象的共享指针，如果资源不存在则返回nullptr
     * @note 线程安全，内部调用FindNodeMap和Get方法
     */
    std::shared_ptr<const T> GetResource(const std::string &nodeId, const std::string &resId)
    {
        if (auto nodePtr = FindNodeMap(nodeId)) {
            return nodePtr->Get(resId);
        }
        return nullptr;
    }

    /**
     * @brief 通过资源ID查询导出资源（仅ExportObj类型有效）
     * @param[in] resId 导出资源ID
     * @return 指向常量资源对象的共享指针，如果资源不存在或类型不支持则返回nullptr
     * @note 线程安全，使用读锁保护
     *       仅对ExportObj类型有效，ImportObj类型始终返回nullptr
     *       利用资源ID到节点ID的索引实现跨节点资源快速查询
     */
    std::shared_ptr<const T> GetExportResourceByResId(const std::string &resId)
    {
        if constexpr (IsExportObjTypeV<T>) {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto indexIt = exportResIdToNodeId_.find(resId);
            if (indexIt == exportResIdToNodeId_.end()) {
                return nullptr;
            }
            auto nodeIt = nodeMap_.find(indexIt->second);
            if (nodeIt == nodeMap_.end()) {
                return nullptr;
            }
            return nodeIt->second->Get(resId);
        } else {
            return nullptr;
        }
    }

    /**
     * @brief 在某个节点添加或更新资源（共享指针版本）
     * @param[in] nodeId 节点ID
     * @param[in] resId 资源ID
     * @param[in] obj 资源对象的共享指针
     * @note 线程安全，如果节点账本不存在会自动创建
     *       对于ExportObj类型，会同步更新资源ID到节点ID的索引
     */
    void PutResource(const std::string &nodeId, const std::string &resId, std::shared_ptr<T> obj)
    {
        if (!obj) {
            return;
        }
        if constexpr (IsExportObjTypeV<T>) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto &nodePtr = nodeMap_[nodeId];
            if (!nodePtr) {
                nodePtr = std::make_shared<UbseMemNodeDebtMap<T>>();
            }
            nodePtr->Put(resId, obj);
            exportResIdToNodeId_[resId] = nodeId;
        } else {
            auto nodePtr = GetOrCreateNodeMap(nodeId);
            nodePtr->Put(resId, obj);
        }
    }

    /**
     * @brief 在某个节点添加或更新资源（对象引用版本）
     * @param[in] nodeId 节点ID
     * @param[in] resId 资源ID
     * @param[in] obj 资源对象的常量引用
     * @note 线程安全，内部会创建资源对象的副本
     *       对于ExportObj类型，会同步更新资源ID到节点ID的索引
     */
    void PutResource(const std::string &nodeId, const std::string &resId, const T &obj)
    {
        if constexpr (IsExportObjTypeV<T>) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto &nodePtr = nodeMap_[nodeId];
            if (!nodePtr) {
                nodePtr = std::make_shared<UbseMemNodeDebtMap<T>>();
            }
            nodePtr->Put(resId, std::make_shared<T>(obj));
            exportResIdToNodeId_[resId] = nodeId;
        } else {
            auto nodePtr = GetOrCreateNodeMap(nodeId);
            nodePtr->Put(resId, std::make_shared<T>(obj));
        }
    }

    /**
     * @brief 删除某个节点的某个资源
     * @param[in] nodeId 节点ID
     * @param[in] resId 资源ID
     * @return 成功删除返回true，资源或节点不存在返回false
     * @note 线程安全，对于ExportObj类型，会同步清理资源ID索引
     */
    bool RemoveResource(const std::string &nodeId, const std::string &resId)
    {
        if constexpr (IsExportObjTypeV<T>) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto it = nodeMap_.find(nodeId);
            if (it != nodeMap_.end() && it->second && it->second->Remove(resId)) {
                exportResIdToNodeId_.erase(resId);
                return true;
            }
            return false;
        } else {
            auto nodePtr = FindNodeMap(nodeId);
            if (nodePtr && nodePtr->Remove(resId)) {
                return true;
            }
            return false;
        }
    }

    /**
     * @brief 清空所有节点账本
     * @note 线程安全，使用写锁保护
     *       对于ExportObj类型，会同步清空资源ID索引
     */
    void ClearAllNodeMaps()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        nodeMap_.clear();
        if constexpr (IsExportObjTypeV<T>) {
            exportResIdToNodeId_.clear();
        }
    }

private:
    mutable std::shared_mutex mutex_;
    NodeMap nodeMap_;
    std::unordered_map<std::string, std::string> exportResIdToNodeId_;
};

/**
 * @brief 内存债务总账本类（单例模式）
 * @note 汇总管理所有类型的内存债务信息，提供统一的查询接口
 *       支持8种借用类型：FD/NUMA/Share/Addr × Import/Export
 *       使用std::tuple统一管理各类型账本，通过模板方法访问
 */
class UbseMemDebtLedger {
public:
    UbseMemDebtLedger(const UbseMemDebtLedger &) = delete;
    UbseMemDebtLedger &operator=(const UbseMemDebtLedger &) = delete;

    /**
     * @brief 获取单例实例
     * @return 账本单例的引用
     * @note 使用静态局部变量实现线程安全的单例模式（C++11 magic statics）
     */
    static UbseMemDebtLedger &GetInstance()
    {
        static UbseMemDebtLedger instance;
        return instance;
    }

    /**
     * @brief 获取某个类型的节点账本（非const版本）
     * @tparam T 资源对象类型
     * @return 对应类型账本的引用
     * @note 通过std::get从tuple中获取对应类型的账本
     */
    template <typename T>
    auto &GetDebtMap()
    {
        return std::get<UbseMemTypeDebtMap<T>>(debtMaps_);
    }

    /**
     * @brief 获取某个类型的节点账本（const版本）
     * @tparam T 资源对象类型
     * @return 对应类型账本的常量引用
     * @note 通过std::get从tuple中获取对应类型的账本
     */
    template <typename T>
    const auto &GetDebtMap() const
    {
        return std::get<UbseMemTypeDebtMap<T>>(debtMaps_);
    }

    /**
     * @brief 获取所有节点的账本信息
     * @param[in] filterDestroyed 是否过滤已销毁状态的资源，默认为false
     * @return 包含所有节点账本信息的映射，键为节点ID，值为节点债务信息结构体
     * @note 遍历所有8种资源类型，将数据合并到结果映射中
     *       当filterDestroyed为true时，会过滤掉状态为DESTROYED的资源
     */
    UbseNodeMemDebtInfoMap GetAllDebtInfo(bool filterDestroyed = false) const
    {
        UbseNodeMemDebtInfoMap result;

        // targetField 是 UbseNodeMemDebtInfo 结构体中对应字段的成员指针
        auto mergeToResult = [&](const auto &sourceMap, auto targetField) {
            // 直接遍历所有节点映射
            for (const auto &[nodeId, nodeMap] : sourceMap.GetAllNodeMaps()) {
                if (!nodeMap) {
                    continue;
                }

                if (filterDestroyed) {
                    // (result[nodeId].*targetField) 的意思是：找到或者创建对应 nodeId 的 info，并访问它的指定字段
                    (result[nodeId].*targetField) = FilterDestroyedResources(nodeMap->GetAll());
                } else {
                    (result[nodeId].*targetField) = nodeMap->GetAll();
                }
            }
        };

        // 依次将 8 种资源的数据“灌”入 result，不需要提前收集 ID
        mergeToResult(GetDebtMap<UbseMemFdBorrowImportObj>(), &UbseNodeMemDebtInfo::fdImportObjMap);
        mergeToResult(GetDebtMap<UbseMemFdBorrowExportObj>(), &UbseNodeMemDebtInfo::fdExportObjMap);
        mergeToResult(GetDebtMap<UbseMemNumaBorrowImportObj>(), &UbseNodeMemDebtInfo::numaImportObjMap);
        mergeToResult(GetDebtMap<UbseMemNumaBorrowExportObj>(), &UbseNodeMemDebtInfo::numaExportObjMap);
        mergeToResult(GetDebtMap<UbseMemShareBorrowImportObj>(), &UbseNodeMemDebtInfo::shareImportObjMap);
        mergeToResult(GetDebtMap<UbseMemShareBorrowExportObj>(), &UbseNodeMemDebtInfo::shareExportObjMap);
        mergeToResult(GetDebtMap<UbseMemAddrBorrowImportObj>(), &UbseNodeMemDebtInfo::addrImportObjMap);
        mergeToResult(GetDebtMap<UbseMemAddrBorrowExportObj>(), &UbseNodeMemDebtInfo::addrExportObjMap);

        return result;
    }

    /**
     * @brief 清空所有节点账本
     * @note 使用std::apply和折叠表达式遍历tuple中的所有账本并清空
     *       这会清除所有节点上的所有资源记录
     */
    void ClearAllNodeMaps()
    {
        // 使用std::apply 和折叠表达式遍历 tuple 中的所有成员
        std::apply([](auto &...maps) { (maps.ClearAllNodeMaps(), ...); }, debtMaps_);
    }

    /**
     * @brief 清除除指定节点外的所有节点账本
     * @param[in] excludeNodeId 要保留的节点ID
     * @note 使用std::apply和折叠表达式遍历tuple中的所有账本
     *       通常用于节点下线场景，保留本节点账本，清理其他节点账本
     */
    void ClearOtherNodeMaps(const std::string &excludeNodeId)
    {
        std::apply([&excludeNodeId](auto &...maps) { (maps.RemoveOtherNodeMaps(excludeNodeId), ...); }, debtMaps_);
    }

    /**
     * @brief 获取指定节点的账本信息
     * @param[in] nodeId 节点ID
     * @param[in] filterDestroyed 是否过滤已销毁状态的资源，默认为false
     * @return 该节点的内存债务信息结构体
     * @note 遍历所有8种资源类型，收集指定节点的资源信息
     *       当filterDestroyed为true时，会过滤掉状态为DESTROYED的资源
     */
    UbseNodeMemDebtInfo GetNodeMemDebtInfo(const std::string &nodeId, bool filterDestroyed = false) const
    {
        UbseNodeMemDebtInfo info;
        // 使用通用 Lambda 简化 8 种资源的处理逻辑
        auto fillMap = [&](auto &sourceMap, auto &targetField) {
            if (auto nodeMap = sourceMap.FindNodeMap(nodeId)) {
                if (filterDestroyed) {
                    // 执行过滤逻辑，产生一个新的 Map
                    targetField = FilterDestroyedResources(nodeMap->GetAll());
                } else {
                    // 不过滤，直接赋值（会触发一次 Map 的拷贝）
                    targetField = nodeMap->GetAll();
                }
            }
        };

        // 依次处理 8 种内存类型
        fillMap(GetDebtMap<UbseMemFdBorrowImportObj>(), info.fdImportObjMap);
        fillMap(GetDebtMap<UbseMemFdBorrowExportObj>(), info.fdExportObjMap);
        fillMap(GetDebtMap<UbseMemNumaBorrowImportObj>(), info.numaImportObjMap);
        fillMap(GetDebtMap<UbseMemNumaBorrowExportObj>(), info.numaExportObjMap);
        fillMap(GetDebtMap<UbseMemShareBorrowImportObj>(), info.shareImportObjMap);
        fillMap(GetDebtMap<UbseMemShareBorrowExportObj>(), info.shareExportObjMap);
        fillMap(GetDebtMap<UbseMemAddrBorrowImportObj>(), info.addrImportObjMap);
        fillMap(GetDebtMap<UbseMemAddrBorrowExportObj>(), info.addrExportObjMap);

        return info;
    }

    /**
     * @brief 从NodeMemDebtInfo加载账本数据
     * @param[in] nodeId 节点ID
     * @param[in] nodeMemDebtInfo 节点内存债务信息结构体
     * @note 将旧版NodeMemDebtInfo结构体中的数据迁移到新账本系统
     *       遍历所有8种资源类型，将数据逐一存入对应类型的账本
     */
    void LoadFromNodeMemDebtInfo(const std::string &nodeId, const NodeMemDebtInfo &nodeMemDebtInfo)
    {
        for (const auto &[name, obj] : nodeMemDebtInfo.fdImportObjMap) {
            GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.fdExportObjMap) {
            GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.numaImportObjMap) {
            GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.numaExportObjMap) {
            GetDebtMap<UbseMemNumaBorrowExportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.shareImportObjMap) {
            GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.shareExportObjMap) {
            GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.addrImportObjMap) {
            GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(nodeId, name, obj);
        }
        for (const auto &[name, obj] : nodeMemDebtInfo.addrExportObjMap) {
            GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(nodeId, name, obj);
        }
    }

private:
    UbseMemDebtLedger() = default;
    ~UbseMemDebtLedger() = default;

    /**
     * @brief 过滤已销毁状态的资源
     * @tparam T 资源对象类型
     * @param[in] resources 待过滤的资源映射
     * @return 过滤后的资源映射，不包含已销毁状态的资源
     * @note 对于ExportObj类型，过滤状态为UBSE_MEM_EXPORT_DESTROYED的资源
     *       对于ImportObj类型，过滤状态为UBSE_MEM_IMPORT_DESTROYED的资源
     */
    template <typename T>
    std::unordered_map<std::string, std::shared_ptr<const T>>
    FilterDestroyedResources(const std::unordered_map<std::string, std::shared_ptr<const T>> &resources) const
    {
        std::unordered_map<std::string, std::shared_ptr<const T>> filtered;
        for (const auto &[key, obj] : resources) {
            if (!obj) {
                continue;
            }
            // 根据类型判断应该过滤的状态
            if constexpr (IsExportObjTypeV<T>) {
                // Export 类型过滤 UBSE_MEM_EXPORT_DESTROYED
                if (obj->status.state != UbseMemState::UBSE_MEM_EXPORT_DESTROYED) {
                    filtered[key] = obj;
                }
            } else {
                // Import 类型过滤 UBSE_MEM_IMPORT_DESTROYED
                if (obj->status.state != UbseMemState::UBSE_MEM_IMPORT_DESTROYED) {
                    filtered[key] = obj;
                }
            }
        }
        return filtered;
    }

    /**
     * @brief 所有类型的账本元组
     * @note 使用std::tuple统一管理8种类型的账本，便于通过模板方法统一访问
     *       包含：FD/NUMA/Share/Addr × Import/Export 共8种类型
     */
    std::tuple<UbseMemTypeDebtMap<UbseMemFdBorrowImportObj>, UbseMemTypeDebtMap<UbseMemFdBorrowExportObj>,
               UbseMemTypeDebtMap<UbseMemNumaBorrowImportObj>, UbseMemTypeDebtMap<UbseMemNumaBorrowExportObj>,
               UbseMemTypeDebtMap<UbseMemShareBorrowImportObj>, UbseMemTypeDebtMap<UbseMemShareBorrowExportObj>,
               UbseMemTypeDebtMap<UbseMemAddrBorrowImportObj>, UbseMemTypeDebtMap<UbseMemAddrBorrowExportObj>>
        debtMaps_;
};

} // namespace ubse::mem::controller::debt
#endif // UBSE_MEM_DEBT_LEDGER_H
