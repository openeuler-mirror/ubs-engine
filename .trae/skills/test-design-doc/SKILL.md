---
name: "test-design-doc"
description: "Generates structured test design documents from source code and requirements. Invoke when user asks for 测试设计文档/测试文档/test design, or wants to plan UT/IT/PT/FUZZ/E2E testing for a module."
---

# 测试设计文档生成指南

本 skill 为代码模块生成结构化的测试设计文档(Test Design Document),从测试视角描述测试策略、测试分层、用例设计方法、测试场景与验收标准,作为开发、测试与持续集成(CI)的依据。文档基于源码分析与需求规约,通过解读接口契约、业务流程、状态机与异常路径,整理出完整的测试规约。

## 1. 何时触发

### 1.1 触发条件(满足任一即触发)

- 用户明确要求生成"测试设计文档"、"测试文档"、"test design document"
- 用户给出某模块路径并要求规划测试
- 用户要求为某模块编写测试用例/测试场景
- 用户要求梳理 UT/IT/PT/FUZZ/E2E 测试体系
- 用户要求从需求文档的 AC-XXX 验收标准推导测试用例

### 1.2 不触发的情况

- 用户要求生成需求分析文档(应使用 requirement-analysis-doc skill)
- 用户要求生成模块设计文档(应使用 module-design-doc skill)
- 用户仅要求编写单个测试用例(直接编写即可)
- 用户仅要求修改已有测试文档的个别章节(直接编辑即可)

### 1.3 与需求/设计文档的衔接

测试设计文档验证"是否做到",需与需求/设计文档对齐:
- 引用需求文档的 FR-XXX 功能需求,设计对应测试场景
- 引用需求文档的 NFR-XXX 非功能需求,设计性能/可靠性测试
- 引用需求文档的 AC-XXX 验收标准,设计验收用例
- 引用设计文档的时序图/状态机,设计集成测试与 E2E 测试

若无需求/设计文档,测试设计文档可独立生成,但仍应在"功能场景覆盖矩阵"章节明确被测功能点。

## 2. 文档标准结构

测试设计文档必须包含以下 14 章,章节顺序与标题固定:

```
1. 引言                —— 文档目的、范围、术语、参考资料
2. 测试策略            —— 测试金字塔、分层目标、通过准则、环境
3. 测试框架与规范      —— 框架选型、代码组织、命名/注释/SetUp 规范
4. 测试场景设计        —— 功能场景覆盖矩阵、用例设计方法
5. 单元测试设计        —— 各子模块 UT 用例(核心章节)
6. 集成测试设计        —— 模块装配、RPC、HTTP 集成测试
7. 性能测试设计        —— 性能指标验证场景
8. 模糊测试设计        —— 异常输入 fuzz 场景
9. 端到端测试设计      —— 完整业务链路 E2E 场景
10. 测试数据管理       —— 数据分类、工厂方法、隔离策略
11. 测试执行与 CI 集成 —— 本地命令、CI 流水线、覆盖率度量
12. 缺陷管理           —— 缺陷分级、跟踪流程
13. 风险与对策         —— 测试风险识别与对策
14. 附录               —— 现有用例统计、覆盖率目标、常量、相关文档
```

## 3. 各章节编写指南

### 3.1 第 1 章 引言

**1.1 文档目的**:一句话说明文档目标(作为开发、测试与 CI 的依据)。

**1.2 文档范围**:列出测试域(UT/IT/PT/FUZZ/E2E),从模块功能边界反推。

**1.3 术语与缩略语**:从测试框架、项目规范提取:

| 术语 | 含义 |
|------|------|
| UT | Unit Test,单元测试 |
| IT | Integration Test,集成测试 |
| PT | Performance Test,性能测试 |
| FUZZ | Fuzzing Test,模糊测试 |
| E2E | End-to-End Test,端到端测试 |
| GTest | Google Test,C++ 单元测试框架 |
| MockCpp | 轻量级 C++ Mock 框架 |
| ASAN | AddressSanitizer,内存错误检测 |
| 覆盖率 | 代码被执行的行/分支/函数占比 |
| Fixture | GTest 测试夹具 |

