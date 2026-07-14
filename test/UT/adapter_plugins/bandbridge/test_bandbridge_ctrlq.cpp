#include <gtest/gtest.h>
#include "test_access.h"

class BandbridgeCtrlqTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        mock_reset_all();
        test_reset_ctrlq_globals();
        test_set_ctrlq_timeout(2);
        mock_msleep_instant = true;
    }

    void TearDown() override
    {
        mock_msleep_instant = false;
    }
};

static void setup_sq(int depth, void *base_addr)
{
    g_ctrlq_info.sq.depth = depth;
    g_ctrlq_info.sq.base_addr = (void *)base_addr;
    g_ctrlq_info.sq.pi = 0;
    g_ctrlq_info.sq.ci = 0;
}

static void setup_rq(int depth, void *base_addr)
{
    g_ctrlq_info.rq.depth = depth;
    g_ctrlq_info.rq.base_addr = (void *)base_addr;
    g_ctrlq_info.rq.pi = 0;
    g_ctrlq_info.rq.ci = 0;
}

TEST_F(BandbridgeCtrlqTest, GetSqSize_ReturnsCorrectValue)
{
    g_ctrlq_info.sq.depth = 16;
    EXPECT_EQ(bandbridge_ctrlq_get_sq_size(), 16 * CTRLQ_BB_SIZE);
}

TEST_F(BandbridgeCtrlqTest, GetRqSize_ReturnsCorrectValue)
{
    g_ctrlq_info.rq.depth = 16;
    EXPECT_EQ(bandbridge_ctrlq_get_rq_size(), 16 * CTRLQ_BB_SIZE);
}

TEST_F(BandbridgeCtrlqTest, RemainSpace_EmptyQueue_ReturnsDepth)
{
    g_ctrlq_info.sq.depth = 16;
    g_ctrlq_info.sq.pi = 0;
    g_ctrlq_info.sq.ci = 0;
    mock_set_reg(CTRLQ_TX_HEAD_REG, 0);
    EXPECT_EQ(test_ctrlq_remain_space(), 16);
}

TEST_F(BandbridgeCtrlqTest, RemainSpace_HalfFull_ReturnsHalfDepth)
{
    g_ctrlq_info.sq.depth = 16;
    g_ctrlq_info.sq.pi = 8;
    mock_set_reg(CTRLQ_TX_HEAD_REG, 0);
    EXPECT_EQ(test_ctrlq_remain_space(), 8);
}

TEST_F(BandbridgeCtrlqTest, CheckSqEnough_EnoughSpace_ReturnsZero)
{
    g_ctrlq_info.sq.depth = 16;
    g_ctrlq_info.sq.pi = 0;
    mock_set_reg(CTRLQ_TX_HEAD_REG, 0);
    EXPECT_EQ(bandbridge_ctrlq_check_sq_enough(CTRLQ_BB_SIZE), 0);
}

TEST_F(BandbridgeCtrlqTest, CheckSqEnough_NotEnoughSpace_ReturnsNospc)
{
    g_ctrlq_info.sq.depth = 2;
    g_ctrlq_info.sq.pi = 1;
    mock_set_reg(CTRLQ_TX_HEAD_REG, 0);
    EXPECT_EQ(bandbridge_ctrlq_check_sq_enough(3 * CTRLQ_BB_SIZE), -ENOSPC);
}

TEST_F(BandbridgeCtrlqTest, RqIsEmpty_EmptyQueue_ReturnsTrue)
{
    g_ctrlq_info.rq.depth = 16;
    g_ctrlq_info.rq.pi = 0;
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 0);
    EXPECT_EQ(test_ctrlq_rq_is_empty(), 1);
}

TEST_F(BandbridgeCtrlqTest, RqIsEmpty_NotEmpty_ReturnsFalse)
{
    g_ctrlq_info.rq.depth = 16;
    g_ctrlq_info.rq.pi = 0;
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 5);
    EXPECT_EQ(test_ctrlq_rq_is_empty(), 0);
}

TEST_F(BandbridgeCtrlqTest, UpdateRqCi_AdvancesCi)
{
    g_ctrlq_info.rq.depth = 16;
    g_ctrlq_info.rq.ci = 3;
    test_ctrlq_update_rq_ci(4);
    EXPECT_EQ(g_ctrlq_info.rq.ci, 7);
    EXPECT_EQ(mock_get_reg(CTRLQ_RX_HEAD_REG), 7);
}

