# Restore old style debuginfo creation for rpm >= 4.14.
%undefine _debugsource_packages
%undefine _debuginfo_subpackages

# -*- rpm-spec -*-
Summary:        RPM package
Name:           ubs-engine
Version:        1.0.1
Release:        1
License:        Mulan PSL v2
URL:            https://atomgit.com/openeuler/ubs-engine
Source0:        %{name}-%{version}.tar.gz
Group:          System Environment/Base
Vendor:         Huawei Technologies Co., Ltd.
Prefix: /usr

BuildRequires:  cmake >= 3.22 make >= 4.3 gcc-c++ >= 10.3 gcc >= 10.3 python3-setuptools
BuildRequires:  glibc-devel >= 2.34 libstdc++-devel >= 10.3
BuildRequires:  systemd-devel >= 249
BuildRequires:  libboundscheck >= v1.1 libxml2-devel >= 2.9 openssl-devel >= 3.0 cpp-httplib-devel >= 0.40.0 rapidjson-devel >= 1.1.0 ubs-comm-devel >= 1.0.0-27
BuildRequires:  numactl-libs >= 2.0
BuildRequires:  glib2-devel >= 2.56
BuildRequires:  ninja-build >= 1.10 bash bc coreutils sudo util-linux-user patch
BuildRequires:  libvirt-devel >= 9.0 kernel-devel
Requires: glibc >= 2.34 libgcc >= 10.3 libstdc++ >= 10.3 libboundscheck >= v1.1 libxml2 >= 2.9 openssl-libs >= 3.0 cpp-httplib >= 0.40.0 ubs-comm-lib >= 1.0.0-27 obmm
Requires: tar systemd
Requires(pre): coreutils shadow systemd glibc-common
Requires(post): coreutils gawk util-linux systemd grep sed
Requires(preun): systemd grep
Requires(postun): coreutils gawk util-linux systemd shadow glibc-common
%define _rpmdir %_topdir/RPMS
%define _srcrpmdir %_topdir/SRPMS
%define _unpackaged_files_terminate_build 0

%description
UBS Engine

# ========================================================
#                   SUBPACKAGE: ubs-engine-process-mem
# ========================================================
%package processmem
Summary: processmem plugin
Requires: %{name} = %{version}-%{release}
%description processmem
Development package for processmem plugin

# ========================================================
#                   SUBPACKAGE: ubs-engine-client-libs
# ========================================================
%define lib_name libubse-client
%define lib_soversion 1

%package client-libs
Summary: UBSE client shared library for third-party integration
Provides: %{lib_name}.so.%{lib_soversion}
Requires: libboundscheck, libstdc++
Conflicts: %{name} < %{version}-%{release}
Obsoletes: %{name}-client-libs < %{version}-%{release}
Provides: %{name}-client-libs = %{version}-%{release}

%description client-libs
UBSE client shared library (%{lib_name}.so.%{lib_soversion}) for third-party applications to access UBSE services.
This package contains the runtime shared library required by programs that dynamically link to UBSE.


# ========================================================
#                   SUBPACKAGE: ubs-engine-client-devel
# ========================================================
%package client-devel
Summary: Development package for UBSE client SDK
Requires: %{name}-client-libs = %{version}-%{release}
Requires: pkgconfig
Provides: %{name}-client-devel = %{version}-%{release}

%description client-devel
Header files and static libraries for developing applications that use the UBSE client SDK.
This package is required for compiling programs that link against UBSE.


# ========================================================
#                   SUBPACKAGE: python3-ubs-engine
# ========================================================
%package -n python3-%{name}
Summary: Development package for UBSE python SDK
BuildArch: noarch
%description -n python3-%{name}
Development package for UBSE python SDK

# ========================================================
#                   SUBPACKAGE: ubs-engine-virtagent
# ========================================================
%package virtagent
Summary: virtagent plugin
Requires: %{name} = %{version}-%{release}
%description virtagent
Package for virt_agent plugin

