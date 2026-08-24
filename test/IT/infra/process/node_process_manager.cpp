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

#include "node_process_manager.h"

#include <signal.h>
#include <spawn.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_console_log.h"
#include "it_xalarm_helper.h"

extern char** environ;

namespace ubse::it::infra {

NodeProcessManager::NodeProcessManager(NodeProcessConfig config)
    : binaryPath_(std::move(config.binaryPath)),
      launcherPath_((std::filesystem::path(binaryPath_).parent_path() / "ubse_it_node_launcher").string()),
      nodeId_(std::move(config.nodeId)),
      nodeIp_(std::move(config.nodeIp)),
      workDir_(std::move(config.workDir)),
      slotId_(config.slotId),
      clusterNodeIds_(std::move(config.clusterNodeIds)),
      clusterIps_(std::move(config.clusterIps)),
      clusterSlotIds_(std::move(config.clusterSlotIds)),
      stubLibDir_(std::move(config.stubLibDir)),
      sceneType_(std::move(config.sceneType)),
      meshType_(config.meshType),
      lcneUdsPath_(std::move(config.lcneUdsPath)),
      certResourceDir_(std::move(config.certResourceDir)),
      certAuthorityDir_(std::move(config.certAuthorityDir)),
      childPid_(-1),
      udsSocketPath_(workDir_ + "/run/ubse.sock"),
      xalarmFifoPath_(workDir_ + "/run/xalarm_fifo")
{
}

NodeProcessManager::~NodeProcessManager()
{
    Stop();
}

std::vector<std::string> NodeProcessManager::BuildChildEnvironment() const
{
    std::vector<std::string> environment;
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        std::string value(*entry);
        if (value.rfind("LD_LIBRARY_PATH=", 0) == 0 || value.rfind("LD_PRELOAD=", 0) == 0 ||
            value.rfind("UBSE_IT_", 0) == 0 || value.rfind("UBSE_UDS_ADDRESS=", 0) == 0 ||
            value.rfind(ubse::common::def::UBSE_HCOM_FILE_PATH_PREFIX + "=", 0) == 0) {
            continue;
        }
        environment.push_back(std::move(value));
    }

    environment.emplace_back("UBSE_IT_NODE_ID=" + nodeId_);
    environment.emplace_back("UBSE_IT_NODE_IP=" + nodeIp_);
    environment.emplace_back("UBSE_IT_LOCAL_IP=" + nodeIp_);
    environment.emplace_back("UBSE_IT_CONF_DIR=" + workDir_);
    environment.emplace_back("UBSE_IT_SLOT_ID=" + std::to_string(slotId_));
    environment.emplace_back("UBSE_IT_CLUSTER_NODES=" + clusterNodeIds_);
    environment.emplace_back("UBSE_IT_CLUSTER_IPS=" + clusterIps_);
    environment.emplace_back("UBSE_IT_UDS_SOCKET_PATH=" + udsSocketPath_);
    environment.emplace_back("UBSE_UDS_ADDRESS=" + udsSocketPath_);
    environment.emplace_back("UBSE_IT_LCNE_UDS_PATH=" + lcneUdsPath_);
    environment.emplace_back("UBSE_IT_LOG_PATH=" + workDir_ + "/log");
    environment.emplace_back(ubse::common::def::UBSE_HCOM_FILE_PATH_PREFIX + "=" + workDir_ + "/hcom");
    environment.emplace_back("UBSE_IT_MESH_TYPE=" + std::to_string(meshType_));
    // CLOS: podId=0, superPodId=1, serverIdx=nodeId-1 (逐节点递增)
    // FULL_MESH: podId/superPodId/serverIdx 保持默认即可
    environment.emplace_back("UBSE_IT_POD_ID=0");
    environment.emplace_back("UBSE_IT_SUPER_POD_ID=1");
    // nodeId 从 1 开始，serverIdx 从 0 开始：serverIdx = nodeId - 1
    uint32_t serverIdx = 0;
    try {
        serverIdx = static_cast<uint32_t>(std::stoul(nodeId_)) - 1;
    } catch (...) {
        serverIdx = 0;
    }
    environment.emplace_back("UBSE_IT_SERVER_IDX=" + std::to_string(serverIdx));
    if (!sceneType_.empty()) {
        environment.emplace_back("SCENE_TYPE=" + sceneType_);
    }
    environment.emplace_back("UBSE_IT_XALARM_FIFO_PATH=" + xalarmFifoPath_);
    environment.emplace_back("UBSE_IT_SYSFS_DIR=" + workDir_ + "/sysfs");
    // Per-node cert dirs (consumed by the preload stub's path redirection
    // for non-privileged runs; privileged runs use launcher bind-mounts).
    environment.emplace_back("UBSE_IT_CERT_DIR=" + workDir_ + "/cert");
    environment.emplace_back("UBSE_IT_LCNE_CERT_DIR=" + workDir_ + "/lcne_cert");
    return environment;
}

