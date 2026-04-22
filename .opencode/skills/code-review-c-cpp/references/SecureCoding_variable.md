
## 禁止使用未经初始化的变量

**【描述】** 
这里的变量，指的是局部动态变量，并且还包括内存堆上申请的内存块。 
因为他们的初始值都是不可预料的，所以禁止未经有效初始化就直接读取其值。

```
void foo( ...)
{
	int data;
	bar(data); // 错误：未初始化就使用
	...
}
```

如果有不同分支，要确保所有分支都得到初始化后才能使用：

```
#define CUSTOMIZED_SIZE 100
void foo( ...)
{
	int data;
	if (condition > 0) {
		data = CUSTOMIZED_SIZE;
	}

	bar(data); // 错误：部分分支该值未初始化
	...
}
```

## 指向资源句柄或描述符的变量，在资源释放后立即赋予新值

**【描述】** 
指向资源句柄或描述符的变量包括指针、文件描述符、socket描述符以及其它指向资源的变量。 
以指针为例，当指针成功申请了一段内存之后，在这段内存释放以后，如果其指针未立即设置为NULL，也未分配一个新的对象，那这个指针就是一个悬空指针。 
如果再对悬空指针操作，可能会发生重复释放或访问已释放内存的问题，造成安全漏洞。 
消减该漏洞的有效方法是将释放后的指针立即设置为一个确定的新值，例如设置为NULL。对于全局性的资源句柄或描述符，在资源释放后，应该马上设置新值，以避免使用其已释放的无效值；对于只在单个函数内使用的资源句柄或描述符，应确保资源释放后其无效值不被再次使用。

**【错误代码示例】** 
如下代码示例中，根据消息类型处理消息，处理完后释放掉body或head指向的内存，但是释放后未将指针设置为NULL。如果还有其他函数再次处理该消息结构体时，可能出现重复释放内存或访问已释放内存的问题。

```
int foo(void)
{
	SomeStruct *msg = NULL;
	... // 初始化msg->type，分配 msg->body 的内存空间
	if (msg->type == MESSAGE_A) {
		...
		free(msg->body);
	}

	...
EXIT:
	...
	free(msg->body);
	return ret;
}
```

**【正确代码示例】** 
如下代码示例中，立即对释放后的指针设置为NULL，避免重复放指针。

```
int foo(void)
{
	SomeStruct  *msg = NULL;
	... // 初始化msg->type，分配 msg->body 的内存空间

	if (msg->type == MESSAGE_A) {
		...
		free(msg->body);
		msg->body = NULL;
	}

	...
EXIT:
	...
	free(msg->body);
	return ret;
}
```

当free()函数的入参为NULL时，函数不执行任何操作。

**【错误代码示例】** 
如下代码示例中文件描述符关闭后未赋新值。

```
SOCKET s = INVALID_SOCKET;
int fd = -1;
...
closesocket(s);
...
close(fd);
...
```

**【正确代码示例】** 
如下代码示例中，在资源释放后，对应的变量应该立即赋予新值。

```
SOCKET s = INVALID_SOCKET;
int fd = -1;
...
closesocket(s);
s = INVALID_SOCKET;
...
close(fd);
fd = -1;
...
```


