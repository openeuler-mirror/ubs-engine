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

#include "over_commit_msg.h"

#include "ubse_logger.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "numa_info.h"
#include "exporter.h"
#include "ubse_election.h"
#include "over_commit_storage.h"

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace turbo::rmrs;

// rpc消息向agent获取node本地的虚拟机信息
MpResult OverCommitMsg::GetVmNumaInfoMapRpc(std::string importNodeId,
                                            std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList,
                                            uint16_t localNumaId)
{
    LOG_DEBUG
        << "[OverCommit][MsgHandler] Master to invoke the slave GetVmNumaInfoMapRpc. NodeId=" << importNodeId
        << ", localNumaId=" << localNumaId << ".";
    UbseComEndpoint endpoint_get_vm_info = {.moduleId = MP_MODULE_CODE,
                                            .serviceId = message::OPCODE_OVER_COMMIT_MIGRATE_GET_LOCAL_VM_INFO,
                                            .address = importNodeId};
    OverCommitFaultVmNumaInfoParam vmNumaInfoParam = {.remoteNumaId = localNumaId};
    RmrsOutStream builder;
    builder << vmNumaInfoParam;
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    reqData.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    uint32_t ret = UbseRpcSend(endpoint_get_vm_info, reqData, &vmNumaInfoWithSocketList, GetVmNumaInfoMapResHandler);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[OverCommit][MsgHandler] UbseRpcSend failed.";
        return MEM_POOLING_ERROR;
    }
    if (vmNumaInfoWithSocketList.size() == 0) {
        LOG_ERROR << "[OverCommit][MsgHandler] GetVmNumaInfoMapResHandler failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[OverCommit][MsgHandler] GetVmNumaInfoMapResHandler success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitMsg::GetVmNumaInfoMapLocal(std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList,
                                              uint16_t localNumaId)
{
    LOG_DEBUG
        << "[OverCommit][MsgHandler] GetVmNumaInfoMapRpc by local node." << " localNumaId=" << localNumaId << ".";
    auto ret = GetLocalNumaVms(localNumaId, vmNumaInfoWithSocketList);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR
            << "[OverCommit][MsgHandler] GetVmNumaInfoMapResHandler failed.";
        return MEM_POOLING_ERROR;
    }
    if (vmNumaInfoWithSocketList.size() == 0) {
        LOG_ERROR
            << "[OverCommit][MsgHandler] GetVmNumaInfoMapResHandler failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[OverCommit][MsgHandler] GetVmNumaInfoMapResHandler success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitMsg::GetVmNumaInfoMapRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    LOG_INFO
        << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler start.";

    if (req.data == nullptr || req.len == 0) {
        LOG_ERROR << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler req.data is nullptr.";
        return MEM_POOLING_ERROR;
    }
    OverCommitFaultVmNumaInfoParam param{};
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> param;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MpResult ret = GetLocalNumaVms(param.remoteNumaId, vmNumaInfoWithSocketList);

    OverCommitFaultVmNumaInfoResult result{.vmNumaInfoWithSocketList = vmNumaInfoWithSocketList};
    RmrsOutStream outBuilder;
    outBuilder << result;
    resp.len = outBuilder.GetSize();
    resp.data = outBuilder.GetBufferPointer();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (MEM_POOLING_OK != ret) {
        LOG_ERROR
            << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler failed res=" << ret << ".";
    }
    LOG_INFO
        << "[OverCommit][FaultManagement] GetVmNumaInfoMapRecvHandler end.";
    return ret;
}

