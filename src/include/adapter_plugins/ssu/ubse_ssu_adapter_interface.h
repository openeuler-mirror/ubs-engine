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

#ifndef UBSE_SSU_ADAPTER_INTERFACE_H
#define UBSE_SSU_ADAPTER_INTERFACE_H
#include "ubse_ssu_def.h"
namespace ubse::adapter_plugins::ssu::def {
using ubse::adapter_plugins::ssu::def::UbseCreateBlockDeviceOptions;
using ubse::adapter_plugins::ssu::def::UbseSsuDevInfo;
using ubse::adapter_plugins::ssu::def::UbseSsuDevNameSpace;
/*
 * SSU 适配器抽象接口
 *
 * 定义 SSU（Solid State Unit）NVMe 存储设备的底层操作接口，包括：
 *   - 物理设备信息查询（GetDevList）
 *   - 命名空间生命周期管理（CreateDevNameSpace / DeleteDevNameSpace）
 *   - 命名空间挂载/卸载（AttachDevNameSpace / DetachDevNameSpace）
 *   - 访问权限控制（AddSubSystemAllowHost / RemoveSubSystemAllowHost 等）
 *   - 块设备聚合管理（CreateBlockDevice / DeleteBlockDevice）
 *
 * 本接口采用抽象基类设计，具体实现由适配器插件提供，
 * 通过 GetInstance 获取单例实例，实现与底层 NVMe 硬件的解耦。
 *
 * 接口调用约束：
 *   - 仅 Master 节点可调用的接口：GetDevList、CreateDevNameSpace、DeleteDevNameSpace、
 *     AddSubSystemAllowHost、RemoveSubSystemAllowHost、AddNameSpaceAllowHost、RemoveNameSpaceAllowHost
 *   - Master 和 Agent 节点均可调用的接口：AttachDevNameSpace、DetachDevNameSpace、
 *     CreateBlockDevice、DeleteBlockDevice
 */
class UbseSsuAdapterInterface {
public:
    /*
     * 默认构造函数
     */
    UbseSsuAdapterInterface() = default;

    /*
     * 默认虚析构函数
     */
    virtual ~UbseSsuAdapterInterface() = default;

    /*
     * 获取单例实例
     *
     * 返回当前进程中唯一的 UbseSsuAdapter 实例引用。
     * 实际类型由具体实现决定，通常在初始化阶段通过工厂方法注册。
     */
    static UbseSsuAdapterInterface &GetInstance();

    /*
     * 获取 SSU 物理设备信息列表
     * 只有Master节点可以调用此接口
     *
     * 扫描系统中所有 NVMe SSD 设备，返回指定 eid 列表对应的设备详细信息。
     *
     * @param ssuInfoList [inout] 输入SSU物理设备的eid信息，返回的设备信息列表，含控制器、NS、容量等
     * @return 0 成功，非零失败
     */
    virtual uint32_t GetDevList(std::vector<UbseSsuDevInfo> &ssuInfoList) = 0;

    /*
     * 在指定 SSU 设备上创建命名空间
     * 只有Master节点可以调用此接口
     *
     * 在 devCtrl 指向的 NVMe 控制器上创建指定大小的 Namespace。
     * 在tgt侧，先创建NS，再在tgt侧执行attach-ns，将NS attach到tgt侧控制器
     *
     * 可靠性要求：
     *   - options 中应携带预生成的 guid（NGUID）
     *   - guid 在算法规划阶段生成，确保失败重试时使用同一 guid 实现幂等
     *   - 创建失败无需回滚（无副作用）
     *
     * @param nameSpace  [inout] eid，guid，nsze，ncap，nsOptions，customData必填，返回的namespaceId等信息
     * @return 0 成功，非零失败
     */
    virtual uint32_t CreateDevNameSpace(UbseSsuDevNameSpace &nameSpace) = 0;

    /*
     * 删除指定 SSU 设备上的命名空间
     * 只有Master节点可以调用此接口
     *
     * 将指定 NS 从物理设备上永久删除，NS 上所有数据将丢失。
     *
     * 可靠性要求：
     *   - 删除前必须验证 nameSpace.guid 与设备上实际 NS 的 NGUID 匹配，
     *     防止因 devicePath 变化导致误删其他 NS
     *   - 删除前应确保 NS 已 detach（状态为 DELETING）
     *   - NS 不存在时应返回成功（幂等性保证）
     *
     * @param nameSpace  要删除的命名空间信息，guid 用于防误删验证
     * @return 0 成功，非零失败
     */
    virtual uint32_t DeleteDevNameSpace(const UbseSsuDevNameSpace &nameSpace) = 0;

    /*
     * 将namespace attach到host节点，由host节点调用，执行后，host节点会生成nvme设备
     * Master，Agent节点都可以调用此接口
     *
     * 可靠性要求：
     *   - attach 后必须验证 NGUID 一致性（读回 nvme id-ns 与预期 guid 比对），
     *     确认挂载的 NS 确实是目标 NS
     *   - 已 attach 的 NS 重复调用应返回当前 devicePath（幂等性）
     *
     * @param hostNqn    Host 的 NVMe Qualified Name，标识发起挂载请求的主机
     * @param nameSpace  要挂载的命名空间信息，guid 用于验证
     * @return 0 成功，非零失败
     */
    virtual uint32_t AttachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace) = 0;

