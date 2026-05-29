
## 确保字符串存储有足够的空间容纳字符数据和null结束符

**【描述】** 
将数据复制到不足以容纳数据的缓冲区，会导致缓冲区溢出。缓冲区溢出经常发生在字符串操作中。为了避免这种错误，截断拷贝的数据以限制字符串的字节长度是一种防御方法，但是最好的措施是确保目标缓冲区的大小足以容纳复制数据和null结束符。当字符串存储在堆空间时， 确保分配内存时已分配了足够的空间。

部分字符串处理函数由于设计时安全考虑不足，或者存在一些隐含的目的缓冲区长度要求，容易被误用，导致缓冲区写溢出。此类典型函数包括不在C标准库函数中的itoa()，realpath()函数。

**【错误代码示例】**(itoa) 
有些函数如itoa(), realpath()需要在对传入的缓冲区指针位置进行写入操作，但函数并没有提供缓冲区长度。因此，在调用这些函数前，必须提供足够的缓冲区。 
如下代码示例中，试图将数字转为字符串，但是目标存储空间的预留长度不足：

```
int num = ...
char str[8];
itoa(num, str, 10); // 10进制整数的最大存储长度是12个字节
```

**【正确代码示例】** 
如下代码示例中，在对外部数据进行解析并将内容保存到name中，考虑了name的大小：

```
int num = ...
char str[13];
itoa(num, str, 10); // 10进制整数的最大存储长度是12个字节
```

【错误代码示例】**(realpath)**

如下代码示例中，试图将路径标准化，但是目标存储空间的长度不足：

```
#define MAX_PATH_LEN 100
char resolved_path[MAX_PATH_LEN];
/ *
- realpath函数的存储缓冲区长度是由PATH_MAX常量定义，
- 或是由_PC_PATH_MAX系统值配置的，通常都大于100字节
*/

char  *res = realpath(path, resolved_path);
...
```

**【正确代码示例】**

可以将realpath的第二个参数传入NULL, 以让系统自动分配合适的内存。

```
char *resolved_path = NULL;
resolved_path = realpath(path, NULL);
if (resolved_path == NULL) {
	... // 处理错误
}

...
if (resolved_path != NULL) {
	free(resolved_path);
	resolved_path = NULL;
}
...
```

## 对字符串进行存储操作，确保字符串有null结束符

**【描述】** 
部分字符串处理函数操作字符串时，将截断超出指定长度的字符串，如strncpy()函数最多复制n个字符到目的缓冲区，如果源字符串长度大于n，则不会写入null结束符到目的缓冲区，目的缓冲区的内容为n个被复制的字符。使用这类函数时，可能会无意截断导致数据丢失，并在某些情况下会导致软件漏洞。 
因此，对字符串进行存储操作，必须确保字符串有null结束符，否则在后续的调用strlen等操作中，可能会导致内存越界访问漏洞。

**【错误代码示例】** 
在如下代码示例中，使用strncpy函数复制字符串时可能会发生截断（发生条件为：strlen(name) > sizeof(file_name) - 1）。当发生截断时，file_name的内容是不完整的，并且缺少 '  0 '结束符，后续对file_name的操作可能会导致软件漏洞：

```
#define FILE_NAME_LEN 128
char file_name [FILE_NAME_LEN ];
(void)strncpy(file_name, name, sizeof(file_name) - 1);
...
```

**【正确代码示例】**

```
#define FILE_NAME_LEN 128
char file_name[FILE_NAME_LEN ];

if (strlen(name)  > FILE_NAME_LEN - 1) {
	... // 处理错误
}

(void)strcpy(file_name, name);
...
```

**【例外】**

程序员的目的是故意截断字符串。

**【相关软件CWE编号】** CWE-170，CWE-464