**1.4 参考资料**:关联需求文档、设计文档、UT 开发指南、AGENTS.md。

### 3.2 第 2 章 测试策略

**2.1 测试金字塔**:用 ASCII 图展示测试分层(UT 多、E2E 少):

```
        ┌──────────┐
        │   E2E    │  少量,覆盖关键业务链路
        └──────────┘
    ┌──────────────────┐
    │   PT / FUZZ      │  中量,关注性能与健壮性
    └──────────────────┘
┌──────────────────────────┐
│         IT               │  中量,模块间联动
└──────────────────────────┘
┌──────────────────────────────────┐
│              UT                  │  大量,覆盖核心逻辑
└──────────────────────────────────┘
```

**2.2 分层测试目标**:用表格列出每层的目标、执行频率、责任人:

| 测试层 | 目标 | 执行频率 | 责任人 |
|--------|------|----------|--------|
| UT | 函数级行为验证,覆盖率 ≥ 80% | 每次提交 | 开发 |
| IT | 模块装配、RPC、插件加载 | 每日构建 | 开发 + 测试 |
| PT | 性能指标达标 | 每版本发布前 | 测试 |
| FUZZ | 异常输入无崩溃 | 每版本发布前 | 测试 |
| E2E | 完整业务链路达验收标准 | 每版本发布前 | 测试 |

**2.3 测试通过准则**:
- UT:100% 通过,核心子模块行覆盖率 ≥ 80%,分支覆盖率 ≥ 70%
- IT:100% 通过,关键集成点全覆盖
- PT:所有性能指标满足 NFR-PERF-XX
- FUZZ:24 小时无崩溃,无 ASAN/UBSAN 报错
- E2E:所有 AC-XXX 验收项全部通过

**2.4 测试环境**:从 CMakeLists.txt、构建脚本、AGENTS.md 提取:

| 项目 | 要求 |
|------|------|
| 操作系统 | openEuler Linux ARM64 |
| 编译器 | GCC 10+,C++17/C11 |
| 测试框架 | GoogleTest + MockCpp |
| 覆盖率工具 | gcov / lcov |
| 内存检测 | ASAN / UBSAN |
| 硬件 | 真实设备(E2E)、Mock 设备(UT/IT) |

### 3.3 第 3 章 测试框架与规范

**3.1 框架选型**:用表格列出工具与用途:

| 工具 | 用途 |
|------|------|
| GoogleTest | `TEST_F`/`TEST`/`EXPECT_*`/`ASSERT_*` |
| MockCpp | `MOCKER_CPP_VIRTUAL`/`stubs().will(...)` |
| CMake | `add_ut()` 宏注册测试目标 |
| build.sh | `bash build.sh ut` 封装命令 |

**3.2 测试代码组织**:用 ASCII 树展示测试目录结构:

```
test/UT/
├── controllers/<module>/          # 控制器层 UT
│   ├── CMakeLists.txt
│   └── test_<module>_<submodule>.{h,cpp}
├── adapter_plugins/<module>/      # 适配器层 UT
└── cli/
    └── test_ubse_cli_<module>_*.{h,cpp}
```

**3.3 命名规范**:

| 类型 | 规范 | 示例 |
|------|------|------|
| TestSuite | `Test<模块名><子模块>` | `TestUbseSsuScheduler` |
| TestCase | `<方法>_<场景>_<期望>` | `PreCheckAllocSizeZero` |
| 文件 | `test_<被测文件名>.{h,cpp}` | `test_ubse_ssu_scheduler.cpp` |
| 命名空间 | `ubse::<模块>::ut` | `ubse::ssu::scheduler::ut` |

**3.4 用例注释规范**:每个 `TEST_F` 上方必须含三段注释:

