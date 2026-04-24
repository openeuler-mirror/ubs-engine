
## 调用格式化输入/输出函数时，使用有效的格式字符串

**【描述】**

格式化输入/输出函数（如fscanf()/fprintf()及相关函数）在format字符串控制下进行转换、格式化、打印其实参。

在创建格式化字符串时的常见错误包括：

-   format中参数个数与实参个数不一致；

-   使用无效的转换指示符；

-   使用与转换指示符不兼容的标志字符；

-   使用与转换指示符不兼容的长度修饰符；

-   format中转换指示符与实参类型不匹配；

-   使用int以外类型的实参指定宽度或者精度；

不要为格式化输入/输出函数提供未知的或者无效的转换规格，以及标志字符、精度、长度修饰符、转换指示符的无效组合。同样，不要提供与格式化字符串中的转换指示符类型不匹配的实参。这可能会使程序产生未定义行为。

**【错误代码示例】**

如下代码示例中，printf()的实参infoLevel类型与对应的转换指示符 's '不匹配，正确的转换指示符要使用 'd '。同样，实参infoMsg类型与对应的转换指示符 'd '不匹配，正确的转换指示符要使用 's '。 
这些用法会使程序产生未定义行为，比如：printf()将把infoLevel实参解释为指针，试图从infoLevel包含的地址中读取一个字符串，从而发生非法访问。

```
void foo(void)
{
	const char *info_msg = "Information seed to user.";
	int info_level = 3;
	...
	printf("infoLevel: %s, infoMsg: %d\n", info_level, info_msg);
	...
}
```

**【正确代码示例】**

正确的做法是确保printf()函数的实参匹配format的转换指示符。

```
void foo(void)
{
	const char *info_msg = "Information seed to user.";
	int info_level = 3;
	...

	printf("infoLevel: %d, infoMsg: %s\n", info_level, info_msg);
	...
}
```

**【影响】**

错误的格式串可能造成内存破坏或者程序异常终止。

## 禁止使用alloca()函数申请栈上内存

**【描述】** 
POSIX和C99均未定义alloca()的行为，在有些平台下不支持该函数，使用alloca会降低程序的兼容性和可移植性，该函数在栈帧里申请内存，申请的大小很可能超过栈的边界，影响后续的代码执行。

请使用malloc从堆中动态分配内存。

【影响】

程序栈的大小非常有限，如果分配导致栈溢出，则程序产生未定义行

## 禁止使用realloc()函数

**【描述】** 
realloc()是一个非常特殊的函数，原型如下：

void *realloc(void *ptr, size_t size);

随着参数的不同，其行为也是不同：

-   当ptr不为NULL，且size不为0时，该函数会重新调整内存大小，并将新的内存指针返回，并保证最小的size的内容不变；

-   参数ptr为NULL，但size不为0，那么其行为等同于malloc(size)；

-   参数size为0，则realloc的行为等同于free(ptr)。

由此可见，一个简单的C函数，却被赋予了3种行为，这不是一个设计良好的函数。虽然在编程中提供了一些便利性，如果认识不足，使用不当，是却极易引发各种bug。

**【错误代码示例】** 
如下代码示例中，使用realloc不当导致内存泄漏。 
代码中希望对ptr的空间进行扩充，当realloc()分配失败的时候，会返回NULL。但是参数中的ptr的内存是没有被释放的，如果直接将realloc()的返回值赋给ptr，那么ptr原来指向的内存就会丢失，造成内存泄漏。

```
// 当realloc()分配内存失败时会返回NULL，导致内存泄漏
char *ptr = (char  *)realloc(ptr, NEW_SIZE);
if (ptr == NULL) {
	...// 错误处理
}
```

**【正确代码示例】** 
使用malloc()函数代替realloc()函数。

```
// 使用malloc()函数代替realloc()函数
char *new_ptr = (char *)malloc(NEW_SIZE);
if (new_ptr == NULL) {
	... // 错误处理
}

(void)memcpy(new_ptr, old_ptr, old_size);
... // 返回前，释放old_Ptr
```

**【影响】**

使用不当容易造成内存泄漏和双重释放问题。不正确的内存对齐可能导致对象访问异常。

## 禁止外部可控数据作为进程启动函数的参数

**【描述】** 
本条款中进程启动函数包括system、popen、execl、execlp、execle、execv、execvp等。

system()、popen()等函数会创建一个新的进程，如果外部可控数据作为这些函数的参数，会导致注入漏洞。

使用execl()、execlp()等函数执行新进程时，如果使用shell启动的新进程，则同样存在命令注入风险。

因此，总是优先考虑使用C标准函数实现需要的功能。如果确实需要使用这些函数，请使用白名单机制确保这些函数的参数不受任何外来数据的影响。

**【错误代码示例】** 
如下代码示例中，使用 system() 函数执行 cmd 命令串来自外部，攻击者可以执行任意命令：

```
char  *cmd = get_cmd_from_remote();
if (cmd == NULL) {
	... // 处理错误
}

system(cmd);
```

