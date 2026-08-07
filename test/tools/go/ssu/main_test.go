package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"reflect"
	"strings"
	"testing"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
)

// TestHelpListsAllSdkCommands 验证帮助输出完整列出 16 条 SDK 命令。
func TestHelpListsAllSdkCommands(t *testing.T) {
	var output bytes.Buffer

	err := runOnce([]string{"help"}, &output, sdkFuncs{})

	if err != nil {
		t.Fatalf("help failed: %v", err)
	}
	for _, name := range []string{
		"ssu_list_alloc_info",
		"ssu_get_ns_stats",
		"ssu_get_connect_info",
		"ssu_get_fe_device_list",
		"ssu_alloc_space",
		"ssu_free_space",
		"ssu_add_access_permission",
		"ssu_remove_access_permission",
		"ssu_attach_space",
		"ssu_detach_space",
		"ssu_attach_linear_space",
		"ssu_detach_linear_space",
		"ssu_attach_striped_space",
		"ssu_detach_striped_space",
		"ssu_fe_device_alloc",
		"ssu_fe_device_free",
	} {
		if !strings.Contains(output.String(), name) {
			t.Errorf("help is missing command %q", name)
		}
	}
}

// TestResponseFormats 验证单值、列表、多返回值、空值和仅 error 返回值的精确输出。
func TestResponseFormats(t *testing.T) {
	deps := sdkFuncs{
		listAllocInfo: func() ([]ssu.UbsSsuAllocResult, error) { return nil, nil },
		getNsStats:    func(string) ([]ssu.UbsSsuNsStats, error) { return []ssu.UbsSsuNsStats{}, nil },
		getConnectInfo: func(string, *ssu.UbsUbVfe) ([]ssu.UbsSsuConnectInfo, error) {
			return []ssu.UbsSsuConnectInfo{{SrcEid: "source eid"}}, nil
		},
		allocSpace: func(ssu.UbsSsuAllocSpaceReq) (ssu.UbsSsuAllocResult, error) { return ssu.UbsSsuAllocResult{}, nil },
		freeSpace:  func(string) error { return nil },
		attachLinearSpace: func(ssu.UbsSsuLinearSpaceReq) ([]string, string, error) {
			return []string{"dev-a", "dev-b"}, "aggregate path", nil
		},
		feDeviceAlloc: func(uint32, *ssu.UbsUbVfe, string) (string, error) { return "", nil },
	}
	tests := []struct {
		name string
		args []string
		want string
	}{
		{"nil list", []string{"ssu_list_alloc_info"}, "{\"response\":null}\n"},
		{"empty list", []string{"ssu_get_ns_stats", "name"}, "{\"response\":[]}\n"},
		{"structured list", []string{"ssu_get_connect_info", "name", "0", "0", "0", "0", "0", "0", "", ""}, "{\"response\":[{\"SrcEid\":\"source eid\",\"TgtEid\":\"\",\"TgtNqn\":\"\",\"HostNqn\":\"\",\"NsUuid\":\"\",\"NsId\":0}]}\n"},
		{"empty string", []string{"ssu_fe_device_alloc", "0", "0", "0", "0", "0", "0", "", "", ""}, "{\"response\":\"\"}\n"},
		{"zero value", []string{"ssu_alloc_space", "", "0", "0", "0", "0", ""}, "{\"response\":{\"Name\":\"\",\"Strategy\":0,\"Namespaces\":null}}\n"},
		{"multiple values", []string{"ssu_attach_linear_space", "", "", "", ""}, "{\"response\":[[\"dev-a\",\"dev-b\"],\"aggregate path\"]}\n"},
		{"error only", []string{"ssu_free_space", ""}, "{\"response\":\"success\"}\n"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			var output bytes.Buffer
			if err := runOnce(test.args, &output, deps); err != nil {
				t.Fatalf("command failed: %v", err)
			}
			if output.String() != test.want {
				t.Fatalf("response %q, want %q", output.String(), test.want)
			}
		})
	}
}

// TestEveryCommandCallsMatchingSdkFunction 验证每条 CLI 命令只调用同名语义的 SDK 函数。
func TestEveryCommandCallsMatchingSdkFunction(t *testing.T) {
	cases := []struct {
		name string
		args []string
	}{
		{"ssu_list_alloc_info", nil},
		{"ssu_get_ns_stats", []string{"name"}},
		{"ssu_get_connect_info", []string{"name", "0", "0", "0", "0", "0", "0", "", ""}},
		{"ssu_get_fe_device_list", nil},
		{"ssu_alloc_space", []string{"name", "1024", "2", "4096", "9", "tenant"}},
		{"ssu_free_space", []string{"name"}},
		{"ssu_add_access_permission", []string{"name", "nqn"}},
		{"ssu_remove_access_permission", []string{"name", "nqn"}},
		{"ssu_attach_space", []string{"name", "nqn", "eid"}},
		{"ssu_detach_space", []string{"name", "nqn", "eid"}},
		{"ssu_attach_linear_space", []string{"name", "nqn", "eid", "linear"}},
		{"ssu_detach_linear_space", []string{"name", "nqn", "eid", "linear"}},
		{"ssu_attach_striped_space", []string{"name", "nqn", "eid", "striped", "7", "13"}},
		{"ssu_detach_striped_space", []string{"name", "nqn", "eid", "striped", "7", "13"}},
		{"ssu_fe_device_alloc", []string{"1", "2", "3", "4", "5", "6", "vfe", "bind", "bus"}},
		{"ssu_fe_device_free", []string{"1", "2", "3", "4", "5", "6", "vfe", "bind"}},
	}
	for _, test := range cases {
		t.Run(test.name, func(t *testing.T) {
			called := ""
			deps := recordingSDK(&called)

			err := runOnce(append([]string{test.name}, test.args...), io.Discard, deps)

			if err != nil {
				t.Fatalf("command failed: %v", err)
			}
			if called != test.name {
				t.Fatalf("called %q, want %q", called, test.name)
			}
		})
	}
}

