ARG BASE_IMAGE=hub.oepkgs.net/openeuler/openeuler:24.03-lts-sp3
FROM ${BASE_IMAGE}

ARG BUILD_TYPE=Release
ARG ENABLE_UT=OFF
ARG JOBS=8
ARG REPO_DIR=/workspace

RUN dnf install -y \
        gcc gcc-c++ make cmake git python3 python3-pip \
        "dnf-command(builddep)" \
        glibc-devel libstdc++-devel systemd-devel \
        libboundscheck libxml2-devel openssl-devel cpp-httplib-devel rapidjson-devel \
        ubs-comm-devel numactl-libs numactl-devel ninja-build \
        libvirt-devel kernel-devel \
        gtest gtest-devel gmock gmock-devel \
        python3-setuptools util-linux-user patch bc bash coreutils sudo tar \
    && dnf clean all

WORKDIR ${REPO_DIR}
COPY . ${REPO_DIR}

RUN bash build.sh -T ${BUILD_TYPE} -j ${JOBS} \
    && if [ "${ENABLE_UT}" = "ON" ]; then \
           bash build.sh ut --skip-run-tests -j ${JOBS} || true; \
           bash build.sh ut -j ${JOBS} || true; \
       fi \
    && BUILD_DIR="cmake-build-$(echo "${BUILD_TYPE}" | tr 'A-Z' 'a-z')" \
    && cmake --install "${BUILD_DIR}" --component ubse_sdk --prefix /usr

WORKDIR ${REPO_DIR}
CMD ["/bin/bash"]
