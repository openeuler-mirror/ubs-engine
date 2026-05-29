
## 内存分配后必须判断是否成功

**【描述】** 
内存分配一旦失败，那么后续的操作会存在未定义的行为风险。比如malloc申请失败返回了空指针，对空指针的解引用是一种未定义行为。

**【错误代码示例】** 
如下代码示例中，调用malloc分配内存之后，没有判断是否成功，直接引用了p。如果malloc失败，它将返回一个空指针并传递给p。当如下代码在内存拷贝中解引用了该空指针p时，程序会出现未定义行为。

```
struct tm *make_tm(int year, int mon, int day, int hour, int min, int sec)
{
	struct tm *tmb = (struct tm*)malloc(sizeof(*tmb));
	tmb->year = year;
	...

	return tmb;
}
```

**【正确代码示例】** 
如下代码示例中，在malloc分配内存之后，立即判断其是否成功，消除了上述的风险。

```
struct tm  *make_tm(int year, int mon, int day, int hour, int min, int sec)
{
	struct tm  *tmb = (struct tm *)malloc(sizeof(*tmb));
	if (tmb == NULL) {
		... // 错误处理
	}

	tmb->year = year;
	...
	return tmb;
}
```

## 外部输入作为内存操作相关函数的复制长度时，需要校验其合法性

**【描述】** 
将数据复制到容量不足以容纳该数据的内存中会导致缓冲区溢出。为了防止此类错误，必须根据目标容量的大小限制被复制的数据大小，或者必须确保目标容量足够大以容纳要复制的数据。

**【错误代码示例】** 
外部输入的数据不一定会直接作为内存复制长度使用，还可能会间接参与内存复制操作。 
如下代码示例中，inputTable->count来自外部报文，虽然没有直接作为内存复制长度使用，而是作为for循环体的上限使用，间接参与了内存复制操作。由于没有校验其大小，可造成缓冲区溢出：

```
typedef struct {
	size_t count;
	int val[MAX_num_bERS];
} ValueTable;

ValueTable *value_table_dup(const ValueTable *input_table)
{
	ValueTable *output_table = ... // 分配内存
	...
	for (size_t i = 0; i  < input_table->count; i++) {
		output_table->val[i] = input_table->val[i];
	}
 	...
}
```

**【正确代码示例】** 
如下代码示例中，对input_table->count做了校验。

```
typedef struct {
size_t count;
int val[MAX_num_bERS];
}ValueTable;

ValueTable *value_table_dup(const ValueTable *input_table)
{
	ValueTable *output_table = ... // 分配内存
	...

	/ *
	- 根据应用场景，对来自外部报文的循环长度input_table->count
	- 与output_table->val数组大小做校验，避免造成缓冲区溢出
	*/
	if (input_table->count  > sizeof(output_table->val) / sizeof(output_table->val[0]){
		return NULL;
	}

	for (size_t i = 0; i  < input_table->count; i++) {
		output_table->val[i] = input_table->val[i];
	}

	...
}
```

## 内存中的敏感信息使用完毕后立即清0

**【描述】** 
内存中的口令、密钥等敏感信息使用完毕后立即清0，避免被攻击者获取或者无意间泄漏给低权限用户。这里所说的内存包括但不限于：

-   动态分配的内存

-   静态分配的内存

-   自动分配（堆栈）内存

-   内存缓存

-   磁盘缓存

**【错误代码示例】** 
通常内存在释放前不需要清除内存数据，因为这样在运行时会增加额外开销，所以在这段内存被释放之后，之前的数据还是会保留在其中。如果这段内存中的数据包含敏感信息，则可能会意外泄漏敏感信息。为了防止敏感信息泄漏，必须先清除内存中的敏感信息，然后再释放。 
在如下代码示例中，存储在所引用的动态内存中的敏感信息secret被复制到新动态分配的缓冲区newSecret，最终通过free()释放。因为释放前未清除这块内存数据，这块内存可能被重新分配到程序的另一部分，之前存储在newSecret中的敏感信息可能会无意中被泄露。

```
char *secret = NULL;

/ *
- 假设 secret 指向敏感信息，敏感信息的内容是长度小于SIZE_MAX个字符，
- 并且以null终止的字节字符串
*/
size_t size = strlen(secret);
char *new_secret = NULL;
new_secret = (char *)malloc(size + 1);
if (new_secret == NULL) {
	... // 错误处理
} else {
	strcpy(new_secret, secret);
	... // 处理 new_secret ...
	free(new_secret);
	new_secret = NULL;
}

...
```

**【正确代码示例】** 
如下代码示例中，为了防止信息泄漏，应先清除包含敏感信息的动态内存（用 '  0 '字符填充空间），然后再释放它。

```
char *secret = NULL;
/ *
- 假设 secret 指向敏感信息，敏感信息的内容是长度小于SIZE_MAX个字符，
- 并且以null终止的字节字符串
*/
size_t size = strlen(secret);
char *new_secret = NULL;
new_secret = (char *)malloc(size + 1);
if (new_secret == NULL) {
	... // 错误处理
} else {
	strcpy(new_secret, secret);
	... // 处理 new_secret ...
	(void)memset(new_secret, 0, size + 1);
	free(new_secret);
	new_secret = NULL;
}
...
```

**【正确代码示例】** 
下面是另外一个涉及敏感信息清理的场景，在代码获取到密码后，将密码保存到password中，进行密码验证，使用完毕后，通过memset对password清0。

```
int foo(void)
{
	char password [MAX_PWD_LEN ] = {0};
	if (!get_password(password, sizeof(password))) {
		...  // 处理错误 
	}

	if (!verify_password(password)) {
		... // 处理错误
	}

	...
	(void)memset(password, 0, sizeof(password));
	...
}
```


