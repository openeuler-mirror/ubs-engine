
## 内核mmap接口实现中，确保对映射起始地址和大小进行合法性校验

**【描述】**

**说明：**Linux内核 mmap接口中，经常使用remap_pfn_range()函数将设备物理内存映射到用户进程空间。如果映射起始地址等参数由用户态控制并缺少合法性校验，将导致用户态可通过映射读写任意内核地址。如果攻击者精心构造传入参数，甚至可在内核中执行任意代码。

**【错误代码示例】**

如下代码在使用remap_pfn_range()进行内存映射时，未对用户可控的映射起始地址和空间大小进行合法性校验，可导致内核崩溃或任意代码执行。

```
static int incorrect_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size;
	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	//错误：未对映射起始地址、空间大小做合法性校验
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) { 
		err_log("%s, remap_pfn_range fail", __func__);
		return EFAULT;
	} else {
		vma->vm_flags &=  ~VM_IO;
	}

	return EOK;
}
```

**【正确代码示例】**

增加对映射起始地址等参数的合法性校验。

```
static int correct_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size;
	size = vma->vm_end - vma->vm_start;
	//修改：添加校验函数，验证映射起始地址、空间大小是否合法
	if (!valid_mmap_phys_addr_range(vma->vm_pgoff, size)) { 
		return EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot)) {
		err_log( "%s, remap_pfn_range fail ", __func__);
		return EFAULT;
	} else {
		vma->vm_flags &=  ~VM_IO;
	}

	return EOK;
}
```

## 内核程序中必须使用内核专用函数读写用户态缓冲区

**【描述】**

用户态与内核态之间进行数据交换时，如果在内核中不加任何校验（如校验地址范围、空指针）而直接引用用户态传入指针，当用户态传入非法指针时，可导致内核崩溃、任意地址读写等问题。因此，应当禁止使用memcpy()、sprintf()等危险函数，而是使用内核提供的专用函数：copy_from_user()、copy_to_user()、put_user()和get_user()来读写用户态缓冲区，这些函数内部添加了入参校验功能。

所有禁用函数列表为：memcpy()、bcopy()、memmove()、strcpy()、strncpy()、strcat()、strncat()、sprintf()、vsprintf()、snprintf()、vsnprintf()、sscanf()、vsscanf()。

**【错误代码示例】**

内核态直接使用用户态传入的buf指针作为snprintf()的参数，当buf为NULL时，可导致内核崩溃。

```
ssize_t incorrect_show(struct file *file, char__user *buf, size_t size, loff_t *data)
{
	// 错误：直接引用用户态传入指针，如果buf为NULL，则空指针异常导致内核崩溃
	return snprintf(buf, size, "%ld\n", debug_level); 
}
```

**【正确代码示例】**

使用copy_to_user()函数代替snprintf()。

```
ssize_t correct_show(struct file *file, char __user *buf, size_t size, loff_t *data)
{
	int ret = 0;
	char level_str[MAX_STR_LEN] = {0};
	snprintf(level_str, MAX_STR_LEN, "%ld \n", debug_level);
	if(strlen(level_str) >= size) {
		return EFAULT;
	}
	
	// 修改：使用专用函数copy_to_user()将数据写入到用户态buf，并注意防止缓冲区溢出
	ret = copy_to_user(buf, level_str, strlen(level_str)+1); 
	return ret;
}
```

**【错误代码示例】**

内核态直接使用用户态传入的指针user_buf作为数据源进行memcpy()操作，当user_buf为NULL时，可导致内核崩溃。

```
size_t incorrect_write(struct file  *file, const char __user  *user_buf, size_t count, loff_t  *ppos)
{
	...
	char buf [128] = {0};
	int buf_size = 0;
	buf_size = min(count, (sizeof(buf)-1));
	// 错误：直接引用用户态传入指针，如果user_buf为NULL，则可导致内核崩溃
	(void)memcpy(buf, user_buf, buf_size); 
	...
}
```

**【正确代码示例】**

使用copy_from_user()函数代替memcpy()。

```
ssize_t correct_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	...
	char buf[128] = {0};
	int buf_size = 0;

	buf_size = min(count, (sizeof(buf)-1));
	// 修改：使用专用函数copy_from_user()将数据写入到内核态buf，并注意防止缓冲区溢出
	if (copy_from_user(buf, user_buf, buf_size)) { 
		return EFAULT;
	}

	...
}
```

## 必须对copy_from_user()拷贝长度进行校验，防止缓冲区溢出

**说明：**内核态从用户态拷贝数据时通常使用copy_from_user()函数，如果未对拷贝长度做校验或者校验不当，会造成内核缓冲区溢出，导致内核panic或提权。

**【错误代码示例】**

未校验拷贝长度。

