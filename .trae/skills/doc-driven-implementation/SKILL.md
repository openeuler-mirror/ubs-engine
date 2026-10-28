---
name: "doc-driven-implementation"
description: "Generates implementation code and unit tests from requirement/design/test-design documents. Invoke when user asks to implement a module from design docs or convert three-piece docs into code."
---

# 文档驱动代码实现 Skill

本 skill 用于在 UBS-Engine 项目中,以**需求分析文档 + 模块设计文档 + 测试设计文档**(三件套)为输入,正向生成符合设计契约与编码规范的实现代码与单元测试。文档上游为 requirement-analysis-doc / module-design-doc / test-design-doc 三个 skill 生成的三件套。

## 1. 何时触发

### 1.1 触发条件(满足任一即触发)

- 用户给出三件套文档路径(或设计文档路径),要求生成实现代码
- 用户要求按需求分析文档/设计文档实现模块
- 用户要求按测试设计文档生成 UT 用例
- 用户要求从设计文档生成代码骨架(类、接口、handler 框架)
- 用户要求"按文档编码"、"按设计实现"、"实现已设计好的模块"
- 用户描述新模块需求并要求先出文档再实现(此时先调用文档类 skill,再调用本 skill)

### 1.2 不触发的情况

- 仅修改/重构现有代码,无设计文档驱动 → 使用 coding-standards skill
- 仅生成文档,不涉及代码 → 使用 requirement-analysis-doc / module-design-doc / test-design-doc skill
- 仅查询/阅读代码,不涉及生成
- 设计文档不完整(缺关键章节)且用户拒绝补全

### 1.3 前置条件

执行本 skill 前,**必须**确认以下文档已存在且关键章节完整:

| 文档 | 路径 | 必读章节 |
|------|------|----------|
| 需求分析文档 | `docs/design/modules/<模块名>/<模块名>模块需求分析文档.md` | 第 4 章功能需求(FR-XXX)、第 5 章非功能需求(NFR-XXX)、第 11 章验收标准(AC-XXX) |
| 模块设计文档 | `docs/design/modules/<模块名>/<模块名>模块设计文档.md` | 第 4 章服务接口与数据模型、第 5 章核心子模块设计、第 6 章接入层设计、第 7 章关键流程、第 8 章并发控制 |
| 测试设计文档 | `docs/design/modules/<模块名>/<模块名>模块测试设计文档.md` | 第 3 章测试场景、第 4 章 UT 用例规划、第 9 章测试数据管理 |

若任一文档缺失或关键章节不完整,使用 `AskUserQuestion` 澄清:是先补全文档,还是按现有文档实现(标注 TBD)。

### 1.4 与其他 skill 的协作

| 场景 | skill |
|------|-------|
| 文档缺失,需先补全 | requirement-analysis-doc / module-design-doc / test-design-doc |
| 代码规范细节(命名/格式/版权) | coding-standards |
| SSU 控制器专项 | [SSU模块开发指南](../../docs/design/modules/ssu/SSU模块开发指南.md) |
| **从三件套生成代码** | **doc-driven-implementation(本 skill)** |

## 2. 实现流程总览

