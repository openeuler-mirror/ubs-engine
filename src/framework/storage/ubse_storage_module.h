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

#ifndef UBSE_STORE_MODULE_H
#define UBSE_STORE_MODULE_H
#include <functional>
#include "ubse_module.h"
#include "ubse_storage.h"

namespace ubse::storage {
using std::uint32_t;
using std::uint8_t;
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

const std::string DEFAULT_DB_NAME = "default";
const std::string DB_STORE_DIR = "/var/lib/ubse/data";
const uint32_t DIR_MODE = 0750;
const uint32_t DIR_MODE_MASK = 0777;

/* *
 * 远程查询回调
 * @param kvs [OUT] 模糊查询查询 结果列表
 * @return UbseResult, 成功返回0, 失败返回非0
 * @brief kv需要使用ResultFree进行释放
 */
using RemoteGetHandler = std::function<UbseResult(const std::vector<KV>& kvs)>;

class UbseStorageModule : public UbseModule {
public:
    UbseStorageModule();
    ~UbseStorageModule() override;
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    static std::shared_ptr<UbseStorageModule> GetStorageModule();

    /* *
     * 存储数据
     * @param dbName [IN] 数据库名称
     * @param key [IN] 存储数据 key
     * @param value [IN] 存储数据 value
     * @param valueLen [IN] 存储数据 value 长度
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult Put(const std::string& dbName, const std::string& key, uint8_t* value, const uint32_t valueLen);

    /* *
     * 精确查询
     * @param dbName [IN] 数据库名称
     * @param key [IN] 精确查询数据 key
     * @param kvs [OUT] 精确查询查询 结果
     * @return UbseResult, 成功返回0, 失败返回非0
     * @brief kv需要使用ResultFree进行释放
     */
    UbseResult Get(const std::string& dbName, const std::string& key, KV& kv);

    /* *
     * 从主节点精确查询数据
     * @param dbName [IN] 数据库名称
     * @param key [IN] 精确查询数据 key
     * @param handler [IN] 结果处理函数
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult RemoteGet(const std::string& dbName, const std::string& key, const RemoteGetHandler& handler);

    /* *
     * 清理 精确查询接口为 kv 数据分配空间
     * @param data Get接口返回的 kv 对象
     */
    static void ResultFree(KV& data);

    /* *
     * 清理 模糊查询接口为 kvs 数据分配空间
     * @param data Get接口返回的 kvs 对象
     */
    static void ResultFree(std::vector<KV>& data);

    /* *
     * 删除数据
     * @param dbName [IN] 数据库名称
     * @param key [IN] 删除数据 key
     * @return UbseResult, 成功返回0, 失败返回非0
     */
    UbseResult Delete(const std::string& dbName, const std::string& key);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
} // namespace ubse::storage

#endif // UBSE_STORE_MODULE_H
