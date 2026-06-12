#!/bin/bash
#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#
set -e
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPTS_DIR")")"
PROJECT_BASENAME="$(basename "$PROJECT_DIR")"
PACKAGE_NAME="ubs-engine"
SOURCE_PACKAGE="ubs-engine.tar.gz"
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
cd "$(dirname "$PROJECT_DIR")"
tar -czf "$SOURCE_PACKAGE" \
    --exclude="$PROJECT_BASENAME/cmake-build-*" \
    "$PROJECT_BASENAME"
mv "$SOURCE_PACKAGE" ~/rpmbuild/SOURCES/
cp "$PROJECT_DIR"/"$PACKAGE_NAME".spec ~/rpmbuild/SPECS/
sed -i "s/%{name}-%{version}.tar.gz/$SOURCE_PACKAGE/" ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
sed -i "s/%define project_dir %{name}-%{version}/%define project_dir $PROJECT_BASENAME/" ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
sed -i "s/BuildRequires/#BuildRequires/" ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
if [[ "$1" == '-D' ]]; then
    sed -i 's/bash build.sh -T RelWithDebInfo/bash build.sh -D/' ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
    sed -i '3a\%global debug_package %{nil}' ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
    sed -i '4a\%global __strip /bin/true' ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
    sed -i '5a\%global __objcopy /bin/true' ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
    sed -i 's/%define cmake_build_dir cmake-build-relwithdebinfo/%define cmake_build_dir cmake-build-debug/' ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
fi
if find ~/rpmbuild/RPMS/aarch64/  -type f | grep "$PACKAGE_NAME" >/dev/null 2>&1 ; then
    rm -rf ~/rpmbuild/RPMS/*/*"$PACKAGE_NAME"*.rpm
fi
rpmbuild -bb --clean ~/rpmbuild/SPECS/"$PACKAGE_NAME".spec
mkdir -p "$PROJECT_DIR"/output
rm -rf "$PROJECT_DIR"/output/*
cp -p ~/rpmbuild/RPMS/*/*"$PACKAGE_NAME"*.rpm "$PROJECT_DIR"/output/