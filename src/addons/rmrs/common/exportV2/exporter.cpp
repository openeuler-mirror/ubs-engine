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

#include <filesystem>
#include <fstream>

#include "LibvirtHelper/LibvirtHelper.h"
#include "OsHelper/OsHelper.h"
#include "ubse_logger.h"
#include "exporter.h"
#include <iostream>

namespace mempooling::exportV2 {
using namespace ubse::log;

const std::string SUB_MODULE_NAME = "[ExporterV2] ";
constexpr size_t MAX_THREADPOOL_SIZE = 128;
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << SUB_MODULE_NAME

std::atomic<bool> Exporter::inited_{false};
std::mutex Exporter::init_mu_;
Exporter::Options Exporter::cfg_{};
ThreadPool *Exporter::pool_ = nullptr;

MpResult Exporter::Init()
{
    return Init(Options{});
}
MpResult Exporter::Init(Options opt)
{
    LOG_INFO << "Exporter init starting...";
    std::lock_guard<std::mutex> lk(init_mu_);
    if (inited_.load(std::memory_order_acquire)) {
        LOG_WARN << "Exporter already inited";
        return MEM_POOLING_OK;
    }

    cfg_ = opt;
    size_t hw = std::thread::hardware_concurrency();
    size_t n = cfg_.threads ? cfg_.threads : std::min(hw, MAX_THREADPOOL_SIZE);
    n = std::max(n, size_t(1));
    LOG_DEBUG << "Thread pool size: " << n;
    try {
        pool_ = new ThreadPool(n);
    } catch (const std::exception &e) {
        LOG_ERROR << "ThreadPool init failed: " << e.what();
        pool_ = nullptr;  // 防御性置空
        return MEM_POOLING_ERROR;
    }
    auto ret = LibvirtHelper::GetInstance().Init();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Libvirt init failed: " << ret;
        // 失败回滚：释放已创建的线程池
        if (pool_) {
            pool_->stop(true);
            delete pool_;
            pool_ = nullptr;
        }
        return MEM_POOLING_ERROR;
    }

    inited_.store(true, std::memory_order_release);
    LOG_INFO << "Exporter init done.";
    return MEM_POOLING_OK;
}

ThreadPool &Exporter::Pool()
{
    auto *p = pool_;
    if (p == nullptr) {
        throw std::runtime_error("Exporter pool accessed after Shutdown()");
    }
    return *p;
}

void Exporter::Shutdown(bool wait)
{
    std::lock_guard<std::mutex> lk(init_mu_);
    if (!inited_.load(std::memory_order_acquire)) {
        return;
    }
    inited_.store(false, std::memory_order_release);

    // 先停池，避免新任务再调 libvirt
    if (pool_) {
        pool_->stop(wait);
        delete pool_;
        pool_ = nullptr;
    }

    // 再关 libvirt
    LibvirtHelper::GetInstance().Shutdown();
    LOG_INFO << "Exporter shutdown done.";
}

time_t Exporter::NowTimeT()
{
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void Exporter::StartGate::wait()
{
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return go_; });
}
void Exporter::StartGate::release()
{
    {
        std::lock_guard<std::mutex> lk(mu_);
        go_ = true;
    }
    cv_.notify_all();
}

bool isVmDomainInfoError(const VmDomainInfo &info)
{
    if (info.timestamp == -1) {
        return false;
    }
    return true;
}

void fillErrorVmDomainInfo(VmDomainInfo &info)
{
    info.timestamp = -1;
}

VmDomainInfo Exporter::genVmDomainInfo(const std::string &vmName)
{
    VmDomainInfo info;
    info.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    info.metaData.name = vmName;
    try {
        info.metaData.nodeId = MpConfiguration::GetInstance().GetNodeId();
        auto ret = OsHelper::GetHostName(info.metaData.hostName);
        LOG_DEBUG << "GetHostName ret=" << ret << ".";
        ret |= OsHelper::GetVmPidByVmName(vmName, info.metaData.pid);
        LOG_DEBUG << "GetVmPidByVmName ret=" << ret << ".";
        ret |= OsHelper::GetProcessStartTimeByPid(info.metaData.pid, info.metaData.vmCreateTime);
        LOG_DEBUG << "GetProcessStartTimeByPid ret=" << ret << ".";
        ret |= OsHelper::GetVmNumaInfoByPid(info.metaData.pid, info);
        LOG_DEBUG << "GetVmNumaInfoByPid ret=" << ret << ".";
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Gen vm domain info failed, vm name: " << vmName;
            fillErrorVmDomainInfo(info);
            return info;
        }
        VirDomainPtr dom = nullptr;
        ret = LibvirtHelper::GetInstance().GetDomainByName(vmName, dom);
        ret |= LibvirtHelper::GetInstance().GetVmUuidByDomain(dom, info.metaData.uuid);
        ret |= LibvirtHelper::GetInstance().GetVmStateAndMaxMemByDomain(dom, info);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Gen vm domain info failed, vm name: " << vmName;
            LibvirtHelper::GetInstance().FreeDomain(dom);
            fillErrorVmDomainInfo(info);
            return info;
        }
        LibvirtHelper::GetInstance().FreeDomain(dom);
        return info;
    } catch (const std::exception &e) {
        LOG_ERROR << "Gen vm domain info threw std::exception, vm name: " << vmName << ", what=" << e.what();
        fillErrorVmDomainInfo(info);
        return info;
    } catch (...) {
        LOG_ERROR << "Gen vm domain info threw unknown exception, vm name: " << vmName;
        fillErrorVmDomainInfo(info);
        return info;
    }
}

