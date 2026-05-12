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

#include <cerrno>
#include <dlfcn.h>
#include "rmrs_libvirt_module.h"
#include "ubse_logger.h"
#include "mp_error.h"
#include "mp_configuration.h"

namespace mempooling::libvirt {
using namespace ubse::log;

void *LibvirtModule::libvirtHandle = nullptr;
VirConnectOpenFunc LibvirtModule::virConnectOpenFunc = nullptr;
VirConnectCloseFunc LibvirtModule::virConnectCloseFunc = nullptr;
VirConnectListAllDomainsFunc LibvirtModule::virConnectListAllDomainsFunc = nullptr;
VirDomainGetNameFunc LibvirtModule::virDomainGetNameFunc = nullptr;
VirDomainGetUUIDStringFunc LibvirtModule::virDomainGetUUIDStringFunc = nullptr;
VirDomainGetInfoFunc LibvirtModule::virDomainGetInfoFunc = nullptr;
VirDomainGetVcpusFunc LibvirtModule::virDomainGetVcpusFunc = nullptr;
VirDomainGetIDFunc LibvirtModule::virDomainGetIDFunc = nullptr;
VirConnectGetHostnameFunc LibvirtModule::virConnectGetHostnameFunc = nullptr;
VirDomainFreeFunc LibvirtModule::virDomainFreeFunc = nullptr;
VirConnectIsAliveFunc LibvirtModule::virConnectIsAliveFunc = nullptr;
VirEventRegisterDefaultImplFunc LibvirtModule::virEventRegisterDefaultImplFunc = nullptr;
VirEventRunDefaultImplFunc LibvirtModule::virEventRunDefaultImplFunc = nullptr;
VirConnectDomainEventRegisterFunc LibvirtModule::virConnectDomainEventRegisterFunc = nullptr;
VirConnectDomainEventDeRegisterFunc LibvirtModule::virConnectDomainEventDeRegisterFunc = nullptr;
VirConnectSetKeepAliveFunc LibvirtModule::virConnectSetKeepAliveFunc = nullptr;
VirDomainLookupByNameFunc LibvirtModule::virDomainLookupByNameFunc = nullptr;
VirDomainGetXMLDescFunc LibvirtModule::virDomainGetXMLDescFunc = nullptr;

MpResult LibvirtModule::Init()
{
    libvirtHandle = dlopen("libvirt.so.0", RTLD_LAZY);
    if (libvirtHandle == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "load libvirt.so.0 failed. " << strerror(errno) << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "load libvirt.so.0 success.";
    return MEM_POOLING_OK;
}

void LibvirtModule::CloseLibvirtHandle()
{
    if (libvirtHandle == nullptr) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "libvirtHandle is null, no need to close.";
    } else if (dlclose(libvirtHandle) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Close libvirtHandle failed," << dlerror() << ".";
    }
    virConnectOpenFunc = nullptr;
    virConnectCloseFunc = nullptr;
    virConnectListAllDomainsFunc = nullptr;
    virDomainGetNameFunc = nullptr;
    virDomainGetUUIDStringFunc = nullptr;
    virDomainGetInfoFunc = nullptr;
    virDomainGetVcpusFunc = nullptr;
    virDomainGetIDFunc = nullptr;
    virConnectGetHostnameFunc = nullptr;
    virDomainFreeFunc = nullptr;
    virConnectIsAliveFunc = nullptr;
    virEventRegisterDefaultImplFunc = nullptr;
    virEventRunDefaultImplFunc = nullptr;
    virConnectDomainEventRegisterFunc = nullptr;
    virConnectDomainEventDeRegisterFunc = nullptr;
    virConnectSetKeepAliveFunc = nullptr;
    virDomainLookupByNameFunc = nullptr;
    virDomainGetXMLDescFunc = nullptr;
    return;
}

VirConnectOpenFunc LibvirtModule::VirConnectOpen()
{
    if (virConnectOpenFunc != nullptr) {
        return virConnectOpenFunc;
    }
    virConnectOpenFunc = reinterpret_cast<VirConnectOpenFunc>(dlsym(libvirtHandle, "virConnectOpen"));
    if (virConnectOpenFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirConnectOpen ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectOpenFunc;
}

VirConnectCloseFunc LibvirtModule::VirConnectClose()
{
    if (virConnectCloseFunc != nullptr) {
        return virConnectCloseFunc;
    }
    virConnectCloseFunc = reinterpret_cast<VirConnectCloseFunc>(dlsym(libvirtHandle, "virConnectClose"));
    if (virConnectCloseFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virConnectClose ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectCloseFunc;
}

VirConnectListAllDomainsFunc LibvirtModule::VirConnectListAllDomains()
{
    if (virConnectListAllDomainsFunc != nullptr) {
        return virConnectListAllDomainsFunc;
    }
    virConnectListAllDomainsFunc =
        reinterpret_cast<VirConnectListAllDomainsFunc>(dlsym(libvirtHandle, "virConnectListAllDomains"));
    if (virConnectListAllDomainsFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirConnectListAllDomains ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectListAllDomainsFunc;
}

VirDomainGetNameFunc LibvirtModule::VirDomainGetName()
{
    if (virDomainGetNameFunc != nullptr) {
        return virDomainGetNameFunc;
    }
    virDomainGetNameFunc = reinterpret_cast<VirDomainGetNameFunc>(dlsym(libvirtHandle, "virDomainGetName"));
    if (virDomainGetNameFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirDomainGetName ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetNameFunc;
}

VirDomainGetIDFunc LibvirtModule::VirDomainGetID()
{
    if (virDomainGetIDFunc != nullptr) {
        return virDomainGetIDFunc;
    }
    virDomainGetIDFunc = reinterpret_cast<VirDomainGetIDFunc>(dlsym(libvirtHandle, "virDomainGetID"));
    if (virDomainGetIDFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirDomainGetID ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetIDFunc;
}

VirDomainGetUUIDStringFunc LibvirtModule::VirDomainGetUUIDString()
{
    if (virDomainGetUUIDStringFunc != nullptr) {
        return virDomainGetUUIDStringFunc;
    }
    virDomainGetUUIDStringFunc =
        reinterpret_cast<VirDomainGetUUIDStringFunc>(dlsym(libvirtHandle, "virDomainGetUUIDString"));
    if (virDomainGetUUIDStringFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virDomainGetUUIDString ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetUUIDStringFunc;
}

VirDomainGetInfoFunc LibvirtModule::VirDomainGetInfo()
{
    if (virDomainGetInfoFunc != nullptr) {
        return virDomainGetInfoFunc;
    }
    virDomainGetInfoFunc = reinterpret_cast<VirDomainGetInfoFunc>(dlsym(libvirtHandle, "virDomainGetInfo"));
    if (virDomainGetInfoFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirDomainGetInfo ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetInfoFunc;
}

VirDomainGetVcpusFunc LibvirtModule::VirDomainGetVcpus()
{
    if (virDomainGetVcpusFunc != nullptr) {
        return virDomainGetVcpusFunc;
    }
    virDomainGetVcpusFunc = reinterpret_cast<VirDomainGetVcpusFunc>(dlsym(libvirtHandle, "virDomainGetVcpus"));
    if (virDomainGetVcpusFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirDomainGetVcpus ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetVcpusFunc;
}

VirConnectGetHostnameFunc LibvirtModule::VirConnectGetHostname()
{
    if (virConnectGetHostnameFunc != nullptr) {
        return virConnectGetHostnameFunc;
    }
    virConnectGetHostnameFunc =
        reinterpret_cast<VirConnectGetHostnameFunc>(dlsym(libvirtHandle, "virConnectGetHostname"));
    if (virConnectGetHostnameFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirConnectGetHostname ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectGetHostnameFunc;
}

VirDomainFreeFunc LibvirtModule::VirDomainFree()
{
    if (virDomainFreeFunc != nullptr) {
        return virDomainFreeFunc;
    }
    virDomainFreeFunc = reinterpret_cast<VirDomainFreeFunc>(dlsym(libvirtHandle, "virDomainFree"));
    if (virDomainFreeFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get VirDomainFree ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainFreeFunc;
}

VirConnectIsAliveFunc LibvirtModule::VirConnectIsAlive()
{
    if (virConnectIsAliveFunc != nullptr) {
        return virConnectIsAliveFunc;
    }
    virConnectIsAliveFunc = reinterpret_cast<VirConnectIsAliveFunc>(dlsym(libvirtHandle, "virConnectIsAlive"));
    if (virConnectIsAliveFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get VirConnectIsAlive ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectIsAliveFunc;
}

VirEventRegisterDefaultImplFunc LibvirtModule::VirEventRegisterDefaultImpl()
{
    if (virEventRegisterDefaultImplFunc != nullptr) {
        return virEventRegisterDefaultImplFunc;
    }
    virEventRegisterDefaultImplFunc =
        reinterpret_cast<VirEventRegisterDefaultImplFunc>(dlsym(libvirtHandle, "virEventRegisterDefaultImpl"));
    if (virEventRegisterDefaultImplFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virEventRegisterDefaultImpl ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virEventRegisterDefaultImplFunc;
}

VirEventRunDefaultImplFunc LibvirtModule::VirEventRunDefaultImpl()
{
    if (virEventRunDefaultImplFunc != nullptr) {
        return virEventRunDefaultImplFunc;
    }
    virEventRunDefaultImplFunc =
        reinterpret_cast<VirEventRunDefaultImplFunc>(dlsym(libvirtHandle, "virEventRunDefaultImpl"));
    if (virEventRunDefaultImplFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virEventRunDefaultImpl ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virEventRunDefaultImplFunc;
}

VirConnectDomainEventRegisterFunc LibvirtModule::VirConnectDomainEventRegister()
{
    if (virConnectDomainEventRegisterFunc != nullptr) {
        return virConnectDomainEventRegisterFunc;
    }
    virConnectDomainEventRegisterFunc =
        reinterpret_cast<VirConnectDomainEventRegisterFunc>(dlsym(libvirtHandle, "virConnectDomainEventRegister"));
    if (virConnectDomainEventRegisterFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virConnectDomainEventRegister ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectDomainEventRegisterFunc;
}

VirConnectDomainEventDeRegisterFunc LibvirtModule::VirConnectDomainEventDeRegister()
{
    if (virConnectDomainEventDeRegisterFunc != nullptr) {
        return virConnectDomainEventDeRegisterFunc;
    }
    virConnectDomainEventDeRegisterFunc =
        reinterpret_cast<VirConnectDomainEventDeRegisterFunc>(dlsym(libvirtHandle, "virConnectDomainEventDeRegister"));
    if (virConnectDomainEventDeRegisterFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virConnectDomainEventDeRegister ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectDomainEventDeRegisterFunc;
}

VirConnectSetKeepAliveFunc LibvirtModule::VirConnectSetKeepAlive()
{
    if (virConnectSetKeepAliveFunc != nullptr) {
        return virConnectSetKeepAliveFunc;
    }
    virConnectSetKeepAliveFunc =
        reinterpret_cast<VirConnectSetKeepAliveFunc>(dlsym(libvirtHandle, "virConnectSetKeepAlive"));
    if (virConnectSetKeepAliveFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virConnectSetKeepAlive ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectSetKeepAliveFunc;
}

VirDomainLookupByNameFunc LibvirtModule::VirDomainLookupByName()
{
    if (virDomainLookupByNameFunc != nullptr) {
        return virDomainLookupByNameFunc;
    }
    virDomainLookupByNameFunc =
        reinterpret_cast<VirDomainLookupByNameFunc>(dlsym(libvirtHandle, "virDomainLookupByName"));
    if (virDomainLookupByNameFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virDomainLookupByName ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainLookupByNameFunc;
}

VirDomainGetXMLDescFunc LibvirtModule::VirDomainGetXMLDesc()
{
    if (virDomainGetXMLDescFunc != nullptr) {
        return virDomainGetXMLDescFunc;
    }
    virDomainGetXMLDescFunc =
        reinterpret_cast<VirDomainGetXMLDescFunc>(dlsym(libvirtHandle, "virDomainGetXMLDesc"));
    if (virDomainGetXMLDescFunc == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get virDomainGetXMLDesc ptr failed. " << std::string(strerror(errno)) << ".";
        return nullptr;
    }
    return virDomainGetXMLDescFunc;
}
} // namespace mempooling::libvirt