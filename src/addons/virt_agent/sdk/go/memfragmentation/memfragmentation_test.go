/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// This file contains pure Go test cases only. The Go toolchain forbids cgo in
// _test.go files (import "C"), so all fixture construction and reading involving
// C structs is bridged in test_cgo_fixture_fragmentation.go, which is compiled
// together with the product; the assertion logic stays here.

package memfragmentation

import (
	"strings"
	"testing"
	"unsafe"
)

// TestSetCStringAndCStringN verifies Go/C string conversion: roundtrip, zeroing, and overlong truncation behavior
func TestSetCStringAndCStringN(t *testing.T) {
	buf := make([]byte, maxNodeIDLen)

	// Normal roundtrip
	src := "node-001"
	setCString(unsafe.Pointer(&buf[0]), len(buf), src)
	got := cStringN(unsafe.Pointer(&buf[0]), len(buf))
	t.Logf("roundtrip: src=%q got=%q bufLen=%d", src, got, len(buf))
	if got != src {
		t.Errorf("roundtrip got %q, want %q", got, src)
	}

	// Write an empty string over a dirty buffer to verify it is zeroed first
	for i := range buf {
		buf[i] = 'x'
	}
	setCString(unsafe.Pointer(&buf[0]), len(buf), "")
	got = cStringN(unsafe.Pointer(&buf[0]), len(buf))
	t.Logf("emptyOverwrite: got=%q", got)
	if got != "" {
		t.Errorf("empty string got %q, want empty", got)
	}

	// Exact fit (len-1 chars) is not truncated
	exact := strings.Repeat("b", maxNodeIDLen-1)
	setCString(unsafe.Pointer(&buf[0]), len(buf), exact)
	got = cStringN(unsafe.Pointer(&buf[0]), len(buf))
	t.Logf("exactFit: len=%d gotLen=%d", len(exact), len(got))
	if got != exact {
		t.Errorf("exact fit got %q, want %q", got, exact)
	}

	// Overlong truncation: 60 chars into the 48-byte buffer, keeping 47 chars
	long := strings.Repeat("a", 60)
	setCString(unsafe.Pointer(&buf[0]), len(buf), long)
	got = cStringN(unsafe.Pointer(&buf[0]), len(buf))
	t.Logf("truncated: srcLen=%d gotLen=%d maxKeep=%d", len(long), len(got), maxNodeIDLen-1)
	if got != long[:maxNodeIDLen-1] {
		t.Errorf("truncated got %q, want %q", got, long[:maxNodeIDLen-1])
	}
}

// TestConvertNumaInfoC verifies conversion from C numa_info_t to the Go struct (including the huge-page array and nil-pointer branches)
func TestConvertNumaInfoC(t *testing.T) {
	info := numaInfoConvertForTest([]NumaPageData{
		{PageSize: 2048, HugePageTotal: 100, HugePageFree: 40},
		{PageSize: 1024, HugePageTotal: 50, HugePageFree: 25},
	})
	t.Logf("numaInfo: nodeId=%s hostName=%s timestamp=%d numaId=%d socketId=%d isLocal=%v memTotal=%d memFree=%d hugePageCount=%d",
		info.NodeId, info.HostName, info.Timestamp, info.NumaId, info.SocketId, info.IsLocal,
		info.MemTotal, info.MemFree, len(info.HugePageData))
	for i, hp := range info.HugePageData {
		t.Logf("  hugePage[%d]: pageSize=%d total=%d free=%d", i, hp.PageSize, hp.HugePageTotal, hp.HugePageFree)
	}
	if info.Timestamp != 1700000000 || info.NodeId != "node-01" || info.HostName != "host-01" {
		t.Errorf("basic fields mismatch: %+v", info)
	}
	if info.NumaId != 3 || info.SocketId != 1 || !info.IsLocal {
		t.Errorf("numa fields mismatch: %+v", info)
	}
	if info.MemTotal != 102400 || info.MemFree != 51200 {
		t.Errorf("mem fields mismatch: %+v", info)
	}
	if len(info.HugePageData) != 2 {
		t.Fatalf("huge page count got %d, want 2", len(info.HugePageData))
	}
	if info.HugePageData[0] != (NumaPageData{PageSize: 2048, HugePageTotal: 100, HugePageFree: 40}) {
		t.Errorf("huge page[0] mismatch: %+v", info.HugePageData[0])
	}
	if info.HugePageData[1] != (NumaPageData{PageSize: 1024, HugePageTotal: 50, HugePageFree: 25}) {
		t.Errorf("huge page[1] mismatch: %+v", info.HugePageData[1])
	}

	// A nil huge-page pointer must not panic and yields no huge-page data
	if info2 := numaInfoConvertForTest(nil); len(info2.HugePageData) != 0 {
		t.Errorf("nil huge page data got %d, want 0", len(info2.HugePageData))
	}
}

