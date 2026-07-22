#include <gtest/gtest.h>
#include "test_access.h"

class BandbridgeDeviceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        mock_reset_all();
        test_reset_bandbridge_ctx();
        test_reset_ctrlq_globals();
        mock_msleep_instant = true;
    }

    void TearDown() override
    {
        mock_msleep_instant = false;
    }
};

static void setup_rq_with_response(int depth, u16 seq, u8 bb_num)
{
    char* base = (char*)malloc(depth * CTRLQ_BB_SIZE);
    memset(base, 0, depth * CTRLQ_BB_SIZE);
    g_ctrlq_info.rq.depth = depth;
    g_ctrlq_info.rq.base_addr = base;
    g_ctrlq_info.rq.ci = 0;
    g_ctrlq_info.rq.pi = 0;
    mock_set_reg(CTRLQ_RX_TAIL_REG, bb_num);

    write_msg_header(base, seq, bb_num);
}

static void setup_sq_for_send_recv(int depth, u16 seq)
{
    g_ctrlq_info.sq.depth = depth;
    g_ctrlq_info.sq.pi = 0;
    g_ctrlq_info.sq.ci = 0;
    mock_set_reg(CTRLQ_TX_HEAD_REG, 0);
    g_ctrlq_info.sq.base_addr = malloc(depth * CTRLQ_BB_SIZE);
    setup_rq_with_response(depth, seq, 1);
}

static void cleanup_sq_rq()
{
    free(g_ctrlq_info.sq.base_addr);
    g_ctrlq_info.sq.base_addr = NULL;
    free(g_ctrlq_info.rq.base_addr);
    g_ctrlq_info.rq.base_addr = NULL;
}

static void setup_open_mbuf(struct file& filp, struct inode& inode)
{
    mock_set_reg(CTRLQ_TX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_RX_DEPTH_REG, 4);
    g_ctrlq_info.sq.depth = 32;
    g_ctrlq_info.rq.depth = 32;
    memset(&filp, 0, sizeof(filp));
    memset(&inode, 0, sizeof(inode));
    test_open(&inode, &filp);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_AllPass_ReturnsZero)
{
    EXPECT_EQ(test_check_permission(), 0);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_CapNetAdminFails_ReturnsPerm)
{
    mock_set_capable_net_admin(false);
    EXPECT_EQ(test_check_permission(), -EPERM);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_ProcessNameNotUbse_ReturnsPerm)
{
    mock_set_process_name("other_proc");
    EXPECT_EQ(test_check_permission(), -EPERM);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_MmNull_ReturnsPerm)
{
    mock_current_task.mm = NULL;
    EXPECT_EQ(test_check_permission(), -EPERM);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_ExeFileNull_ReturnsPerm)
{
    mock_set_exe_file_null();
    EXPECT_EQ(test_check_permission(), -EPERM);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_ExePathNotUbse_ReturnsPerm)
{
    mock_set_exe_path("/usr/bin/other");
    EXPECT_EQ(test_check_permission(), -EPERM);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_NotInitPidNs_ReturnsPerm)
{
    mock_set_pid_ns_is_init(false);
    EXPECT_EQ(test_check_permission(), -EPERM);
}

TEST_F(BandbridgeDeviceTest, CheckPermission_KmallocFail_ReturnsNoMem)
{
    mock_set_kmalloc_fail(true);
    EXPECT_EQ(test_check_permission(), -ENOMEM);
}

TEST_F(BandbridgeDeviceTest, Open_Success_ReturnsZero)
{
    mock_set_reg(CTRLQ_TX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_RX_DEPTH_REG, 4);
    g_ctrlq_info.sq.depth = 32;
    g_ctrlq_info.rq.depth = 32;

    struct file filp = {0};
    struct inode inode = {0};
    int ret = test_open(&inode, &filp);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(filp.private_data, nullptr);

    struct bandbridge_mbuf* mbuf = (struct bandbridge_mbuf*)filp.private_data;
    EXPECT_NE(mbuf->sendbuf, nullptr);
    EXPECT_NE(mbuf->recvbuf, nullptr);

    test_close(&inode, &filp);
}

