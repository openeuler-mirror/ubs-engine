# UBS Engine 设计文档归档索引

> 本目录归档 UBS Engine 的所有设计文档,按"架构 / 框架 / 模块"三层组织。
> 图片资源统一存放在 `images/` 下,按相同层级分目录。

## 目录结构

```
docs/design/
├── architecture/         # 架构设计(顶层)
│   ├── architecture.md
│   └── UBSE技术架构设计文档.md
├── framework/            # 框架设计(横切关注点)
│   ├── 安全设计.md
│   ├── UBSE角色访问控制设计.md
│   ├── 模块与插件加载统一化设计.md
│   └── uds_api.md
├── modules/              # 模块设计(业务模块,按模块分子目录)
│   ├── mem_pool/         # 内存池化模块
│   │   ├── scheduler.md
│   │   └── 内存池化借用性能规格设计.md
│   └── ssu/              # SSU 存储子系统模块(主体 + 特性扩展)
│       ├── SSU模块需求分析文档.md           # 主体三件套
│       ├── SSU模块设计文档.md
│       ├── SSU模块测试设计文档.md
│       ├── SSU模块开发指南.md               # 开发规范与排查指南
│       ├── virt/                        # 虚机化特性文档
│       │   ├── SSU模块虚机化特性需求分析文档.md
│       │   └── SSU模块虚机化特性模块设计文档.md
│       └── cli/                         # CLI 特性文档
│           ├── SSU模块CLI特性需求分析文档.md
│           ├── SSU模块CLI特性设计文档.md
│           └── SSU模块CLI特性测试设计文档.md
├── images/               # 图片资源(按层级分目录)
│   ├── architecture/     # 架构图
│   ├── framework/        # 框架设计图
│   └── modules/          # 模块设计图
└── README.md             # 本索引文件
```

## 文档清单

### 一、架构设计(`architecture/`)

描述 UBS Engine 的整体架构、软件定位与核心能力。

| 文档 | 说明 |
|------|------|
| [architecture.md](architecture/architecture.md) | UBS Engine 顶层业务架构,含软件定位、部署形态、模块组成、依赖关系、关键业务流程与可靠性能力 |
| [UBSE技术架构设计文档.md](architecture/UBSE技术架构设计文档.md) | UBS Engine 技术架构设计,含架构原则、分层设计、模块化机制、通信架构、高可用架构、安全架构、插件化架构与关键设计模式 |

### 二、框架设计(`framework/`)

描述跨模块复用的框架级设计(安全、通信、加载机制等横切关注点)。

| 文档 | 说明 |
|------|------|
| [安全设计.md](framework/安全设计.md) | 安全架构,含最小化特权、暴露面安全(UDS/RPC/配置/lib) |
| [UBSE角色访问控制设计.md](framework/UBSE角色访问控制设计.md) | 轻量化 RBAC 能力,权限模型元素定义与权限矩阵 |
| [模块与插件加载统一化设计.md](framework/模块与插件加载统一化设计.md) | 模块与插件的统一注册、加载、场景切换机制 |
| [uds_api.md](framework/uds_api.md) | UDS IPC 序列化与反序列化规则,UDS 文件路径与权限 |

### 三、模块设计(`modules/`)

描述各业务控制器模块的需求、设计与测试,按模块分目录。

#### 3.1 内存池化模块(`modules/mem_pool/`)

| 文档 | 类型 | 说明 |
|------|------|------|
| [scheduler.md](modules/mem_pool/scheduler.md) | 设计 | 内存借用决策原理,含决策原则、借用算法、成环避免 |
| [内存池化借用性能规格设计.md](modules/mem_pool/内存池化借用性能规格设计.md) | 性能规格 | 借用速率规则、性能指标与配置要求 |

#### 3.2 SSU 存储子系统模块(`modules/ssu/`)

