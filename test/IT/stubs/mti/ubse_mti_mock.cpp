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

#include "ubse_error.h"
#include "ubse_mti_interface_default.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"

namespace ubse::adapter_plugins::mti {

class UbseMtiInterfaceMock : public UbseMtiInterface {
public:
    // Non-ETS methods: delegate to UbseMtiInterfaceDefault
    // (which calls UbseLcneModule → HTTP → MockLcneServer)

    common::def::UbseResult GetLocalNodeInfo(UbseMtiNodeInfo& nodeInfo) override
    {
        return defaultImpl_.GetLocalNodeInfo(nodeInfo);
    }

    common::def::UbseResult GetCurNodeTopo(UbseDevTopology& topo) override
    {
        return defaultImpl_.GetCurNodeTopo(topo);
    }

    common::def::UbseResult GetClusterNodeInfoList(std::vector<UbseMtiNodeInfo>& nodeInfoList) override
    {
        return defaultImpl_.GetClusterNodeInfoList(nodeInfoList);
    }

    common::def::UbseResult GetClusterCpuTopo(UbseMtiCpuTopoInfoMap& topo) override
    {
        return defaultImpl_.GetClusterCpuTopo(topo);
    }

    common::def::UbseResult GetLocalIp(std::string& localIp) override
    {
        const char* envIp = std::getenv("UBSE_IT_NODE_IP");
        if (envIp != nullptr && envIp[0] != '\0') {
            localIp = envIp;
            return UBSE_OK;
        }
        return defaultImpl_.GetLocalIp(localIp);
    }

    common::def::UbseResult GetClusterIpList(std::vector<std::string>& ipList) override
    {
        const char* envIps = std::getenv("UBSE_IT_CLUSTER_IPS");
        if (envIps != nullptr && envIps[0] != '\0') {
            ipList.clear();
            std::string ips(envIps);
            size_t start = 0;
            while (start <= ips.size()) {
                size_t end = ips.find(',', start);
                std::string ip = ips.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!ip.empty()) {
                    ipList.push_back(ip);
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            return UBSE_OK;
        }
        return defaultImpl_.GetClusterIpList(ipList);
    }

    common::def::UbseResult AddDecoderEntry(const mami::UbseMamiMemImportInfo& importInfo,
                                            mami::UbseMamiMemImportResult& importResult,
                                            const UbseDecoderTrustRingData& trustRingData) override
    {
        return defaultImpl_.AddDecoderEntry(importInfo, importResult, trustRingData);
    }

    common::def::UbseResult DeleteDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) override
    {
        return defaultImpl_.DeleteDecoderEntry(drawInfo);
    }

    common::def::UbseResult InvalidateDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) override
    {
        return defaultImpl_.InvalidateDecoderEntry(drawInfo);
    }

    common::def::UbseResult GetAllMemHandles(const mami::UbseMamiMemHandleQueryInfo& queryInfo,
                                             std::vector<mami::UbseMamiMemHandleValue>& handleValues) override
    {
        return defaultImpl_.GetAllMemHandles(queryInfo, handleValues);
    }

    common::def::UbseResult GetMtiComEid(std::map<UbseMtiIouInfo, UbseMtiEidGroup>& comUrmaInfoMap) override
    {
        return defaultImpl_.GetMtiComEid(comUrmaInfoMap);
    }

    common::def::UbseResult UbseGetFeEid(UbseMtiIouInfo iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos) override
    {
        return defaultImpl_.UbseGetFeEid(iouInfo, allFeInfos);
    }

    // ETS methods: no-op stubs (ETS is bypassed in IT)

    common::def::UbseResult UbseCreateEtsProfile(const UbseMtiEtsProfile&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseAddEtsVlsToProfile(const std::string&, const std::vector<UbseEtsVl>&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseAddEtsPriorityGroupsToProfile(const std::string&,
                                                              const std::vector<UbseEtsPriorityGroup>&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseAddEtsVlsAndPriorityGroupsToProfile(const std::string&, const std::vector<UbseEtsVl>&,
                                                                    const std::vector<UbseEtsPriorityGroup>&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseDeleteEtsProfile(const std::string&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseRemoveEtsVlsFromProfile(const std::string&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseRemoveEtsPriorityGroupsFromProfile(const std::string&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseQueryEtsProfile(const std::string&, UbseMtiEtsProfile&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseQueryAllEtsProfiles(std::vector<UbseMtiEtsProfile>&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseApplyEtsProfileToInterface(const std::string&, const std::string&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseRemoveEtsProfileFromInterface(const std::string&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseQueryAllInterfaceEtsProfile(std::vector<UbseMtiInterfaceEtsApplication>&) override
    {
        return UBSE_OK;
    }
    common::def::UbseResult UbseQueryInterfaceEtsProfile(const std::string&, std::string&) override
    {
        return UBSE_OK;
    }

private:
    UbseMtiInterfaceDefault defaultImpl_;
};

UbseMtiInterface& UbseMtiInterface::GetInstance()
{
    static UbseMtiInterfaceMock instance;
    return instance;
}

} // namespace ubse::adapter_plugins::mti