
## 1 总体设计原则

UBS提供统一的命令行工具ubsectl

命令行风格整体遵从linux常规的命令行规范（UNIX风格和GNU风格）

具体命令行格式：“ubsectl  <命令字>  <操作对象>  [{<参数名1>  [参数值1]} … … {<参数名1>  [参数值1]}]”

## 2 命令字

定义系统能够执行的功能，当前可选命令字如下：

| 关键字     | 关键字说明           |
|---------|-----------------|
| import  | 向UBSE导入信息       |
| remove  | 从UBSE中移除先前导入的信息 |
| create  | 在UBSE中创建资源      |
| delete  | 从UBSE中删除资源      |
| change  | 修改UBSE中的信息或资源   |
| display | 查看UBSE中的信息或资源   |
| attach  | 在UBSE中绑定资源      |
| detach  | 从UBSE中解绑资源      |

## 3 操作对象

用于指定命令字的操作对象，由UBSE中的能力决定，比如：memory、topo、cert等

## 4 参数列表

命令字执行时需要的参数，包括一对或多对参数名和参数值

参数名：可遵从长选项风格（两个连接符“--”后面紧跟单词)，也可遵从短选项风格（单个连接符“-”后面紧跟单字母）

参数值：与参数名用空格分开，表达具体的参数取值；参数名可以没有参数值

## 5 命令行举例

```
# 查询内存借用账本详细信息
ubsectl display memory -t borrow_detail
# 触发内存借用
ubsectl create memory -t numa -s <size> -n <name>
```