| 文档 | 类型 | 说明 |
|------|------|------|
| [SSU模块需求分析文档.md](modules/ssu/SSU模块需求分析文档.md) | 需求分析 | 业务背景、功能/非功能需求、验收标准(12 章) |
| [SSU模块设计文档.md](modules/ssu/SSU模块设计文档.md) | 设计 | 架构、子模块、时序图、并发控制(13 章) |
| [SSU模块测试设计文档.md](modules/ssu/SSU模块测试设计文档.md) | 测试设计 | UT/IT/PT/FUZZ/E2E 测试体系(14 章) |
| [SSU模块开发指南.md](modules/ssu/SSU模块开发指南.md) | 开发指南 | 架构概览、文件索引、开发规范、问题排查、测试编写、关键约束 |
| [SSU模块虚机化特性需求分析文档.md](modules/ssu/virt/SSU模块虚机化特性需求分析文档.md) | 需求分析 | SSU 模块虚机化特性扩展:VM 模式部署、vsock 通信、查询/attach/detach、CLI、host 推送(混合模式) |
| [SSU模块虚机化特性模块设计文档.md](modules/ssu/virt/SSU模块虚机化特性模块设计文档.md) | 设计 | 特性扩展设计:VM 侧 vsock server/host 侧 client、缓存、服务实现、推送执行器、时序图(混合模式) |
| [SSU模块CLI特性需求分析文档.md](modules/ssu/cli/SSU模块CLI特性需求分析文档.md) | 需求分析 | SSU 模块 CLI 特性需求规约:启动/注册/display/create/attach/detach/校验/IPC/wire/输出/认证/覆盖边界(代码反推模式,12 章) |
| [SSU模块CLI特性设计文档.md](modules/ssu/cli/SSU模块CLI特性设计文档.md) | 设计 | 特性设计:模块架构、子模块、服务接口、关键流程时序图、并发控制、设计模式(代码反推模式,13 章) |
| [SSU模块CLI特性测试设计文档.md](modules/ssu/cli/SSU模块CLI特性测试设计文档.md) | 测试设计 | 特性测试:UT/IT/PT/FUZZ/E2E 测试体系,映射现有 50 用例 + 规划 53 待补用例(14 章) |

> **说明**:SSU 虚机化特性与 CLI 特性均是对现有 SSU 模块的能力扩展。虚机化特性通过 `vm_mode` 配置开关在 `UbseSsuControllerModule` 中分流,代码归入 `src/controllers/ssu/`;CLI 特性为独立可执行程序 `ubsectl-ssu`,通过 UDS 短连接与 `ubse` daemon 通信,代码归入 `src/cli/ssu/`。两者文档统一归入 SSU 模块目录。

## 归档规范

### 文档分类

| 层级 | 目录 | 适用文档 |
|------|------|----------|
| 架构 | `architecture/` | 顶层架构、整体能力图谱 |
| 框架 | `framework/` | 跨模块复用的框架级设计(安全/通信/加载/配置) |
| 模块 | `modules/<模块名>/` | 业务模块的需求/设计/测试文档 |

### 命名规范

- 文档名使用中文或英文,保持与原文件名一致
- 新增模块文档建议遵循三件套:`<模块名>模块需求分析文档.md` / `<模块名>模块设计文档.md` / `<模块名>模块测试设计文档.md`

### 图片归档

图片统一存放在 `images/` 下,按层级分目录,与文档目录结构对齐:

| 图片目录 | 对应文档目录 |
|----------|--------------|
| `images/architecture/` | `architecture/` |
| `images/framework/` | `framework/` |
| `images/modules/` | `modules/`(所有模块共用) |

文档内引用图片时使用相对路径:
- `architecture/*.md` → `../images/architecture/xxx.png`
- `framework/*.md` → `../images/framework/xxx.png`
- `modules/<module>/*.md` → `../../images/modules/xxx.png`

### 文档生成 skill

文档生成可借助 `.trae/skills/` 下的 skill 自动触发:

| skill | 文档类型 |
|-------|----------|
| `requirement-analysis-doc` | 需求分析文档(12 章) |
| `module-design-doc` | 模块设计文档(13 章) |
| `test-design-doc` | 测试设计文档(14 章) |

## 相关文档

- [UBSE代码Wiki.md](architecture/UBSE代码Wiki.md) —— 项目代码 Wiki(整体架构/目录结构/模块职责/关键类函数)
- [ubse使用手册.md](../ubse使用手册.md) —— 使用手册
- [docs/api/](../api/) —— API 文档
- [docs/cli/](../cli/) —— CLI 用户指南
- [docs/build_install/](../build_install/) —— 构建与部署
- [docs/config/](../config/) —— 配置说明
- [docs/test/](../test/) —— 测试指南(单元测试开发指南)
