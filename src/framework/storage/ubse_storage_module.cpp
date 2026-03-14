/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include "ubse_storage_module.h"
#include <sys/stat.h>
#include "ubse_context.h"
#include "ubse_logger_module.h"
#include "ubse_storage_req_handler.h"
#include "ubse_storage_req_simpo.h"
#include "ubse_storage_resp_simpo.h"
#include "ubse_com_module.h"
#include "ubse_election_module.h"
#include "ubse_election.h"
#include "ubse_fs.h"
#include "ubse_security_module.h"

namespace ubse::storage {
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::election;
using namespace ubse::misc::fs;
using namespace ubse::security;

BASE_DYNAMIC_CREATE(UbseStorageModule, UbseLoggerModule, UbseElectionModule);
UBSE_DEFINE_THIS_MODULE("ubse");

class UbseStorageModule::Impl {
public:
    static std::string GetDbStorageDir();

    static std::shared_ptr<UbseComModule> GetUbseComModule();

    static std::string GetMasterNode();

    static UbseResult RpcSend(const SendParam &sendParam, UbseBaseMessagePtr &request, UbseBaseMessagePtr &response);

    static UbseResult SendRemoteReq(const std::string &dbName, const std::string &key,
                                    message::UbseStorageReqCmdType cmdType, const RemoteGetHandler &handler);

    static UbseResult RegRemoteReqHandler();

    const std::string UBSE_DATA_PATH_ = "/var/lib/ubse/data";
    UbseFs ubseFs_{ UBSE_DATA_PATH_ };
};

UbseStorageModule::UbseStorageModule() : pImpl_(std::make_unique<Impl>()) {}
UbseStorageModule::~UbseStorageModule() = default;

bool IsDirectoryExists(const std::string &path)
{
    struct stat info{};
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    return S_ISDIR(info.st_mode);
}

UbseResult CreateDirectory(const std::string &path)
{
    size_t pos = 0;
    std::vector<__u32> caps{CAP_DAC_OVERRIDE};
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        std::string subPath = path.substr(0, pos);
        if (!subPath.empty() && mkdir(subPath.c_str(), DIR_MODE) != 0 && errno != EEXIST) {
            UBSE_LOG_ERROR << ("Failed to create directory: " + subPath).c_str();
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
            return UBSE_ERROR;
        }
    }
    if (mkdir(path.c_str(), DIR_MODE) != 0 && errno != EEXIST) {
        UBSE_LOG_ERROR << ("Failed to create directory: " + path).c_str();
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        return UBSE_ERROR;
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    return UBSE_OK;
}

bool CheckDirectoryPermission(const std::string &path, mode_t mode)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        UBSE_LOG_ERROR << "Failed to access directory [" << path << "]";
        return false;
    }
    return (info.st_mode & DIR_MODE_MASK) == mode;
}

