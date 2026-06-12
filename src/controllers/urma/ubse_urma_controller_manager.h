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

#ifndef UBSE_URMA_CONTROLLER_MANAGER_H
#define UBSE_URMA_CONTROLLER_MANAGER_H
#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <string>

#include "ubse_common_def.h"
#include "ubse_mti_interface.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"
#include "lock/ubse_lock.h"

namespace ubse::urmaController {
using common::def::UbseResult;
using ubse::adapter_plugins::mti::UbseMtiFeInfo;
using ubse::urma::FeTopoType;
using ubse::urma::UbseUrmaInfo;
using ubse::urma::UbseUrmaNodeInfo;
using ubse::urma::UbseUrmaUvsNodeInfo;
using ubse::urma::UrmaDevState;

struct UrmaDevRollbackState {
    uint16_t feId;                     ///< 操作前的全局feId
    uint64_t urmaId;                   ///< 操作前的全局urmaId
    std::vector<std::string> urmaDevs; ///< 本次操作过程中已创建的urma设备名称列表，回滚时逐一删除
};

class UbseUrmaControllerManager {
public:
    /**
     * @brief 获取单例实例
     * @return 单例引用
     */
    static UbseUrmaControllerManager& GetInstance()
    {
        static UbseUrmaControllerManager instance;
        return instance;
    }

    /**
     * @brief 存储通过RPC从其他节点（包括本节点）获取的URMA节点信息
     * @details 将节点信息插入或更新到nodeInfos中。如果本地已有该节点的信息且时间戳更新，
     *          则跳过本次更新，避免旧数据覆盖新数据。
     * @param nodeId 节点唯一标识
     * @param insertNodeInfo 待插入的节点URMA信息（move语义，调用后内容被转移）
     */
    void InsertNewNodeInfo(const std::string& nodeId, UbseUrmaNodeInfo& insertNodeInfo);

    /**
     * @brief 基于节点的FE信息计算并构建URMA bonding设备信息（左值引用版本）
     * @details 根据组网类型（CLOS/非CLOS）对FE信息进行拆分和过滤，将每个IOU上配对的两组FE
     *          的EID组逐一组成bonding设备。CLOS组网下会拆分为容器侧和主机侧两套bonding。
     *          操作完成后更新节点的updateTimeStamp。
     * @param nodeId 节点唯一标识
     * @param feInfos 按IOU分组的前端引擎信息，格式为 feInfos[IOU索引][FE索引]
     * @return UbseResult UBSE_OK表示成功，其他值表示失败
     */
    UbseResult ConstructNewUrmaInfo(const std::string& nodeId, std::vector<std::vector<UbseMtiFeInfo>>& feInfos);

    // 基于节点的FE信息计算并构建URMA bonding设备信息（右值引用版本）
    UbseResult ConstructNewUrmaInfo(const std::string& nodeId, std::vector<std::vector<UbseMtiFeInfo>>&& feInfos);

    /**
     * @brief 根据基准节点的URMA bonding信息推算出其他节点的URMA设备信息
     * @details 仅在CLOS组网下生效。以basedNodeId节点的bonding信息为模板，遍历指定范围的
     *          serverIdx，为每个节点重新生成唯一的devEid和primaryEid/portEid，实现跨节点的
     *          bonding信息复制。跳过基准节点自身（curServerIdx）。
     * @param isInferHostOnly 是否只推算主机侧（hostUrmaList）的bonding，false则同时推算容器侧
     * @param basedNodeId 作为推算模板的基准节点ID
     * @param startIdx 起始server索引（从0开始）
     * @param batchNodeNum 批量处理的节点数量
     * @return UbseResult UBSE_OK表示成功，其他值表示失败
     */
    UbseResult InferOtherNodesUrmaDevInfo(bool isInferHostOnly, const std::string& basedNodeId, uint32_t startIdx,
                                          uint32_t batchNodeNum);

    /**
     * @brief 删除除当前节点外其他所有节点的URMA信息
     * @details 在下发拓扑后调用，通过extract保留当前节点信息，再swap清空整个nodeInfos，
     *          最后重新插入当前节点信息。此方式可强制释放旧map占用的全部内存（含Rb-tree节点）。
     *          仅在CLOS组网下执行清理操作。
     * @param curNodeId 当前节点ID，该节点的信息将被保留
     */
    void DeleteOtherNodesUrmaInfo(const std::string& curNodeId);