```
┌─────────────────────────────────────────────────────────────┐
│  阶段 1: 文档解析与一致性校验                                  │
│  - 读取三件套                                                 │
│  - 提取 FR/NFR/AC、接口契约、数据模型、状态机、流程、并发策略    │
│  - 校验 FR → 设计决策 → 测试场景 的映射完整性                   │
└────────────────────────────┬────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  阶段 2: 代码骨架生成(按设计文档)                              │
│  - 服务契约抽象基类(纯虚接口 + Doxygen)                       │
│  - 模块入口类(继承 UbseModule,Init/Start/Stop)               │
│  - 子模块类骨架(成员/关键方法签名)                             │
│  - 接入层 handler 骨架(IPC/HTTP/CLI)                          │
│  - 数据模型 struct/enum 定义                                   │
└────────────────────────────┬────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  阶段 3: 业务逻辑实现(按设计文档第 5/7/8 章)                   │
│  - ServiceImp 业务编排(master/agent 分支、幂等、回滚)          │
│  - 子模块关键方法实现(按设计文档方法表)                         │
│  - 状态机转换与合法性校验                                      │
│  - 并发控制(锁层次、shared_mutex、COW)                         │
│  - 错误处理(错误码 + 日志 + 回滚)                              │
└────────────────────────────┬────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  阶段 4: 单元测试实现(按测试设计文档)                          │
│  - TestSuite/TestName 框架                                    │
│  - Mock 依赖(mockcpp / 自定义 mock)                          │
│  - 用例实现(覆盖 AC-XXX 验收项)                               │
│  - 边界值/异常场景/并发场景                                    │
└────────────────────────────┬────────────────────────────────┘
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  阶段 4: 自检与验证                                            │
│  - 设计一致性 checklist(接口/数据模型/状态机/流程)             │
│  - 编码规范 checklist(调用 coding-standards)                  │
│  - 构建验证(bash build.sh)                                   │
│  - UT 运行(bash build.sh ut)                                 │
│  - 追溯矩阵更新(AC → 代码 → UT)                               │
└─────────────────────────────────────────────────────────────┘
```

## 3. 阶段 1:文档解析与一致性校验

### 3.1 文档读取顺序

**必须按以下顺序读取**(后序依赖前序):

1. **需求分析文档**:提取 FR/NFR/AC 清单(做什么)
2. **模块设计文档**:提取接口契约、数据模型、子模块、流程、并发(怎么做)
3. **测试设计文档**:提取测试场景、用例规划、Mock 依赖(怎么验)

### 3.2 关键信息提取

#### 3.2.1 从需求分析文档提取

| 提取项 | 用途 | 去向 |
|--------|------|------|
| FR-XXX 功能需求清单 | 确认功能覆盖 | 映射到服务接口 |
| NFR-PERF-XX 性能指标 | 约束实现策略(如超时、并发) | 写入代码常量 |
| NFR-REL-XX 可靠性需求 | 幂等/重试/回滚设计 | 业务逻辑实现 |
| NFR-SEC-XX 安全需求 | mTLS/权限校验/安全函数 | 接入层与 ServiceImp |
| AC-XXX 验收标准 | 测试用例验收点 | UT 断言点 |
| CON-XXX 约束条件 | 边界值、容量限制 | 代码常量 |

#### 3.2.2 从模块设计文档提取

| 提取项 | 用途 | 去向 |
|--------|------|------|
| 服务契约(抽象基类 + 纯虚接口) | 生成 `src/include/plugin_services/<module>/<module>_service.h` | 头文件 |
| 关键枚举/结构体 | 生成数据模型定义 | 头文件 |
| 状态机(状态 + 转换) | 生成 `enum class *State` + 转换校验函数 | 头文件 + ServiceImp |
| 模块入口类(继承 UbseModule) | 生成 `<module>_controller_module.{h,cpp}` | 入口文件 |
| 子模块类设计 | 生成 `<module>_<submodule>.{h,cpp}` | 子模块文件 |
| 接入层设计(IPC/HTTP/CLI handler) | 生成 handler 文件 | 接入层文件 |
| 关键流程时序图 | 指导 ServiceImp 业务编排实现 | ServiceImp 实现 |
| 并发控制(锁层次) | 指导锁的声明与使用 | 类成员 + 方法实现 |
| 目录结构 | 确认文件归档路径(遵循目录规范) | 文件创建位置 |

#### 3.2.3 从测试设计文档提取

| 提取项 | 用途 | 去向 |
|--------|------|------|
| TestSuite/TestName 清单 | 生成测试文件框架 | `test/UT/<层级>/<module>/test_ubse_<module>.cpp` |
| Mock 依赖清单 | 生成 mock 文件或配置 mockcpp | `mock_<name>.cpp/.h` |
| 用例与 AC 映射 | 确保每个 AC-XXX 有对应 UT | UT 断言 |
| 测试数据管理 | 测试夹具 SetUp/TearDown | TestFixture |

