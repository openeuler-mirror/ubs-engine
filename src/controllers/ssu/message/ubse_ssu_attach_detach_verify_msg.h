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

#ifndef UBSE_SSU_ATTACH_DETACH_VERIFY_MSG_H
#define UBSE_SSU_ATTACH_DETACH_VERIFY_MSG_H

#include <cstdint>
#include <string>
#include <vector>
#include "ubse_com.h"
#include "ubse_ssu_service.h"

namespace ubse::ssu::message {

using namespace ubse::plugin::service::ssu;

// master验证identity通过后，为单个NS返回的字段。
// 仅包含agent本地无法获取的字段（依赖Master-only的GetDevList设备缓存）；
// tgtEid/namespaceId/subNqn由agent从ledger的UbseSsuNameSpaceInfo中取，无需回传。
struct UbseSsuNsVerifyInfo {
    std::string defaultNqn; // 默认NQN
    uint32_t jettyId{0};    // jetty id，BuildNamespaceInfoForBasic需要
    std::string guid;       // 命名空间GUID，AttachDevNameSpace的GUID验证需要
};

// agent端提交verify的意图与挂载/卸载方式对应策略
// isAttach=true时master校验账本state（attach），false时校验state（detach）；
// expectedStrategy表示挂载/卸载方式对应的分配策略，master校验其与账本分配策略严格一致，
// 禁止混用：AttachSpace/DetachSpace=NORMAL，AttachLinearSpace/DetachLinearSpace=LINEAR，
// AttachStripedSpace/DetachStripedSpace=STRIPED
struct UbseSsuAttachDetachVerifyOption {
    bool isAttach{true};         // true=attach校验（含state），false=detach校验
    UbseSsuAllocStrategy expectedStrategy{UbseSsuAllocStrategy::NORMAL}; // 挂载/卸载方式对应的分配策略
    uint32_t raidLevel{0};       // expectedStrategy==STRIPED且isAttach时有效：RAID0/RAID5
    uint32_t chunkSize{0};       // expectedStrategy==STRIPED且isAttach时有效：chunk大小，单位KB
};

struct UbseSsuAttachDetachVerifyReq {
    std::string requestId;
    std::string requestNodeId;
    std::string name;
    UbseSsuAllocIdentityInfo identityInfo;
    UbseSsuAttachDetachVerifyOption option;
};

struct UbseSsuAttachDetachVerifyResp {
    std::string requestId;
    uint32_t errorCode{0};
    bool alreadyAttached{false}; // attach幂等：master校验state==ATTACHED时置true，agent端无需实际attach
    bool alreadyDetached{false}; // detach幂等：master校验state==CREATED时置true，agent端无需实际detach
    std::vector<UbseSsuNsVerifyInfo> nsVerifyList;
    // agent无本地账本，attach/detach时需从这里获取namespace列表
    std::vector<UbseSsuNameSpaceInfo> nameSpaceList;
    std::string devName; // 聚合块设备名称（Linear/Striped场景），master账本记录；agent重复挂载时据此回填devPath，序列化在末尾
};

class UbseSsuAttachDetachVerifyReqMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuAttachDetachVerifyReqMsg() = default;

    UbseSsuAttachDetachVerifyReqMsg(const std::string &requestId, const std::string &requestNodeId,
                                    const std::string &name, const UbseSsuAllocIdentityInfo &identity,
                                    const UbseSsuAttachDetachVerifyOption &option);

    const UbseSsuAttachDetachVerifyReq &GetAttachDetachVerifyReq() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuAttachDetachVerifyReq req_{};
};

class UbseSsuAttachDetachVerifyRespMsg : public ubse::com::UbseRpcMessage {
public:
    UbseSsuAttachDetachVerifyRespMsg() = default;

    explicit UbseSsuAttachDetachVerifyRespMsg(const UbseSsuAttachDetachVerifyResp &resp);
    const UbseSsuAttachDetachVerifyResp &GetAttachDetachVerifyResp() const;

    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override;
    uint32_t Deserialize(const uint8_t *data, uint32_t size) override;

private:
    UbseSsuAttachDetachVerifyResp resp_{};
};

} // namespace ubse::ssu::message

#endif // UBSE_SSU_ATTACH_DETACH_VERIFY_MSG_H
