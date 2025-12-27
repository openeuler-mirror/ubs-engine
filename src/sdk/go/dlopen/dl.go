// Package dlopen
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

func GetLibHandle() error {
	cLibPath := C.CString(soPath)
	defer C.free(unsafe.Pointer(cLibPath))
	libHandle = C.dlopen(cLibPath, C.RTLD_LAZY)
	if libHandle == nil {
		return fmt.Errorf("failed to open so file")
	}
	return nil
}

func LoadFunc(funcName string) (unsafe.Pointer, error) {
	if libHandle == nil {
		return nil, fmt.Errorf("dlopen handle is nil")
	}
	symbol := C.CString(funcName)
	defer C.free(unsafe.Pointer(symbol))
	fn := C.dlsym(libHandle, symbol)

	if fn == nil {
		return nil, fmt.Errorf("failed to load function %s", funcName)
	}
	return fn, nil
}

func CloseLibHandle() {
	cLibPath := C.CString(soPath)
	defer C.free(unsafe.Pointer(cLibPath))
	C.dlclose(libHandle)
}
