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

#ifndef UBSEFS_UBSE_FS_H
#define UBSEFS_UBSE_FS_H
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace ubse::misc::fs {
class UbseFs {
public:
    explicit UbseFs(std::string rootPath) : rootPath_(std::move(rootPath)){};
    /**
    * @brief 删除指定文件
    * @param fileName [in] 文件名
    * @return 成功返回0，失败返回非0
    */
    uint32_t DeleteFile(const std::string &fileName);

    /**
    * @brief 读取指定文件
    * @param fileName [in] 文件名
    * @param data [out] 读取内容指针
    * @param dataLen [out] 读取内容长度
    * @return 成功返回0，失败返回非0
    */
    uint32_t ReadFile(const std::string &fileName, uint8_t *&data, uint32_t &dataLen);

    /**
    * @brief 写入数据到指定文件
    * @param fileName [in] 文件名
    * @param data [in] 写入文件内容指针
    * @param dataLen [in] 写入文件内容长度
    * @return 成功返回0，失败返回非0
    */
    uint32_t WriteFile(const std::string &fileName, const uint8_t *data, const uint32_t dataLen);

private:
    std::string rootPath_;
    std::shared_ptr<std::mutex> GetLock(const std::string &fileName);

    void RemoveLock(const std::string &fileName);

    std::mutex mapLock_;
    std::unordered_map<std::string, std::shared_ptr<std::mutex>> shareMutexMap_;
};
} // namespace ubse::misc::fs
#endif // UBSEFS_UBSE_FS_H
