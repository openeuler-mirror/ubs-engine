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

#include "ubse_storage.h"

namespace ubse::storage {
using UbseStorageDealDataFunc =
    std::function<void(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff, void *ctx)>;

/**
 * @brief 向数据库中插入数据
 * @param[in] keyPrefix: key前缀
 * @param[in] key: 数据键
 * @param[in] data: 数据值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseStoragePutData(const std::string &keyPrefix, const std::string &key, UbseByteBuffer *data)
{
    return 0;
}

/**
 * @brief 从数据库中查询数据
 * @param[in] keyPrefix: key前缀
 * @param[in] key: 数据键
 * @param[in,out] ctx: 上下文指针
 * @param[in] func: 数据处理函数，函数调用完后，代表单条数据处理完成，数据内存被释放
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseStorageQueryData(const std::string &keyPrefix, const std::string &key, void *ctx,
    UbseStorageDealDataFunc func)
{
    return 0;
}

/**
 * @brief 从数据库中删除数据
 * @param[in] keyPrefix: key前缀
 * @param[in] key: 数据键
 * @return RackResult, 成功返回0, 失败返回非0
 */
uint32_t UbseStorageDeleteData(const std::string &keyPrefix, const std::string &key)
{
    return 0;
}
}