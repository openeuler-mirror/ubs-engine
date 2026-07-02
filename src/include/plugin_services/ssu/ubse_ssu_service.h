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

#ifndef UBSE_SSU_SERVICE_H
#define UBSE_SSU_SERVICE_H
#include <string>
#include <vector>
#include "ubse_service_registry.h"
namespace ubse::plugin::service::ssu {
using ubse::service::UbseIService;
enum class UbseSsuLBAFormat : uint32_t {
    LBA_FORMAT_512 = 512, // 512B
    LBA_FORMAT_4K = 4096, // 4K
};

enum class UbseSsuAggregationRaidLevel : uint8_t {
    RAID0 = 0,
    RAID5 = 5,
};

// chunkSize满足UbseSsuLBAFormat的整数倍
enum class UbseSsuChunkSize : uint32_t {
    CHUNK_SIZE_4K = 4,     // 4KB
    CHUNK_SIZE_16K = 16,   // 16KB
    CHUNK_SIZE_32K = 32,   // 32KB
    CHUNK_SIZE_64K = 64,   // 64KB
    CHUNK_SIZE_128K = 128, // 128KB
    CHUNK_SIZE_256K = 256, // 256KB
    CHUNK_SIZE_512K = 512, // 512KB
};

enum class UbseSsuAllocStrategy : uint8_t {
    STRIPED = 0, // 分布式策略，尽量从多个设备分配，均等分配，适用于条带化编址使用场景
    LINEAR = 1, // 顺序策略，尽量从单个设备分配，可能均等也可能不均等分配，适用于线性编址使用场景
};

struct UbseSsuAllocIdentityInfo {
    std::string userName; // 使用方进程的运行用户的名称
    uid_t uid;            // 使用方进程的运行用户的uid
};

struct UbseSsuAllocSpaceReq {
    std::string name; // 请求标识，最大48个字符
    uint64_t nsSize; // 申请总容量，单位字节, 条带化策略时，需整除nsNum且整除后需要为chunkSize的整数倍
    uint32_t nsNum;                        // 命名空间数量，等于1时，strategy不生效
    UbseSsuLBAFormat lbaFormat;            // LBA 格式
    UbseSsuAllocStrategy strategy;         // 分配策略
    std::string tenant;                    // 请求方tenant（租户隔离标识）
};

struct UbseSsuNameSpaceInfo {
    std::string tgtEid;         // Target EID
    std::string tgtNqn;         // TargetNQN
    std::string nsUuid;         // 物理设备UUID
    uint32_t namespaceId;       // 命名空间ID
    std::string nsDevPath;      // 命名空间设备路径
    uint64_t nsSize;            // 分配的容量，单位字节
    UbseSsuLBAFormat lbaFormat; // LBA 格式
};

struct UbseSsuAllocResult {
    std::string name;                                // 请求标识，最大48个字符
    UbseSsuAllocStrategy strategy;                   // 分配策略
    std::vector<UbseSsuNameSpaceInfo> nameSpaceList; // 命名空间信息列表
};

struct UbseSsuStripedAttachReq {
    std::string devName;               // 聚合后的块设备名称，由外部指定
    UbseSsuAggregationRaidLevel level; // RAID级别（RAID0 或 RAID5）
    UbseSsuChunkSize chunkSize;        // 条带化的chunk大小，单位KB
};

enum class UbseSsuNsState : uint8_t {
    IDLE = 0,      // 初始/失败回退
    CREATING = 1,  // Master正在创建NS
    CREATED = 2,   // NS创建完成，等待Agent Attach
    ATTACHING = 3, // Agent正在Attach
    ATTACHED = 4,  // 全部完成
};

// 虚拟功能单元(VFE)信息
struct UbseSsuVfe {
    uint8_t slotId; // 槽位ID
    uint8_t chipId; // 芯片ID
    uint8_t dieId;  // Die ID
    uint16_t pfeId; // 物理功能单元ID
    uint16_t vfeId; // 虚拟功能单元ID
};

// 功能单元(FE)信息, 包含所属PFE及其下的VFE列表
struct UbseSsuFe {
    uint8_t slotId;                  // 槽位ID
    uint8_t chipId;                  // 芯片ID
    uint8_t dieId;                   // Die ID
    uint16_t pfeId;                  // 物理功能单元ID
    std::vector<UbseSsuVfe> vfeList; // VFE列表, 由SDK内部动态分配, 需通过释放接口回收
};

// 存储空间连接信息
struct UbseSsuConnectInfo {
    std::string srcEid;  // Source EID
    std::string tgtEid;  // Target EID
    std::string tgtNqn;  // Target NQN
    std::string hostNqn; // 默认NQN, 例: nqn.2024-01.com.huawei:uuid:12345678-...
    std::string nsUuid;  // 物理设备UUID
    uint32_t nsId;       // 命名空间ID
};

// 存储空间状态
struct UbseSsuNsStats {
    std::string nsUuid; // 物理设备UUID
    uint32_t nsId;      // 命名空间ID
    uint64_t totalSize; // 总容量, 单位字节
    uint64_t usedSize;  // 已用容量, 单位字节
};

/**
 * @class UbseSsuService
 * @brief SSU存储池分配控制器
 *
 * 提供SSU设备的命名空间分配、释放、挂载和卸载等核心功能，
 * 支持线性编址和条带化编址两种聚合方式，以及独占和共享两种使用模式。
 *
 * @note 本控制器负责管理NVMe SSU的命名空间生命周期，
 *       与底层适配器交互完成实际的硬件操作。
 */
class UbseSsuService : public UbseIService {
public:
    static constexpr const char *kServiceName = "UbseSsuService";
    /**
     * @brief 默认构造函数
     */
    UbseSsuService() = default;