```cpp
/*
 * 用例描述:
 * <一句话描述被测行为>
 * 测试步骤:
 * 1、<步骤1>
 * 2、<步骤2>
 * 预期结果:
 * 1、<断言1>
 * 2、<断言2>
 */
TEST_F(TestXxx, Method_Scenario_Expectation) { ... }
```

**3.5 SetUp/TearDown 规范**:
- `SetUp()`:构造 Mock 对象、初始化被测单例、注入 stub
- `TearDown()`:调用 `GlobalMockObject::verify()` 校验 Mock 期望、恢复单例状态
- 跨用例共享的 Mock 通过 `static` 成员或 helper 工厂方法构造

**3.6 测试数据构造**:通过 helper 工厂方法构造测试数据:

```cpp
static UbseXxx MakeXxx(param1, param2, ...);  // 构造合法对象
static UbseXxx MakeInvalidXxx(...);            // 构造非法对象(边界值)
```

### 3.4 第 4 章 测试场景设计

**4.1 功能场景覆盖矩阵**:用表格列出需求编号、测试模块、TestSuite、用例数、覆盖率目标:

| 需求编号 | 测试模块 | TestSuite | 用例数 | 覆盖率目标 |
|----------|----------|-----------|--------|-----------|
| FR-ALLOC | 调度器 | TestUbseSsuScheduler | 24 | ≥ 90% |
| FR-FREE | 服务实现 | TestUbseSsuServiceImpFree | 待补 | ≥ 80% |

**4.2 测试用例设计方法**:用表格列出方法、适用场景、示例:

| 方法 | 适用场景 | 示例 |
|------|----------|------|
| 等价类划分 | 参数取值域 | `allocSize=0`(无效) vs `>0`(有效) |
| 边界值分析 | 边界条件 | `nsCount=127/128/129` |
| 决策表 | 多条件组合 | STRIPED × 整除性 × chunkSize |
| 状态迁移 | 状态机 | IDLE→CREATING→CREATED→ATTACHING→ATTACHED |
| 错误猜测 | 异常路径 | RPC 超时、MTI 调用失败 |
| 因果图 | 因果关系 | 预扣容量 × collector 刷新 × 容量可见性 |

### 3.5 第 5 章 单元测试设计(核心章节)

每个核心子模块单独一节展开,结构如下:

```
### 5.X 子模块名(TestSuite)

#### 5.X.1 测试目标
验证 <被测类> 的 <功能>,覆盖需求 FR-XXX、NFR-XXX。

#### 5.X.2 测试场景
按功能分组列出用例,每组用表格:

**A. 功能分组名**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Method_NormalInput | 正常输入 | 返回 UBSE_OK |
| Method_InvalidParam | 非法参数 | 返回错误码 |
| Method_BoundaryValue | 边界值 | 返回错误码或正确值 |

#### 5.X.3 测试夹具
展示 TestSuite 的 SetUp/TearDown 与 helper 方法:

```cpp
class TestXxx : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;  // GlobalMockObject::verify()
protected:
    static UbseXxx MakeXxx(...);
};
```

#### 5.X.4 Mock 依赖
用表格列出 Mock 对象、Mock 方法、用途:

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| `XxxAdapter::GetInstance()` | `CreateXxx` / `DeleteXxx` | 模拟硬件操作 |
| `UbseGetRole` | C 函数 | 模拟节点角色 |
```

**子模块识别要点**:
- 从设计文档的第 5 章"核心子模块设计"识别
- 从测试目录 `test/UT/controllers/<module>/` 已有测试文件识别
- 从服务实现类的成员变量识别(每个成员通常对应一个子模块)

**用例设计要点**:
- 已实现的测试:从现有 `test_*.cpp` 的 `TEST_F` 提取,标注"已实现"
- 待补的测试:根据设计文档的时序图/状态机推导,标注"待补"
- 每个子模块至少覆盖:正常路径、参数校验、边界值、异常路径、幂等性(若适用)

### 3.6 第 6 章 集成测试设计

