/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef RACK_MANAGER_RACK_ELECTION_H
#define RACK_MANAGER_RACK_ELECTION_H

#include <functional>
#include <string>

namespace ubse::election {
constexpr const char* ELECTION_ROLE_INITIALIZE = "initiator";
constexpr const char* ELECTION_ROLE_MASTER = "master";
constexpr const char* ELECTION_ROLE_STANDBY = "standby";
constexpr const char* ELECTION_ROLE_AGENT = "agent";
constexpr const int ELECTION_NODE_OFFLINE = 0;            // 服务下线
constexpr const int ELECTION_NODE_ONLINE = 1;             // 服务上线
constexpr const int ELECTION_HANDLER_NAME_MAX_SIZE = 128; // 回调函数名字最长长度
constexpr const uint32_t UbseElectionOk = 0;              // 0 返回成功
constexpr const uint32_t UbseElectionError = 1;           // 1 默认在回调函数中不中断所有执行
constexpr const uint32_t UbseElectionTypeError = 2;       // 2 订阅类型错误
constexpr const uint32_t UbseElectionHandlerError = 3;    // 3 回调函数异常
constexpr const uint32_t UbseElectionNameError = 4;       // 4 回调函数名字长度异常
constexpr const uint32_t UbseElectionDupNameError = 5;    // 5 回调函数名字重复

struct UbseRoleInfo {
    std::string nodeId;   // 节点 id
    std::string nodeRole; // 节点角色
    int status;           // 节点服务状态（0：下线，1：在线，2：异常）

    UbseRoleInfo() : nodeId("unknown"), nodeRole("unknown"), status(0) {} // 设置默认值

    UbseRoleInfo(const std::string &id, const std::string &role, int stat = 0)
        : nodeId(id),
          nodeRole(role),
          status(stat)
    {
    }
};

using UBSE_ID_TYPE = std::string;

enum class UbseElectionEventType {
    CHANGE_TO_MASTER,           // 初始化为主(只在主节点)
    CHANGE_TO_STANDBY,          // 初始化为备(只在备节点)
    CHANGE_TO_AGENT,            // 初始化为从(只在从节点)
    CHANGE_SWITCH_TO_STANDBY,   // 主选出备(只在主节点)
    STANDBY_CHANGE_TO_MASTER,   // 备升主(只在备节点)
    CHANGE_TO_SWITCHOVER,       // 主备倒换前，主节点执行(只在主节点)
    MASTER_ONLINE_NOTIFICATION, // 主节点上线通知(在所有节点)
    NODE_UP,                    // 感知节点上线(只在主节点)
    NODE_DOWN,                  // 感知节点下线(只在主节点)

    COUNT
};

// 获取所有枚举值的数组
constexpr std::array<UbseElectionEventType, static_cast<std::size_t>(UbseElectionEventType::COUNT)> GetEleEventTypes()
{
    return {{
        UbseElectionEventType::CHANGE_TO_MASTER,
        UbseElectionEventType::CHANGE_TO_STANDBY,
        UbseElectionEventType::CHANGE_TO_AGENT,
        UbseElectionEventType::CHANGE_TO_SWITCHOVER,
        UbseElectionEventType::STANDBY_CHANGE_TO_MASTER,
        UbseElectionEventType::CHANGE_SWITCH_TO_STANDBY,
        UbseElectionEventType::MASTER_ONLINE_NOTIFICATION,
        UbseElectionEventType::NODE_UP,
        UbseElectionEventType::NODE_DOWN,
    }};
}

// 检查 UbseElectionEventType 是否存在于枚举中
bool IsValidElectionEventType(UbseElectionEventType type);

enum class UbseElectionHandlerPriority {
    HIGH,
    MEDIUM,
    LOW
};

using ElectinHandler = std::function<uint32_t(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId)>;

struct UbseElectionHandler {
    UbseElectionEventType type;           // 事件类型
    std::string name;                     // 注册函数名字（唯一）
    UbseElectionHandlerPriority priority; // 优先级
    int sequenceId;                       // 执行顺序
    ElectinHandler handler;               // 注册函数
    bool operator<(const UbseElectionHandler &other) const
    {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return sequenceId < other.sequenceId;
    }
    bool operator==(const UbseElectionHandler &other) const
    {
        return type == other.type && name == other.name && priority == other.priority &&
               sequenceId == other.sequenceId && handler.target_type() == other.handler.target_type();
    }
};

// 构造函数，初始化优先级为低，事件类型为枚举类型的计数，顺序 ID为 100，函数名为空，回调函数为空
class UbseElectionHandlerBuilder {
public:
    UbseElectionHandlerBuilder()
    {
        ubseElectionHandler.priority = UbseElectionHandlerPriority::LOW;
        ubseElectionHandler.type = UbseElectionEventType::COUNT;
        ubseElectionHandler.sequenceId = 100; // 100, 顺序ID为100
        ubseElectionHandler.name = "";
        ubseElectionHandler.handler = nullptr;
    }