// TestConvertMemBorrowResultC verifies borrow-result conversion (including the over-limit clamping branch)
func TestConvertMemBorrowResultC(t *testing.T) {
	result := memBorrowResultConvertForTest("task-123",
		[]string{"borrow-1", "borrow-2"}, []uint16{1, 2}, 2, 2)
	t.Logf("borrowResult: taskId=%s borrowIds=%v presentNumaIds=%v borrowIdsSize=%d presentNumaIdsSize=%d",
		result.TaskId, result.BorrowIds, result.PresentNumaIds, len(result.BorrowIds), len(result.PresentNumaIds))
	if result.TaskId != "task-123" {
		t.Errorf("task id got %q, want %q", result.TaskId, "task-123")
	}
	if len(result.BorrowIds) != 2 || result.BorrowIds[0] != "borrow-1" || result.BorrowIds[1] != "borrow-2" {
		t.Errorf("borrow ids mismatch: %v", result.BorrowIds)
	}
	if len(result.PresentNumaIds) != 2 || result.PresentNumaIds[0] != 1 || result.PresentNumaIds[1] != 2 {
		t.Errorf("present numa ids mismatch: %v", result.PresentNumaIds)
	}

	// Counts above the limit are clamped to maxBorrowIdCount without out-of-bounds access
	clamped := memBorrowResultConvertForTest("task-123",
		[]string{"borrow-1", "borrow-2"}, []uint16{1, 2},
		uint32(maxBorrowIdCount+5), uint32(maxBorrowIdCount+5))
	t.Logf("borrowResultClamped: srcCount=%d clampedBorrowIds=%d clampedNumaIds=%d maxBorrowIdCount=%d",
		maxBorrowIdCount+5, len(clamped.BorrowIds), len(clamped.PresentNumaIds), maxBorrowIdCount)
	if len(clamped.BorrowIds) != maxBorrowIdCount {
		t.Errorf("clamped borrow ids got %d, want %d", len(clamped.BorrowIds), maxBorrowIdCount)
	}
	if len(clamped.PresentNumaIds) != maxBorrowIdCount {
		t.Errorf("clamped numa ids got %d, want %d", len(clamped.PresentNumaIds), maxBorrowIdCount)
	}
}

// TestBuildBorrowSettingC verifies Go-to-C borrow-strategy construction (including destination-count and per-destination NUMA-count clamping)
func TestBuildBorrowSettingC(t *testing.T) {
	setting := BorrowSetting{
		BorrowStrategy: BorrowStrategy{
			SrcHostId:   "src-host",
			SrcSocketId: 1,
			SrcNumaId:   2,
			BorrowSize:  4096,
			DestNumaInfos: []DstNumaInfo{
				{HostId: "dst-host", SocketId: 3, NumaIds: []int32{4}, MemSizes: []uint64{1024}},
				{HostId: "dst-host2", SocketId: 5, NumaIds: []int32{6, 7}, MemSizes: []uint64{2048, 4096}},
				{HostId: "dst-host3", SocketId: 8, NumaIds: nil, MemSizes: []uint64{512}},
			},
		},
		IsAsync: true,
	}

	view := borrowSettingViewForTest(setting)
	t.Logf("borrowSetting: srcHostID=%s srcSocketID=%d srcNumaID=%d borrowSize=%d isAsync=%v destCount=%d",
		view.SrcHostID, view.SrcSocketID, view.SrcNumaID, view.BorrowSize, view.IsAsync, len(view.DestInfos))
	for i, d := range view.DestInfos {
		t.Logf("  dest[%d]: hostID=%s socketID=%d numaNums=%d numaIDs=%v memSizes=%v",
			i, d.HostID, d.SocketID, d.NumaNums, d.NumaIDs, d.MemSizes)
	}
	if view.SrcHostID != "src-host" {
		t.Errorf("src host id got %q", view.SrcHostID)
	}
	if view.SrcSocketID != 1 || view.SrcNumaID != 2 {
		t.Errorf("src socket/numa mismatch: %d/%d", view.SrcSocketID, view.SrcNumaID)
	}
	if view.BorrowSize != 4096 {
		t.Errorf("borrow size got %d", view.BorrowSize)
	}
	if !view.IsAsync {
		t.Error("isAsync should be true")
	}
	if len(view.DestInfos) != 3 {
		t.Errorf("dest count got %d, want 3", len(view.DestInfos))
	}

	d0 := view.DestInfos[0]
	if d0.HostID != "dst-host" {
		t.Errorf("dst[0] host id got %q", d0.HostID)
	}
	if d0.SocketID != 3 || d0.NumaNums != 1 {
		t.Errorf("dst[0] socket/numa nums mismatch: %d/%d", d0.SocketID, d0.NumaNums)
	}
	if len(d0.NumaIDs) != 1 || d0.NumaIDs[0] != 4 || len(d0.MemSizes) != 1 || d0.MemSizes[0] != 1024 {
		t.Errorf("dst[0] numa id/mem size mismatch: %v/%v", d0.NumaIDs, d0.MemSizes)
	}

	// dst[1]: NUMA count exceeding MAX_DEST_NUMA_NUM (1) is clamped to 1
	d1 := view.DestInfos[1]
	if d1.NumaNums != uint16(maxDestNumaNum) {
		t.Errorf("dst[1] numa nums got %d, want %d", d1.NumaNums, maxDestNumaNum)
	}
	if len(d1.NumaIDs) != 1 || d1.NumaIDs[0] != 6 || d1.MemSizes[0] != 2048 {
		t.Errorf("dst[1] first numa mismatch: %v/%v", d1.NumaIDs, d1.MemSizes)
	}

	// dst[2]: numa_nums should be 0 when NumaIds is empty
	d2 := view.DestInfos[2]
	if d2.NumaNums != 0 {
		t.Errorf("dst[2] numa nums got %d, want 0", d2.NumaNums)
	}

	// Destination counts exceeding MAX_DEST_PARAM_SIZE are clamped
	many := make([]DstNumaInfo, maxDestParamSize+10)
	view2 := borrowSettingViewForTest(BorrowSetting{BorrowStrategy: BorrowStrategy{DestNumaInfos: many}})
	t.Logf("borrowSettingClamped: srcDestCount=%d clampedDestCount=%d maxDestParamSize=%d",
		maxDestParamSize+10, len(view2.DestInfos), maxDestParamSize)
	if len(view2.DestInfos) != maxDestParamSize {
		t.Errorf("clamped dest count got %d, want %d", len(view2.DestInfos), maxDestParamSize)
	}
}

