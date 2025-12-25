set -e
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_DIR=$(dirname "$(dirname "$SCRIPTS_DIR")")
PROJECT_BASENAME="$(basename "$PROJECT_DIR")"
PACKAGE_NAME="ubs-engine"
TAR_NAME="$PACKAGE_NAME-1.0.0"
mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
cd "$(dirname "$PROJECT_DIR")"
tar -czf $TAR_NAME.tar.gz \
    --exclude="$PROJECT_BASENAME/cmake-build-*" \
    --exclude="$PROJECT_BASENAME/.git" \
    --exclude="$PROJECT_BASENAME/.deps" \
    "$PROJECT_BASENAME"
mv $TAR_NAME.tar.gz ~/rpmbuild/SOURCES/
cp "$PROJECT_DIR"/$PACKAGE_NAME.spec ~/rpmbuild/SPECS/
sed -i "s/%define project_dir %{name}-%{version}/%define project_dir $PROJECT_BASENAME/" ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
if [[ "$1" == '-D' ]]; then
    sed -i 's/bash build.sh/bash build.sh -D/' ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
    sed -i '3a\%global debug_package %{nil}' ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
    sed -i '4a\%global __strip /bin/true' ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
    sed -i '5a\%global __objcopy /bin/true' ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
    sed -i 's/%define cmake_build_dir cmake-build-release/%define cmake_build_dir cmake-build-debug/' ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
fi
if find ~/rpmbuild/RPMS/aarch64/  -type f |grep $PACKAGE_NAME >/dev/null 2>&1 ; then
    rm -rf ~/rpmbuild/RPMS/aarch64/$PACKAGE_NAME*.rpm
fi
rpmbuild -bb --clean ~/rpmbuild/SPECS/$PACKAGE_NAME.spec
mkdir -p "$PROJECT_DIR"/output
rm -rf "$PROJECT_DIR"/output/*
cp -p ~/rpmbuild/RPMS/aarch64/$PACKAGE_NAME*.rpm "$PROJECT_DIR"/output/