bool NodeProcessManager::InitSharedCaDir() const
{
    // Idempotently stage the shared CA authority dir (baseWorkDir/cert_authority):
    //   CA/{cacert.pem, cakey.pem, ca.crl} + cert.sh
    // The first node to start creates it; later nodes reuse it as-is, so the
    // whole cluster signs against ONE CA database (index.txt/serial, managed
    // inside cert.sh) and every server cert gets a globally unique serial.
    if (std::filesystem::exists(certAuthorityDir_ + "/CA/cacert.pem")) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(certAuthorityDir_ + "/CA", ec);
    if (ec) {
        IT_LOG_WARN << "Failed to create shared CA dir " << certAuthorityDir_ << ": " << ec.message();
        return false;
    }
    const std::vector<std::pair<std::string, std::string>> resources = {
        {"/cacert.pem", "/CA/cacert.pem"}, {"/cakey.pem", "/CA/cakey.pem"}, {"/ca.crl", "/CA/ca.crl"},
        {"/cert.sh",    "/cert.sh"},
    };
    for (const auto& [name, relTarget] : resources) {
        const std::string src = certResourceDir_ + name;
        if (!std::filesystem::exists(src)) {
            continue; // ca.crl is optional
        }
        std::filesystem::copy_file(src, certAuthorityDir_ + relTarget, std::filesystem::copy_options::overwrite_existing,
                                   ec);
        if (ec) {
            IT_LOG_WARN << "Failed to copy cert resource " << src << ": " << ec.message();
            return false;
        }
    }
    return true;
}

void NodeProcessManager::CopyCertResources(const std::string& certToolDir) const
{
    // Ship the signing script to this node's workDir; CA files stay in the
    // shared authority dir so all nodes sign against one database.
    const std::string src = certResourceDir_ + "/cert.sh";
    std::error_code ec;
    std::filesystem::copy_file(src, certToolDir + "/cert.sh", std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        IT_LOG_WARN << "Failed to copy cert resource " << src << ": " << ec.message();
    }
}

bool NodeProcessManager::GenerateServerCert(const std::string& certToolDir) const
{
    // Sign this node's server certificate against the SHARED CA database:
    //   cert.sh --server-only -o <nodeId> --ca <sharedCertAuthorityDir> <certToolDir>/out
    // The cert embeds the per-node identity (otherName/CN/DNS = nodeId) and is
    // recorded in the shared index.txt with a globally unique serial number.
    // Output: out/server/{cert.pem,key.pem} + out/key_pwd.txt
    const std::string logFile = certToolDir + "/gen.log";
    const std::string cmd = "bash " + certToolDir + "/cert.sh --server-only -o " + nodeId_ + " --ca " +
                            certAuthorityDir_ + " " + certToolDir + "/out < /dev/null > " + logFile + " 2>&1";
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        IT_LOG_WARN << "cert.sh failed for node " << nodeId_ << ", rc=" << rc << ", log: " << logFile;
        return false;
    }
    return true;
}

