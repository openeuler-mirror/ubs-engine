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
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "ubse_def.h"
#include "ubse_mmi_obmm_def.h"
#include "adapter_plugins/mti/ubse_mti_mami_def.h"

namespace ubse::adapter_plugins::mmi {
constexpr uint32_t UBSE_MAX_USR_INFO_LEN = 32;
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
    UBSE_MEM_IMPORT_DESTROYED
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
    uint16_t onePth : 1;
    uint16_t wrDelayComp : 1;
    uint16_t reduceDelayComp : 1;
    uint16_t cmoDelayComp : 1;
    uint16_t so : 1;
    uint16_t adTrOchip : 1;
    uint16_t cacheableFlag : 1;
    uint16_t marId : 3;
    uint16_t rsv0 : 6;
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
    int srcSocket{-1};         // 内存申请需求方节点socket信息 -1 无效
    int srcNuma{-1};           // 内存申请需求方节点NUMA信息  -1 无效
    size_t highWatermark{100}; // 必填，算法百分比，vm自己决策场景，单位%
    size_t lowWatermark{11};   // 必填，算法比分在，单位%
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
    UbseMemPrivData
        ubseMemPrivData; // 传递给OBMM的内存访问属性；全0表示无效，UBSE自行组装访问属性；非全零，直接使用该字段传递给OBMM
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
    uint16_t wrDelayComp{1}; // 目前仅支持接力/非接力模式，1为非接力模式，0为接力模式，默认使用非接力模式
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
    uint32_t portId{0};
    uint32_t chipId{0}; // 中心侧算法后获取，用于远端numa获取以及内存预上线
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

class UbseMemShareBorrowImportObj final : public UbseMemBorrowImportBaseObj {
public:
    // 有沒有真的執行obmm操作，共享内存，自己map自己不需要執行obmm操作，也不需要執行unmap操作
    bool realExe{true};
    std::string importNodeId{};                 // 节点id
    UbseMemAttachResourceShareAttr shareAttr{}; // map 参数
    UbseMemShareBorrowReq req;                  // 请求参数
    UbseMemReturnReq returnReq;
    friend std::ostream& operator<<(std::ostream& os, const UbseMemShareBorrowImportObj& obj);
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
