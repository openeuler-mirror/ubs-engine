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

脚本在构建前自动将 `include/` 中的头文件以软链接形式指向
`src/sdk/c/include/` 中的 SDK 源头文件，避免手工副本漂移。
程序运行时通过 `dlopen` 加载 SDK 动态库。

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
UBSE_CLIENT_LIBRARY=/opt/ubse/lib64/libubse-client.so \
UBSE_SSU_CLIENT_LIBRARY=/opt/ubse/lib64/libubse-ssu-client.so \
./build/ubse_ssu_test_c
```

`UBSE_CLIENT_LIBRARY` 指定通用 SDK 库，`UBSE_SSU_CLIENT_LIBRARY` 指定 SSU SDK 库。
未设置时分别默认加载 `libubse-client.so` 和 `libubse-ssu-client.so`。符号会在两个库中查找，
因此也兼容 SSU API 未拆库的旧版 SDK。

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
除三个 detach 命令末尾的可选 `[resource_uuid]` 外，所有参数均为必填位置参数；空字符串需显式传入
`""`，枚举参数使用 SDK 定义的十进制数值。
完整命令列表参见 [command-examples.md](command-examples.md)。

## 精确卸载（resource_uuid）

脑裂合并后可能出现同名多份分配。detach 命令（`ssu_detach_space`、`ssu_detach_linear_space`、
`ssu_detach_striped_space`）支持可选尾随参数 `[resource_uuid]`：先用 `ssu_list_alloc_info` 查询
各次分配的 `ResourceUuid`，再带该参数精确卸载其中一份（按 name+resource_uuid 复合键命中，跳过同名
多份的歧义拒绝；未命中报空间不存在）。不传时按 name 解析，同名多份会被拒绝，行为与从前一致。
resource_uuid 长度 1~36、仅允许 `[0-9a-f-]`，工具在本地校验，非法时直接报错不发起 SDK 调用；
后续可用 `ssu_free_space_by_resource_uuid` 按同一 resource_uuid 精确释放对应分配。
