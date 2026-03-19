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
namespace UbseSSLConfig {
inline constexpr const char *ServerCertFile = "/var/lib/ubse/lcne_cert/server.pem";
inline constexpr const char *TrustCertFile = "/var/lib/ubse/lcne_cert/trust.pem";
inline constexpr const char *CrlFile = "/var/lib/ubse/lcne_cert/ca.crl";
inline constexpr const char *ServerKeyFile = "/var/lib/ubse/lcne_cert/server_key.pem";
inline constexpr const char *PasswordFile = "/var/lib/ubse/lcne_cert/key_pwd.txt";
} // namespace UbseSSLConfig
#endif
