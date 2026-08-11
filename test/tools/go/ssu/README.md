# SSU Go SDK 测试器

`ubse_ssu_test_go` 是 `src/sdk/go/ssu` 的人工测试入口。运行 `make build` 或 `./build.sh` 使用当前环境构建；无参数启动交互模式，带参数时执行一次命令，例如 `bin/ubse_ssu_test_go ssu_list_alloc_info`。
所有参数均为必填位置参数。字符串空值写作 `""`，数值零写作 `0`；枚举只接受底层十进制数值，不接受名称或业务别名。
本目录通过 `go.mod` 的相对 `replace` 将 SDK 模块映射到仓库根目录 `../../../..`，直接使用本地 `src/sdk/go/ssu`。Makefile 关闭 workspace、模块代理和校验数据库，防止覆盖映射或回退网络。

## 文档说明

本目录包含以下 Markdown 文档：

- `README.md`：本文件，工具总览与快速上手——用途、编译方式、命令使用要点和离线验证入口。
- `BUILD.md`：编译说明——本机编译、x86_64/ARM64 交叉编译、指定输出文件、产物架构验证以及 `make clean` 清理。
- `command-examples.md`：命令示例——16 条 SDK 命令的完整用法示例，包括参数顺序、占位符约定（`""`、`0`）与 VFE 特殊场景。

## 编译

本工具位于 UBS-Engine 项目的 `test/tools/go/ssu` 目录；`build.sh`、`Makefile` 和 `go.mod` 依赖 `../../../..` 定位项目根目录与本地 SDK，不能移到其他位置或项目外。
直接执行 `./build.sh` 即可使用当前 Go 环境构建，产物位于本目录下的 `bin/ubse_ssu_test_go`。
交叉编译（x86_64/ARM64）与指定输出文件等详细说明参见 [build.md](build.md)。

## 命令

运行 `bin/ubse_ssu_test_go help` 查看全部 16 条 SDK 命令，使用 `help <command>` 查看参数顺序；各命令的详细参数说明与完整示例参见 [command-examples.md](command-examples.md)。
`ssu_get_connect_info` 的 `vfe_present=0` 传入 nil，`1` 使用随后七个字段构造 VFE；占位字段仍必须写全。
交互模式在单行内按 shell 的引用与反斜线规则拆词，但不支持续行，也不执行任何展开。
交互模式还支持 `help`、`?`、`quit` 和 `exit`，SDK 错误会原样显示并继续循环。
SDK 成功应答统一打印为 `{"response":...}`，多返回值按 SDK 顺序放入 `response` 数组；
SDK、参数和命令错误统一打印为 `{"error":"..."}`。帮助文本和交互提示保持普通文本。

## 离线验证
运行 `make all`；单元测试向命令分发器注入 SDK 函数替身，不连接 Unix Socket，也不访问 SSU 设备。
