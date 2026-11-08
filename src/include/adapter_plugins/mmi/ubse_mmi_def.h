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

#ifndef UBSE_MMI_DEF_H
#define UBSE_MMI_DEF_H
#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "ubse_def.h"
#include "ubse_mmi_obmm_def.h"
#include "adapter_plugins/mti/ubse_mti_mami_def.h"

namespace ubse::adapter_plugins::mmi {
constexpr uint32_t UBSE_MAX_USR_INFO_LEN = 32;
// 共享内存标记[isExport:1|originExportNodeId:15|version:4|index:12]；gap读取的仍为旧priv身份。
constexpr uint32_t SHM_MIGRATE_MAX_BLOCKS = 4096; // index的值超出 12bit 编码能力
constexpr uint32_t SHM_MIGRATE_MAX_ORIGIN_NODE_ID = 0x7fffU;
constexpr uint32_t SHM_MIGRATE_EXPORT_FLAG = 1U << 31;
// 调用方校验origin范围；导入不记录origin，导出包括普通创建都必须设置身份位。
constexpr uint32_t MakeShmMigrateInfo(uint32_t version, uint32_t index, uint32_t originExportNodeId, bool isExport)
{
    const auto identity =
        isExport ? SHM_MIGRATE_EXPORT_FLAG | ((originExportNodeId & SHM_MIGRATE_MAX_ORIGIN_NODE_ID) << 16) : 0U;
    return identity | ((version & 0xfU) << 12) | (index & 0xfffU);
}
constexpr bool ShmMigrateIsExport(uint32_t info)
{
    return (info & SHM_MIGRATE_EXPORT_FLAG) != 0;
}
constexpr uint32_t ShmMigrateVersion(uint32_t info)
{
    return (info >> 12) & 0xfU;
}
constexpr uint32_t ShmMigrateIndex(uint32_t info)
{
    return info & 0xfffU;
}
constexpr uint32_t ShmMigrateOrigin(uint32_t info)
{
    return (info >> 16) & SHM_MIGRATE_MAX_ORIGIN_NODE_ID;
}
constexpr uint32_t ShmMigrateImportInfo(uint32_t info)
{
    // 普通导入、切换导入及逻辑self-import均只继承version/index。
    return info & 0xffffU;
}

enum class MemOperationType
{
    FD_BORROW,
    NUMA_BORROW,
    ADDR_BORROW,
    SHARED_BORROW,
    SHARED_ATTACH,
    SHARED_DETACH,
    FD_RETURN,
    NUMA_RETURN,
    ADDR_RETURN,
    SHARED_RETURN,
    COMMON_RETURN // 用于没匹配到任何归还时，回调处理
};

// 使用方进程信息
struct UbseUdsInfo {
    uid_t uid; // 使用方进程的运行用户的uid
    gid_t gid; // 使用方进程的的运行用户的groupid
    pid_t pid; // 使用方进程的的运行用户的pid
    std::string username{};

