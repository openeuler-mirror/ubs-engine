#include "bandbridge_ctrlq.h"
#include "mock_kernel.h"

#include "bandbridge_ctrlq.c"

int test_ctrlq_remain_space(void)
{
    return bandbridge_ctrlq_remain_space();
}
int test_ctrlq_rq_is_empty(void)
{
    return bandbridge_ctrlq_rq_is_empty();
}
void test_ctrlq_update_rq_ci(u8 bb_num)
{
    bandbridge_ctrlq_update_rq_ci(bb_num);
}
void test_ctrlq_reset_rq_ci(void)
{
    bandbridge_ctrlq_reset_rq_ci();
}
void test_ctrlq_read_data_from_rq(void* recvbuf, u8 bb_num)
{
    bandbridge_ctrlq_read_data_from_rq(recvbuf, bb_num);
}
int test_ctrlq_map_reg(void)
{
    return bandbridge_ctrlq_map_reg();
}
void test_ctrlq_unmap_reg(void)
{
    bandbridge_ctrlq_unmap_reg();
}
int test_ctrlq_map_queue(void)
{
    return bandbridge_ctrlq_map_queue();
}
void test_ctrlq_unmap_queue(void)
{
    bandbridge_ctrlq_unmap_queue();
}
void test_set_ctrlq_timeout(int timeout)
{
    bandbridge_ctrlq_timeout = timeout;
}

void test_reset_ctrlq_globals(void)
{
    g_ctrlq_va = mock_reg_space;
    memset(&g_ctrlq_info, 0, sizeof(g_ctrlq_info));
    bandbridge_ctrlq_timeout = 1000;
}
