#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/writer.h"
#include "mock_ssu_config_path.h"
#include "ubse_ssu_def.h"

using namespace ubse::adapter_plugins::ssu::def;

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
    uint8_t dps;
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

namespace {

std::mutex g_mutex;
uint16_t g_nextCntlId = 1;

struct MockNamespace {
    DevNamespaceInfoT info{};
    std::map<std::string, int> allowedNqns;
};

struct MockDevice {
    uint16_t cntlId = 0;
    uint32_t nextNamespaceId = 1;
    uint64_t tnvmcap = 0;
    uint64_t unvmcap = 0;
    char name[DEV_NAME_SIZE] = {};
    char devPath[DEV_PATH_SIZE] = {};
    char sn[SN_SIZE] = {};
    char mn[MN_SIZE] = {};
    DevAddrT devAddr = {};
    DevStatusT state = DevStatusT::DEV_ONLINE;
    std::vector<MockNamespace> namespaces;
};

std::map<std::string, MockDevice> g_devices;

std::string EidToKey(const DevEidT &eid)
{
    return std::string(reinterpret_cast<const char *>(eid.raw), EID_SIZE);
}

bool ShouldFail(const char *op)
{
    const char *env = std::getenv("MOCK_SSU_FAIL");
    if (env == nullptr) {
        return false;
    }
    std::string failList(env);
    return failList.find(op) != std::string::npos;
}

bool HexStringToBytes(const std::string &hex, unsigned char *buf, size_t bufSize)
{
    std::memset(buf, 0, bufSize);

    if (hex.empty()) {
        return false;
    }

    if (hex.size() % 2 != 0) {
        fprintf(stderr, "[WARN] HexStringToBytes: odd length (%zu), aborting\n", hex.size());
        return false;
    }

    for (char c : hex) {
        if (!isxdigit(static_cast<unsigned char>(c))) {
            fprintf(stderr, "[WARN] HexStringToBytes: invalid hex char '%c' (0x%02x)\n", c, static_cast<unsigned char>(c));
            return false;
        }
    }

    size_t len = std::min(hex.size() / 2, bufSize);
    for (size_t i = 0; i < len; ++i) {
        unsigned int val = 0;
        sscanf(hex.c_str() + i * 2, "%02x", &val);
        buf[i] = static_cast<unsigned char>(val);
    }

    return true;
}

bool LoadUserDataFromJson(const rapidjson::Value &userDataJson, UbseSsuDevNameSpaceCustomData &customData)
{
    memset(&customData, 0, sizeof(customData));

    if (userDataJson.HasMember("version") && userDataJson["version"].IsNumber()) {
        customData.version = static_cast<uint8_t>(userDataJson["version"].GetInt());
    }

    if (userDataJson.HasMember("name") && userDataJson["name"].IsString()) {
        const char *name = userDataJson["name"].GetString();
        size_t len = std::min(strlen(name), sizeof(customData.name) - 1);
        memcpy(customData.name, name, len);
        customData.name[len] = '\0';
    }

    if (userDataJson.HasMember("defaultNqn") && userDataJson["defaultNqn"].IsString()) {
        const char *defaultNqn = userDataJson["defaultNqn"].GetString();
        size_t len = std::min(strlen(defaultNqn), sizeof(customData.defaultNqn) - 1);
        memcpy(customData.defaultNqn, defaultNqn, len);
        customData.defaultNqn[len] = '\0';
    }

    if (userDataJson.HasMember("uid") && userDataJson["uid"].IsNumber()) {
        customData.uid = static_cast<uint32_t>(userDataJson["uid"].GetInt());
    }

    if (userDataJson.HasMember("userName") && userDataJson["userName"].IsString()) {
        const char *userName = userDataJson["userName"].GetString();
        size_t len = std::min(strlen(userName), sizeof(customData.userName) - 1);
        memcpy(customData.userName, userName, len);
        customData.userName[len] = '\0';
    }

    if (userDataJson.HasMember("tenant") && userDataJson["tenant"].IsString()) {
        const char *tenant = userDataJson["tenant"].GetString();
        size_t len = std::min(strlen(tenant), sizeof(customData.tenant) - 1);
        memcpy(customData.tenant, tenant, len);
        customData.tenant[len] = '\0';
    }

    if (userDataJson.HasMember("allocStrategy") && userDataJson["allocStrategy"].IsNumber()) {
        customData.allocStrategy = static_cast<uint8_t>(userDataJson["allocStrategy"].GetInt());
    }

    if (userDataJson.HasMember("raidLevel") && userDataJson["raidLevel"].IsNumber()) {
        customData.raidLevel = static_cast<uint8_t>(userDataJson["raidLevel"].GetInt());
    }

    if (userDataJson.HasMember("nsNum") && userDataJson["nsNum"].IsNumber()) {
        customData.nsNum = static_cast<uint8_t>(userDataJson["nsNum"].GetInt());
    }

    if (userDataJson.HasMember("totalBytes") && userDataJson["totalBytes"].IsNumber()) {
        customData.totalBytes = static_cast<uint64_t>(userDataJson["totalBytes"].GetInt64());
    }

    if (userDataJson.HasMember("crc") && userDataJson["crc"].IsNumber()) {
        customData.crc = static_cast<uint32_t>(userDataJson["crc"].GetInt());
    }

    return true;
}

bool LoadNamespaceFromJson(const rapidjson::Value &nsJson, MockDevice &dev, MockNamespace &ns)
{
    ns.info.state = DevStatusT::DEV_ONLINE;

    if (nsJson.HasMember("namespaceId") && nsJson["namespaceId"].IsNumber()) {
        ns.info.namespaceId = static_cast<uint32_t>(nsJson["namespaceId"].GetInt());
        dev.nextNamespaceId = std::max(dev.nextNamespaceId, static_cast<uint32_t>(ns.info.namespaceId + 1));
    } else {
        ns.info.namespaceId = dev.nextNamespaceId++;
    }

    if (nsJson.HasMember("nsze") && nsJson["nsze"].IsNumber()) {
        ns.info.baseAttr.nsze = static_cast<uint64_t>(nsJson["nsze"].GetInt64());
    }

    if (nsJson.HasMember("ncap") && nsJson["ncap"].IsNumber()) {
        ns.info.baseAttr.ncap = static_cast<uint64_t>(nsJson["ncap"].GetInt64());
    }

    if (nsJson.HasMember("flbas") && nsJson["flbas"].IsNumber()) {
        ns.info.baseAttr.flbas = static_cast<uint32_t>(nsJson["flbas"].GetInt());
    }

    if (nsJson.HasMember("dps") && nsJson["dps"].IsNumber()) {
        ns.info.baseAttr.dps = static_cast<uint8_t>(nsJson["dps"].GetInt());
    }

    if (nsJson.HasMember("anagrpid") && nsJson["anagrpid"].IsNumber()) {
        ns.info.baseAttr.anagrpid = static_cast<uint32_t>(nsJson["anagrpid"].GetInt());
    }

    if (nsJson.HasMember("nvmsetid") && nsJson["nvmsetid"].IsNumber()) {
        ns.info.baseAttr.nvmsetid = static_cast<uint16_t>(nsJson["nvmsetid"].GetInt());
    }

    if (nsJson.HasMember("nmic") && nsJson["nmic"].IsBool()) {
        ns.info.baseAttr.nmic = nsJson["nmic"].GetBool();
    }

    if (nsJson.HasMember("usedBytes") && nsJson["usedBytes"].IsNumber()) {
        ns.info.usedBytes = static_cast<uint64_t>(nsJson["usedBytes"].GetInt64());
    }

    if (nsJson.HasMember("devPath") && nsJson["devPath"].IsString()) {
        std::strncpy(ns.info.devPath, nsJson["devPath"].GetString(), DEV_PATH_SIZE - 1);
    } else {
        std::string defaultNsPath = std::string(dev.devPath) + "n" + std::to_string(ns.info.namespaceId);
        std::strncpy(ns.info.devPath, defaultNsPath.c_str(), DEV_PATH_SIZE - 1);
    }

    if (nsJson.HasMember("guid") && nsJson["guid"].IsString()) {
        if (!HexStringToBytes(nsJson["guid"].GetString(), ns.info.guid, GUID_SIZE)) {
            fprintf(stderr, "[ERROR] LoadNamespaceFromJson: failed to parse guid\n");
            return false;
        }
    } else {
        for (uint32_t k = 0; k < GUID_SIZE; ++k) {
            ns.info.guid[k] = static_cast<unsigned char>(ns.info.namespaceId + k);
        }
    }

    if (nsJson.HasMember("uuid") && nsJson["uuid"].IsString()) {
        if (!HexStringToBytes(nsJson["uuid"].GetString(), ns.info.uuid, UUID_SIZE)) {
            fprintf(stderr, "[ERROR] LoadNamespaceFromJson: failed to parse uuid\n");
            return false;
        }
    } else {
        for (uint32_t k = 0; k < UUID_SIZE; ++k) {
            ns.info.uuid[k] = static_cast<unsigned char>(ns.info.namespaceId + k + 0x80);
        }
    }

    if (nsJson.HasMember("userData") && nsJson["userData"].IsObject()) {
        UbseSsuDevNameSpaceCustomData customData;
        LoadUserDataFromJson(nsJson["userData"], customData);
        memcpy(ns.info.userData, &customData, sizeof(customData));
    }

    if (nsJson.HasMember("state") && nsJson["state"].IsNumber()) {
        ns.info.state = static_cast<DevStatusT>(nsJson["state"].GetInt());
    }

    uint64_t lbaSize = (ns.info.baseAttr.flbas == 0) ? 512ULL : 4096ULL;
    ns.info.maxLba = ns.info.baseAttr.nsze - 1;
    ns.info.lbas = ns.info.baseAttr.nsze;
    ns.info.totalBytes = ns.info.baseAttr.nsze * lbaSize;
    ns.info.devAddr = dev.devAddr;

    uint64_t usedBytes = ns.info.baseAttr.ncap * lbaSize;
    if (dev.unvmcap >= usedBytes) {
        dev.unvmcap -= usedBytes;
    }

    return true;
}

bool LoadDeviceFromJson(const rapidjson::Value &devJson, MockDevice &dev)
{
    if (devJson.HasMember("cntlId") && devJson["cntlId"].IsNumber()) {
        dev.cntlId = static_cast<uint16_t>(devJson["cntlId"].GetInt());
        g_nextCntlId = std::max(g_nextCntlId, static_cast<uint16_t>(dev.cntlId + 1));
    } else {
        dev.cntlId = g_nextCntlId++;
    }

    std::strncpy(dev.name, "mock_ctrl", DEV_NAME_SIZE - 1);

    if (devJson.HasMember("devPath") && devJson["devPath"].IsString()) {
        std::strncpy(dev.devPath, devJson["devPath"].GetString(), DEV_PATH_SIZE - 1);
    } else {
        std::string defaultPath = "/dev/nvme" + std::to_string(dev.cntlId - 1);
        std::strncpy(dev.devPath, defaultPath.c_str(), DEV_PATH_SIZE - 1);
    }

    if (devJson.HasMember("sn") && devJson["sn"].IsString()) {
        std::strncpy(dev.sn, devJson["sn"].GetString(), SN_SIZE - 1);
    } else {
        std::string defaultSn = "MOCK_SN_" + std::to_string(dev.cntlId);
        std::strncpy(dev.sn, defaultSn.c_str(), SN_SIZE - 1);
    }

    if (devJson.HasMember("mn") && devJson["mn"].IsString()) {
        std::strncpy(dev.mn, devJson["mn"].GetString(), MN_SIZE - 1);
    } else {
        std::string defaultMn = "MOCK_MN_" + std::to_string(dev.cntlId);
        std::strncpy(dev.mn, defaultMn.c_str(), MN_SIZE - 1);
    }

    if (devJson.HasMember("tnvmcap") && devJson["tnvmcap"].IsNumber()) {
        dev.tnvmcap = static_cast<uint64_t>(devJson["tnvmcap"].GetInt64());
    } else {
        return false;
    }

    if (devJson.HasMember("unvmcap") && devJson["unvmcap"].IsNumber()) {
        dev.unvmcap = static_cast<uint64_t>(devJson["unvmcap"].GetInt64());
    } else {
        return false;
    }

    if (devJson.HasMember("eid") && devJson["eid"].IsString()) {
        if (!HexStringToBytes(devJson["eid"].GetString(), dev.devAddr.tgtEid.raw, EID_SIZE)) {
            fprintf(stderr, "[ERROR] LoadDeviceFromJson: failed to parse eid\n");
            return false;
        }
    }

    if (devJson.HasMember("subNqn") && devJson["subNqn"].IsString()) {
        std::strncpy(dev.devAddr.subNqn, devJson["subNqn"].GetString(), SUBNQN_SIZE - 1);
    }

    if (devJson.HasMember("state") && devJson["state"].IsNumber()) {
        dev.state = static_cast<DevStatusT>(devJson["state"].GetInt());
    } else {
        dev.state = DevStatusT::DEV_ONLINE;
    }

    dev.nextNamespaceId = 1;

    if (devJson.HasMember("namespaces") && devJson["namespaces"].IsArray()) {
        const rapidjson::Value &namespaces = devJson["namespaces"];
        for (rapidjson::SizeType j = 0; j < namespaces.Size(); ++j) {
            const rapidjson::Value &nsJson = namespaces[j];
            if (!nsJson.IsObject()) {
                continue;
            }

            MockNamespace ns;
            if (!LoadNamespaceFromJson(nsJson, dev, ns)) {
                fprintf(stderr, "[ERROR] LoadDeviceFromJson: failed to load namespace\n");
                continue;
            }
            dev.namespaces.push_back(std::move(ns));
        }
    }

    return true;
}

void LoadJsonConfig()
{
    const char *envPath = std::getenv("MOCK_SSU_CONFIG_PATH");
    std::string configPath = (envPath != nullptr) ? envPath : MOCK_SSU_CONFIG_PATH;

    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        return;
    }

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        return;
    }

    if (!doc.HasMember("devices") || !doc["devices"].IsArray()) {
        return;
    }

    const rapidjson::Value &devices = doc["devices"];
    for (rapidjson::SizeType i = 0; i < devices.Size(); ++i) {
        const rapidjson::Value &devJson = devices[i];
        if (!devJson.IsObject()) {
            continue;
        }

        MockDevice dev;
        if (LoadDeviceFromJson(devJson, dev)) {
            std::string key = EidToKey(dev.devAddr.tgtEid);
            g_devices.emplace(key, std::move(dev));
        }
    }
}