    // 权限校验方法：比较当前对象与传入对象的权限信息
    bool CheckPermission(const UbseUdsInfo& other) const;
    friend std::ostream& operator<<(std::ostream& os, const UbseUdsInfo& obj);
};

struct FdOwner {
    uid_t uid{0}; // 使用方进程的运行用户的uid
    gid_t gid{0}; // 使用方进程的的运行用户的groupid
    pid_t pid{0}; // 使用方进程的的运行用户的pid
    mode_t mode;
    friend std::ostream& operator<<(std::ostream& os, const FdOwner& obj);
};

enum UbseMemDistance
{
    MEM_DISTANCE_L0, // *L0对应直连节点
    MEM_DISTANCE_L1, // *L1对应通过1跳节点，暂不支持
    MEM_DISTANCE_L2  // *L2对应过超过1跳节点 ，暂不支持
};

// 内存状态枚举
enum UbseMemState
{
    UBSE_MEM_STATE_INIT,
    UBSE_MEM_STATE_SUCCEEDED,
    UBSE_MEM_STATE_FAILED,
    UBSE_MEM_STATE_DESTROYING,
    UBSE_MEM_SCHEDULING,
    UBSE_MEM_EXPORT_RUNNING,
    UBSE_MEM_EXPORT_SUCCESS,
    UBSE_MEM_EXPORT_DESTROYING,
    UBSE_MEM_EXPORT_DESTROYED,
    UBSE_MEM_EXPORT_DESTROYING_WAIT,
    UBSE_MEM_IMPORT_RUNNING,
    UBSE_MEM_IMPORT_SUCCESS,
    UBSE_MEM_IMPORT_DESTROYING,
    UBSE_MEM_IMPORT_DESTROYING_WAIT,
    UBSE_MEM_IMPORT_DESTROYED,
    UBSE_MEM_IMPORT_ABNORMAL
};

// 内存故障类型
enum UbMemFaultType
{
    UB_MEM_ATOMIC_DATA_ERR = 0,
    UB_MEM_READ_DATA_ERR,
    UB_MEM_FLOW_POISON,
    UB_MEM_FLOW_READ_AUTH_POISON,
    UB_MEM_FLOW_READ_AUTH_RESPERR,
    UB_MEM_TIMEOUT_POISON,
    UB_MEM_TIMEOUT_RESPERR,
    UB_MEM_READ_DATA_POISON,
    UB_MEM_READ_DATA_RESPERR,
    MAR_NOPORT_VLD_INT_ERR, // 无物理地址
    MAR_FLUX_INT_ERR,
    MAR_WITHOUT_CXT_ERR,
    RSP_BKPRE_OVER_TIMEOUT_ERR, // 响应备份超时
    MAR_NEAR_AUTH_FAIL_ERR,
    MAR_FAR_AUTH_FAIL_ERR,
    MAR_TIMEOUT_ERR,
    MAR_ILLEGAL_ACCESS_ERR,
    REMOTE_READ_DATA_ERR_OR_WRITE_RESPONSE_ERR,
    MEM_EXPORT_FAULT,
    MEM_LINK_DOWN,
    MEM_LINK_UP,
    UB_MEM_HEALTHY = 1000, // 无故障
    UB_MEM_SWITCHING = 1001, // 当前迁移或结束待确认的块，同一资源最多一块，不等同于内核gap状态
};

enum UbseNodeStatus
{
    UBSE_NODE_STATUS_NORMAL,  // 正常
    UBSE_NODE_STATUS_ABNORMAL // 异常
};

// 节点信息
struct UbseNodeInfo {
    uint32_t index{0};     // 节点编号
    std::string nodeId;    // index 和 node_id 看能不能归一
    std::string hostName;  // 主机名，最大长度MAX_HOSTNAME
    UbseNodeStatus status; // 节点状态，外部在选择共享域的时候，可以做参考
};

struct UbseShmRegionDesc {
    uint32_t nodeNum{0};                // 节点数
    std::vector<UbseNodeInfo> nodelist; // 节点列表
};

struct UbseMemAddrInfo {
    uint64_t addr{0}; // 内存起始地址
    uint64_t size{0}; // 该段地址长度
    friend std::ostream& operator<<(std::ostream& os, const UbseMemAddrInfo& obj);
};

struct UbseNumaLocation {
    std::string nodeId; // 节点ID
    uint32_t numaId{0}; // numa id
    friend std::ostream& operator<<(std::ostream& os, const UbseNumaLocation& obj);
};

struct UbseMemPrivData {
    uint16_t onePth : 1;      // bit0: 查单路径/多路径路由表，1表示单路径，读命令固定走多路径
    uint16_t wrDelayComp : 1; // bit1: 写命令是否comp接力，1表示不接力，0表示接力
    uint16_t reduceDelayComp : 1; // bit2: reduce操作是否comp接力，1表示不接力，0表示接力
    uint16_t cmoDelayComp : 1; // bit3: cache maintain操作是否comp接力，1表示不接力，0表示接力，暂预留
    uint16_t so : 1;           // bit4: 命令是否强保序，读命令固定为0
    uint16_t adTrOchip : 1;    // bit5: UMMU查片上页表/DDR页表，1表示查片上页表，0表示查DDR页表
    uint16_t cacheableFlag : 1; // bit6: 访问模式，1表示cc，0表示nc
    uint16_t marId : 3;         // bit7-9: mar id
    uint16_t rsv0 : 6;          // bit10-15: 保留位
};

// 高安签名信息
struct UbseTrustRingData {
    std::string trustRingId{};                  // 信任环id
    std::string reqSignedData{};                // 请求签名信息
    std::vector<std::string> lendSignedDatas{}; // 借出签名信息

    void ClearReqSignedDataMemory()
    {
        std::string().swap(reqSignedData);
    }

    void ClearLendSignedDataMemory()
    {
        lendSignedDatas.clear();
        std::vector<std::string>().swap(lendSignedDatas);
    }

