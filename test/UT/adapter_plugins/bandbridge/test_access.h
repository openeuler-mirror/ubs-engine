#ifndef TEST_ACCESS_H
#define TEST_ACCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bandbridge_ctrlq.h"
#include "bandbridge_device.h"
#include "mock_kernel.h"

extern struct bandbridge_ctrlq_info g_ctrlq_info;

int test_check_permission(void);
int test_open(struct inode* inode, struct file* filp);
int test_close(struct inode* inode, struct file* filp);
int test_validate_user_buf(struct bandbridge_mbuf* tmpbuf);
int test_do_send_recv(struct bandbridge_mbuf* mbuf, struct bandbridge_mbuf* tmpbuf);
long test_send_request(struct bandbridge_mbuf* mbuf, void* arg);
long test_ioctl(struct file* filp, unsigned int cmd, unsigned long arg);
void test_reset_bandbridge_ctx(void);

int test_ctrlq_remain_space(void);
int test_ctrlq_rq_is_empty(void);
void test_ctrlq_update_rq_ci(u8 bb_num);
void test_ctrlq_reset_rq_ci(void);
void test_ctrlq_read_data_from_rq(void* recvbuf, u8 bb_num);
int test_ctrlq_map_reg(void);
void test_ctrlq_unmap_reg(void);
int test_ctrlq_map_queue(void);
void test_ctrlq_unmap_queue(void);
void test_set_ctrlq_timeout(int timeout);
void test_reset_ctrlq_globals(void);
void mock_set_exe_file_null(void);

#ifdef __cplusplus
}
#endif

static inline void write_msg_header(char* base, u16 seq, u8 bb_num)
{
    base[0] = 1;
    base[1] = 2;
    base[2] = bb_num;
    base[3] = 3;
    base[4] = 0;
    *(u16*)(base + 6) = seq;
}

#endif