func recordingSDK(called *string) sdkFuncs {
	mark := func(name string) {
		*called = name
	}
	return sdkFuncs{
		listAllocInfo: func() ([]ssu.UbsSsuAllocResult, error) {
			mark("ssu_list_alloc_info")
			return nil, nil
		},
		getNsStats: func(string) ([]ssu.UbsSsuNsStats, error) {
			mark("ssu_get_ns_stats")
			return nil, nil
		},
		getConnectInfo: func(string, *ssu.UbsUbVfe) ([]ssu.UbsSsuConnectInfo, error) {
			mark("ssu_get_connect_info")
			return nil, nil
		},
		getFeDeviceList: func() ([]ssu.UbsSsuFe, error) {
			mark("ssu_get_fe_device_list")
			return nil, nil
		},
		allocSpace: func(ssu.UbsSsuAllocSpaceReq) (ssu.UbsSsuAllocResult, error) {
			mark("ssu_alloc_space")
			return ssu.UbsSsuAllocResult{}, nil
		},
		freeSpace: func(string) error {
			mark("ssu_free_space")
			return nil
		},
		addAccessPermission: func(string, string) error {
			mark("ssu_add_access_permission")
			return nil
		},
		removeAccessPermission: func(string, string) error {
			mark("ssu_remove_access_permission")
			return nil
		},
		attachSpace: func(ssu.UbsSsuSpaceReq) ([]string, error) {
			mark("ssu_attach_space")
			return nil, nil
		},
		detachSpace: func(ssu.UbsSsuSpaceReq) error {
			mark("ssu_detach_space")
			return nil
		},
		attachLinearSpace: func(ssu.UbsSsuLinearSpaceReq) ([]string, string, error) {
			mark("ssu_attach_linear_space")
			return nil, "", nil
		},
		detachLinearSpace: func(ssu.UbsSsuLinearSpaceReq) error {
			mark("ssu_detach_linear_space")
			return nil
		},
		attachStripedSpace: func(ssu.UbsSsuStripedSpaceReq) ([]string, string, error) {
			mark("ssu_attach_striped_space")
			return nil, "", nil
		},
		detachStripedSpace: func(ssu.UbsSsuStripedSpaceReq) error {
			mark("ssu_detach_striped_space")
			return nil
		},
		feDeviceAlloc: func(uint32, *ssu.UbsUbVfe, string) (string, error) {
			mark("ssu_fe_device_alloc")
			return "", nil
		},
		feDeviceFree: func(uint32, *ssu.UbsUbVfe) error {
			mark("ssu_fe_device_free")
			return nil
		},
	}
}

// TestArgumentsArePassedWithoutBusinessConversion 验证字符串、整数、枚举和 VFE 参数按输入透传。
func TestArgumentsArePassedWithoutBusinessConversion(t *testing.T) {
	t.Run("allocation", func(t *testing.T) {
		var got ssu.UbsSsuAllocSpaceReq
		deps := recordingSDK(new(string))
		deps.allocSpace = func(req ssu.UbsSsuAllocSpaceReq) (ssu.UbsSsuAllocResult, error) {
			got = req
			return ssu.UbsSsuAllocResult{}, nil
		}
		err := runOnce(strings.Fields("ssu_alloc_space name 18446744073709551615 4294967295 17 9 tenant"), io.Discard, deps)
		if err != nil {
			t.Fatalf("allocation arguments failed: %v", err)
		}
		want := ssu.UbsSsuAllocSpaceReq{
			Name: "name", NsSize: ^uint64(0), NsNum: ^uint32(0),
			LbaFormat: ssu.UbsSsuLbaFormat(17), Strategy: ssu.UbsSsuAllocStrategy(9), Tenant: "tenant",
		}
		if !reflect.DeepEqual(got, want) {
			t.Fatalf("request %+v, want %+v", got, want)
		}
	})

	t.Run("vfe pointer", func(t *testing.T) {
		var got []*ssu.UbsUbVfe
		deps := recordingSDK(new(string))
		deps.getConnectInfo = func(_ string, vfe *ssu.UbsUbVfe) ([]ssu.UbsSsuConnectInfo, error) {
			got = append(got, vfe)
			return nil, nil
		}
		for _, present := range []string{"0", "1"} {
			args := []string{"ssu_get_connect_info", "name", present, "0", "0", "0", "0", "0", "", ""}
			if err := runOnce(args, io.Discard, deps); err != nil {
				t.Fatalf("VFE arguments failed: %v", err)
			}
		}
		if got[0] != nil || got[1] == nil || *got[1] != (ssu.UbsUbVfe{}) {
			t.Fatalf("nil and non-nil zero-value VFE were not distinguished: %+v", got)
		}
	})

	t.Run("striped values", func(t *testing.T) {
		var got ssu.UbsSsuStripedSpaceReq
		deps := recordingSDK(new(string))
		deps.detachStripedSpace = func(req ssu.UbsSsuStripedSpaceReq) error {
			got = req
			return nil
		}
		args := strings.Fields("ssu_detach_striped_space name nqn eid dev 7 13")
		if err := runOnce(args, io.Discard, deps); err != nil {
			t.Fatalf("striped arguments failed: %v", err)
		}
		if got.Level != 7 || got.ChunkSize != 13 {
			t.Fatalf("enum values were changed: %+v", got)
		}
	})
}

