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
################################################################################
# Build westeros-sink GStreamer plugin for all variants
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
INSTALL_DIR="${PROJECT_ROOT}/install"

cd "$PROJECT_ROOT"

# The variant build needs generated Westeros protocol headers and client libs.
# Install dependencies on demand so invoking this script directly is sufficient.
if [ ! -f /usr/local/include/simpleshell-client-protocol.h ] || \
   [ ! -f /usr/local/include/simplebuffer-client-protocol.h ]; then
    echo "Westeros protocol headers are missing; running install-dependencies.sh..."
    bash "${SCRIPT_DIR}/install-dependencies.sh"
fi
if [ ! -f /usr/local/include/simpleshell-client-protocol.h ] || \
   [ ! -f /usr/local/include/simplebuffer-client-protocol.h ]; then
    echo "ERROR: Westeros SimpleShell/SimpleBuffer protocol headers are still missing."
    echo "Check install-dependencies.sh output and verify the Westeros components installed under /usr/local/include."
    exit 1
fi

echo "=========================================="
echo " Building Westeros-Sink"
echo "=========================================="
echo ""

#Step 1: Check and install dependencies
sudo mkdir -p /usr/local/include/IL
sudo cp -f "${PROJECT_ROOT}/utilities/IL/OMX_Core.h" /usr/local/include/IL/ 2>/dev/null || true
sudo cp -f "${PROJECT_ROOT}/utilities/IL/OMX_Broadcom.h" /usr/local/include/IL/ 2>/dev/null || true
echo "✓ All stub headers installed"
echo ""


# Step 2: Get compiler flags 
# Use pkg-config to get all necessary compiler and linker flags for GStreamer, Wayland, and DRM
GSTREAMER_CFLAGS=$(pkg-config --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0)
GSTREAMER_LIBS=$(pkg-config --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0)
WAYLAND_CFLAGS=$(pkg-config --cflags wayland-client wayland-server)
WAYLAND_LIBS=$(pkg-config --libs wayland-client wayland-server)
DRM_CFLAGS=$(pkg-config --cflags libdrm)
DRM_LIBS=$(pkg-config --libs libdrm)

# Combine all compiler flags into one variable
COMMON_FLAGS="-fPIC -D_FILE_OFFSET_BITS=64 -DUSE_GST1 -DUSE_GST_VIDEO -DUSE_GST_ALLOCATORS -DNATIVE_BUILD"
COMMON_FLAGS="$COMMON_FLAGS -DVERSION=\"1.0.0\" -DPACKAGE_NAME=\"westeros-sink\""
COMMON_FLAGS="$COMMON_FLAGS -Wno-unused-but-set-variable -Wno-format-truncation -Wno-deprecated-declarations"
# Add include paths for source, utilities, GStreamer, GLib, and DRM
COMMON_FLAGS="$COMMON_FLAGS -I${PROJECT_ROOT} -I${PROJECT_ROOT}/westeros-cloned/simpleshell/protocol -I${PROJECT_ROOT}/westeros-cloned/simplebuffer/protocol -I/usr/local/include/simpleshell/protocol -I${PROJECT_ROOT}/utilities -I/usr/include/gstreamer-1.0 -I/usr/include/gstreamer-1.0/gst/video -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/libdrm"
# Add pkg-config flags
COMMON_FLAGS="$COMMON_FLAGS $GSTREAMER_CFLAGS $WAYLAND_CFLAGS $DRM_CFLAGS"

# Combine all linker flags into one variable
LINK_LIBS="$GSTREAMER_LIBS $WAYLAND_LIBS $DRM_LIBS -L/usr/local/lib -lwesteros_simplebuffer_client -lwesteros_simpleshell_client -lgbm -lEGL -lGLESv2 -ldl -lpthread"
# Software decoder is optional and only enabled when the FFmpeg development toolchain is available.
SW_DECODER_FLAGS=""
SW_DECODER_LIBS=""
if pkg-config --exists libavcodec libavutil; then
    SW_DECODER_FLAGS="-DENABLE_SW_DECODE"
    SW_DECODER_LIBS="-lavcodec -lavutil"
else
    echo "NOTE: FFmpeg development packages are not installed; skipping software-decoder variants."
fi