    /**
     * @brief 根据URMA设备的EID设置其状态
     * @details 先在nodeInfos中按EID查找对应的URMA设备名称，再通过写锁更新其状态。
     *          先在读锁下查找设备名，再升级为写锁进行状态更新。
     * @param urmaDevEid URMA设备的全局唯一EID标识
     * @param state 目标状态值
     */
    void SetUrmaDevStateByDevEid(const std::string& urmaDevEid, UrmaDevState state);

    /**
     * @brief 根据URMA设备名称获取本节点的URMA设备详细信息
     * @details 在读锁保护下查找当前节点的urmaList中匹配名称的设备信息。
     *          仅在当前节点的nodeInfos中查找，不跨节点查询。
     * @param urmaName URMA设备名称（如 "bonding_dev_0"）
     * @param urmaInfo 输出参数，返回匹配的URMA设备完整信息
     * @return UbseResult UBSE_OK表示找到，UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST表示设备不存在
     */
    UbseResult GetLocalUrmaDevInfoByName(const std::string& urmaName, UbseUrmaInfo& urmaInfo);

    /**
     * @brief 分配一个URMA设备，返回其关联的FE名称列表和EID
     * @details 在读锁保护下查找当前节点的URMA设备，校验设备状态必须为ACTIVED才可分配。
     *          返回结果包含设备的subPath以及所有eidGroups中FE的名称。
     * @param urmaName URMA设备名称
     * @param feNames 输出参数，依次返回subPath和各eidGroup关联的FE名称
     * @param eid 输出参数，返回URMA设备的EID
     * @return UbseResult UBSE_OK表示分配成功，
     *         UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST表示设备不存在，
     *         UBSE_URMACONTRL_ERROR_DEV_NOT_INACTIVE表示设备未激活不可分配
     */
    UbseResult AllocUrmaDev(const std::string& urmaName, std::vector<std::string>& feNames, std::string& eid);

    /**
     * @brief 将URMA CTL的设备信息转换为拓扑下发时URMA UVS模块所需的格式
     * @details 在写锁保护下执行：先调InferOtherNodesUrmaDevInfo推算其他节点拓扑（仅CLOS组网），
     *          再遍历所有nodeInfos，将每个节点的urmaList和hostUrmaList中的设备信息转换为
     *          UbseUrmaUvsNodeInfo格式。转换完成后自动清理其他节点信息以释放内存。
     * @param isBuildHostOnly 是否只构建主机侧拓扑（仅含hostUrmaList），
     *                        false则同时构建容器侧和主机侧拓扑
     * @param startServerIdx 推算其他节点拓扑时的起始server索引
     * @param batchNodeNum 推算其他节点拓扑时的批量节点数
     * @param uvsInfos 输出参数，所有节点的URMA UVS拓扑信息列表
     * @return UbseResult UBSE_OK表示成功，其他值表示失败
     */
    UbseResult BuildUvsTopoNodeInfo(bool isBuildHostOnly, uint32_t startServerIdx, uint32_t batchNodeNum,
                                    std::vector<UbseUrmaUvsNodeInfo>& uvsInfos);

    /**
     * @brief 根据URMA设备的EID设置其子路径
     * @details 在写锁保护下遍历所有节点的urmaList，匹配EID后设置对应设备的subPath字段。
     *          找到第一个匹配的设备后立即返回。
     * @param urmaEid URMA设备的EID
     * @param urmaSubPath 要设置的子路径字符串
     */
    void SetUrmaSubPath(const std::string& urmaEid, const std::string& urmaSubPath);

    /**
     * @brief 根据FE的primaryEid设置对应FE的名称
     * @details 在写锁保护下遍历所有节点所有URMA设备的eidGroups，找到primaryEid匹配的
     *          eidGroup后设置其关联feInfo的name字段。找到第一个匹配后立即返回。
     * @param feEid FE的primaryEid，用作匹配条件
     * @param urmaEidName 要设置的FE名称
     */
    void SetFeName(const std::string& feEid, const std::string& urmaEidName);

    /**
     * @brief 获取或创建指定节点的URMA节点信息
     * @details 先以读锁查找nodeInfos，若存在则直接返回副本；若不存在则升级为写锁，
     *          创建并插入一个新的UbseUrmaNodeInfo，设置其nodeId后返回。
     * @param nodeId 节点唯一标识
     * @return UbseUrmaNodeInfo 节点的URMA信息副本
     */
    UbseUrmaNodeInfo GetUrmaNodeInfo(const std::string& nodeId);

    /**
     * @brief 设置当前节点所有URMA设备的状态
     * @details 在写锁保护下遍历当前节点urmaList中的所有设备，将状态统一设置为指定值。
     *          常用于批量激活或去激活操作。
     * @param state 目标状态值
     */
    void SetAllUrmaDevStateForNode(UrmaDevState state);

