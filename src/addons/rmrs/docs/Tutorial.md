# Tutorial

## Best Practices

UBS RMRS 的英文全名为 UB Server Core RackMemoryResourceSchedule，是用于集群内存资源调度的组件。本项目为UBS Engine 内rmrs插件。
虚机碎片场景，BS Engine 内各计算节点 Agent 端仅负责数据采集和消息转发；主节点侧作为功能入口，提供碎片内存借用、迁出、归还和回滚接口，负责节点间碎片内存管理。
虚机超分场景，MemLink动态回收虚机的空余内存或补充内存。在numa内存触发高水位水线时，触发内存借用，并迁出虚机内存，缓解内存压力。在numa内存触发低水位水线时，归还借用内存。
UBS RMRS提供内存借用、虚机迁移接口，提供虚机迁移算法。OS Turbo提供资源采集能力，供迁出算法决策。
容器超分场景，支持容器绑定单numa和不绑定numa的场景，容器绑定单numa时，触发numa水线告警，发起numa内存借用。容器不绑定numa时，触发节点水线告警，发起节点内存借用。
Demo
使用rmrs插件的一个典型场景是：通过dlopen调用rmrs对外提供的接口。
 
~~~cpp
# demo_mempooling.cpp
# demo代码说明：
# 前置条件：调用rmrs对外接口方是ubse的一个插件（插件具体写法参考ubse框架说明），插件中通过dlopen形式调用rmrs插件对外接口
# 环境要求：已安装UBS Engine、UBS-rmrs、ubturbo-rmrs、ubturbo-smap、libboundscheck
# 调用示例编写参考如下：
 
#include <dlfcn.h>
#include <cstdint>
#include <iostream>
#include <string>
 
using UBSRMRSSetRunModeFunc = uint32_t (*)(const int &);
UBSRMRSSetRunModeFunc ubsRMRSSetRunModeFunc_ = nullptr;
void *libmempoolingHandler_ = nullptr;
constexpr int OVER_COMMITMENT_MODE = 0;
constexpr int MEM_FRAGMENTATION_MODE = 1;
 
uint32_t Init()
{
    const std::string libmempoolingPath = "/usr/lib64/libmempooling.so";
    libmempoolingHandler_ = dlopen(libmempoolingPath.c_str(), RTLD_LAZY);
    if (libmempoolingHandler_ == nullptr) {
        std::cerr << "load libmempooling.so failed, " << dlerror();
        return 1;
    }
    return 0;
}
 
UBSRMRSSetRunModeFunc UBSRMRSSetRunMode()
{
    if (ubsRMRSSetRunModeFunc_ != nullptr) {
        return ubsRMRSSetRunModeFunc_;
    }
    ubsRMRSSetRunModeFunc_ = reinterpret_cast<UBSRMRSSetRunModeFunc>(dlsym(libmempoolingHandler_, "UBSRMRSSetRunMode"));
    if (ubsRMRSSetRunModeFunc_ == nullptr) {
        std::cerr << "Get UBSRMRSSetRunMode ptr failed, " << dlerror();
        return nullptr;
    }
    return ubsRMRSSetRunModeFunc_;
}
 
int main(int argc, char* argv[])
{
    auto res = Init();
    if (res == 1) {
        std::cerr << "Init failed.";
        return 1;
    }
 
    auto UBSRMRSSetRunModeFunc = UBSRMRSSetRunMode();
    if (UBSRMRSSetRunModeFunc == nullptr) {
        std::cerr << "Get UBSRMRSSetRunModeFunc ptr failed.";
        return 1;
    }
 
    res = UBSRMRSSetRunModeFunc(MEM_FRAGMENTATION_MODE);
    std::cerr << "The result of SetRunMode = " << res;
    return 0;
}
```
