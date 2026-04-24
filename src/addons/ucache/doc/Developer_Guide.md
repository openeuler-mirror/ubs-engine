# UBS uCache开发者指南

## 源码编译构建指导

**根目录**

```shell
sh build.sh
```

**参数说明**

| 参数名                     | 有效性参数                 | 参数类型     | 描述                                                         |
| -------------------------- | -------------------------- | ------------ | ------------------------------------------------------------ |
| `-ub`                      | 无                         | 开关参数     | 启用 UB 环境变量设置 (`ub_environment="ON"`)                 |
| `-h`, `--help`             | 无                         | 帮助参数     | 显示帮助信息并退出                                           |
| `-D`, `--debug`            | 无                         | 模式参数     | 设置构建类型为 Debug                                         |
| `-T`, `--type`             | 无                         | 模式参数     | 设置构建类型（如 Debug、Release）                            |
| `-t`, `--target`           | 无                         | 构建目标参数 | 设置构建目标（如具体组件名称）                               |
| `-C`, `--coverage`         | 无                         | 开关参数     | 启用代码覆盖率功能 (`enable_coverage='ON'`)                  |
| `-H`, `--http-server`      | 无                         | 开关参数     | 启用 HTTP 服务功能 (`enable_http_server='ON'`)               |
| `-c`, `--clean`            | 无                         | 开关参数     | 清理构建结果 (`enable_clean='ON'`)                           |
| `-S`, `--source-compiling` | 无                         | 开关参数     | 启用源代码编译模式 (`enable_source_compiling='ON'`)          |
| `-j`, `--jobs`             | 无                         | 并行参数     | 指定构建时的并行 job 数量                                    |
| `--std`                    | 无                         | 编译参数     | 设置 C++ 标准版本（如 11、14、17）                           |
| `-v`, `--verbose`          | 无                         | 日志参数     | 启用详细输出（设置环境变量 `VERBOSE=1`）                     |
| `-V`, `--deploy-version`   | 无                         | 版本参数     | 设置部署版本号（如 2.0.0.B099）                              |
| `--skip-run-tests`         | 无                         | 开关参数     | 跳过运行测试阶段                                             |
| `--asan`                   | 无                         | 开关参数     | 启用 AddressSanitizer (`enable_asan='ON'`)                   |
| `--lsan`                   | 无                         | 开关参数     | 启用 LeakSanitizer (`enable_lsan='ON'`)                      |
| `--tsan`                   | 无                         | 开关参数     | 启用 ThreadSanitizer (`enable_tsan='ON'`)                    |
| `--ubsan`                  | 无                         | 开关参数     | 启用 UndefinedBehaviorSanitizer (`enable_ubsan='ON'`)        |
| `--ninja`                  | 无                         | 构建工具参数 | 使用 Ninja 构建系统 (`generator='Ninja'`)                    |
| `--make`                   | 无                         | 构建工具参数 | 使用 Unix Makefiles 构建系统 (`generator='Unix Makefiles'`)  |
| `--`                       | 后续为传递参数             | 特殊标志     | 表示后续参数为传递给其它命令的参数（保存在 `trans_params` 中） |
| `*`                        | 条件处理（默认目标或错误） | 位置参数     | 未识别的参数，若未设置 trans_flag，则视为构建目标（`build_target`） |

**约束和注意事项**
产物目录`cmake-build-release/lib`，包含产物libucache_plugin.so