// TestBuildRollbackParamC verifies Go-to-C rollback-parameter construction (including borrow-ID count clamping)
func TestBuildRollbackParamC(t *testing.T) {
	view := rollbackParamViewForTest(RollbackParam{
		NodeId:       "node-01",
		BorrowIdList: []string{"bid-1", "bid-2"},
	})
	t.Logf("rollbackParam: nodeID=%s borrowIDSize=%d borrowIDs=%v",
		view.NodeID, view.BorrowIDSize, view.BorrowIDs)
	if view.NodeID != "node-01" {
		t.Errorf("node id got %q", view.NodeID)
	}
	if view.BorrowIDSize != 2 {
		t.Errorf("borrow id size got %d, want 2", view.BorrowIDSize)
	}
	if len(view.BorrowIDs) != 2 || view.BorrowIDs[0] != "bid-1" || view.BorrowIDs[1] != "bid-2" {
		t.Errorf("borrow ids mismatch: %v", view.BorrowIDs)
	}

	// Borrow-ID counts above the limit are clamped to maxBorrowIdCount
	ids := make([]string, maxBorrowIdCount+5)
	for i := range ids {
		ids[i] = "bid"
	}
	view2 := rollbackParamViewForTest(RollbackParam{NodeId: "n", BorrowIdList: ids})
	t.Logf("rollbackParamClamped: srcCount=%d clampedSize=%d maxBorrowIdCount=%d",
		maxBorrowIdCount+5, view2.BorrowIDSize, maxBorrowIdCount)
	if view2.BorrowIDSize != uint32(maxBorrowIdCount) {
		t.Errorf("clamped borrow id size got %d, want %d", view2.BorrowIDSize, maxBorrowIdCount)
	}
}

