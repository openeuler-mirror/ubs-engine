# 贡献指南（Contributing to UBS Engine）

感谢您对 **UBS Engine（Unified Bus Service Engine）** 项目的关注与贡献！  
本项目是面向内存等硬件资源的系统级调度引擎，属于 openEuler 社区 **UB 计算系统** 的核心控制平面组件。我们欢迎社区开发者通过提交 Issue、修复 Bug、新增功能或改进文档等方式参与共建。

为确保代码质量与社区协作效率，请在贡献前仔细阅读本指南。

---

## 📌 1. 贡献流程概览

1. **Fork 本仓库** 到您的 Gitee 账号。
2. **克隆 Fork 后的仓库**：
   ```bash
   git clone https://gitee.com/<your-username>/ubs-engine.git
   cd ubs-engine
   git remote add upstream https://gitee.com/openeuler/ubs-engine.git
   ```
3. **创建特性分支**（建议以 `feat/xxx` 或 `fix/xxx` 命名）：
   ```bash
   git checkout -b feat/add-mem-algo
   ```
4. **开发并验证**：编写代码、补充测试、更新文档。
5. **提交代码**（遵循 [提交规范](#commit-message-convention)）。
6. **推送分支并发起 Pull Request（PR）** 到 `openeuler/ubs-engine:master`。
7. **响应 Review 意见**，直至 PR 合并。

> 💡 提示：请保持您的 fork 与上游仓库同步：
> ```bash
> git fetch upstream
> git rebase upstream/master
> ```

---

## 🛠️ 2. 开发环境与构建验证

### 推荐环境
- **操作系统**：openEuler 22.03 LTS SP3 或更高版本（ARM64 架构优先）
- **编译器**：GCC ≥ 9.3 或 Clang ≥ 12
- **构建工具**：CMake ≥ 3.18, Ninja（可选）

### 构建与测试
所有贡献代码必须通过以下验证：

```bash
# 1. 构建 Release 版本（默认）
./build.sh

# 2. 运行单元测试（UT）和集成测试（IT）
./build.sh test

# 3. （可选）生成覆盖率报告，确认关键路径覆盖
./build.sh ut -C
```

> ✅ **要求**：PR 中所有测试必须 **100% 通过**，且不得降低整体代码覆盖率。

---

## 🧼 3. 代码规范

### 日志与错误处理
- 使用项目内置日志模块（`framework/log`）。
- 错误码应定义在 `src/include/ubse_error.h`，并附带清晰语义。

### 安全要求
- 禁止硬编码密钥、路径或敏感信息。
- 所有外部输入需做边界检查，防止缓冲区溢出。
- 商用加密功能（如 KMC）仅在闭源插件中实现，开源部分仅保留接口。

---

<a id="commit-message-convention"></a>
## 📝 4. 提交信息规范

提交信息格式参考 [Conventional Commits](https://www.conventionalcommits.org/)，便于自动生成 CHANGELOG：

```
<type>(<scope>): <subject>

<body>
```

- **type**（必选）：
    - `feat`：新功能
    - `fix`：Bug 修复
    - `docs`：文档更新
    - `test`：测试用例
    - `refactor`：重构（无功能变更）
    - `ci`：CI/CD 配置
    - `perf`：性能优化
- **scope**（可选）：模块名，如 `mem`, `sdk`, `ha`
- **subject**：简明描述（≤50 字符）
- **body**（可选）：详细说明、关联 Issue 等

✅ 示例：
```
feat(mem): add round-robin scheduling algorithm

- Implement basic round-robin allocator in MemScheduler
- Add corresponding UT cases
- Resolves #12
```

---

## 📚 5. 文档与注释

- **公共 API** 必须使用 Doxygen 风格注释：
  ```cpp
  /**
   * @brief Allocate memory block from pool.
   * @param size Size in bytes.
   * @return Pointer to allocated memory, or nullptr on failure.
   */
  void* mem_alloc(size_t size);
  ```
- 新增功能或架构变更需同步更新 `docs/` 下对应文档（如设计、API、示例）。
- 修改 CLI 命令需更新 `docs/cli/`。

---

## 🧪 6. 测试要求

- **新增功能必须包含单元测试（UT）**，覆盖率建议 ≥80%。
- 涉及多节点/资源交互的逻辑需提供集成测试（IT）。
- 测试代码位于 `test/UT/` 和 `test/IT/`，命名规范：`Test<Module><Feature>.cpp`。

---

## 🌐 7. 与 openEuler 社区集成

- 本项目将作为 **RPM 包** 发布至 openEuler 官方源，因此：
    - 构建脚本 `build.sh package` 必须能成功生成符合 openEuler 规范的 RPM。
    - 依赖项需来自 openEuler 官方仓库
- 参与 SIG（Special Interest Group）讨论：  
  **SIG-UB-ServiceCore**（Gitee 群组或邮件列表）

---

## ❓ 8. 获取帮助

- 提交 Issue：[https://gitee.com/openeuler/ubs-engine/issues](https://gitee.com/openeuler/ubs-engine/issues)
- openEuler 社区论坛：[https://gitee.com/openeuler/community](https://gitee.com/openeuler/community)

---

## 📄 许可证

所有贡献代码将依据项目 LICENSE（Mulan PSL v2）开源。  
提交 PR 即表示您同意您的贡献遵循该许可证条款。

---

> **再次感谢您的贡献！让我们共同打造更强大的 UB 计算基础设施。**

---

### 附：快速检查清单（提交 PR 前）

- [ ] 代码通过 `./build.sh` 构建
- [ ] 所有测试通过：`./build.sh test`
- [ ] 代码已格式化（`clang-format`）
- [ ] 新功能有对应文档更新
- [ ] 提交信息符合规范
- [ ] 无敏感信息泄露
