#!/bin/bash

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

set -e

# Check if a version file path is provided as an argument
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <path_to_version_file> [--pure] [--hccs]"
    exit 1
fi

# Define the path to the VERSION file from the first argument
VERSION_FILE="$1"

# Initialize flags
PURE=false
HCCS=false

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --pure)
            PURE=true
            ;;
        --hccs)
            HCCS=true
            ;;
        *)
            # Skip unknown arguments
            ;;
    esac
done

# Function to write only the commit ID to the VERSION file
write_pure_version() {
    # Get the latest commit information
    local commit_info
    commit_info=$(git log -1 --pretty=format:"%H")  || echo ""
    {
      if [ "$HCCS" = true ];
      then
        echo "[HCCS]"
      else
        echo "[UB]"
      fi
      echo "$commit_info"
    } > "$VERSION_FILE"
    echo "Commit ID written to $VERSION_FILE"
}

# Function to write detailed information to the VERSION file
write_verbose_version() {
    # Get the latest commit information
    local commit_info
    commit_info=$(git log -1 --pretty=format:"%H %s %an <%ae>") || echo ""

    # Get the packager's information
    local packager_name
    local packager_email
    packager_name=$(git config user.name) || echo ""
    packager_email=$(git config user.email) || echo ""

    # Get the current date and time
    local build_time
    build_time=$(date +"%Y-%m-%d %H:%M:%S")

    # Write the commit information, packager's information, and build time to the VERSION file
    {
        if [ "$HCCS" = true ];
        then
          echo "[HCCS]"
        else
          echo "[UB]"
        fi
        echo "Commit info: $commit_info"
        echo "Packager: $packager_name <$packager_email>"
        echo "Build time: $build_time"
    } > "$VERSION_FILE"

    # Check if the working directory has uncommitted changes
    if [[ -n $(git status --porcelain) ]]; then
        # If there are uncommitted changes, get the list of dirty files
        local dirty_files
        dirty_files=$(git status --porcelain)
        echo -e "\n\nDirty files:" >> "$VERSION_FILE"
        echo "$dirty_files" >> "$VERSION_FILE"

        # Get the diff of the dirty files
        echo -e "\n\nDiff of dirty files:" >> "$VERSION_FILE"
        git diff HEAD --color >> "$VERSION_FILE"
    fi

    if [[ -n $(git ls-files --others --exclude-standard) ]]; then
        git ls-files --others --exclude-standard | xargs cat >> "$VERSION_FILE"
    fi
    echo "Commit ID written to $VERSION_FILE"
}

# Call the appropriate function based on the mode
if [ "$PURE" = true ]; then
    write_pure_version
else
    write_verbose_version
fi
