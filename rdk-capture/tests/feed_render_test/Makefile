# Build capture_session_feeder (C++, GStreamer + ICaptureSession) and capture_session_renderer
# (C, Wayland/EGL/GLES2). Cross-compile by sourcing the bitbake environment
# (CC/CXX/CFLAGS/LDFLAGS/PKG_CONFIG_*), e.g. temp/run.do_compile.

PKG_CONFIG ?= pkg-config
SRC_DIR    := ../../src
# Output directory (<workdir>/build/feed_render_test); override with OUT_DIR=...
OUT_DIR    ?= $(abspath ../../../../build/feed_render_test)

FEED_CPPFLAGS   := -I$(SRC_DIR) $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 glib-2.0)
FEED_LIBS       := $(shell $(PKG_CONFIG) --libs gstreamer-1.0 glib-2.0) -ldl

RENDER_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags wayland-client)
RENDER_LIBS     := $(shell $(PKG_CONFIG) --libs wayland-client) -lwayland-egl \
                   -Wl,--no-as-needed -lEGL -lGLESv2

all: $(OUT_DIR)/capture_session_feeder $(OUT_DIR)/capture_session_renderer

$(OUT_DIR):
	mkdir -p $@

$(OUT_DIR)/capture_session_feeder: capture_session_feeder.cpp capture_session_ipc.h $(SRC_DIR)/ICaptureSession.h | $(OUT_DIR)
	$(CXX) -std=c++11 $(CXXFLAGS) $(CPPFLAGS) $(FEED_CPPFLAGS) $< -o $@ $(LDFLAGS) $(FEED_LIBS)

$(OUT_DIR)/capture_session_renderer: capture_session_renderer.c capture_session_ipc.h | $(OUT_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(RENDER_CPPFLAGS) $< -o $@ $(LDFLAGS) $(RENDER_LIBS)

clean:
	rm -f $(OUT_DIR)/capture_session_feeder $(OUT_DIR)/capture_session_renderer

.PHONY: all clean