bool NodeProcessManager::ImportCertsViaUbsectl(const std::string& certToolDir) const
{
    // Production-equivalent import: "ubsectl import cert" copies the cert set
    // into /var/lib/ubse/cert as user ubse with 0600 permissions. The preload
    // stub redirects that hardcoded path onto this node's workDir/cert, so
    // every IT node keeps its own certificate set without mount namespaces.
    // Falls back to direct file placement when unavailable.
    if (geteuid() != 0) {
        return false; // import switches to the ubse user, which requires root
    }
    struct passwd* pw = getpwnam("ubse");
    if (pw == nullptr) {
        IT_LOG_WARN << "user 'ubse' not found, fallback to direct cert placement";
        return false;
    }
    const std::string ubsectl = (std::filesystem::path(binaryPath_).parent_path() / "ubsectl").string();
    if (!std::filesystem::exists(ubsectl)) {
        IT_LOG_WARN << "ubsectl not found at " << ubsectl << ", fallback to direct cert placement";
        return false;
    }

    // ubsectl creates destination files as user ubse, so the target dir must
    // be owned by ubse (like the production /var/lib/ubse/cert directory).
    const std::string certDir = workDir_ + "/cert";
    std::error_code ec;
    std::filesystem::create_directories(certDir, ec);
    if (chown(certDir.c_str(), pw->pw_uid, pw->pw_gid) != 0) {
        IT_LOG_WARN << "chown " << certDir << " to ubse failed: " << strerror(errno);
        return false;
    }

    // Password is generated by cert.sh into out/key_pwd.txt; pipe it via
    // stdin for ubsectl's interactive prompt.
    std::string password;
    std::ifstream pwdFile(certToolDir + "/out/key_pwd.txt");
    std::getline(pwdFile, password);
    if (password.empty()) {
        IT_LOG_WARN << "key password missing from " << certToolDir << "/out/key_pwd.txt";
        return false;
    }

    const std::string preload = stubLibDir_ + "/libubse_interface_preload.so";
    const std::string logFile = certToolDir + "/import.log";
    const std::string cmd = "printf '%s\\n' '" + password + "' | env LD_PRELOAD=" + preload +
                            " UBSE_IT_CERT_DIR=" + certDir + " " + ubsectl + " import cert" + " --server-cert-file " +
                            certToolDir + "/out/server/cert.pem" + " --server-key-file " + certToolDir +
                            "/out/server/key.pem" + " --ca-cert-file " + certAuthorityDir_ + "/CA/cacert.pem" +
                            " --ca-crl-file " + certAuthorityDir_ + "/CA/ca.crl" + " > " + logFile + " 2>&1";
    const int rc = std::system(cmd.c_str());
    if (rc != 0 || !std::filesystem::exists(certDir + "/server.pem")) {
        IT_LOG_WARN << "ubsectl import failed for node " << nodeId_ << ", rc=" << rc << ", log: " << logFile;
        return false;
    }

    // ubsectl only populates /var/lib/ubse/cert; mirror the imported set into
    // lcne_cert (http/vsock TLS) so the privileged launcher's lcne_cert bind
    // mount has a source and root/non-root runs expose the same cert layout
    // as the InstallCertFiles fallback.
    const std::string lcneCertDir = workDir_ + "/lcne_cert";
    std::filesystem::create_directories(lcneCertDir, ec);
    for (const char* name : {"server.pem", "server_key.pem", "trust.pem", "ca.crl", "key_pwd.txt"}) {
        const std::string src = certDir + "/" + name;
        if (!std::filesystem::exists(src)) {
            continue; // optional file (e.g. ca.crl) not present
        }
        std::filesystem::copy_file(src, lcneCertDir + "/" + name, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            IT_LOG_WARN << "Failed to mirror " << src << " into lcne_cert: " << ec.message();
        }
    }
    IT_LOG_INFO << "Imported certs for node " << nodeId_ << " via ubsectl (production path)";
    return true;
}