// TestBuildMemBorrowParamC verifies borrow-parameter construction: empty metas do not allocate, non-empty metas allocate and fill, over-limit counts clamp
func TestBuildMemBorrowParamC(t *testing.T) {
	// No NUMA metas: no C-side memory is allocated
	view1, err := memBorrowParamViewForTest(MemBorrowParam{SrcNid: "node-01", BorrowSize: 2048})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	t.Logf("memBorrowParamEmpty: srcNid=%s borrowSize=%d numaIDs=%v numaLen=%d",
		view1.SrcNid, view1.BorrowSize, view1.NumaIDs, view1.NumaLen)
	if view1.NumaIDs != nil || view1.NumaLen != 0 {
		t.Errorf("empty metas should not allocate: ids=%v len=%d", view1.NumaIDs, view1.NumaLen)
	}
	if view1.SrcNid != "node-01" {
		t.Errorf("src nid got %q", view1.SrcNid)
	}
	if view1.BorrowSize != 2048 {
		t.Errorf("borrow size got %d", view1.BorrowSize)
	}

	// With NUMA metas: memory is allocated and filled
	view2, err := memBorrowParamViewForTest(MemBorrowParam{
		SrcNid:        "node-02",
		NumaMetaInfos: []NumaMetaInfo{{NumaId: 1}, {NumaId: 2}, {NumaId: 3}},
		BorrowSize:    8192,
	})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	t.Logf("memBorrowParamFilled: srcNid=%s borrowSize=%d numaLen=%d numaIDs=%v",
		view2.SrcNid, view2.BorrowSize, view2.NumaLen, view2.NumaIDs)
	if view2.NumaIDs == nil {
		t.Fatal("numa_meta_infos should be allocated")
	}
	if len(view2.NumaIDs) != 3 || view2.NumaLen != 3 {
		t.Errorf("numa len got %d, want 3", view2.NumaLen)
	}
	for i, id := range view2.NumaIDs {
		if id != int16(i+1) {
			t.Errorf("meta[%d].numa_id got %d, want %d", i, id, i+1)
		}
	}

	// NUMA meta counts above the limit are clamped to maxNumaNum
	big := make([]NumaMetaInfo, maxNumaNum+10)
	view3, err := memBorrowParamViewForTest(MemBorrowParam{SrcNid: "node-03", NumaMetaInfos: big})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	t.Logf("memBorrowParamClamped: srcCount=%d clampedLen=%d maxNumaNum=%d",
		maxNumaNum+10, view3.NumaLen, maxNumaNum)
	if view3.NumaLen != uint32(maxNumaNum) {
		t.Errorf("clamped numa len got %d, want %d", view3.NumaLen, maxNumaNum)
	}
}

