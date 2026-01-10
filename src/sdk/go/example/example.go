// Package main
package main

import (
	"fmt"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/engine"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/urma"
)

func main() {
	_, connectError := engine.UbsEngineClientInitialize("")
	if connectError != nil {
		fmt.Printf("connectError: %s\n", connectError.Error())
		return
	}
	defer engine.UbsEngineClientFinalize()
	uniqueDevices, err := urma.UbsGetVfeDevice()
	if err != nil {
		fmt.Printf("UbsGetVfeDevice Error: %s\n", err.Error())
		return
	}
	fmt.Printf("uniqueDevices: %v\n", uniqueDevices)

	if len(uniqueDevices) == 0 {
		fmt.Println("No unique device found")
		return
	}

	uniqueDevicePaths, err := urma.UbsAllocateDevice(uniqueDevices[0].Name)
	if err != nil {
		fmt.Printf("Error allocating device1: %s, err: %s", uniqueDevices[0].Name, err)
		return
	}
	fmt.Printf("uniqueDevicePaths: %v\n", uniqueDevicePaths)

	shareDevices, err := urma.UbsGetSharedDevice()
	if err != nil {
		fmt.Printf("Error getting shared device: %s", err)
		return
	}
	fmt.Printf("shareDevices: %v\n", shareDevices)

	if len(shareDevices) == 0 {
		fmt.Println("No share device found")
		return
	}

	shareDevicePaths, err := urma.UbsAllocateDevice(shareDevices[0].Name)
	if err != nil {
		fmt.Printf("Error allocating device2: %s, err: %s", shareDevices[0].Name, err)
		return
	}
	fmt.Printf("shareDevicePaths: %v\n", shareDevicePaths)
	return
}