TEST_F(BandbridgeCtrlqTest, ResetRqCi_SetsCiToPi)
{
    g_ctrlq_info.rq.depth = 16;
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 10);
    test_ctrlq_reset_rq_ci();
    EXPECT_EQ(g_ctrlq_info.rq.ci, 10);
    EXPECT_EQ(mock_get_reg(CTRLQ_RX_HEAD_REG), 10);
}

TEST_F(BandbridgeCtrlqTest, SendToSq_WritesDataAndAdvancesPi)
{
    char *base = (char *)malloc(16 * CTRLQ_BB_SIZE);
    memset(base, 0, 16 * CTRLQ_BB_SIZE);
    setup_sq(16, base);
    mock_set_reg(CTRLQ_TX_TAIL_REG, 0);

    char sendbuf[CTRLQ_BB_SIZE];
    memset(sendbuf, 0xAB, CTRLQ_BB_SIZE);
    bandbridge_ctrlq_send_to_sq(sendbuf, CTRLQ_BB_SIZE);

    EXPECT_EQ(g_ctrlq_info.sq.pi, 1);
    EXPECT_EQ(memcmp(base, sendbuf, CTRLQ_BB_SIZE), 0);

    free(base);
    g_ctrlq_info.sq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, SendToSq_MultiBb_WritesAllAndWraps)
{
    int depth = 4;
    char *base = (char *)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    setup_sq(depth, base);
    mock_set_reg(CTRLQ_TX_TAIL_REG, 0);

    char sendbuf[2 * CTRLQ_BB_SIZE];
    memset(sendbuf, 0xCC, 2 * CTRLQ_BB_SIZE);
    bandbridge_ctrlq_send_to_sq(sendbuf, 2 * CTRLQ_BB_SIZE);

    EXPECT_EQ(g_ctrlq_info.sq.pi, 2);
    EXPECT_EQ(memcmp(base, sendbuf, 2 * CTRLQ_BB_SIZE), 0);

    free(base);
    g_ctrlq_info.sq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, ReadDataFromRq_ReadsCorrectData)
{
    int depth = 8;
    char *base = (char *)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    setup_rq(depth, base);
    g_ctrlq_info.rq.ci = 3;

    char pattern[CTRLQ_BB_SIZE];
    memset(pattern, 0xDD, CTRLQ_BB_SIZE);
    memcpy(base + 3 * CTRLQ_BB_SIZE, pattern, CTRLQ_BB_SIZE);

    char recvbuf[CTRLQ_BB_SIZE] = {0};
    test_ctrlq_read_data_from_rq(recvbuf, 1);
    EXPECT_EQ(memcmp(recvbuf, pattern, CTRLQ_BB_SIZE), 0);

    free(base);
    g_ctrlq_info.rq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, ReceiveFromRq_Success_ReturnsZero)
{
    int depth = 8;
    char *base = (char *)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    setup_rq(depth, base);
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 1);

    write_msg_header(base, 0x0042, 1);

    char recvbuf[2 * CTRLQ_BB_SIZE] = {0};
    int recvbuf_size = 2 * CTRLQ_BB_SIZE;
    int ret = bandbridge_ctrlq_receive_from_rq(recvbuf, &recvbuf_size, 0x0042);
    EXPECT_EQ(ret, 0);

    free(base);
    g_ctrlq_info.rq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, ReceiveFromRq_BbNumZero_ReturnsNospc)
{
    int depth = 8;
    char *base = (char *)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    setup_rq(depth, base);
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 1);

    write_msg_header(base, 0x0042, 0);

    char recvbuf[CTRLQ_BB_SIZE] = {0};
    int recvbuf_size = CTRLQ_BB_SIZE;
    EXPECT_EQ(bandbridge_ctrlq_receive_from_rq(recvbuf, &recvbuf_size, 0x0042), -ENOSPC);

    free(base);
    g_ctrlq_info.rq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, ReceiveFromRq_RecvbufTooSmall_ReturnsNospc)
{
    int depth = 8;
    char *base = (char *)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    setup_rq(depth, base);
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 1);

    write_msg_header(base, 0x0042, 2);

    char recvbuf[CTRLQ_BB_SIZE] = {0};
    int recvbuf_size = CTRLQ_BB_SIZE;
    EXPECT_EQ(bandbridge_ctrlq_receive_from_rq(recvbuf, &recvbuf_size, 0x0042), -ENOSPC);

    free(base);
    g_ctrlq_info.rq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, ReceiveFromRq_SeqMismatch_SkipsAndTimeout)
{
    int depth = 8;
    char *base = (char *)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    setup_rq(depth, base);
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 1);

    write_msg_header(base, 0x0099, 1);

    char recvbuf[CTRLQ_BB_SIZE] = {0};
    int recvbuf_size = CTRLQ_BB_SIZE;
    EXPECT_EQ(bandbridge_ctrlq_receive_from_rq(recvbuf, &recvbuf_size, 0x0042), -ETIMEDOUT);

    free(base);
    g_ctrlq_info.rq.base_addr = NULL;
}