func TestConvertNodeInfoListC(t *testing.T) {
	node1Numas := []numaSpec{
		{NumaId: 0, SocketId: 0, IsLocal: true, MemTotal: 100000, MemFree: 50000, Timestamp: 1000, HostName: "host1",
			HugePages: []NumaPageData{{PageSize: 4096, HugePageTotal: 100, HugePageFree: 50}}},
		{NumaId: 1, SocketId: 1, IsLocal: true, MemTotal: 16777216, MemFree: 8388608, Timestamp: 1500, HostName: "host1",
			HugePages: []NumaPageData{{PageSize: 1048576, HugePageTotal: 8, HugePageFree: 4}}},
	}
	node2Numas := []numaSpec{
		{NumaId: 1, SocketId: 1, IsLocal: false, MemTotal: 4194304, MemFree: 2097152, Timestamp: 2000, HostName: "host2",
			HugePages: []NumaPageData{{PageSize: 2048, HugePageTotal: 200, HugePageFree: 100},
				{PageSize: 1048576, HugePageTotal: 2, HugePageFree: 1}}},
		{NumaId: 2, SocketId: 2, IsLocal: false, MemTotal: 8388608, MemFree: 4194304, Timestamp: 3000, HostName: "host2",
			HugePages: []NumaPageData{{PageSize: 1048576, HugePageTotal: 4, HugePageFree: 3}}},
	}
	scenarios := []struct {
		label string
		specs []nodeInfoSpec
	}{
		{"master", []nodeInfoSpec{
			{NodeId: "node1", IsCurrent: true, IsMaster: true, PodId: "pod-1", Numas: node1Numas},
			{NodeId: "node2", IsCurrent: false, IsMaster: false, PodId: "", Numas: node2Numas},
		}},
		{"agent", []nodeInfoSpec{
			{NodeId: "node1", IsCurrent: true, IsMaster: false, PodId: "pod-1", Numas: node1Numas},
			{NodeId: "node2", IsCurrent: false, IsMaster: true, PodId: "", Numas: node2Numas},
		}},
	}
	for _, sc := range scenarios {
		nodes, err := nodeInfoListConvertForTest(sc.specs)
		if err != nil {
			t.Fatalf("[%s] unexpected error: %v", sc.label, err)
		}
		if len(nodes) != len(sc.specs) {
			t.Fatalf("[%s] node count got %d, want %d", sc.label, len(nodes), len(sc.specs))
		}
		for i, n := range nodes {
			s := sc.specs[i]
			t.Logf("[%s] nodeInfoList[%d]: nodeId=%s isCurrent=%v isMaster=%v podId=%q numaCount=%d",
				sc.label, i, n.NodeId, n.IsCurrent, n.IsMaster, n.PodId, len(n.NumaInfos))
			if n.NodeId != s.NodeId {
				t.Errorf("[%s] node[%d] id got %q, want %q", sc.label, i, n.NodeId, s.NodeId)
			}
			if n.IsCurrent != s.IsCurrent {
				t.Errorf("[%s] node[%d] isCurrent got %v, want %v", sc.label, i, n.IsCurrent, s.IsCurrent)
			}
			if n.IsMaster != s.IsMaster {
				t.Errorf("[%s] node[%d] isMaster got %v, want %v", sc.label, i, n.IsMaster, s.IsMaster)
			}
			if n.PodId != s.PodId {
				t.Errorf("[%s] node[%d] podId got %q, want %q", sc.label, i, n.PodId, s.PodId)
			}
			if len(n.NumaInfos) != len(s.Numas) {
				t.Errorf("[%s] node[%d] numa count got %d, want %d", sc.label, i, len(n.NumaInfos), len(s.Numas))
				continue
			}
			for j, numa := range n.NumaInfos {
				ns := s.Numas[j]
				t.Logf("[%s]   nodeInfoList[%d].numaInfos[%d]: nodeId=%s hostName=%s numaId=%d socketId=%d isLocal=%v memTotal=%d memFree=%d timestamp=%d hugePageCount=%d",
					sc.label, i, j, numa.NodeId, numa.HostName, numa.NumaId, numa.SocketId,
					numa.IsLocal, numa.MemTotal, numa.MemFree, numa.Timestamp, len(numa.HugePageData))
				if numa.NodeId != s.NodeId {
					t.Errorf("[%s] node[%d] numa[%d] nodeId got %q, want %q", sc.label, i, j, numa.NodeId, s.NodeId)
				}
				if numa.HostName != ns.HostName {
					t.Errorf("[%s] node[%d] numa[%d] hostName got %q, want %q", sc.label, i, j, numa.HostName, ns.HostName)
				}
				if numa.NumaId != ns.NumaId {
					t.Errorf("[%s] node[%d] numa[%d] numaId got %d, want %d", sc.label, i, j, numa.NumaId, ns.NumaId)
				}
				if numa.SocketId != ns.SocketId {
					t.Errorf("[%s] node[%d] numa[%d] socketId got %d, want %d", sc.label, i, j, numa.SocketId, ns.SocketId)
				}
				if numa.IsLocal != ns.IsLocal {
					t.Errorf("[%s] node[%d] numa[%d] isLocal got %v, want %v", sc.label, i, j, numa.IsLocal, ns.IsLocal)
				}
				if numa.MemTotal != ns.MemTotal || numa.MemFree != ns.MemFree {
					t.Errorf("[%s] node[%d] numa[%d] mem got %d/%d, want %d/%d",
						sc.label, i, j, numa.MemTotal, numa.MemFree, ns.MemTotal, ns.MemFree)
				}
				if numa.Timestamp != ns.Timestamp {
					t.Errorf("[%s] node[%d] numa[%d] timestamp got %d, want %d",
						sc.label, i, j, numa.Timestamp, ns.Timestamp)
				}
				if len(numa.HugePageData) != len(ns.HugePages) {
					t.Errorf("[%s] node[%d] numa[%d] huge page count got %d, want %d",
						sc.label, i, j, len(numa.HugePageData), len(ns.HugePages))
					continue
				}
				hpBySize := make(map[uint64]NumaPageData)
				for _, hp := range numa.HugePageData {
					hpBySize[hp.PageSize] = hp
				}
				for _, want := range ns.HugePages {
					got, ok := hpBySize[want.PageSize]
					if !ok {
						t.Errorf("[%s] node[%d] numa[%d] huge page %d missing", sc.label, i, j, want.PageSize)
						continue
					}
					t.Logf("[%s]     nodeInfoList[%d].numaInfos[%d].hugePage: pageSize=%d total=%d free=%d",
						sc.label, i, j, got.PageSize, got.HugePageTotal, got.HugePageFree)
					if got.HugePageTotal != want.HugePageTotal || got.HugePageFree != want.HugePageFree {
						t.Errorf("[%s] node[%d] numa[%d] huge page %d got %d/%d, want %d/%d",
							sc.label, i, j, want.PageSize, got.HugePageTotal, got.HugePageFree,
							want.HugePageTotal, want.HugePageFree)
					}
				}
			}
		}
	}

	empty, err := nodeInfoListConvertForTest(nil)
	t.Logf("nodeInfoListEmpty: len=%d err=%v", len(empty), err)
	if err != nil || len(empty) != 0 {
		t.Errorf("empty list got len=%d err=%v, want len=0 nil", len(empty), err)
	}

	if _, err := nodeInfoListOverCountForTest(uint32(maxNodeNum + 1)); err == nil {
		t.Error("over-limit node count should fail")
	} else {
		t.Logf("nodeInfoListOverCount: maxNodeNum=%d err=%v", maxNodeNum, err)
	}
}

