# RMRS 用户指南

## RMRS 简介

UBS RMRS 的英文全名为 UB Server Core RackMemoryResourceSchedule，是用于集群内存资源调度的组件。本项目为UBS Engine 内rmrs插件。

## 须知

- 需切换为 root 用户执行安装与启动。
- 初始安装需保证环境纯净，否则可能使用环境中的配置导致业务异常。
- 依赖 UBTurbo 服务启动正常，否则无法启动 UBSE。

## 安装 RMRS

- 仅有libmempooling.so文件
从 `memorypooling` 仓库构建出对应包并安装 `mempooling`，将 `libmempooling.so` 放到 `/usr/lib64`。

- RPM包安装

    ```bash
    rpm -ivh rmrs-*.aarch64.rpm
    ```

## 修改配置文件

### /etc/ubse/plugins/plugin_mempooling.conf

```ini
ubse.plugin.name=mempooling
ubse.plugin.pkg=libmempooling.so

# 容器超分场景，是否启用 PageCache 迁移功能
ucache.enable=false
rmrs.ipc.timeout=60

# 内存碎片场景，是否必须同平面（true: 必须同平面，false: 优先同平面）
rmrs.fragment.mustSamePlane=true

# 内存碎片场景，借用是否切分为 4G 粒度（true: 切分，false: 不切分）
rmrs.fragmemt.enableBorrowSplit=true
```

大规格虚机场景下，建议调整为：

```ini
rmrs.fragment.mustSamePlane=false
rmrs.fragmemt.enableBorrowSplit=false
```

### /etc/ubse/ubse_plugin_admission.conf

取消 `mempooling=777` 的注释，示例如下：

```ini
# vm plugin. Default value: 205
# vm=205

# mempooling plugin. Default value: 777
mempooling=777

# ucache plugin. Default value: 778
# ucache=778
```

## 常用命令

### 重启 UBSE

```bash
systemctl restart ubse
```

### 检查 mempooling 日志

```bash
ls -l /var/log/ubse/mempooling_plugin.log
```

输出示例：

```bash
-rw-r----- 1 ubse ubse 20048 Dec 19 10:04 /var/log/ubse/mempooling_plugin.log
```
