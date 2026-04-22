
## 数组作为函数参数时，必须同时将其长度作为函数的参数

**【描述】** 
通过函数参数传递数组函数参数必须同时传递数组可容纳元素的个数，而不是以字节为单位的数组最大大小；同样，通过函数参数传递一块内存进行读写操作时，必须同时传递内存块大小，否则函数在访问内存偏移时，无法判断偏移的合法范围，产生越界访问的漏洞。在本规则中所说的"数组"不仅局限为数组类型变量，还包括字符串或指向连续内存块的指针。

**【错误代码示例】** 
如下代码示例中，函数pars_msg不知道msg的范围，容易产生内存越界访问漏洞。

```
int parse_msg(unsigned char *msg)
{
	...
}

void foo(void)
{
	size_t len = get_msg_len();
	...
	unsigned char *msg = (unsigned char  *)malloc(len);
	...
	parse_msg(msg);
	...
}
```

**【正确代码示例】** 
正确的做法是将msg的大小作为参数传递到parse_msg中，如下代码：

```
int parse_msg(unsigned char *msg, size_t msg_len)
{
	ASSERT(msg != NULL);
	ASSERT(msg_len != 0);
	...
}

void foo(void)
{
	size_t len = get_msg_len();
 	...
	unsigned char *msg = (unsigned char *)malloc(len);

 	...
	parse_msg(msg, len);
 	...
}
```

## 函数的指针参数如果不是用于修改所指向的对象就应该声明为指向const的指针

**【描述】** 
const 指针参数，将限制函数通过该指针修改所指向对象，使代码更牢固、更安全。

示例：如strncmp 的例子，指向的对象不变化的指针参数声明为const。

```
// 正确：不变参数声明为const
int strncmp(const char *s1, const char *s2, size_t n);
```

注意：

指针参数要不要加const取决于函数设计，而不是看函数实体内有没有发生"修改对象"的动作。

## 调用格式化输入/输出函数时，禁止format参数受外部数据控制

**【描述】** 
调用格式化函数时，如果format参数由外部数据提供，或由外部数据拼接而来，会造成字符串格式化漏洞。

攻击者如果能够完全或者部分控制格式字符串内容，可以使被攻击的进程崩溃、查看栈内容、查看内存内容或者在任意内存位置写入数据。结果是，攻击者能够以被攻击进程的权限执行任意代码。

格式化输出函数特别危险，这是因为许多程序员没有意识到它们是具有攻击能力的。比如：格式化输出函数可以使用%n转换符，向指定地址写入一个整数值。

这些格式化函数有： 
格式化输出函数: xxxprintf; 
格式化输入函数: xxxscanf; 
格式化错误消息函数: err(), verr(), errx(), verrx(), warn(), vwarn(), warnx(), vwarnx(), error(), error_at_line(); 
格式化日志函数: syslog(), vsyslog().

**【错误代码示例】** 
如下代码示例中的incorrect_password()函数的功能是在身份验证无效时（指定用户没有找到或者密码不正确），显示一条错误信息。 
该函数接受一个源自用户的字符串数据user，而user是未验证的，是外部可控的。 
该函数将user构造一条错误信息，然后用C语言标准函数fprintf打印到stderr。

```
// 调用者需保证入参user的长度被限制为256个字节或者更少
void incorrect_password(const char *user)
{
	int ret = -1;
	static const char msg_format[] = "%s cannot be authenticated.\n";
	size_t len = strlen(user) + 1 + sizeof(msg_format);

	char *msg = (char *)malloc(len);
	if (msg == NULL) {
 		... // 错误处理
	}

	ret = snprintf(msg, msg_format, user);
	if (ret == -1) {
 		... // 错误处理
	} else {
		fprintf(stderr, msg); // msg中有来自未验证的外部数据，存在格式化漏洞
	}

	free(msg);
}
```

示例代码中首先计算了消息的长度，然后分配内存，接着利用snprintf()函数拼接了消息内容。因此消息内容中包含了msg_format的内容和用户的内容。 
当入参user中含有用户输入的格式符（如%s,%p,%n等后，fprintf()在执行时，会将msg作为一个格式化字符串来进行解析，而不是直接输出消息内容， 
也就是说此时msg中的内容不会被直接打印到stderr中，反而会将一些未知的数据打印到stderr，引发程序未定义的行为。这是一个非常严重的格式化漏洞。

**【正确代码示例】** 
下面是第一种推荐做法，代码中使用fputs()来代替fprintf()函数，fputs()会直接将msg的内容输出到stderr中，而不会去解析它。

```
// 入参user的长度被限制为256个字节或者更少
void incorrect_password(const char *user)
{
	int ret = -1;
	static const char msg_format[] = "%s cannot be authenticated.\n";

	// 这里加法运算不会整数溢出，因为user有限制
	size_t len = strlen(user) + 1 + sizeof(msg_format);
	char *msg = (char *)malloc(len);
	if (msg == NULL) {
 		... // 错误处理
	}

	ret = snprintf(msg, msg_format, user);
	if (ret == -1) {
 		... // 错误处理
	} else {
		fputs(stderr, msg); // 使用fputs函数代替fprintf函数
	}

	free(msg);
}
```

**【正确代码示例】** 
下面是第二种推荐做法，代码中将不受信任的用户输入user作为fprintf()的可选参数之一，用"%s"将user以字符串的形式固定下来，然后输出到stderr中，而不作为格式字符串的一部分，这样就消除了格式字符串漏洞出现的可能性。

```
void incorrect_password(const char  *user)
{
	static const char msg_format[] = "%s cannot be authenticated.\n";
	fprintf(stderr, msg_format, user);
}
```

**【错误代码示例】** 
如下代码示例中，使用了POSIX函数syslog()，但是syslog()函数也可能出现格式字符串漏洞。

```
void foo(void)
{
	char  *msg = get_msg();
	...

	syslog(LOG_INFO, msg); // 存在格式化漏洞
}
```

**【正确代码示例】** 
下面是推荐做法，代码中将不受信任的用户输入msg作为syslog()的可选参数之一，用"%s"将msg以字符串的形式固定下来，然后输出到系统日志中，而不作为格式字符串的一部分，这样就消除了格式字符串漏洞出现的可能性。

```
void foo(void)
{
	static const char msg_format[] =  "%s cannot be authenticated.\n";
	char  *msg = get_msg();
	...
	syslog(LOG_INFO, msg_format, msg); // 这里没有格式化漏洞
}
```


