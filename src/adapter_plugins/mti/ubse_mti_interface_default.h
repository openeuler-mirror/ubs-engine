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

#ifndef UBSE_MTI_INTERFACE_DEFAULT_H
#define UBSE_MTI_INTERFACE_DEFAULT_H
#include "adapter_plugins/mti/ubse_mti_interface.h"

namespace ubse::adapter_plugins::mti {

class UbseMtiInterfaceDefault : public UbseMtiInterface {
public:
    common::def::UbseResult GetLocalNodeInfo(UbseMtiNodeInfo& nodeId) override;

    common::def::UbseResult GetClusterNodeInfoList(std::vector<UbseMtiNodeInfo>& nodeIdList) override;

    common::def::UbseResult GetClusterCpuTopo(UbseMtiCpuTopoInfoMap& topo) override;

    common::def::UbseResult GetLocalIp(std::string& localIp) override;

    common::def::UbseResult GetClusterIpList(std::vector<std::string>& ipList) override;

    common::def::UbseResult AddDecoderEntry(const mami::UbseMamiMemImportInfo& importInfo,
                                            mami::UbseMamiMemImportResult& importResult,
                                            const ubse::adapter_plugins::mti::UbseDecoderTrustRingData &trustRingData = {}) override;

    common::def::UbseResult DeleteDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) override;

    common::def::UbseResult GetAllMemHandles(const mami::UbseMamiMemHandleQueryInfo& queryInfo,
                                             std::vector<mami::UbseMamiMemHandleValue>& handleValues) override;
    common::def::UbseResult GetAllSocketComEid(std::map<UbseDevName, UbseMtiEidGroup> &socketInfoMap) override;

    common::def::UbseResult UbseGetFeEid(UbseMtiIouInfo iouInfo, std::vector<UbseMtiFeInfo> &allFeInfos) override;

    common::def::UbseResult UbseCreateQosProfile(UbseMtiQosProfile ubseLcneQosProfile) override;

    common::def::UbseResult UbseDeleteQosProfile(std::string proflieName) override;

    common::def::UbseResult UbseQueryQosProfile(std::string proflieName,
                                                UbseMtiQosProfile &ubseLcneQosProfile) override;

    common::def::UbseResult UbseApplyVfeQos(UbseMtiFeInfo ubseFeInfo, std::string proflieName) override;

    common::def::UbseResult UbseDeleteVfeQos(UbseMtiFeInfo ubseFeInfo) override;

    common::def::UbseResult UbseQueryVfeQos(UbseMtiFeInfo ubseFeInfo, std::string &proflieName) override;

    common::def::UbseResult UbseCreateEtsProfile(const UbseMtiEtsProfile &etsProfile) override;

    common::def::UbseResult UbseAddEtsVlsToProfile(const std::string &profileName,
                                                 const std::vector<UbseEtsVl> &vls) override;

    common::def::UbseResult UbseAddEtsPriorityGroupsToProfile(
        const std::string &profileName, const std::vector<UbseEtsPriorityGroup> &priorityGroups) override;

    common::def::UbseResult UbseAddEtsVlsAndPriorityGroupsToProfile(
        const std::string &profileName, const std::vector<UbseEtsVl> &vls,
        const std::vector<UbseEtsPriorityGroup> &priorityGroups) override;

    common::def::UbseResult UbseDeleteEtsProfile(const std::string &profileName) override;

    common::def::UbseResult UbseRemoveEtsVlsFromProfile(const std::string &profileName) override;

    common::def::UbseResult UbseRemoveEtsPriorityGroupsFromProfile(const std::string &profileName) override;

    common::def::UbseResult UbseQueryEtsProfile(const std::string &profileName, UbseMtiEtsProfile &etsProfile) override;

    common::def::UbseResult UbseQueryAllEtsProfiles(std::vector<UbseMtiEtsProfile> &etsProfiles) override;

    common::def::UbseResult UbseApplyEtsProfileToInterface(const std::string &interfaceName,
                                                           const std::string &profileName) override;

    common::def::UbseResult UbseRemoveEtsProfileFromInterface(const std::string &interfaceName) override;

    common::def::UbseResult UbseQueryAllInterfaceEtsProfile(
        std::vector<UbseMtiInterfaceEtsApplication> &applications) override;

    common::def::UbseResult UbseQueryInterfaceEtsProfile(const std::string &interfaceName,
                                                         std::string &profileName) override;
};
} // namespace ubse::adapter_plugins::mti

#endif // UBSE_MTI_INTERFACE_DEFAULT_H