# Build function
build_variant() {
    # Arguments
    local VARIANT=$1
    local VARIANT_FLAGS=$2
    local VARIANT_LIBS=${3:-""}
    local EXTRA_INCLUDE=${4:-""}

    echo "Building $VARIANT variant..."
    mkdir -p "${BUILD_DIR}/${VARIANT}"

    # Combine all flags for compilation
    local COMPILE_FLAGS="$COMMON_FLAGS $VARIANT_FLAGS $PKG_CFLAGS -I/usr/local/include $EXTRA_INCLUDE"

    # Compile each source file separately
    echo "Compiling westeros-sink.c..."
    gcc -c westeros-sink.c -o "${BUILD_DIR}/${VARIANT}/westeros-sink.o" $COMPILE_FLAGS -I${VARIANT} || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling ${VARIANT}/westeros-sink-soc.c..."
    gcc -c ${VARIANT}/westeros-sink-soc.c -o "${BUILD_DIR}/${VARIANT}/westeros-sink-soc.o" $COMPILE_FLAGS -I${VARIANT} || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling pipeline_logger.cpp..."
    g++ -c pipeline_logger.cpp -o "${BUILD_DIR}/${VARIANT}/pipeline_logger.o" $COMPILE_FLAGS -I${VARIANT} -Wno-format || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling essos_resmgr_stubs.c..."
    gcc -c L1/mocks/essos_resmgr_stubs.c -o "${BUILD_DIR}/${VARIANT}/essos_resmgr_stubs.o" $COMPILE_FLAGS || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling wayland_stubs.c..."
    gcc -c L1/mocks/wayland_stubs.c -o "${BUILD_DIR}/${VARIANT}/wayland_stubs.o" $COMPILE_FLAGS || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling gst_dmabuf_stubs.c..."
    gcc -c L1/mocks/gst_dmabuf_stubs.c -o "${BUILD_DIR}/${VARIANT}/gst_dmabuf_stubs.o" $COMPILE_FLAGS || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling platform_stubs.c..."
    gcc -c L1/mocks/platform_stubs.c -o "${BUILD_DIR}/${VARIANT}/platform_stubs.o" $COMPILE_FLAGS || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    echo "Compiling main.c..."
    gcc -c L1/mocks/main.c -o "${BUILD_DIR}/${VARIANT}/main.o" $COMPILE_FLAGS || { echo "    ✗ FAILED"; BUILD_FAILED=1; }

    # Check if all object files were created
    local OBJ_COUNT=$(ls "${BUILD_DIR}/${VARIANT}"/*.o 2>/dev/null | wc -l)
    echo "  Object files: $OBJ_COUNT/8"

    # Link the shared library and executable
    if [ "$OBJ_COUNT" -eq 8 ]; then
        echo "Linking libgstwesterossink.so..."
        g++ -shared -o "${BUILD_DIR}/${VARIANT}/libgstwesterossink.so" \
            "${BUILD_DIR}/${VARIANT}/westeros-sink.o" \
            "${BUILD_DIR}/${VARIANT}/westeros-sink-soc.o" \
            "${BUILD_DIR}/${VARIANT}/pipeline_logger.o" \
            "${BUILD_DIR}/${VARIANT}/essos_resmgr_stubs.o" \
            "${BUILD_DIR}/${VARIANT}/wayland_stubs.o" \
            $LINK_LIBS $VARIANT_LIBS && \
            mkdir -p "${INSTALL_DIR}/lib/gstreamer-1.0/${VARIANT}" && \
            cp "${BUILD_DIR}/${VARIANT}/libgstwesterossink.so" "${INSTALL_DIR}/lib/gstreamer-1.0/${VARIANT}/" && \
            echo "✓ $VARIANT .so built successfully" || { echo "✗ $VARIANT .so linking FAILED"; BUILD_FAILED=1; }
        
        echo "Linking westeros-sink-${VARIANT} binary..."
        g++ -o "${BUILD_DIR}/${VARIANT}/westeros-sink-${VARIANT}" \
            "${BUILD_DIR}/${VARIANT}/westeros-sink.o" \
            "${BUILD_DIR}/${VARIANT}/westeros-sink-soc.o" \
            "${BUILD_DIR}/${VARIANT}/pipeline_logger.o" \
            "${BUILD_DIR}/${VARIANT}/essos_resmgr_stubs.o" \
            "${BUILD_DIR}/${VARIANT}/wayland_stubs.o" \
            "${BUILD_DIR}/${VARIANT}/gst_dmabuf_stubs.o" \
            "${BUILD_DIR}/${VARIANT}/platform_stubs.o" \
            "${BUILD_DIR}/${VARIANT}/main.o" \
            $LINK_LIBS $VARIANT_LIBS && \
            mkdir -p "${INSTALL_DIR}/bin" && \
            cp "${BUILD_DIR}/${VARIANT}/westeros-sink-${VARIANT}" "${INSTALL_DIR}/bin/" && \
            chmod +x "${INSTALL_DIR}/bin/westeros-sink-${VARIANT}" && \
            echo "✓ $VARIANT binary built successfully" || { echo "✗ $VARIANT binary linking FAILED"; BUILD_FAILED=1; }
    else
        echo "✗ $VARIANT incomplete (missing object files)"
        BUILD_FAILED=1
    fi
    echo ""
}

# Step 3: Build all variants

BUILD_FAILED=0
rm -rf "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

# Build variants
if [ -n "$SW_DECODER_FLAGS" ]; then
    build_variant "brcm" "$SW_DECODER_FLAGS" "$SW_DECODER_LIBS" "-I${PROJECT_ROOT}/westeros-cloned/test/brcm-em/include"
    build_variant "v4l2" "$SW_DECODER_FLAGS -DUSE_GST_AFD" "$SW_DECODER_LIBS" "-Iv4l2 -Iv4l2/svp"
else
    echo "NOTE: Skipping BRCM and V4L2 software-decoder variants because FFmpeg dev packages are unavailable."
fi
build_variant "drm" "" "" "-I/usr/include/libdrm"
build_variant "emu" "-DEMU" ""
build_variant "icegdl" "" ""
build_variant "rpi" "" ""
build_variant "raw" "" ""

# Build Summary
echo "=========================================="
echo " Build Complete"
echo "=========================================="
echo ""
echo "Built libraries: ${INSTALL_DIR}/lib/gstreamer-1.0/"
echo "Built binaries: ${INSTALL_DIR}/bin/"
echo ""

exit "$BUILD_FAILED"