    friend std::ostream& operator<<(std::ostream& os, const UbseTrustRingData& obj);
};

static constexpr int INVALID_SOCKET_ID = -1;
static constexpr int INVALID_PORT_ID = -1;
// 所有借用类型的基本类型
struct UbseMemBaseBorrowReq {
    std::string name{};
    std::string requestNodeId{};
    uint64_t requestId{};
    UbseUdsInfo udsInfo{}; // 使用方进程信息
    UbseTrustRingData trustRingData{};
    UbseMemPrivData ubseMemPrivData{}; // 内存访问属性
    friend std::ostream& operator<<(std::ostream& os, const UbseMemBaseBorrowReq& obj);
};

struct UbseMemFdBorrowReq : public UbseMemBaseBorrowReq {
    std::string importNodeId;                  // 导入节点ID
    size_t size{0};                            // 必填，单位字节
    UbseMemDistance distance{MEM_DISTANCE_L0}; // 内存访问经过的跳数
    std::vector<UbseNumaLocation> lenderLocs{}; // 借出方地址信息，与lenderSizes一一对应，为空则走基础算法进行决策
    std::vector<uint64_t> lenderSizes{};        // 借出大小，与lenderLocs一一对应
    std::vector<std::string> candidateNodeList; // 借出节点候选列表
    FdOwner owner;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemFdBorrowReq& obj);
};

struct UbseMemFdPermissionReq : public UbseMemBaseBorrowReq {
    FdOwner fdOwner; // 内存资源属主信息和内存资源的访问权限
    friend std::ostream& operator<<(std::ostream& os, const UbseMemFdPermissionReq& req);
};

struct UbseMemFdPermissionResp {
    uint32_t result;    // 执行结果
    uint64_t requestId; // 请求id

    friend std::ostream& operator<<(std::ostream& os, const UbseMemFdPermissionResp& resp);
};

struct UbseMemLenderLinkInfo {
    std::string lenderNode;
    int lenderSocketId{INVALID_SOCKET_ID};
    int lenderPort{INVALID_PORT_ID};
};

struct UbseMemLenderInfo {
    uint64_t lender_size;          // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    std::string nodeId;            // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socketId{UINT32_MAX}; // socket id, UINT32_MAX表示无效值
    uint32_t numaId{UINT32_MAX};   // 节点中的numa id, UINT32_MAX表示无效值
    uint32_t portId{UINT32_MAX};   // 指定链路借用, UINT32_MAX表示无效值
};

struct UbseMemShmAffinitySocketInfo {
    uint32_t affinitySocketId;
    std::string createReqNodeId;   // 发起创建请求的节点ID
    bool enableCreateWithAffinity; // 为true表示指定同一CPU平面进创建共享内存
};

struct UbseMemNumaBorrowReq : public UbseMemFdBorrowReq {
    int srcSocket{-1};           // 内存申请需求方节点socket信息 -1 无效
    int srcNuma{-1};             // 内存申请需求方节点NUMA信息  -1 无效
    size_t highWatermark{100};   // 必填，算法百分比，vm自己决策场景，单位%
    size_t lowWatermark{11};     // 必填，算法比分在，单位%
    bool samePlanePrefer{false}; // true=同平面优先, false=同平面必须(当 srcSocket!=-1 时)
    UbseMemLenderLinkInfo linkInfo{};
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN]; // 调用方私有数据，UBSE只负责保存，get时原样返回
    friend std::ostream& operator<<(std::ostream& os, const UbseMemNumaBorrowReq& obj);
};

struct UbseMemShareBorrowReq : public UbseMemBaseBorrowReq {
    std::string baseNodeId;                    // 导出节点ID
    size_t size{0};                            // 必填，单位字节
    UbseMemDistance distance{MEM_DISTANCE_L0}; // 内存访问经过的跳数
    UbseShmRegionDesc shmRegion{};             // 共享域
    std::vector<std::string> providerList;     // 可供借出的节点集合
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN];    // 调用方私有数据，UBSE只负责保存，get时原样返回
    bool shmAnonymous{false}; // true:匿名共享内存，需要自动清理；false:非匿名共享内存，无需自动清理
    UbseMemShmAffinitySocketInfo withAffinity{0, "", false}; // 默认不启用指定CPU平面创建
    UbseMemLenderInfo lenderInfo;                            // 指定借出方信息
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShareBorrowReq& obj);
};

struct UbseMemShareAttachReq : public UbseMemBaseBorrowReq {
    std::string importNodeId; // 导入节点ID
    size_t size{0};           // 必填，单位字节，map的大小，用来check大小，必须小于create.size
    FdOwner owner;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShareAttachReq& obj);
};

struct UbseMemShareDetachReq : public UbseMemBaseBorrowReq {
    std::string unImportNodeId; // 去导入节点ID
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShareDetachReq& obj);
};

