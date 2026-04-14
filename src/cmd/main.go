package main

import (
	"fmt"

	urmav1 "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/urmav1"
)

func main() {
	fmt.Println("Testing URMA functions...")

	// Test UbsGetVfeDevice
	devices, err := urmav1.UbsGetVfeDevice()
	if err != nil {
		fmt.Printf("UbsGetVfeDevice failed: %v\n", err)
	} else {
		fmt.Printf("Found %d VFE devices\n", len(devices))
		for _, dev := range devices {
			fmt.Printf("Device: %s, Healthy: %v, HwResId: %d\n", dev.Name, dev.Healthy, dev.HwResId)
		}
	}

	// Test UbsAllocateDevice
	info, err := urmav1.UbsAllocateDevice("test-device")
	if err != nil {
		fmt.Printf("UbsAllocateDevice failed: %v\n", err)
	} else {
		fmt.Printf("Allocated device:\n")
		fmt.Printf("BondingPath: %s\n", info.BondingPath)
		fmt.Printf("BondingEid: %s\n", info.BondingEid)
		fmt.Printf("VfePaths: %v\n", info.VfePaths)
	}
}
