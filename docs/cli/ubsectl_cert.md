
## 1.导入证书

### 描述

向UBSE导入证书，用于节点间通信的认证与加密

### 用法

```shell
ubsectl import cert -c <ca-cert-file> -s <server-cert-file> -k <server-key-file> -l <crl-file> password
```

### 参数

| 参数                  | 说明                                     | 取值
| --------------------- | --------------------------------------- | -----
| -c/--ca-cert-file     | 必选参数，根证书文件完整路径及文件名       |字符串
| -s/--server-cert-file | 必选参数，身份证书文件完整路径及文件名     |字符串
| -k/--server-key-file  | 必选参数，身份证书私钥文件完整路径及文件名  |字符串
| -l/--ca-crl-file      | 可选参数，吊销证书列表文件完整路径及文件名  |字符串
| password              | 必选参数，交互式输入，身份证书私钥保护密码  |字符串

### 约束限制

原始证书路径必须是ubsectl可访问

### 输出信息说明

执行成功/失败

### 示例

```
$ ubsectl import cert -c /usr/cert/trust.pem -s /usr/cert/server.pem -k /usr/cert/server_key.pem -l /usr/cert/crl.pem
Enter certificate password: 
Success import cert files
```