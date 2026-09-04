/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// Package iface 实现 hostIP → 网卡名 → 掩码前缀的定位逻辑。
// helper 以 hostNetwork 运行,net.Interfaces() 枚举的是宿主机网卡,
// 与 downward API 注入的 status.hostIP 对应即可锁定承载 VIP 的目标网卡。
package iface

import (
	"fmt"
	"net"
)

// Locate 依节点主 IP(hostIP)定位其所在网卡名及该网卡的 IPv4 掩码前缀长度。
// 返回网卡名(如 "eth0")与前缀长度 prefix(1~32)。
func Locate(hostIP string) (string, int, error) {
	target := net.ParseIP(hostIP)
	if target == nil {
		return "", 0, fmt.Errorf("invalid hostIP %q", hostIP)
	}

	ifaces, err := net.Interfaces()
	if err != nil {
		return "", 0, fmt.Errorf("enumerate interfaces: %w", err)
	}

	for _, i := range ifaces {
		addrs, err := i.Addrs()
		if err != nil {
			continue
		}
		for _, a := range addrs {
			ipnet, ok := a.(*net.IPNet)
			if !ok {
				continue
			}
			if !ipnet.IP.Equal(target) {
				continue
			}
			ones, bits := ipnet.Mask.Size()
			if bits != 32 {
				return "", 0, fmt.Errorf("iface %s mask bits=%d, expect IPv4 (/32)", i.Name, bits)
			}
			if ones < 1 || ones > 32 {
				return "", 0, fmt.Errorf("iface %s invalid IPv4 prefix %d", i.Name, ones)
			}
			return i.Name, ones, nil
		}
	}
	return "", 0, fmt.Errorf("no interface holds hostIP %s", hostIP)
}