struct UbseMemAddrBorrowReq : public UbseMemBaseBorrowReq {
    std::string importNodeId{};                    // 导入节点id
    int srcSocket{-1};                             // 内存申请需求方节点socket信息 -1 无效
    int srcNuma{-1};                               // 内存申请需求方节点NUMA信息  -1 无效
    uint64_t importPid{0};                         // 借入进程PID
    std::string exportNodeId{};                    // 借出节点id
    int dstSocket{-1};                             // 内存申请借出方节点socket信息 -1 无效
    int dstNuma{-1};                               // 内存申请借出方节点NUMA信息  -1 无效
    uint64_t exportPid{0};                         // 借出进程PID
    std::vector<UbseMemAddrInfo> exportAddrList{}; // 借出地址段  最大段数=CPU核数
    friend std::ostream& operator<<(std::ostream& os, const UbseMemAddrBorrowReq& obj);
};

struct UbseMemOperationResp {
    std::string name{};
    std::string requestNodeId{}; // 发起去借用的节点ID
    uint32_t errorCode{0};       // 操作错误码
    std::string errMsg{};        // 错误描述信息
    std::string realSize{};      // request size向上取整
    std::vector<uint64_t> memIdList{};
    int64_t remoteNumaId{-1}; // 远端Numa在本地呈现的NumaId
    uint64_t requestId{};
};

struct UbseMemReturnReq : public UbseMemBaseBorrowReq {
    uid_t uid{}; // 删除的uid
    gid_t gid{};
    std::string importNodeId; // 导入节点
};

struct UbseMemImportResult {
    uint64_t memId{0};  // obmm导入返回的memid
    int64_t numaId{-1}; // 导入类型为NUMA时，该值为导入的numaId，以设备导入时，该值无意义
    friend std::ostream& operator<<(std::ostream& os, const UbseMemImportResult& obj);
};

struct UbseMemImportStatus {
    uint32_t errCode{0}; // 导入执行过程中的所有可能错误信息
    uint32_t scna{0};
    std::vector<UbseMemImportResult> importResults{};
    std::vector<mti::mami::UbseMamiMemImportResult> decoderResult{};
    UbseMemState expectState = UBSE_MEM_STATE_SUCCEEDED;
    UbseMemState state = UBSE_MEM_STATE_INIT;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemImportStatus& obj);
};

struct UbseMemObmmInfo {
    uint64_t memId{0};
    ubse_mem_obmm_mem_desc desc;
    UbMemFaultType memIdStatus{UB_MEM_HEALTHY}; // 内存状态
    friend std::ostream& operator<<(std::ostream& os, const UbseMemObmmInfo& obj);
};

struct UbseMemExportStatus {
    uint32_t errCode{0};                                 // 导出执行过程中的所有可能错误信息
    std::vector<UbseMemObmmInfo> exportObmmInfo{};       // 执行导出后的obmm描述信息
    UbseMemState expectState = UBSE_MEM_STATE_SUCCEEDED; // 对象期望状态
    UbseMemState state = UBSE_MEM_STATE_INIT;            // 对象当前状态
    friend std::ostream& operator<<(std::ostream& os, const UbseMemExportStatus& obj);
};

struct UbseMemDebtNumaInfo {
    std::string nodeId; // 节点id
    int socketId{-1};   // socket id
    int64_t numaId{-1}; // numa id
    uint64_t size{0};
    uint32_t portId{0}; // 导入端portId，仅importNumaInfos有值
    uint32_t chipId{0}; // 导入端chipId，仅importNumaInfos有值
    friend std::ostream& operator<<(std::ostream& os, const UbseMemDebtNumaInfo& obj);
};

// 算法决策结果
struct UbseMemAlgoResult {
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos{}; // 导出numa借用关系
    std::vector<UbseMemDebtNumaInfo> importNumaInfos{}; // 导入numa借用关系
    uint32_t blockSize{128};                            // 芯片拆分粒度, 单位MB
    uint32_t attachSocketId{0};                         // 当前借用实际使用的硬件通路，来源cna查询
    friend std::ostream& operator<<(std::ostream& os, const UbseMemAlgoResult& obj);
};

class UbseMemBorrowExportBaseObj {
public:
    // 导入导出的对应关系，addr的特殊处理，在agent端的export的时候，nodeId, numaId 还有对应的size, 回到master之后更新对socketId
    // 从导出端恢复的时候，这个结果不恢复
    UbseMemAlgoResult algoResult{}; // 算法决策结果
    UbseMemExportStatus status{};   // 执行结果信息
    uint32_t errorCode{0};
    bool isCreateReportReceived{false};    // 主节点是否已经收到过上报，非持久化数据
    bool isDestroyedReportReceived{false}; // 主节点是否已经收到过上报，非持久化数据
    friend std::ostream& operator<<(std::ostream& os, const UbseMemBorrowExportBaseObj& obj);
};