std::string BytesToHexString(const unsigned char *buf, size_t bufSize)
{
    std::string hex;
    hex.reserve(bufSize * 2);
    for (size_t i = 0; i < bufSize; ++i) {
        char tmp[3];
        snprintf(tmp, sizeof(tmp), "%02x", buf[i]);
        hex += tmp;
    }
    return hex;
}

void SaveUserDataToJson(const UbseSsuDevNameSpaceCustomData *customData, rapidjson::Value &userDataObj,
                        rapidjson::Document::AllocatorType &alloc)
{
    userDataObj.SetObject();
    userDataObj.AddMember("version", customData->version, alloc);
    userDataObj.AddMember("name", rapidjson::StringRef(customData->name), alloc);
    userDataObj.AddMember("defaultNqn", rapidjson::StringRef(customData->defaultNqn), alloc);
    userDataObj.AddMember("uid", customData->uid, alloc);
    userDataObj.AddMember("userName", rapidjson::StringRef(customData->userName), alloc);
    userDataObj.AddMember("tenant", rapidjson::StringRef(customData->tenant), alloc);
    userDataObj.AddMember("allocStrategy", customData->allocStrategy, alloc);
    userDataObj.AddMember("raidLevel", customData->raidLevel, alloc);
    userDataObj.AddMember("nsNum", customData->nsNum, alloc);
    userDataObj.AddMember("totalBytes", customData->totalBytes, alloc);
    userDataObj.AddMember("crc", customData->crc, alloc);
}

