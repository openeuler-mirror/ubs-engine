---
name: comment-pr
description: Submit the review findings from a review-pr generated code review report as comments on the openEuler repo PR (companion skill to review-pr).
---

# PR Comment Skill

输入参数为 PR 号（如 `157`），如果没有入参则直接退出，并提示输入。
用户也可以指定需要提交检视报告中的哪些问题，默认提交所有问题。

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

step2：在code_review_report目录下，查找最新的检视报告，报告名格式：codereview_{repo}_{时间戳}_{PR号}_{model_name}.md文件，
- 其中model_name和当前运行的model_name可以不同，但是repo必须相同
- 如果没有找到符合条件的报告，就提示用户，并退出skill; 如果有多份，直接使用最新的一份报告.

step3：将检视报告中的问题提交到 PR 对应的代码行。
**必须使用本 skill 目录下的固化脚本提交**（按平台选择）：Windows 用 `post_comment.ps1`，Linux/macOS 用 `post_comment.sh`。两个脚本均已处理转义与 UTF-8 发送，禁止手动内联构造 JSON 或直接调用 REST。

1. **收集已有检视意见（针对所有 reviewer）**：先调用列出脚本，把该 PR 的**全部**已有评论拉到 UTF-8 文件并用 `Read` 工具读取。Windows：`list_comments.ps1`；Linux/macOS：`list_comments.sh`（参数 `--token/--owner/--repo/--pr/--out-file`）。**不要用终端控制台看结果**（GBK/终端 locale 会乱码），以 `Read` 读取的文件为准。建议同时加 `-Raw`（Windows）或 `--raw`（Linux/macOS）再拉一份原始 JSON 数组文件（如 `_tmp_bodies/comments_raw.json`），供第 5 步脚本级去重复用，避免每条评论重复请求 API。
2. **语义去重分析（agent 层面，主判重）**：对报告里的每个问题，与第 1 步收集到的**所有 reviewer 的评论**（不限作者）逐条做语义比对，判断该 PR 上是否已有相同或高度相似的问题。**判定为重复则跳过该条，不提交**。判定依据（满足其一即为重复）：
   - 已有评论在同一文件、同一位置（行号相近），且问题类型/描述语义一致；
   - 已有评论涉及**同一稳定代码符号**（相同的错误码/枚举/函数名），且问题语义一致；
   - 已有评论的问题简述与本问题语义高度相似（即便措辞不同）。
   - 注意：多 committer 用同一 skill 时，不同人/AI 对同一问题的 ID、措辞、符号可能都不同，**必须靠语义而非字符串精确匹配**来判断；`atomgit-bot` 等机器人/门禁评论不算检视意见，应忽略。
3. 解析报告文件，提取每个问题的 `**文件**: <路径>:<行号>` 字段，得到目标文件路径和代码行号。
4. 评论内容使用检视报告中该问题的**完整内容**（标题、严重程度、问题类型、文件、问题简述、详细描述、原始/修改代码块等），逐项拼接到评论正文。**必须用 `Write` 工具创建 body 文件**（Write 工具写 UTF-8 无 BOM），文件路径如 `_tmp_bodies/body_S3-01.txt`。禁止在命令行/脚本中用 here-string 或字符串内联中文 body。
5. **脚本级去重（最后一道保险，非主判重）**：提交脚本内部会再做一次轻量检查，命中则跳过（输出 `SKIP_ALREADY_EXISTS`）：
   - **内容符号 MatchKey**：评论 body 首行追加标记 `**检视意见ID**: <问题ID>`（如 `S2-01`，供人阅读与追溯）。同时为每个问题选一个**稳定的代码符号**作为 `-MatchKey`（或 `--match-key`）传入——即能唯一标识该问题的函数名/错误码/枚举名（如 `RETURN_MASTER_TO_REQ_SEND_FAILED`，具体到本问题，不用泛化函数名）。若已有评论 body 包含该符号则跳过。此通道不依赖行号，能覆盖"提交者改码导致行号漂移"。
   - **行号 + 作者**（快速通道）：若已存在由**你自己**提交、且行号在目标行 ±2 内的行内评论，则跳过。此通道仅在本 PR 状态重跑（diff 未变）时可靠。按作者过滤可避免把 `atomgit-bot` 等机器人在同行的评论误判为已检视。
   - 脚本自动通过 `/user` 接口获取当前登录用户名做作者过滤，无需手动配置。注意：脚本级行号/作者通道只认"自己"，**跨 reviewer 的重复主要靠第 2 步 agent 语义去重兜住**。
   - **避免冗余 API 调用**：若第 1 步已用 `--raw`/`-Raw` 拉取了原始 JSON 评论文件，可将其通过 `-CommentsFile`（Windows）或 `--comments-file`（Linux/macOS）传入脚本，脚本直接从文件读取已有评论做去重，跳过 API 分页拉取。