TEST_F(BandbridgeDeviceTest, Open_PermissionFail_ReturnsPerm)
{
    mock_set_capable_net_admin(false);
    struct file filp = {0};
    struct inode inode = {0};
    EXPECT_EQ(test_open(&inode, &filp), -EPERM);
}

TEST_F(BandbridgeDeviceTest, Open_KmallocFail_ReturnsNoMem)
{
    mock_set_kmalloc_fail(true);
    g_ctrlq_info.sq.depth = 32;
    g_ctrlq_info.rq.depth = 32;
    struct file filp = {0};
    struct inode inode = {0};
    EXPECT_EQ(test_open(&inode, &filp), -ENOMEM);
}

TEST_F(BandbridgeDeviceTest, Open_SqSizeZero_ReturnsInval)
{
    g_ctrlq_info.sq.depth = 0;
    struct file filp = {0};
    struct inode inode = {0};
    EXPECT_EQ(test_open(&inode, &filp), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, Close_ValidMbuf_FreesAndReturnsZero)
{
    mock_set_reg(CTRLQ_TX_DEPTH_REG, 4);
    mock_set_reg(CTRLQ_RX_DEPTH_REG, 4);
    g_ctrlq_info.sq.depth = 32;
    g_ctrlq_info.rq.depth = 32;

    struct file filp = {0};
    struct inode inode = {0};
    test_open(&inode, &filp);

    int ret = test_close(&inode, &filp);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(filp.private_data, nullptr);
}

TEST_F(BandbridgeDeviceTest, Close_NullPrivateData_ReturnsZero)
{
    struct file filp = {0};
    struct inode inode = {0};
    filp.private_data = NULL;
    EXPECT_EQ(test_close(&inode, &filp), 0);
}

TEST_F(BandbridgeDeviceTest, ValidateUserBuf_ValidBuf_ReturnsZero)
{
    g_ctrlq_info.sq.depth = 32;
    struct bandbridge_mbuf tmpbuf;
    tmpbuf.sendbuf_size = 64;
    EXPECT_EQ(test_validate_user_buf(&tmpbuf), 0);
}

TEST_F(BandbridgeDeviceTest, ValidateUserBuf_NullBuf_ReturnsInval)
{
    EXPECT_EQ(test_validate_user_buf(NULL), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, ValidateUserBuf_SqSizeZero_ReturnsInval)
{
    g_ctrlq_info.sq.depth = 0;
    struct bandbridge_mbuf tmpbuf;
    tmpbuf.sendbuf_size = 64;
    EXPECT_EQ(test_validate_user_buf(&tmpbuf), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, ValidateUserBuf_SendbufSizeZero_ReturnsInval)
{
    g_ctrlq_info.sq.depth = 32;
    struct bandbridge_mbuf tmpbuf;
    tmpbuf.sendbuf_size = 0;
    EXPECT_EQ(test_validate_user_buf(&tmpbuf), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, ValidateUserBuf_SendbufSizeTooLarge_ReturnsInval)
{
    g_ctrlq_info.sq.depth = 32;
    struct bandbridge_mbuf tmpbuf;
    tmpbuf.sendbuf_size = 32 * CTRLQ_BB_SIZE + 1;
    EXPECT_EQ(test_validate_user_buf(&tmpbuf), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, Ioctl_UnknownCmd_ReturnsInval)
{
    struct file filp = {0};
    filp.private_data = NULL;
    EXPECT_EQ(test_ioctl(&filp, 9999, 0), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, DoSendRecv_SqNotEnough_ReturnsNospc)
{
    struct file filp = {0};
    struct inode inode = {0};
    setup_open_mbuf(filp, inode);
    struct bandbridge_mbuf* mbuf = (struct bandbridge_mbuf*)filp.private_data;

    g_ctrlq_info.sq.depth = 1;
    g_ctrlq_info.sq.pi = 1;
    g_ctrlq_info.sq.ci = 0;
    mock_set_reg(CTRLQ_TX_HEAD_REG, 0);

    struct bandbridge_mbuf tmpbuf = {0};
    tmpbuf.sendbuf_size = 2 * CTRLQ_BB_SIZE;
    tmpbuf.sendbuf = mbuf->sendbuf;
    tmpbuf.recvbuf = mbuf->recvbuf;
    tmpbuf.recvbuf_size = CTRLQ_BB_SIZE;
    EXPECT_EQ(test_do_send_recv(mbuf, &tmpbuf), -ENOSPC);

    test_close(&inode, &filp);
}

TEST_F(BandbridgeDeviceTest, DoSendRecv_Success_ReturnsZero)
{
    struct file filp = {0};
    struct inode inode = {0};
    setup_open_mbuf(filp, inode);
    struct bandbridge_mbuf* mbuf = (struct bandbridge_mbuf*)filp.private_data;

    setup_sq_for_send_recv(16, 0x0042);

    struct bandbridge_mbuf tmpbuf = {0};
    tmpbuf.sendbuf_size = CTRLQ_BB_SIZE;
    tmpbuf.recvbuf_size = 2 * CTRLQ_BB_SIZE;
    char user_sendbuf[CTRLQ_BB_SIZE] = {0};
    char user_recvbuf[2 * CTRLQ_BB_SIZE] = {0};
    tmpbuf.sendbuf = user_sendbuf;
    tmpbuf.recvbuf = user_recvbuf;

    write_msg_header(user_sendbuf, 0x0042, 1);

    int ret = test_do_send_recv(mbuf, &tmpbuf);
    EXPECT_EQ(ret, 0);

    cleanup_sq_rq();
    test_close(&inode, &filp);
}

TEST_F(BandbridgeDeviceTest, SendRequest_Success_ReturnsZero)
{
    struct file filp = {0};
    struct inode inode = {0};
    setup_open_mbuf(filp, inode);
    struct bandbridge_mbuf* mbuf = (struct bandbridge_mbuf*)filp.private_data;

    setup_sq_for_send_recv(16, 0x0042);

    struct bandbridge_mbuf user_mbuf = {0};
    user_mbuf.sendbuf_size = CTRLQ_BB_SIZE;
    user_mbuf.recvbuf_size = 2 * CTRLQ_BB_SIZE;
    char user_sendbuf[CTRLQ_BB_SIZE] = {0};
    char user_recvbuf[2 * CTRLQ_BB_SIZE] = {0};
    user_mbuf.sendbuf = user_sendbuf;
    user_mbuf.recvbuf = user_recvbuf;

    write_msg_header(user_sendbuf, 0x0042, 1);

    int ret = (int)test_send_request(mbuf, &user_mbuf);
    EXPECT_EQ(ret, 0);

    cleanup_sq_rq();
    test_close(&inode, &filp);
}

TEST_F(BandbridgeDeviceTest, Ioctl_SendRequestCmd_Success)
{
    struct file filp = {0};
    struct inode inode = {0};
    setup_open_mbuf(filp, inode);
    struct bandbridge_mbuf* mbuf = (struct bandbridge_mbuf*)filp.private_data;

    setup_sq_for_send_recv(16, 0x0042);

    struct bandbridge_mbuf user_mbuf = {0};
    user_mbuf.sendbuf_size = CTRLQ_BB_SIZE;
    user_mbuf.recvbuf_size = 2 * CTRLQ_BB_SIZE;
    char user_sendbuf[CTRLQ_BB_SIZE] = {0};
    char user_recvbuf[2 * CTRLQ_BB_SIZE] = {0};
    user_mbuf.sendbuf = user_sendbuf;
    user_mbuf.recvbuf = user_recvbuf;

    write_msg_header(user_sendbuf, 0x0042, 1);

    long ret = test_ioctl(&filp, BANDBRIDGE_SEND_REQUEST, (unsigned long)&user_mbuf);
    EXPECT_EQ(ret, 0);

    cleanup_sq_rq();
    test_close(&inode, &filp);
}

TEST_F(BandbridgeDeviceTest, CdevRegister_Success_ReturnsZero)
{
    EXPECT_EQ(bandbridge_cdev_register(), 0);
    bandbridge_cdev_unregister();
}

TEST_F(BandbridgeDeviceTest, CdevRegister_MiscRegisterFail_ReturnsError)
{
    mock_set_misc_register_fail(true);
    EXPECT_EQ(bandbridge_cdev_register(), -EINVAL);
}

TEST_F(BandbridgeDeviceTest, CdevUnregister_Success)
{
    bandbridge_cdev_register();
    bandbridge_cdev_unregister();
}
