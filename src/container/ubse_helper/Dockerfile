# syntax=docker/dockerfile:1.11.1

#######################################################################
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# ubs-engine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#######################################################################

ARG BUILDER=golang
ARG BUILDER_VERSION=1.24.5-oe-24.03-lts-sp3   # 构建器 Go 版本(1.24.5)高于 go.mod 声明的 1.24.1,可直接编译
# Use the prebuilt openFuyao builder image to reduce build time.
ARG BUILDER_IMAGE=cr.openfuyao.cn/openfuyao/$BUILDER:$BUILDER_VERSION

# 阶段一：编译(纯 Go,CGO_ENABLED=0 静态链接;构建上下文为仓库根目录)
FROM ${BUILDER_IMAGE} AS build
USER 0
WORKDIR /go/src/app
ARG TARGETOS=linux
ARG TARGETARCH=arm64
# 仓库根目录:满足 go.mod replace => ../../..
COPY . .

RUN --mount=type=cache,target=/go/pkg/mod,sharing=locked \
    <<'EOF'
#!/bin/bash -xeu
export GOPROXY="https://goproxy.cn,https://goproxy.io,https://proxy.golang.org,direct"
# client-go 版本注入(rest.UserAgent 依赖 version.Get,注入规避 -trimpath/-s/-w 下版本探测缺失)
export KUBE_TAG="1.34.3-of.1"
export CLIENT_GO_VERSION="-X 'k8s.io/client-go/pkg/version.getVersion=${KUBE_TAG}'"
export COMPONENT_BASE_VERSION="-X 'k8s.io/component-base/version.getVersion=${KUBE_TAG}'"
export GO_BUILD_FLAGS='-v -buildmode=pie -a -trimpath'
export LDFLAGS='-w -s -buildid none'
cd src/container/ubse_helper
# go.sum 已随仓库提交,以只读模式校验并下载锁定版本的依赖(支持 GOPROXY=off 离线构建)
go mod download
mkdir -p /out
CGO_ENABLED=0 GOOS=${TARGETOS} GOARCH=${TARGETARCH} \
  go build -mod=readonly ${GO_BUILD_FLAGS} -o /out/ubse-helper \
  -ldflags "${CLIENT_GO_VERSION} ${COMPONENT_BASE_VERSION} ${LDFLAGS}" ./cmd/ubse-helper
EOF

# 阶段二：运行(openEuler ARM64,非 root)
FROM hub.oepkgs.net/openeuler/openeuler:24.03-lts-sp1 AS release
# uid/gid 固定 983(对齐宿主机 ubse 用户与 kHelperUid,非配置项);离线追加 /etc/passwd、/etc/group 创建用户,避免 dnf 拉取 shadow 拖慢构建
RUN echo 'ubse:x:983:983::/:/sbin/nologin' >> /etc/passwd \
    && echo 'ubse:x:983:' >> /etc/group
COPY --link --from=build --chown=983:983 --chmod=500 /out/ubse-helper /usr/local/bin/ubse-helper
USER 983:983
ENTRYPOINT ["/usr/local/bin/ubse-helper"]