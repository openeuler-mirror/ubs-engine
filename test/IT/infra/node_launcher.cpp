/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include <sched.h>
#include <sys/mount.h>
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
        const std::string preloadPath = stubLibDir + "/libubse_interface_preload.so";
        setenv("LD_PRELOAD", preloadPath.c_str(), 1);
    }
    if (chdir(workDir.c_str()) != 0) {
        std::cerr << "failed to chdir to " << workDir << ", errno=" << errno << std::endl;
        return 3;
    }
    execl(binaryPath.c_str(), binaryPath.c_str(), static_cast<char*>(nullptr));
    std::cerr << "failed to exec " << binaryPath << ", errno=" << errno << std::endl;
    return 4;
}
