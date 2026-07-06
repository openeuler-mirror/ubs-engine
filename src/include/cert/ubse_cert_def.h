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
#ifndef UBSE_SSL_CONFIG_PATH_H
#define UBSE_SSL_CONFIG_PATH_H
#include <string>
namespace ubse::cert {
/**
 * @brief 证书相关文件路径集合
 *
 * 调用方初始化时填充各路径，路径格式如 "/var/lib/ubse/lcne_cert/server.pem"。
 */
struct UbseCertPaths {
    std::string serverCertFile;  // 服务端证书文件路径
    std::string serverKeyFile;   // 服务端私钥文件路径
    std::string trustCertFile;   // 信任CA证书文件路径
    std::string crlFile;         // 证书吊销列表文件路径
    std::string passwordFile;    // 私钥密码文件路径
};
} // namespace ubse::cert
#endif
