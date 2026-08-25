#!/bin/bash

# 多节点集群证书生成脚本（IT集中签发模式）
# 三种模式：
# 1. 生成共享 CA（仅手动更新种子CA时使用）: cert.sh --ca-only <输出目录>
# 2. 签发服务器证书（写入共享CA数据库，供IT框架每节点调用）:
#      cert.sh --server-only -o <nodeId> --ca <CA目录> <输出目录>
# 3. 吊销证书并重新生成CRL: cert.sh --revoke <证书文件> --ca <CA目录>
#
# CA目录布局（由IT框架的InitSharedCaDir铺设）:
#   <CA目录>/CA/{cacert.pem, cakey.pem, ca.crl, index.txt, serial, crlnumber}
# 所有节点共用同一份数据库，签发序列号全局递增、记录入index.txt，
# 这是吊销（--revoke）的前提。节点最终只消费5个文件：
#   server.pem / server_key.pem / trust.pem / ca.crl / key_pwd.txt

set -e

# 默认值
OUTPUT_DIR="./certs"
OTHER_NAME_VALUE="testCertValue000"
CA_DIR=""
MODE=""
REVOKE_CERT=""

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --ca-only)
            MODE="ca-only"
            shift
            ;;
        --server-only)
            MODE="server-only"
            shift
            ;;
        --revoke)
            MODE="revoke"
            REVOKE_CERT="$2"
            shift 2
            ;;
        -o|--other-name)
            OTHER_NAME_VALUE="$2"
            shift 2
            ;;
        --ca)
            CA_DIR="$2"
            shift 2
            ;;
        -*)
            echo "未知选项: $1"
            exit 1
            ;;
        *)
            OUTPUT_DIR="$1"
            shift
            ;;
    esac
done

DAYS_VALID=3650
KEY_PASSWORD="huawei"

# 验证参数
if [[ -z "$MODE" ]]; then
    echo "错误: 必须指定模式 (--ca-only | --server-only | --revoke)"
    exit 1
fi

if [[ "$MODE" != "ca-only" && -z "$CA_DIR" ]]; then
    echo "错误: $MODE 需要指定 --ca 参数"
    exit 1
fi

if [[ "$MODE" != "ca-only" && ! -f "$CA_DIR/CA/cacert.pem" ]]; then
    echo "错误: CA 证书不存在: $CA_DIR/CA/cacert.pem"
    exit 1
fi

if [[ "$MODE" == "revoke" && ! -f "$REVOKE_CERT" ]]; then
    echo "错误: 待吊销的证书文件不存在: $REVOKE_CERT"
    exit 1
fi

# 生成指向共享CA目录的openssl ca配置（签发/吊销共用）
write_ca_cnf() {
    local cnf_dir=$1
    local ca_abs=$2
    mkdir -p "$cnf_dir"
    cat > "$cnf_dir/ca.cnf" << EOF
[ca]
default_ca = CA_default

[CA_default]
dir = $ca_abs/CA
database = \$dir/index.txt
serial = \$dir/serial
crlnumber = \$dir/crlnumber
certificate = \$dir/cacert.pem
private_key = \$dir/cakey.pem
new_certs_dir = \$dir/newcerts
crl = \$dir/ca.crl
default_md = sha256
policy = policy_any
default_days = $DAYS_VALID
default_crl_days = $DAYS_VALID
unique_subject = no

[policy_any]
countryName = supplied
stateOrProvinceName = supplied
organizationName = supplied
commonName = supplied
EOF
}

# 首次签发前初始化CA数据库（幂等）
init_ca_db() {
    local ca_abs=$1
    if [ ! -f "$ca_abs/CA/index.txt" ]; then
        echo "# 初始化 CA 数据库（index.txt/serial/crlnumber）"
        mkdir -p "$ca_abs/CA/newcerts"
        touch "$ca_abs/CA/index.txt"
        echo "01" > "$ca_abs/CA/serial"
        echo "01" > "$ca_abs/CA/crlnumber"
    fi
}

# openssl ca 对数据库(index.txt/serial)无内置锁。IT框架在集群启动时顺序
# 签发，但节点重启/并发签发没有上层同步——对 CA 数据库的所有读写
# （初始化/签发/吊销）必须在锁内执行，否则会产生重复序列号或损坏
# index.txt。用法: ca_locked <CA目录> <命令...>
ca_locked() {
    local ca_abs=$1
    shift
    mkdir -p "$ca_abs/CA"
    (
        flock -x 9
        "$@"
    ) 9>"$ca_abs/CA/.lock"
}

# ==========================================
# 模式1: 生成共享 CA
# ==========================================
if [[ "$MODE" == "ca-only" ]]; then
    echo "# 创建共享根 CA: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR/CA"

    cat > "$OUTPUT_DIR/ca.cnf" << EOF
[ca]
default_ca = CA_default

[CA_default]
dir = $OUTPUT_DIR/CA
database = \$dir/index.txt
serial = \$dir/serial
crlnumber = \$dir/crlnumber
certificate = \$dir/cacert.pem
private_key = \$dir/cakey.pem
new_certs_dir = \$dir/newcerts
crl = \$dir/ca.crl
default_md = sha256
policy = policy_any
default_days = $DAYS_VALID
default_crl_days = $DAYS_VALID