// TestInvalidInputDoesNotCallSDK 验证参数数量或类型错误会在 SDK 调用前被拒绝。
func TestInvalidInputDoesNotCallSDK(t *testing.T) {
	calls := 0
	deps := recordingSDK(new(string))
	deps.allocSpace = func(ssu.UbsSsuAllocSpaceReq) (ssu.UbsSsuAllocResult, error) {
		calls++
		return ssu.UbsSsuAllocResult{}, nil
	}
	cases := [][]string{
		{"ssu_alloc_space", "name"},
		{"ssu_alloc_space", "name", "1", "2", "4096", "linear", "tenant"},
		{"ssu_alloc_space", "name", "-1", "2", "4096", "0", "tenant"},
		{"ssu_alloc_space", "name", "1", "4294967296", "4096", "0", "tenant"},
	}
	for _, args := range cases {
		if err := runOnce(args, io.Discard, deps); err == nil {
			t.Errorf("invalid arguments did not fail: %v", args)
		}
	}
	if calls != 0 {
		t.Fatalf("invalid arguments called the SDK %d times", calls)
	}
}

// TestResponseErrorAndInteractiveRecovery 验证 SDK 错误透传、错误输出分流和交互模式错误恢复。
func TestResponseErrorAndInteractiveRecovery(t *testing.T) {
	sentinel := errors.New("sdk sentinel")
	deps := recordingSDK(new(string))
	deps.freeSpace = func(string) error { return sentinel }
	var output bytes.Buffer

	err := runOnce([]string{"ssu_free_space", "name"}, &output, deps)

	if !errors.Is(err, sentinel) || output.Len() != 0 {
		t.Fatalf("SDK error was not returned unchanged: err=%v output=%q", err, output.String())
	}

	output.Reset()
	var errorOutput bytes.Buffer
	deps.freeSpace = func(string) error { return nil }
	input := "unknown\nssu_free_space\nssu_alloc_space name bad 0 0 0 tenant\nssu_free_space \"unterminated\nssu_free_space \"\"\nquit\n"
	if err := runInteractive(strings.NewReader(input), &output, &errorOutput, deps); err != nil {
		t.Fatalf("interactive execution failed: %v", err)
	}
	for _, text := range []string{"unknown command", "invalid argument count", "not a valid", "unterminated"} {
		if strings.Contains(output.String(), text) || !strings.Contains(errorOutput.String(), text) {
			t.Fatalf("interactive streams are wrong: output=%q error=%q", output.String(), errorOutput.String())
		}
	}
	errorResponses := bytes.Split(bytes.TrimSpace(errorOutput.Bytes()), []byte{'\n'})
	if len(errorResponses) != 4 {
		t.Fatalf("error response count %d, want 4", len(errorResponses))
	}
	for _, response := range errorResponses {
		if !json.Valid(response) {
			t.Fatalf("error response is not JSON: %q", response)
		}
	}
	if !strings.Contains(output.String(), "{\"response\":\"success\"}") {
		t.Fatalf("interactive streams are wrong: output=%q error=%q", output.String(), errorOutput.String())
	}
}

// TestSplitLineFollowsShellEscaping 验证拆词遵循 shell 引用与反斜线规则。
func TestSplitLineFollowsShellEscaping(t *testing.T) {
	cases := []struct {
		name, line, want string
	}{
		{"single quoted literals", `ssu_free_space 'space\\\"name'`, `space\\\"name`},
		{"double quoted non-special escape", `ssu_free_space "space\'name"`, `space\'name`},
		{"double quoted special escapes", "ssu_free_space \"a\\\"b\\\\c\\$d\\`e\"", "a\"b\\c$d`e"},
	}
	for _, test := range cases {
		t.Run(test.name, func(t *testing.T) {
			fields, err := splitLine(test.line)
			if err != nil || len(fields) != 2 || fields[1] != test.want {
				t.Fatalf("splitLine(%q) = %q, %v; want second field %q", test.line, fields, err, test.want)
			}
		})
	}
}
