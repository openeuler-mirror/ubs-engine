
## 外部数据作为数组索引时必须确保在数组大小范围内

**【描述】** 
外部数据作为数组索引对内存进行访问时，必须对数据的大小进行严格的校验，确保数组索引在有效范围内，否则会导致严重的错误。 
当一个指针指向数组元素时，可以指向数组最后一个元素的下一个元素的位置，但是不能读写该位置的内存。

**【错误代码示例】** 
如下代码示例中, set_dev_id()函数存在差一错误，当 index 等于 DEV_NUM 时，恰好越界写一个元素； 
同样get_dev()函数也存在差一错误，虽然函数执行过程中没有问题，但是当解引用这个函数返回的指针时，行为是未定义的。

```
#define DEV_NUM 10
#define MAX_NAME_LEN 128
typedef struct {
	int id;
	char name[MAX_NAME_LEN];
} Dev;

static Dev devs[DEV_NUM];
int set_dev_id(size_t index, int id)
{
	if (index > DEV_NUM) { // 错误：差一错误。
 		... // 错误处理
	}

	devs[index].id = id;
	return 0;
}

static Dev *get_dev(size_t index)
{
	if (index > DEV_NUM) { // 错误：差一错误。
 		... // 错误处理
	}

	return devs + index;
}
```



**【正确代码示例】** 
如下代码示例中，修改校验索引的条件，避免差一错误。

```
#define DEV_NUM 10
#define MAX_NAME_LEN 128
typedef struct {
	int id;
	char name[MAX_NAME_LEN];
} Dev;

static Dev devs[DEV_NUM];

int set_dev_Id (size_t index, int id)
{
	if (index >= DEV_NUM) {
		... // 错误处理
	}

	devs[index].id = id;
	return 0;
}

static Dev *get_dev(size_t index)
{
	if (index >= DEV_NUM) {
 		... // 错误处理
	}

	return devs + index;
}
```

**【相关软件CWE编号】** CWE-119，CWE-123，CWE-125

## 禁止通过对指针变量进行sizeof操作来获取数组大小

**【描述】** 
将指针当做数组进行sizeof操作时，会导致实际的执行结果与预期不符。例如：变量定义 char *p = array，其中array的定义为char array[LEN]，表达式sizeof(p) 得到的结果与 sizeof(char *)相同，并非array的长度。

**【错误代码示例】** 
如下代码示例中，buffer和path分别是指针和数组，程序员想对这2个内存进行清0操作，但由于程序员的疏忽，将内存大小误写成了sizeof(buffer)，与预期不符。

```
char path[MAX_PATH];
char *buffer = (char *)malloc(SIZE);
...
(void)memset(path, 0, sizeof(path));
// sizeof与预期不符，其结果为指针本身的大小而不是缓冲区大小
(void)memset(buffer, 0, sizeof(buffer));
```

**【正确代码示例】** 
如下代码示例中，将sizeof(buffer)修改为申请的缓冲区大小：

```
char path[MAX_PATH];
char *buffer = (char *)malloc(SIZE);
...
(void)memset(path, 0, sizeof(path));
(void)memset(buffer, 0, SIZE); // 使用申请的缓冲区大小
```