void SaveNamespaceToJson(const MockNamespace &ns, rapidjson::Value &nsObj,
                         rapidjson::Document::AllocatorType &alloc)
{
    nsObj.SetObject();
    nsObj.AddMember("namespaceId", ns.info.namespaceId, alloc);
    nsObj.AddMember("nsze", ns.info.baseAttr.nsze, alloc);
    nsObj.AddMember("ncap", ns.info.baseAttr.ncap, alloc);
    nsObj.AddMember("flbas", ns.info.baseAttr.flbas, alloc);
    nsObj.AddMember("dps", ns.info.baseAttr.dps, alloc);
    nsObj.AddMember("anagrpid", ns.info.baseAttr.anagrpid, alloc);
    nsObj.AddMember("nvmsetid", ns.info.baseAttr.nvmsetid, alloc);
    nsObj.AddMember("nmic", ns.info.baseAttr.nmic, alloc);
    nsObj.AddMember("usedBytes", ns.info.usedBytes, alloc);
    nsObj.AddMember("devPath", rapidjson::StringRef(ns.info.devPath), alloc);
    rapidjson::Value guidValue(BytesToHexString(ns.info.guid, GUID_SIZE).c_str(), alloc);
    nsObj.AddMember("guid", guidValue, alloc);
    rapidjson::Value uuidValue(BytesToHexString(ns.info.uuid, UUID_SIZE).c_str(), alloc);
    nsObj.AddMember("uuid", uuidValue, alloc);

    const UbseSsuDevNameSpaceCustomData *customData =
        reinterpret_cast<const UbseSsuDevNameSpaceCustomData *>(ns.info.userData);
    rapidjson::Value userDataObj(rapidjson::kObjectType);
    SaveUserDataToJson(customData, userDataObj, alloc);
    nsObj.AddMember("userData", userDataObj, alloc);

    nsObj.AddMember("state", static_cast<uint32_t>(ns.info.state), alloc);
}