### 3.3 一致性校验

生成代码前,必须校验三件套的一致性:

| 校验项 | 校验内容 | 不一致处理 |
|--------|----------|------------|
| FR 覆盖 | 每个 FR-XXX 在设计文档中有对应接口/子模块 | 标注 TBD,提示用户补全设计 |
| 接口一致性 | 设计文档的接口签名与测试文档的 Mock 一致 | 以设计文档为准,标注测试文档差异 |
| 状态机一致性 | 设计文档的状态机与测试文档的状态转换用例一致 | 以设计文档为准 |
| AC 覆盖 | 每个 AC-XXX 在测试文档中有对应用例 | 标注缺失用例,提示用户补全 |
| 数据模型一致性 | 三件套中的 struct/enum 定义一致 | 以设计文档为准 |

**校验结果输出**:用表格列出不一致项,提示用户确认后再继续。

## 4. 阶段 2:代码骨架生成

按设计文档第 2 章(目录结构)、第 3 章(模块入口)、第 4 章(服务接口与数据模型)、第 5 章(子模块)、第 6 章(接入层)生成骨架。

### 4.1 文件生成顺序

```
1. 服务契约头文件    src/include/plugin_services/<module>/<module>_service.h
2. 数据模型头文件    src/include/plugin_services/<module>/<module>_def.h(可选,若 struct 较多)
3. 模块入口          src/controllers/<module>/<module>_controller_module.{h,cpp}
4. 服务实现          src/controllers/<module>/<module>_service_imp.{h,cpp}
5. 子模块            src/controllers/<module>/<module>_<submodule>.{h,cpp}
6. 接入层 IPC        src/controllers/<module>/ipc/<module>_ipc_handler.{h,cpp}
                     src/controllers/<module>/ipc/handler/<xxx>_handler.{h,cpp}
7. 接入层 HTTP       src/controllers/<module>/<module>_http_handler.{h,cpp}(可选)
8. 跨节点消息        src/controllers/<module>/message/<module>_<msg>.{h,cpp}(可选)
9. CMakeLists        src/controllers/<module>/CMakeLists.txt
10. 单元测试         test/UT/controllers/<module>/test_ubse_<module>.{h,cpp}
11. Mock             test/UT/controllers/<module>/mock_<name>.{h,cpp}(若需)
```

### 4.2 服务契约生成规则

**输入**:设计文档第 4.1 节服务契约表 + 第 4.2 节关键枚举 + 第 4.3 节关键数据结构

**输出**:`src/include/plugin_services/<module>/<module>_service.h`

**生成要点**:
- 抽象基类名遵循设计文档(如 `UbseSsuService`)
- `kServiceName` 静态常量
- 每个纯虚接口必须有 Doxygen 注释(从设计文档接口表提取)
- 枚举用 `enum class`,值与设计文档一致
- struct 字段与设计文档一致,加注释
- 头文件保护:`#ifndef UBSE_<MODULE>_SERVICE_H`
- 完整版权头
- 命名空间:`ubse::<module>::service`

**模板**:

```cpp
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * ...(完整版权块)
 */

#ifndef UBSE_<MODULE>_SERVICE_H
#define UBSE_<MODULE>_SERVICE_H

#include <cstdint>
#include <string>
#include <vector>

namespace ubse::<module>::service {

// 关键枚举(从设计文档 4.2 提取)
enum class Ubse<Module>XxxState : uint8_t {
    IDLE = 0,
    CREATING = 1,
    CREATED = 2,
    // ...(与设计文档一致)
};

// 关键数据结构(从设计文档 4.3 提取)
struct Ubse<Module>XxxReq {
    std::string name;
    uint64_t size{0};
    // ...(与设计文档一致)
};

struct Ubse<Module>XxxResult {
    std::string name;
    // ...
};

/**
 * @brief <模块名> 服务契约
 *
 * <从设计文档 1.1 模块概述提取>
 */
class Ubse<Module>Service {
public:
    static constexpr const char *kServiceName = "Ubse<Module>Service";

    virtual ~Ubse<Module>Service() = default;

    /**
     * @brief <接口描述>(从设计文档 4.1 接口表提取)
     * @param req <参数描述>
     * @param identity <参数描述>
     * @param result <参数描述>
     * @return uint32_t 错误码
     * @retval 0 成功
     * @retval 非零 失败
     * @note <从设计文档提取的注意事项>
     */
    virtual uint32_t XxxSpace(const Ubse<Module>XxxReq &req,
                              const UbseIdentity &identity,
                              Ubse<Module>XxxResult &result) = 0;

    // ...(其余接口,与设计文档接口表一致)
};

} // namespace ubse::<module>::service

#endif // UBSE_<MODULE>_SERVICE_H
```

