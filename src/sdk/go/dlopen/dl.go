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

// package dlopen 运行时加载动态库包
package dlopen

/*
#include <dlfcn.h>
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

var soPath = "/usr/lib64/libubse-client.so.1"
var libHandle unsafe.Pointer

// GetLibHandle 获取动态库句柄
func GetLibHandle() error {
	cLibPath := C.CString(soPath)
	defer C.free(unsafe.Pointer(cLibPath))
	// 指定so路径进行加载
	libHandle = C.dlopen(cLibPath, C.RTLD_LAZY)
	if libHandle == nil {
		return fmt.Errorf("failed to open so file")
	}
	return nil
}

// LoadFunc 加载动态库中的函数
func LoadFunc(funcName string) (unsafe.Pointer, error) {
	if libHandle == nil {
		return nil, fmt.Errorf("dlopen handle is null")
	}
	// 符号为所要调用的函数名
	symbol := C.CString(funcName)
	defer C.free(unsafe.Pointer(symbol))
	// 根据符号获取到对应地址的指针
	fn := C.dlsym(libHandle, symbol)

	if fn == nil {
		return nil, fmt.Errorf("failed to load func %s", funcName)
	}
	return fn, nil
}

// CloseLibHandle 释放函数句柄
func CloseLibHandle() {
	cLibPath := C.CString(soPath)
	defer C.free(unsafe.Pointer(cLibPath))
	C.dlclose(libHandle)
}