TEST_F(BandbridgeCtrlqTest, ReceiveFromRq_EmptyTimeout_ReturnsTimedout)
{
    setup_rq(8, NULL);
    g_ctrlq_info.rq.ci = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, 0);

    char recvbuf[CTRLQ_BB_SIZE] = {0};
    int recvbuf_size = CTRLQ_BB_SIZE;
    EXPECT_EQ(bandbridge_ctrlq_receive_from_rq(recvbuf, &recvbuf_size, 0x0042), -ETIMEDOUT);
}

TEST_F(BandbridgeCtrlqTest, MapReg_IoremapFail_ReturnsNoMem)
{
    mock_set_ioremap_fail(true);
    EXPECT_EQ(test_ctrlq_map_reg(), -ENOMEM);
}

TEST_F(BandbridgeCtrlqTest, MapReg_Success_ReturnsZero)
{
    int ret = test_ctrlq_map_reg();
    EXPECT_EQ(ret, 0);
    EXPECT_NE(g_ctrlq_va, nullptr);
    test_ctrlq_unmap_reg();
}

TEST_F(BandbridgeCtrlqTest, UnmapQueue_ClearsBaseAddr)
{
    mock_set_reg(CTRLQ_TX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_RX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_TX_BASEADDR_L_REG, 0x1000);
    mock_set_reg(CTRLQ_TX_BASEADDR_H_REG, 0);
    mock_set_reg(CTRLQ_RX_BASEADDR_L_REG, 0x2000);
    mock_set_reg(CTRLQ_RX_BASEADDR_H_REG, 0);
    EXPECT_EQ(test_ctrlq_map_queue(), 0);

    test_ctrlq_unmap_queue();
    EXPECT_EQ(g_ctrlq_info.sq.base_addr, nullptr);
    EXPECT_EQ(g_ctrlq_info.rq.base_addr, nullptr);
}

TEST_F(BandbridgeCtrlqTest, MapQueue_DepthZero_ReturnsInval)
{
    mock_set_reg(CTRLQ_TX_DEPTH_REG, 0);
    mock_set_reg(CTRLQ_RX_DEPTH_REG, 0);
    EXPECT_EQ(test_ctrlq_map_queue(), -EINVAL);
}

TEST_F(BandbridgeCtrlqTest, Init_Success_ReturnsZero)
{
    mock_set_reg(CTRLQ_TX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_RX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_TX_BASEADDR_L_REG, 0x1000);
    mock_set_reg(CTRLQ_TX_BASEADDR_H_REG, 0);
    mock_set_reg(CTRLQ_RX_BASEADDR_L_REG, 0x2000);
    mock_set_reg(CTRLQ_RX_BASEADDR_H_REG, 0);

    int ret = bandbridge_ctrlq_init();
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_ctrlq_info.sq.depth, 4 * CTRLQ_DEPTH_UINT);
    EXPECT_NE(g_ctrlq_info.sq.base_addr, nullptr);
    EXPECT_NE(g_ctrlq_info.rq.base_addr, nullptr);

    bandbridge_ctrlq_deinit();
}

TEST_F(BandbridgeCtrlqTest, Init_IoremapFail_RegMapFails)
{
    mock_set_ioremap_fail(true);
    EXPECT_NE(bandbridge_ctrlq_init(), 0);
}