### 4.3 模块入口生成规则

**输入**:设计文档第 3 章(模块入口与生命周期)

**输出**:`src/controllers/<module>/<module>_controller_module.{h,cpp}`

**生成要点**:
- 继承 `ubse::module::UbseModule`
- `kModuleName` 静态常量
- `PLUGIN_MODULE_IMPL` 宏注册(依赖模块按设计文档)
- 持有核心成员(从设计文档 3.1 提取,通常为 `std::shared_ptr<Ubse<Module>Service>`)
- `Initialize/Start/Stop` 三阶段生命周期
- Init:创建任务执行器(线程数/队列容量按设计文档)
- Start:注册服务到 ServiceRegistry(空删除器)+ 启动子模块 + 注册 IPC/HTTP handler
- Stop:反向注销

### 4.4 服务实现生成规则

**输入**:设计文档第 5.1 节(ServiceImp 子模块)+ 第 7 章(关键流程)+ 第 8 章(并发控制)

**输出**:`src/controllers/<module>/<module>_service_imp.{h,cpp}`

**生成要点**:
- 单例(`GetInstance()`),继承服务契约
- 持有子模块成员(从设计文档 5.1 提取,如 `scheduler_`、`collector_`、`debtLedger_`)
- 每个接口实现按设计文档时序图:
  - 角色判定(master/agent 分支)
  - 幂等检查(查账本/状态机)
  - 调用子模块执行
  - 状态机更新
  - 错误处理与回滚
- 并发控制:按设计文档 8.1 锁层次表声明成员锁
- 错误码返回 + `UBSE_LOG_ERROR` 日志

### 4.5 子模块生成规则

**输入**:设计文档第 5.X 节(各子模块)

**输出**:`src/controllers/<module>/<module>_<submodule>.{h,cpp}`

**生成要点**:
- 类名、继承关系按设计文档类图
- 关键方法签名按设计文档方法表
- 方法实现按设计文档"关键逻辑"列
- 设计要点(幂等/并发/错误处理)按设计文档 5.X.5

### 4.6 接入层生成规则

**输入**:设计文档第 6 章(接入层设计)

**输出**:
- IPC:`ipc/<module>_ipc_handler.{h,cpp}`(模板方法基类) + `ipc/handler/<xxx>_handler.{h,cpp}`(具体 handler)
- HTTP:`<module>_http_handler.{h,cpp}`(路由注册)
- CLI(可选):`src/cli/<module>/`(若设计文档 6.3 涉及)

**生成要点**:
- IPC handler 三阶段:Unpack → Handle → Pack(模板方法模式)
- 具体 handler 数量与设计文档接口表一致
- HTTP 路由表(Method + URL + Handler)与设计文档一致
- 鉴权方式按设计文档(通常 mTLS + 证书 CN/OU 提取)

## 5. 阶段 3:业务逻辑实现

骨架生成后,填充业务逻辑。**严格按设计文档第 7 章时序图实现**,每个步骤对应代码块。

### 5.1 实现顺序

1. **数据模型与状态机**(纯数据,无依赖)
2. **底层子模块**(无下游依赖,如 Collector、Adapter 封装)
3. **中间层子模块**(依赖底层,如 Scheduler、ReservationMgr)
4. **账本/状态管理**(中间层,如 DebtLedger)
5. **ServiceImp 业务编排**(依赖中间层)
6. **接入层 handler**(依赖 ServiceImp)

