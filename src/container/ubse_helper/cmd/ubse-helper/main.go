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

// Package main 为 helper 服务入口:
// 监听 Endpoints VIP 真源 → 依 hostIP 定位目标网卡与掩码前缀 → 经 UDS 注入宿主机 ubse,
// 并以 30s 心跳重推(幂等),保证 master 重启/绑定丢失后自动恢复。
package main

import (
	"context"
	"encoding/binary"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"atomgit.com/openeuler/ubs-engine.git/src/container/ubse_helper/internal/iface"
	"atomgit.com/openeuler/ubs-engine.git/src/container/ubse_helper/internal/inject"
	"atomgit.com/openeuler/ubs-engine.git/src/container/ubse_helper/internal/watch"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ipc"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
)

// heartbeatInterval 为心跳重推周期。
// 服务端对"配置未变"幂等处理:已绑定 no-op,未绑定且 master 补 bind。
const heartbeatInterval = 30 * time.Second

func main() {
	hostIP := os.Getenv("HOST_IP")
	namespace := os.Getenv("POD_NAMESPACE")
	endpointsName := envOr("ENDPOINTS_NAME", "ubse-helper")
	udsSocketPath := os.Getenv("UDS_SOCKET_PATH")

	if hostIP == "" || namespace == "" {
		log.Fatalf("[VIP] missing env: HOST_IP=%q POD_NAMESPACE=%q", hostIP, namespace)
	}
	if udsSocketPath != "" {
		ipc.SetSocketPath(udsSocketPath)
	}

	// 定位承载 hostIP 的网卡及掩码前缀(节点静态信息,启动期解析一次)
	ifaceName, prefix, err := iface.Locate(hostIP)
	if err != nil {
		log.Fatalf("[VIP] locate iface by hostIP %q failed: %v", hostIP, err)
	}
	log.Printf("[VIP] hostIP=%s -> iface=%s prefix=%d", hostIP, ifaceName, prefix)

	// in-cluster 配置:hostNetwork 下仍挂载 SA token,可正常访问 apiserver
	config, err := rest.InClusterConfig()
	if err != nil {
		log.Fatalf("[VIP] in-cluster config failed: %v", err)
	}
	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		log.Fatalf("[VIP] build clientset failed: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	vipCh, errCh := watch.Endpoints(ctx, clientset, namespace, endpointsName)
	heartbeat := time.NewTicker(heartbeatInterval)
	defer heartbeat.Stop()

	var lastPayload *inject.VipCfgPushPayload
	run := func(vip watch.Vip) {
		payload, err := buildPayload(vip, ifaceName, prefix)
		if err != nil {
			log.Printf("[VIP] build payload failed: %v", err)
			return
		}
		lastPayload = payload
		if err := inject.PushVipCfg(payload); err != nil {
			log.Printf("[VIP] push vip cfg failed: %v", err)
			return
		}
		log.Printf("[VIP] pushed vip cfg: addr=%s port=%d prefix=%d iface=%s",
			vip.IP.String(), vip.Port, prefix, ifaceName)
	}

	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGTERM, syscall.SIGINT)

	for {
		select {
		case vip, ok := <-vipCh:
			if !ok {
				log.Printf("[VIP] watch closed, exit")
				return
			}
			if vip.IP == nil { // 约定:零值 Vip 表示真源已删除
				lastPayload = nil
				log.Printf("[VIP] endpoints deleted, stop re-push")
				continue
			}
			run(vip)
		case err := <-errCh:
			log.Printf("[VIP] watch error: %v", err)
		case <-heartbeat.C:
			if lastPayload != nil {
				if err := inject.PushVipCfg(lastPayload); err != nil {
					log.Printf("[VIP] heartbeat re-push failed: %v", err)
				}
			}
		case <-sigCh:
			log.Printf("[VIP] signal received, exit")
			cancel()
			return
		}
	}
}

func buildPayload(vip watch.Vip, ifaceName string, prefix int) (*inject.VipCfgPushPayload, error) {
	ip4 := vip.IP.To4()
	if ip4 == nil {
		return nil, fmt.Errorf("vip %s is not IPv4", vip.IP)
	}
	var name [16]byte
	copy(name[:], ifaceName)
	return &inject.VipCfgPushPayload{
		Addr:   binary.BigEndian.Uint32(ip4),
		Port:   vip.Port,
		Prefix: uint8(prefix),
		Iface:  name,
	}, nil
}

func envOr(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}