**6.1 测试目标**:验证模块与其他模块的协同,覆盖需求 FR-DISTRIBUTED、FR-NORTHBOUND。

**6.2 测试场景**:

**6.2.1 模块装配**:

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| ModuleInit_Success | 模块初始化 | UBSE_OK,依赖注册成功 |
| ModuleStart_AsMaster | master 启动 | collector 启动,路由注册 |
| ModuleStop_Cleanup | 停止清理 | 资源释放 |

**6.2.2 RPC 通信**:

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| Agent_ForwardToMaster | agent 转发 | master 收到请求 |
| RpcTimeout_Retry | RPC 超时 | 重试后失败/成功 |
| MasterFailover | master 故障切换 | 新 master 接管 |

**6.2.3 HTTP 北向接口**:

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| HttpXxx_Success | HTTP 操作成功 | 200 OK |
| HttpXxx_MtlsReject | mTLS 失败 | 401 Unauthorized |
| HttpXxx_PermissionDeny | 无权限 | 403 Forbidden |

### 3.7 第 7 章 性能测试设计

**7.1 测试目标**:验证满足 NFR-PERF-XX 性能指标。

**7.2 测试场景**:用表格列出用例、场景、期望指标:

| 用例名 | 场景 | 期望指标 |
|--------|------|----------|
| PerfXxx_Normal | 正常操作 | 响应时间 ≤ Xs |
| PerfConcurrent_NThreads | N 并发 | 全部成功,无超时 |
| PerfQueue_N | N 任务排队 | 队列不溢出 |

**7.3 测试方法**:
- 使用 `std::chrono` 测量响应时间
- 使用 `std::thread` 模拟并发
- 使用 Google Benchmark 或自写循环执行 N 次取平均
- 关闭日志输出避免 IO 影响

### 3.8 第 8 章 模糊测试设计

**8.1 测试目标**:通过随机/异常输入发现崩溃、内存错误、未定义行为。

**8.2 测试场景**:用表格列出用例、输入域、期望结果:

| 用例名 | 输入域 | 期望结果 |
|--------|--------|----------|
| FuzzXxx_Field | 随机值(0~MAX,特殊字符) | 不崩溃,返回合理错误码 |
| FuzzXxx_Empty | 空字符串/空指针 | 不崩溃 |
| FuzzXxx_Overflow | UINT64_MAX 等极值 | 不崩溃,不整型溢出 |

**8.3 测试方法**:
- 使用 libFuzzer 或自定义随机输入生成器
- 编译时启用 ASAN/UBSAN
- 持续运行 24 小时,监控崩溃与告警

### 3.9 第 9 章 端到端测试设计

**9.1 测试目标**:验证完整业务链路,覆盖所有 AC-XXX 验收项。

**9.2 测试环境**:
- 真实硬件设备(E2E)
- 多节点集群(1 master + 2 agent + 1 standby)
- 真实客户端

**9.3 测试场景**:按业务场景分组:

**9.3.1 业务用户场景(AC-XXX)**:

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_Xxx_Success | SDK → Xxx | AC-XXX-01 |
| E2E_Xxx_Idempotent | 重复操作 | AC-XXX-03 |
| E2E_Xxx_Concurrent | 并发操作 | AC-XXX-04 |

**9.3.2 高可用场景(AC-XXX)**:

| 用例名 | 链路 | 验收项 |
|--------|------|--------|
| E2E_MasterFailover | kill master | AC-XXX-01 |
| E2E_StateRebuild | 重启后状态恢复 | AC-XXX-02 |

### 3.10 第 10 章 测试数据管理

**10.1 测试数据分类**:

| 类型 | 说明 | 示例 |
|------|------|------|
| 输入数据 | 接口参数 | name、size |
| Mock 数据 | 模拟外部依赖返回 | devList、busInsts |
| 期望数据 | 断言期望值 | ret=UBSE_OK |
| 状态数据 | 被测对象内部状态 | 缓存、计数器 |

