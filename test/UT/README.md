# 组织单元测试

## 运行单元测试

```shell
# 运行所有的单元测试
bash build.sh ut

# 运行某一个模块的单元测试 (以 ubse_http 模块为例)
bash build.sh ubse_http_ut
```

## 添加模块单元测试

默认 ut 会扫描 `test/UT` 下的所有单元测试，这里介绍下如何添加模块单元测试。

以 `test/UT/http` 目录下的 ubse_http 模块的单元测试用例为例。

```cmake
# test/UT/http/CMakeLists.txt
add_ut(ubse_http)
```

```cmake
# test/UT/CMakeLists.txt
add_subdirectory(http)
```
完成模块单元测试的配置后，即可执行构建与运行。

```shell
bash build.sh ubse_http_ut
```

## 查看覆盖率

```shell
bash build.sh ut -C -H
```
