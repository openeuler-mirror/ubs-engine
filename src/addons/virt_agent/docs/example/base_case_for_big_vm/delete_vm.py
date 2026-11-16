# -*- coding: utf-8 -*-
"""超大规格虚机删除示例.

先undefine并销毁虚机, 等待虚机占用的大页内存释放; 若当前节点仍存在借用内存,
再通过内存归还接口触发内存归还, 使节点内存恢复到借用前状态.

使用方式: 按需修改下方配置项后执行 `python3 delete_vm.py`.
"""
import asyncio
import logging
import subprocess

from ubse.ubs_virt_agent_fragmentation import (
    ubs_task_result_query,
    ubs_mem_return,
    ubs_mem_fragmentation_node_info_list
)

from ubse.ubs_engine_log import ubs_engine_log_callback_register
from ubse.ubs_virt_agent_log import ubs_virt_agent_log_callback_register

# 屏蔽SDK内部日志, 示例中只输出脚本自身日志
ubs_engine_log_callback_register(lambda level, msg: None)
ubs_virt_agent_log_callback_register(lambda level, msg: None)

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
LOG = logging.getLogger(__name__)

# ==============================================================================
# 虚机基础配置
# ==============================================================================
VM_NAME = "vm1"  # 虚机名称, 需与create_vm.py中的VM_NAME一致

# ==============================================================================
# 大页配置
# ==============================================================================
MB_TO_KB = 1024                                                                  # 1MB对应的KiB数
HUGE_PAGE_TYPE = "2M"                                                            # 大页类型, 支持 "1G" 或 "2M", 需与create_vm.py一致
HUGE_PAGE_TO_KB = {"1G": 1024 * MB_TO_KB, "2M": 2 * MB_TO_KB}[HUGE_PAGE_TYPE]    # 大页大小(KiB), 用于大页信息字典取值

# ==============================================================================
# 超时与轮询参数
# ==============================================================================
VIRSH_TIMEOUT = 3600        # virsh命令执行超时时间, 单位: 秒
MONITOR_INTERVAL = 10       # 虚机状态与内存释放轮询间隔, 单位: 秒
MAX_MONITOR_ATTEMPTS = 720  # 虚机状态最大轮询次数
ASYNC_MAX_RETRY = 360       # 异步任务最大轮询次数
ASYNC_RETRY_INTERVAL = 10   # 异步任务轮询间隔, 单位: 秒
TASK_ID_MAX_LEN = 32        # 异步任务ID最大长度


class AsyncTaskStatus:
    """异步任务状态码, 与接口返回的status字段对应."""

    RUNNING = 1     # 任务执行中
    SUCCESS = 2     # 任务执行成功
    FAILED = 3      # 任务执行失败


# ==============================================================================
# 大页与NUMA信息
# ==============================================================================
def get_huge_page_info(numa_info):
    """获取指定大页类型的大页信息.

    Args:
        numa_info: 单个NUMA信息.

    Returns:
        object | None: 指定大页类型的大页信息(含huge_page_free/huge_page_total);
        该NUMA不支持指定大页类型时返回None.
    """
    return numa_info.numa_huge_page_info.get(HUGE_PAGE_TO_KB)


# ==============================================================================
# libvirt操作与虚机状态等待
# ==============================================================================
async def send_cmd(cmd, timeout=VIRSH_TIMEOUT):
    """异步执行命令并返回标准输出.

    Args:
        cmd: 命令及参数列表.
        timeout: 命令执行超时时间, 单位: 秒.

    Returns:
        str: 命令标准输出(已去除首尾空白).
    """
    result = await asyncio.to_thread(
        subprocess.run, cmd, shell=False, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, text=True
    )
    return result.stdout.strip()


async def get_vm_status(vm_name):
    """查询虚机状态.

    Args:
        vm_name: 虚机名称.

    Returns:
        str | None: 虚机状态(如running); 虚机不存在时返回None.
    """
    try:
        output = await send_cmd(["virsh", "list", "--all"])
        for line in output.splitlines():
            if vm_name in line:
                parts = line.split()
                return parts[2] if len(parts) >= 3 else None
        return None
    except Exception:
        return None