# ========================================================
#                   SUBPACKAGE: ubs-engine-ucache
# ========================================================
%package ucache
Summary: ucache plugin
Requires: %{name} = %{version}-%{release}
%description ucache
Development package for ucache plugin

# ========================================================
#                   SUBPACKAGE: ubs-engine-rmrs
# ========================================================
%package rmrs
Summary: rmrs plugin
Requires: %{name} = %{version}-%{release}
Requires(post): coreutils shadow
%description rmrs
Development package for rmrs plugin
%post rmrs
if id "ubse" > /dev/null 2>&1; then
    usermod -aG ubturbo ubse
    usermod -aG libvirt ubse
    echo "Success: ubse user added to ubturbo and libvirt groups"
else
    echo "Warning: ubse user does not exist, skip group addition" >&2
fi


# ========================================================
#                   SUBPACKAGE: ubs-engine-ssu
# ========================================================
%package ssu
Summary: ssu
Requires: %{name} = %{version}-%{release}
%description ssu
Package for ssu


# ========================================================
#                   SUBPACKAGE: ubs-engine-ssu-client-libs
# ========================================================
%define ssu_lib_name libubse-ssu-client
%define ssu_lib_soversion 1

%package ssu-client-libs
Summary: UBSE SSU client shared library for third-party integration
Provides: %{ssu_lib_name}.so.%{ssu_lib_soversion}
Requires: %{name}-client-libs = %{version}-%{release}
Requires: libboundscheck, libstdc++
Conflicts: %{name} < %{version}-%{release}
%description ssu-client-libs
UBSE SSU client shared library (%{ssu_lib_name}.so.%{ssu_lib_soversion}) for third-party
applications to access UBSE SSU services. This package depends on ubs-engine-client-libs
for the common IPC client code (libubse-client.so) at runtime.


# ========================================================
#                   SUBPACKAGE: ubs-engine-ssu-client-devel
# ========================================================
%package ssu-client-devel
Summary: Development package for UBSE SSU client SDK
Requires: %{name}-ssu-client-libs = %{version}-%{release}
Requires: %{name}-client-devel = %{version}-%{release}
Requires: pkgconfig
Provides: %{name}-ssu-client-devel = %{version}-%{release}
%description ssu-client-devel
Header files and static libraries for developing applications that use the UBSE SSU client SDK.
This package is required for compiling programs that link against UBSE SSU. It depends on
ubs-engine-client-devel for the common SDK headers (e.g. ubs_error.h) and libubse-client.so
link symlink.


# ========================================================
#                   SUBPACKAGE: python3-ubs-engine-ssu
# ========================================================
%package -n python3-%{name}-ssu
Summary: Development package for UBSE python SSU SDK
BuildArch: noarch
Requires: python3-%{name} = %{version}-%{release}
%description -n python3-%{name}-ssu
Development package for UBSE python SSU SDK. Extends the ubse namespace package
with SSU-specific modules (ubs_engine_ssu, ubs_engine_binding_ssu, ubs_engine_model_ssu).


# ========================================================
#                   SUBPACKAGE: ubs-engine-npu-client-libs
# ========================================================
%define npu_lib_name libubse-npu-client
%define npu_lib_soversion 1

%package npu-client-libs
Summary: UBSE NPU client shared library for third-party integration
Provides: %{npu_lib_name}.so.%{npu_lib_soversion}
Requires: %{name}-client-libs = %{version}-%{release}
Requires: libboundscheck, libstdc++
Conflicts: %{name} < %{version}-%{release}
%description npu-client-libs
UBSE NPU client shared library (%{npu_lib_name}.so.%{npu_lib_soversion}) for third-party
applications to access UBSE NPU services. This package depends on ubs-engine-client-libs
for the common IPC client code (libubse-client.so) at runtime.