6. 按平台调用固化脚本提交：
   - **Windows**（PowerShell）：
     ```
     & "<skill目录>/post_comment.ps1" -Token $env:GITCODE_TOKEN -Owner <owner> -Repo <repo> -Pr <PR号> -BodyFile <body文件> -Path <文件路径> -Position <行号> -OutFile <响应文件> -MatchKey <稳定符号> -CommentsFile <raw_json文件>
     ```
   - **Linux/macOS**（bash，依赖 python3 标准库）：
     ```
     bash "<skill目录>/post_comment.sh" --token "$GITCODE_TOKEN" --owner <owner> --repo <repo> --pr <PR号> --body-file <body文件> --path <文件路径> --position <行号> --out-file <响应文件> --match-key <稳定符号> --comments-file <raw_json文件>
     ```
   - `owner`/`repo` 从 `git remote get-url origin` 推导；`number` 为 PR 号；`position` 为新版本文件行号（diff 右侧行号）。
   - 普通/无法定位到行的意见：省略 `-Path`/`-Position`（或 `--path`/`--position`），提交为 PR 级普通评论。此类评论同样可传 `-MatchKey`/`--match-key` 去重。
7. 提交后用 `Read` 工具（按 UTF-8）读取 `-OutFile`（或 `--out-file`）响应文件确认无乱码、note_id 正常。**不要用终端控制台输出判断是否乱码**：Windows 的 PowerShell 控制台是 GBK，Linux 终端 locale 也可能非 UTF-8，都可能误显示乱码；以 `Read` 工具读取的文件内容为准。
8. 逐条提交所有问题；若某行 `position` 不对应新增行会报错，跳过该条并提示，不阻塞其余评论。失败时（脚本输出 `FAIL` + 响应内容）根据响应定位原因。若脚本输出 `SKIP_ALREADY_EXISTS`，说明该意见已提交过，直接跳过，不视为错误。第 2 步语义去重跳过的问题，在最终输出中说明"已存在相同检视意见，未重复提交"。

step4：**清理临时文件**。提交全部完成后，删除本 skill 运行期间产生的临时文件，避免污染技能目录/仓库。用 `DeleteFile` 工具删除：
- `_tmp_bodies/` 目录下的所有 body 文件、提交响应文件、评论列表文件、原始 JSON 评论文件（如 `body_*.txt`、`out_*.txt`、`list_*.txt`、`comments_raw.json`）；

**编码说明（Linux/macOS）**：
- Linux/macOS 无 PowerShell 的 GBK 解码问题，bash 读取 UTF-8 文件天然正确，中文可直接出现在脚本/文件中。
- `post_comment.sh` 用 python3 标准库（`json.dumps(..., ensure_ascii=False)` + `urllib`）构造请求体并显式 UTF-8 发送，规避 bash 手工转义 JSON 的坑（引号、换行、反斜杠）。前置条件：系统装有 python3（openEuler 默认自带）。