### 5.2 业务逻辑实现要点

| 设计文档章节 | 实现要点 |
|--------------|----------|
| 7.X 时序图 | 每个步骤对应代码块,alt/else 分支用 if/else 实现,loop 用 for/while |
| 8.1 锁层次 | 按表声明锁,使用 `std::lock_guard`/`std::unique_lock`/`std::shared_lock` |
| 8.2 一致性保证 | 幂等检查(查现有状态)、预扣/预释放、状态机合法性校验 |
| 5.X 设计要点 | 幂等/并发/错误处理按设计文档列点实现 |

### 5.3 错误处理实现

按设计文档时序图的异常分支:

```cpp
uint32_t Ubse<Module>ServiceImp::XxxSpace(...)
{
    // 1. 角色判定
    if (UbseGetRole() != UbseRole::MASTER) {
        return ForwardViaRpc(req, result);
    }

    // 2. 幂等检查
    auto existing = debtLedger_->Get(req.name);
    if (existing && existing->state == XxxState::CREATED) {
        UBSE_LOG_INFO("idempotent hit, name=%s", req.name.c_str());
        result = existing->result;
        return UBSE_OK;
    }

    // 3. 执行
    uint32_t ret = ExecuteXxx(req, result);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR("ExecuteXxx failed, name=%s, ret=%u", req.name.c_str(), ret);
        Rollback(req.name);
        return ret;
    }

    // 4. 更新账本与状态机
    debtLedger_->Put({req.name, XxxState::CREATED, result});
    return UBSE_OK;
}
```

## 6. 阶段 4:单元测试实现

按测试设计文档生成 UT。**严格按测试设计文档的用例清单实现**,每个用例对应一个 `TEST_F`。

### 6.1 测试文件生成规则

**输入**:测试设计文档第 3 章(测试场景)+ 第 4 章(UT 用例规划)+ 第 9 章(测试数据管理)

**输出**:
- `test/UT/<层级>/<module>/test_ubse_<module>.{h,cpp}`
- `test/UT/<层级>/<module>/mock_<name>.{h,cpp}`(若需)

**生成要点**:
- TestSuite 命名 `Test<Module>`(CamelCase)
- TestName 描述性命名(如 `AllocSpace_ValidInput_ReturnsOk`)
- 命名空间 `ubse::ut::<module>`
- 测试夹具 `SetUp`/`TearDown` 按测试设计文档 9.X 数据管理
- Mock 依赖按测试设计文档 Mock 清单
- 每个用例标注对应的 AC-XXX(注释)
- 用例覆盖:正常流程 / 边界值 / 异常分支 / 并发场景

### 6.2 用例实现模板

```cpp
// AC-XXX-01: <验收项描述>
TEST_F(TestUbse<Module>, Xxx_ValidInput_ReturnsOk)
{
    // Given(测试数据,从测试设计文档 9.X 提取)
    Ubse<Module>XxxReq req;
    req.name = "test-001";
    req.size = 1024 * 1024 * 1024;  // 1GB

    // When
    uint32_t ret = service_->XxxSpace(req, identity_, result_);

    // Then
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("test-001", result_.name);
    // ...(按 AC-XXX-01 验收点)
}

// AC-XXX-02: <异常场景验收项>
TEST_F(TestUbse<Module>, Xxx_DuplicateName_IdempotentReturnOk)
{
    // Given: 已存在同名条目
    // When: 再次调用
    // Then: 返回 OK,结果与首次一致(幂等)
}
```

### 6.3 Mock 实现

按测试设计文档 Mock 清单生成:

```cpp
// mock_<name>.h
class MockXxx : public Xxx {
public:
    MOCK_METHOD(uint32_t, DoSomething, (const Req &), (override));
    // ...(按测试设计文档 Mock 方法清单)
};
```

## 7. 阶段 5:自检与验证

### 7.1 设计一致性 checklist

生成代码后,对照设计文档逐项自检:

