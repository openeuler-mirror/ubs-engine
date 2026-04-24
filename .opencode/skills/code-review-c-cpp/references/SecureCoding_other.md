
## 不要在信号处理函数中访问共享对象

**【描述】** 
在信号处理程序中访问和修改共享对象可能会造成竞争条件，使数据处于不确定的状态。 
这条规则有两个不适用的场景（C11标准第5.1.2.3章节第5段）是：

1） 读写不需要加锁的原子对象;

2）读写volatile sig_atomic_t类型的对象，因为具有volatile sig_atomic_t类型的对象即使在出现异步中断的时候也可以作为一个原子实体访问，是异步安全的。

此外，在信号处理程序中，如果要调用函数，请仅调用异步信号安全函数。

**【错误代码示例】** 
在这个信号处理过程中，程序打算将p_msg作为共享对象，当产生SIGINT信号时更新共享对象的内容，但是该p_msg变量类型不是volatile sig_atomic_t，所以不是异步安全的。

```
#define MAX_MSG_SIZE 32
static char g_msg_buf[MAX_MSG_SIZE] = {0};
static char *g_msg = g_msg_buf;
void signal_handler(int signum)
{
	// 下面代码操作g_msg不合规，因为不是异步安全的
	(void)memset(g_msg, 0, MAX_MSG_SIZE);
	strcpy(g_msg,  "signal SIGINT received.");
	... //
}

int main(void)
{
	strcpy(g_msg,  "No msg yet."); // 初始化消息内容
	signal(SIGINT, signal_handler); // 设置SIGINT信号对应的处理函数
	... // 程序主循环代码
	return 0;
}
```

**【正确代码示例】** 
如下代码示例中，在信号处理函数中仅将volatile sig_atomic_t类型作为共享对象使用。

```
#define MAX_MSG_SIZE 32
volatile sig_atomic_t g_sig_flag = 0;
void signal_handler(int signum)
{
	g_sig_flag = 1; // 合规
}

int main(void)
{
	signal(SIGINT, signal_handler);
	char msg_buf[MAX_MSG_SIZE];
	strcpy(msg_buf, "No msg yet."); // 初始化消息内容
	... // 程序主循环代码
	if (g_sig_flag == 1) { // 在退出主循环之后，根据sigFlag状态再刷新消息内容
		strcpy(msgBuf, "signal SIGINT received.");
	}

	return 0;
}
```

**【相关软件CWE编号】** CWE-662，CWE-828

## 禁用rand函数产生用于安全用途的伪随机数

**【描述】** 
C语言标准库rand()函数生成的是伪随机数，所以不能保证其产生的随机数序列质量。所以禁止使用rand()函数产生的随机数用于安全用途，必须使用安全的随机数产生方式，如： /dev/random文件。

典型的安全用途场景包括(但不限于)以下几种：

-   会话标识SessionID的生成；

-   挑战算法中的随机数生成；

-   验证码的随机数生成；

-   用于密码算法用途（例如用于生成IV、盐值、密钥等）的随机数生成。

**【错误代码示例】** 
程序员期望生成一个唯一的不可被猜测的HTTP会话ID，但该ID是通过调用rand()函数产生的数字随机数，它的ID是可猜测的，并且随机性有限。

**【正确代码示例】(POSIX)** 
可以使用/dev/random文件得到随机数。

**【影响】**

使用rand()函数可能造成可预测的随机数。


