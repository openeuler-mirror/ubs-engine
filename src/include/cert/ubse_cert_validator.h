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
#include "src/include/cert/ubse_cert_def.h"
namespace ubse::cert {
class UbseSslValidator {
public:
    /**
    * @brief 构造函数，传入证书相关文件路径
    *
    * @param paths 证书文件路径集合，文件路径作为成员变量保存
    */
    explicit UbseSslValidator(UbseCertPaths paths);

    /**
    * @brief 验证所有证书的有效性
    *
    * @return  返回验证结果，true表示所有证书都是有效的
    */
    bool ValidateAll();

    /**
    * @brief 加载私钥的密码，用于后续获取私钥
    *
    * @return  返回一个保存敏感信息的结构体，存储着密码结果
    */
    utils::SecureBuffer LoadPassword();

    /**
     * @brief 配置吊销列表，用于校验证书是否可信
     *
     * @param ctx SSL上下文
     * @return 配置吊销列表是否成功
     */
    bool ConfigureCrlValidation(SSL_CTX *ctx);

    /**
     * @brief 检查对应路径文件是否完整
     *
     * @return 返回检查结果，true表示所需文件都存在
     */
    bool CheckAllFileExist();

private:
    X509 *LoadAndValidateCert(const char *path, const char *name);
    EVP_PKEY *LoadAndValidatePrivateKey(const char *keyPath, const char *password, const char *name);
    X509_STORE *LoadAndValidateCaStore(const char *caPath);
    bool VerifyCertAndKeyMatch(X509 *cert, EVP_PKEY *pkey, const char *certName, const char *keyName);
    bool ValidateCRLIfExists();

    UbseCertPaths paths_;
};
} // namespace ubse::cert
#endif // UBS_ENGINE_UBSE_SSL_VALIDATOR_H
