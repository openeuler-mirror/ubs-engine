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

#ifndef UBSE_SSU_ADAPTER_IMPL_H
#define UBSE_SSU_ADAPTER_IMPL_H

#include <mutex>
#include <string>
#include <vector>

#include "framework/misc/ubse_dl_manager.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_adapter_interface.h"

namespace ubse::adapter_plugins::ssu::def {
using namespace ubse::common::def;
using ubse::utils::UbseDlManager;

constexpr int EID_SIZE = 16;
constexpr int MAX_NAMESPACES_PER_CTRL = 64;
constexpr uint32_t DEV_PATH_SIZE = 32;
constexpr uint32_t SUBNQN_SIZE = 32;
constexpr uint32_t GUID_SIZE = 16;
constexpr uint32_t UUID_SIZE = 16;
constexpr uint32_t USER_DATA_SIZE = 1024;
constexpr uint32_t DEV_NAME_SIZE = 32;
constexpr uint32_t SN_SIZE = 21;
constexpr uint32_t MN_SIZE = 41;

typedef struct {
    uint8_t raw[EID_SIZE];
} DevEidT;

enum class DevStatusT : int {
    DEV_ONLINE = 0,
    DEV_CONNECT_ERROR,
    DEV_DISCOVER_ERROR,
    DEV_IDENTIFY_ERROR,
};

typedef struct {
    DevEidT srcEid;
    DevEidT tgtEid;
    char *devIp;
    bool useUb;
    char subNqn[SUBNQN_SIZE];
    uint32_t jettyId;
} DevAddrT;

typedef struct {
    uint64_t ncap;
    uint64_t nsze;
    uint32_t flbas;
    uint8_t  dps;
    uint32_t anagrpid;
    uint16_t nvmsetid;
    bool nmic;
} NamespaceBaseAttrT;

typedef struct {
    uint32_t namespaceId;
    uint64_t maxLba;
    uint64_t lbas;
    uint64_t totalBytes;
    uint64_t usedBytes;
    char devPath[DEV_PATH_SIZE];
    unsigned char guid[GUID_SIZE];
    unsigned char uuid[UUID_SIZE];
    uint8_t userData[USER_DATA_SIZE];
    DevAddrT devAddr;
    NamespaceBaseAttrT baseAttr;
    DevStatusT state;
} DevNamespaceInfoT;

typedef struct {
    uint64_t tnvmcap;
    uint64_t unvmcap;
    bool sgls;
    uint32_t nsCount;
    uint16_t cntlId;
    char name[DEV_NAME_SIZE];
    char devPath[DEV_PATH_SIZE];
    char sn[SN_SIZE];
    char mn[MN_SIZE];
    DevAddrT devAddr;
    DevNamespaceInfoT namespaces[MAX_NAMESPACES_PER_CTRL];
    DevStatusT state;
} DevInfoT;

class UbseSsuAdapterImpl : public UbseSsuAdapterInterface {
public:
    ~UbseSsuAdapterImpl();

    uint32_t GetDevList(std::vector<UbseSsuDevInfo> &ssuInfoList) override;

    uint32_t CreateDevNameSpace(UbseSsuDevNameSpace &nameSpace) override;

    uint32_t DeleteDevNameSpace(const UbseSsuDevNameSpace &nameSpace) override;

    uint32_t AttachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace) override;

    uint32_t DetachDevNameSpace(const std::string &hostNqn, const UbseSsuDevNameSpace &nameSpace) override;

    uint32_t AddNameSpaceAllowHost(const UbseSsuDevNameSpace &nameSpace,
                                   const std::string &hostNqn) override;

    uint32_t RemoveNameSpaceAllowHost(const UbseSsuDevNameSpace &nameSpace,
                                      const std::string &hostNqn) override;

    uint32_t GetNameSpaceAllowHostList(const UbseSsuDevNameSpace &nameSpace,
                                       std::vector<std::string> &allowHostList) override;

    uint32_t CreateBlockDevice(const std::string &deviceName, const std::vector<std::string> &devicePathList,
                               const UbseCreateBlockDeviceOptions &options, std::string &devicePath) override;

    uint32_t DeleteBlockDevice(const std::string &deviceName) override;

    static UbseSsuAdapterImpl &GetInstance();

    UbseSsuAdapterImpl(const UbseSsuAdapterImpl &other) = delete;
    UbseSsuAdapterImpl(UbseSsuAdapterImpl &&other) = delete;
    UbseSsuAdapterImpl &operator=(const UbseSsuAdapterImpl &other) = delete;
    UbseSsuAdapterImpl &operator=(UbseSsuAdapterImpl &&other) noexcept = delete;

private:
    using AcquireDevInfoFunc = int (*)(const char*, const DevAddrT*, const int, DevInfoT*);
    using CreateNamespaceFunc = int (*)(const char*, DevNamespaceInfoT*);
    using DeleteNamespaceFunc = int (*)(const char*, DevNamespaceInfoT*);
    using AttachNamespaceFunc = int (*)(const char*, DevNamespaceInfoT*);
    using DetachNamespaceFunc = int (*)(const char*, DevNamespaceInfoT*);
    using AddNamespaceAllowHostFunc = int (*)(const char*, DevNamespaceInfoT*, const char*);
    using RemoveNamespaceAllowHostFunc = int (*)(const char*, DevNamespaceInfoT*, const char*);
    using GetNamespaceAllowHostsFunc = int (*)(const char*, DevNamespaceInfoT*, char***, uint32_t*);
    using FreeAllowHostsMemFunc = void (*)(char**, uint32_t);

    UbseSsuAdapterImpl();

    static constexpr auto SSU_PATH = "libssu.so";

    UbseResult DlOpenLib();

    uint32_t BuildDevAddrList(const std::vector<UbseSsuDevInfo>& ssuInfoList, std::vector<DevAddrT>& devList);

    void ConvertDevInfo(const DevInfoT& devInfo, UbseSsuDevInfo& info);

    uint32_t GetSrcEid(DevEidT &srcEid);

    uint32_t BuildNamespaceInfoForCreate(const UbseSsuDevNameSpace& nameSpace, DevNamespaceInfoT& nsInfo);
    uint32_t BuildNamespaceInfoForBasic(const UbseSsuDevNameSpace& nameSpace, DevNamespaceInfoT& nsInfo);

    bool VerifyNamespaceGuid(const UbseSsuDevNameSpace& nameSpace);

    uint32_t ValidatePersistentPaths(const std::vector<std::string>& devicePathList);

    uint32_t CreateLinearBlockDevice(const std::string& deviceName,
                                     const std::vector<std::string>& devicePathList,
                                     std::string& devicePath);

    uint32_t CreateStripedBlockDevice(const std::string& deviceName,
                                      const std::vector<std::string>& devicePathList,
                                      const UbseCreateBlockDeviceOptions& options,
                                      std::string& devicePath);

    UbseDlManager dlManager_;
    std::mutex mutex_;

    AcquireDevInfoFunc acquireDevInfo_{};
    CreateNamespaceFunc createNamespace_{};
    DeleteNamespaceFunc deleteNamespace_{};
    AttachNamespaceFunc attachNamespace_{};
    DetachNamespaceFunc detachNamespace_{};
    AddNamespaceAllowHostFunc addNamespaceAllowHost_{};
    RemoveNamespaceAllowHostFunc removeNamespaceAllowHost_{};
    GetNamespaceAllowHostsFunc getNamespaceAllowHosts_{};
    FreeAllowHostsMemFunc freeAllowHostsMem_{};
};
} // namespace ubse::adapter_plugins::ssu::def

#endif // UBSE_SSU_ADAPTER_IMPL_H