**10.2 测试数据工厂**:通过 helper 方法集中管理:

```cpp
static UbseXxx MakeXxx(param1, param2);           // 构造合法对象
static UbseXxx MakeXxxWithBoundary(...);           // 边界值
static UbseXxx MakeInvalidXxx(...);                // 非法对象
```

**10.3 测试数据隔离**:
- 每个用例独立构造数据,避免共享状态污染
- 单例对象在 TearDown 中恢复初始状态
- Mock 期望在 TearDown 中通过 `GlobalMockObject::verify()` 校验并清理

### 3.11 第 11 章 测试执行与 CI 集成

**11.1 本地执行**:列出测试命令(从 AGENTS.md 提取):

```shell
# 执行所有 UT
bash build.sh ut

# 执行特定模块 UT
bash build.sh ut -- --gtest_filter="TestXxx.*"

# 执行单个用例
bash build.sh ut -- --gtest_filter="TestXxx.Method_Scenario"

# 生成覆盖率报告
bash build.sh ut -C

# 覆盖率报告 + HTTP 服务查看
bash build.sh ut -C -H
```

**11.2 CI 流水线**:用表格列出阶段、触发、动作:

| 阶段 | 触发 | 动作 |
|------|------|------|
| Pre-Commit | git commit | clang-format + clang-tidy |
| CI-Build | push / PR | `bash build.sh` 编译 |
| CI-UT | push / PR | `bash build.sh ut`,覆盖率 ≥ 80% |
| CI-IT | 每日 | `bash build.sh it` |
| CI-PT | 每版本 | 性能基线对比 |
| CI-FUZZ | 每版本 | 24 小时模糊测试 |
| CI-E2E | 每版本 | 真实硬件 E2E |

**11.3 覆盖率度量**:

| 度量项 | 目标 | 工具 |
|--------|------|------|
| 行覆盖率 | ≥ 80% | gcov / lcov |
| 分支覆盖率 | ≥ 70% | gcov / lcov |
| 函数覆盖率 | ≥ 90% | gcov / lcov |
| 核心子模块 | ≥ 90% | Scheduler / Manager / Adapter |

**11.4 测试报告**:
- UT 报告:JUnit XML 格式
- 覆盖率报告:`coverage/index.html`
- CI 集成:通过 `gtest_discover_tests` 注册到 CTest

### 3.12 第 12 章 缺陷管理

**12.1 缺陷分级**:

| 级别 | 定义 | 处理时限 |
|------|------|----------|
| P0 | 阻塞主流程,数据丢失 | 立即修复 |
| P1 | 核心功能不可用 | 1 个工作日 |
| P2 | 次要功能不可用 | 3 个工作日 |
| P3 | 体验问题 | 版本发布前 |

**12.2 缺陷跟踪**:
- 录入缺陷管理系统(如 Gitee Issue)
- 包含:标题、复现步骤、期望/实际结果、环境、日志、截图
- 关联对应的 AC-XXX 或 NFR-XXX

### 3.13 第 13 章 风险与对策

识别 5~8 项测试风险,每项含影响与对策:

| 风险 | 影响 | 对策 |
|------|------|------|
| 真实硬件资源不足 | E2E 覆盖度受限 | 优先 Mock,关键路径用真实硬件 |
| Mock 与真实行为不一致 | UT 通过但 E2E 失败 | 定期对比 Mock 与真实返回 |
| 单例状态污染 | 用例间相互影响 | TearDown 严格恢复状态 |
| ASAN 误报 | 性能下降,CI 失败 | UT 不开 ASAN,FUZZ 单独启用 |
| 覆盖率达标但质量不足 | 仅追求覆盖率 | 增加边界值、异常路径用例 |

### 3.14 第 14 章 附录

**14.1 现有测试用例统计**:

| 测试套件 | 文件 | 用例数 | 状态 |
|----------|------|--------|------|
| TestXxx | test_xxx.cpp | N | 已实现 |
| TestYyy | 待新增 | 0 | 待补 |

