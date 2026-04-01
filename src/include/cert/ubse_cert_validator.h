/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_SSL_VALIDATOR_H
#define UBS_ENGINE_UBSE_SSL_VALIDATOR_H

#include <openssl/err.h>
#include "src/framework/misc/ubse_secure_buffer.h"
namespace ubse::cert {
class UbseSslValidator {
public:
    /**
    * @brief 验证所有证书的有效性
    *
    * @return  返回验证结果，true表示所有证书都是有效的
    */
    static bool ValidateAll();

    /**
    * @brief 加载私钥的密码，用于后续获取私钥
    *
    * @param path 私钥密码路径
    * @return  返回一个保存敏感信息的结构体，存储着密码结果
    */
    static utils::SecureBuffer LoadPasswordFromFile(const char *path);

    /**
     * @brief 配置吊销列表，用于校验证书是否可信
     *
     * @param ctx SSL上下文
     * @return 配置吊销列表是否成功
     */
    static bool ConfigureCrlValidation(SSL_CTX *ctx);

    /**
     * @brief 检查对应路径文件是否完整
     *
     * @return 返回检查结果，true表示所需文件都存在
     */
    static bool CheckAllFileExist();

    /**
     * @brief 检查对应路径文件权限是否为600
     *
     * @return 返回检查结果，true表示所有文件权限合法
     */
    static bool CheckAllFilePermission600();

    /**
     * @brief 客户端配置吊销列表，用于校验服务的的证书是否可信
     *
     * @param ctx http client端的ssl ctx
     * @return 客户端配置吊销列表是否成功
     */
    static bool ConfigureClientCrlValidation(SSL_CTX *ctx);

private:
    static X509 *LoadAndValidateCert(const char *path, const char *name);
    static EVP_PKEY *LoadAndValidatePrivateKey(const char *keyPath, const char *password, const char *name);
    static X509_STORE *LoadAndValidateCaStore(const char *caPath);
    static bool VerifyCertAndKeyMatch(X509 *cert, EVP_PKEY *pkey, const char *certName, const char *keyName);
    static bool ValidateCRLIfExists();
};
} // namespace ubse::cert
#endif // UBS_ENGINE_UBSE_SSL_VALIDATOR_H