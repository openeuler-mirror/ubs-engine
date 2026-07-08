#!/usr/bin/env bash
#
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#

# Generate code coverage report using fastcov and lcov
# Usage: ./coverage.sh [build_dir]
# Environment variables:
#   COVERAGE_MODULE - module name for filter selection (ut|it|base|rmrs|ucache|virt-agent)

set -o errexit    # Exit immediately on any command failure
set -o nounset    # Treat unset variables as errors
set -o pipefail   # Catch failures in pipes

# --------------------------
# Configuration variables
# --------------------------
declare -r DEFAULT_BUILD_DIR="${PWD}/cmake-build-debug"
declare -r DEFAULT_LCOVRC="${PWD}/.lcovrc"
declare -r coverage_dir="${1:-${DEFAULT_BUILD_DIR}}/coverage"
declare -r FASTCOV_PATH="${HOME}/.local/bin/fastcov"

# --------------------------
# Module mapping table
# --------------------------
declare -A MODULE_INCLUDE=(
    ["ut"]="src/"
    ["it"]="src/"
    ["ubs-engine_ut_independent_ut"]="src/"
    ["ubs-engine_virtagent_ut_independent_ut"]="src/addons/virt_agent"
    ["ubs-engine_rmrs_plugin_ut_independent_ut"]="src/addons/rmrs"
    ["ubs-engine_ucache_plugin_ut_independent_ut"]="src/addons/ucache"
    ["ubs-engine_process_mem_ut_independent_ut"]="src/addons/process_mem"
)

COMMON_EXCLUDE=".h src/main/ test/ cmake-build-debug/"
ADDON_EXCLUDE="src/addons/rmrs src/addons/ucache src/addons/virt_agent"

declare -A MODULE_EXCLUDE=(
    ["ut"]="${COMMON_EXCLUDE}"
    ["it"]="${COMMON_EXCLUDE} test/UT/"  # Exclude UT code from IT coverage report
    ["ubs-engine_ut_independent_ut"]="${COMMON_EXCLUDE} ${ADDON_EXCLUDE}"
    ["ubs-engine_virtagent_ut_independent_ut"]="${COMMON_EXCLUDE}"
    ["ubs-engine_rmrs_plugin_ut_independent_ut"]="${COMMON_EXCLUDE}"
    ["ubs-engine_ucache_plugin_ut_independent_ut"]="${COMMON_EXCLUDE}"
    ["ubs-engine_process_mem_ut_independent_ut"]="${COMMON_EXCLUDE}"
)

function resolve_coverage_filters() {
    # 从环境变量 COVERAGE_MODULE 获取模块名称，若未设置则使用默认值 "ut"
    local module="${COVERAGE_MODULE:-ut}"
    include_patterns="${MODULE_INCLUDE[${module}]:-}"
    exclude_patterns="${MODULE_EXCLUDE[${module}]:-}"
}

##
# Generate coverage report using fastcov and genhtml
# Arguments:
#   $1 - output directory for coverage report
#   $2 - path to .lcovrc configuration file
##
function generate_coverage_report() {
    local output_dir="$1"
    local lcovrc_file="$2"

    # Validate required tools
    local genhtml_path
    genhtml_path="$(command -v genhtml)" || error_exit "genhtml not found in PATH"

    # Create clean output directory
    mkdir -p "${output_dir}"

    # Generate intermediate coverage info
    "${FASTCOV_PATH}" -b -n -p \
        --include ${include_patterns} \
        --exclude ${exclude_patterns} \
        --lcov -o "${output_dir}/fastcov.info"

    # Generate HTML report
    "${genhtml_path}" -q --config-file "${lcovrc_file}" \
        -o "${output_dir}" "${output_dir}/fastcov.info"
}

##
# Print coverage summary to console
# Arguments:
#   $1 - path to coverage info file
#   $2 - path to .lcovrc configuration file
##
function print_coverage_summary() {
    local info_file="$1"
    local lcovrc_file="$2"
    local lcov_path
    lcov_path="$(command -v lcov)" || error_exit "lcov not found in PATH"

    echo ""
    echo "=========================================="
    echo "         Coverage Summary Report"
    echo "=========================================="
    "${lcov_path}" --summary --config-file "${lcovrc_file}" "${info_file}" 2>&1 | grep -E "(lines|functions|branches)" || true
    echo "=========================================="
}

##
# Display error message and exit
# Arguments:
#   $1 - error message
##
function error_exit() {
    echo "[ERROR] $1" >&2
    exit 1
}

##
# Ensure fastcov installed
# GloGlobals::
#   FASTCOV_PATH (input)
##
ensure_fastcov_installed() {
    # 检查两种安装方式的存在性（PATH 和指定路径）
    if ! command -v fastcov >/dev/null 2>&1 && \
       [[ ! -f "${FASTCOV_PATH}" ]]; then
        echo "Installing fastcov via pip..."
        if ! pip3 install fastcov \
               --disable-pip-version-check \
               --user \
        ; then
            error_exit "Failed to install fastcov. Check network connection."
        fi
    fi
}

# --------------------------
# Main execution
# --------------------------
function main() {
    local lcovrc_file="${DEFAULT_LCOVRC}"
    local include_patterns=""
    local exclude_patterns=""

    ensure_fastcov_installed || error_exit "Missing fastcov installed"

    [[ -f "${lcovrc_file}" ]] || error_exit "Missing lcovrc file: ${lcovrc_file}"
    [[ -f "${FASTCOV_PATH}" ]] || error_exit "Missing fastcov script: ${FASTCOV_PATH}"

    resolve_coverage_filters
    generate_coverage_report "${coverage_dir}" "${lcovrc_file}"

    print_coverage_summary "${coverage_dir}/fastcov.info" "${lcovrc_file}"
    echo ""
    echo "Coverage module: ${COVERAGE_MODULE:-base}"
    echo "Coverage report generated at: ${coverage_dir}/index.html"
}

main "$@"