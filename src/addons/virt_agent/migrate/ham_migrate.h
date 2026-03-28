/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef HAM_MIGRATE_H
#define HAM_MIGRATE_H

#include "libvirt_handler.h"

namespace vm {
    class HamMigrate {
    public:
        static VmResult Start();
        static void ClearQueueOperation();
        static VmResult Run();
        static VmResult Stop();
        static void LoadData();
        static void EnterClearQueue(HamMigrateVmInfo& hamMigrateVmInfo, const bool& isUpdate = false);
        static void EnterClearQueue(std::vector<HamMigrateVmInfo>& hamMigrateVmInfos, const bool& isUpdate = false);
        static void HamMigrateCancel(const UbseByteBuffer &req, UbseByteBuffer &resp);
        static void MasterDstInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
        static void MasterDstInfoReplyHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
        static void SrcNodeInfoReplyHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
        static void AgentDstInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
        static VmResult PidIsVm(const uint64_t pid);
        static void LendMemInDstNode(UbseByteBuffer &req, const HamMigrateDstInfo& hamMigrateVmInfo);
        static std::string GetMasterNodeId();
        static void HamMigrateCancelReply(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
        static VmResult PanicEventHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);
        static VmResult HandleHamMigrateBorrow(const Document& msgJson, RespInfo& respInfo,
                                               UbseIpcMessage& resp, const UbseRequestContext& context);
        static VmResult HandleHamMigrateClear(const Document& msgJson, RespInfo& respInfo,
                                              UbseIpcMessage& resp, const UbseRequestContext& context);

    private:
        static VmResult ConvertToBorrow(const Value &msgJson, BorrowInfo& borrowInfo);
        static VmResult ConvertToClear(const Value &msgJson, ClearInfo& clearInfo);
        static void HandleBorrowFailure(HamMigrateVmInfo &hamMigrateVmInfo);
        static VmResult MigrateAndTracking(const HostVmDomainInfo &hostVmDomainInfo, BorrowInfo &borrowInfo,
                                           BorrowResponse &borrowResponse);
        static VmResult Borrow(BorrowInfo& borrowInfo, BorrowResponse& borrowResponse);
        static VmResult Clear(const ClearInfo& clearInfo);
        static bool HasTask(const BorrowInfo &borrowInfo);
        static bool IsMigrating(const HamMigrateVmInfo &hamMigrateVmInfo);
        static void UpdateHamMigrateVmInfo(const BorrowInfo& borrowInfo, const VmDomainInfo& vmDomainInfo,
            HamMigrateVmInfo &hamMigrateVmInfo);
        static std::string GetPrefixLog(const HamMigrateVmInfo& hamMigrateVmInfo);
        static VmResult DoProcessMigrate(HamMigrateVmInfo& hamMigrateVmInfo);
        static VmResult DoProcessTracking(HamMigrateVmInfo& hamMigrateVmInfo);
        static VmResult DoBorrowAddress(BorrowInfo& borrowInfo, HamMigrateVmInfo& hamMigrateVmInfo,
                                        BorrowResponse& borrowResponse);
        static VmResult DoUbseBorrowAddress(const BorrowInfo &borrowInfo, BorrowResponse &borrowResponse);
        static VmResult Rollback(HamMigrateVmInfo& hamMigrateVmInfo);
        static VmResult RollbackBorrowAddress(HamMigrateVmInfo& hamMigrateVmInfo);
        static VmResult UbseRollbackBorrowAddress(const HamMigrateVmInfo &hamMigrateVmInfo);
        static VmResult RollbackProcessTracking(HamMigrateVmInfo& hamMigrateVmInfo);
        static VmResult RollbackProcessMigrate(HamMigrateVmInfo& hamMigrateVmInfo);
        static void ReSetReTry(HamMigrateVmInfo& hamMigrateVmInfo);
        static void UpdateDstNodeState(HamMigrateVmInfo &hamMigrateVmInfo);
        static DynamicPriorityQueue<HamMigrateVmInfo> clearQueue;
        static std::mutex clearMutex;
        static std::condition_variable clearCv;
        static std::atomic<bool> exitFlag;
        static std::atomic_bool runFlag;

        static VmResult GetLocalNumaInfoFromNumaMemInfo(const MemNumaInfo &numaMemInfo, int &numaId, int &socketId);
    };
} // namespace vm

#endif // HAM_MIGRATE_H
