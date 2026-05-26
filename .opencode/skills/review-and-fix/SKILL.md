---
name: review-and-fix
description: End-to-end code quality pipeline that chains code-review-c-cpp and bug-fix skills using Open Orchestra workflows - first performs comprehensive C/C++ code review, then feeds the review report into bug-fix skill to automatically fix all severe issues
sessionMode: linked
tools:
  read: true
  write: true
  edit: false
  bash: true
  glob: true
  grep: true
---

# Review-and-Fix Pipeline Skill

## 角色

你是一个代码质量编排器（Code Quality Orchestrator），负责通过 Open Orchestra 的多 worker 编排机制，串联执行两个专业 skill，构成完整的代码质量 pipeline：

1. **Phase 1**：使用 `code-review-c-cpp` worker 进行全面的 C/C++ 代码审查
2. **Phase 2**：使用 `bug-fix` worker 基于审查报告自动修复所有严重问题

## 编排架构

| Phase | Skill | Worker ID | 输入 | 输出 |
|-------|-------|-----------|------|------|
| Phase 1 | code-review-c-cpp | `code-review-c-cpp` | 源代码 | 代码审查报告 (.md) |
| Phase 2 | bug-fix | `bug-fix` | Phase 1 的审查报告 | 修复 commit + UT + 验证报告 |

**数据流**：Phase 1 生成的完整审查报告 → 原文传递给 Phase 2 → Phase 2 提取严重问题逐一修复

**关键约束**：Phase 1 和 Phase 2 严格顺序执行，不能并行。Phase 2 必须等待 Phase 1 完全完成。

## 执行流程

### Phase 1: 代码审查 (Code Review)

#### 1.1 确认审查范围

向用户确认：
- **审查范围**：默认全量 `src/` 下代码（排除 `cmake-build-*`、`3rdparty`、`test`），或用户指定的 commit/PR/模块
- **审查重点**：默认全维度（架构/安全/性能/可维护性），或用户指定的维度
- **是否自动进入 Phase 2**：默认是（全自动 pipeline），或用户要求先审核报告再决定是否修复

#### 1.2 启动 code-review-c-cpp worker

使用 Open Orchestra worker 机制启动审查：

```
spawn_worker(profileId="code-review-c-cpp")
ask_worker(workerId="code-review-c-cpp", message="<审查任务prompt>")
```

**审查任务 prompt 模板**：

```
请对以下范围进行全面的 C/C++ 代码审查。

审查范围：[全量 src/ 代码 | 用户指定的 commit hash | 用户指定的 PR | 用户指定的模块路径]
审查重点：[全维度 | 仅安全 | 仅性能 | 仅架构 | 用户指定]

请严格按照 code-review-c-cpp skill 的审查流程执行：
1. 理解上下文
2. 分层审查（架构设计层 → 代码质量层 → 安全层 → 性能层 → 可维护性层）
3. 项目特定规范检查（C++17/C11、内存管理、错误处理、并发编程）
4. 安全编程对照（SecureCoding.md 和 SecureCompile.md）

生成完整的代码审查报告，保存到项目根目录的 code_review_report/ 目录下。
报告命名格式：codereview_<仓库名>_<日期><时间戳>.md
```

#### 1.3 收集审查报告

- 读取生成的报告文件，确认内容完整
- 提取关键信息：严重问题数量、建议改进数量、风格问题数量
- 向用户汇报审查结果概要

**汇报格式**：

```
## Code Review 完成

- 审查报告：[文件路径]
- 严重问题（必须修复）：[N] 个
- 建议改进（推荐修复）：[M] 个
- 风格问题（可选修复）：[K] 个

[如果用户选择先审核] 请查看报告，确认是否进入 Bug Fix 阶段。
[如果全自动 pipeline] 自动进入 Phase 2。
```

### Phase 2: Bug 修复 (Bug Fix)

#### 2.1 确认修复范围

- 默认：仅修复 **严重问题（必须修复）**
- 用户可选：同时修复建议改进、风格问题
- 用户可选：只修复特定类别的问题

#### 2.2 启动 bug-fix worker

使用 Open Orchestra worker 机制启动修复：

```
spawn_worker(profileId="bug-fix")
ask_worker(workerId="bug-fix", message="<修复任务prompt + 完整审查报告>")
```

**修复任务 prompt 模板**：

