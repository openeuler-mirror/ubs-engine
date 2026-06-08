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
#ifndef UBSE_MEM_SERVICE_IMPL_H
#define UBSE_MEM_SERVICE_IMPL_H

#include "plugin_services/mem/ubse_mem_service.h"

namespace ubse::service::mem {

class UbseMemServiceImpl : public UbseMemService {
public:
    uint32_t UbseQueryResult(const std::string &name, UbseMemResult &result,
                             UbseMemBorrowType borrowType = UbseMemBorrowType::NUMA_BORROW) override;

    UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string &nodeId,
                                              std::vector<UbseNumaMemoryDebtInfo> &debtInfos) override;

    UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo> &debtInfos) override;

    UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos) override;

    UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList) override;

    UbseResult UbseGetNodeNumaInfoByNodeId(const std::string &nodeId,
                                           std::vector<UbseNodeNumaInfo> &numaNodeInfoList) override;

    UbseResult UbseMemNumaCreateWithLender(const std::string &name, const UbseMemBorrower &borrower,
                                           const std::vector<UbseMemNumaLender> &lenders,
                                           uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN],
                                           UbseMemNumaDesc &desc) override;

    UbseResult UbseMemNumaCreate(const std::string &name, const UbseMemBorrower &borrower,
                                 const UbseMemNumaCreateOpt &opt, UbseMemNumaDesc &desc) override;

    UbseResult UbseMemNumaCreateWithCandidate(const std::string &name, const UbseMemBorrower &borrower,
                                              const UbseMemNumaCandidateOpt &opt, UbseMemNumaDesc &desc) override;

    UbseResult UbseMemNumaDelete(const std::string &name, const UbseMemBorrower &borrower) override;

    UbseResult UbseMemAddrCreate(const std::string &name, const UbseMemBorrower &borrower,
                                 const UbseMemProcessLender &lender, uint32_t flag, uint8_t exportAccessMode,
                                 UbseMemAddrDesc &desc) override;

    UbseResult UbseMemAddrDelete(const std::string &name, const UbseMemBorrower &borrower) override;

    UbseResult UbseMemDebtCircleCheck(const std::string &srcNodeId, const std::string &dstNodeId,
                                      bool &isCircle) override;

    UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string &nodeId,
                                              std::vector<UbseMemAddrDesc> &debtInfos) override;

    uint32_t UbseGetMemDebtInfoFromMaster(const std::string &nodeId,
                                          adapter_plugins::mmi::NodeMemDebtInfoMap &memDebtInfoMap) override;
    adapter_plugins::mmi::NodeMemDebtInfoMap UbseGetLocalMemDebtInfo() override;

    std::string GetAllNumaJsonInfo(const std::string &nodeId) override;

    uint32_t UbseAllNumaInfo(std::vector<UbseNumaNodeInfo> &numaNodeInfoList) override;
    UbseResult MemReportWhenExportNodeOnFault(int faultType, std::string &faultId) override;

    UbseResult GetChipAndDieId(uint32_t socketId, std::pair<uint32_t, uint32_t> &chipDiePair) override;

    uint32_t PreImportDecoderEntry(const ubse::mem::decoder::utils::PreImportDecoderParam &importDecoderParam,
                                   ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult &outValue) override;

    bool IsNeedPreOnline(const ubse::mem::decoder::utils::DecoderEntryLoc &loc, uint32_t dcna,
                         ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult &outValue) override;

    void RollbackPreImportHandle(const ubse::mem::decoder::utils::DecoderEntryLoc &loc) override;

    UbseResult InitPreHandle(const std::vector<ubse::mem::decoder::utils::BasicPreImportInfo> &preImportInfos) override;

    ~UbseMemServiceImpl() override = default;
};

} // namespace ubse::service::mem

#endif