MpResult OverCommitMsg::GetLocalNumaVms(uint16_t localNumaId,
                                        std::vector<VmNumaInfoWithSocket> &vmNumaInfoWithSocketList)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (MEM_POOLING_OK != ret) {
        LOG_ERROR << "[OverCommit][MsgHandler] GetVmInfoImmediately failed.";
        return MEM_POOLING_ERROR;
    }

    if (vmDomainInfos.empty()) {
        LOG_INFO << "[OverCommit][MsgHandler] CurRemoteNode vm empty, numaId=" << localNumaId << ".";
        return MEM_POOLING_OK;
    }

    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    ret = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    if (MEM_POOLING_OK != ret) {
        LOG_ERROR << "[OverCommit][MsgHandler] GetNumaInfoImmediately failed.";
        return ret;
    }
    if (numaInfos.empty()) {
        LOG_INFO << "[OverCommit][MsgHandler] NumaInfos is empty.";
        return MEM_POOLING_OK;
    }

    for (const mempooling::exportV2::VmDomainInfo &vmDomainInfo : vmDomainInfos) {
        VmNumaInfoWithSocket info{};
        info.pid = vmDomainInfo.metaData.pid;
        info.localNumaId = 0;
        for (const auto &[numaId, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.isLocal) {
                // 本地 NUMA
                info.localNumaId = numaInfo.numaId;
                info.localUsedMem = numaInfo.usedMem;
                info.socketId = numaInfo.socketId; // ★ socketId 必须来自本地 NUMA
            } else {
                // 远端 NUMA
                info.remoteNumaId = numaInfo.numaId;
                info.remoteUsedMem = numaInfo.usedMem;
            }
        }

        if (info.localNumaId == localNumaId) {
            vmNumaInfoWithSocketList.push_back(info);

            LOG_DEBUG << "[OverCommit][MsgHandler][VmNumaInfo] Add pid=" << info.pid
                      << ", localNumaId=" << info.localNumaId << ", localUsedMem=" << info.localUsedMem << "KB"
                      << ", remoteUsedMem=" << info.remoteUsedMem << "KB"
                      << ", socketId=" << info.socketId << ", localFreeMem=" << info.localFreeMem << "KB.";
        }
    }

    LOG_INFO << "[OverCommit][MsgHandler] allVmNumaInfoInfoList size=" << vmNumaInfoWithSocketList.size() << ".";

    return MEM_POOLING_OK;
}

void OverCommitMsg::GetVmNumaInfoMapResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        LOG_WARN << "[OverCommit][FaultManagement] GetVmNumaInfoMapResHandler ctx or respData is null.";
        return;
    }
    OverCommitFaultVmNumaInfoResult result;
    auto *overCommitFaultVmNumaInfoResult = static_cast<OverCommitFaultVmNumaInfoResult *>(ctx);
    if (resCode != MEM_POOLING_OK) {
        LOG_ERROR << "[OverCommit] Send error " << resCode << ".";
    } else {
        RmrsInStream builder(respData.data, respData.len);
        builder >> result;
    }
    *overCommitFaultVmNumaInfoResult = result;
}

uint32_t OverCommitMsg::NumaMemInfoCollectRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    LOG_DEBUG << "[NumaMemInfoCollect] NumaMemInfoCollectRecvHandler start.";
    if (req.data == nullptr || req.len == 0) {
        LOG_ERROR << "[NumaMemInfoCollect] NumaMemInfoCollectRecvHandler req.data is nullptr.";
        return MEM_POOLING_ERROR;
    }
    turbo::rmrs::NumaMemInfoCollectParam numaMemInfoCollectParam;
    turbo::rmrs::ResponseInfoSimpo responseInfoSimpo;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> numaMemInfoCollectParam;

    auto ret = MempoolingMessage::rmrsNumaMemInfoCollect(numaMemInfoCollectParam, responseInfoSimpo);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR
            << "[NumaMemInfoCollectRecv] NumaMemInfoCollectRecvHandler failed res=" << ret << ".";
        return MEM_POOLING_ERROR;
    }

    RmrsOutStream builderOut;
    builderOut << responseInfoSimpo;
    resp.data = builderOut.GetBufferPointer();
    if (resp.data == nullptr) {
        LOG_ERROR << "[NumaMemInfoCollectRecv] Get buffer pointer failed.";
        return MEM_POOLING_ERROR;
    }
    resp.len = builderOut.GetSize();
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    LOG_INFO << "[PidNumaInfoCollect] PidNumaInfoCollect sucess.";
    return MEM_POOLING_OK;
}

void OverCommitMsg::SetResponse(ResponseInfoSimpo &response, const MpResult &retCode, const std::string &msg,
                                UbseByteBuffer &resBuffer)
{
    response.SetResponseInfo(retCode, msg);
    RmrsOutStream builder;
    builder << response;

    resBuffer.data = builder.GetBufferPointer();
    resBuffer.len = builder.GetSize();
    resBuffer.freeFunc = DefaultFreeFunc;
}

