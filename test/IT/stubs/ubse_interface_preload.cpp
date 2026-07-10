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

/*
 * LD_PRELOAD stub library for IT testing.
 *
 * Only contains libc function overrides that LD_PRELOAD CAN intercept
 * (shared library symbols). Class method overrides for static libraries
 * do NOT work via LD_PRELOAD and are handled with IT-only link-time mocks
 * in ubse_it_daemon.
 *
 * Current overrides:
 *   getgrnam("ubm_nuds") -> return fake group (bypass group lookup)
 *   getpwuid/getpwuid_r  -> return fake "ubse" user for IT peer permission checks
 *   connect(2) to election TCP peers -> bind source to this IT node IP first
 *   connect(2) to LCNE fixed UDS -> redirect to this IT node's mock LCNE UDS
 *   /var/run/ubse access -> redirect to this IT node runtime directory
 *   /etc/ubse access -> redirect to this IT node config directory
 *     (opendir, lstat, stat, fopen, realpath, access)
 *   /sys/devices/system/node/* -> redirect to UBSE_IT_SYSFS_DIR sysfs tree
 *   /sys/devices/system/cpu/*  -> redirect to UBSE_IT_SYSFS_DIR sysfs tree
 *   /sys/kernel/obmm_mempool/* -> redirect to UBSE_IT_SYSFS_DIR sysfs tree
 *   /proc/net/fib_trie         -> redirect to UBSE_IT_SYSFS_DIR sysfs tree
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>

namespace {
constexpr uint16_t ELECTION_TCP_PORT = 1901;
constexpr const char* UBSE_DEFAULT_CONF_DIR = "/etc/ubse";
constexpr const char* UBSE_DEFAULT_LCNE_UDS_PATH = "/run/ubm/socket/ubm_nuds/restconf.sock";
constexpr const char* UBSE_DEFAULT_RUNTIME_DIR = "/var/run/ubse";
constexpr const char* UBSE_IT_UDS_SOCKET_PATH_ENV = "UBSE_IT_UDS_SOCKET_PATH";
constexpr const char* UBSE_IT_AUTH_USER = "ubse";
constexpr const char* UBSE_OBMM_DEV_PREFIX = "/dev/obmm_shmdev";

bool IsObmmDevicePath(const char* path)
{
    if (path == nullptr) {
        return false;
    }
    return strncmp(path, UBSE_OBMM_DEV_PREFIX, strlen(UBSE_OBMM_DEV_PREFIX)) == 0;
}

bool CsvContainsIp(const char* csv, in_addr_t ip)
{
    if (csv == nullptr || csv[0] == '\0') {
        return false;
    }
    std::string values(csv);
    size_t start = 0;
    while (start <= values.size()) {
        size_t end = values.find(',', start);
        std::string item = values.substr(start, end == std::string::npos ? std::string::npos : end - start);
        in_addr parsed{};
        if (inet_pton(AF_INET, item.c_str(), &parsed) == 1 && parsed.s_addr == ip) {
            return true;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

void BindElectionTcpSourceIfNeeded(int fd, const struct sockaddr* addr, socklen_t addrlen)
{
    if (addr == nullptr || addrlen < static_cast<socklen_t>(sizeof(sockaddr_in)) || addr->sa_family != AF_INET) {
        return;
    }

    const auto* target = reinterpret_cast<const sockaddr_in*>(addr);
    if (ntohs(target->sin_port) != ELECTION_TCP_PORT ||
        !CsvContainsIp(getenv("UBSE_IT_CLUSTER_IPS"), target->sin_addr.s_addr)) {
        return;
    }

    const char* localIp = getenv("UBSE_IT_NODE_IP");
    if (localIp == nullptr || localIp[0] == '\0') {
        return;
    }

    sockaddr_in current{};
    socklen_t currentLen = sizeof(current);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&current), &currentLen) == 0 && current.sin_family == AF_INET &&
        current.sin_port != 0) {
        return;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = 0;
    if (inet_pton(AF_INET, localIp, &local.sin_addr) != 1) {
        return;
    }

    int savedErrno = errno;
    if (bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
        errno = savedErrno;
        return;
    }
    errno = savedErrno;
}

std::string GetParentDir(const std::string& path)
{
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        return "/";
    }
    return path.substr(0, lastSlash);
}

std::string GetItUbseUdsSocketPath()
{
    const char* redirectedPath = getenv(UBSE_IT_UDS_SOCKET_PATH_ENV);
    if (redirectedPath == nullptr || redirectedPath[0] == '\0') {
        redirectedPath = getenv("UBSE_UDS_ADDRESS");
    }
    if (redirectedPath == nullptr || redirectedPath[0] == '\0') {
        return "";
    }
    return redirectedPath;
}

std::string RedirectUbseRuntimePath(const char* path)
{
    if (path == nullptr) {
        return "";
    }

    std::string udsSocketPath = GetItUbseUdsSocketPath();
    if (udsSocketPath.empty()) {
        return path;
    }

    std::string runtimeDir = GetParentDir(udsSocketPath);
    std::string original(path);
    if (original == UBSE_DEFAULT_RUNTIME_DIR) {
        return runtimeDir;
    }

    std::string runtimePrefix = std::string(UBSE_DEFAULT_RUNTIME_DIR) + "/";
    if (original.rfind(runtimePrefix, 0) == 0) {
        return runtimeDir + original.substr(strlen(UBSE_DEFAULT_RUNTIME_DIR));
    }
    return original;
}

bool FillRedirectedUdsAddr(const std::string& redirectedPath, sockaddr_un& redirected, socklen_t& redirectedLen)
{
    if (redirectedPath.empty()) {
        return false;
    }
    if (redirectedPath.size() >= sizeof(redirected.sun_path)) {
        errno = ENAMETOOLONG;
        return false;
    }

    memset(&redirected, 0, sizeof(redirected));
    redirected.sun_family = AF_UNIX;
    memcpy(redirected.sun_path, redirectedPath.c_str(), redirectedPath.size() + 1);
    redirectedLen = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + redirectedPath.size() + 1);
    return true;
}

bool BuildRedirectedUdsAddr(const struct sockaddr* addr, socklen_t addrlen, const std::string& redirectedPath,
                            sockaddr_un& redirected, socklen_t& redirectedLen)
{
    if (addr == nullptr || addrlen < static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1) ||
        addr->sa_family != AF_UNIX) {
        return false;
    }

    const auto* udsAddr = reinterpret_cast<const sockaddr_un*>(addr);
    if (udsAddr->sun_path[0] == '\0') {
        return false;
    }

    return FillRedirectedUdsAddr(redirectedPath, redirected, redirectedLen);
}

bool BuildRedirectedLcneUdsAddr(const struct sockaddr* addr, socklen_t addrlen, sockaddr_un& redirected,
                                socklen_t& redirectedLen)
{
    if (addr == nullptr || addrlen < static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1) ||
        addr->sa_family != AF_UNIX) {
        return false;
    }

    const auto* udsAddr = reinterpret_cast<const sockaddr_un*>(addr);
    if (udsAddr->sun_path[0] == '\0') {
        return false;
    }

    std::string originalPath(udsAddr->sun_path, strnlen(udsAddr->sun_path, sizeof(udsAddr->sun_path)));
    if (originalPath != UBSE_DEFAULT_LCNE_UDS_PATH) {
        return false;
    }

    const char* redirectedPath = getenv("UBSE_IT_LCNE_UDS_PATH");
    if (redirectedPath == nullptr || redirectedPath[0] == '\0') {
        return false;
    }
    return FillRedirectedUdsAddr(redirectedPath, redirected, redirectedLen);
}

bool BuildRedirectedUbseUdsAddr(const struct sockaddr* addr, socklen_t addrlen, sockaddr_un& redirected,
                                socklen_t& redirectedLen)
{
    if (addr == nullptr || addrlen < static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1) ||
        addr->sa_family != AF_UNIX) {
        return false;
    }

    const auto* udsAddr = reinterpret_cast<const sockaddr_un*>(addr);
    if (udsAddr->sun_path[0] == '\0') {
        return false;
    }

    std::string originalPath(udsAddr->sun_path, strnlen(udsAddr->sun_path, sizeof(udsAddr->sun_path)));
    std::string redirectedPath = RedirectUbseRuntimePath(originalPath.c_str());
    if (redirectedPath == originalPath) {
        return false;
    }
    return BuildRedirectedUdsAddr(addr, addrlen, redirectedPath, redirected, redirectedLen);
}

std::string RedirectUbseConfPath(const char* path)
{
    if (path == nullptr) {
        return "";
    }

    const char* itConfDir = getenv("UBSE_IT_CONF_DIR");
    if (itConfDir == nullptr || itConfDir[0] == '\0') {
        return path;
    }

    std::string original(path);
    // Redirect /etc/ubse paths to the IT workDir directly.
    // Combined with fopen/realpath interception, Init("/etc/ubse") can
    // find and read the IT-generated ubse.conf in workDir, eliminating
    // the need for a separate -f <workDir> config pass.
    std::string confDir(itConfDir);
    if (original == UBSE_DEFAULT_CONF_DIR) {
        return confDir;
    }
    if (original.rfind(std::string(UBSE_DEFAULT_CONF_DIR) + "/", 0) == 0) {
        return confDir + original.substr(strlen(UBSE_DEFAULT_CONF_DIR));
    }
    return original;
}

// Sysfs paths read by UbseNodeControllerCollector via std::ifstream.
// When UBSE_IT_SYSFS_DIR is set, redirect these paths so the collector
// reads from the IT-generated virtual sysfs tree instead of the real
// kernel filesystem (which doesn't exist in CI / x86 containers).
constexpr const char* SYSFS_NODE_PREFIX = "/sys/devices/system/node/";
constexpr const char* SYSFS_CPU_PREFIX = "/sys/devices/system/cpu/";
constexpr const char* SYSFS_OBMM_PREFIX = "/sys/kernel/obmm_mempool/";
constexpr const char* PROC_FIB_TRIE = "/proc/net/fib_trie";
constexpr const char* UBSE_IT_SYSFS_DIR_ENV = "UBSE_IT_SYSFS_DIR";

std::string RedirectSysfsPath(const char* path)
{
    if (path == nullptr) {
        return "";
    }
    const char* sysfsDir = getenv(UBSE_IT_SYSFS_DIR_ENV);
    if (sysfsDir == nullptr || sysfsDir[0] == '\0') {
        return path;
    }
    std::string original(path);
    if (original.rfind(SYSFS_NODE_PREFIX, 0) == 0) {
        return std::string(sysfsDir) + original;
    }
    if (original.rfind(SYSFS_CPU_PREFIX, 0) == 0) {
        return std::string(sysfsDir) + original;
    }
    if (original.rfind(SYSFS_OBMM_PREFIX, 0) == 0) {
        return std::string(sysfsDir) + original;
    }
    if (original == PROC_FIB_TRIE) {
        return std::string(sysfsDir) + original;
    }
    return original;
}
} // namespace

// ============================================================
// /etc/ubse path redirection: redirect config directory access
// to the IT node workDir so Init("/etc/ubse") sees an empty
// directory (or the IT-generated ubse.conf if -f is also set).
// This is needed because CI environments lack /etc/ubse/ and
// the test user has no permission to create it. By redirecting
// opendir and lstat for /etc/ubse paths, UbseConfigManager::Init
// succeeds even when /etc/ubse/ doesn't physically exist.
// ============================================================

static DIR* (*real_opendir)(const char*) = nullptr;
static int (*real_lstat)(const char*, struct stat*) = nullptr;
static FILE* (*real_fopen64)(const char*, const char*) = nullptr;
static int (*real_stat)(const char*, struct stat*) = nullptr;

static void init_real_conf_funcs()
{
    if (real_opendir == nullptr) {
        real_opendir = reinterpret_cast<DIR* (*)(const char*)>(dlsym(RTLD_NEXT, "opendir"));
    }
    if (real_lstat == nullptr) {
        real_lstat = reinterpret_cast<int (*)(const char*, struct stat*)>(dlsym(RTLD_NEXT, "lstat"));
    }
    if (real_fopen64 == nullptr) {
        real_fopen64 = reinterpret_cast<FILE* (*)(const char*, const char*)>(dlsym(RTLD_NEXT, "fopen64"));
    }
    if (real_stat == nullptr) {
        real_stat = reinterpret_cast<int (*)(const char*, struct stat*)>(dlsym(RTLD_NEXT, "stat"));
    }
}

extern "C" DIR* opendir(const char* name)
{
    init_real_conf_funcs();
    if (real_opendir == nullptr) {
        errno = ENOSYS;
        return nullptr;
    }
    std::string redirected = RedirectSysfsPath(name);
    redirected = RedirectUbseConfPath(redirected.c_str());
    return real_opendir(redirected.c_str());
}

extern "C" int lstat(const char* path, struct stat* buf)
{
    // OBMM device files don't exist in IT; return a fake stat result
    if (IsObmmDevicePath(path)) {
        if (buf != nullptr) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = S_IFCHR | 0660;
            buf->st_uid = getuid();
            buf->st_gid = getgid();
        }
        return 0;
    }

    init_real_conf_funcs();
    if (real_lstat == nullptr) {
        errno = ENOSYS;
        return -1;
    }
    std::string redirected = RedirectSysfsPath(path);
    redirected = RedirectUbseConfPath(redirected.c_str());
    redirected = RedirectUbseRuntimePath(redirected.c_str());
    return real_lstat(redirected.c_str(), buf);
}

extern "C" FILE* fopen64(const char* path, const char* mode)
{
    init_real_conf_funcs();
    if (real_fopen64 == nullptr) {
        errno = ENOSYS;
        return nullptr;
    }
    std::string redirected = RedirectSysfsPath(path);
    redirected = RedirectUbseConfPath(redirected.c_str());
    return real_fopen64(redirected.c_str(), mode);
}

// fopen and fopen64 are aliased on 64-bit glibc; intercept both to be safe.
extern "C" FILE* fopen(const char* path, const char* mode) __attribute__((alias("fopen64")));

extern "C" int stat(const char* path, struct stat* buf)
{
    // OBMM device files don't exist in IT; return a fake stat result
    if (IsObmmDevicePath(path)) {
        if (buf != nullptr) {
            memset(buf, 0, sizeof(struct stat));
            buf->st_mode = S_IFCHR | 0660;
            buf->st_uid = getuid();
            buf->st_gid = getgid();
        }
        return 0;
    }

    init_real_conf_funcs();
    if (real_stat == nullptr) {
        errno = ENOSYS;
        return -1;
    }
    std::string redirected = RedirectSysfsPath(path);
    redirected = RedirectUbseConfPath(redirected.c_str());
    redirected = RedirectUbseRuntimePath(redirected.c_str());
    return real_stat(redirected.c_str(), buf);
}

// ============================================================
// /var/run/ubse runtime path redirection for non-root IT nodes.
// ============================================================

static int (*real_bind)(int, const struct sockaddr*, socklen_t) = nullptr;
static char* (*real_realpath)(const char*, char*) = nullptr;
static int (*real_mkdir)(const char*, mode_t) = nullptr;
static int (*real_chmod)(const char*, mode_t) = nullptr;
static int (*real_chown)(const char*, uid_t, gid_t) = nullptr;
static int (*real_unlink)(const char*) = nullptr;
static int (*real_access)(const char*, int) = nullptr;

static void init_real_runtime_funcs()
{
    if (real_bind == nullptr) {
        real_bind = reinterpret_cast<int (*)(int, const struct sockaddr*, socklen_t)>(dlsym(RTLD_NEXT, "bind"));
    }
    if (real_realpath == nullptr) {
        real_realpath = reinterpret_cast<char* (*)(const char*, char*)>(dlsym(RTLD_NEXT, "realpath"));
    }
    if (real_mkdir == nullptr) {
        real_mkdir = reinterpret_cast<int (*)(const char*, mode_t)>(dlsym(RTLD_NEXT, "mkdir"));
    }
    if (real_chmod == nullptr) {
        real_chmod = reinterpret_cast<int (*)(const char*, mode_t)>(dlsym(RTLD_NEXT, "chmod"));
    }
    if (real_chown == nullptr) {
        real_chown = reinterpret_cast<int (*)(const char*, uid_t, gid_t)>(dlsym(RTLD_NEXT, "chown"));
    }
    if (real_unlink == nullptr) {
        real_unlink = reinterpret_cast<int (*)(const char*)>(dlsym(RTLD_NEXT, "unlink"));
    }
    if (real_access == nullptr) {
        real_access = reinterpret_cast<int (*)(const char*, int)>(dlsym(RTLD_NEXT, "access"));
    }
}

extern "C" int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    init_real_runtime_funcs();
    if (real_bind == nullptr) {
        errno = ENOSYS;
        return -1;
    }

    sockaddr_un redirected{};
    socklen_t redirectedLen = 0;
    if (BuildRedirectedUbseUdsAddr(addr, addrlen, redirected, redirectedLen)) {
        return real_bind(sockfd, reinterpret_cast<const sockaddr*>(&redirected), redirectedLen);
    }
    return real_bind(sockfd, addr, addrlen);
}

extern "C" char* realpath(const char* path, char* resolvedPath)
{
    init_real_runtime_funcs();
    init_real_conf_funcs();
    if (real_realpath == nullptr) {
        errno = ENOSYS;
        return nullptr;
    }

    std::string sysfsRedirected = RedirectSysfsPath(path);
    std::string confRedirected = RedirectUbseConfPath(sysfsRedirected.c_str());
    std::string finalRedirected = RedirectUbseRuntimePath(confRedirected.c_str());
    return real_realpath(finalRedirected.c_str(), resolvedPath);
}

extern "C" int mkdir(const char* pathname, mode_t mode)
{
    init_real_runtime_funcs();
    if (real_mkdir == nullptr) {
        errno = ENOSYS;
        return -1;
    }

    std::string redirected = RedirectUbseRuntimePath(pathname);
    return real_mkdir(redirected.c_str(), mode);
}

extern "C" int chmod(const char* pathname, mode_t mode)
{
    // OBMM device files don't exist in IT; chmod is a no-op for them
    if (IsObmmDevicePath(pathname)) {
        return 0;
    }

    init_real_runtime_funcs();
    if (real_chmod == nullptr) {
        errno = ENOSYS;
        return -1;
    }

    std::string redirected = RedirectUbseRuntimePath(pathname);
    return real_chmod(redirected.c_str(), mode);
}

extern "C" int chown(const char* pathname, uid_t owner, gid_t group)
{
    // OBMM device files don't exist in IT; chown is a no-op for them
    if (IsObmmDevicePath(pathname)) {
        return 0;
    }

    init_real_runtime_funcs();
    if (real_chown == nullptr) {
        errno = ENOSYS;
        return -1;
    }

    std::string redirected = RedirectUbseRuntimePath(pathname);
    return real_chown(redirected.c_str(), owner, group);
}

extern "C" int unlink(const char* pathname)
{
    init_real_runtime_funcs();
    if (real_unlink == nullptr) {
        errno = ENOSYS;
        return -1;
    }

    std::string redirected = RedirectUbseRuntimePath(pathname);
    return real_unlink(redirected.c_str());
}

extern "C" int access(const char* pathname, int mode)
{
    // OBMM device files don't exist in IT; pretend they are accessible
    if (IsObmmDevicePath(pathname)) {
        return 0;
    }

    init_real_runtime_funcs();
    init_real_conf_funcs();
    if (real_access == nullptr) {
        errno = ENOSYS;
        return -1;
    }

    std::string sysfsRedirected = RedirectSysfsPath(pathname);
    std::string confRedirected = RedirectUbseConfPath(sysfsRedirected.c_str());
    std::string finalRedirected = RedirectUbseRuntimePath(confRedirected.c_str());
    return real_access(finalRedirected.c_str(), mode);
}

// ============================================================
// connect stub: bind IT election TCP sockets to the node IP
// ============================================================

static int (*real_connect)(int, const struct sockaddr*, socklen_t) = nullptr;

static void init_real_connect()
{
    if (real_connect == nullptr) {
        real_connect = reinterpret_cast<int (*)(int, const struct sockaddr*, socklen_t)>(dlsym(RTLD_NEXT, "connect"));
    }
}

extern "C" int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    init_real_connect();
    if (real_connect == nullptr) {
        errno = ENOSYS;
        return -1;
    }
    BindElectionTcpSourceIfNeeded(sockfd, addr, addrlen);
    sockaddr_un redirected{};
    socklen_t redirectedLen = 0;
    if (BuildRedirectedLcneUdsAddr(addr, addrlen, redirected, redirectedLen)) {
        return real_connect(sockfd, reinterpret_cast<const sockaddr*>(&redirected), redirectedLen);
    }
    if (BuildRedirectedUbseUdsAddr(addr, addrlen, redirected, redirectedLen)) {
        return real_connect(sockfd, reinterpret_cast<const sockaddr*>(&redirected), redirectedLen);
    }
    return real_connect(sockfd, addr, addrlen);
}

// ============================================================
// getgrnam stub: fake ubm_nuds group
// ============================================================

static struct group fake_ubm_group;
static char fake_ubm_group_name[] = "ubm_nuds";
static char* fake_ubm_group_members[1] = {nullptr};

static struct group* (*real_getgrnam)(const char*) = nullptr;

static void init_real_getgrnam()
{
    if (real_getgrnam == nullptr) {
        real_getgrnam = reinterpret_cast<struct group* (*)(const char*)>(dlsym(RTLD_NEXT, "getgrnam"));
    }
}

extern "C" struct group* getgrnam(const char* name)
{
    if (name != nullptr && strcmp(name, "ubm_nuds") == 0) {
        fake_ubm_group.gr_name = fake_ubm_group_name;
        fake_ubm_group.gr_passwd = nullptr;
        fake_ubm_group.gr_gid = getgid();
        fake_ubm_group.gr_mem = fake_ubm_group_members;
        return &fake_ubm_group;
    }
    init_real_getgrnam();
    if (real_getgrnam == nullptr) {
        return nullptr;
    }
    return real_getgrnam(name);
}

// ============================================================
// passwd stubs: make API permission checks see the built-in ubse user.
// ============================================================

static struct passwd fake_ubse_passwd;
static char fake_ubse_user_name[] = "ubse";
static char fake_ubse_user_passwd[] = "x";
static char fake_ubse_user_gecos[] = "ubse IT user";
static char fake_ubse_user_dir[] = "/tmp";
static char fake_ubse_user_shell[] = "/sbin/nologin";

static void FillFakeUbsePasswd(struct passwd& pwd, uid_t uid)
{
    pwd.pw_name = fake_ubse_user_name;
    pwd.pw_passwd = fake_ubse_user_passwd;
    pwd.pw_uid = uid;
    pwd.pw_gid = getgid();
    pwd.pw_gecos = fake_ubse_user_gecos;
    pwd.pw_dir = fake_ubse_user_dir;
    pwd.pw_shell = fake_ubse_user_shell;
}

extern "C" struct passwd* getpwuid(uid_t uid)
{
    FillFakeUbsePasswd(fake_ubse_passwd, uid);
    return &fake_ubse_passwd;
}

extern "C" int getpwuid_r(uid_t uid, struct passwd* pwd, char* buf, size_t buflen, struct passwd** result)
{
    if (pwd == nullptr || result == nullptr) {
        return EINVAL;
    }

    const size_t nameLen = strlen(UBSE_IT_AUTH_USER) + 1;
    if (buf == nullptr || buflen < nameLen) {
        *result = nullptr;
        return ERANGE;
    }

    memcpy(buf, UBSE_IT_AUTH_USER, nameLen);
    FillFakeUbsePasswd(*pwd, uid);
    pwd->pw_name = buf;
    *result = pwd;
    return 0;
}