**接口一致性**:
- [ ] 服务契约的纯虚接口数量、签名、返回类型与设计文档 4.1 一致
- [ ] 每个 FR-XXX 有对应接口实现
- [ ] 接入层 handler 数量与设计文档 6.X 一致

**数据模型一致性**:
- [ ] 枚举值与设计文档 4.2 一致
- [ ] struct 字段、类型、顺序与设计文档 4.3 一致
- [ ] 状态机状态与转换与设计文档 4.4 一致

**流程一致性**:
- [ ] 关键流程(7.X)的步骤在代码中有对应实现
- [ ] 异常分支(7.X.4)有对应错误处理与回滚
- [ ] 并发控制(8.1)的锁层次在代码中正确声明与使用

**测试一致性**:
- [ ] TestSuite/TestName 与测试设计文档一致
- [ ] 每个 AC-XXX 有对应 UT 用例
- [ ] Mock 依赖与测试设计文档一致

### 7.2 编码规范 checklist

调用 [coding-standards](../coding-standards/SKILL.md) skill 的自检清单:
- [ ] 版权头、头文件保护、Include 顺序
- [ ] 命名规范(CamelCase/camelBack/UPPER_CASE/`_`后缀/`g_`前缀)
- [ ] 格式化(4 空格、120 行宽、Allman/K&R 混合)
- [ ] 现代 C++(using/nullptr/override/emplace_back/make_unique)
- [ ] RAII(智能指针、lock_guard)
- [ ] 函数体 ≤ 50 语句

### 7.3 构建与测试验证

```shell
# 1. Release 构建
bash build.sh

# 2. 仅编译 UT(不运行,快速验证编译通过)
bash build.sh ut --skip-run-tests

# 3. 运行模块 UT
bash build.sh ut -- --gtest_filter="Test<Module>.*"

# 4. 运行所有 UT
bash build.sh ut

# 5. 生成覆盖率报告(可选)
bash build.sh ut -C
```

### 7.4 追溯矩阵更新

生成代码后,更新端到端追溯矩阵(若存在):

