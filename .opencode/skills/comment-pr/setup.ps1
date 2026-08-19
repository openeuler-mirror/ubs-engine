# PR Review Skill - Setup script for Windows (PowerShell)
# Reads GitCode personal access token and persists it to the USER-LEVEL
# environment variable (Windows registry), so ALL new processes (including
# AI terminals / CI / non-interactive shells) inherit it automatically
# without needing to source the PowerShell profile.

$ErrorActionPreference = "Stop"

$EnvVar = "GITCODE_TOKEN"

Write-Host "=== PR Review Skill Setup (Windows) ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "GitCode 访问令牌获取方式：GitCode 个人主页 -> 访问令牌 -> 新建访问令牌"
Write-Host ""

# 如果用户级环境变量已存在，提示是否覆盖
$existingToken = [Environment]::GetEnvironmentVariable($EnvVar, 'User')
if (-not [string]::IsNullOrWhiteSpace($existingToken)) {
    Write-Host "检测到用户级环境变量 $EnvVar 已存在配置。" -ForegroundColor Yellow
    $overwrite = Read-Host "是否覆盖？(y/N)"
    if ($overwrite -notin @("y", "Y")) {
        Write-Host "已取消。"
        exit 0
    }
}

$token = Read-Host "请输入 GitCode 访问令牌" -AsSecureString
$plainToken = [System.Net.NetworkCredential]::new("", $token).Password
if ([string]::IsNullOrWhiteSpace($plainToken)) {
    Write-Host "错误：令牌不能为空。" -ForegroundColor Red
    exit 1
}

# 持久化到用户级环境变量（注册表），所有新进程自动继承，无需 source profile
[Environment]::SetEnvironmentVariable($EnvVar, $plainToken, 'User')
# 同时写入当前会话，便于本次立即生效
Set-Item -Path "Env:$EnvVar" -Value $plainToken

Write-Host ""
Write-Host "✅ 已持久化到用户级环境变量 $EnvVar (User 作用域)" -ForegroundColor Green
Write-Host ""
Write-Host "下一步："
Write-Host "  1. 新开终端自动生效（已开启的终端需重开或手动赋值）"
Write-Host "  2. 验证:  `$env:GITCODE_TOKEN"
Write-Host "  3. 运行 skill: /comment-pr <PR号或链接>"
