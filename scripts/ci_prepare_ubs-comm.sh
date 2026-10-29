#!/bin/bash
set -euo pipefail

# ci流水线构建使用，ubs-engine编译环境依赖ubs-comm
# 说明：
#   - 需要提前在环境上下载好ubs-comm的源码
#   - 将编译产物ubs-comm-lib和ubs-comm-devel安装在环境上

os_type=$(uname -i)
mkdir -p ~/rpmbuild/SPECS ~/rpmbuild/SOURCES/
cp /tmp/src/ubs-comm/ubs-comm.spec ~/rpmbuild/SPECS/
cp -rf /tmp/src/ubs-comm/ubs-comm-*.tar.gz ~/rpmbuild/SOURCES/
sudo dnf install -y 'dnf-command(builddep)'
sudo dnf builddep -y ~/rpmbuild/SPECS/ubs-comm.spec
rpmbuild -ba ~/rpmbuild/SPECS/ubs-comm.spec
sudo dnf install -y ~/rpmbuild/RPMS/${os_type}/ubs-comm-lib-*.rpm
sudo dnf install -y ~/rpmbuild/RPMS/${os_type}/ubs-comm-devel-*.rpm
echo "[INFO]: ubs-comm install success!"