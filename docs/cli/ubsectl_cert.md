
## 1.导入证书

### 描述

向UBSE导入证书，用于节点间通信的认证与加密

### 用法

```shell
ubsectl import cert -c <ca-cert-file> -s <server-cert-file> -k <server-key-file> -l <ca-crl-file>
```

### 参数

| 参数                  | 说明                                     | 取值
| --------------------- | --------------------------------------- | -----
| -c/--ca-cert-file     | 必选参数，根证书文件完整路径及文件名       |字符串
| -s/--server-cert-file | 必选参数，身份证书文件完整路径及文件名     |字符串
| -k/--server-key-file  | 必选参数，身份证书私钥文件完整路径及文件名  |字符串
| -l/--ca-crl-file      | 可选参数，吊销证书列表文件完整路径及文件名  |字符串

### 约束限制

1. 只允许root和ubse用户使用，原始证书路径必须是ubsectl可访问
2. 导入可信机构颁发的证书时，需交互式输入私钥保护密码，密码不显示，输入完按Enter键确认即可，只支持回退键操作密码
3. 密码长度不能超过1000字符，不支持"\n"和"\b"等不可见字符
4. 文件路径只能是绝对路径（路径只允许大小写字母、数字、"-"、"_"、"."和"/"），不允许软硬链接
5. 文件大小不超过2m

### 输出信息说明

执行成功/失败

### 示例

```
$ ubsectl import cert -c /usr/cert/trust.pem -s /usr/cert/server.pem -k /usr/cert/server_key.pem -l /usr/cert/crl.pem
Enter certificate password: 
Certificates imported successfully
```


## 2.移除证书

### 描述

从UBSE中移除证书信息

### 用法

```shell
ubsectl remove cert
```
### 参数

无

### 约束限制

只允许root和ubse用户使用，原始证书路径必须是ubsectl可访问

### 输出信息说明

执行成功/失败

### 示例

```
$ ubsectl remove cert
Certificates removed successfully
```


## 3.更新证书吊销列表

### 描述

向UBSE更新吊销证书信息

### 用法

```shell
ubsectl change cert -l <ca-crl-file>
```

### 参数

| 参数                  | 说明                                 |  取值
| --------------------- | ----------------------------------- |-----
| -l/--ca-crl-file      | 必选参数，根证书文件完整路径及文件名    | 字符串

### 约束限制

1. 只允许root和ubse用户使用，原始证书路径必须是ubsectl可访问 
2. 文件路径只能是绝对路径（路径只允许大小写字母、数字、"-"、"_"、"."和"/"），不允许软硬链接
3. 文件大小不超过2m

### 输出信息说明

执行成功/失败

### 示例

```
$ ubsectl change cert -l /usr/cert/crl.pem
Certificate Revocation List changed successfully
```