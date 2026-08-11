# UBS-Engine SSU C SDK 命令行测试工具

`ubse_ssu_test_c` 是基于 `ubs_engine_ssu.h` 的交互式和单次执行测试客户端。它将命令行位置参数转换为
SDK 请求，并把 SDK 返回值输出为 JSON，适合接口联调、手工验证和脚本调用。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `main.cpp` | CLI 实现 |
| `command-examples.md` | 全部命令、参数及调用示例 |
| `build.sh` | 构建 CLI 程序 |

## 前置条件

- Linux 和支持 C++17 的编译器（默认使用 `g++`）
- 构建程序时需要 `ubs_engine.h` 和 `ubs_engine_ssu.h` 
- 运行命令时需要可访问的 UBS-Engine 服务

脚本使用同目录 `include/` 中的头文件。程序运行时通过 `dlopen` 加载 SDK 动态库。

## 构建

```bash
cd test/tools/c/ssu

# 构建 CLI 程序
bash build.sh app

# 清理本示例构建产物
bash build.sh clean
```

若动态库不在系统默认搜索路径，运行时可指定完整路径：

```bash
UBSE_CLIENT_LIBRARY=/opt/ubse/lib64/libubse-client.so ./build/ubse_ssu_test_c
```

构建结果位于 `test/tools/c/ssu/build/`：

- `ubse_ssu_test_c`：运行时加载 SDK 的程序

## 使用

交互模式：

```text
./build/ubse_ssu_test_c
ubse_ssu_test_c> help
ubse_ssu_test_c> ssu_list_alloc_info
ubse_ssu_test_c> quit
```

单次执行模式：

```bash
./build/ubse_ssu_test_c ssu_get_ns_stats test-space
./build/ubse_ssu_test_c ssu_alloc_space test-space 1073741824 2 4096 0 tenant-a
```

退出码为 `0` 表示成功，`1` 表示未知命令、参数错误或 SDK 错误，`2` 表示执行了 `quit`/`exit`。
所有参数均为必填位置参数；空字符串需显式传入 `""`，枚举参数使用 SDK 定义的十进制数值。
完整命令列表参见 [command-examples.md](command-examples.md)。