void NodeProcessManager::InstallCertFiles(const std::string& certToolDir) const
{
    // The daemon reads two hardcoded cert dirs: /var/lib/ubse/cert (com engine
    // TLS) and /var/lib/ubse/lcne_cert (http/vsock). The same file set is
    // installed to both per-node dirs, which are bind-mounted by the launcher.
    const std::vector<std::string> targets = {workDir_ + "/cert", workDir_ + "/lcne_cert"};
    // Per-node files come from the local signing output; CA files come from
    // the shared authority dir.
    const std::vector<std::pair<std::string, std::string>> nodeFiles = {
        {"/out/server/cert.pem", "/server.pem"},
        {"/out/server/key.pem", "/server_key.pem"},
        {"/out/key_pwd.txt", "/key_pwd.txt"},
    };
    const std::vector<std::pair<std::string, std::string>> caFiles = {
        {"/CA/cacert.pem", "/trust.pem"},
        {"/CA/ca.crl", "/ca.crl"},
    };
    for (const auto& target : targets) {
        std::error_code ec;
        std::filesystem::create_directories(target, ec);
        if (ec) {
            IT_LOG_WARN << "Failed to create cert dir " << target << ": " << ec.message();
            continue;
        }
        for (const auto* fileList : {&nodeFiles, &caFiles}) {
            const std::string& base = (fileList == &nodeFiles) ? certToolDir : certAuthorityDir_;
            for (const auto& [relSrc, relDst] : *fileList) {
                const std::string src = base + relSrc;
                if (!std::filesystem::exists(src)) {
                    continue; // optional file (e.g. ca.crl) not generated
                }
                std::filesystem::copy_file(src, target + relDst, std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    IT_LOG_WARN << "Failed to install cert file " << src << " to " << target << ": " << ec.message();
                }
            }
        }
    }
}

bool NodeProcessManager::DeployCerts() const
{
    if (certResourceDir_.empty()) {
        return true; // certs not configured for this scenario
    }
    if (!std::filesystem::exists(certResourceDir_ + "/cert.sh") ||
        !std::filesystem::exists(certResourceDir_ + "/cacert.pem")) {
        IT_LOG_ERROR << "Cert resources incomplete in " << certResourceDir_
                     << " (need cert.sh + cacert.pem), skip cert deployment";
        return false;
    }
    if (certAuthorityDir_.empty() || !InitSharedCaDir()) {
        return false;
    }

    const std::string certToolDir = workDir_ + "/cert_tool";
    std::error_code ec;
    std::filesystem::create_directories(certToolDir, ec);
    if (ec) {
        IT_LOG_ERROR << "Failed to create cert tool dir " << certToolDir << ": " << ec.message();
        return false;
    }

    CopyCertResources(certToolDir);
    if (!GenerateServerCert(certToolDir)) {
        return false;
    }
    // Prefer the production import path (ubsectl import cert); fall back to
    // direct file placement when root/ubse user/ubsectl are unavailable.
    if (!ImportCertsViaUbsectl(certToolDir)) {
        InstallCertFiles(certToolDir);
    }
    IT_LOG_INFO << "Deployed per-node certs for node " << nodeId_
                << " (centralized signing against shared CA database)";
    return true;
}

