#!/bin/bash
#
# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
#
# scripts/build-deb.sh
# Build 4 Debian packages from a single build, mimicking RPM spec behavior.
# Compatible with ARM64 and /usr/lib64 layout.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."
DEB_ROOT="$PROJECT_ROOT/deb"
STAGING="$DEB_ROOT/common"

VERSION="1.0.0"
RELEASE="1"
FULL_VERSION="${VERSION}-${RELEASE}"
ARCH="arm64"

# 自动获取当前系统的 multiarch 目录名（如 aarch64-linux-gnu）
DEB_HOST_MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null)
if [ -z "$DEB_HOST_MULTIARCH" ]; then
    echo "Warning: dpkg-architecture not found, falling back to aarch64-linux-gnu"
    DEB_HOST_MULTIARCH="aarch64-linux-gnu"
fi

# 定义 libdir 变量
LIBDIR="usr/lib/$DEB_HOST_MULTIARCH"

# --- Step 0: Download third-party source code ---
echo ">>> Downloading third-party source code..."

# Function to clone or update a git repo into source dir
download_source() {
    local lib_name="$1"
    local repo_url="$2"
    local branch_or_tag="$3"
    local dest_dir="$PROJECT_ROOT/3rdparty/${lib_name}/source"

    echo "  -> $lib_name: $repo_url ($branch_or_tag)"

    if [ -d "$dest_dir/.git" ]; then
        # Already cloned, try to update
        (cd "$dest_dir" && git fetch origin && git checkout "$branch_or_tag")
    else
        # Clone fresh
        [ -n "$dest_dir" ] && rm -rf "$dest_dir"
        mkdir -p "$(dirname "$dest_dir")"
        git clone --branch "$branch_or_tag" --depth 1 "$repo_url" "$dest_dir"
    fi

    # === 无论新 clone 还是 update，都要同步 submodule ===
    if [ "$lib_name" = "hcom" ] && [ -f "$dest_dir/.gitmodules" ]; then
        echo "      Initializing submodules for $lib_name..."
        (cd "$dest_dir" && git submodule update --init --recursive --depth 1)
    fi
}

# === 配置三方库列表 ===
# 格式: download_source <name> <git-url> <branch/tag>

# 如果 is_build_project 为 "false",不在CI环境,则下载源码
if [ "${is_build_project:-false}" == "false" ]; then
    download_source "hcom" "https://szv-y.codehub.huawei.com/BeiMing/service_domain/matrix_core/matrix_comm/hcom.git" "br_noncom_beiming_25.3.T1_tencent_20251231"
    download_source "cpp-httplib" "https://szv-open.codehub.huawei.com/OpenSourceCenter/yhirose/cpp-httplib.git" "v0.18.7-h2"
    download_source "mxml" "https://szv-open.codehub.huawei.com/OpenSourceCenter/michaelrsweet/mxml.git" "v4.0.3"
    download_source "rapidjson" "https://szv-open.codehub.huawei.com/OpenSourceCenter/Tencent/rapidjson.git" "6089180ecb704cb2b136777798fa1be303618975"
    download_source "securec" "https://codehub-dg-y.huawei.com/hwsecurec_group/huawei_secure_c.git" "tag_Huawei_Secure_C_V100R001C01SPC017B001_00001"
fi

echo ">>> Third-party sources downloaded."

# --- Step 1: Build and install to staging (like RPM %install) ---
echo ">>> Building project and installing to staging..."
[ -n "$STAGING" ] && rm -rf "$STAGING"
mkdir -p "$STAGING"

cd "$PROJECT_ROOT"

# Build
bash build.sh 3rdparty -S
bash build.sh -S -c

# Install main binaries
install -Dm755 cmake-build-release/bin/ubse "$STAGING/usr/bin/ubse"
install -Dm755 cmake-build-release/bin/ubsectl "$STAGING/usr/bin/ubsectl"

# Systemd
# === 安装并修改 systemd service 文件 ===
SERVICE_FILE="$STAGING/usr/lib/systemd/system/ubse.service"
install -Dm644 scripts/rpm/ubse.service "$SERVICE_FILE"
# 替换 LD_LIBRARY_PATH 中的 /usr/lib64 为实际的 multiarch 路径
sed -i "s|/usr/lib64/ubse|/usr/lib/${DEB_HOST_MULTIARCH}/ubse|g" "$SERVICE_FILE"

# Configs
install -Dm644 cmake-build-release/conf/ubse.conf "$STAGING/etc/ubse/ubse.conf"
install -Dm644 cmake-build-release/conf/ubse_auth_default.conf "$STAGING/etc/ubse/ubse_auth_default.conf"
install -Dm644 cmake-build-release/conf/ubse_plugin_admission.conf "$STAGING/etc/ubse/ubse_plugin_admission.conf"
install -Dm644 src/addons/virt_agent/conf/plugin_vm.conf "$STAGING/etc/ubse/plugins/plugin_vm.conf"
install -Dm644 src/addons/virt_agent/conf/auth-virtagent.conf "$STAGING/etc/ubse/plugins/auth-virtagent.conf"

