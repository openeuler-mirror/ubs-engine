set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." >/dev/null 2>&1 && pwd)"
SKIP_BUILD=false
[[ "${1:-}" == "--skip-build" ]] && SKIP_BUILD=true

echo "==> 检查操作系统"
if ! grep -qi "openEuler" /etc/openEuler-release 2>/dev/null; then
    echo "错误：仅支持 openEuler 操作系统（构建指导 2.1）。当前系统：$(cat /etc/openEuler-release 2>/dev/null || echo 未知)"
    exit 1
fi
cat /etc/openEuler-release

echo "==> 安装基础工具链"
dnf install -y gcc gcc-c++ make cmake git python3 python3-pip 'dnf-command(builddep)'

cd "${REPO_ROOT}"

if dnf list ubs-comm-devel >/dev/null 2>&1; then
    echo "==> 安装构建依赖（spec BuildRequires）"
    dnf builddep -y ubs-engine.spec
else
    echo "==> 仓库无 ubs-comm-devel，按构建指导 2.1.2 源码编译 ubs-comm"
    dnf install -y rpm-build rpmdevtools
    mkdir -p /tmp/src
    [ -d /tmp/src/ubs-comm ] || git clone -b openEuler-24.03-LTS-SP4 --depth 1 https://gitcode.com/src-openeuler/ubs-comm.git /tmp/src/ubs-comm
    rpmdev-setuptree
    cp /tmp/src/ubs-comm/ubs-comm.spec ~/rpmbuild/SPECS/
    cp -rf /tmp/src/ubs-comm/ubs-comm-*.tar.gz ~/rpmbuild/SOURCES/
    dnf builddep -y ~/rpmbuild/SPECS/ubs-comm.spec
    rpmbuild -ba ~/rpmbuild/SPECS/ubs-comm.spec
    dnf install -y ~/rpmbuild/RPMS/"$(uname -m)"/ubs-comm-lib-*.rpm ~/rpmbuild/RPMS/"$(uname -m)"/ubs-comm-devel-*.rpm
    echo "==> 安装其余构建依赖"
    dnf install -y libxml2-devel openssl-devel cpp-httplib-devel rapidjson-devel numactl-libs ninja-build libvirt-devel kernel-devel systemd-devel python3-setuptools util-linux-user patch bc glibc-devel libstdc++-devel
fi

echo "==> 安装单元测试依赖"
dnf install -y gtest gtest-devel gmock gmock-devel numactl-devel

if [ "${SKIP_BUILD}" = "true" ]; then
    echo "==> 环境配置完成（已跳过构建）"
else
    echo "==> 构建 ubs-engine（Release）"
    bash build.sh
    echo "==> 安装 SDK"
    cmake --install cmake-build-release --component ubse_sdk --prefix /usr
fi

echo "==> 完成。常用命令：bash build.sh（构建）、bash build.sh ut（单元测试）、bash build.sh package（打包 RPM）"