[policy_any]
countryName = supplied
stateOrProvinceName = supplied
organizationName = supplied
commonName = supplied

[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no

[req_distinguished_name]
C = CN
ST = Beijing
L = Beijing
O = UBSE-Cluster-CA
OU = Root-CA
CN = UBSE-Cluster-Root-CA

[v3_ca]
basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always, issuer
EOF

    mkdir -p "$OUTPUT_DIR/CA/newcerts"
    touch "$OUTPUT_DIR/CA/index.txt"
    echo "01" > "$OUTPUT_DIR/CA/serial"
    echo "01" > "$OUTPUT_DIR/CA/crlnumber"

    openssl genrsa -out "$OUTPUT_DIR/CA/cakey.pem" 4096
    openssl req -x509 -new -nodes -key "$OUTPUT_DIR/CA/cakey.pem" \
        -sha256 -days $DAYS_VALID \
        -config "$OUTPUT_DIR/ca.cnf" \
        -extensions v3_ca \
        -out "$OUTPUT_DIR/CA/cacert.pem"

    echo "# 生成 CRL（有效期 $DAYS_VALID 天）"
    openssl ca -gencrl -config "$OUTPUT_DIR/ca.cnf" \
        -crldays $DAYS_VALID \
        -out "$OUTPUT_DIR/CA/ca.crl"

    echo "共享 CA 生成完成: $OUTPUT_DIR/CA/{cacert.pem, cakey.pem, ca.crl}"
    echo "将这3个文件 + cert.sh 放入 test/IT/resource/ 作为种子CA"
    exit 0
fi

CA_ABS=$(cd "$CA_DIR" && pwd)

# ==========================================
# 模式2: 吊销证书并重新生成 CRL
# ==========================================
if [[ "$MODE" == "revoke" ]]; then
    echo "# 吊销证书: $REVOKE_CERT"
    write_ca_cnf "$OUTPUT_DIR" "$CA_ABS"

    ca_locked "$CA_ABS" init_ca_db "$CA_ABS"
    ca_locked "$CA_ABS" openssl ca -config "$OUTPUT_DIR/ca.cnf" -revoke "$REVOKE_CERT"

    echo "# 重新生成 CRL（有效期 $DAYS_VALID 天）"
    ca_locked "$CA_ABS" openssl ca -gencrl -config "$OUTPUT_DIR/ca.cnf" \
        -crldays $DAYS_VALID \
        -out "$CA_ABS/CA/ca.crl"

    echo "证书吊销完成，新 CRL: $CA_ABS/CA/ca.crl"
    echo "分发新 CRL 到各节点并重启后，被吊销节点的 TLS 握手将被拒绝"
    exit 0
fi

# ==========================================
# 模式3: 签发服务器证书（使用共享 CA）
# ==========================================
echo "# 签发服务器证书: nodeId=$OTHER_NAME_VALUE CA=$CA_DIR"
mkdir -p "$OUTPUT_DIR/server"

write_ca_cnf "$OUTPUT_DIR" "$CA_ABS"

# SAN配置：otherName/CN/DNS 携带节点身份（nodeId）
cat > "$OUTPUT_DIR/san.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
C = CN
ST = Beijing
L = Beijing
O = UBSE-Cluster
OU = Server
CN = ubse-server-$OTHER_NAME_VALUE

[v3_req]
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth, clientAuth
subjectKeyIdentifier = hash
subjectAltName = @alt_names

[alt_names]
otherName.1 = 1.3.6.1.4.1.2011.999.1;UTF8:$OTHER_NAME_VALUE
DNS.1 = localhost
DNS.2 = ubse-server-$OTHER_NAME_VALUE
IP.1 = 127.0.0.1

[v3_server]
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth, clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid, issuer
subjectAltName = @alt_names
EOF

echo "# 生成服务器私钥（带密码）"
openssl genrsa -aes256 -passout pass:$KEY_PASSWORD \
    -out "$OUTPUT_DIR/server/key.pem" 2048

echo "# 生成 CSR"
openssl req -new -key "$OUTPUT_DIR/server/key.pem" \
    -passin pass:$KEY_PASSWORD \
    -config "$OUTPUT_DIR/san.cnf" \
    -out "$OUTPUT_DIR/server/server.csr"

ca_locked "$CA_ABS" init_ca_db "$CA_ABS"

echo "# 使用共享 CA 签发（写入数据库，序列号全局唯一，flock保护）"
ca_locked "$CA_ABS" openssl ca -config "$OUTPUT_DIR/ca.cnf" \
    -in "$OUTPUT_DIR/server/server.csr" \
    -out "$OUTPUT_DIR/server/cert.pem" \
    -extensions v3_server \
    -extfile "$OUTPUT_DIR/san.cnf" \
    -batch \
    -notext

echo "$KEY_PASSWORD" > "$OUTPUT_DIR/key_pwd.txt"
chmod 600 "$OUTPUT_DIR/key_pwd.txt"

echo "# 验证证书链"
openssl verify -CAfile "$CA_ABS/CA/cacert.pem" "$OUTPUT_DIR/server/cert.pem"

echo "服务器证书生成完成: $OUTPUT_DIR/server/{cert.pem, key.pem}, 密码: $OUTPUT_DIR/key_pwd.txt"
