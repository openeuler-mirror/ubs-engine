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

#ifndef OVER_COMMIT_MSG_HANDLER_H
#define OVER_COMMIT_MSG_HANDLER_H

#include "ubse_def.h"
#include "mempooling_message.h"
#include "mp_module.h"
#include "over_commit_election_handler.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_msg.h"
#include "response_info_simpo.h"

namespace mempooling::over_commit {

class OverCommitMsgHandler {
public:
    static MpResult MigrateLocalHandler(const outinterface::SrcMemoryBorrowParam& srcMemoryBorrowParam,
                                        const std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs,
                                        const std::vector<MemMigrateResult>& memMigrateResults);

    /**
     * 接收Smap::SetSmapRemoteNuma消息并处理
     * @param req
     * @param resp
     * @return
     */
    static MpResult SetSmapRemoteNumaHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    /**
     * 接收Smap::SetSmapRemoteNuma消息并处理，本节点处理
     */
    static MpResult SetSmapRemoteNumaLocalHandler(const outinterface::SrcMemoryBorrowParam& srcParam,
                                                  const std::vector<MemBorrowInfo>& memBorrowInfos);

    /**
     * 接收Smap::Remove消息并处理
     * @param req
     * @param resp
     * @return
     */
    static MpResult RemoveHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    /**
     * 接收Smap::Remove消息并处理,本节点处理
     */
    static MpResult RemoveLocalHandler(const uint16_t presentNumaId, const std::vector<pid_t>& pids);

    /**
     * 接收Smap::SmapQueryProcessConfig消息并处理
     * @param req
     * @param resp
     * @return
     */
    static MpResult ProcessQueryHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    /**
     * 接收Smap::SmapQueryProcessConfig消息并处理,本节点处理
     */
    static MpResult ProcessQueryLocalHandler(const std::vector<uint32_t>& numaIds, std::string& numa2pidMapJson,
                                             bool isReturn = false);

private:
    static void SetResponse(ResponseInfoSimpo& response, const MpResult& retCode, const std::string& msg,
                            UbseByteBuffer& resBuffer);
    static MpResult NormMigrate(const std::vector<MemMigrateResult>& memMigrateResults,
                                const std::vector<MemBorrowInfoWithSrc>& memBorrowInfoWithSrcs, int16_t srcNumaId);
};

uint32_t NumaBindTypeNotify(UbseByteBuffer& buffer);
uint32_t NumaBindTypeGetData(UbseByteBuffer& data);

uint32_t InitOverCommitReg();
uint32_t InitUCacheOverCommitReg();
uint32_t InitExportReg();

void UCacheSendMigrationStrategyRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void UCacheStopMigrationRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void UCacheSendMigrationStrategyResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void UCacheStopMigrationResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
void UpdateUCacheRatioRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void UpdateUCacheRatioResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
uint32_t PidNumaInfoCollectRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
void PidNumaInfoCollectRpcResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode);
uint32_t PidNumaInfoCollectHandler(const turbo::rmrs::PidNumaInfoCollectParam& pidNumaInfoCollectParam,
                                   turbo::rmrs::PidNumaInfoCollectResult& pidNumaInfoCollectResult);

class MpOverCommitSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        auto ret = OverCommitSubscribeSwitchover();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        ret = InitOverCommitReg();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        ret = InitUCacheOverCommitReg();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        ret = InitExportReg();
        if (ret != MEM_POOLING_OK) {
            return MEM_POOLING_ERROR;
        }
        ubse::com::UbseComEndpoint endpoint_sync_bindtype = {.moduleId = MP_MODULE_CODE,
                                                             .serviceId = OPCODE_SYNC_BIND_TYPE_DATA_TO_NODE};
        ret = UbseRegRpcService(endpoint_sync_bindtype, OverCommitMsg::SyncBindTypeDataRecvHandler);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "SyncBindTypeDataRecvHandler reg failed res: " << ret << ".";
            return MEM_POOLING_ERROR;
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        return;
    }
    const std::string Name() override
    {
        return "OverCommit";
    }
};

} // namespace mempooling::over_commit

#endif // OVER_COMMIT_MSG_HANDLER_H