如下代码示例中，使用 system() 函数执行 cmd 命令串的一部分来自外部，攻击者可能输入  'some dir;useradd xxx '字符串，创建一个xxx的用户：

```
char cmd[MAX_LEN ];
int ret;

char  *name = get_dir_name_from_remote();
if (name == NULL) {
	... // 处理错误
}

ret = sprintf(cmd, "ls %s", name);
...
system(cmd);
```

使用exec系列函数来避免命令注入时，注意exec系列函数中的path、file参数禁止使用命令解析器(如/bin/sh)。

```
int execl(const char *path, const char *arg,  ...);
int execlp(const char *file, const char *arg,  ...);
int execle(const char *path, const char *arg,  ..., char * const envp[]);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
```

例如，禁止如下使用方式：

```
char *cmd = get_dir_name_from_remote();
execl("/bin/sh", "sh",  "-c", cmd, NULL);
```

**【正确代码示例】** (使用库函数)

在Linux下实现对当前目录下文件名的打印，可以使用opendir(), readdir(), stat()等函数直接实现ls-l命令的功能，不必使用system()函数。下面是一个简化的ls -l示例版本，列出一个由程序内部指定的文件的信息，该函数仅考虑了不需要重入的情况。

```
static int OutputFileInfo(const char *file_name)
{
	const char priv[] = {'x', 'w', 'r'};
	ASSERT(file_name != NULL);

	struct stat st;
	int ret = stat(file_name, &st);
	if (ret == -1) {
		return -1;
	}

	const struct passwd *pw = getpwuid(st.st_uid);
	if (pw == NULL) {
		return -1;
	}

	const struct group *gp = getgrgid(st.st_gid);
	if (gp == NULL) {
		return -1;
	}

	if (S_ISREG(st.st_mode)) {
		printf("-");
	} else if (S_ISDIR(st.st_mode)) {
		printf("d");
	}

	for (int i = 8; i >= 0; i --) {
		if ((st.st_mode & (1 < < i)) != 0) {
			printf("%c", priv[i % 3]);
		} else {
			printf("-");
		}
	}

	printf("%s %s %ld %s\n",
	pw->pw_name,
	gp->gr_name,
	st.st_size,
	file_name);
	return 0;
}
```

**【正确代码示例】** (使用exec系列函数)

可以通过库函数简单实现的功能（如上例），需要避免调用命令处理器来执行外部命令。如果确实需要调用单个命令，应使用exec *函数来实现参数化调用，并对调用的命令实施白名单管理。

```
pid_t pid;
char * const envp[] = { NULL };
...
char *file_name = get_dir_name_from_remote();
if (file_name == NULL) {
	... // 处理错误
}
...
if ((pid = fork()) < 0) {
	...
} else if (pid == 0) {
	// 使用some_tool对指定文件进行加工
	execle( "/bin/some_tool", "some_tool", file_name, NULL, envp);
	_Exit(-1);
}

...
int status;
waitpid(pid, &status, 0);
FILE *fp = fopen(file_name, "r");
...
```

此时，外部输入的file_name仅作为some_tool命令的参数，没有命令注入的风险。

**【正确代码示例】** (使用白名单)

对输入的文件名基于合理的白名单检查，避免命令注入。

```
char *cmd = get_cmd_from_remote();
if (cmd == NULL) {
	... // 处理错误
}

// 使用白名单检查命令是否合法，仅允许 "some_tool_a", "some_tool_b"命令，外部无法随意控制
if (!is_valid_cmd(cmd)) {
	... // 处理错误
}

system(cmd);
...
```

**【相关软件CWE编号】** CWE-676，CWE-88

## 禁止在信号处理例程中调用非异步安全函数

**【描述】** 
在信号处理程序中只调用异步安全函数。

除了C语言标准函数以外，其他系统函数也提供了一些的异步安全函数，在信号处理程序中使用这些函数之前，应确保调用的函数在所有可能的执行环境下均是异步安全的。

**【错误代码示例】** 
如下代码示例中，信号处理函数中调用了非异步安全函数printf()：

```
void handler(int num)
{
	printf("receive signal = %d \n", SIGINT);
}

int main(int argc, char **argv)
{
	if (signal(SIGINT, handler) == SIG_ERR) {
 		... // 错误处理
	}

	while (true) {
 		... // 程序主循环代码
	}

	return 0;
}
```

**【正确代码示例】** 
如下代码示例中，尽量不在信号处理函数中调用其他函数，仅在信号处理程序中修改volatile sig_atomic_t类型的变量：

```
static volatile sig_atomic_t g_flag = 0;
void handler(int num)
{
	g_flag = 1;
}

int main(int argc, char **argv)
{
	if (signal(SIGINT, handler) == SIG_ERR) {
	... // 错误处理
	}

	while (true) {
		if (g_flag != 0) {
			printf("receive signal = %d\n", SIGINT);
		}

		... // 程序主循环代码
	}

	...
	return 0;
}
```

**【相关软件CWE编号】** CWE-479

