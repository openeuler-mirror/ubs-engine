# Restore old style debuginfo creation for rpm >= 4.14.
%undefine _debugsource_packages
%undefine _debuginfo_subpackages

# -*- rpm-spec -*-
Summary:        RPM package
Name:           ubs-engine
Version:        1.0.0
Release:        1
License:        MIT
Source0:        %{name}.tar.gz
Group:          System Environment/Base
Vendor:         Huawei Technologies Co., Ltd.
Prefix: /usr

BuildRequires: cmake make gcc-c++ gcc
BuildRequires: glibc-devel libstdc++-devel
BuildRequires: systemd-devel
BuildRequires: audit-devel libboundscheck ubs-hcom-devel libxml2-devel
BuildRequires: numactl-libs
BuildRequires: bash bc coreutils findutils gawk grep kmod lsof libcap sed sudo util-linux vim util-linux-user cpio tar unzip mlocate ninja-build ccache java-11-openjdk python3-pre-commit libffi-devel zlib-devel clang-devel
Requires: bash glibc libgcc libstdc++ obmm libboundscheck libxml2 ubs-hcom

%define _rpmdir %_topdir/RPMS
%define _srcrpmdir %_topdir/SRPMS
%define _unpackaged_files_terminate_build 0

%description
UBS Engine


# ========================================================
#                   SUBPACKAGE: ubs-engine-client-libs
# ========================================================
%define lib_name libubse-client
%define lib_soversion 1

%package client-libs
Summary: UBSE client shared library for third-party integration
Provides: %{lib_name}.so.%{lib_soversion}
Requires: libboundscheck, libstdc++
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


%define project_dir %{name}
%define cmake_build_dir cmake-build-release

%define log_dir /var/log/ubse
%define data_dir /var/lib/ubse
%define cert_dir /var/lib/ubse/cert
%define socket_dir /var/run/ubse

%define system_user ubse
%define system_group ubse
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

# Function to check the file and modify its content
%define modify_udev_rule() modify_udev_rule() { \
    local rules_file="/etc/udev/rules.d/99-obmm.rules" \
    local old_content='KERNEL=="obmm", OWNER="root", GROUP="root", MODE="0600"' \
    local new_content='KERNEL=="obmm", OWNER="%{system_user}", GROUP="%{system_group}", MODE="0600"' \
    if [[ -f "$rules_file" ]]; then \
        sed -i "s|$old_content|$new_content|" "$rules_file" \
    fi \
}

# Function to check the file and restore its content
%define restore_udev_rule() restore_udev_rule() { \
    local rules_file="/etc/udev/rules.d/99-obmm.rules" \
    local old_content='KERNEL=="obmm", OWNER="%{system_user}", GROUP="%{system_group}", MODE="0600"' \
    local new_content='KERNEL=="obmm", OWNER="root", GROUP="root", MODE="0600"' \
    if [[ -f "$rules_file" ]]; then \
        sed -i "s|$old_content|$new_content|" "$rules_file" \
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
bash build.sh 3rdparty
bash build.sh

%install
#install main package
mkdir -p %{buildroot}/usr/bin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/bin/ubse %{buildroot}/usr/bin
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/bin/ubsectl %{buildroot}/usr/bin

mkdir -p %{buildroot}/usr/lib/systemd/system/
cp %{_builddir}/%{project_dir}/scripts/rpm/%{service_name} %{buildroot}/usr/lib/systemd/system/

mkdir -p %{buildroot}/etc/ubse/
cp %{_builddir}/%{project_dir}/%{cmake_build_dir}/conf/*.conf %{buildroot}/etc/ubse/

mkdir -p %{buildroot}/etc/bash_completion.d/
cp -f %{_builddir}/%{project_dir}/scripts/command_completion/cli_commands.sh %{buildroot}/etc/bash_completion.d/

#install client-libs
cmake --install %{_builddir}/%{project_dir}/%{cmake_build_dir} \
    --component ubse_sdk \
    --prefix %{buildroot}/usr
ln -sf libubse-client.so.1.0.0 %{buildroot}/usr/lib64/libubse-client.so.1

#install client-devel
ln -sf libubse-client.so.1 %{buildroot}/usr/lib64/libubse-client.so
chmod 644 %{buildroot}/usr/lib64/libubse-client.a


%pre
set -e
if ! getent group %{system_group} > /dev/null; then
    groupadd -r %{system_group}
fi
if ! getent passwd %{system_user} > /dev/null; then
    useradd -r -g %{system_group} -s /sbin/nologin %{system_user}
fi
if systemctl cat %{service_name} >/dev/null 2>&1 ; then
    systemctl stop %{service_name} || true
    systemctl disable %{service_name} || true
fi


%post
set -e
%{ensure_directory_owner}
%{modify_udev_rule}
%{deleted_semaphore}
systemctl daemon-reload
ensure_directory_owner "%{log_dir}" true
ensure_directory_owner "%{data_dir}" true
ensure_directory_owner "%{cert_dir}" true
ensure_directory_owner "%{socket_dir}" true
chmod 750 "%{log_dir}" "%{data_dir}" "%{socket_dir}"
chmod 700 "%{cert_dir}"
systemctl enable %{service_name}
modify_udev_rule
deleted_semaphore


%preun
set -e
if [ "$1" -ne 0 ]; then
    echo "skip preun"
    exit 0
fi
%{restore_udev_rule}
if systemctl cat %{service_name} >/dev/null 2>&1 ; then
    systemctl stop %{service_name} || true
    systemctl disable %{service_name} || true
fi
if systemctl list-units --type=service | grep -q %{service_name}; then
    systemctl reset-failed %{service_name} || true
fi
restore_udev_rule


%postun
if [ "$1" -ne 0 ]; then
    echo "skip postun"
    exit 0
fi
%{deleted_semaphore}
%{remove_directory}
systemctl daemon-reload
remove_directory %{log_dir}
remove_directory %{cert_dir}
remove_directory %{socket_dir}
deleted_semaphore
if id %{system_user} &>/dev/null; then
    userdel -r %{system_user} &>/dev/null || true
fi
if getent group %{system_group} &>/dev/null; then
    groupdel %{system_group}
fi


%files
%defattr(755,root,root,-)
/usr/bin/ubse
%defattr(755,root,root,-)
/usr/bin/ubsectl
%defattr(644,root,root,-)
/usr/lib/systemd/system/ubse.service
%defattr(644,root,root,755)
%dir /etc/ubse/
%config(noreplace) /etc/ubse/ubse.conf
%defattr(644,root,root,-)
/etc/bash_completion.d/cli_commands.sh

%files client-libs
%defattr(755,root,root,-)
/usr/lib64/libubse-client.so.1.0.0
%defattr(-,root,root,-)
/usr/lib64/libubse-client.so.1

%files client-devel
%defattr(-,root,root,-)
/usr/lib64/libubse-client.so
/usr/lib64/libubse-client.a
%defattr(644,root,root,755)
/usr/include/ubse/