func TestConvertVmInfoListC(t *testing.T) {
	node1Vms := []vmSpec{
		{Uuid: "uuid1", Name: "vm1", State: "running", HostName: "host1", VmCreateTime: 1000, MaxMem: 1024, Pid: 123,
			Numas: []vmNumaSpec{
				{NumaId: 0, SocketId: 0, IsLocal: true, PageSize: 2048, UsedMem: 100},
				{NumaId: 1, SocketId: 1, IsLocal: false, PageSize: 1048576, UsedMem: 1024},
			}},
	}
	node2Vms := []vmSpec{
		{Uuid: "uuid2", Name: "vm2", State: "running", HostName: "host2", VmCreateTime: 2000, MaxMem: 2048, Pid: 456,
			Numas: []vmNumaSpec{
				{NumaId: 1, SocketId: 1, IsLocal: false, PageSize: 2048, UsedMem: 200},
				{NumaId: 2, SocketId: 2, IsLocal: false, PageSize: 1048576, UsedMem: 2048},
			}},
	}
	scenarios := []struct {
		label string
		specs []vmNodeInfoSpec
	}{
		{"master", []vmNodeInfoSpec{
			{NodeId: "node1", IsCurrent: true, IsMaster: true, PodId: "pod-1", Vms: node1Vms},
			{NodeId: "node2", IsCurrent: false, IsMaster: false, PodId: "", Vms: node2Vms},
		}},
		{"agent", []vmNodeInfoSpec{
			{NodeId: "node1", IsCurrent: true, IsMaster: false, PodId: "pod-1", Vms: node1Vms},
			{NodeId: "node2", IsCurrent: false, IsMaster: true, PodId: "", Vms: node2Vms},
		}},
	}
	for _, sc := range scenarios {
		nodes, err := vmInfoListConvertForTest(sc.specs)
		if err != nil {
			t.Fatalf("[%s] unexpected error: %v", sc.label, err)
		}
		if len(nodes) != len(sc.specs) {
			t.Fatalf("[%s] node count got %d, want %d", sc.label, len(nodes), len(sc.specs))
		}
		for i, n := range nodes {
			s := sc.specs[i]
			t.Logf("[%s] vmInfoList[%d]: nodeId=%s isCurrent=%v isMaster=%v podId=%q vmCount=%d",
				sc.label, i, n.NodeId, n.IsCurrent, n.IsMaster, n.PodId, len(n.VmInfos))
			if n.NodeId != s.NodeId {
				t.Errorf("[%s] node[%d] id got %q, want %q", sc.label, i, n.NodeId, s.NodeId)
			}
			if n.IsCurrent != s.IsCurrent {
				t.Errorf("[%s] node[%d] isCurrent got %v, want %v", sc.label, i, n.IsCurrent, s.IsCurrent)
			}
			if n.IsMaster != s.IsMaster {
				t.Errorf("[%s] node[%d] isMaster got %v, want %v", sc.label, i, n.IsMaster, s.IsMaster)
			}
			if n.PodId != s.PodId {
				t.Errorf("[%s] node[%d] podId got %q, want %q", sc.label, i, n.PodId, s.PodId)
			}
			if len(n.VmInfos) != len(s.Vms) {
				t.Errorf("[%s] node[%d] vm count got %d, want %d", sc.label, i, len(n.VmInfos), len(s.Vms))
				continue
			}
			for j, vm := range n.VmInfos {
				vs := s.Vms[j]
				t.Logf("[%s]   vmInfoList[%d].vmInfos[%d]: nodeId=%s uuid=%s name=%s state=%s hostName=%s vmCreateTime=%d maxMem=%d pid=%d numaCount=%d",
					sc.label, i, j, vm.MetaData.NodeId, vm.MetaData.Uuid, vm.MetaData.Name, vm.MetaData.State,
					vm.MetaData.HostName, vm.MetaData.VmCreateTime, vm.MetaData.MaxMem, vm.MetaData.Pid, len(vm.NumaInfo))
				if vm.MetaData.NodeId != s.NodeId {
					t.Errorf("[%s] node[%d] vm[%d] nodeId got %q, want %q", sc.label, i, j, vm.MetaData.NodeId, s.NodeId)
				}
				if vm.MetaData.Uuid != vs.Uuid {
					t.Errorf("[%s] node[%d] vm[%d] uuid got %q, want %q", sc.label, i, j, vm.MetaData.Uuid, vs.Uuid)
				}
				if vm.MetaData.Name != vs.Name {
					t.Errorf("[%s] node[%d] vm[%d] name got %q, want %q", sc.label, i, j, vm.MetaData.Name, vs.Name)
				}
				if vm.MetaData.State != vs.State {
					t.Errorf("[%s] node[%d] vm[%d] state got %q, want %q", sc.label, i, j, vm.MetaData.State, vs.State)
				}
				if vm.MetaData.HostName != vs.HostName {
					t.Errorf("[%s] node[%d] vm[%d] hostName got %q, want %q", sc.label, i, j, vm.MetaData.HostName, vs.HostName)
				}
				if vm.MetaData.VmCreateTime != vs.VmCreateTime {
					t.Errorf("[%s] node[%d] vm[%d] vmCreateTime got %d, want %d",
						sc.label, i, j, vm.MetaData.VmCreateTime, vs.VmCreateTime)
				}
				if vm.MetaData.MaxMem != vs.MaxMem {
					t.Errorf("[%s] node[%d] vm[%d] maxMem got %d, want %d", sc.label, i, j, vm.MetaData.MaxMem, vs.MaxMem)
				}
				if vm.MetaData.Pid != vs.Pid {
					t.Errorf("[%s] node[%d] vm[%d] pid got %d, want %d", sc.label, i, j, vm.MetaData.Pid, vs.Pid)
				}
				if len(vm.NumaInfo) != len(vs.Numas) {
					t.Errorf("[%s] node[%d] vm[%d] numa count got %d, want %d",
						sc.label, i, j, len(vm.NumaInfo), len(vs.Numas))
					continue
				}
				for k, vn := range vm.NumaInfo {
					vns := vs.Numas[k]
					t.Logf("[%s]     vmInfoList[%d].vmInfos[%d].numaInfo[%d]: numaId=%d socketId=%d isLocal=%v pageSize=%d usedMem=%d",
						sc.label, i, j, k, vn.NumaId, vn.SocketId, vn.IsLocal, vn.PageSize, vn.UsedMem)
					if vn.NumaId != vns.NumaId {
						t.Errorf("[%s] node[%d] vm[%d] numa[%d] numaId got %d, want %d",
							sc.label, i, j, k, vn.NumaId, vns.NumaId)
					}
					if vn.SocketId != vns.SocketId {
						t.Errorf("[%s] node[%d] vm[%d] numa[%d] socketId got %d, want %d",
							sc.label, i, j, k, vn.SocketId, vns.SocketId)
					}
					if vn.IsLocal != vns.IsLocal {
						t.Errorf("[%s] node[%d] vm[%d] numa[%d] isLocal got %v, want %v",
							sc.label, i, j, k, vn.IsLocal, vns.IsLocal)
					}
					if vn.PageSize != vns.PageSize {
						t.Errorf("[%s] node[%d] vm[%d] numa[%d] pageSize got %d, want %d",
							sc.label, i, j, k, vn.PageSize, vns.PageSize)
					}
					if vn.UsedMem != vns.UsedMem {
						t.Errorf("[%s] node[%d] vm[%d] numa[%d] usedMem got %d, want %d",
							sc.label, i, j, k, vn.UsedMem, vns.UsedMem)
					}
				}
			}
		}
	}

	empty, err := vmInfoListConvertForTest(nil)
	t.Logf("vmInfoListEmpty: len=%d err=%v", len(empty), err)
	if err != nil || len(empty) != 0 {
		t.Errorf("empty list got len=%d err=%v, want len=0 nil", len(empty), err)
	}

	if _, err := vmInfoListOverCountForTest(uint32(maxNodeNum + 1)); err == nil {
		t.Error("over-limit node count should fail")
	} else {
		t.Logf("vmInfoListOverCount: maxNodeNum=%d err=%v", maxNodeNum, err)
	}
}