UbseResult NodeProcessManager::Start()
{
    if (IsRunning()) {
        IT_LOG_INFO << "Node " << nodeId_ << " already running with pid " << childPid_;
        return UBSE_OK;
    }

    std::filesystem::create_directories(workDir_);
    std::filesystem::create_directories(workDir_ + "/run");
    std::filesystem::create_directories(workDir_ + "/log");
    std::filesystem::create_directories(workDir_ + "/hcom");
    std::filesystem::create_directories(workDir_ + "/plugin");

    // Copy plugin .so files into per-node plugin directory so the daemon can
    // discover and dlopen them via the bind-mounted /usr/lib64/ubse_plugin.
    // - libmock_plugin.so: IT fault/OOM handler (always deployed)
    // - libmem_plugin.so: mem controller module (PLUGIN_MODULE_IMPL), needed for
    //   mem CLI/SDK APIs; without it UbseMemControllerModule never registers.
    const std::vector<std::pair<std::string, std::string>> pluginArtifacts = {
        {stubLibDir_ + "/libmem_plugin.so", workDir_ + "/plugin/libmem_plugin.so"},
    };
    for (const auto& [src, dst] : pluginArtifacts) {
        if (std::filesystem::exists(src)) {
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
        } else {
            IT_LOG_WARN << "Plugin artifact not found, skip: " << src;
        }
    }

    /* Create xalarm FIFO (idempotent, already done by stub if daemon started first) */
    if (mkfifo(xalarmFifoPath_.c_str(), 0666) != 0 && errno != EEXIST) {
        IT_LOG_WARN << "Failed to create xalarm FIFO at " << xalarmFifoPath_ << ": " << strerror(errno);
    }

    // Deploy per-node certificates before spawning so the daemon (and its
    // bind-mounted cert dirs) see a complete certificate set from the start.
    // Fail fast: with cert.use=true a missing cert set only surfaces much
    // later as an obscure daemon TLS init failure far from the root cause.
    if (!DeployCerts()) {
        IT_LOG_ERROR << "Cert deployment failed for node " << nodeId_ << ", abort node start";
        return UBSE_ERROR_DEF(10);
    }

    isPrivileged_ = (geteuid() == 0);

    if (!std::filesystem::exists(launcherPath_)) {
        IT_LOG_ERROR << "IT node launcher not found: " << launcherPath_;
        return UBSE_ERROR_DEF(9);
    }

    std::vector<std::string> environment = BuildChildEnvironment();
    std::vector<char*> envp;
    envp.reserve(environment.size() + 1);
    for (auto& entry : environment) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);

    std::string privileged = isPrivileged_ ? "1" : "0";
    std::vector<char*> argv = {launcherPath_.data(), binaryPath_.data(), workDir_.data(),
                               privileged.data(),    stubLibDir_.data(), nullptr};
    pid_t pid = -1;
    int spawnRet = posix_spawn(&pid, launcherPath_.c_str(), nullptr, nullptr, argv.data(), envp.data());
    if (spawnRet != 0) {
        IT_LOG_ERROR << "Failed to spawn node " << nodeId_ << ": " << strerror(spawnRet);
        return UBSE_ERROR_DEF(1);
    }

    childPid_ = pid;
    IT_LOG_INFO << "Node " << nodeId_ << " started with pid " << childPid_;
    return UBSE_OK;
}

void NodeProcessManager::StopAuxiliaryServices()
{
    if (!xalarmFifoPath_.empty()) {
        unlink(xalarmFifoPath_.c_str());
    }
}

UbseResult NodeProcessManager::Stop()
{
    if (childPid_ <= 0) {
        return UBSE_OK;
    }
    if (!IsRunning()) {
        IT_LOG_INFO << "Node " << nodeId_ << " not running";
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }
    return StopProcess(SIGTERM);
}

UbseResult NodeProcessManager::Kill()
{
    if (!IsRunning()) {
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }
    return StopProcess(SIGKILL);
}