class UbseMemBorrowImportBaseObj {
public:
    // addr借用，导出完成之后，在master发给agent之前，这个结果保证给更新完, 导入端恢复数据的时候，这块需要恢复
    UbseMemAlgoResult algoResult{};                // 算法决策结果
    std::vector<UbseMemObmmInfo> exportObmmInfo{}; // 执行导出的obmm信息，用于导入
    UbseMemImportStatus status{};                  // 执行导入结果
    uint32_t errorCode{0};
    bool isCreateReportReceived{false};    // 主节点是否已经收到过上报，非持久化数据
    bool isDestroyedReportReceived{false}; // 主节点是否已经收到过上报，非持久化数据
    bool isLastExportSuccess{false};       // 上一次状态是否是exportSuccess
    friend std::ostream& operator<<(std::ostream& os, const UbseMemBorrowImportBaseObj& obj);
};

class UbseMemFdBorrowExportObj final : public UbseMemBorrowExportBaseObj {
public:
    UbseMemFdBorrowReq req; // 请求参数
    UbseMemReturnReq returnReq;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemFdBorrowExportObj& obj);
};

class UbseMemFdBorrowImportObj final : public UbseMemBorrowImportBaseObj {
public:
    UbseMemFdBorrowReq req; // 请求参数
    UbseMemReturnReq returnReq;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemFdBorrowImportObj& obj);
};

class UbseMemNumaBorrowExportObj final : public UbseMemBorrowExportBaseObj {
public:
    UbseMemNumaBorrowReq req; // 请求参数
    UbseMemReturnReq returnReq;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemNumaBorrowExportObj& obj);
};

class UbseMemNumaBorrowImportObj final : public UbseMemBorrowImportBaseObj {
public:
    UbseMemNumaBorrowReq req; // 请求参数
    UbseMemReturnReq returnReq;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemNumaBorrowImportObj& obj);
};

class UbseMemAddrBorrowExportObj final : public UbseMemBorrowExportBaseObj {
public:
    UbseMemAddrBorrowReq req{}; // 请求参数
    UbseMemReturnReq returnReq{};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemAddrBorrowExportObj& obj);
};

class UbseMemAddrBorrowImportObj : public UbseMemBorrowImportBaseObj {
public:
    UbseMemAddrBorrowReq req; // 请求参数
    UbseMemReturnReq returnReq;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemAddrBorrowImportObj& obj);
};

class UbseMemShareBorrowExportObj final : public UbseMemBorrowExportBaseObj {
public:
    UbseMemShareBorrowReq req; // 请求参数
    UbseMemReturnReq returnReq;
    // 每块一个迁移标记[isExport:1|originExportNodeId:15|version:4|index:12]，isExport恒为1；
    // index==块下标、块间version一致；普通创建version=0/origin=0(执行层兜底生成)，
    // 迁移时生成version=源+1、origin=源nodeId(1～32767)；
    // 重启恢复时从内核sysfs priv的customMeta.migrateInfo回填
    std::vector<uint32_t> migrateInfos{};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShareBorrowExportObj& obj);
};

// 共享内存map的入参，map的uid必须等于create的uid 等等
struct UbseMemAttachResourceShareAttr {
    uid_t uid{}; // 使用的uid
    gid_t gid{};
    size_t size{0};  // 必填，单位字节，map的大小，用来check大小，必须小于create.size
    FdOwner owner{}; // 内存资源属主信息和访问权限
    friend std::ostream& operator<<(std::ostream& os, const UbseMemAttachResourceShareAttr& obj);
};

// 当前块的待提交内容；正式数组在 end(1) 成功前仍表示旧后端。
struct UbseMemMigrateBlockData {
    UbseMemObmmInfo exportInfo{};
    std::optional<mti::mami::UbseMamiMemImportResult> decoder;
    UbseMemAlgoResult targetLayout{};
};

// 每个 name 只保留当前块，不保存 RPC 结果或整份 import/export 对象。
struct UbseMemMigrateSwitchContext {
    uint32_t blockIndex{0};
    uint64_t exportMemId{0};
    uint32_t migrateInfo{0};
    std::optional<UbseMemMigrateBlockData> target; // B 目标直接使用本地 export 账本
    std::optional<bool> endDecision; // true=end(1)已成功；false=回退；无值=尚未确认提交或决定回退
};