// TestFreeNodeInfoListC verifies C-side memory release of the three-level structure (empty lists and nil pointers must not panic)
func TestFreeNodeInfoListC(t *testing.T) {
	// An empty list must not panic
	freeNodeInfoListForTest(true)
	t.Logf("nodeInfoListFree: emptyList=ok")

	nodesNil, nodeLen := freeNodeInfoListForTest(false)
	t.Logf("nodeInfoListFree: nodesNil=%v nodeLen=%d", nodesNil, nodeLen)
	if !nodesNil {
		t.Error("node_infos should be nil after free")
	}
	if nodeLen != 0 {
		t.Error("node_len should be 0 after free")
	}
}

// TestAsyncTaskStatusValues verifies the enum values match the C-side async_task_status_c
func TestAsyncTaskStatusValues(t *testing.T) {
	statuses := []AsyncTaskStatus{AsyncTaskNotExist, AsyncTaskRunning, AsyncTaskSuccess, AsyncTaskFailed}
	for i, s := range statuses {
		t.Logf("asyncTaskStatus[%d]=%d", i, int32(s))
		if int32(s) != int32(i) {
			t.Errorf("status[%d] got %d", i, s)
		}
	}
}

// TestUbsSyncTaskQueryInvalidTaskId verifies task-ID length validation (performed before dlopen, so no dynamic library is needed)
func TestUbsSyncTaskQueryInvalidTaskId(t *testing.T) {
	if _, err := UbsSyncTaskQuery(""); err == nil {
		t.Error("empty task id should fail")
	} else {
		t.Logf("taskQueryEmpty: err=%v", err)
	}
	long := strings.Repeat("t", maxTaskIdLen)
	if _, err := UbsSyncTaskQuery(long); err == nil {
		t.Error("overlong task id should fail")
	} else {
		t.Logf("taskQueryOverlong: len=%d maxTaskIdLen=%d err=%v", len(long), maxTaskIdLen, err)
	}
}