    /**
     * @brief 获取指定节点的URMA信息更新时间戳
     * @details 在读锁保护下查找节点信息，返回其updateTimeStamp。
     *          如果节点不存在则返回0。
     * @param nodeId 节点唯一标识
     * @return uint64_t 时间戳（毫秒级Unix时间），节点不存在时返回0
     */
    uint64_t GetUrmaUpdateTimeStamp(const std::string& nodeId);

    /**
     * @brief 设置FE拓扑类型
     * @details FE拓扑类型由CalculateFeTopoType根据每个IOU上的PFE/VFE数量计算得出，
     *          用于指导bonding命名和拓扑构建逻辑。
     * @param topoType FE拓扑类型（ALL_PFE / PFE_VFE_HYBRID / INVALID）
     */
    void SetFeTopoType(FeTopoType topoType);

    /**
     * @brief 获取当前FE拓扑类型
     * @return FeTopoType 当前FE拓扑类型
     */
    FeTopoType GetFeTopoType() const;

private:
    /**
     * @brief 基于两个FE的EID组逐一创建URMA bonding设备并插入到指定的urmaList中
     * @details 核心bonding逻辑：取lcneFe0和lcneFe1的EID组一一配对，每组生成唯一的
     *          feId、urmaId和devEid，组成一个UrmaDevInfo并命名后插入urmaList。
     *          操作前保存回滚状态，任意一步失败时通过CreateAndInsertUrmaInfoRollback
     *          恢复feId、urmaId并删除已创建的部分设备，保证可重入。
     * @param serverIdx 服务器索引（从SMBIOS获取，用于生成devEid）
     * @param nodeId 节点ID
     * @param lcneFe0 第一个FE的信息（来自IOU 0的某一entity）
     * @param lcneFe1 第二个FE的信息（来自IOU 1的对应entity）
     * @param urmaList 目标URMA列表（容器侧urmaList或主机侧hostUrmaList）
     * @return UbseResult UBSE_OK表示成功，UBSE_ERROR_AGAIN表示需重试
     */
    UbseResult CreateAndInsertUrmaInfo(const uint32_t serverIdx, const std::string& nodeId, UbseMtiFeInfo& lcneFe0,
                                       UbseMtiFeInfo& lcneFe1,
                                       std::map<std::string, UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList);

    /**
     * @brief 遍历feInfos中配对的FE，逐一调用CreateAndInsertUrmaInfo完成bonding
     * @details 当前约定每次从两个IOU区域各取一个FE组成bonding对（feInfos.size() == 2）。
     *          对每对FE校验其EID组非空且未被使用后，调用CreateAndInsertUrmaInfo组建bonding。
     *          若FE配置不足（无法为主机预留EID），接受feInfos为空的情况。
     * @param nodeId 节点ID
     * @param serverIdx 服务器索引
     * @param feInfos 按IOU分组排序后的FE信息（已拆分/过滤为容器侧或主机侧）
     * @param urmaList 目标URMA列表
     * @return UbseResult UBSE_OK表示成功，UBSE_ERROR_AGAIN表示需重试
     */
    UbseResult ProcessFeBounding(const std::string& nodeId, uint32_t serverIdx,
                                 std::vector<std::vector<UbseMtiFeInfo>>& feInfos,
                                 std::map<std::string, UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList);

    /**
     * @brief 为单个节点推算URMA设备信息
     * @details 以basedNodeId节点的nodeInfo为模板复制到目标节点（serverIdx对应），
     *          重新生成nodeId，并为urmaList和hostUrmaList中的所有设备重新计算
     *          devEid、primaryEid和portEid（通过解析原EID中的feId并替换serverIdx部分）。
     *          若isInferHostOnly为true则跳过容器侧urmaList的推算。
     * @param isInferHostOnly 是否只推算主机侧bonding
     * @param serverIdx 目标服务器索引
     * @param baseNodeId 作为模板的基准节点ID
     * @return UbseResult UBSE_OK表示成功，其他值表示失败
     */
    UbseResult InferOneNodeUrmaDevInfo(bool isInferHostOnly, uint32_t serverIdx, const std::string& baseNodeId);

    /**
     * @brief 生成节点内唯一的FE ID
     * @details 基于原子变量globalFeId自增，保证线程安全
     * @return uint16_t 新生成的唯一FE ID
     */
    uint16_t GenerateUniqueFeId();

    /**
     * @brief 生成节点内唯一的URMA设备ID
     * @details 基于原子变量globalUrmaId自增，保证线程安全
     * @return uint64_t 新生成的唯一URMA设备ID
     */
    uint64_t GenerateUrmaDevId();