| AC-XXX | FR-XXX | 设计决策 | 代码位置 | UT 用例 |
|--------|--------|----------|----------|---------|
| AC-ALLOC-01 | FR-ALLOC-01 | ServiceImp::AllocSpace | [ubse_ssu_service_imp.cpp#L123](../../../src/controllers/ssu/ubse_ssu_service_imp.cpp) | TestUbseSsuServiceImp.AllocSpace_ValidInput_ReturnsOk |

追溯矩阵归档到:`docs/design/modules/<模块名>/追溯矩阵.md`

## 8. 与其他 skill 的关系

```
┌──────────────────────────────────────────────────────────┐
│  文档驱动交付链路(需求 → 设计 → 测试 → 代码)               │
│                                                          │
│  需求分析  → requirement-analysis-doc                     │
│  模块设计  → module-design-doc                            │
│  测试设计  → test-design-doc                              │
│  代码开发  → doc-driven-implementation(本 skill)          │
│  测试开发  → doc-driven-implementation(本 skill)          │
│                                                          │
│  代码规范约束    → coding-standards(被本 skill 调用)       │
└──────────────────────────────────────────────────────────┘
```

| skill | 职责 | 与本 skill 的关系 |
|-------|------|-------------------|
| requirement-analysis-doc | 生成需求文档 | 上游,提供 FR/NFR/AC |
| module-design-doc | 生成设计文档 | 上游,提供接口/数据模型/流程 |
| test-design-doc | 生成测试设计文档 | 上游,提供测试场景/用例规划 |
| coding-standards | 编码规范约束 | 并行,本 skill 调用其 checklist |
| [SSU模块开发指南](../../docs/design/modules/ssu/SSU模块开发指南.md) | SSU 专项 | SSU 模块可参考本 skill |

## 9. 示例

### 9.1 示例:从设计文档实现一个新接口

**场景**:SSU 模块新增 `SnapshotSpace` 接口,已有设计文档。

**输入**:
- [SSU模块需求分析文档.md](../../docs/design/modules/ssu/SSU模块需求分析文档.md)(含 FR-SNAP-01~05)
- [SSU模块特性设计文档.md](../../docs/design/modules/ssu/SSU模块特性设计文档.md)(含 4.1 接口表新增 `SnapshotSpace`、5.X 快照子模块、7.X 快照时序图)
- [SSU模块测试设计文档.md](../../docs/design/modules/ssu/SSU模块测试设计文档.md)(含 TestUbseSsuSnapshot 用例)

**执行步骤**:

1. **阶段 1**:读取三件套,提取 FR-SNAP-01~05、`SnapshotSpace` 接口签名、快照子模块类设计、快照时序图、TestUbseSsuSnapshot 用例清单
2. **阶段 2**:
   - 在 `ubse_ssu_service.h` 添加 `SnapshotSpace` 纯虚接口 + Doxygen
   - 新建 `ubse_ssu_snapshot.{h,cpp}`(快照子模块)
   - 在 `ubse_ssu_service_imp.h` 添加 `snapshot_` 成员
3. **阶段 3**:
   - 在 `ubse_ssu_service_imp.cpp` 实现 `SnapshotSpace`(按时序图:角色判定→幂等→调用 snapshot_→更新账本→错误回滚)
   - 实现 `UbseSsuSnapshot` 子模块关键方法
4. **阶段 4**:
   - 新建 `test/UT/controllers/ssu/test_ubse_ssu_snapshot.cpp`
   - 实现 `TestUbseSsuSnapshot` 夹具 + 用例(覆盖 FR-SNAP-01~05 对应的 AC-SNAP-XX)
5. **阶段 5**:
   - 自检设计一致性(接口/数据模型/流程/测试)
   - 自检编码规范(调用 coding-standards checklist)
   - `bash build.sh` 验证编译
   - `bash build.sh ut -- --gtest_filter="TestUbseSsuSnapshot.*"` 验证用例
   - 更新追溯矩阵

### 9.2 示例:从零实现新模块

**场景**:实现全新的"配额管理"模块,已有三件套文档。

**执行步骤**:

1. **阶段 1**:读取 `docs/design/modules/quota/` 下三件套,提取 FR-QUOTA-XX、接口契约、子模块、时序图、用例
2. **阶段 2**:按 4.1 文件生成顺序生成全部骨架:
   - `src/include/plugin_services/quota/ubse_quota_service.h`
   - `src/controllers/quota/ubse_quota_controller_module.{h,cpp}`
   - `src/controllers/quota/ubse_quota_service_imp.{h,cpp}`
   - `src/controllers/quota/ubse_quota_validator.{h,cpp}`(子模块)
   - `src/controllers/quota/ipc/...`(接入层)
   - `src/controllers/quota/CMakeLists.txt`
   - `test/UT/controllers/quota/test_ubse_quota_*.cpp`
3. **阶段 3**:填充业务逻辑(校验、配额检查、扣减、回滚)
4. **阶段 4**:实现 UT(覆盖 AC-QUOTA-XX)
5. **阶段 5**:自检 + 构建验证 + 追溯矩阵

## 10. 参考文档

- [编码规范文档](../../docs/conventions/编码规范文档.md) — 编码规范源头
- [目录规范文档](../../docs/conventions/目录规范文档.md) — 目录归档约束
- [AGENTS.md](../../AGENTS.md) — 构建/测试命令
- [coding-standards skill](../coding-standards/SKILL.md) — 编码规范自检清单
- [requirement-analysis-doc skill](../requirement-analysis-doc/SKILL.md) — 需求文档生成
- [module-design-doc skill](../module-design-doc/SKILL.md) — 设计文档生成
- [test-design-doc skill](../test-design-doc/SKILL.md) — 测试设计文档生成
- [SSU模块特性设计文档.md](../../docs/design/modules/ssu/SSU模块特性设计文档.md) — 设计文档示例
- [SSU模块测试设计文档.md](../../docs/design/modules/ssu/SSU模块测试设计文档.md) — 测试设计文档示例