void SaveDeviceToJson(const MockDevice &dev, rapidjson::Value &devObj,
                      rapidjson::Document::AllocatorType &alloc)
{
    devObj.SetObject();
    devObj.AddMember("tnvmcap", dev.tnvmcap, alloc);
    devObj.AddMember("unvmcap", dev.unvmcap, alloc);
    devObj.AddMember("cntlId", dev.cntlId, alloc);
    devObj.AddMember("devPath", rapidjson::StringRef(dev.devPath), alloc);
    devObj.AddMember("sn", rapidjson::StringRef(dev.sn), alloc);
    devObj.AddMember("mn", rapidjson::StringRef(dev.mn), alloc);
    rapidjson::Value eidValue(BytesToHexString(dev.devAddr.tgtEid.raw, EID_SIZE).c_str(), alloc);
    devObj.AddMember("eid", eidValue, alloc);
    devObj.AddMember("subNqn", rapidjson::StringRef(dev.devAddr.subNqn), alloc);
    devObj.AddMember("state", static_cast<uint32_t>(dev.state), alloc);

    rapidjson::Value namespaces(rapidjson::kArrayType);
    for (const auto &ns : dev.namespaces) {
        rapidjson::Value nsObj(rapidjson::kObjectType);
        SaveNamespaceToJson(ns, nsObj, alloc);
        namespaces.PushBack(nsObj, alloc);
    }
    devObj.AddMember("namespaces", namespaces, alloc);
}

