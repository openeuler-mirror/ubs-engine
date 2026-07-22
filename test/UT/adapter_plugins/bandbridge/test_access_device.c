#include "bandbridge_device.h"
#include "mock_kernel.h"

#include "bandbridge_device.c"

int test_check_permission(void)
{
    return bandbridge_check_permission();
}
int test_open(struct inode* inode, struct file* filp)
{
    return bandbridge_open(inode, filp);
}
int test_close(struct inode* inode, struct file* filp)
{
    return bandbridge_close(inode, filp);
}
int test_validate_user_buf(struct bandbridge_mbuf* tmpbuf)
{
    return bandbridge_validate_user_buf(tmpbuf);
}
int test_do_send_recv(struct bandbridge_mbuf* mbuf, struct bandbridge_mbuf* tmpbuf)
{
    return bandbridge_do_send_recv(mbuf, tmpbuf);
}
long test_send_request(struct bandbridge_mbuf* mbuf, void* arg)
{
    return bandbridge_send_request(mbuf, arg);
}
long test_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    return bandbridge_ioctl(filp, cmd, arg);
}

void test_reset_bandbridge_ctx(void)
{
    memset(&g_bandbridge_ctx, 0, sizeof(g_bandbridge_ctx));
}