async def wait_vm_deleted(vm_name, max_attempts=MAX_MONITOR_ATTEMPTS, interval=MONITOR_INTERVAL):
    """等待虚机被删除或退出running状态.

    Args:
        vm_name: 虚机名称.
        max_attempts: 最大轮询次数.
        interval: 轮询间隔, 单位: 秒.

    Raises:
        TimeoutError: 超过最大轮询次数虚机仍处于running状态.
    """
    for _ in range(max_attempts):
        status = await get_vm_status(vm_name)
        if status is None or status != "running":
            LOG.info(f"VM {vm_name} has been deleted or is not running")
            return
        await asyncio.sleep(interval)
    raise TimeoutError(f"VM {vm_name} failed to be deleted")


async def undefine_vm(vm_name):
    """删除虚机定义及nvram文件.

    Args:
        vm_name: 虚机名称.
    """
    await send_cmd(["virsh", "undefine", vm_name, "--nvram"])
    LOG.info(f"VM {vm_name} undefined")


async def destroy_vm(vm_name):
    """强制销毁(下电)虚机.

    Args:
        vm_name: 虚机名称.
    """
    try:
        await send_cmd(["virsh", "destroy", vm_name])
        LOG.info(f"VM {vm_name} destroyed")
    except Exception as e:
        LOG.warning(f"Destroy vm failed (may not be running): {e}")


# ==============================================================================
# 内存释放与归还
# ==============================================================================
async def wait_vm_release_mem():
    """确认借用NUMA的大页内存已全部释放.

    Raises:
        Exception: 仍有借用NUMA的大页未全部释放.
    """
    node_info_list = ubs_mem_fragmentation_node_info_list()
    for node_info in node_info_list:
        for numa_info in node_info.numa_infos:
            if numa_info.is_local:
                continue
            huge_page_info = get_huge_page_info(numa_info)
            if huge_page_info is None:
                continue
            if huge_page_info.huge_page_free != huge_page_info.huge_page_total:
                raise Exception(
                    f"Waiting vm mem release, numa {numa_info.numa_id} free "
                    f"{huge_page_info.huge_page_free} != total {huge_page_info.huge_page_total}"
                )
    LOG.info("Release vm mem successfully.")


async def check_remote_mem():
    """检查当前节点是否存在借用(远端)NUMA.

    Returns:
        bool: 当前节点存在借用NUMA时返回True, 否则返回False.
    """
    node_info_list = ubs_mem_fragmentation_node_info_list()
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            if not numa_info.is_local:
                return True
    return False


async def mem_free():
    """通过内存归还接口归还借用内存, 并等待异步任务完成.

    Raises:
        Exception: 任务ID非法、任务查询失败、任务执行失败或轮询超时.
    """
    LOG.info("Executing mem free...")
    _, task_id = ubs_mem_return(is_async=True)
    if len(task_id) > TASK_ID_MAX_LEN:
        raise Exception(f"Illegal task_id: {task_id}")

    for _ in range(ASYNC_MAX_RETRY):
        ret, interface_result = await asyncio.to_thread(ubs_task_result_query, task_id)
        if ret != 0:
            raise RuntimeError(f"Failed to get task result, task_id: {task_id}")

        if interface_result.status == AsyncTaskStatus.RUNNING:
            await asyncio.sleep(ASYNC_RETRY_INTERVAL)
            continue
        elif interface_result.status == AsyncTaskStatus.SUCCESS:
            LOG.info("Mem free task completed successfully.")
            return
        else:
            raise Exception(f"Mem free task failed: {interface_result}")

    raise Exception("Max retry exceeded, mem free task failed")


# ==============================================================================
# 主流程
# ==============================================================================
async def delete():
    """超大规格虚机删除主流程.

    步骤: undefine并销毁虚机 -> 等待虚机状态变更 -> 确认大页内存释放 ->
    存在借用内存时触发内存归还.
    """
    try:
        LOG.info(f"Deleting VM: {VM_NAME}")
        await undefine_vm(VM_NAME)
        await destroy_vm(VM_NAME)
        await wait_vm_deleted(VM_NAME)
        await wait_vm_release_mem()
        if await check_remote_mem():
            await mem_free()
        LOG.info(f"VM {VM_NAME} deleted successfully")
    except Exception as e:
        LOG.error(f"Delete VM failed: {e}")
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(delete())