UbseResult UbseStorageModule::Initialize()
{
    const std::string dbStoreDir = pImpl_->GetDbStorageDir();
    auto ret = UBSE_OK;
    if (!IsDirectoryExists(dbStoreDir)) {
        ret = CreateDirectory(dbStoreDir);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    if (!CheckDirectoryPermission(dbStoreDir, DIR_MODE)) {
        UBSE_LOG_ERROR << "The permission of the directory that db stores data in is incorrect";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseStorageModule::UnInitialize()
{
}

UbseResult UbseStorageModule::Start()
{
    if (const auto ret = Impl::RegRemoteReqHandler(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegRemoteReqHandler failed";
        return ret;
    }
    return UBSE_OK;
}

void UbseStorageModule::Stop()
{
}

std::shared_ptr<UbseStorageModule> UbseStorageModule::GetStorageModule()
{
    UbseContext &ubseCtx = UbseContext::GetInstance();
    auto storageModule = ubseCtx.GetModule<UbseStorageModule>();
    if (storageModule == nullptr) {
        return nullptr;
    }
    return storageModule;
}

std::shared_ptr<UbseComModule> UbseStorageModule::Impl::GetUbseComModule()
{
    UbseContext &ubseCtx = UbseContext::GetInstance();
    auto comModule = ubseCtx.GetModule<UbseComModule>();
    if (comModule == nullptr) {
        return nullptr;
    }
    return comModule;
}

std::string UbseStorageModule::Impl::GetMasterNode()
{
    UbseRoleInfo masterNode{};
    auto retCode = UbseGetMasterInfo(masterNode);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master id failed, " << FormatRetCode(retCode);
        return "";
    }
    return masterNode.nodeId;
}

UbseResult UbseStorageModule::Put([[maybe_unused]] const std::string &dbName, const std::string &key, uint8_t *value,
                                  const uint32_t valueLen)
{
    return pImpl_->ubseFs_.WriteFile(key, value, valueLen);
}

UbseResult UbseStorageModule::Get([[maybe_unused]] const std::string &dbName, const std::string &key, KV &kv)
{
    return pImpl_->ubseFs_.ReadFile(key, kv.value, kv.valueLen);
}

UbseResult UbseStorageModule::RemoteGet(const std::string &dbName, const std::string &key,
                                        const RemoteGetHandler &handler)
{
    return Impl::SendRemoteReq(dbName, key, message::UbseStorageReqCmdType::GET, handler);
}

void UbseStorageModule::ResultFree(KV &data)
{
    if (data.value) {
        delete[] data.value;
        data.value = nullptr;
    }
}

void UbseStorageModule::ResultFree(std::vector<KV> &data)
{
    for (auto& i : data) {
        i.key.clear();
        ResultFree(i);
    }
    data.clear();
    data.shrink_to_fit();
}

UbseResult UbseStorageModule::Delete([[maybe_unused]] const std::string &dbName, const std::string &key)
{
    return pImpl_->ubseFs_.DeleteFile(key);
}

std::string UbseStorageModule::Impl::GetDbStorageDir()
{
    return DB_STORE_DIR;
}

UbseResult UbseStorageModule::Impl::RpcSend(const SendParam &sendParam, UbseBaseMessagePtr &request,
                                            UbseBaseMessagePtr &response)
{
    auto comModule = GetUbseComModule();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "The UbseComModule has not been initialized";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RpcSend(sendParam, request, response);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseStorageModule::Impl::SendRemoteReq(const std::string &dbName, const std::string &key,
                                                  message::UbseStorageReqCmdType cmdType,
                                                  const RemoteGetHandler &handler)
{
    auto masterNode = GetMasterNode();
    if (masterNode.empty()) {
        return UBSE_ERROR;
    }
    SendParam sendParam(masterNode, static_cast<uint16_t>(UbseModuleCode::STORAGE),
                        static_cast<uint16_t>(UbseStorageOpCode::STORAGE_REQ));
    message::UbseStorageReq req;
    req.key = key;
    req.cmdType = cmdType;
    req.dbName = dbName;
    UbseBaseMessagePtr requestSimpoPtr = new (std::nothrow) message::UbseStorageReqSimpo(req);
    UbseBaseMessagePtr responseSimpoPtr = new (std::nothrow) message::UbseStorageRespSimpo();
    if (requestSimpoPtr == nullptr || responseSimpoPtr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = RpcSend(sendParam, requestSimpoPtr, responseSimpoPtr);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    message::UbseStorageRespSimpoPtr response =
        UbseBaseMessage::DeConvert<message::UbseStorageRespSimpo>(responseSimpoPtr);
    if (response == nullptr) {
        UBSE_LOG_ERROR << "UbseStorageRespSimpoPtr is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ret = handler(response->GetStorageResp().kvs);
    return ret;
}

UbseResult UbseStorageModule::Impl::RegRemoteReqHandler()
{
    auto comModule = GetUbseComModule();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "The UbseComModule has not been initialized";
        return UBSE_ERROR_NULLPTR;
    }
    UbseComBaseMessageHandlerPtr ubseNodeHostInfoHandler = new (std::nothrow) UbseStorageReqHandler();
    if (ubseNodeHostInfoHandler == nullptr) {
        UBSE_LOG_ERROR << "new register UbseStorageReqHandler fail";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret =
        comModule->RegRpcService<message::UbseStorageReqSimpo, message::UbseStorageRespSimpo>(ubseNodeHostInfoHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg UbseStorageReqHandler fail";
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::storage