# Bash completion
install -Dm644 scripts/command_completion/cli_commands.sh "$STAGING/etc/bash_completion.d/cli_commands.sh"

# Version file
install -Dm644 cmake-build-release/VERSION "$STAGING/usr/share/ubse/VERSION"

# Internal libs (in /${LIBDIR}/ubse/)
install -Dm755 cmake-build-release/lib/libhcom.so "$STAGING/${LIBDIR}/ubse/libhcom.so"
install -Dm755 cmake-build-release/lib/libsecurec.so "$STAGING/${LIBDIR}/ubse/libsecurec.so"

# Virtagent libs (in /${LIBDIR}/)
install -Dm755 cmake-build-release/lib/libvm.so "$STAGING/${LIBDIR}/libvm.so"
install -Dm755 cmake-build-release/lib/libstrategy.so "$STAGING/${LIBDIR}/libstrategy.so"
install -Dm755 cmake-build-release/lib/libubse-client.so.1.0.0 "$STAGING/${LIBDIR}/libubs-virt-agent.so.1.0.0"

# Client libs
install -Dm755 cmake-build-release/lib/libubse-client.so.1.0.0 "$STAGING/${LIBDIR}/libubse-client.so.1.0.0"
install -Dm644 cmake-build-release/lib/libubse-client.a "$STAGING/${LIBDIR}/libubse-client.a"

