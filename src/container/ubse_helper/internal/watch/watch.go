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

// Package watch 实现对 Endpoints VIP 真源的 list/watch 与契约校验。
// 契约:恰好 1 个 IPv4 地址 + 1 个 port,否则显式报错、不注入。
package watch

import (
	"context"
	"fmt"
	"log"
	"net"
	"time"

	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	watchapi "k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
)

// Vip 为一条从 Endpoints 解析出的 VIP 真源(单一 IPv4 地址 + 单一端口)。
type Vip struct {
	IP   net.IP
	Port uint16
}

// Endpoints 返回 Endpoints 变更的 VIP 流与错误流。
// watch 断流(网络抖动/资源重建)时自动从 List 重连,事件语义收敛于最新状态。
func Endpoints(ctx context.Context, clientset kubernetes.Interface, namespace, name string) (<-chan Vip, <-chan error) {
	vipCh := make(chan Vip)
	errCh := make(chan error, 1)

	go func() {
		defer close(vipCh)
		for ctx.Err() == nil {
			if err := run(ctx, clientset, namespace, name, vipCh); err != nil {
				if ctx.Err() != nil {
					return
				}
				select {
				case errCh <- err:
				default:
				}
				// 简单退避,避免断流后忙轮询
				select {
				case <-ctx.Done():
					return
				case <-time.After(5 * time.Second):
				}
			}
		}
	}()
	return vipCh, errCh
}

func run(ctx context.Context, clientset kubernetes.Interface, namespace, name string, vipCh chan<- Vip) error {
	eps := clientset.CoreV1().Endpoints(namespace)
	fieldSel := fmt.Sprintf("metadata.name=%s", name)

	// 首次 List,拿到当前 VIP 真源与 resourceVersion 作为 watch 起点
	list, err := eps.List(ctx, metav1.ListOptions{FieldSelector: fieldSel})
	if err != nil {
		return fmt.Errorf("list endpoints %s/%s: %w", namespace, name, err)
	}
	if len(list.Items) == 0 {
		return fmt.Errorf("endpoints %s/%s not found", namespace, name)
	}
	vip, err := ParseVip(&list.Items[0])
	if err != nil {
		return fmt.Errorf("endpoints %s/%s contract violation: %w", namespace, name, err)
	}
	if !push(ctx, vipCh, vip) {
		return ctx.Err()
	}

	w, err := eps.Watch(ctx, metav1.ListOptions{
		FieldSelector:   fieldSel,
		ResourceVersion: list.ResourceVersion,
	})
	if err != nil {
		return fmt.Errorf("watch endpoints %s/%s: %w", namespace, name, err)
	}
	defer w.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case ev, ok := <-w.ResultChan():
			if !ok {
				return fmt.Errorf("watch channel closed")
			}
			ep, ok := ev.Object.(*corev1.Endpoints)
			if ev.Type == watchapi.Error || !ok {
				// ERROR(如 RV 过期)或意外类型:记录日志,交由断流重连逻辑恢复,不误判为删除
				log.Printf("[VIP] watch event type=%v, object cast ok=%v", ev.Type, ok)
				continue
			}
			if ev.Type == watchapi.Deleted {
				// 删除事件:通知上层清除缓存,停止心跳重推旧 VIP
				if !push(ctx, vipCh, Vip{}) {
					return ctx.Err()
				}
				continue
			}
			if len(ep.Subsets) == 0 {
				// subsets 被清空等价于真源删除:清除缓存,停止重推
				if !push(ctx, vipCh, Vip{}) {
					return ctx.Err()
				}
				continue
			}
			vip, err := ParseVip(ep)
			if err != nil {
				log.Printf("[VIP] endpoints %s/%s contract violation: %v", namespace, name, err)
				continue
			}
			if !push(ctx, vipCh, vip) {
				return ctx.Err()
			}
		}
	}
}

func push(ctx context.Context, vipCh chan<- Vip, vip Vip) bool {
	select {
	case vipCh <- vip:
		return true
	case <-ctx.Done():
		return false
	}
}

// ParseVip 校验 Endpoints 契约(恰好 1 IPv4 + 1 port)并解析出 Vip。
func ParseVip(ep *corev1.Endpoints) (Vip, error) {
	if len(ep.Subsets) != 1 {
		return Vip{}, fmt.Errorf("require exactly 1 subset, got %d", len(ep.Subsets))
	}
	subset := ep.Subsets[0]
	if len(subset.Addresses) != 1 {
		return Vip{}, fmt.Errorf("require exactly 1 IPv4 address, got %d", len(subset.Addresses))
	}
	if len(subset.Ports) != 1 {
		return Vip{}, fmt.Errorf("require exactly 1 port, got %d", len(subset.Ports))
	}
	ip := net.ParseIP(subset.Addresses[0].IP).To4()
	if ip == nil {
		return Vip{}, fmt.Errorf("address %q is not a valid IPv4", subset.Addresses[0].IP)
	}
	port := subset.Ports[0].Port
	if port < 1024 || port > 65535 {
		return Vip{}, fmt.Errorf("port %d out of range [1024,65535]", port)
	}
	return Vip{IP: ip, Port: uint16(port)}, nil
}