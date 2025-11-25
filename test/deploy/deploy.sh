#!/bin/bash

# ========================
# UBSE 动态安装脚本（按优先级安装：ubs-engine 优先）
# 功能：自动扫描当前目录下的 .rpm 文件 → 提取包名 → 
#       优先安装 ubs-engine（主服务），再安装其余组件 → 启动 ubse 服务
# 支持灵活版本管理，不写死版本号
# 日志输出到控制台，失败时输出完整错误信息
# ========================

cd "$(dirname "$(readlink -f "$0")")"

set -euo pipefail  # 出错立即退出

echo "🚀 开始安装 UBSE 组件（按顺序：ubs-engine 优先）..."

# === 1. 查找当前目录下的所有 .rpm 文件 ===
echo "📁 正在查找当前目录下的 RPM 包..."
rpm_files=()
for file in *.rpm; do
    [[ -f "$file" ]] && rpm_files+=("$file")
done

if [[ ${#rpm_files[@]} -eq 0 ]]; then
    echo "❌ 当前目录下没有找到任何 .rpm 文件！请确保已放置 UBSE 相关 RPM 包。"
    exit 1
fi

echo "✅ 找到 ${#rpm_files[@]} 个 RPM 包：${rpm_files[*]}"

# === 2. 分类：主服务包 ubs-engine，其余为其他组件 ===
main_rpm=""
other_rpms=()

for rpm in "${rpm_files[@]}"; do
    if [[ "$rpm" == ubs-engine-*.rpm && "$rpm" != ubs-engine-debuginfo-*.rpm && "$rpm" != ubs-engine-client-* ]]; then
        main_rpm="$rpm"
    elif [[ "$rpm" != ubs-engine-debuginfo-*.rpm ]]; then
        # 排除 debuginfo 包（通常不需要安装）
        other_rpms+=("$rpm")
    fi
done

# 检查主服务包
if [[ -z "$main_rpm" ]]; then
    echo "❌ 错误：未找到主服务包 ubs-engine-*.rpm！无法继续安装。"
    exit 1
else
    echo "📌 检测到主服务包：$main_rpm"
fi

# === 3. 卸载已安装的旧版本（使用 yum remove，更安全）===
echo "🔍 正在检查并卸载已安装的旧版本..."

# 要卸载的包列表
ubse_packages=("ubs-engine" "ubs-engine-client-libs" "ubs-engine-client-devel")

# 构建待卸载的包名列表（只包含已安装的）
pkgs_to_remove=()
for pkg in "${ubse_packages[@]}"; do
    if rpm -q "$pkg" &>/dev/null; then
        pkgs_to_remove+=("$pkg")
        echo "📌 发现已安装的旧版本：$pkg"
    else
        echo "✅ 未检测到 $pkg，跳过卸载。"
    fi
done

# 如果没有需要卸载的包，跳过
if [[ ${#pkgs_to_remove[@]} -eq 0 ]]; then
    echo "✅ 所有旧版本均已清除，无需卸载。"
else
    echo "🔄 正在卸载旧版本：${pkgs_to_remove[*]}"
    if yum -y remove "${pkgs_to_remove[@]}"; then
        echo "✅ 旧版本卸载完成"
    else
        echo "❌ 卸载失败！可能存在依赖冲突或权限问题。"
        exit 1
    fi
fi

# === 4. 使用 yum 安装所有 RPM 包（禁用所有仓库，避免源错误）===
echo "📦 正在使用 yum 安装所有 RPM 包（禁用仓库，避免源错误）..."

# 排除 debuginfo 包
install_rpms=()
for rpm in "${rpm_files[@]}"; do
    if [[ "$rpm" != *debuginfo*.rpm ]]; then
        install_rpms+=("$rpm")
    fi
done

if [[ ${#install_rpms[@]} -eq 0 ]]; then
    echo "❌ 没有可安装的 RPM 包。"
    exit 1
fi

echo "➡️  即将安装：${install_rpms[*]}"

# 关键：--disablerepo=* 避免访问任何仓库
if yum -y install --nogpgcheck --disablerepo=* "${install_rpms[@]}"; then
    echo "✅ 所有 RPM 包安装成功"
else
    echo "❌ 安装失败！"
    exit 1
fi

# === 5. 启动 ubse 服务 ===
echo "⚙️ 正在启动 ubse.service..."
if systemctl start ubse.service; then
    echo "✅ ubse.service 启动成功"
else
    echo "❌ 启动 ubse.service 失败！"
    echo "👉 请运行以下命令查看具体原因："
    echo "    systemctl status ubse.service"
    echo "    journalctl -u ubse.service -n 50 --no-pager"
    exit 1
fi

# === 6. 验证服务状态 ===
if systemctl is-active ubse.service >/dev/null; then
    echo "🎉 所有操作完成！UBSE 已成功安装并运行。"
else
    echo "⚠️ 服务启动后状态异常，请检查配置或日志。"
    exit 1
fi
