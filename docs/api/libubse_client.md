# 1. ubs\_engine\_client\_initialize

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine.h>
int32_t ubs_engine_client_initialize(const char *ubs_engine_uds_path);
```

## 描述 DESCRIPTION

创建ubse客户端，完成内部资源申请，记录uds\_path信息

在后续的流程中，根据传入的uds\_path创建与ubse服务端的socket连接，并将业务端请求发送给ubse服务端，并等待服务端返回结果。

## 参数 Parameters

| name                   | IN/OUT | description                                                                                                                                                  |
| ---------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| ubs\_engine\_uds\_path | IN     | ubse服务端的uds文件路径；传入空路径，则采用ubse默认地址/var/run/ubse/ubse.sock；路径长度限制：遵从linux `sun_path` 的大小是 108 字节 (`#define UNIX_PATH_MAX 108`)，所以路径不能超过 107 个字符（不含结尾的空字符 `\0`） |

## 返回值 RETURN VALUE

返回 `UBS_SUCCESS` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error                            | Description  |
| -------------------------------- | ------------ |
| UBS\_ENGINE\_ERR\_OUT\_OF\_RANGE | 参数数据长度超108字节 |
| UBS\_ENGINE\_ERR\_RESOURCE       | 资源创建失败       |

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作。

```c
#include <stdio.h>
#include <ubs_engine.h>

int main(void)
{
	int32_t ret;

	char *path = "/var/run/ubse/ubse.sock";
	ret = ubs_engine_client_initialize(path);
	if (UBS_SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
	/* Do your work here... */

	/* Do your work here... */

	return 0;
}
```

# 2. ubs\_engine\_client\_finalize

## 库 LIBRARY

ubse库 (/usr/lib64/libubse-client.so)

## 摘要 SYNOPSIS

```c
#include <ubs_engine.h>
void ubs_engine_client_finalize(void);
```

## 描述 DESCRIPTION

销毁ubse客户端，完成内部资源释放

## 参数 PARAMETERS

无

## 返回值 RETURN VALUE

无

## 错误 ERRORS

无

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序初始化UBSE客户端，并基于客户端完成后续操作，并销毁客户端，释放资源。

```c
#include <stdio.h>
#include <ubs_engine.h>

int main(void)
{
	int32_t ret;

	char *path = "/var/run/ubse/ubse.sock";
	ret = ubs_engine_client_initialize(path);
	if (UBS_SUCCESS != ret) {
		perror("init failed.\n");
		return -1;
	}
	
	/* Do your work here... */

	ubs_engine_client_finalize();

	return 0;
}
```