# ========================================================
#                   SUBPACKAGE: ubs-engine-npu-client-devel
# ========================================================
%package npu-client-devel
Summary: Development package for UBSE NPU client SDK
Requires: %{name}-npu-client-libs = %{version}-%{release}
Requires: %{name}-client-devel = %{version}-%{release}
Requires: pkgconfig
Provides: %{name}-npu-client-devel = %{version}-%{release}
%description npu-client-devel
Header files and static libraries for developing applications that use the UBSE NPU client SDK.
This package is required for compiling programs that link against UBSE NPU. It depends on
ubs-engine-client-devel for the common SDK headers (e.g. ubs_error.h) and libubse-client.so
link symlink.


# ========================================================
#                   SUBPACKAGE: python3-ubs-engine-npu
# ========================================================
%package -n python3-%{name}-npu
Summary: Development package for UBSE python NPU SDK
BuildArch: noarch
Requires: python3-%{name} = %{version}-%{release}
Requires: %{name}-npu-client-libs = %{version}-%{release}
%description -n python3-%{name}-npu
Development package for UBSE python NPU SDK. Extends the ubse namespace package
with NPU-specific modules (ubs_engine_npu, ubs_engine_binding_npu, ubs_engine_model_npu).
The ctypes binding loads libubse-npu-client.so.1 at runtime, so this package depends on
ubs-engine-npu-client-libs.


# ========================================================
#                   SUBPACKAGE: ubs-engine-npu
# ========================================================
%package npu
Summary: npu
Requires: %{name} = %{version}-%{release}
%description npu
Package for npu plugin


%define project_dir %{name}-%{version}
%define cmake_build_dir cmake-build-relwithdebinfo

%define log_dir /var/log/ubse
%define data_dir /var/lib/ubse
%define cert_dir /var/lib/ubse/cert
%define lcne_cert_dir /var/lib/ubse/lcne_cert
%define vip_server_cert_dir /var/lib/ubse/vip_server_cert
%define socket_dir /var/run/ubse

%define system_user ubse
%define system_group ubse
%define ubm_group ubm_nuds
%define service_name ubse.service

%define ensure_directory_owner() ensure_directory_owner() { \
    local dir_path="$1" \
    local recursive="${2:-false}" \
    if [ ! -d "$dir_path" ]; then \
        mkdir -p "$dir_path" \
    fi \
    local current_owner \
    current_owner=$(stat -c "%U:%G" "$dir_path") \
    if [ "$current_owner" != "%{system_user}:%{system_group}" ]; then \
        chown "%{system_user}:%{system_group}" "$dir_path" \
    fi \
    if [ "$recursive" = true ]; then \
        chown -R "%{system_user}:%{system_group}" "$dir_path" \
    fi \
}

%define deleted_semaphore() deleted_semaphore() { \
    echo "Checking and deleting semaphores..." \
    while read -r line; do \
        semid=$(echo "$line" | awk '{print $1}') \
        owner=$(echo "$line" | awk '{print $2}') \
        echo "Checking semaphore ID: $semid, Owner: $owner" \
        if [ "$owner" = "%{system_user}" ]; then \
            echo "Deleting semaphore $semid..." \
            if ! ipcrm -s $semid; then \
                echo "Failed to delete semaphore $semid" \
            else \
                echo "Deleted semaphore $semid" \
            fi \
        fi \
    done < <(ipcs -s | awk '/^[0-9]/ {print $2, $3, $4}') \
    echo "delete %{system_user} semaphores finished" \
}

%define update_config() update_config() { \
    config_file="$1" \
    if grep -q '^# mempooling=777' "$config_file"; then \
        sed -i 's/^# mempooling=777/mempooling=777/' "$config_file" \
    fi \
    if grep -q '^# virt_agent=205' "$config_file"; then \
        sed -i 's/^# virt_agent=205/virt_agent=205/' "$config_file" \
    fi \
    if grep -q '^# mem_plugin=779' "$config_file"; then \
        sed -i 's/^# mem_plugin=779/mem_plugin=779/' "$config_file" \
    fi \
    if ! grep -q 'mempooling=777' "$config_file"; then \
        echo "mempooling=777" >> "$config_file" \
    fi \
    if ! grep -q 'virt_agent=205' "$config_file"; then \
        echo "virt_agent=205" >> "$config_file" \
    fi \
}