// TestConvertVmDomainInfoC verifies conversion from C vm_domain_info_t to the Go struct (including the NUMA array and nil-pointer branches)
func TestConvertVmDomainInfoC(t *testing.T) {
	vm := vmDomainInfoConvertForTest([]VmNumaInfo{
		{NumaId: 0, SocketId: 1, IsLocal: true, PageSize: 2048, UsedMem: 102400},
		{NumaId: 1, SocketId: 1, IsLocal: false, PageSize: 2048, UsedMem: 51200},
	})
	t.Logf("vmDomainInfo: nodeId=%s hostName=%s uuid=%s name=%s state=%s timestamp=%d vmCreateTime=%d maxMem=%d pid=%d numaCount=%d",
		vm.MetaData.NodeId, vm.MetaData.HostName, vm.MetaData.Uuid, vm.MetaData.Name, vm.MetaData.State,
		vm.Timestamp, vm.MetaData.VmCreateTime, vm.MetaData.MaxMem, vm.MetaData.Pid, len(vm.NumaInfo))
	for i, n := range vm.NumaInfo {
		t.Logf("  vmNuma[%d]: numaId=%d socketId=%d isLocal=%v pageSize=%d usedMem=%d",
			i, n.NumaId, n.SocketId, n.IsLocal, n.PageSize, n.UsedMem)
	}
	if vm.Timestamp != 1700000000 {
		t.Errorf("timestamp got %d, want 1700000000", vm.Timestamp)
	}
	md := vm.MetaData
	if md.NodeId != "node-01" || md.HostName != "host-01" || md.Name != "vm-001" || md.State != "running" {
		t.Errorf("meta string fields mismatch: %+v", md)
	}
	if md.Uuid != "11111111-2222-3333-4444-555555555555" {
		t.Errorf("uuid got %q", md.Uuid)
	}
	if md.VmCreateTime != 1699999999 || md.MaxMem != 8388608 || md.Pid != 12345 {
		t.Errorf("meta numeric fields mismatch: %+v", md)
	}
	if len(vm.NumaInfo) != 2 {
		t.Fatalf("numa count got %d, want 2", len(vm.NumaInfo))
	}
	want0 := VmNumaInfo{NumaId: 0, SocketId: 1, IsLocal: true, PageSize: 2048, UsedMem: 102400}
	want1 := VmNumaInfo{NumaId: 1, SocketId: 1, IsLocal: false, PageSize: 2048, UsedMem: 51200}
	if vm.NumaInfo[0] != want0 {
		t.Errorf("numa[0] mismatch: %+v, want %+v", vm.NumaInfo[0], want0)
	}
	if vm.NumaInfo[1] != want1 {
		t.Errorf("numa[1] mismatch: %+v, want %+v", vm.NumaInfo[1], want1)
	}

	// A nil NUMA pointer must not panic and yields no NUMA data
	if vm2 := vmDomainInfoConvertForTest(nil); len(vm2.NumaInfo) != 0 {
		t.Errorf("nil numa info got %d, want 0", len(vm2.NumaInfo))
	}
}

// TestFreeVmInfoListC verifies C-side memory release of the three-level structure (empty lists and nil pointers must not panic)
func TestFreeVmInfoListC(t *testing.T) {
	// An empty list must not panic
	freeVmInfoListForTest(true)
	t.Logf("vmInfoListFree: emptyList=ok")

	nodesNil, nodeLen := freeVmInfoListForTest(false)
	t.Logf("vmInfoListFree: nodesNil=%v nodeLen=%d", nodesNil, nodeLen)
	if !nodesNil {
		t.Error("vm_node_infos should be nil after free")
	}
	if nodeLen != 0 {
		t.Error("node_len should be 0 after free")
	}
}
