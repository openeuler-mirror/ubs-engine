# IT 测试证书资源

本目录存放 IT 框架 TLS 证书场景的种子资源，由 `ItClusterBuilder::WithCerts()`
消费（默认即指向本目录），供集群内各节点集中签发服务器证书。

## 文件清单

| 文件 | 用途 |
|------|------|
| `cacert.pem` | 根 CA 证书（部署到节点后作为 `trust.pem` 信任锚） |
| `cakey.pem` | 根 CA 私钥（仅用于在共享 CA 目录签发服务器证书） |
| `ca.crl` | 证书吊销列表（CRL，初始为空表，支持吊销场景） |
| `cert.sh`  | 证书生成脚本（签发 / 吊销 / 重新生成 CA） |

## ⚠️ 仅供测试

`cakey.pem` 是随仓库分发的测试专用 CA 私钥（O=UBSE-Cluster-CA）。
**严禁用于任何生产环境或真实业务系统。** CA 证书及 CRL 有效期为 10 年，
过期后可按下方说明重新生成并覆盖本目录。

## 工作方式

IT 框架启动集群时，会把本目录的种子资源铺设到运行时共享 CA 目录
（`workDir/cert_authority/`），各节点在共享数据库上用同一 CA 签发
各自的服务器证书（序列号全局唯一、记录入 `index.txt`），
再通过 `ubsectl import cert` 生产路径导入节点证书目录。

## 手动操作（更新种子 CA / 排查问题）

```bash
# 重新生成整套测试 CA（10 年有效期），输出到 /tmp 避免弄脏仓库
bash cert.sh --ca-only /tmp/newca

# 为节点签发证书（--ca 指向共享 CA 目录）
bash cert.sh --server-only -o <nodeId> --ca <CA目录> <输出目录>

# 吊销某张证书并重新生成 CRL
bash cert.sh --revoke <证书文件> --ca <CA目录> <输出目录>
```

注意：cert.sh 不带输出目录参数时默认写到 `./certs`（已加入 .gitignore），
建议显式传 `/tmp` 下的路径。