**14.2 测试覆盖率目标**:

| 子模块 | 当前行覆盖率 | 目标行覆盖率 | 目标分支覆盖率 |
|--------|-------------|-------------|---------------|
| Xxx | 待测 | ≥ 90% | ≥ 80% |
| Yyy | 待测 | ≥ 80% | ≥ 70% |

**14.3 关键常量**:从代码常量定义提取,用于边界值测试:

| 常量 | 值 | 测试用途 |
|------|----|---------|
| MAX_XXX | 128 | 边界值测试(127/128/129) |
| MAX_TIMEOUT_SECONDS | 30 | 超时测试 |
| MAX_RETRY_COUNT | 3 | 重试次数测试 |

**14.4 相关文档**:关联项目内文档,使用相对路径链接。

## 4. 编写流程

### 4.1 信息收集阶段

**必须阅读的关键文件**(按阅读顺序):

1. **需求分析文档**:`docs/design/modules/<模块名>/<模块名>模块需求分析文档.md`(若存在)
   - 提取:FR-XXX 功能需求、NFR-XXX 非功能需求、AC-XXX 验收标准

2. **设计文档**:`docs/design/modules/<模块名>/<模块名>模块设计文档.md`(若存在)
   - 提取:核心子模块、时序图、状态机、并发机制

3. **服务契约**:`src/include/plugin_services/<module>/<module>_service.h`
   - 提取:对外接口、枚举、结构体(用于设计接口测试)

4. **服务实现**:`src/controllers/<module>/<module>_service_imp.{h,cpp}`
   - 提取:业务流程、错误码、状态机、重试/回滚(用于设计 UT 场景)

5. **核心子模块**:`src/controllers/<module>/<module>_<submodule>.{h,cpp}`
   - 提取:每个子模块的类设计、关键方法(用于设计 UT 用例)

6. **现有测试**:`test/UT/controllers/<module>/`、`test/UT/adapter_plugins/<module>/`
   - 提取:已有 TestSuite、TestCase、Mock 依赖、helper 方法
   - 标注"已实现",统计用例数

7. **测试框架规范**:`docs/test/单元测试开发指南.md`(若存在)
   - 提取:框架选型、命名规范、注释规范

8. **构建脚本**:`AGENTS.md`、`build.sh`、`CMakeLists.txt`
   - 提取:测试命令、CI 集成方式、覆盖率工具

**辅助工具调用**:
- 使用 `LS` 列出测试目录结构,识别已有测试文件
- 使用 `Grep` 搜索 `^TEST_F\(` 提取已有用例列表
- 使用 `Grep` 搜索 `MOCKER_CPP_VIRTUAL|MOCKER_CPP` 提取 Mock 依赖
- 使用 `Grep` 搜索 `constexpr|MAX_|MIN_` 提取边界值常量
- 使用 `Grep` 搜索 `enum class.*State` 提取状态机(用于状态迁移测试)
- 使用 `Grep` 搜索 `UBSE_LOG_ERROR` 提取异常场景
- 使用 `SearchCodebase` 进行语义搜索(如"how does alloc handle concurrent requests")

### 4.2 文档生成阶段

1. 先用 `TodoWrite` 创建 14 章节的任务清单
2. 按章节顺序生成,每章完成立即标记 completed
3. 第 5 章 UT 设计是核心,每个子模块单独一节,含场景表 + 夹具 + Mock 依赖
4. 已实现测试标注"已实现"与用例数,待补测试标注"待补"
5. 测试场景关联 FR-XXX / NFR-XXX / AC-XXX,确保可追溯
6. 使用相对路径链接关联项目内文档与代码

### 4.3 用例设计要点

**已实现测试的处理**:
- 从现有 `test_*.cpp` 的 `TEST_F` 提取用例名
- 按功能分组组织(如 PreCheckHandler、Algorithm、TenantIsolation)
- 统计用例数,填入"功能场景覆盖矩阵"