    /*
     * 将namespace 从host节点detach，由host节点调用，执行后，host节点会删除nvme设备
     * Master，Agent节点都可以调用此接口
     *
     * 可靠性要求：
     *   - 已 detach 的 NS 重复调用应返回成功（幂等性）
     *
     * @param hostNqn    Host 的 NVMe Qualified Name，标识发起卸载请求的主机
     * @param nameSpace  要detach的命名空间信息
     * @return 0 成功，非零失败
     */
    virtual uint32_t DetachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace) = 0;

    /*
     * 将 Host 添加到命名空间的允许主机列表
     * 只有Master节点可以调用此接口
     *
     * 在 Target 侧的指定命名空间上，将 Host NQN 添加到命名空间级别的访问控制列表，
     * 授权该 Host 通过 NVMe-oF 协议访问该特定命名空间。
     * 与 AddSubSystemAllowHost 不同，本接口实现命名空间粒度的细粒度访问控制。
     *
     * 可靠性要求：
     *   - 已存在的 Host 重复添加应返回成功（幂等性保证）
     *
     * @param nameSpace  命名空间信息，subSystem 和 guid 用于定位目标命名空间
     * @param hostNqn    被授权主机的 NVMe Qualified Name
     * @return 0 成功，非零失败
     */
    virtual uint32_t AddNameSpaceAllowHost(const UbseSsuDevNameSpace &nameSpace,
                                           const std::string &hostNqn) = 0;

    /*
     * 将 Host 从命名空间的允许主机列表中移除
     * 只有Master节点可以调用此接口
     *
     * 在 Target 侧的指定命名空间上，将 Host NQN 从命名空间级别的访问控制列表中移除，
     * 撤销该 Host 对该特定命名空间的访问权限。
     *
     * 可靠性要求：
     *   - 不存在的 Host 移除应返回成功（幂等性保证）
     *   - 移除前应确保该 Host 已断开与对应命名空间的连接
     *
     * @param nameSpace  命名空间信息，subSystem 和 guid 用于定位目标命名空间
     * @param hostNqn    被撤销权限主机的 NVMe Qualified Name
     * @return 0 成功，非零失败
     */
    virtual uint32_t RemoveNameSpaceAllowHost(const UbseSsuDevNameSpace &nameSpace,
                                              const std::string &hostNqn) = 0;

    /*
     * 查询命名空间的允许主机列表
     * 只有Master节点可以调用此接口
     *
     * 获取 Target 侧指定命名空间上已授权的 Host NQN 列表，
     * 用于查询哪些 Host 被允许通过 NVMe-oF 协议访问该命名空间。
     *
     * @param nameSpace     命名空间信息，subSystem 和 guid 用于定位目标命名空间
     * @param allowHostList [out] 返回已授权的 Host NQN 列表
     * @return 0 成功，非零失败
     */
    virtual uint32_t GetNameSpaceAllowHostList(const UbseSsuDevNameSpace &nameSpace,
                                               std::vector<std::string> &allowHostList) = 0;

    /*
     * 创建块设备
     * Master，Agent节点都可以调用此接口
     *
     * 将多个 Namespace 设备路径组装为一个逻辑块设备
     * 支持LINEAR（线性拼接）和 STRIPED（条带化）两种编址方式，条带化支持RAID0和RAID5
     *
     * 可靠性要求：
     *   - devicePathList 应使用 persistentPath（/dev/disk/by-id/nvme-eui.<guid>），
     *     而非 devicePath（/dev/nvmeXnY），确保重启后 mdadm 超级块匹配可靠
     *   - 创建成功后必须执行 mdadm --detail --scan >> /etc/mdadm/mdadm.conf
     *     并 update-initramfs -u，确保重启后 RAID 自动组装
     *   - 创建失败时需回滚所有已创建的 NS（detach + delete-ns）
     *   - 通过 raidUuid 标识阵列，重启后用于匹配当前 md 设备
     *
     * @param deviceName    聚合后的块设备名，由外部指定
     * @param devicePathList 成员 NS 的持久化设备路径列表（by-id 路径）
     * @param options       块设备创建选项，含 RAID 级别、编址方式、chunk 大小
     * @param devicePath   [out] 返回创建的块设备路径（如 /dev/md/ubse-pool-0）
     * @return 0 成功，非零失败
     */
    virtual uint32_t CreateBlockDevice(const std::string &deviceName, const std::vector<std::string> &devicePathList,
                                       const UbseCreateBlockDeviceOptions &options, std::string &devicePath) = 0;

    /*
     * 删除块设备
     * Master，Agent节点都可以调用此接口
     *
     * 可靠性要求：
     *   - 删除前应确保块设备上无活跃 I/O
     *   - 设备不存在时应返回成功（幂等性）
     *
     * @param deviceName 要删除的块设备名
     * @return 0 成功，非零失败
     */
    virtual uint32_t DeleteBlockDevice(const std::string &deviceName) = 0;
};

} // namespace ubse::adapter_plugins::ssu::def
#endif // UBSE_SSU_ADAPTER_INTERFACE_H
