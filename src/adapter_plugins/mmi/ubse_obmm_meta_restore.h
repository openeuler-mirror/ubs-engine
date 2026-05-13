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

#ifndef UBSE_MANAGER_RM_OBMM_META_RESTORE_H
#define UBSE_MANAGER_RM_OBMM_META_RESTORE_H

#include <dirent.h>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_obmm_def.h"

namespace ubse::mmi {
using namespace ubse::common::def;

class RmObmmMetaRestore {
public:
    /**
     *
     * @param taskId 标记一次收集请求，避免重复消息处理,可能同一个task，多次请求，涉及分页
     * @param ubseMemLocalObmmMetaDatas 本地所有obmm元数据，已经处理过异常数据后
     * @param lastPage 是否最后一页
     * @return code 0 返回成功 非0错误
     */
    static UbseResult ReadAgentLocalObmmMetaData(uint64_t taskId,
                                                 std::vector<UbseMemLocalObmmMetaData>& ubseMemLocalObmmMetaDatas,
                                                 bool& lastPage);
    /**
     * 读取所有obmm_shmdev目录路径
     * @return 所有obmm_shmdev目录路径
     */
    static std::vector<std::string> ReadAllObmmShmDevPath();

    /**
     * @brief 遍历每个obmm_shmdev目录，恢复账本数据
     * @param path obmm_shmdev目录
     * @param localObmmMetaData 恢复的账本数据
     * @return code 0 返回成功 非0错误
     */
    static UbseResult RestoreAgentLocalObmmMetaData(const std::string& path,
                                                    UbseMemLocalObmmMetaData& localObmmMetaData);

    /**
     * @param buffer 从obmm接口的获取的buff
     * @param length buffer的长度
     * @param customMeta 从buffer恢复的数据
     * @return code 0 返回成功 非0错误
     */
    static UbseResult RestoreAgentLocalObmmMetaDataFromBuffer(const uint8_t* buffer, uint64_t length,
                                                              UbseMemLocalObmmCustomMeta& customMeta,
                                                              UbMemPrivData& privData);

    RmObmmMetaRestore(const RmObmmMetaRestore& other) = delete;
    RmObmmMetaRestore(RmObmmMetaRestore&& other) = delete;
    RmObmmMetaRestore& operator=(const RmObmmMetaRestore& other) = delete;
    RmObmmMetaRestore& operator=(RmObmmMetaRestore&& other) noexcept = delete;

private:
    RmObmmMetaRestore() = default;
};

class RmObmmDevRead {
public:
    static UbseResult GetNuma(mem_id memid, uint64_t& numa);

    /**
     * @param path obmm存放内存信息的目录，example: /sys/devices/obmm/obmm_shmdev%d
     * @param memIdType 内存的类型，导入或导出
     * @return code 0 返回成功 非0错误
     */
    static UbseResult GetMemIdType(const std::string& path, uint8_t& memIdType);

    /**
     * @param path obmm存放内存信息的目录，example: /sys/devices/obmm/obmm_shmdev%d
     * @param memIdType 导出或导入类型
     * @param remoteNuma 远端numa呈现
     * @return code 0 返回成功 非0错误
     */
    static UbseResult GetRemoteNuma(const std::string& path, uint8_t memIdType, int16_t& remoteNuma);

    /**
     * @param path obmm存放内存信息的目录，example: /sys/devices/obmm/obmm_shmdev%d
     * @param totalSize 借入借出的内存总大小
     * @return code 0 返回成功 非0错误
     */
    static UbseResult GetTotalSize(const std::string& path, uint64_t& totalSize);

    /**
     * @param path obmm存放内存信息的目录，example: /sys/devices/obmm/obmm_shmdev%d
     * @param localMemId 同一节点上的内存唯一标识
     * @return code 0 返回成功 非0错误
     */
    static UbseResult GetLocalMemId(const std::string& path, uint64_t& localMemId);

    /**
     * @param path obmm存放内存信息的目录，example: /sys/devices/obmm/obmm_shmdev%d
     * @param memIdType 导出或导入类型
     * @param ubMemInfo 恢复的内存描述符
     * @return code 0 返回成功 非0错误
     */
    static UbseResult GetMemUbMemInfo(const std::string& path, uint8_t memIdType, ubse_mem_obmm_mem_desc& ubMemInfo);

    /**
     * @param path obmm存放内存信息的目录，example: /sys/devices/obmm/obmm_shmdev%d
     * @param customMeta 存储的buffer数据，转成定义好的结构体
     * @return code 0 返回成功 非0错误
     */
    static UbseResult GetCustomMeta(const std::string& path, UbseMemLocalObmmCustomMeta& customMeta,
                                    UbMemPrivData& privData);

    static UbseResult GetUbMemInfoFromFile(const std::string& path, std::string& uba, std::string& length,
                                           std::string& tokenid, std::string& deid);

    static UbseResult GetNameByMemId(mem_id memId, std::string& name);
    static UbseResult GetBorrowTypeByMemId(mem_id memId, uint8_t& borrowType);
};
} // namespace ubse::mmi

#endif // UBSE_MANAGER_RM_OBMM_META_RESTORE_H
