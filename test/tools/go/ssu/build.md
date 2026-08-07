# SSU Go SDK 测试器编译说明

本文档说明如何在 Linux 环境中编译 `ubse_ssu_test_go`，包括本机编译以及
x86_64、ARM64 交叉编译。

## 环境要求

- Linux 操作系统
- Go 1.21 或更高版本
- UBS-Engine 完整源码
- 本工具位于 UBS-Engine 项目的 `test/tools/go/ssu` 目录下，并在该目录中执行命令

`build.sh`、`Makefile` 和 `go.mod` 都通过相对路径 `../../../..` 定位项目根目录（含根 `go.mod` 与 `src/sdk/go/ssu`），因此本目录位置是硬性要求：必须位于 `test/tools/go/ssu`，不能移到其他位置或项目外。

检查 Go 环境：

```bash
go version
go env GOOS GOARCH
```

测试器通过 `go.mod` 中的相对 `replace` 使用仓库内的 Go SDK，因此无需从网络
下载 UBS-Engine SDK。

## 本机编译

不指定目标架构时，脚本使用当前 Go 环境进行编译：

```bash
./build.sh
```

默认产物：

```text
bin/ubse_ssu_test_go
```

该模式不修改当前环境的 `GOOS`、`GOARCH` 和 `CGO_ENABLED`。

## x86_64 交叉编译

```bash
./build.sh --arch x86_64
```

等效目标环境：

```text
GOOS=linux
GOARCH=amd64
CGO_ENABLED=0
```

默认产物：

```text
bin/ubse_ssu_test_go-linux-amd64
```

`x86` 和 `amd64` 也会被识别为 Linux x86_64 目标。

## ARM64 交叉编译

```bash
./build.sh --arch arm64
```

等效目标环境：

```text
GOOS=linux
GOARCH=arm64
CGO_ENABLED=0
```

默认产物：

```text
bin/ubse_ssu_test_go-linux-arm64
```

`arm`、`ARM` 和 `aarch64` 也会被识别为 Linux ARM64 目标。

## 指定输出文件

使用 `--output` 或 `-o` 指定产物路径：

```bash
./build.sh --arch arm64 --output bin/ubse_ssu_test_go
./build.sh -a x86_64 -o output/ubse_ssu_test_go-x86_64
```

相对路径以 `build.sh` 所在目录为基准。脚本会自动创建输出目录。

## 查看帮助

```bash
./build.sh --help
```

## 验证产物架构

使用 `file` 检查编译产物：

```bash
file bin/ubse_ssu_test_go-linux-amd64
file bin/ubse_ssu_test_go-linux-arm64
```

预期分别包含：

```text
x86-64
ARM aarch64
```

也可以通过 Go 工具读取构建信息：

```bash
go version -m bin/ubse_ssu_test_go-linux-amd64
go version -m bin/ubse_ssu_test_go-linux-arm64
```

## 运行测试器

本机产物可以直接运行：

```bash
bin/ubse_ssu_test_go help
bin/ubse_ssu_test_go ssu_list_alloc_info
```

交叉编译产物需要复制到对应架构的 Linux 系统运行。例如，ARM64 产物不能在
未配置模拟器的 x86_64 系统上直接运行。

## 离线检查

运行测试器目录的全部格式检查、静态检查、单元测试和本机构建：

```bash
make all
```

构建脚本和 Makefile 都会关闭 Go workspace、模块代理及校验数据库，确保使用
仓库内通过 `replace` 映射的 SDK，不回退到网络模块。

## 清理产物

```bash
make clean
```

该命令会删除测试器目录下的整个 `bin` 目录。