void SaveJsonConfig()
{
    const char *envPath = std::getenv("MOCK_SSU_CONFIG_PATH");
    std::string configPath = (envPath != nullptr) ? envPath : MOCK_SSU_CONFIG_PATH;

    std::ofstream ofs(configPath);
    if (!ofs.is_open()) {
        return;
    }

    rapidjson::OStreamWrapper osw(ofs);
    rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
    rapidjson::Document doc;
    doc.SetObject();

    rapidjson::Document::AllocatorType &alloc = doc.GetAllocator();

    rapidjson::Value devices(rapidjson::kArrayType);
    for (const auto &pair : g_devices) {
        rapidjson::Value devObj(rapidjson::kObjectType);
        SaveDeviceToJson(pair.second, devObj, alloc);
        devices.PushBack(devObj, alloc);
    }
    doc.AddMember("devices", devices, alloc);

    doc.Accept(writer);
}

std::atomic<bool> g_configLoaded{false};

void EnsureConfigLoaded()
{
    bool expected = false;
    if (g_configLoaded.compare_exchange_strong(expected, true)) {
        LoadJsonConfig();
    }
}

MockDevice *EnsureDevice(const DevAddrT &addr)
{
    EnsureConfigLoaded();

    std::string key = EidToKey(addr.tgtEid);
    auto it = g_devices.find(key);
    if (it != g_devices.end()) {
        return &it->second;
    }

    return nullptr;
}

void CopyDevInfo(const MockDevice &mockDev, DevInfoT &out)
{
    std::memset(&out, 0, sizeof(DevInfoT));
    out.tnvmcap = mockDev.tnvmcap;
    out.unvmcap = mockDev.unvmcap;
    out.sgls = false;
    out.nsCount =
        static_cast<uint32_t>(std::min(mockDev.namespaces.size(), static_cast<size_t>(MAX_NAMESPACES_PER_CTRL)));
    out.cntlId = mockDev.cntlId;
    std::strncpy(out.name, mockDev.name, DEV_NAME_SIZE - 1);
    std::strncpy(out.devPath, mockDev.devPath, DEV_PATH_SIZE - 1);
    std::strncpy(out.sn, mockDev.sn, SN_SIZE - 1);
    std::strncpy(out.mn, mockDev.mn, MN_SIZE - 1);
    out.devAddr = mockDev.devAddr;
    out.state = mockDev.state;

    for (uint32_t i = 0; i < out.nsCount; ++i) {
        out.namespaces[i] = mockDev.namespaces[i].info;
    }
}

MockDevice *FindDevice(const DevEidT &tgtEid)
{
    EnsureConfigLoaded();

    std::string key = EidToKey(tgtEid);
    auto it = g_devices.find(key);
    if (it == g_devices.end()) {
        return nullptr;
    }
    return &it->second;
}

MockNamespace *FindNamespace(MockDevice &dev, uint32_t namespaceId)
{
    for (auto &ns : dev.namespaces) {
        if (ns.info.namespaceId == namespaceId) {
            return &ns;
        }
    }
    return nullptr;
}

} // namespace
