/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include <sched.h>
#include <sys/mount.h>
#include <sys/personality.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void BindMount(const std::string& source, const std::string& target)
{
    std::filesystem::create_directories(target);
    if (mount(source.c_str(), target.c_str(), nullptr, MS_BIND, nullptr) != 0) {
        std::cerr << "[WARN] bind mount " << target << " failed, errno=" << errno << std::endl;
    }
}

void SetupMountNamespace(const std::string& workDir)
{
    if (unshare(CLONE_NEWNS) != 0) {
        std::cerr << "[WARN] unshare(CLONE_NEWNS) failed, errno=" << errno << std::endl;
        return;
    }
    if (mount("none", "/", nullptr, MS_PRIVATE | MS_REC, nullptr) != 0) {
        std::cerr << "[WARN] making mount namespace private failed, errno=" << errno << std::endl;
    }
    BindMount(workDir + "/run", "/var/run/ubse");
    BindMount(workDir + "/log", "/var/log/ubse");
    BindMount(workDir + "/run/ubm/socket/ubm_nuds", "/run/ubm/socket/ubm_nuds");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 5) {
        std::cerr << "usage: ubse_it_node_launcher <binary> <work-dir> <privileged> <stub-lib-dir>" << std::endl;
        return 2;
    }

    const std::string binaryPath = argv[1];
    const std::string workDir = argv[2];
    if (std::string(argv[3]) == "1") {
        SetupMountNamespace(workDir);
    }
    const std::string stubLibDir = argv[4];
    if (!stubLibDir.empty()) {
        setenv("LD_LIBRARY_PATH", stubLibDir.c_str(), 1);
        // 注意：不要把 libasan 放进 LD_PRELOAD。preload 的 libasan 会在 ld.so 完成
        // 可执行文件依赖初始化之前运行 interceptor init（内部 dlsym 重入 loader），
        // glibc 2.38 下随机 SEGV。daemon 本身已通过 DT_NEEDED 链接 libasan，由 ld.so
        // 在正确时机初始化，与 UT 二进制一致，稳定。stub 在 libasan 之前加载的
        // link-order 差异由 ASAN_OPTIONS 的 verify_asan_link_order=0 兜底（build.sh 已设置）。
        const std::string preloadPath = stubLibDir + "/libubse_interface_preload.so";
        setenv("LD_PRELOAD", preloadPath.c_str(), 1);
        // 新内核（6.x，如 WSL2）默认 vm.mmap_rnd_bits=32，高熵 ASLR 与 gcc 12 的
        // libasan 不兼容：main 之前 ASAN 初始化阶段随机 SEGV（体积越大的二进制
        // 概率越高，daemon 几乎必现）。ASAN 模式下禁用 daemon 进程 ASLR 规避；
        // personality 标志跨 fork/exec 继承，daemon 子进程同样生效。
        const char* asanLibasan = getenv("UBSE_ASAN_LIBASAN");
        if (asanLibasan != nullptr && asanLibasan[0] != '\0') {
            personality(ADDR_NO_RANDOMIZE);
        }
    }
    if (chdir(workDir.c_str()) != 0) {
        std::cerr << "failed to chdir to " << workDir << ", errno=" << errno << std::endl;
        return 3;
    }
    execl(binaryPath.c_str(), binaryPath.c_str(), static_cast<char*>(nullptr));
    std::cerr << "failed to exec " << binaryPath << ", errno=" << errno << std::endl;
    return 4;
}