    std::string GetServiceName() const override
    {
        return kServiceName;
    }

    /**
     * @brief 默认析构函数
     */
    virtual ~UbseSsuService() = default;

    /**
     * @brief 列出所有已分配的存储空间信息
     *
     * 获取系统中所有已分配的SSU存储空间详细信息, 包括命名空间列表、容量、LBA格式和使用类型等。
     *
     * @param result   [输出] 已分配空间信息列表
     * @param identity 调用方身份信息, 包含用户名和uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t ListAllocInfo(std::vector<UbseSsuAllocResult> &result, const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 根据名称获取已分配的存储空间信息
     *
     * 根据存储空间的名称查询其详细信息，包括命名空间列表、容量、LBA格式和使用类型等。
     *
     * @param name     存储空间标识（与 AllocSpace 时的 name 参数一致）
     * @param result   [输出] 已分配空间信息
     * @param identity 调用方身份信息, 包含用户名和uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t GetAllocInfoByName(const std::string &name, UbseSsuAllocResult &result,
                                        const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 获取存储空间的命名空间统计信息
     *
     * 查询指定存储空间下各命名空间的容量使用情况, 包括总容量和已用容量。
     *
     * @param name       存储空间标识（与 AllocSpace 时的 name 参数一致）
     * @param statsList  [输出] 命名空间统计信息列表
     * @param identity   调用方身份信息, 包含用户名和uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t GetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                                const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 获取存储空间的连接信息
     *
     * 查询指定存储空间在指定VFE上的NVMe连接信息, 包括子系统NQN、Host NQN、命名空间ID等。
     *
     * @param name             存储空间标识（与 AllocSpace 时的 name 参数一致）
     * @param vfe              VFE信息, 指定查询的虚拟功能单元
     * @param connectInfoList  [输出] 连接信息列表
     * @param identity         调用方身份信息, 包含用户名和uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t GetConnectInfo(const std::string &name, const UbseSsuVfe &vfe,
                                    std::vector<UbseSsuConnectInfo> &connectInfoList,
                                    const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 分配SSU存储空间
     *
     * 根据请求参数分配指定数量和大小的命名空间，支持顺序分配和
     * 分布式分配两种策略。
     *
     * @param req     分配请求参数
     * @param identity 调用方身份信息, 包含用户名和uid
     * @param result  [输出] 分配结果，包含已分配的命名空间信息列表
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note 当 nsNum 为 1 时，strategy 参数不生效
     * @see UbseSsuAllocSpaceReq, UbseSsuAllocResult
     */
    virtual uint32_t AllocSpace(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                                UbseSsuAllocResult &result);