```
static long gser_ioctl(struct file  *fp, unsigned cmd, unsigned long arg)
{
	char smd_write_buf[GSERIAL_BUF_LEN];
	switch (cmd)
	{
		case GSERIAL_SMD_WRITE:
			if (copy_from_user(&smd_write_arg, argp, sizeof(smd_write_arg))) {...}
			// 错误：拷贝长度参数smd_write_arg.size由用户输入，未校验
			copy_from_user(smd_write_buf, smd_write_arg.buf, smd_write_arg.size); 
			...
	}
}
```

**【正确代码示例】**

添加长度校验。

```
static long gser_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	char smd_write_buf[GSERIAL_BUF_LEN];
	switch (cmd)
	{
		case GSERIAL_SMD_WRITE:
			if (copy_from_user(&smd_write_arg, argp, sizeof(smd_write_arg))){...}
			// 修改：添加校验
			if (smd_write_arg.size  >= GSERIAL_BUF_LEN) {......} 
			copy_from_user(smd_write_buf, smd_write_arg.buf, smd_write_arg.size);
 			...
	}
}
```

## 必须对copy_to_user()拷贝的数据进行初始化，防止信息泄漏

**【描述】**

**说明：**内核态使用copy_to_user()向用户态拷贝数据时，当数据未完全初始化（如结构体成员未赋值、字节对齐引起的内存空洞等），会导致栈上指针等敏感信息泄漏。攻击者可利用绕过kaslr等安全机制。

**【错误代码示例】**

未完全初始化数据结构成员。

```
static long rmnet_ctrl_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	struct ep_info info;
	switch (cmd) {
		case FRMNET_CTRL_EP_LOOKUP:
			info.ph_ep_info.ep_type = DATA_EP_TYPE_HSUSB;
			info.ipa_ep_pair.cons_pipe_num = port->ipa_cons_idx;
			info.ipa_ep_pair.prod_pipe_num = port->ipa_prod_idx;
			// 错误: info结构体有4个成员，未全部赋值
			ret = copy_to_user((void __user *)arg, &info, sizeof(info)); 
			...
	}
}
```

**【正确代码示例】**

全部进行初始化。

```
static long rmnet_ctrl_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	struct ep_info info;
	// 修改：使用memset初始化缓冲区，保证不存在因字节对齐或未赋值导致的内存空洞
	(void)memset(&info, '0', sizeof(ep_info)); 
	switch (cmd) {
		case FRMNET_CTRL_EP_LOOKUP:
			info.ph_ep_info.ep_type = DATA_EP_TYPE_HSUSB;
			info.ipa_ep_pair.cons_pipe_num = port->ipa_cons_idx;
			info.ipa_ep_pair.prod_pipe_num = port->ipa_prod_idx;
			ret = copy_to_user((void __user *)arg, &info, sizeof(info));
			...
	}
}
```

## 禁止在异常处理中使用BUG_ON宏，避免造成内核panic

**【描述】**

BUG_ON宏会调用内核的panic()函数，打印错误信息并主动崩溃系统，在正常逻辑处理中（如ioctl接口的cmd参数不识别）不应当使系统崩溃，禁止在此类异常处理场景中使用BUG_ON宏，推荐使用WARN_ON宏。

**【错误代码示例】**

正常流程中使用了BUG_ON宏

```
/ * 判断Q6侧设置定时器是否繁忙，1-忙，0-不忙 */
static unsigned int is_modem_set_timer_busy(special_timer *smem_ptr)
{
	int i = 0;
	if (smem_ptr == NULL) {
		printk(KERN_EMERG"%s:smem_ptr NULL!\n", __FUNCTION__);
		// 错误：系统BUG_ON宏打印调用栈后调用panic()，导致内核拒绝服务，不应在正常流程中使用
		BUG_ON(1); 
		return 1;
	}

	...
}
```

**【正确代码示例】**

去掉BUG_ON宏。

```
/ * 判断Q6侧设置定时器是否繁忙，1-忙，0-不忙  */
static unsigned int is_modem_set_timer_busy(special_timer *smem_ptr)
{
	int i = 0;
	if (smem_ptr == NULL) {
		printk(KERN_EMERG"%s:smem_ptr NULL!\n",  __FUNCTION__);
		// 修改：去掉BUG_ON调用，或使用WARN_ON
		return 1;
	}

	...
}
```

## 在中断处理程序或持有自旋锁的进程上下文代码中，禁止使用会引起进程休眠的函数

**【描述】**

Linux以进程为调度单位，在Linux中断上下文中，只有更高优先级的中断才能将其打断，系统在中断处理的时候不能进行进程调度。如果中断处理程序处于休眠状态，就会导致内核无法唤醒，从而使得内核处于瘫痪。

