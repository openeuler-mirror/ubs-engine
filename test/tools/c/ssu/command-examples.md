# 命令示意：ubse_ssu_test_c

## 1. 基本格式

命令扁平下划线形式，可在交互提示符后输入，也可直接跟在 `ubse_ssu_test_c` 后执行一次。

```text
ubse_ssu_test_c> help
ubse_ssu_test_c> help ssu_alloc_space
ubse_ssu_test_c> ssu_list_alloc_info
```

尖括号表示有序位置参数。不存在方括号或可选参数；命令列出的每个参数都必须输入。所有枚举值均填写
SDK 底层十进制数值。需要空字符串时输入 `""`，需要数值零时输入 `0`。

## 2. 查询命令

```text
ssu_list_alloc_info

ssu_get_ns_stats <name>

ssu_get_connect_info <name> <vfe_present> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id>
  <vfe_guid> <bind_bus_instance_guid>

ssu_get_fe_device_list
```

示例：

```text
ssu_get_ns_stats test-space
ssu_get_connect_info test-space 0 0 0 0 0 0 "" ""
ssu_get_connect_info test-space 1 0 0 0 0 0 "" ""
ssu_get_connect_info test-space 1 1 0 0 0 2 \
  00112233445566778899aabbccddeeff 11112222333344445555666677778888
```

`vfe_present=0` 时向 SDK 传 `nil`，但仍必须写全后续占位参数；`vfe_present=1` 时使用后续全部
字段组装非空 VFE。第二条示例专门表示“非空指针指向零值 VFE”，它与第一条语义不同。

## 3. 分配与权限

```text
ssu_alloc_space <name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>
ssu_free_space <name>
ssu_add_access_permission <name> <nqn>
ssu_remove_access_permission <name> <nqn>
```

示例：

```text
# lba_format=4096，strategy=0；CLI 不把它们转换为名称
ssu_alloc_space test-space 1073741824 2 4096 0 tenant-a
ssu_add_access_permission test-space nqn.2024-01.example:host-a
ssu_remove_access_permission test-space nqn.2024-01.example:host-a
ssu_free_space test-space
```

## 4. 普通空间挂卸载

```text
ssu_attach_space <name> <nqn> <src_eid>
ssu_detach_space <name> <nqn> <src_eid>
```

示例：

```text
ssu_attach_space test-space nqn.2024-01.example:host-a 0000000000000001
ssu_detach_space test-space nqn.2024-01.example:host-a 0000000000000001
```

## 5. 线性空间挂卸载

```text
ssu_attach_linear_space <name> <nqn> <src_eid> <dev_name>
ssu_detach_linear_space <name> <nqn> <src_eid> <dev_name>
```

示例：

```text
ssu_attach_linear_space test-space nqn.2024-01.example:host-a 0000000000000001 test-linear
ssu_detach_linear_space test-space nqn.2024-01.example:host-a 0000000000000001 test-linear
```

## 6. 条带空间挂卸载

```text
ssu_attach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>
ssu_detach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>
```

示例：

```text
# level=0，chunk_size=64；CLI 直接强制转换为 SDK 枚举类型
ssu_attach_striped_space test-space nqn.2024-01.example:host-a 0000000000000001 test-striped 0 64
ssu_detach_striped_space test-space nqn.2024-01.example:host-a 0000000000000001 test-striped 0 64
```

## 7. FE 设备

```text
ssu_fe_device_alloc <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id>
  <vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>

ssu_fe_device_free <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id>
  <vfe_guid> <bind_bus_instance_guid>
```

示例：

```text
ssu_fe_device_alloc 1 1 0 0 10 20 00112233445566778899aabbccddeeff \
  11112222333344445555666677778888 9999aaaabbbbccccddddeeeeffff0000

ssu_fe_device_free 1 1 0 0 10 20 00112233445566778899aabbccddeeff \
  11112222333344445555666677778888
```

## 8. 透传说明

- `ns_size` 不接受 `1G` 等单位形式，只接受可解析为 `uint64` 的十进制数值。
- `lba_format`、`strategy`、`level`、`chunk_size` 不接受符号名称。
- CLI 不判断这些数值是否属于 SDK 支持集合；SDK 决定调用成功或失败。
- shell 引号只用于把包含空格的文本组成一个参数，去除引号后的字符串内容传给 SDK。
- 命令不接受参数省略；空值必须使用 `""` 或相应数值类型的 `0` 显式占位。

## 9. 应答示意

每条实际 SDK 调用都打印 JSON 应答。有业务返回值时完整编码；只有 `error` 返回值的接口成功时打印
固定成功应答。

```text
# 单返回值
{"response":{"Name":"test-space","Strategy":0,"Namespaces":[]}}

# 列表返回值
{"response":[{"NsUuid":"ns-a","NsId":1,"TotalSize":1024,"UsedSize":128}]}

# 两个业务返回值
{"response":[["/dev/nvme0n1","/dev/nvme1n1"],"/dev/mapper/test-linear"]}

# 只有 error 返回，且 error == nil
{"response":"success"}

# SDK 返回错误
{"error":"SDK 原始错误文本"}
```

结构体字段、列表元素和多返回值不能省略或重排；nil 列表使用 `null`，空列表、空字符串和数值零也必须
显式显示。帮助文本不属于 SDK 应答，保持普通文本。