NumaInfo Exporter::genNumaInfo(const std::uint16_t &numaId)
{
    NumaInfo info;
    info.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    info.metaData.numaId = numaId;
    try {
        info.metaData.nodeId = MpConfiguration::GetInstance().GetNodeId();
        auto ret = OsHelper::GetHostName(info.metaData.hostName);
        ret |= OsHelper::IsNumaLocal(numaId, info.metaData.isLocal);
        ret |= OsHelper::GetMemInfoByNumaId(numaId, info);
        if (info.metaData.isLocal) {
            ret |= OsHelper::GetSocketIdByNumaId(numaId, info.metaData.socketId);
        } else {
            info.metaData.socketId = -1;
        }
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Gen numa info failed, numa id: " << numaId;
            return info;
        }
        return info;
    } catch (const std::exception &e) {
        LOG_ERROR << "Gen numa info threw std::exception, numaId: " << numaId << ", what=" << e.what();
        return info;
    } catch (...) {
        LOG_ERROR << "Gen numa info threw unknown exception, numaId: " << numaId;
        return info;
    }
}

MpResult Exporter::GetNumaInfoImmediately(std::vector<NumaInfo> &numaInfos)
{
    try {
        if (!inited_.load(std::memory_order_acquire)) {
            LOG_ERROR << "Exporter is not inited.";
            return MEM_POOLING_ERROR;
        }

        std::vector<std::uint16_t> numaSet;
        auto ret = OsHelper::GetNumaSet(numaSet);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Get numaId set failed";
            return ret;
        }

        LOG_DEBUG << "GetNumaInfoImmediately: numaSet size=" << numaSet.size();

        parallel_collect<uint16_t, NumaInfo>(
            numaSet, numaInfos, [](uint16_t numaId) -> NumaInfo { return genNumaInfo(numaId); },
            "Exporter.GetNumaInfo");

        for (auto &numaInfo : numaInfos) {
            LOG_DEBUG << "NumaInfo: " << numaInfo.ToString();
        }

        if (numaSet.size() != numaInfos.size()) {
            LOG_ERROR << "Get numa info failed, numa set size: " << numaSet.size()
                      << ", numa info size: " << numaInfos.size();
            ret = MEM_POOLING_ERROR;
        }
        return ret;
    } catch (const std::exception &e) {
        LOG_ERROR << "Exporter::GetNumaInfoImmediately caught std::exception: " << e.what();
        return MEM_POOLING_ERROR;
    } catch (...) {
        LOG_ERROR << "Exporter::GetNumaInfoImmediately caught unknown exception";
        return MEM_POOLING_ERROR;
    }
}

MpResult Exporter::GetVmInfoImmediately(std::vector<VmDomainInfo> &vmInfos)
{
    try {
        if (!inited_.load(std::memory_order_acquire)) {
            LOG_ERROR << "Exporter is not inited.";
            return MEM_POOLING_ERROR;
        }
        std::vector<std::string> vmNameSet; // 存储vmName
        auto ret = OsHelper::GetVmNameSet(vmNameSet);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Get vm name set failed";
            return ret;
        }
        ret = LibvirtHelper::GetInstance().CheckConnectAndReconnect();
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Check connect and reconnect failed";
            return ret;
        }
        LOG_DEBUG << "GetVmInfoImmediately: vmNameSet size=" << vmNameSet.size();
        parallel_collect<std::string, VmDomainInfo>(
            vmNameSet, vmInfos, [](const std::string &vmName) -> VmDomainInfo { return genVmDomainInfo(vmName); },
            "Exporter.GetVmInfo");
        for (auto it = vmInfos.begin(); it != vmInfos.end();) {
            if (!isVmDomainInfoError(*it)) {
                LOG_ERROR << "Failed vmInfo= " << it->ToString();
                it = vmInfos.erase(it);
            } else {
                LOG_DEBUG << "vmInfo= " << it->ToString();
                ++it;
            }
        }
        if (vmNameSet.size() != vmInfos.size()) {
            LOG_ERROR << "Get vm info failed, vm set size: " << vmNameSet.size()
                      << ", vm info size: " << vmInfos.size();
            ret = MEM_POOLING_ERROR;
        }
        return ret;
    } catch (const std::exception &e) {
        LOG_ERROR << "Exporter::GetVmInfoImmediately caught std::exception: " << e.what();
        return MEM_POOLING_ERROR;
    } catch (...) {
        LOG_ERROR << "Exporter::GetVmInfoImmediately caught unknown exception";
        return MEM_POOLING_ERROR;
    }
}

}  // namespace mempooling::exportV2