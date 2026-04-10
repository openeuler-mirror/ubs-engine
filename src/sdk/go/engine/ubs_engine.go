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

// package engine 实现了engine的go sdk.
package engine

/*
#include <stdlib.h>

// 定义函数指针类型
typedef int (*ubs_engine_client_initialize_ptr)(const char*);
typedef void (*ubs_engine_client_finalize_ptr)();

// C桥接函数
int call_ubs_engine_client_initialize(ubs_engine_client_initialize_ptr fn, const char* path) {
	if (fn == NULL) return -1;
	return fn(path);
}

void call_ubs_engine_client_finalize(ubs_engine_client_finalize_ptr fn) {
	if (fn != NULL) {
		fn();
	}
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/dlopen" // 替换为实际的模块路径
)

// UbsEngineClientInitialize 初始化ubse客户端
func UbsEngineClientInitialize(ubsEngineUdsPath string) (int32, error) {
	// 加载动态库
	dlopenRet := dlopen.GetLibHandle()
	if dlopenRet != nil {
		return -1, dlopenRet
	}

	// 加载函数符号
	initializePtr, err := dlopen.LoadFunc("ubs_engine_client_initialize")
	if err != nil {
		dlopen.CloseLibHandle()
		return -1, err
	}

	// 转换为函数指针类型
	initializeFunc := (C.ubs_engine_client_initialize_ptr)(initializePtr)

	// 准备参数
	var cPath *C.char
	cPath = C.CString(ubsEngineUdsPath)
	defer C.free(unsafe.Pointer(cPath))

	// 调用函数
	result := C.call_ubs_engine_client_initialize(initializeFunc, cPath)
	if result != 0 {
		dlopen.CloseLibHandle() // 调用失败时关闭库
		return int32(result), fmt.Errorf("ubs_engine_client_initialize failed with error code: %d", result)
	}

	return int32(result), nil
}

// UbsEngineClientFinalize 销毁ubse客户端
func UbsEngineClientFinalize() error {
	// 加载函数符号
	finalizePtr, err := dlopen.LoadFunc("ubs_engine_client_finalize")
	if err != nil {
		dlopen.CloseLibHandle()
		return err
	}

	// 转换为函数指针类型
	finalizeFunc := (C.ubs_engine_client_finalize_ptr)(finalizePtr)

	// 调用函数
	C.call_ubs_engine_client_finalize(finalizeFunc)

	// 关闭动态库
	dlopen.CloseLibHandle()
	return nil
}
