#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

"""
URMA EID Collector

Collect:
- Host UUID
- Host EID
- Container -> URMA device mapping
- Device -> EID mapping

Generate Prometheus textfile metrics.
Metric value format kept as original (EID string as value)
"""

import json
import subprocess
import sys
import argparse
import os
import re
import shutil
from typing import Dict, List, Optional


class UrmaEidCollector:
    """Collects URMA device EID mappings and exports Prometheus metrics."""

    K8S_CHECKPOINT_PATH = "/var//lib/kubelet/device-plugins/kubelet_internal_checkpoint"
    HOST_UUID_CMD = ["dmidecode", "-t", "system"]
    HOST_EID_CMD = ["sudo", "-u", "ubse", "ubsectl", "display", "node"]
    DEVICE_EID_CMD_PREFIX = ["sudo", "-u", "ubse", "ubsectl", "display", "urma", "--dev"]
    FROM_FILE_DEFAULT = "/var/log/ub_dev_eid.prom"
    KUBELET_IP_CMD = ["ss", "-lntp"]
    CRICTL_PS_CMD = ["crictl", "ps", "-o", "json"]
    CRICTL_INSPECTP_CMD = ["crictl", "inspectp"]

    URMA_VFE_RESOURCE_NAME = "unifiedbus.com/urma.vfe"
    URMA_SHARE_RESOURCE_NAME = "unifiedbus.com/urma.shared"

    UUID_PATTERN = re.compile(r'UUID:\s*([0-9a-fA-F\-]+)')
    EID_PATTERN = re.compile(r'^[0-9a-fA-F]{4}(?::[0-9a-fA-F]{4}){7}$')
    IP_PATTERN = re.compile(r'(\d+\.\d+\.\d+\.\d+):\d+')

    def __init__(self, output_path: str):
        """Initialize collector with output path."""
        self.output_path = output_path
        self.host_uuid: Optional[str] = None
        self.host_eid: Optional[str] = None
        self.container_mappings: List[Dict] = []
        self.device_eid_map: Dict[str, str] = {}
        self.host_ip: Optional[str] = None

    def _run_cmd(self, cmd: List[str], timeout: int = 15) -> str:
        """Run a command and return stdout."""
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                check=True
            )
            return result.stdout.strip()
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"Command timed out: {' '.join(cmd)}")
        except subprocess.CalledProcessError as e:
            raise RuntimeError(
                f"Command failed ({e.returncode}): {' '.join(cmd)}\n{e.stderr.strip()}"
            )

    def get_host_ip(self) -> str:
        """Get kubelet listening IP (non-loopback)"""
        print("  - Getting kubelet listening IP...")
        output = self._run_cmd(self.KUBELET_IP_CMD)

        for line in output.splitlines():
            if "kubelet" in line and "LISTEN" in line:
                match = self.IP_PATTERN.search(line)
                if not match:
                    continue
                ip = match.group(1)
                if ip != "127.0.0.1":
                    print(f"    Host IP: {ip}")
                    return ip

        print("  - No non-loopback IP found,using 127.0.0.1")
        return "127.0.0.1"

    def build_container_ip_map(self):
        """
        Build (container_name, pod_uid) -> pod_ip mapping using crictl inspectp.

        Rules:
          - Only collect IPs for containers present in container_mappings.
          - Match using container_name + pod_uid.
          - Avoid duplicate inspectp calls for the same podSandboxId.
        """
        print("  - Building container IP map via crictl inspectp...")

        container_ip_map = {}
        removed = 0

        try:
            ps_output = self._run_cmd(self.CRICTL_PS_CMD)
            ps_data = json.loads(ps_output)
        except Exception as e:
            print(f"ERROR: crictl ps failed: {e}")
            self.container_mappings = []
            return

        # Build a set of valid (container_name, pod_uid) keys from container_mappings
        mapping_keys = {
            (m["container_name"], m["pod_uid"])
            for m in self.container_mappings
        }

        # Cache inspected pod results to avoid duplicate inspectp calls
        inspected_pods = {}

        for item in ps_data.get("containers", []):
            container_name = item.get("metadata", {}).get("name")
            pod_id = item.get("podSandboxId")

            if not container_name or not pod_id:
                continue

            # Skip containers not present in container_mappings
            if not any(container_name == m["container_name"] for m in self.container_mappings):
                continue

            # Inspect each podSandboxId only once
            if pod_id not in inspected_pods:
                try:
                    inspect_output = self._run_cmd(self.CRICTL_INSPECTP_CMD + [pod_id])
                    inspect_data = json.loads(inspect_output)

                    pod_uid = inspect_data.get("status", {}).get("metadata", {}).get("uid")
                    pod_ip = inspect_data.get("status", {}).get("network", {}).get("ip")

                    inspected_pods[pod_id] = (pod_uid, pod_ip)

                except Exception as e:
                    print(f"      Failed to inspect pod {pod_id}: {e}")
                    continue

            pod_uid, pod_ip = inspected_pods.get(pod_id, (None, None))

            if not pod_uid:
                continue

            key = (container_name, pod_uid)

            # Only collect IP if the key exists in container_mappings
            if key not in mapping_keys:
                continue

            if pod_ip:
                container_ip_map[key] = pod_ip
                print(f"      container={container_name}, pod_uid={pod_uid}, ip={pod_ip}")

        # Update container_mappings and remove entries without valid IP
        filtered = []
        for m in self.container_mappings:
            key = (m["container_name"], m["pod_uid"])
            ip = container_ip_map.get(key, None)

            if not ip:
                removed += 1
                continue

            m["ip_addr"] = ip
            filtered.append(m)

        self.container_mappings = filtered

        print(
            f"    Found {len(container_ip_map)} container IP mappings, "
            f"removed {removed} container_mappings entries"
        )

    def get_host_uuid(self) -> str:
        """ Retrieve host UUID using dmidecode."""
        print("  - Getting host UUID...")
        output = self._run_cmd(self.HOST_UUID_CMD)
        match = self.UUID_PATTERN.search(output)
        if not match:
            raise RuntimeError(f"ERROR: host UUID not found: {output}")
        uuid = match.group(1).strip().lower()
        print(f"    Host UUID: {uuid}")
        return uuid

    def get_host_eid(self) -> str:
        """ Retrieve host URMA EID"""
        print("  - Getting host EID...")
        output = self._run_cmd(self.HOST_EID_CMD)
        for token in output.split():
            if self.EID_PATTERN.match(token):
                eid = token
                print(f"     Host EID: {eid}")
                return eid
        raise RuntimeError(f"ERROR: host EID not found: {output}")

    def parse_k8s_checkpoint(self) -> List[Dict]:
        """Parse Kubernetes device plugin checkpoint dile and extract container-device mapping."""
        print("  - Parsing k8s checkpoint file...")
        if not os.path.exists(self.K8S_CHECKPOINT_PATH):
            raise FileNotFoundError(self.K8S_CHECKPOINT_PATH)

        with open(self.K8S_CHECKPOINT_PATH, 'r') as f:
            data = json.load(f)

        entries = data.get("Data", {}).get("PodDeviceEntries", [])
        mappings = []

        for entry in entries:
            resource_name = entry.get("ResourceName", "").strip()
            if resource_name not in {self.URMA_VFE_RESOURCE_NAME, self.URMA_SHARE_RESOURCE_NAME}:
                continue

            pod_uid = entry.get("PodUID", "").strip()
            container_name = entry.get("ContainerName", "").strip()
            device_ids_map = entry.get("DeviceIDs", {})
            if not isinstance(device_ids_map, dict):
                continue

            device_ids = [
                dev
                for dev_list in device_ids_map.values()
                if isinstance(dev_list, list)
                for dev in dev_list
            ]

            if pod_uid and container_name and device_ids:
                mappings.append({
                    "pod_uid": pod_uid,
                    "container_name": container_name,
                    "device_id": device_ids[0],
                })

        print(f"    Found {len(mappings)} container URMA device mappings")
        if mappings:
            device_list_preview = ", ".join([m["device_id"] for m in mappings[:5]])
            if len(mappings) > 5:
                device_list_preview += "..."
            print(f"    Devices to query: {device_list_preview}")
        return mappings

    def get_device_eids(self, device_list: List[str]) -> Dict[str, str]:
        """Batch query URMA device EIDs for given device IDs."""
        if not device_list:
            print("  - No container devices found, skipping device EID query")
            return {}

        print("  - Querying device EIDs...")
        unique_devices = list({d for d in device_list})
        cmd = self.DEVICE_EID_CMD_PREFIX + [",".join(unique_devices)]

        output = self._run_cmd(cmd, timeout=20)
        eid_map = {}
        for line in output.splitlines():
            parts = line.strip().split()
            if len(parts) < 3:
                continue
            dev_name = parts[0]
            dev_eid = parts[2]
            if self.EID_PATTERN.match(dev_eid):
                eid_map[dev_name] = dev_eid

        print(f"    Retrieved EIDS for {len(eid_map)}/{len(device_list)} devices.")
        return eid_map

    def generate_prom_file(self):
        """Generate Prometheus metrics file."""
        print("  - Generating Prometheus metrics file...")
        lines = []
        lines.append("# HELP urma_eid_type URMA device EID information")
        lines.append("# TYPE urma_eid_type gauge")

        # Host metric: dev_eid=host_eid, value=0
        lines.append(
            f'urma_eid_type{{pod_uid="",container="",host_uuid="{self.host_uuid}",'
            f'ub_dev="",dev_eid="{self.host_eid}",ip_addr="{self.host_ip}"}} 0'
        )

        # Container metric: dev_eid=device EID, value=1
        for mapping in self.container_mappings:
            dev_id = mapping["device_id"]
            eid = self.device_eid_map.get(dev_id)
            if not eid:
                continue
            labels = (
                f'pod_uid="{mapping["pod_uid"]}",'
                f'container="{mapping["container_name"]}",'
                f'host_uuid="{self.host_uuid}",'
                f'ub_dev="{dev_id}",'
                f'dev_eid="{eid}",'
                f'ip_addr="{mapping["ip_addr"]}"'
            )
            lines.append(f'urma_eid_type{{{labels}}} 1')

        os.makedirs(os.path.dirname(self.output_path), exist_ok=True)
        with open(self.output_path, 'w') as f:
            f.write("\n".join(lines) + "\n")
        print(f"Prometheus metrics file generated: {self.output_path}")

    def run(self):
        """Execute the collection workflow."""
        print("Starting URMA EID collection...")
        self.host_uuid = self.get_host_uuid()
        self.host_eid = self.get_host_eid()
        self.host_ip = self.get_host_ip()

        self.container_mappings = self.parse_k8s_checkpoint()

        self.build_container_ip_map()

        device_list = [m["device_id"] for m in self.container_mappings]
        self.device_eid_map = self.get_device_eids(device_list)

        self.generate_prom_file()
        print("Collection completed successfully!")


def parse_args():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="URMA EID Collector for Huawei Lingqu Super Node",
        epilog="Example: python3 urma_eid_collector.py --output /var/log/ub_dev_eid.prom",
    )
    parser.add_argument(
        "-o", "--output",
        default=UrmaEidCollector.FROM_FILE_DEFAULT,
        help=f"Output path for Prometheus metrics file (default: {UrmaEidCollector.FROM_FILE_DEFAULT})",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output",
    )
    return parser.parse_args()


def main():
    """Command-line entry point."""
    # Check root privileges
    if os.getuid() != 0:
        print("Error:This script requires root privileges.", file=sys.stderr)
        sys.exit(1)

    # Check required commands
    for cmd in ["dmidecode", "ubsectl", "crictl"]:
        if not shutil.which(cmd):
            print(f"Error: Required command '{cmd}' not found in PATH.", file=sys.stderr)
            sys.exit(1)

    args = parse_args()

    # Validate output path
    output_dir = os.path.dirname(args.output) or "."
    if not os.path.exists(output_dir):
        print(f"Error: Output directory does not exist:'{output_dir}'.", file=sys.stderr)
        sys.exit(1)

    try:
        collector = UrmaEidCollector(args.output)
        collector.run()
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
