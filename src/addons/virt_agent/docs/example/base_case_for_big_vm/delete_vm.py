# -*- coding: utf-8 -*-
import asyncio
import logging
import subprocess

from ubse.ubs_virt_agent_fragmentation import ubs_task_result_query, ubs_mem_return, ubs_mem_fragmentation_node_info_list

from ubse.ubs_engine_log import ubs_engine_log_callback_register
from ubse.ubs_virt_agent_log import ubs_virt_agent_log_callback_register

ubs_engine_log_callback_register(lambda level, msg: None)
ubs_virt_agent_log_callback_register(lambda level, msg: None)

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
LOG = logging.getLogger(__name__)

VM_NAME = "vm1"
VIRSH_TIMEOUT = 3600
MONITOR_INTERVAL = 10
MAX_MONITOR_ATTEMPTS = 720
ASYNC_MAX_RETRY = 360
ASYNC_RETRY_INTERVAL = 10
HUGE_PAGE_TO_KB = 2048


class AsyncTaskStatus:
    RUNNING = 1
    SUCCESS = 2
    FAILED = 3


async def send_cmd(cmd, timeout=VIRSH_TIMEOUT):
    result = await asyncio.to_thread(
        subprocess.run, cmd, shell=False, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, text=True
    )
    return result.stdout.strip()


async def get_vm_status(vm_name):
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
    for _ in range(max_attempts):
        status = await get_vm_status(vm_name)
        if status is None or status != "running":
            LOG.info(f"VM {vm_name} has been deleted or is not running")
            return
        await asyncio.sleep(interval)
    raise TimeoutError(f"VM {vm_name} failed to be deleted")


async def wait_vm_release_mem():
    node_info_list = ubs_mem_fragmentation_node_info_list()
    for node_info in node_info_list:
        for numa_info in node_info.numa_infos:
            if not numa_info.is_local:
                huge_page_free = numa_info.numa_huge_page_info[HUGE_PAGE_TO_KB].huge_page_free
                huge_page_total = numa_info.numa_huge_page_info[HUGE_PAGE_TO_KB].huge_page_total
                if huge_page_free != huge_page_total:
                    raise Exception("Waiting vm mem release")
    LOG.info("Release vm mem successfully.")


async def undefine_vm(vm_name):
    await send_cmd(["virsh", "undefine", vm_name, "--nvram"])
    LOG.info(f"VM {vm_name} undefined")


async def destroy_vm(vm_name):
    try:
        await send_cmd(["virsh", "destroy", vm_name])
        LOG.info(f"VM {vm_name} destroyed")
    except Exception as e:
        LOG.warning(f"Destroy vm failed (may not be running): {e}")


async def mem_free():
    LOG.info("Executing mem free...")
    _, task_id = ubs_mem_return(is_async=True)
    if len(task_id) > 32:
        raise Exception(f"Illegal task_id: {task_id}")

    for _ in range(ASYNC_MAX_RETRY):
        ret, interface_result = await asyncio.to_thread(ubs_task_result_query, task_id)
        if ret != 0:
            raise RuntimeError(f"Failed to get task result, task_id: {task_id}")

        if interface_result.status == AsyncTaskStatus.RUNNING:
            await asyncio.sleep(ASYNC_RETRY_INTERVAL)
            continue
        elif interface_result.status == AsyncTaskStatus.SUCCESS:
            LOG.info(f"Mem free task completed successfully.")
            return
        else:
            raise Exception(f"Mem free task failed: {interface_result}")

    raise Exception(f"Max retry exceeded, mem free task failed")


async def check_remote_mem():
        node_info_list = ubs_mem_fragmentation_node_info_list()
        for node_info in node_info_list:
            if not node_info.is_current:
                continue
            for numa_info in node_info.numa_infos:
                if not numa_info.is_local:
                   return True
        return False


async def delete():
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