# Headers
mkdir -p "$STAGING/usr/include/ubse"
cp -r src/include/* "$STAGING/usr/include/ubse/"
mkdir -p "$STAGING/usr/include/virtagent"
cp -r src/addons/virt_agent/sdk/include/* "$STAGING/usr/include/virtagent/"
cmake --install cmake-build-release \
    --component ubse_sdk \
    --prefix "$STAGING/usr"

# Python SDK
python3 setup.py install --root="$STAGING" --prefix=/usr

# --- Step 2: Helper function to build a deb package ---
build_deb() {
    local pkg_name="$1"
    local pkg_dir="$DEB_ROOT/$pkg_name"
    local deb_file="$DEB_ROOT/${pkg_name}_${FULL_VERSION}_${ARCH}.deb"

    if [[ "$pkg_name" == python3-ubs-engine ]]; then
        local deb_arch="all"
        deb_file="$DEB_ROOT/${pkg_name}_${FULL_VERSION}_all.deb"
    else
        local deb_arch="$ARCH"
    fi

    echo ">>> Building $pkg_name ($deb_arch)..."

    [ -n "$pkg_dir" ] && rm -rf "$pkg_dir"
    mkdir -p "$pkg_dir/DEBIAN"

    # Copy control file
    if [ -f "$DEB_ROOT/control/$pkg_name" ]; then
        # Old style: single control file
        cp "$DEB_ROOT/control/$pkg_name" "$pkg_dir/DEBIAN/control"
    elif [ -d "$DEB_ROOT/control/$pkg_name" ]; then
        # New style: directory with control + scripts
        cp "$DEB_ROOT/control/$pkg_name/control" "$pkg_dir/DEBIAN/control"

        # Copy maintainer scripts with correct permissions
        for script in preinst postinst prerm postrm; do
            if [ -f "$DEB_ROOT/control/$pkg_name/$script" ]; then
                install -Dm755 "$DEB_ROOT/control/$pkg_name/$script" "$pkg_dir/DEBIAN/$script"
            fi
        done
    else
        echo "ERROR: No control file or directory for $pkg_name" >&2
        exit 1
    fi

    # Special handling per package
    case "$pkg_name" in
        ubs-engine)
            # Binaries
            install -Dm755 "$STAGING/usr/bin/ubse" "$pkg_dir/usr/bin/ubse"
            install -Dm755 "$STAGING/usr/bin/ubsectl" "$pkg_dir/usr/bin/ubsectl"
            # Systemd
            install -Dm644 "$STAGING/usr/lib/systemd/system/ubse.service" "$pkg_dir/usr/lib/systemd/system/ubse.service"
            # Configs
            install -Dm644 "$STAGING/etc/ubse/ubse.conf" "$pkg_dir/etc/ubse/ubse.conf"
            install -Dm644 "$STAGING/etc/ubse/ubse_plugin_admission.conf" "$pkg_dir/etc/ubse/ubse_plugin_admission.conf"
            install -Dm644 "$STAGING/etc/ubse/ubse_auth_default.conf" "$pkg_dir/etc/ubse/ubse_auth_default.conf"
            # Completion
            install -Dm644 "$STAGING/etc/bash_completion.d/cli_commands.sh" "$pkg_dir/etc/bash_completion.d/cli_commands.sh"
            # Version
            install -Dm644 "$STAGING/usr/share/ubse/VERSION" "$pkg_dir/usr/share/ubse/VERSION"
            # Internal libs
            install -Dm755 "$STAGING/${LIBDIR}/ubse/libhcom.so" "$pkg_dir/${LIBDIR}/ubse/libhcom.so"
            install -Dm755 "$STAGING/${LIBDIR}/ubse/libsecurec.so" "$pkg_dir/${LIBDIR}/ubse/libsecurec.so"
            ;;

        ubs-engine-client-libs)
            install -Dm755 "$STAGING/usr/lib64/libubse-client.so.1.0.0" "$pkg_dir/${LIBDIR}/libubse-client.so.1.0.0"
            ln -sf libubse-client.so.1.0.0 "$pkg_dir/${LIBDIR}/libubse-client.so.1"
            ;;

        ubs-engine-client-dev)
            mkdir -p "$pkg_dir/usr/include"
            install -Dm755 "$STAGING/usr/lib64/libubse-client.so" "$pkg_dir/${LIBDIR}/libubse-client.so"
            install -Dm644 "$STAGING/usr/lib64/libubse-client.a" "$pkg_dir/${LIBDIR}/libubse-client.a"
            # 创建开发用软链接: libubse-client.so -> libubse-client.so.1
            ln -sf libubse-client.so.1 "$pkg_dir/${LIBDIR}/libubse-client.so"

            cp -r "$STAGING/usr/include/ubse" "$pkg_dir/usr/include/ubse"
            ;;

        python3-ubs-engine)
            # Find Python site-packages
            PYTHON_SITE=$(find "$STAGING/usr/lib" -name "site-packages" -type d | head -n1)
            if [ -z "$PYTHON_SITE" ]; then
                echo "ERROR: Python site-packages not found!" >&2
                exit 1
            fi
            REL_SITE="${PYTHON_SITE#$STAGING}"
            mkdir -p "$pkg_dir$REL_SITE"

            # Copy Python module and egg-info
            cp -r "$PYTHON_SITE/ubse" "$pkg_dir$REL_SITE/"
            cp -r "$PYTHON_SITE/ubse-"*.egg-info "$pkg_dir$REL_SITE/" 2>/dev/null || true

            # === 关键：清理 .pyc 和 __pycache__ ===
            # 删除所有 .pyc 文件
            find "$pkg_dir$REL_SITE/ubse" -type f -name "*.pyc" -delete
            # 删除所有 __pycache__ 目录（包括子目录中的）
            find "$pkg_dir$REL_SITE/ubse" -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
            ;;
          ubs-engine-virtagent)
            # Virtagent libs
            install -Dm755 "$STAGING/${LIBDIR}/libvm.so" "$pkg_dir/${LIBDIR}/libvm.so"
            install -Dm755 "$STAGING/${LIBDIR}/libstrategy.so" "$pkg_dir/${LIBDIR}/libstrategy.so"
            install -Dm755 "$STAGING/${LIBDIR}/libubs-virt-agent.so.1.0.0" "$pkg_dir/${LIBDIR}/libubs-virt-agent.so.1.0.0"
            ln -sf libubs-virt-agent.so.1.0.0 "$pkg_dir/${LIBDIR}/libubs-virt-agent.so.1"
            ln -sf libubs-virt-agent.so.1 "$pkg_dir/${LIBDIR}/libubs-virt-agent.so"

            install -Dm644 "$STAGING/etc/ubse/plugins/plugin_vm.conf" "$pkg_dir/etc/ubse/plugins/plugin_vm.conf"
            install -Dm644 "$STAGING/etc/ubse/plugins/auth-virtagent.conf" "$pkg_dir/etc/ubse/plugins/auth-virtagent.conf"
            mkdir -p "$pkg_dir/usr/include"
            cp -r "$STAGING/usr/include/virtagent"  "$pkg_dir/usr/include/virtagent"
            ;;
    esac

    # Fix permissions
    find "$pkg_dir" -type f -name "*.so*" -exec chmod 755 {} \;
    find "$pkg_dir" -type f -name "*.a" -exec chmod 644 {} \;

    # Build deb
    dpkg-deb --build "$pkg_dir" "$deb_file"
    echo ">>> Built: $deb_file"
}

# --- Step 3: Build all packages ---
build_deb "ubs-engine"
build_deb "ubs-engine-client-libs"
build_deb "ubs-engine-client-dev"
build_deb "python3-ubs-engine"
build_deb "ubs-engine-virtagent"

echo "All .deb packages built in $DEB_ROOT/"