class UbseMemShareBorrowImportObj final : public UbseMemBorrowImportBaseObj {
public:
    // 有沒有真的執行obmm操作，共享内存，自己map自己不需要執行obmm操作，也不需要執行unmap操作
    bool realExe{true};
    std::string importNodeId{};                 // 节点id
    UbseMemAttachResourceShareAttr shareAttr{}; // map 参数
    UbseMemShareBorrowReq req;                  // 请求参数
    UbseMemReturnReq returnReq;
    // 每块一个迁移标记，version/index与对应导出块一致，isExport和originExportNodeId恒为0；
    // 共享创建时继承导出标记(经ShmMigrateImportInfo清除身份位及origin)，迁移begin时随临时对象下发；
    // self-import仅为逻辑账本，所共享的物理export设备priv仍标记为导出。
    std::vector<uint32_t> migrateInfos{};
    std::optional<UbseMemMigrateSwitchContext> switchContext;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShareBorrowImportObj& obj);
};

// 共享内存迁移 EID 长度（128bit 小端，与 obmm 对接层 ObmmSwitchParam 的 UBSE_EID_LENGTH 一致）
constexpr uint32_t UBSE_SHM_SWITCH_EID_LENGTH = 16u;

// 目标节点已导入该共享内存时，将已有导入设备切为本地导出池。desc 仅在本节点 begin 后暂存，
// 远端导入节点必须等 switch_end(1) 成功后才能收到新后端凭证。
struct UbseMemShmSwitchExportObj {
    uint64_t importMemId{0};
    uint32_t blockIndex{0};
    std::vector<uint64_t> numaSizes{};
    uint64_t exportFlags{0};
    std::array<uint8_t, UBSE_SHM_SWITCH_EID_LENGTH> seid{};
    std::array<uint8_t, UBSE_SHM_SWITCH_EID_LENGTH> deid{};
    std::vector<uint8_t> priv{};
    UbseMemShareBorrowExportObj exportObj{}; // priv 为空时供 MMI 复用普通导出元数据布局
};

// O3: u32 EID 零扩展为 128bit 小端 EID 数组（低 32bit 承载有效值，高位补 0；
// 拓扑查询 GetEid 仅产出 u32，禁止截断语义缺失的直接当 128bit 使用, Step1 设计 §8 O3）
inline void FillEid16FromU32(std::array<uint8_t, UBSE_SHM_SWITCH_EID_LENGTH>& eid, uint32_t value)
{
    eid.fill(0);
    eid[0] = static_cast<uint8_t>(value & 0xFFU);
    eid[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    eid[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    eid[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

// 共享内存故障迁移：导入链路块级切换入参（Step1 设计 §6 输入契约，编排层按 (memId, importNodeId) 组装）
class UbseMemShmSwitchObj final {
public:
    std::string name{};                                        // 迁移资源名，日志定位
    std::string importNodeId{};                                // 导入节点 id
    uint64_t oldImportMemId{0};                                // 待切换的导入 memId
    uint64_t newPa{0};                                         // 新链路物理地址
    uint32_t newScna{0};                                       // 新链路使用方侧 CNA
    uint32_t newDcna{0};                                       // 新链路提供方侧 CNA
    uint8_t baseDist{0};                                       // 新链路基础 NUMA 距离，0~255
    std::array<uint8_t, UBSE_SHM_SWITCH_EID_LENGTH> newSeid{}; // 使用方侧 EID，128bit 小端
    std::array<uint8_t, UBSE_SHM_SWITCH_EID_LENGTH> newDeid{}; // 提供方侧 EID，128bit 小端
    std::vector<uint8_t> priv{}; // 原 import 链路私有数据原样透传，switch_end(1) 后内核替换
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShmSwitchObj& obj);
};

// 共享内存故障迁移受理请求（SDK 报文字段一一对应，Step2 设计 §4.1）
struct UbseMemShmMigrateReq {
    std::string name{};         // 借用标识
    std::string targetNodeId{}; // dispatcher 解包后使用的目标节点 ID
    UbseUdsInfo caller{};       // 接入端从连接凭据获取，master做资源级鉴权
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShmMigrateReq& req);
};

// 共享内存故障迁移受理响应：errCode 0=受理成功；幂等命中回 MIGRATE_IN_PROGRESS
struct UbseMemShmMigrateResp {
    uint32_t errCode{0};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShmMigrateResp& resp);
};

// 共享内存故障迁移查询请求(Step6 设计 §5)：仅 name，受理/查询共用 mem.shm.migrate 权限对象
struct UbseMemShmMigrateGetReq {
    std::string name{}; // 借用标识
    uint32_t reserved{0};
    UbseUdsInfo caller{}; // 接入端从连接凭据获取，master做资源级鉴权
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShmMigrateGetReq& req);
};