**待补测试的设计**:
- 根据设计文档的时序图,识别未覆盖的流程
- 根据状态机,设计状态迁移测试(合法转换 + 非法转换)
- 根据并发控制,设计并发冲突测试
- 根据异常路径(UBSE_LOG_ERROR),设计异常场景测试
- 根据边界值常量,设计边界值测试

**用例设计方法论**:
- 等价类划分:参数取值域划分(有效/无效)
- 边界值分析:边界条件(0、MAX、MAX-1、MAX+1)
- 决策表:多条件组合(如 STRIPED × 整除性 × chunkSize)
- 状态迁移:状态机的合法/非法转换
- 错误猜测:异常路径(RPC 超时、Mock 失败、空指针)
- 因果图:因果关系(预扣 × 刷新 × 容量可见性)

### 4.4 质量校验阶段

生成完成后,逐项校验:

**结构完整性**:
- [ ] 14 章全部生成,无遗漏
- [ ] 第 5 章每个子模块都有测试目标、场景、夹具、Mock 依赖
- [ ] 表格、代码块格式正确

**内容准确性**:
- [ ] 已实现测试的用例数与代码一致
- [ ] TestSuite 名、TestCase 名与代码一致
- [ ] Mock 依赖与代码 `MOCKER_CPP` 使用一致
- [ ] 关键常量与代码 `constexpr` 一致
- [ ] 测试命令与 `build.sh` 一致

**可追溯性**:
- [ ] 测试场景关联 FR-XXX / NFR-XXX / AC-XXX
- [ ] 引用代码位置标注文件名
- [ ] 关联文档使用相对路径链接

**完整性**:
- [ ] 已实现测试全部统计,无遗漏
- [ ] 待补测试已规划,标注"待补"
- [ ] 覆盖率目标明确(行覆盖率、分支覆盖率)
- [ ] CI 集成完整(从 Pre-Commit 到 E2E)

**归档规范性**:
- [ ] 文档输出到 `docs/design/modules/<模块名>/` 目录
- [ ] 文件名符合 `<模块名>模块测试设计文档.md` 规范
- [ ] 图片存放到 `docs/design/images/<模块名>/`,引用路径为 `../../images/<模块名>/xxx.png`
- [ ] 已更新 `docs/design/README.md` 的文档清单表

## 5. 编写规范

### 5.1 文档头格式

```
# <模块名> 模块测试设计文档

> 源码路径:`src/controllers/<module>/`、`src/adapter_plugins/<module>/`
> 测试路径:`test/UT/controllers/<module>/`、`test/UT/adapter_plugins/<module>/`
> 文档版本:V1.0
> 编写日期:YYYY-MM-DD

---
```

### 5.2 语言与风格

- 使用中文编写(除非用户另有要求)
- 技术术语保留英文(如 GTest、MockCpp、ASAN、Fixture)
- 表格优先于长文本描述
- 代码示例使用 ```cpp 代码块
- 文档内链接使用相对路径
- 已实现测试标注"已实现",待补测试标注"待补"

### 5.3 输出位置

文档输出到 `docs/design/modules/<模块名>/<模块名>模块测试设计文档.md`,遵循设计文档三层归档规则(架构/框架/模块):

```
docs/design/
├── architecture/         # 架构设计
├── framework/            # 框架设计
├── modules/              # 模块文档(按模块分子目录)
│   └── <模块名>/         # 本模块文档目录
│       ├── <模块名>模块需求分析文档.md
│       ├── <模块名>模块设计文档.md
│       └── <模块名>模块测试设计文档.md   ← 本 skill 生成
├── images/               # 图片资源(按层级分目录)
│   ├── architecture/
│   ├── framework/
│   └── modules/          # 模块文档图片存放于此
│       └── <模块名>/     # 按模块归档 .drawio 源文件与导出图片
└── README.md             # 文档索引(新增文档需更新此文件)
```

**图片归档规则**:
- 模块文档引用的图片存放到 `docs/design/images/<模块名>/`(按模块分子目录)
- 文档内引用使用相对路径:`../../images/<模块名>/xxx.png`
- 新增文档后需更新 `docs/design/README.md` 的文档清单表

**归档规则参考**:[docs/design/README.md](../../../docs/design/README.md)

### 5.4 用例命名规范

测试用例命名遵循 `<方法>_<场景>_<期望>` 格式:

| 命名部分 | 说明 | 示例 |
|----------|------|------|
| 方法 | 被测方法名 | `PreCheck`、`AllocSpace` |
| 场景 | 测试场景 | `AllocSizeZero`、`NormalInput` |
| 期望 | 预期结果 | `ReturnError`、`Success` |

示例:`PreCheck_AllocSizeZero_ReturnError`、`FeDeviceAlloc_Success`

## 6. 与其他文档的关系

测试设计文档是文档体系的验证环节:

```
需求分析文档 (FR/NFR/AC)  ←  定义"做什么"
       │
       ▼