UbseResult NodeProcessManager::StopProcess(int signal)
{
    if (kill(childPid_, signal) != 0 && errno != ESRCH) {
        IT_LOG_ERROR << "Failed to send signal " << signal << " to node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(2);
    }

    if (signal == SIGKILL) {
        int status = 0;
        (void)waitpid(childPid_, &status, 0);
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }

    constexpr uint32_t waitTimeoutMs = 5000;
    constexpr uint32_t pollIntervalMs = 100;
    uint32_t elapsed = 0;
    while (elapsed < waitTimeoutMs) {
        int status = 0;
        pid_t ret = waitpid(childPid_, &status, WNOHANG);
        if (ret == childPid_) {
            IT_LOG_INFO << "Node " << nodeId_ << " terminated gracefully";
            childPid_ = -1;
            StopAuxiliaryServices();
            return UBSE_OK;
        }
        if (ret < 0) {
            IT_LOG_ERROR << "waitpid error for node " << nodeId_ << ": " << strerror(errno);
            childPid_ = -1;
            StopAuxiliaryServices();
            return UBSE_ERROR_DEF(3);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }

    IT_LOG_INFO << "Node " << nodeId_ << " did not terminate, sending SIGKILL";
    if (kill(childPid_, SIGKILL) != 0 && errno != ESRCH) {
        IT_LOG_ERROR << "Failed to send SIGKILL to node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(4);
    }

    int status = 0;
    pid_t ret = waitpid(childPid_, &status, 0);
    if (ret == childPid_) {
        IT_LOG_INFO << "Node " << nodeId_ << " killed";
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }

    IT_LOG_ERROR << "Failed to kill node " << nodeId_;
    return UBSE_ERROR_DEF(5);
}

bool NodeProcessManager::IsRunning() const
{
    if (childPid_ <= 0) {
        return false;
    }
    int status = 0;
    pid_t ret = waitpid(childPid_, &status, WNOHANG);
    if (ret == childPid_ || (ret < 0 && errno == ECHILD)) {
        childPid_ = -1;
        return false;
    }
    return ret == 0;
}

pid_t NodeProcessManager::GetPid() const
{
    return childPid_;
}

UbseResult NodeProcessManager::WaitForStartup(uint32_t timeoutMs)
{
    constexpr uint32_t pollIntervalMs = 100;
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        struct stat st {
        };
        if (stat(udsSocketPath_.c_str(), &st) == 0) {
            IT_LOG_INFO << "Node " << nodeId_ << " UDS socket ready at " << udsSocketPath_;
            return UBSE_OK;
        }
        if (!IsRunning()) {
            IT_LOG_ERROR << "Node " << nodeId_ << " process died during startup";
            return UBSE_ERROR_DEF(6);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }
    IT_LOG_ERROR << "Node " << nodeId_ << " startup timed out after " << timeoutMs << "ms";
    return UBSE_ERROR_DEF(7);
}

UbseResult NodeProcessManager::WaitForDaemonReady(uint32_t timeoutMs)
{
    constexpr uint32_t pollIntervalMs = 100;
    std::string logPath = workDir_ + "/log/ubse.log";
    std::string readyMarker = "ubse service started successfully.";
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        if (!IsRunning()) {
            IT_LOG_ERROR << "Node " << nodeId_ << " process died while waiting for daemon readiness";
            return UBSE_ERROR_DEF(6);
        }
        std::ifstream logFile(logPath);
        if (logFile.is_open()) {
            std::string line;
            while (std::getline(logFile, line)) {
                if (line.find(readyMarker) != std::string::npos) {
                    IT_LOG_INFO << "Node " << nodeId_ << " daemon fully ready";
                    return UBSE_OK;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }
    IT_LOG_WARN << "Node " << nodeId_ << " daemon readiness not confirmed after " << timeoutMs
                << "ms, proceeding anyway";
    return UBSE_OK;
}

const std::string& NodeProcessManager::GetNodeId() const
{
    return nodeId_;
}

const std::string& NodeProcessManager::GetWorkDir() const
{
    return workDir_;
}

const std::string& NodeProcessManager::GetUdsSocketPath() const
{
    return udsSocketPath_;
}

const std::string& NodeProcessManager::GetXalarmFifoPath() const
{
    return xalarmFifoPath_;
}

UbseResult NodeProcessManager::InjectAlarmEvent(unsigned short alarmId, const std::string& paras)
{
    return ItXalarmHelper::InjectEvent(xalarmFifoPath_, alarmId, paras);
}

} // namespace ubse::it::infra
