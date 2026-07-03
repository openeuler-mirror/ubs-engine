#include "bandbridge_ctrlq.h"
#include "bandbridge_log.h"
#include "mock_kernel.h"

extern struct bandbridge_ctrlq_info g_ctrlq_info;
extern void* __iomem g_ctrlq_va;

#define MOCK_REG_SPACE_SIZE (CTRLQ_REG_LEN / sizeof(u32))

struct task_struct mock_current_task = {
    .comm = "ubse", .group_leader = &mock_current_task, .mm = NULL, .nsproxy = NULL};
bool mock_capable_net_admin = true;
char mock_exe_path[PATH_MAX] = "/usr/bin/ubse";
bool mock_kmalloc_fail = false;
bool mock_vmalloc_fail = false;
bool mock_ioremap_fail = false;
bool mock_misc_register_fail = false;
bool mock_msleep_instant = false;

u32 mock_reg_space[MOCK_REG_SPACE_SIZE];
uint32_t g_bandbridge_log_level = BANDBRIDGE_LOG_LEVEL_INFO;

struct pid_namespace init_pid_ns = {.level = 0};
static struct mm_struct mock_mm = {0};
static struct file mock_exe_file = {0};
static struct nsproxy mock_nsproxy = {.pid_ns_for_children = &init_pid_ns};

void mock_reset_all(void)
{
    mock_capable_net_admin = true;
    strncpy(mock_current_task.comm, "ubse", sizeof(mock_current_task.comm));
    mock_current_task.group_leader = &mock_current_task;
    mock_mm.exe_file = &mock_exe_file;
    mock_current_task.mm = &mock_mm;
    mock_current_task.nsproxy = &mock_nsproxy;
    strncpy(mock_exe_path, "/usr/bin/ubse", sizeof(mock_exe_path));
    mock_kmalloc_fail = false;
    mock_vmalloc_fail = false;
    mock_ioremap_fail = false;
    mock_misc_register_fail = false;
    mock_reset_reg_space();
    g_ctrlq_va = mock_reg_space;
    memset(&g_ctrlq_info, 0, sizeof(g_ctrlq_info));
    g_bandbridge_log_level = BANDBRIDGE_LOG_LEVEL_INFO;
}

void mock_set_capable_net_admin(bool val)
{
    mock_capable_net_admin = val;
}
void mock_set_process_name(const char* name)
{
    strncpy(mock_current_task.comm, name, sizeof(mock_current_task.comm) - 1);
}
void mock_set_exe_path(const char* path)
{
    strncpy(mock_exe_path, path, sizeof(mock_exe_path) - 1);
}
void mock_set_pid_ns_is_init(bool is_init)
{
    if (is_init) {
        mock_current_task.nsproxy = &mock_nsproxy;
    } else {
        mock_current_task.nsproxy = NULL;
    }
}
void mock_set_kmalloc_fail(bool fail)
{
    mock_kmalloc_fail = fail;
}
void mock_set_vmalloc_fail(bool fail)
{
    mock_vmalloc_fail = fail;
}
void mock_set_ioremap_fail(bool fail)
{
    mock_ioremap_fail = fail;
}
void mock_set_misc_register_fail(bool fail)
{
    mock_misc_register_fail = fail;
}

void mock_set_exe_file_null(void)
{
    mock_mm.exe_file = NULL;
}

void mock_reset_reg_space(void)
{
    memset(mock_reg_space, 0, sizeof(mock_reg_space));
}
void mock_set_reg(u32 offset, u32 value)
{
    mock_reg_space[offset / sizeof(u32)] = value;
}
u32 mock_get_reg(u32 offset)
{
    return mock_reg_space[offset / sizeof(u32)];
}

bool capable(int cap)
{
    (void)cap;
    return mock_capable_net_admin;
}

void* kmalloc(size_t size, int flags)
{
    (void)flags;
    if (mock_kmalloc_fail)
        return NULL;
    return malloc(size);
}

void kfree(const void* ptr)
{
    free((void*)ptr);
}

void* vmalloc(size_t size)
{
    if (mock_vmalloc_fail)
        return NULL;
    return malloc(size);
}

void vfree(const void* ptr)
{
    free((void*)ptr);
}

void* ioremap(uint64_t phys_addr, size_t size)
{
    if (mock_ioremap_fail)
        return NULL;
    u64 ctrlq_pa = UB_REG_BASEADDR + CTRLQ_PER_OFFSET * CTRLQ_FE15_INDEX;
    if (phys_addr == ctrlq_pa)
        return mock_reg_space;
    (void)size;
    return malloc(size);
}

void iounmap(void* addr)
{
    if (addr == mock_reg_space)
        return;
    free(addr);
}

uint32_t readl(const volatile void* addr)
{
    return *(const volatile uint32_t*)addr;
}
void writel(uint32_t value, volatile void* addr)
{
    *(volatile uint32_t*)addr = value;
}

void memcpy_toio(volatile void* dst, const void* src, size_t count)
{
    memcpy((void*)dst, src, count);
}
void memcpy_fromio(void* dst, const volatile void* src, size_t count)
{
    memcpy(dst, (const void*)src, count);
}

unsigned long copy_from_user(void* to, const void* from, unsigned long n)
{
    memcpy(to, from, n);
    return 0;
}
unsigned long copy_to_user(void* to, const void* from, unsigned long n)
{
    memcpy(to, from, n);
    return 0;
}

int misc_register(struct miscdevice* misc)
{
    (void)misc;
    if (mock_misc_register_fail)
        return -EINVAL;
    return 0;
}

void misc_deregister(struct miscdevice* misc)
{
    (void)misc;
}

char* d_path(const struct path* path, char* buf, int buflen)
{
    (void)path;
    (void)buflen;
    strncpy(buf, mock_exe_path, buflen - 1);
    buf[buflen - 1] = '\0';
    return buf;
}

void mutex_init(struct mutex* m)
{
    pthread_mutex_init(&m->mtx, NULL);
}
void mutex_lock(struct mutex* m)
{
    pthread_mutex_lock(&m->mtx);
}
void mutex_unlock(struct mutex* m)
{
    pthread_mutex_unlock(&m->mtx);
}

void msleep(unsigned int msecs)
{
    if (!mock_msleep_instant)
        usleep(msecs * 1000);
}

int strscpy(char* dest, const char* src, size_t count)
{
    size_t len = strlen(src);
    if (len >= count) {
        if (count > 0) {
            memcpy(dest, src, count - 1);
            dest[count - 1] = '\0';
        }
        return -E2BIG;
    }
    memcpy(dest, src, len + 1);
    return (int)len;
}

void init_waitqueue_head(struct wait_queue_head* q)
{
    (void)q;
}
