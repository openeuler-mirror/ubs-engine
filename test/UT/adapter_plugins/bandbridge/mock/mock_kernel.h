#ifndef MOCK_KERNEL_H
#define MOCK_KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#ifdef __cplusplus
struct bb_class {
    int dummy;
};
#else
typedef uint32_t dev_t;
struct class {
    int dummy;
};
#endif

#define __iomem
#define __user
#define __init
#define __exit
#define __packed __attribute__((packed))
#ifndef __cplusplus
#define __attribute__(x)
#endif

#define CAP_NET_ADMIN 12
#define MISC_DYNAMIC_MINOR 255
#define PATH_MAX 4096
#define HZ 1000
#define THIS_MODULE ((struct module*)0)

#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))
#define unlikely(x) (x)
#define IS_ERR_OR_NULL(ptr) (!ptr)

#ifndef __cplusplus
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define le16_to_cpu(val) (val)
#define le32_to_cpu(val) (val)
#define cpu_to_le16(val) (val)
#define cpu_to_le32(val) (val)

#undef _IO
#undef _IOR
#undef _IOW
#undef _IOWR
#define _IO(type, nr) ((0U << 30) | ((unsigned)(type) << 8) | (unsigned)(nr))
#define _IOR(type, nr, datatype) \
    ((2U << 30) | ((unsigned)sizeof(datatype) << 16) | ((unsigned)(type) << 8) | (unsigned)(nr))
#define _IOW(type, nr, datatype) \
    ((1U << 30) | ((unsigned)sizeof(datatype) << 16) | ((unsigned)(type) << 8) | (unsigned)(nr))
#define _IOWR(type, nr, datatype) \
    ((3U << 30) | ((unsigned)sizeof(datatype) << 16) | ((unsigned)(type) << 8) | (unsigned)(nr))

#define module_init(x)
#define module_exit(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_AUTHOR(x)
#define MODULE_LICENSE(x)

#define GFP_KERNEL 0
#define GFP_ATOMIC 1

#define pr_info(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) \
    do {                   \
    } while (0)
#define KERN_INFO ""
#define KERN_ERR ""
#define KERN_WARNING ""

#define DEFINE_RATELIMIT_STATE(name, interval, burst) int name __attribute__((unused))
#define __ratelimit(rs) 1

struct task_struct {
    char comm[16];
    struct task_struct* group_leader;
    struct mm_struct* mm;
    struct nsproxy* nsproxy;
};

struct mm_struct {
    struct file* exe_file;
};

struct nsproxy {
    struct pid_namespace* pid_ns_for_children;
};

struct pid_namespace {
    int level;
};

extern struct pid_namespace init_pid_ns;

struct dentry {
    int dummy;
};
struct vfsmount {
    int dummy;
};

struct path {
    struct dentry* dentry;
    struct vfsmount* mnt;
};

struct file {
    void* private_data;
    struct path f_path;
};

struct inode {
    int dummy;
};

struct cdev {
    struct module* owner;
};

struct mutex {
    pthread_mutex_t mtx;
};

struct miscdevice {
    int minor;
    const char* name;
    const struct file_operations* fops;
    struct device* this_device;
};

struct poll_table_struct {
    int dummy;
};

struct file_operations {
    struct module* owner;
    int (*open)(struct inode*, struct file*);
    int (*release)(struct inode*, struct file*);
    long (*unlocked_ioctl)(struct file*, unsigned int, unsigned long);
    unsigned int (*poll)(struct file*, struct poll_table_struct*);
};

struct device {
    int dummy;
};
struct module {
    int dummy;
};
struct wait_queue_head {
    int dummy;
};

extern struct task_struct mock_current_task;
#define current (&mock_current_task)

extern bool mock_capable_net_admin;
extern char mock_exe_path[PATH_MAX];
extern bool mock_kmalloc_fail;
extern bool mock_vmalloc_fail;
extern bool mock_ioremap_fail;
extern bool mock_misc_register_fail;

extern u32 mock_reg_space[];
extern bool mock_msleep_instant;

bool capable(int cap);
void* kmalloc(size_t size, int flags);
void kfree(const void* ptr);
void* vmalloc(size_t size);
void vfree(const void* ptr);
void* ioremap(uint64_t phys_addr, size_t size);
void iounmap(void* addr);
uint32_t readl(const volatile void* addr);
void writel(uint32_t value, volatile void* addr);
void memcpy_toio(volatile void* dst, const void* src, size_t count);
void memcpy_fromio(void* dst, const volatile void* src, size_t count);
unsigned long copy_from_user(void* to, const void __user* from, unsigned long n);
unsigned long copy_to_user(void __user* to, const void* from, unsigned long n);
int misc_register(struct miscdevice* misc);
void misc_deregister(struct miscdevice* misc);
char* d_path(const struct path* path, char* buf, int buflen);
void mutex_init(struct mutex* m);
void mutex_lock(struct mutex* m);
void mutex_unlock(struct mutex* m);
void msleep(unsigned int msecs);
int strscpy(char* dest, const char* src, size_t count);
void init_waitqueue_head(struct wait_queue_head* q);

void mock_reset_all(void);
void mock_set_capable_net_admin(bool val);
void mock_set_process_name(const char* name);
void mock_set_exe_path(const char* path);
void mock_set_pid_ns_is_init(bool is_init);
void mock_set_kmalloc_fail(bool fail);
void mock_set_vmalloc_fail(bool fail);
void mock_set_ioremap_fail(bool fail);
void mock_set_misc_register_fail(bool fail);
void mock_reset_reg_space(void);
void mock_set_reg(u32 offset, u32 value);
u32 mock_get_reg(u32 offset);
void mock_set_exe_file_null(void);

#ifdef __cplusplus
}
#endif

#endif