    /**
     * @brief bonding操作失败时的回滚处理
     * @details 将globalUrmaId和globalFeId恢复到操作前的值，并删除本次操作中
     *          已创建的URMA设备条目，确保后续重试时状态正确。
     * @param state 操作前保存的回滚状态快照
     * @param urmaList 目标URMA列表，从中删除已创建的设备
     */
    void CreateAndInsertUrmaInfoRollback(const UrmaDevRollbackState& state,
                                         std::map<std::string, UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList);

    void PrintNodeInfo(const UbseUrmaNodeInfo& nodeInfo);

    /**
     * @brief 获取本节点URMA设备信息的内部实现（不加锁）
     * @details 在调用方已持有读锁的前提下，直接查找当前节点的urmaList中匹配名称的设备。
     *          由GetLocalUrmaDevInfoByName加锁后调用。
     * @param urmaName URMA设备名称
     * @param urmaInfo 输出参数，返回匹配的设备信息
     * @return UbseResult UBSE_OK表示找到，UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST表示不存在
     */
    UbseResult GetLocalUrmaDevInfoByNameInner(const std::string& urmaName, UbseUrmaInfo& urmaInfo);

    /**
     * @brief 检查一对FE是否已在指定的urmaList中被使用（组成过bonding）
     * @details 遍历urmaList中的所有设备，比较每个设备的两个eidGroup关联的feInfo
     *          的四元组是否与fe0和fe1完全匹配。
     *          注意：此方法基于传入的urmaList独立判断，而非全局状态，
     *          因为CLOS组网下同一物理FE需要分别为容器侧和主机侧bonding。
     * @param fe0 第一个FE信息
     * @param fe1 第二个FE信息
     * @param urmaList 待检查的URMA列表
     * @return true 该FE对已被使用
     * @return false 该FE对未被使用
     */
    bool IsLcneFeUsed(const UbseMtiFeInfo& fe0, const UbseMtiFeInfo& fe1,
                      const std::map<std::string, UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList);
    // clos组网下，获取主机bonding，插入到本节点的urmaList中，拓展到96bonding
    UbseResult InsertHostUrmaDevInner();

    /**
     * @brief 基于通信bonding设备信息构建主机URMA设备
     * @details 从通信bonding设备（comDev）的feList中提取ubpuId和entityId，
     *          在已有的urmaList中查找匹配的feInfo，构建包含primaryEid、portEid
     *          和feInfo的完整URMA设备信息。
     * @param comDev 通信bonding的聚合设备信息（来自NodeComUrmaCollector）
     * @param urmaList 已组建的URMA列表（用于查找匹配的feInfo）
     * @param urmaDev 输出参数，构建完成的主机URMA设备信息
     * @return UbseResult UBSE_OK表示成功，其他值表示失败
     */
    UbseResult BuildHostUrmaDev(const ubse::urma::UbseUrmaUvsAggrDev& comDev,
                                const std::map<std::string, UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList,
                                UbseUrmaInfo& urmaDev);

    /**
     * @brief 生成URMA设备名称
     * @details 根据组网类型和FE拓扑类型决定命名规则：
     *          - CLOS全PFE场景且当前bonding对位于第16组EID（下标15）且与主机bonding同FE时，
     *            固定命名为 "bonding_dev_96"（兼容此前版本，避免容器重建）
     *          - 其他情况则使用全局自增的urmaId生成 "bonding_dev_{urmaId}" 格式的名称
     * @param nodeId 节点ID
     * @param lcneFe0 第一个FE信息
     * @param lcneFe1 第二个FE信息
     * @param idx 当前bonding的EID组索引（从0开始）
     * @return std::string 生成的URMA设备名称，失败时返回空字符串
     */
    std::string GenerateUrmaDevName(const std::string& nodeId, const UbseMtiFeInfo& lcneFe0,
                                    const UbseMtiFeInfo& lcneFe1, const size_t idx);

private:
    utils::ReadWriteLock rwLock;                         // 读写锁，保护nodeInfos等共享数据的并发访问
    std::map<std::string, UbseUrmaNodeInfo> nodeInfos{}; // 各节点的URMA信息映射，key为nodeId
    std::atomic<uint16_t> globalFeId{0};                 // 节点内全局唯一的FE ID生成器（原子自增）
    std::atomic<uint64_t> globalUrmaId{0};      // 节点内全局唯一的URMA设备ID生成器（原子自增）
    FeTopoType feTopoType{FeTopoType::INVALID}; // 当前FE拓扑类型，由CalculateFeTopoType计算并设置
};
} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_MANAGER_H