MpResult OverCommitMsg::SyncDataToStandByNode(ResponseInfoSimpo& response, const UbseByteBuffer &req,
                                              UbseByteBuffer &resp, const std::string& currentNodeId)
{
    // 查询当前节点是否主节点，如果是，则备节点也要存
    std::string masterNodeId;
    auto ret = ubse::election::UbseGetMasterNodeId(masterNodeId);
    if (ret != 0) {
        LOG_ERROR << "[MpElection][SyncData] UbseGetMasterInfo failed.";
        OverCommitMsg::SetResponse(response, MEM_POOLING_ERROR, "UbseGetMasterInfo failed.", resp);
        return MEM_POOLING_ERROR;
    }
    if (currentNodeId == masterNodeId) {
        UbseRoleInfo standbyRole;
        ret = ubse::election::UbseGetStandbyInfo(standbyRole);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "[MpElection][SyncData] UbseGetStandbyInfo failed.";
            OverCommitMsg::SetResponse(response, MEM_POOLING_ERROR, "UbseGetStandbyInfo failed.", resp);
            return MEM_POOLING_ERROR;
        }
        std::vector<std::string> nodeIdList;
        nodeIdList.push_back(standbyRole.nodeId);
        ret = sync::MpSyncDataHelper::Instance().SyncDataToNode(nodeIdList, message::OPCODE_SYNC_BIND_TYPE_DATA_TO_NODE,
                                                                req);
        if (ret != MEM_POOLING_OK) {
            OverCommitMsg::SetResponse(response, ret, "UpdateBindTypeDB finished.", resp);
            LOG_ERROR << "[MpElection][SyncData] SyncData to standBy failed.";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

bool ValidateSyncBindTypeDataBuffer(const UbseByteBuffer &req)
{
    if (req.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Empty request buffer!";
        return false;
    }
    if (req.data != nullptr && req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Invalid request buffer!";
        return false;
    }

    JSON_MAP jsonMap;
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> jsonMap;
    if (jsonMap.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "The jsonMap is empty.";
        return false;
    }
    return true;
}

uint32_t OverCommitMsg::SyncBindTypeDataRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    LOG_DEBUG << "[MpElection][SyncData] SyncBindTypeDataRecvHandler start.";
    std::string currentNodeId;
    ResponseInfoSimpo response;
    if (!ValidateSyncBindTypeDataBuffer(req)) {
        OverCommitMsg::SetResponse(response, MEM_POOLING_ERROR, "Validate req failed.", resp);
        return MEM_POOLING_ERROR;
    }
    if (ubse::election::UbseGetCurrentNodeId(currentNodeId) != 0) {
        LOG_ERROR << "[MpElection][SyncData] UbseGetCurrentNodeId failed.";
        OverCommitMsg::SetResponse(response, MEM_POOLING_ERROR, "UbseGetCurrentNodeId failed.", resp);
        return MEM_POOLING_ERROR;
    }
    JSON_MAP jsonMap;
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> jsonMap;

    // 更新内存值
    auto& nodeNumaBindMap = OverCommitStorage::Instance().getNodeNumaBindMap();
    for (auto &[k, v] : jsonMap) {
        auto it = STR_TO_BIND_TYPE_MAP.find(v);
        if (it == STR_TO_BIND_TYPE_MAP.end()) {
            LOG_ERROR << "[MpElection][SyncData] Invalid bind type, key=" << k << ", value=" << v << ".";
            continue;
        };
        nodeNumaBindMap[k] = it->second;
        LOG_DEBUG << "[MpElection][SyncData] JsonMap key=" << k << ", value=" << v << ".";
    }
    UbseByteBuffer buffer{};
    RmrsOutStream outBuilder;
    outBuilder << jsonMap;
    size_t size = outBuilder.GetSize();
    buffer.data = outBuilder.GetBufferPointer();
    buffer.len = size;
    LOG_DEBUG << "[MpElection][SyncData] Bind type size=" << buffer.len << ".";
    // 更新数据库
    uint32_t ret = ubse::storage::UbseStoragePutData("over_commit_", "numa_bind_type", &buffer);
    delete[] buffer.data;
    if (ret != 0) {
        LOG_ERROR << "[MpElection][SyncData] UbseStoragePutData failed, nodeId="
                  << currentNodeId << ", ret=" << ret << ".";
        OverCommitMsg::SetResponse(response, MEM_POOLING_ERROR, "UbseStoragePutData failed.", resp);
        return MEM_POOLING_ERROR;
    }
    LOG_INFO << "[MpElection][SyncData] UbseStoragePutData success, nodeId=" << currentNodeId << ".";
    if (OverCommitMsg::SyncDataToStandByNode(response, req, resp, currentNodeId) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    OverCommitMsg::SetResponse(response, ret, "UpdateBindTypeDB finished.", resp);
    LOG_INFO << "[MpElection][SyncData] UpdateBindTypeDB finished.";
    return MEM_POOLING_OK;
}

} // namespace mempooling::over_commit