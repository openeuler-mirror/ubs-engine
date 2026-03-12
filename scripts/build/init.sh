#!/bin/bash

# ***********************************************************************
# Copyright: (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
# script for init dev env for ubs-engine
# ***********************************************************************

set -e

# 定义颜色
GREEN='\033[0;32m'  # 绿色
RED='\033[0;31m'    # 红色
YELLOW='\033[1;33m' # 黄色
NC='\033[0m'        # 无颜色

error() {
    echo -e "${RED}$1${NC}"
}

warning() {
    echo -e "${YELLOW}$1${NC}"
}

success() {
    echo -e "${GREEN}$1${NC}"
}

# 检测当前的 shell
get_rc_file() {
    local current_shell=$(basename "$SHELL")
    case "$current_shell" in
        bash)
            echo "$HOME/.bashrc"
            ;;
        zsh)
            echo "$HOME/.zshrc"
            ;;
        fish)
            echo "$HOME/.config/fish/config.fish"
            ;;
        *)
            echo "Unsupported shell: $current_shell"
            exit 1
            ;;
    esac
}

# 安装 clang-format 和 clang-tidy
install_clang_tools() {
    echo "Installing clang-format and clang-tidy..."
    sudo ln -sf /usr/bin/clang-format-15 /usr/bin/clang-format
    echo "Created symlink for clang-format."
    sudo ln -sf /usr/bin/clang-tidy-15 /usr/bin/clang-tidy
    echo "Created symlink for clang-tidy."
    success "clang-format and clang-tidy installation completed."
}

# 安装 pre-commit
install_pre_commit() {
    local domain="cmc-cd-mirror.rnd.huawei.com"
    local pypi_mirror="http://$domain/pypi/simple/"

    echo "Installing pre-commit..."
    pip3 install pre-commit --index-url "$pypi_mirror" --trusted-host "$domain"
    success "pre-commit installed successfully."
    pre-commit install
    success "pre-commit hooks installed."
}

# 安装工具链
install_toolchain() {
    local toolchain_path="/opt/toolchain-aarch64.tar.gz"

    # 检测是否为 aarch64 环境
    if [ "$(uname -m)" != "aarch64" ]; then
        error "This script is intended to run on an aarch64 environment."
        exit 1
    fi

    if [ ! -f "$toolchain_path" ]; then
        error "File $toolchain_path does not exist."
        warning "Please visit the following link to download the toolchain and upload it to $toolchain_path:"
        warning "Download link: [https://onebox.huawei.com/v/fbe81b35f53885071a111e3eb66505c2]"
        warning "Please ensure to upload the file to the /opt directory."
        exit 1
    else
        success "Toolchain file $toolchain_path already exists."
        echo "Extracting the toolchain..."
        sudo tar -xzvf "$toolchain_path" -C /opt/

        echo "Running the setup script..."
        sudo /opt/toolchain-aarch64/setup
        sudo rm -rf /opt/toolchain-aarch64
    fi
}

# 更新环境变量的通用函数
update_env_var() {
    local var_name="$1"
    local var_value="$2"
    local rc_file="$3"
    local export_statement="export $var_name=\"$var_value\""

    if ! grep -q "$export_statement" "$rc_file"; then
        echo "$export_statement" >> "$rc_file"
        success "$export_statement has been added to $rc_file"
        warning "Please run 'source $rc_file' to apply the changes."
    else
        success "$export_statement already exists in $rc_file"
    fi
}

# 更新 PATH 的通用函数
update_path() {
    local new_path="$1"
    local rc_file="$2"
    local export_statement="export PATH=\"$new_path:\$PATH\""

    if ! grep -q "$export_statement" "$rc_file"; then
        echo "$export_statement" >> "$rc_file"
        success "$export_statement has been added to $rc_file"
        warning "Please run 'source $rc_file' to apply the changes."
    else
        success "$export_statement already exists in $rc_file"
    fi
}

# 更新 PIP PATH
update_pip_path() {
    # root 用户不需要更新 pip 路径
    if [ "$EUID" -ne 0 ]; then
        update_path "$HOME/.local/bin" "$rc_file"
    fi
}

# 更新 JAVA_HOME
update_java_home() {
    local java_home="/opt/buildtools/bisheng_jdk8_enterprise-202.1.0.b011"
    update_env_var "JAVA_HOME" "$java_home" "$rc_file"
    update_path "\$JAVA_HOME/bin" "$rc_file"
}

# 主函数
main() {
    rc_file=$(get_rc_file)

    install_toolchain
    install_clang_tools
    install_pre_commit

    update_java_home
    update_pip_path
}

# 执行主函数
main