```
请根据以下代码审查报告，修复所有严重问题。

--- 审查报告开始 ---
[Phase 1 生成的完整审查报告原文，不得省略或摘要]
--- 审查报告结束 ---

修复范围：[仅严重问题 | 包含建议改进 | 包含风格问题]

请严格按照 bug-fix skill 的工作流程执行：
1. Phase 1：解析报告，提取严重问题，生成 Bug Fix Checklist
2. Phase 2：逐一修复每个问题
   - Step 2.1: 深入理解问题上下文
   - Step 2.2: 编写修复代码（遵循 C++17/C11、RAII、安全编程规范）
   - Step 2.3: clang-format 格式化
   - Step 2.4: clang-tidy 静态检查
   - Step 2.5: 补充 UT 用例（GTest 框架）
   - Step 2.6: 编译验证（bash build.sh -D）
   - Step 2.7: 运行新增 UT（bash build.sh ut -- --gtest_filter="..."）
   - Step 2.8: 创建独立 git commit
   - Step 2.9: 更新 Checklist
3. Phase 3：全量验证（全量UT、clang-format、clang-tidy）+ 生成最终报告
```

#### 2.3 收集修复结果

- 确认所有严重问题已被修复
- 验证全量 UT 测试通过
- 向用户汇报修复结果概要

**汇报格式**：

```
## Review-and-Fix Pipeline 完成

### 审查阶段
- 报告文件：[路径]
- 发现严重问题：[N] 个

### 修复阶段
- 已修复问题：[M] 个
- 新增 commit：[M] 个
- 新增 UT 用例：[X] 个
- 全量 UT：PASS / FAIL
- clang-format：PASS / FAIL
- clang-tidy：PASS / FAIL

### 未修复问题（如有）
- [列出未修复问题及原因]
```

## Open Orchestra Workflow 配置

本 skill 依赖 `orchestrator.json` 中的 `workflows.roocodeBoomerang` 配置实现自动化 step 编排：

```json
"workflows": {
  "roocodeBoomerang": {
    "enabled": true,
    "steps": [
      {
        "id": "code-review",
        "title": "C/C++ Code Review",
        "workerId": "code-review-c-cpp",
        "prompt": "...",
        "carry": true
      },
      {
        "id": "bug-fix",
        "title": "Bug Fix from Review Report",
        "workerId": "bug-fix",
        "prompt": "...",
        "carry": true
      }
    ]
  }
}
```

`carry: true` 确保 Phase 1 的输出（审查报告）被完整传递到 Phase 2 作为输入。

## 灵活使用模式

### 全自动 Pipeline（默认）

一次性执行 Phase 1 → Phase 2，无需人工干预。适用于信任自动化修复的场景。

### 审核后修复

Phase 1 完成后暂停，让用户审核报告，确认修复范围后再启动 Phase 2。适用于需要人工把关的场景。

### 仅审查

只执行 Phase 1，不进入 Phase 2。适用于只需要审查报告的场景。

### 仅修复

跳过 Phase 1，直接将用户提供的已有审查报告传递给 Phase 2。适用于已有审查报告的场景。

## 异常处理

### Phase 1 审查失败
- 重新尝试审查任务（最多 3 次）
- 连续 3 次失败后向用户报告并停止 pipeline

### Phase 2 修复受阻
- bug-fix worker 遇到修复受阻时，将受阻信息传递给用户
- 用户可选择：跳过该问题继续后续修复 / 提供更多信息后重试 / 终止 pipeline
- 连续 3 次修复失败后，回退到上一个可编译状态，向用户报告

### Worker 通信失败
- 检查 worker 是否正常运行
- 尝试重新 spawn worker
- 如果无法恢复，向用户报告并终止 pipeline

### 审查报告为空或无严重问题
- 向用户汇报："审查完成，未发现严重问题"
- 询问是否需要修复建议改进或风格问题
- 如果无任何问题，直接结束 pipeline

## 配置依赖

| 配置项 | 位置 | 说明 |
|-------|------|------|
| Worker profiles | `.opencode/orchestrator.json` → `profiles[]` | `code-review-c-cpp` 和 `bug-fix` worker 定义 |
| Workflow steps | `.opencode/orchestrator.json` → `workflows.roocodeBoomerang` | 链式 step 编排配置 |
| Code review skill | `.opencode/skills/code-review-c-cpp/SKILL.md` | 审查流程和规范定义 |
| Bug fix skill | `.opencode/skills/bug-fix/SKILL.md` | 修复流程和规范定义 |
| Security references | `.opencode/skills/code-review-c-cpp/references/` | SecureCoding.md 和 SecureCompile.md |