// 共享内存故障迁移查询响应(Step6 设计 §5)：state 原值透传, SDK 侧强转枚举;
struct UbseMemShmMigrateGetResp {
    uint32_t errCode{0};
    std::string name{};
    std::string srcNodeId{};
    std::string dstNodeId{};
    uint32_t state{0}; // ubs_mem_migrate_state_t 原值
    uint32_t totalMemids{0};
    uint32_t migratedMemids{0};
    uint64_t totalSize{0};
    uint32_t failReason{0}; // UbseMemMigrateFailReason, FAILED 时非 0
    std::string errMsg{};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShmMigrateGetResp& resp);
};

// 共享内存故障迁移取消请求(Step7 设计 §8)：仅 name，受理/查询/取消共用 mem.shm.migrate 权限对象
struct UbseMemMigrateCancelReq {
    std::string name{}; // 借用标识
    uint32_t reserved{0};
    UbseUdsInfo caller{}; // 接入端从连接凭据获取，master做资源级鉴权
    friend std::ostream& operator<<(std::ostream& os, const UbseMemMigrateCancelReq& req);
};

// 共享内存故障迁移取消响应(Step7 设计 §8)：同步回包仅表示取消受理成功，清理异步执行
struct UbseMemMigrateCancelResp {
    uint32_t errCode{0};
    uint32_t reserved{0};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemMigrateCancelResp& resp);
};

// 共享内存故障迁移细分失败码
enum UbseMemMigrateFailReason
{
    MIGRATE_FAIL_ALLOCATE = 1,         // 决策失败(水位/节点不可用)
    MIGRATE_FAIL_RPC = 2,              // RPC发起/发送或入队ACK失败，不包含异步业务结果超时
    MIGRATE_FAIL_OBMM_EXPORT = 3,      // agent侧obmm_export失败(rsp.errCode)
    MIGRATE_FAIL_LAYOUT = 4,           // 老/新导出块数不一致(迁移按索引逐块对齐)
    MIGRATE_FAIL_OLD_OBJ_LOST = 5,     // 老exportObj丢失
    MIGRATE_FAIL_SIGN = 6,             // 高安签名/验签失败
    MIGRATE_FAIL_BLOCK_TOO_LARGE = 7,  // 值保留占位(报文错误码冻结): 块过大预检已删除, 不再产生
    MIGRATE_FAIL_COPY = 8,             // Agent返回copy业务失败
    MIGRATE_FAIL_CONVERGE = 9,         // Master有效账本合入或一致性检查失败
    MIGRATE_FAIL_RESULT_TIMEOUT = 10,  // 等待异步业务结果超时
    MIGRATE_FAIL_DISPATCH = 11,        // 初始迁移任务投递失败
    MIGRATE_FAIL_TIMER = 12,           // 阶段定时器注册失败
    MIGRATE_FAIL_BEGIN_IMPORT = 13,    // Agent返回begin_import业务失败（含decoder准备）
    MIGRATE_FAIL_BEGIN_EXPORT = 14,    // Agent返回begin_export业务失败
    MIGRATE_FAIL_END_IMPORT = 15,      // Agent返回end_import(1)业务失败
    MIGRATE_FAIL_END_EXPORT = 16,      // Agent返回end_export(1)业务失败
    MIGRATE_FAIL_REQUEST_PREPARE = 17, // Master准备当前块请求失败，尚未发起RPC
    MIGRATE_FAIL_ROLLBACK_IMPORT = 18, // Agent返回end_import(0)业务失败
    MIGRATE_FAIL_ROLLBACK_EXPORT = 19, // Agent返回end_export(0)业务失败
};

// 请求RPC仅确认入队，结果以name定位任务，按块、节点及已有业务资源信息确认归属。
struct UbseMemMigrateRpcInfo {
    std::string name{};
    uint32_t blockIndex{0};
    std::string srcNodeId{};
    uint64_t exportMemId{0};
    uint32_t migrateInfo{0};
    // 请求归属标记：end(0)为true，其余操作为false；回包匹配时区分提交与回退方向，执行决定由end请求的success指定。
    bool rollback{false};
};

// begin 只携当前块目标和该块所需的授权材料；本地设备、旧 decoder 来自账本。
struct UbseMemMigrateSwitchBeginImportReq {
    UbseMemObmmInfo targetBlock{};
    UbseMemAlgoResult targetLayout{};
    UbseTrustRingData trustRingData{};
    uint64_t oldExportMemId{0};
    UbseMemMigrateRpcInfo rpcInfo{};
};

