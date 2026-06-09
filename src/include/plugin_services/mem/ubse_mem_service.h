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

#ifndef UBSE_MEM_SERVICE_H
#define UBSE_MEM_SERVICE_H

#include "ubse_mem_controller.h"
#include "ubse_mem_service_def.h"
#include "ubse_mmi_def.h"
#include "ubse_service_registry.h"
namespace ubse::service::mem {
using namespace ubse::mem::controller;
class UbseMemService : public UbseIService {
public:
    static constexpr const char *kServiceName = "UbseMemService";
    virtual ~UbseMemService() = default;
    std::string GetServiceName() const override
    {
        return kServiceName;
    }

    virtual uint32_t UbseQueryResult(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType) = 0;

    virtual UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string &nodeId,
                                                      std::vector<UbseNumaMemoryDebtInfo> &debtInfos) = 0;

    virtual UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo> &debtInfos) = 0;

    virtual UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(
        std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos) = 0;

    virtual UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList) = 0;

    virtual UbseResult UbseGetNodeNumaInfoByNodeId(const std::string &nodeId,
                                                   std::vector<UbseNodeNumaInfo> &numaNodeInfoList) = 0;

    virtual UbseResult UbseMemNumaCreateWithLender(const std::string &name, const UbseMemBorrower &borrower,
                                                   const std::vector<UbseMemNumaLender> &lenders,
                                                   uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN],
                                                   UbseMemNumaDesc &desc) = 0;

    virtual UbseResult UbseMemNumaCreate(const std::string &name, const UbseMemBorrower &borrower,
                                         const UbseMemNumaCreateOpt &opt, UbseMemNumaDesc &desc) = 0;

    virtual UbseResult UbseMemNumaCreateWithCandidate(const std::string &name, const UbseMemBorrower &borrower,
                                                      const UbseMemNumaCandidateOpt &opt, UbseMemNumaDesc &desc) = 0;

    virtual UbseResult UbseMemNumaDelete(const std::string &name, const UbseMemBorrower &borrower) = 0;

    virtual UbseResult UbseMemAddrCreate(const std::string &name, const UbseMemAddrCreateOpt &opt,
                                         UbseMemAddrDesc &desc) = 0;

    virtual UbseResult UbseMemAddrDelete(const std::string &name, const UbseMemBorrower &borrower) = 0;

    virtual UbseResult UbseMemDebtCircleCheck(const std::string &srcNodeId, const std::string &dstNodeId,
                                              bool &isCircle) = 0;

    virtual UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string &nodeId,
                                                      std::vector<UbseMemAddrDesc> &debtInfos) = 0;

    virtual uint32_t UbseGetMemDebtInfoFromMaster(const std::string &nodeId,
                                                  adapter_plugins::mmi::NodeMemDebtInfoMap &memDebtInfoMap) = 0;
    virtual adapter_plugins::mmi::NodeMemDebtInfoMap UbseGetLocalMemDebtInfo() = 0;

    virtual std::string GetAllNumaJsonInfo(const std::string &nodeId) = 0;

    virtual uint32_t UbseAllNumaInfo(std::vector<ubse::service::mem::UbseNumaNodeInfo> &numaNodeInfoList) = 0;

    virtual UbseResult MemReportWhenExportNodeOnFault(int faultType, std::string &faultId) = 0;

    virtual UbseResult GetChipAndDieId(uint32_t socketId, std::pair<uint32_t, uint32_t> &chipDiePair) = 0;

    virtual uint32_t PreImportDecoderEntry(const ubse::service::mem::PreImportDecoderParam &importDecoderParam,
                                           ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult &outValue) = 0;

    virtual bool IsNeedPreOnline(const ubse::service::mem::DecoderEntryLoc &loc, uint32_t dcna,
                                 ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult &outValue) = 0;

    virtual void RollbackPreImportHandle(const ubse::service::mem::DecoderEntryLoc &loc) = 0;

    virtual UbseResult InitPreHandle(
        const std::vector<ubse::service::mem::BasicPreImportInfo> &preImportInfos) = 0;
};
inline std::shared_ptr<UbseMemService> GetMemService()
{
    auto weakPtr = UbseServiceRegistry::GetInstance().GetService<UbseMemService>(UbseMemService::kServiceName);
    return weakPtr.lock();
}
} // namespace ubse::service::mem

#endif