    // 设置事件类型
    inline UbseElectionHandlerBuilder &SetType(UbseElectionEventType type)
    {
        ubseElectionHandler.type = type;
        return *this;
    }

    // 设置优先级
    inline UbseElectionHandlerBuilder &SetPriority(UbseElectionHandlerPriority priority)
    {
        ubseElectionHandler.priority = priority;
        return *this;
    }

    // 设置执行顺序
    inline UbseElectionHandlerBuilder &SetSequenceId(int sequenceId)
    {
        ubseElectionHandler.sequenceId = sequenceId;
        return *this;
    }

    // 设置函数名
    inline UbseElectionHandlerBuilder &SetName(const std::string &name)
    {
        ubseElectionHandler.name = name;
        return *this;
    }

    // 设置回调函数
    inline UbseElectionHandlerBuilder &SetHandler(ElectinHandler handler = nullptr)
    {
        ubseElectionHandler.handler = std::move(handler);
        return *this;
    }

    // 构建函数，返回一个 UbseElectionHandler 对象
    UbseElectionHandler Build()
    {
        return ubseElectionHandler;
    }

private:
    UbseElectionHandler ubseElectionHandler; // 私有成员变量，用于存储构建过程中的数据
};

/**
 * @brief 获取集群的节点数量
 * @param[in/out] count: 节点数量
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetNodeCount(uint32_t &count)
{
    return 0;
}

/**
 * @brief 获取count个节点的角色信息
 * @param[out] roleInfos: 节点角色信息列表
 * @param[in/out] count: 节点个数
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetRoleInfos(std::vector<UbseRoleInfo> &roleInfos, uint32_t &count)
{
    return 0;
}

/**
 * @brief 获取单个节点的角色信息
 * @param[out] roleInfo: 节点信息
 * @param[in] nodeId: 节点ID
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetRoleInfo(UbseRoleInfo &roleInfo, const std::string &nodeId)
{
    return 0;
}

/**
 * @brief 获取主节点信息
 * @param[out] roleInfo: 主节点信息
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetMasterInfo(UbseRoleInfo &roleInfo)
{
    return 0;
}

/**
 * @brief 获取备节点信息
 * @param[out] roleInfo: 备节点信息
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetStandbyInfo(UbseRoleInfo &roleInfo)
{
    return 0;
}

/**
 * @brief 获取所有节点信息(分布式选举集群中的节点)
 * @param[out] roleInfos: 节点角色信息列表
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetAllNodeInfos(std::vector<UbseRoleInfo> &roleInfos)
{
    return 0;
}

/**
 * @brief 新增选举角色改变的回调函数
 * @param[in] handler 回调函数
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseElectionChangeAttachHandler(const UbseElectionHandler &handler)
{
    return 0;
}

/**
 * @brief 移除选举角色改变的回调函数
 * @param[in] handler 回调函数
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseElectionChangeDeAttachHandler(const UbseElectionHandler &handler)
{
    return 0;
}

/**
 * @brief 查询 master/standby/agent 节点工作状态（agent 表示当前节点工作状态）
 * @param[in] 查询角色信息
 * @param[in/out] status 节点工作状态 0：没有准备好，1：准备好
 * @return 成功返回 0, 失败返回非 0
 */
uint32_t UbseGetNodeStatus(const std::string &role, uint8_t &status)
{
    return 0;
}

/**
 * @brief 查询当前节点信息
 * @param[out] currentNode: 当前节点信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetCurrentNodeInfo(UbseRoleInfo &currentNode)
{
    return 0;
}

/**
 * @brief 查询所有物理节点 nodeIds
 * @param[out] nodeIds: 所有物理节点id
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetNodeIds(std::vector<UBSE_ID_TYPE> &nodeIds)
{
    return 0;
}

/**
 * @brief 查询当前所有节点状态
 * @param roleInfos [out]: 当前所有节点状态信息
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetAllNodeStatusInfo(std::vector<UbseRoleInfo> &roleInfos)
{
    return 0;
}

/**
 * 获取当前角色
 * @param role [OUT] 待获取角色
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseGetRole(std::string &role)
{
    role = "Master";
    return 0;
}

/**
 * 获得Master角色节点ID
 * @param masterNodeId [out] Master角色节点ID
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseGetMasterNodeId(std::string &masterNodeId)
{
    masterNodeId = "NODE11347";
    return 0;
}

/**
 * 获得当前节点的NodeID
 * @param currentNodeId [out] 获得当前节点的NodeID
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseGetCurrentNodeId(std::string &currentNodeId)
{
    currentNodeId = "NODE11347";
    return 0;
}
} // namespace ubse::election

#endif // RACK_MANAGER_RACK_ELECTION_H
