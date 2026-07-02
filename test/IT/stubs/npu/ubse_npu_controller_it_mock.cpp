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

#include <sys/wait.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mti_1825.h"
#include "ubse_mti_bus_instance.h"
#include "ubse_mti_urma.h"
#include "ubse_npu_monitor_service_api.h"
#include "ubse_os_util.h"

using namespace ubse::common::def;
using namespace ubse::mti::bus_instance;
using namespace ubse::mti::urma;
using namespace ubse::mti::_1825;

static UbseMtiGuid MakeGuid(uint8_t tag)
{
    UbseMtiGuid g{};
    g[15] = tag;
    return g;
}

class UbseMtiUrmaStub : public UbseMtiUrma {
public:
    UbseResult GetIdevFeList(std::vector<UbseMtiIdevPfe>& feList) override
    {
        UbseMtiIdevVfe vfe;
        vfe.ubController = UbseMtiUbController(1, 0);
        vfe.pfeId = 0;
        vfe.vfeId = 0;
        vfe.guid = MakeGuid(0x02);

        UbseMtiIdevPfe pfe;
        pfe.ubController = UbseMtiUbController(1, 0);
        pfe.pfeId = 0;
        pfe.guid = MakeGuid(0x01);
        pfe.vfeList = {vfe};

        feList = {pfe};
        return UBSE_OK;
    }

    UbseResult GetIdevFeDavidMapping(UbseMtiIdevFeDavidMapping& mapping) override
    {
        UbseMtiDavid david;
        david.slotId = 1;
        david.chipId = 1;

        UbseMtiIdevPfe pfe;
        pfe.ubController = UbseMtiUbController(1, 0);
        pfe.pfeId = 0;
        pfe.guid = MakeGuid(0x01);

        mapping[david] = pfe;
        return UBSE_OK;
    }

    UbseResult BindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                         std::vector<bool>& resList) override
    {
        resList.assign(vfeDavidList.size(), true);
        return UBSE_OK;
    }

    UbseResult UnBindDavid(uint16_t upi, const std::vector<UbseMtiIdevVfeDavidPair>& vfeDavidList,
                           std::vector<bool>& resList) override
    {
        resList.assign(vfeDavidList.size(), true);
        return UBSE_OK;
    }

    UbseResult RegDavidFeToVmBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMtiIdevVfe>& vfeList,
                                         std::vector<bool>& resList) override
    {
        resList.assign(vfeList.size(), true);
        return UBSE_OK;
    }

    UbseResult UnRegDavidFeFromVmBusInstance(const UbseMtiBusInst& busInstance,
                                             const std::vector<UbseMtiIdevVfe>& vfeList,
                                             std::vector<bool>& resList) override
    {
        resList.assign(vfeList.size(), true);
        return UBSE_OK;
    }
};

class UbseMti1825Stub : public UbseMti1825 {
public:
    UbseResult Get1825FeList(std::vector<UbseMti1825Pf>& pfList) override
    {
        UbseMti1825Pf pf;
        pf.slotId = 1;
        pf.chipId = 11;
        pf.dieId = 0;
        pf.pfId = 3;
        pf.affinityUbController = UbseMtiUbController();
        pf.guid = MakeGuid(0x03);
        pf.vfList = {};

        pfList = {pf};
        return UBSE_OK;
    }

    UbseResult Reg1825FeToHostBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMti1825Vf>& vfList,
                                          std::vector<bool>& resList) override
    {
        resList.assign(vfList.size(), true);
        return UBSE_OK;
    }

    UbseResult UnReg1825FeFromHostBusInstance(const UbseMtiBusInst& busInstance,
                                              const std::vector<UbseMti1825Vf>& vfList,
                                              std::vector<bool>& resList) override
    {
        resList.assign(vfList.size(), true);
        return UBSE_OK;
    }

    UbseResult Reg1825FeToVmBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMti1825Vf>& vfList,
                                        std::vector<bool>& resList) override
    {
        resList.assign(vfList.size(), true);
        return UBSE_OK;
    }

    UbseResult UnReg1825FeFromVmBusInstance(const UbseMtiBusInst& busInstance, const std::vector<UbseMti1825Vf>& vfList,
                                            std::vector<bool>& resList) override
    {
        resList.assign(vfList.size(), true);
        return UBSE_OK;
    }
};

class UbseMtiBusInstanceStub : public UbseMtiBusInstance {
public:
    UbseResult GetBusInstanceList(std::vector<UbseMtiBusInst>& busInstanceList) override
    {
        UbseMtiBusInst hostInst;
        hostInst.type = UbseMtiBusInstanceType::HOST;
        hostInst.upi = 100;
        hostInst.vendor = 0;
        hostInst.guid = MakeGuid(0x05);
        hostInst.eid = UbseMtiEid{};
        hostInst.subDeviceGuids = {};

        busInstanceList = {hostInst};
        return UBSE_OK;
    }

    UbseResult CreateVmBusInstance(uint16_t upi, UbseMtiBusInst& busInstance) override
    {
        busInstance.type = UbseMtiBusInstanceType::VM;
        busInstance.upi = upi;
        busInstance.vendor = 0;
        busInstance.guid = MakeGuid(0xAA);
        busInstance.eid = UbseMtiEid{};
        busInstance.subDeviceGuids = {};
        return UBSE_OK;
    }

    UbseResult DestroyVmBusInstance(const UbseMtiBusInst& busInstance) override
    {
        return UBSE_OK;
    }

    UbseResult GetD2hMemory(const UbseMtiBusInst& busInstance, uint32_t& tid, uint64_t& uba, uint64_t& size) override
    {
        tid = 1;
        uba = 0x1000;
        size = 0x2000;
        return UBSE_OK;
    }
};

namespace ubse::mti::urma {

UbseMtiUrma& UbseMtiUrma::GetInstance()
{
    static UbseMtiUrmaStub instance;
    return instance;
}

} // namespace ubse::mti::urma

namespace ubse::mti::_1825 {

UbseMti1825& UbseMti1825::GetInstance()
{
    static UbseMti1825Stub instance;
    return instance;
}

} // namespace ubse::mti::_1825

namespace ubse::mti::bus_instance {

UbseMtiBusInstance& UbseMtiBusInstance::GetInstance()
{
    static UbseMtiBusInstanceStub instance;
    return instance;
}

} // namespace ubse::mti::bus_instance

namespace ubse::npu::vm_monitor {

UbseResult StartVMMonitor()
{
    return UBSE_OK;
}

UbseResult ResetNpu(const uint8_t& chipId)
{
    return UBSE_OK;
}

} // namespace ubse::npu::vm_monitor

namespace ubse::utils {

UbseResult UbseOsUtil::Exec(const std::string& cmd, std::string& res)
{
    if (cmd.find("ipmitool") != std::string::npos) {
        res = "00 00 00 00 00 00 00 00 00 00 ff";
        return UBSE_OK;
    }
    res.clear();
    std::array<char, 128> buffer{};
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return UBSE_ERROR;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        res += buffer.data();
    }
    int status = pclose(pipe.release());
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::utils
