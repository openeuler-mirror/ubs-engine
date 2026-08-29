#!/usr/bin/env bash
# PR Review Skill - Setup script for Linux/macOS
# Reads GitCode personal access token from user input and stores it in
# ~/.config/gitcode/token.env (mode 600). The shell profile only gets a
# source reference line, so the token never sits in a world-readable file.

set -e

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_VAR="GITCODE_TOKEN"
PROFILE_FILE="$HOME/.bashrc"
[ -n "$ZSH_VERSION" ] && PROFILE_FILE="$HOME/.zshrc"
[ -f "$HOME/.bash_profile" ] && [ ! -f "$HOME/.bashrc" ] && PROFILE_FILE="$HOME/.bash_profile"
TOKEN_FILE="$HOME/.config/gitcode/token.env"

echo "=== PR Review Skill Setup (Linux/macOS) ==="
echo
echo "GitCode 访问令牌获取方式：GitCode 个人主页 -> 访问令牌 -> 新建访问令牌"
echo

# 如果已经存在，提示是否覆盖（含旧版本写入 profile 的明文配置，便于升级清理）
PROFILE_EXISTING=$(grep "^export $ENV_VAR=" "$PROFILE_FILE" 2>/dev/null || true)
if [ -n "$PROFILE_EXISTING" ] || [ -f "$TOKEN_FILE" ]; then
    echo "检测到已存在 $ENV_VAR 配置（profile 或 $TOKEN_FILE）。"
    read -p "是否覆盖？(y/N) " -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "已取消。"
        exit 0
    fi
    # 删除旧版本写入 profile 的明文配置（含注释行），并清理含明文的 sed 备份
    if [ -n "$PROFILE_EXISTING" ]; then
        sed -i.bak -e "/^export $ENV_VAR=/d" -e "/^# PR Review Skill - GitCode access token$/d" "$PROFILE_FILE"
        rm -f "$PROFILE_FILE.bak"
        echo "已删除 profile 中的旧配置。"
    fi
fi

read -sp "请输入 GitCode 访问令牌: " TOKEN
echo
if [ -z "$TOKEN" ]; then
    echo "错误：令牌不能为空。"
    exit 1
fi
if ! [[ "$TOKEN" =~ ^[A-Za-z0-9._-]+$ ]]; then
    echo "错误：令牌含有非法字符（仅允许字母、数字、点、下划线、连字符），已取消写入。"
    exit 1
fi

# 写入独立的 600 权限令牌文件，profile 中只保留引用
mkdir -p "$(dirname "$TOKEN_FILE")"
(
    umask 077
    cat > "$TOKEN_FILE" <<EOF
# PR Review Skill - GitCode access token
export $ENV_VAR="$TOKEN"
EOF
)
chmod 600 "$TOKEN_FILE"

# profile 中仅追加一行引用（幂等）
grep -q "gitcode/token.env" "$PROFILE_FILE" 2>/dev/null || \
    echo '[ -f "$HOME/.config/gitcode/token.env" ] && . "$HOME/.config/gitcode/token.env"' >> "$PROFILE_FILE"

echo
echo "✅ 令牌已写入 $TOKEN_FILE (权限 600)，$PROFILE_FILE 已添加引用"
echo
echo "下一步："
echo "  1. 重新加载 shell:  source $PROFILE_FILE"
echo "  2. 验证:  echo \$GITCODE_TOKEN"
echo "  3. 运行 skill: /comment-pr <PR号或链接>"
