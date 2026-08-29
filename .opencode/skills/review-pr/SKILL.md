---
name: review-pr
description: Review openEuler repo PR with C/C++ code review and security scan.
---

# PR Review Skill

输入参数为 PR 号（如 `157`），如果没有入参则直接退出，并提示输入。

## PR 链接解析规则

PR 前缀从当前仓库 `git remote get-url origin` 自动推导，无需配置。推导规则：去掉 remote url 的 `.git` 后缀，拼接 `/pull/`。
- `https://gitcode.com/openeuler/ubs-engine.git` → `https://gitcode.com/openeuler/ubs-engine/pull/`
- `git@gitcode.com:openeuler/ubs-engine.git` → 转 HTTPS 后同上

## 配置说明

访问令牌（gitcode-token）通过环境变量 `GITCODE_TOKEN` 读取，不存放在仓库内。
首次使用请运行 setup 脚本（Linux/macOS: `bash setup.sh`，Windows: `在 PowerShell 中运行 setup.ps1`）。

**Windows 持久化说明**：`setup.ps1` 将 token 固化到**用户级环境变量**（注册表 User 作用域），所有新进程自动继承，无需 source profile。由于 Windows 在进程创建时继承父进程环境块，IDE（Trae）需**重启一次**后，其新开的终端才会自动带上 `GITCODE_TOKEN`。当前已运行的旧终端可用以下命令立即赋值，无需 source profile：
```powershell
$env:GITCODE_TOKEN = [Environment]::GetEnvironmentVariable('GITCODE_TOKEN','User')
```

**Linux/macOS 持久化说明**：`setup.sh` 将 token 写入独立文件 `~/.config/gitcode/token.env`（权限 600，仅属主可读），shell 配置文件（`~/.bashrc`/`~/.zshrc`/`~/.bash_profile`）中只追加一行 source 引用，不含 token 本身。首次运行后执行 `source ~/.bashrc`（或重开终端）生效。

TOKEN使用：方式1：请求头'Authorization' = "Bearer $token"，方式2：查询参数'access_token' = $token。两种均可，推荐方式1.

## 执行步骤

step1：从环境变量`GITCODE_TOKEN`读取访问令牌，如果为空则停止 skill 并提示用户运行 setup 脚本。

step2：基于访问令牌解析 PR 中的申请方、申请方 git 地址和分支以及该 PR 有几个 commit，记为 `pr_commit_count`. 如果PR状态是关闭的，则直接退出并提示用户PR已关闭. 如果PR没有ci_successful标签，则做为最后输出的检视结果的一个重要评估项。

step3：基于此生成一个从该仓库 fetch 到本地的 fetch 命令，命令中指定的本地分支名包含 PR 号和申请作者名并用.号隔开，如果分支名本地已存在则追加个递增序号，执行该 fetch 命令，并切到该分支。

step4：如果pr_commit_count>1，则评估各个commit的独立性，是否要提示申请方做rebase，做为最后输出的检视结果的一个重要评估项。将最新的 pr_commit_count 个 commit rebase 成 1 个 commit，以便后续检视skill执行.

step5：获取当前使用的模型名称，记为 `model_name`。调用 code-review-c-cpp 技能进行代码检视最新的 1 个 commit，输出报告；如果报告的整体内容是英文，则翻译成中文。注意，如果通过 git diff 查看差异时，需要添加 --no-pager 参数。

step6：重命名报告文件，末尾追加 PR 号和 `model_name`，用 _ 隔开，双击打开报告，并用 chrome 打开 PR 链接。

step7：如果存在/TRAE-security-review 技能，则调用 /TRAE-security-review 检视最新 1 个已提交的 commit。