// B 目标仍接收完整布局模板以构造 priv，执行结果仅回传当前块。
struct UbseMemMigrateBeginExportReq {
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemMigrateRpcInfo rpcInfo{};
};

struct UbseMemMigrateBeginExportResp {
    uint32_t errCode{0};
    UbseMemObmmInfo block{};
    UbseMemMigrateRpcInfo rpcInfo{};
};

struct UbseMemMigrateEndExportReq {
    uint32_t success{0};
    UbseMemMigrateRpcInfo rpcInfo{};
};

struct UbseMemMigrateSwitchEndImportReq {
    uint32_t success{0}; // 1=提交，0=回退；待提交资源从本地账本取得
    UbseMemMigrateRpcInfo rpcInfo{};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemMigrateSwitchEndImportReq& req);
};

// A/B统一双后端拷贝，设备、长度和访问模式均取本地import账本当前块。
struct UbseMemMigrateDataCopyReq {
    UbseMemMigrateRpcInfo rpcInfo{};
    friend std::ostream& operator<<(std::ostream& os, const UbseMemMigrateDataCopyReq& req);
};

struct UbseMemMigrateOpResp {
    uint32_t errCode{0};
    std::optional<UbseMemMigrateBlockData> block; // begin=待提交块，end=生效块，copy=无
    UbseMemMigrateRpcInfo rpcInfo{};
    bool committed{false}; // 已确认本块end(1)生效；即使errCode非0，也不得回退本块
    friend std::ostream& operator<<(std::ostream& os, const UbseMemMigrateOpResp& resp);
};

using UbseMemFdImportObjMap = std::unordered_map<std::string, UbseMemFdBorrowImportObj>;       // resourceId,obj
using UbseMemFdExportObjMap = std::unordered_map<std::string, UbseMemFdBorrowExportObj>;       // resourceId,obj
using UbseMemNumaImportObjMap = std::unordered_map<std::string, UbseMemNumaBorrowImportObj>;   // resourceId,obj
using UbseMemNumaExportObjMap = std::unordered_map<std::string, UbseMemNumaBorrowExportObj>;   // resourceId,obj
using UbseMemShareImportObjMap = std::unordered_map<std::string, UbseMemShareBorrowImportObj>; // resourceId,obj
using UbseMemShareExportObjMap = std::unordered_map<std::string, UbseMemShareBorrowExportObj>; // resourceId,obj
using UbseMemAddrImportObjMap = std::unordered_map<std::string, UbseMemAddrBorrowImportObj>;   // resourceId,obj
using UbseMemAddrExportObjMap = std::unordered_map<std::string, UbseMemAddrBorrowExportObj>;   // resourceId,obj

struct NodeMemDebtInfo {
    UbseMemFdImportObjMap fdImportObjMap;
    UbseMemFdExportObjMap fdExportObjMap;
    UbseMemNumaImportObjMap numaImportObjMap;
    UbseMemNumaExportObjMap numaExportObjMap;
    UbseMemShareImportObjMap shareImportObjMap;
    UbseMemShareExportObjMap shareExportObjMap;
    UbseMemAddrImportObjMap addrImportObjMap;
    UbseMemAddrExportObjMap addrExportObjMap;
};

using NodeMemDebtInfoMap = std::unordered_map<std::string, NodeMemDebtInfo>; // nodeId,NodeMemDebtInfo

struct UbseMemNumaLoc {
    std::string nodeId; // 节点id
    int socketId{-1};   // socket id
    int64_t numaId{-1}; // numa id
    bool operator==(const UbseMemNumaLoc& other) const;
    // 重载 < 运算符
    bool operator<(const UbseMemNumaLoc& other) const;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemNumaLoc& obj);
};

struct SocketCnaInfo {
    std::string importNodeId; // 导入端的节点ID
    uint32_t importSocketId;  // 挂载的socketId
    uint32_t seid{};          // 挂载的socketId对应的eid
    std::string exportNodeId; // 导出端的节点ID
    uint32_t exportSocketId;  // 导出端的socketId
    uint32_t deid{};          // 导出端的socketId对应的eid
    uint32_t scna{};          // 借入端的cna
    uint32_t dcna{};          // 借出端的cna
    uint16_t marId{};         // 借入端导入时用端口所属的mar
    friend std::ostream& operator<<(std::ostream& os, const SocketCnaInfo& obj);
};
} // namespace ubse::adapter_plugins::mmi
#endif // UBSE_MMI_DEF_H