%define remove_directory() remove_directory() { \
    if [ -d "$1" ]; then \
        rm -rf "$1" \
    fi \
}


%prep
%setup -q -T -b 0 -c -n %{project_dir}

%build
cd %{_builddir}/%{project_dir}/
bash build.sh -T RelWithDebInfo
%py3_build
# 通过清单驱动构建所有 Python 子包(新增子包只需在 subpackages/ 加清单,这里加一行)
python3 build_subpackage.py ssu build
python3 build_subpackage.py npu build

%install
#install main package
mkdir -p %{buildroot}/usr/bin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/bin/ubse %{buildroot}/usr/bin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/bin/ubsectl %{buildroot}/usr/bin

mkdir -p %{buildroot}/usr/lib/systemd/system/
cp %{_builddir}/%{project_dir}/scripts/rpm/%{service_name} %{buildroot}/usr/lib/systemd/system/

mkdir -p %{buildroot}/etc/ubse/
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/conf/ubse*.conf %{buildroot}/etc/ubse/
mkdir -p %{buildroot}/etc/ubse/plugins

mkdir -p %{buildroot}/etc/ubse/topo
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/conf/topo/*.json %{buildroot}/etc/ubse/topo/

mkdir -p %{buildroot}/etc/bash_completion.d/
cp -f %{_builddir}/%{project_dir}/scripts/command_completion/cli_commands.sh %{buildroot}/etc/bash_completion.d/

mkdir -p %{buildroot}/usr/lib64

#install virtagent
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libvirtagent.so %{buildroot}/usr/lib64/
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libstrategy.so %{buildroot}/usr/lib64/
cp %{_builddir}/%{project_dir}/src/addons/virt_agent/conf/plugin_virt_agent.conf %{buildroot}/etc/ubse/plugins/
cp %{_builddir}/%{project_dir}/src/addons/virt_agent/conf/auth-virt_agent.conf %{buildroot}/etc/ubse/plugins/
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libubs-virt-agent.so.1.0.1 %{buildroot}/usr/lib64/
ln -sf libubs-virt-agent.so.1.0.1 %{buildroot}/usr/lib64/libubs-virt-agent.so.1
ln -sf libubs-virt-agent.so.1 %{buildroot}/usr/lib64/libubs-virt-agent.so
mkdir -p %{buildroot}/usr/include/virt_agent
cp -r %{_builddir}/%{project_dir}/src/addons/virt_agent/sdk/include/* %{buildroot}/usr/include/virt_agent/

#install processmem
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libprocess_mem.so %{buildroot}/usr/lib64/
cp %{_builddir}/%{project_dir}/conf/plugin_process_mem.conf %{buildroot}/etc/ubse/plugins/

#install client-libs
cmake --install %{_builddir}/%{project_dir}/%{cmake_build_dir} \
    --component ubse_sdk \
    --prefix %{buildroot}/usr
ln -sf libubse-client.so.1.0.1 %{buildroot}/usr/lib64/libubse-client.so.1

#install client-devel
ln -sf libubse-client.so.1 %{buildroot}/usr/lib64/libubse-client.so
chmod 644 %{buildroot}/usr/lib64/libubse-client.a
cp -r %{_builddir}/%{project_dir}/src/include/* %{buildroot}/usr/include/ubse

#install ucache
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libucache_plugin.so %{buildroot}/usr/lib64/
cp %{_builddir}/%{project_dir}/src/addons/ucache/conf/plugin_ucache.conf %{buildroot}/etc/ubse/plugins/

#install mem
mkdir -p %{buildroot}/usr/lib64/ubse_plugin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libmem_plugin.so %{buildroot}/usr/lib64/ubse_plugin/

#install rmrs
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libmempooling.so %{buildroot}/usr/lib64/
cp %{_builddir}/%{project_dir}/src/addons/rmrs/conf/plugin_mempooling.conf %{buildroot}/etc/ubse/plugins/
mkdir -p %{buildroot}/usr/local/mempooling/include/mempooling/
cp %{_builddir}/%{project_dir}/src/addons/rmrs/interface/mempooling_interface.h %{buildroot}/usr/local/mempooling/include/mempooling/

#install bandbridge kernel module (only on aarch64)
%ifarch aarch64
mkdir -p %{buildroot}/lib/modules/ubse
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/modules/bandbridge.ko %{buildroot}/lib/modules/ubse
%endif


#install python-sdk
%py3_install

# ---- SSU install group (4 sub-packages) ----
#install ssu-client-libs (依赖主 client-libs 提供 libubse-client.so;只装动态库)
cmake --install %{_builddir}/%{project_dir}/%{cmake_build_dir} \
    --component ubse_ssu_sdk \
    --prefix %{buildroot}/usr
ln -sf libubse-ssu-client.so.1.0.1 %{buildroot}/usr/lib64/libubse-ssu-client.so.1

#install ssu-client-devel (只装静态库 + 头文件)
cmake --install %{_builddir}/%{project_dir}/%{cmake_build_dir} \
    --component ubse_ssu_sdk_devel \
    --prefix %{buildroot}/usr
ln -sf libubse-ssu-client.so.1 %{buildroot}/usr/lib64/libubse-ssu-client.so
chmod 644 %{buildroot}/usr/lib64/libubse-ssu-client.a

#install ssu
mkdir -p %{buildroot}/usr/lib64/ubse_plugin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/bin/ubsectl-ssu %{buildroot}/usr/bin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libssu_plugin.so %{buildroot}/usr/lib64/ubse_plugin/

#install python-ssu (通过清单驱动,扩展 ubse namespace)
python3 build_subpackage.py ssu install --root=%{buildroot} --prefix=/usr --no-compile

# ---- NPU install group (3 sub-packages) ----
#install npu-client-libs (依赖主 client-libs 提供 libubse-client.so;只装动态库)
cmake --install %{_builddir}/%{project_dir}/%{cmake_build_dir} \
    --component ubse_npu_sdk \
    --prefix %{buildroot}/usr
ln -sf libubse-npu-client.so.1.0.1 %{buildroot}/usr/lib64/libubse-npu-client.so.1

#install npu-client-devel (只装静态库 + 头文件)
cmake --install %{_builddir}/%{project_dir}/%{cmake_build_dir} \
    --component ubse_npu_sdk_devel \
    --prefix %{buildroot}/usr
ln -sf libubse-npu-client.so.1 %{buildroot}/usr/lib64/libubse-npu-client.so
chmod 644 %{buildroot}/usr/lib64/libubse-npu-client.a

#install python-npu (通过清单驱动,扩展 ubse namespace)
python3 build_subpackage.py npu install --root=%{buildroot} --prefix=/usr --no-compile

#install npu
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/lib/libnpu_plugin.so %{buildroot}/usr/lib64/ubse_plugin/

%pre
set -e
print_error() {
    echo "ERROR" "$1"
    exit 1
}

create_user() {
    local requested_uid=""
    local requested_gid=""

    if [ -n "${UBSE_USER_UID:-}" ]; then
        if [[ "${UBSE_USER_UID}" =~ ^[0-9]+$ ]] && [ "$UBSE_USER_UID" -ge 0 ]; then
            requested_uid="$UBSE_USER_UID"
        else
            print_error "Invalid UBSE_USER_UID: '$UBSE_USER_UID'. Must be a non-negative integer."
        fi
    fi

    if [ -n "${UBSE_USER_GID:-}" ]; then
        if [[ "${UBSE_USER_GID}" =~ ^[0-9]+$ ]] && [ "$UBSE_USER_GID" -ge 0 ]; then
            requested_gid="$UBSE_USER_GID"
        else
            print_error "Invalid UBSE_USER_GID: '$UBSE_USER_GID'. Must be a non-negative integer."
        fi
    fi

    local user_exists=false
    local group_exists=false

    if getent passwd %{system_user} > /dev/null; then
        user_exists=true
    fi

    if getent group %{system_group} > /dev/null; then
        group_exists=true
    fi

    if $user_exists; then
        local current_uid=$(getent passwd %{system_user} | cut -d: -f3)
        local current_gid=$(getent passwd %{system_user} | cut -d: -f4)
        if [ -n "$requested_uid" ] && [ "$current_uid" != "$requested_uid" ]; then
            print_error "User %{system_user} exists with UID $current_uid, but requested UID is $requested_uid. Cannot change UID automatically."
        fi
        if [ -n "$requested_gid" ] && [ "$current_gid" != "$requested_gid" ]; then
            print_error "User %{system_user} exists with GID $current_gid, but requested GID is $requested_gid. Cannot change GID automatically."
        fi
    fi

    if $group_exists; then
        local current_gid=$(getent group %{system_group} | cut -d: -f3)
        if [ -n "$requested_gid" ] && [ "$current_gid" != "$requested_gid" ]; then
            print_error "Group %{system_group} exists with GID $current_gid, but requested GID is $requested_gid. Cannot change GID automatically."
        fi
    fi

    local group_args=("-r")
    if [ -n "$requested_gid" ]; then
        group_args+=(-g "$requested_gid")
    fi
    $group_exists || groupadd "${group_args[@]}" %{system_group} || print_error "Failed to create group %{system_group}"

    local user_args=("-r" "-g" "%{system_group}" "-s" "/sbin/nologin")
    if [ -n "$requested_uid" ]; then
        user_args+=("-u" "$requested_uid")
    fi
    $user_exists || useradd "${user_args[@]}" %{system_user} || print_error "Failed to create user %{system_user}"
}

if systemctl cat %{service_name} >/dev/null 2>&1 ; then
    systemctl stop %{service_name} || true
    systemctl disable %{service_name} || true
fi
create_user

if getent group %{ubm_group} > /dev/null; then
    usermod -aG %{ubm_group} %{system_user}
else
    echo "[WARN] Group '%{ubm_group}' not found. User '%{system_user}' was not added to this group. If UBM is required, please install the corresponding package and run: usermod -aG %{ubm_group} %{system_user}"
fi

%post
set -e
%{ensure_directory_owner}
%{deleted_semaphore}
%{update_config}
systemctl daemon-reload
ensure_directory_owner "%{log_dir}" true
ensure_directory_owner "%{data_dir}" true
ensure_directory_owner "%{data_dir}/data" true
ensure_directory_owner "%{cert_dir}" true
ensure_directory_owner "%{lcne_cert_dir}" true
ensure_directory_owner "%{vip_server_cert_dir}" true
ensure_directory_owner "%{socket_dir}" true
chmod 750 "%{log_dir}" "%{data_dir}" "%{data_dir}/data"
chmod 755 "%{socket_dir}"
chmod 700 "%{cert_dir}"
chmod 700 "%{lcne_cert_dir}"
chmod 700 "%{vip_server_cert_dir}"
%ifarch aarch64
if [ -f /lib/modules/ubse/bandbridge.ko ]; then
    mkdir -p /lib/modules/$(uname -r)/extra
    ln -sf /lib/modules/ubse/bandbridge.ko /lib/modules/$(uname -r)/extra/bandbridge.ko
    depmod -a $(uname -r)
fi
%endif
if [ "$ENABLE_AI" = "true" ]; then
 	sed -i '/^Environment=SCENE_TYPE=/s/common/ai/' /usr/lib/systemd/system/ubse.service
fi
systemctl enable %{service_name}
if [ "$MXE_SCENE" == "vm" ]; then
    update_config /etc/ubse/ubse_plugin_admission.conf
fi
deleted_semaphore


%preun
set -e
if [ "$1" -ne 0 ]; then
    exit 0
fi
%ifarch aarch64
if [ -L /lib/modules/$(uname -r)/extra/bandbridge.ko ]; then
    modprobe -r bandbridge 2>/dev/null || true
    rm -f /lib/modules/$(uname -r)/extra/bandbridge.ko
fi
%endif
if systemctl cat %{service_name} >/dev/null 2>&1 ; then
    systemctl stop %{service_name} || true
    systemctl disable %{service_name} || true
fi
if systemctl list-units --type=service | grep -q %{service_name}; then
    systemctl reset-failed %{service_name} || true
fi


%postun
if [ "$1" -ne 0 ]; then
    exit 0
fi
%{deleted_semaphore}
%{remove_directory}
%ifarch aarch64
depmod -a $(uname -r)
%endif
systemctl daemon-reload
remove_directory %{log_dir}
remove_directory %{cert_dir}
remove_directory %{socket_dir}
remove_directory %{lcne_cert_dir}
remove_directory %{vip_server_cert_dir}

deleted_semaphore
if id "%{system_user}" &>/dev/null; then
    userdel -r "%{system_user}" &>/dev/null || true
fi
if getent group "%{system_group}" &>/dev/null; then
    groupdel "%{system_group}"
fi

%files
%defattr(755,root,root,-)
/usr/bin/ubse
/usr/bin/ubsectl
%defattr(644,root,root,-)
/usr/lib/systemd/system/ubse.service
%defattr(644,root,root,755)
%dir /etc/ubse/
%config(noreplace) /etc/ubse/ubse*.conf
%dir /etc/ubse/plugins
%dir /etc/ubse/topo
%config(noreplace) /etc/ubse/topo/*.json
%defattr(755,root,root,-)
%dir /usr/lib64/ubse_plugin
/usr/lib64/ubse_plugin/libmem_plugin.so
%defattr(644,root,root,-)
/etc/bash_completion.d/cli_commands.sh
%ifarch aarch64
%defattr(644,root,root,755)
%dir /lib/modules/ubse
/lib/modules/ubse/bandbridge.ko
%endif

%files client-libs
%defattr(755,root,root,-)
/usr/lib64/libubse-client.so.1.0.1
%defattr(-,root,root,-)
/usr/lib64/libubse-client.so.1

%files client-devel
%defattr(-,root,root,-)
/usr/lib64/libubse-client.so
/usr/lib64/libubse-client.a
%defattr(644,root,root,755)
/usr/include/ubse/
%exclude /usr/include/ubse/ubs_engine_ssu.h
%exclude /usr/include/ubse/ubs_engine_npu.h

%files -n python3-%{name}
%{python3_sitelib}/ubse/
%{python3_sitelib}/ubse-%{version}*.egg-info
%exclude %{python3_sitelib}/ubse/ubs_engine_ssu.py
%exclude %{python3_sitelib}/ubse/__pycache__/ubs_engine_ssu.*
%exclude %{python3_sitelib}/ubse/ffi/ubs_engine_binding_ssu.py
%exclude %{python3_sitelib}/ubse/ffi/__pycache__/ubs_engine_binding_ssu.*
%exclude %{python3_sitelib}/ubse/models/ubs_engine_model_ssu.py
%exclude %{python3_sitelib}/ubse/models/__pycache__/ubs_engine_model_ssu.*
%exclude %{python3_sitelib}/ubse/ubs_engine_npu.py
%exclude %{python3_sitelib}/ubse/__pycache__/ubs_engine_npu.*
%exclude %{python3_sitelib}/ubse/ffi/ubs_engine_binding_npu.py
%exclude %{python3_sitelib}/ubse/ffi/__pycache__/ubs_engine_binding_npu.*
%exclude %{python3_sitelib}/ubse/models/ubs_engine_model_npu.py
%exclude %{python3_sitelib}/ubse/models/__pycache__/ubs_engine_model_npu.*

%files virtagent
%defattr(644,root,root,-)
%config(noreplace) /etc/ubse/plugins/plugin_virt_agent.conf
%config(noreplace) /etc/ubse/plugins/auth-virt_agent.conf
%defattr(755,root,root,-)
/usr/lib64/libvirtagent.so
/usr/lib64/libstrategy.so
/usr/lib64/libubs-virt-agent.so.1.0.1
%defattr(-,root,root,-)
/usr/lib64/libubs-virt-agent.so.1
/usr/lib64/libubs-virt-agent.so
%defattr(644,root,root,755)
/usr/include/virt_agent/

%files ucache
%defattr(644,root,root,-)
%config(noreplace) /etc/ubse/plugins/plugin_ucache.conf
%defattr(755,root,root,-)
/usr/lib64/libucache_plugin.so

%files rmrs
%defattr(644,root,root,-)
%config(noreplace) /etc/ubse/plugins/plugin_mempooling.conf
%defattr(755,root,root,-)
/usr/lib64/libmempooling.so
%defattr(644,root,root,755)
/usr/local/mempooling/include/mempooling/

%files processmem
%config(noreplace) %{_sysconfdir}/ubse/plugins/plugin_process_mem.conf
%{_libdir}/libprocess_mem.so

# ========================================================
#                   SSU %files group
# ========================================================
%files ssu
%defattr(755,root,root,-)
/usr/lib64/ubse_plugin/libssu_plugin.so
/usr/bin/ubsectl-ssu

%files ssu-client-libs
%defattr(755,root,root,-)
/usr/lib64/libubse-ssu-client.so.1.0.1
%defattr(-,root,root,-)
/usr/lib64/libubse-ssu-client.so.1

%files ssu-client-devel
%defattr(-,root,root,-)
/usr/lib64/libubse-ssu-client.so
/usr/lib64/libubse-ssu-client.a
%defattr(644,root,root,755)
/usr/include/ubse/ubs_engine_ssu.h

%files -n python3-%{name}-ssu
%{python3_sitelib}/ubse/ubs_engine_ssu.py
%{python3_sitelib}/ubse/__pycache__/ubs_engine_ssu.*
%{python3_sitelib}/ubse/ffi/ubs_engine_binding_ssu.py
%{python3_sitelib}/ubse/ffi/__pycache__/ubs_engine_binding_ssu.*
%{python3_sitelib}/ubse/models/ubs_engine_model_ssu.py
%{python3_sitelib}/ubse/models/__pycache__/ubs_engine_model_ssu.*
%{python3_sitelib}/ubse_ssu-%{version}*.egg-info

# ========================================================
#                   NPU %files group
# ========================================================
%files npu-client-libs
%defattr(755,root,root,-)
/usr/lib64/libubse-npu-client.so.1.0.1
%defattr(-,root,root,-)
/usr/lib64/libubse-npu-client.so.1

%files npu-client-devel
%defattr(-,root,root,-)
/usr/lib64/libubse-npu-client.so
/usr/lib64/libubse-npu-client.a
%defattr(644,root,root,755)
/usr/include/ubse/ubs_engine_npu.h

%files -n python3-%{name}-npu
%{python3_sitelib}/ubse/ubs_engine_npu.py
%{python3_sitelib}/ubse/__pycache__/ubs_engine_npu.*
%{python3_sitelib}/ubse/ffi/ubs_engine_binding_npu.py
%{python3_sitelib}/ubse/ffi/__pycache__/ubs_engine_binding_npu.*
%{python3_sitelib}/ubse/models/ubs_engine_model_npu.py
%{python3_sitelib}/ubse/models/__pycache__/ubs_engine_model_npu.*
%{python3_sitelib}/ubse_npu-%{version}*.egg-info

%files npu
%defattr(755,root,root,-)
/usr/lib64/ubse_plugin/libnpu_plugin.so