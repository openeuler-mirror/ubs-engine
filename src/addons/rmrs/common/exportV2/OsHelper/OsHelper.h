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

#ifndef OS_HELPER_H
#define OS_HELPER_H

#include <unistd.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "../export_type.h"
#include "mp_configuration.h"
#include "mp_error.h"

namespace mempooling::exportV2 {

class OsHelper {
public:
    // 获取当前环境上numa 节点集合
    static MpResult GetNumaSet(std::vector<std::uint16_t>& numaSet);

    // 获取当前环境上虚机名称集合
    static MpResult GetVmNameSet(std::vector<std::string>& vmNameSet);

    // 获取当前环境hostname
    static MpResult GetHostName(std::string& hostName);

    // 根据numaId判断numa是本地numa还是远端numa
    static MpResult IsNumaLocal(const std::uint16_t& numaId, bool& isLocal);

    // 根据numaId获取socketId
    static MpResult GetSocketIdByNumaId(const std::uint16_t& numaId, std::int16_t& socketId);

    // 根据numaId获取该numa下的内存信息
    static MpResult GetMemInfoByNumaId(const std::uint16_t& numaId, NumaInfo& info);

    // 根据虚机name获取虚机pid
    static MpResult GetVmPidByVmName(const std::string& vmName, pid_t& pid);

    // 根据pid获取该进程启动时间
    static MpResult GetProcessStartTimeByPid(const pid_t& pid, std::time_t& startTime);

    // 根据虚机pid获取虚机内存使用情况与大页类型
    static MpResult GetVmNumaInfoByPid(const pid_t& pid, VmDomainInfo& info);

private:
    // 解析cpulist文件
    static std::vector<uint16_t> parseCpuList(const std::string& line);
};
} // namespace mempooling::exportV2

#endif // OS_HELPER_H