自旋锁在使用时，抢占是失效的。若自旋锁在锁住以后进入睡眠，由于不能进行处理器抢占，其它进程都将因为不能获得CPU（单核CPU）而停止运行，对外表现为系统将不作任何响应，出现挂死。

因此，在中断处理程序或持有自旋锁的进程上下文代码中，应该禁止使用可能会引起休眠（如vmalloc()、msleep()等）、阻塞（如copy_from_user(),copy_to_user()等）或者耗费大量时间（如printk()等）的函数。

## 合理使用内核栈，防止内核栈溢出

**【描述】**

Linux的内核栈大小是固定的（一般32位系统为8K，64位系统为16K，因此资源非常宝贵。不合理的使用内核栈，可能会导致栈溢出，造成系统挂死。因此需要做到以下几点：

-   在栈上申请内存空间不要超过内核栈大小；

-   注意函数的嵌套使用次数；

-   不要定义过多的变量。

**【错误代码示例】**

以下代码中定义的变量过大，导致栈溢出。

```
...
struct result
{
	char name[4];
	unsigned int a;
	unsigned int b;
	unsigned int c;
	unsigned int d;
}; // 结构体result的大小为20字节

int foo()
{
	struct result temp[512];
	// 错误: temp数组含有512个元素，总大小为10K，远超内核栈大小
	(void)memset(temp, 0, sizeof(result) * 512); 
	... // use temp do something
	return 0;
}

...
```

代码中数组temp有512个元素，总共10K大小，远超内核的8K，明显的栈溢出。

**【正确代码示例】**

使用kmalloc()代替之。

```
...
struct result
{
	char name[4];
	unsigned int a;
	unsigned int b;
	unsigned int c;
	unsigned int d;
}; // 结构体result的大小为20字节

int foo()
{
	struct result  *temp = NULL;
	temp = (result *)kmalloc(sizeof(result) * 512, GFP_KERNEL); //修改：使用kmalloc()申请内存
	... // check temp is not NULL
	(void)memset(temp, 0, sizeof(result)  * 512);
	... // use temp do something
	... // free temp
	return 0;
}
...
```

## 临时关闭地址校验机制后，在操作完成后必须及时恢复

**【描述】**

SMEP安全机制是指禁止内核执行用户空间的代码（PXN是ARM版本的SMEP）。系统调用（如open()，write()等）本来是提供给用户空间程序访问的。默认情况下，这些函数会对传入的参数地址进行校验，如果入参是非用户空间地址则报错。因此，要在内核程序中使用这些系统调用，就必须使参数地址校验功能失效。set_fs()/get_fs()就用来解决该问题。详细说明见如下代码：

```
...
mmegment_t old_fs;
printk("Hello, I'm the module that intends to write message to file.\n");
if (file == NULL) {
	file = filp_open(MY_FILE, O_RDWR | O_APPEND | O_CREAT, 0664);
}

if (IS_ERR(file)) {
	printk("Error occured while opening file %s, exiting ...\n", MY_FILE);
	return 0;
}

sprintf(buf, "%s", "The Message.");
old_fs = get_fs(); // get_fs()的作用是获取用户空间地址上限值  
                   // #define get_fs() (current->addr_limit
set_fs(KERNEL_DS); // set_fs的作用是将地址空间上限扩大到KERNEL_DS，这样内核代码可以调用系统函数
file->f_op->write(file, (char *)buf, sizeof(buf), &file->f_pos); // 内核代码可以调用write()函数
set_fs(old_fs); // 使用完后及时恢复原来用户空间地址限制值
...
```

通过上述代码，可以了解到最为关键的就是操作完成后，要及时恢复地址校验功能。否则SMEP/PXN安全机制就会失效，使得许多漏洞的利用变得很容易。

**【错误代码示例】**

在程序错误处理分支，未通过set_fs()恢复地址校验功能。

```
...
oldfs = get_fs();
set_fs(KERNEL_DS);
/* 在时间戳目录下面创建done文件 */
fd = sys_open(path, O_CREAT | O_WRONLY, FILE_LIMIT);
if (fd < 0) {
	BB_PRINT_ERR("sys_mkdir[%s] error, fd is[%d]\n", path, fd);
	return; // 错误：在错误处理程序分支未恢复地址校验机制
}

sys_close(fd);
set_fs(oldfs);
...
```

**【正确代码示例】**

在错误处理程序中恢复地址校验功能。

```
...
oldfs = get_fs();
set_fs(KERNEL_DS);

/* 在时间戳目录下面创建done文件 */
fd = sys_open(path, O_CREAT | O_WRONLY, FILE_LIMIT);
if (fd < 0) {
	BB_PRINT_ERR("sys_mkdir[%s] error, fd is[%d] \n", path, fd);
	set_fs(oldfs); // 修改：在错误处理程序分支中恢复地址校验机制
	return;
}

sys_close(fd);
set_fs(oldfs);
...
```