设计文档 (架构/类/时序)    ←  定义"怎么做"
       │
       ▼
测试设计文档 (UT/IT/PT)    ←  验证"是否做到"(本 skill)
       │
       ▼
测试代码 (test/*.cpp)      ←  落地"具体测试"
```

**一致性要求**:
- 测试场景应覆盖需求文档的所有 FR-XXX 功能需求
- 测试用例应覆盖需求文档的所有 AC-XXX 验收标准
- 性能测试的指标应与需求文档的 NFR-PERF-XX 一致
- 集成测试应覆盖设计文档的所有跨模块交互
- E2E 测试应覆盖设计文档的所有关键流程时序图

## 7. 示例

### 7.1 完整示例

参考已生成的测试设计文档:
- [SSU模块测试设计文档.md](../../../docs/design/modules/ssu/SSU模块测试设计文档.md) —— 完整 14 章示例

该文档的关键特征:
- 第 5 章:7 个子模块 UT 设计(调度器 24 用例、VFE 直通 45 用例、适配器 72 用例、CLI 42 用例 + 6 个待补)
- 第 4 章:功能场景覆盖矩阵 + 6 种用例设计方法
- 第 6~9 章:IT/RPC/HTTP、PT 性能指标、FUZZ 异常输入、E2E 业务链路
- 第 11 章:本地命令 + 7 阶段 CI 流水线 + 覆盖率度量
- 第 14 章:现有用例统计(4 套件 183 用例)+ 覆盖率目标 + 关键常量

### 7.2 UT 用例表示例

```
### 5.1 调度器测试(TestUbseSsuScheduler)

#### 5.1.2 测试场景

**A. PreCheckHandler(预检查过滤器)**

| 用例名 | 场景 | 预期结果 |
|--------|------|----------|
| PreCheckAllocSizeZero | allocSize=0 | 返回 false,ret=INVALID_PARAM |
| PreCheckNsNumZero | nsNum=0 | 返回 false,ret=INVALID_PARAM |
| PreCheckEmptyDevices | 设备列表为空 | 返回 false,ret=INSUFFICIENT_SPACE |
| PreCheckSuccessSortByFreeAndNsCount | 正常过滤并排序 | 返回 true,按 freeBytes 降序 |

#### 5.1.4 Mock 依赖

| Mock 对象 | Mock 方法 | 用途 |
|-----------|-----------|------|
| (无) | - | 调度器为纯算法模块,直接构造 Context |
```

### 7.3 CI 流水线示例

| 阶段 | 触发 | 动作 |
|------|------|------|
| Pre-Commit | git commit | clang-format + clang-tidy |
| CI-UT | push / PR | `bash build.sh ut`,覆盖率 ≥ 80% |
| CI-IT | 每日 | `bash build.sh it` |
| CI-PT | 每版本 | 性能基线对比 |
| CI-FUZZ | 每版本 | 24 小时模糊测试 |
| CI-E2E | 每版本 | 真实硬件 E2E |
