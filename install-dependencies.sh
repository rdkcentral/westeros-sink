#!/bin/bash
#
# Copyright (C) 2016 RDK Management
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#
# Install dependencies for westeros-sink and Westeros components

set -e

echo "=========================================="
echo " Installing Dependencies"
echo "=========================================="


for i in 1 2 3; do
    sudo apt-get update && break || {
        echo "apt-get update failed, attempt $i/3"
        [ $i -lt 3 ] && sleep 10 || exit 1
    }
done

sudo apt-get install -y -qq \
    build-essential \
    autoconf \
    automake \
    libtool \
    pkg-config

sudo apt-get install -y -qq \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-tools

sudo apt-get install -y -qq \
    libwayland-dev \
    wayland-protocols \
    libwayland-client0 \
    libwayland-server0

sudo apt-get install -y -qq \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    libgl1-mesa-dev \
    freeglut3-dev \
    libglew-dev \
    libgbm-dev \
    libdrm-dev

sudo apt-get install -y -qq \
    libglib2.0-dev \
    libxkbcommon-dev \
    libavcodec-dev \
    libavutil-dev

# Helper function for sparse checkout with token support
fetch_sparse_checkout() {
    local repo_url=$1
    local file_path=$2
    local dest_dir=$3
    local branch=$4
    local token=${5:-""}
    local file_name=$(basename "$file_path")
    echo "Fetching $file_name from repository..."
    if [ ! -d "$dest_dir" ]; then
        mkdir -p "$dest_dir"
        cd "$dest_dir"
        git init
        
        # Add token to URL if provided and it's a GitHub URL
        if [ -n "$token" ] && [[ "$repo_url" == *"github.com"* ]]; then
            repo_url="https://${token}@github.com/${repo_url#https://github.com/}"
        fi
        
        git remote add origin "$repo_url"
        git config core.sparseCheckout true
        echo "$file_path" > .git/info/sparse-checkout
        git pull --depth=1 origin "$branch" || {
            echo "WARNING: Failed to fetch $file_name from repository"
            rm -rf "$dest_dir"
        }
        cd - >/dev/null
    fi
}

# Clone and build Westeros main component
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GITHUB_TOKEN="${GITHUB_TOKEN:-}"
if [ ! -f "/usr/local/include/simpleshell-client-protocol.h" ]; then
    echo "Building Westeros components..."
    WESTEROS_ROOT="${SCRIPT_DIR}/westeros-cloned"
    if [ ! -d "$WESTEROS_ROOT" ]; then
        echo "Cloning Westeros repository from GitHub..."
        WESTEROS_URL="https://github.com/rdkcentral/westeros.git"
        
        # Add token to URL if provided for authentication
        if [ -n "$GITHUB_TOKEN" ]; then
            WESTEROS_URL="https://${GITHUB_TOKEN}@github.com/rdkcentral/westeros.git"
        fi
        
        git clone "$WESTEROS_URL" "$WESTEROS_ROOT" || {
            echo "ERROR: Failed to clone Westeros repository"
            exit 1
        }
    fi
    cd "$WESTEROS_ROOT"
    # Checkout specific commit
    echo "Checking out commit fc75b3cc41948593c67c8c476ace651657af3c03..."
    git checkout fc75b3cc41948593c67c8c476ace651657af3c03 || {
        echo "ERROR: Failed to checkout specified commit"
        exit 1
    }
    bash scripts/build-components.sh
    cd "$SCRIPT_DIR"
    echo "✓ Westeros components built"
else
    echo "✓ Westeros headers already installed"
fi

# Fetch additional headers via sparse checkout
fetch_sparse_checkout "https://code.rdkcentral.com/r/components/opensource/westeros" "essos/essos-resmgr.h" "${SCRIPT_DIR}/westeros-minimal" "master" "$GITHUB_TOKEN"
fetch_sparse_checkout "https://github.com/rdkcentral/westeros-gl-drm.git" "westeros-gl/westeros-gl.h" "${SCRIPT_DIR}/westeros-gl-drm-minimal" "main" "$GITHUB_TOKEN"

# Install stub headers
echo "Installing stub headers..."
HEADERS=(
    "nexus_platform.h" "nexus_config.h" "nexus_surface_client.h"
    "nexus_core_utils.h" "nexus_stc_channel.h" "nexus_simple_video_decoder.h"
    "nxclient.h" "default_nexus.h" "ismd_core.h" "ismd_vidrend.h"
    "ismd_vidpproc.h" "ismd_vidsink.h" "libgdl.h" "gdl_types.h"
    "icegdl-client-protocol.h" "resourcemanage.h" "bcm_host.h" "gst_video_afd.h"
)
for header in "${HEADERS[@]}"; do
    sudo cp -f "${SCRIPT_DIR}/utilities/${header}" /usr/local/include/ 2>/dev/null || true
done
sudo mkdir -p /usr/local/include/IL
sudo cp -f "${SCRIPT_DIR}/utilities/IL/OMX_Core.h" /usr/local/include/IL/ 2>/dev/null || true
sudo cp -f "${SCRIPT_DIR}/utilities/IL/OMX_Broadcom.h" /usr/local/include/IL/ 2>/dev/null || true
sudo cp -f "${SCRIPT_DIR}/westeros-gl-drm-minimal/westeros-gl/westeros-gl.h" /usr/local/include/ 2>/dev/null || \
    sudo cp -f "${SCRIPT_DIR}/westeros-gl.h" /usr/local/include/ 2>/dev/null || true
sudo cp -f "${SCRIPT_DIR}/westeros-minimal/essos/essos-resmgr.h" /usr/local/include/ 2>/dev/null || true
echo "✓ All stub headers installed"