    /**
     * @brief 释放已分配的存储空间
     *
     * 释放之前通过 AllocSpace 分配的存储空间及其关联的所有命名空间。
     *
     * @param name     要释放的存储空间标识（与 AllocSpace 时的 name 参数一致）
     * @param identity 调用方身份信息, 包含用户名和uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note 释放操作具有幂等性，释放不存在的空间应返回成功
     */
    virtual uint32_t FreeSpace(const std::string &name, const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 添加存储空间访问权限
     *
     * 为指定的 Host 授予对已分配存储空间的访问权限，在 Target 侧将 Host NQN
     * 添加到子系统的允许主机列表中，使该 Host 可以通过 NVMe-oF 协议访问对应命名空间。
     *
     * @param name      存储空间标识（与 AllocSpace 时的 name 参数一致）
     * @param nqn       Host 的 NVMe Qualified Name，标识被授权的主机
     * @param identity  使用方进程的身份信息，包含 userName 和 uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note 重复添加同一 Host 的访问权限应返回成功（幂等性保证）
     * @see RemoveAccessPermission, UbseSsuAllocIdentityInfo
     */
    virtual uint32_t AddAccessPermission(const std::string &name, const std::string &nqn,
                                         const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 移除存储空间访问权限
     *
     * 撤销指定 Host 对已分配存储空间的访问权限，在 Target 侧将 Host NQN
     * 从子系统的允许主机列表中移除，使该 Host 无法再通过 NVMe-oF 协议访问对应命名空间。
     *
     * @param name      存储空间标识（与 AllocSpace 时的 name 参数一致）
     * @param nqn       Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity  使用方进程的身份信息，包含 userName 和 uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note 移除不存在的访问权限应返回成功（幂等性保证）
     * @note 移除权限前应确保该 Host 已断开与对应命名空间的连接
     * @see AddAccessPermission, UbseSsuAllocIdentityInfo
     */
    virtual uint32_t RemoveAccessPermission(const std::string &name, const std::string &nqn,
                                            const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 挂载已分配的存储空间
     *
     * 将指定的存储空间挂载到系统，使其可被主机访问。
     *
     * @param name     要挂载的存储空间标识
     * @param nqn  Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity 使用方进程的身份信息，包含 userName 和 uid
     * @param devPath  [输出] 挂载后的设备路径
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t AttachSpace(const std::string &name, const std::string &nqn,
                                 const UbseSsuAllocIdentityInfo &identity, std::string &devPath);

    /**
     * @brief 卸载已分配的存储空间
     *
     * 将指定的存储空间从系统卸载，释放设备占用。
     *
     * @param name  要卸载的存储空间标识
     * @param nqn  Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity 使用方进程的身份信息，包含 userName 和 uid
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note 卸载前需确保没有进程正在使用该存储空间
     */
    virtual uint32_t DetachSpace(const std::string &name, const std::string &nqn,
                                 const UbseSsuAllocIdentityInfo &identity);

    /**
     * @brief 挂载线性编址的存储空间
     *
     * 将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载。
     *
     * @param name     要挂载的存储空间标识
     * @param nqn  Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity 使用方进程的身份信息，包含 userName 和 uid
     * @param devName  聚合后的块设备名称（由外部指定）
     * @param devPath  [输出] 挂载后的聚合设备路径
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note 线性编址模式下，数据按顺序填充各成员设备
     */
    virtual uint32_t AttachLinearSpace(const std::string &name, const std::string &nqn,
                                       const UbseSsuAllocIdentityInfo &identity, const std::string &devName,
                                       std::string &devPath);

    /**
     * @brief 卸载线性编址的存储空间
     *
     * 将线性聚合的块设备卸载并释放。
     *
     * @param name     要卸载的存储空间标识
     * @param nqn  Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity 使用方进程的身份信息，包含 userName 和 uid
     * @param devName  聚合后的块设备名称
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t DetachLinearSpace(const std::string &name, const std::string &nqn,
                                       const UbseSsuAllocIdentityInfo &identity, const std::string &devName);

    /**
     * @brief 挂载条带化编址的存储空间
     *
     * 将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载，
     * 支持 RAID0 和 RAID5 两种级别。
     *
     * @param name      要挂载的存储空间标识
     * @param nqn  Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity  使用方进程的身份信息，包含 userName 和 uid
     * @param req       条带化挂载请求参数，包含块设备名称、RAID级别和chunk大小
     * @param devPath    [输出] 挂载后的聚合设备路径
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     *
     * @note RAID5 至少需要 3 个成员设备
     * @see UbseSsuStripedAttachReq, UbseSsuAggregationRaidLevel, UbseSsuChunkSize
     */
    virtual uint32_t AttachStripedSpace(const std::string &name, const std::string &nqn,
                                        const UbseSsuAllocIdentityInfo &identity, const UbseSsuStripedAttachReq &req,
                                        std::string &devPath);

    /**
     * @brief 卸载条带化编址的存储空间
     *
     * 将条带化聚合的块设备卸载并释放。
     *
     * @param name     要卸载的存储空间标识
     * @param nqn   Host 的 NVMe Qualified Name，标识被撤销权限的主机
     * @param identity 使用方进程的身份信息，包含 userName 和 uid
     * @param devName  聚合后的块设备名称
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t DetachStripedSpace(const std::string &name, const std::string &nqn,
                                        const UbseSsuAllocIdentityInfo &identity, const std::string &devName);

    /**
     * @brief 获取FE设备列表
     *
     * 查询系统中所有FE设备信息, 包括每个PFE下的VFE列表。
     *
     * @param feList [输出] FE设备信息列表
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t GetFeDeviceList(std::vector<UbseSsuFe> &feList);

    /**
     * @brief 分配VFE设备
     *
     * 将指定的虚拟功能单元分配给目标虚拟机, 使虚拟机可通过该VFE访问存储资源。
     *
     * @param upi              租户隔离标识
     * @param vfe              要分配的VFE信息
     * @param busInstanceGuid  [输入，输出] 总线实例GUID, 标识目标虚拟机
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid);

    /**
     * @brief 释放VFE设备
     *
     * 将已分配的虚拟功能单元从目标虚拟机释放, 回收VFE设备资源。
     *
     * @param upi              租户隔离标识
     * @param vfe              要释放的VFE信息
     * @param busInstanceGuid  总线实例GUID, 标识目标虚拟机
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败，具体错误码由实现定义
     */
    virtual uint32_t FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe, const std::string &busInstanceGuid);
};
} // namespace ubse::plugin::service::ssu
#endif // UBSE_SSU_SERVICE_H
