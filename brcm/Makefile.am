#
# Copyright (C) 2016 RDK Management
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
include ${PKG_CONFIG_SYSROOT_DIR}/usr/include/platform_app.inc

AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS = 

REFSW_VERSION ?= refsw14.4

GST_BASE_CFLAGS ?=
GST_BASE_LIBS ?=
LIB_SUB_DIR= gstreamer-0.10

AM_CFLAGS = $(NEXUS_CFLAGS) $(NXCLIENT_CFLAGS) $(GST_CFLAGS) -Wno-unused-but-set-variable -Wno-format-truncation

AM_LDFLAGS = $(GST_LIBS)  $(GSTBASE_LIBS) $(GSTVIDEO_LIBS) $(WAYLANDLIB) -avoid-version

if HAVE_GST_VIDEO
AM_CFLAGS += -DUSE_GST_VIDEO
endif

plugin_LTLIBRARIES = libgstwesterossink.la

lib_LTLIBRARIES = libcapture-broadcom.la

# ICaptureSession.h and capture-sink-bridge.h come from the shared rdk-capture sources
RDK_CAPTURE_INCLUDES = -I$(srcdir)/../rdk-capture/src

noinst_HEADERS = capture-broadcom-ext.h

libcapture_broadcom_la_SOURCES = icapturesession.cpp
libcapture_broadcom_la_CXXFLAGS = -std=c++11 $(RDK_CAPTURE_INCLUDES) $(NEXUS_CFLAGS) $(GST_CFLAGS) -pthread
libcapture_broadcom_la_LDFLAGS = -avoid-version
libcapture_broadcom_la_LIBADD = -lnexus -lpthread $(GST_LIBS)

libgstwesterossink_la_SOURCES = westeros-sink.c westeros-sink-soc.c

# the capture library is dlopen()ed by the sink (capture-sink-bridge.h)
libgstwesterossink_la_LIBADD = -ldl

libgstwesterossink_la_CFLAGS= \
   $(AM_CFLAGS) \
   $(RDK_CAPTURE_INCLUDES) \
   $(GST_CFLAGS)

if HAVE_PIPELINE_LOGGING
AM_CFLAGS += -DUSE_PIPELINE_LOGGING
libgstwesterossink_la_SOURCES +=  pipeline_logger.cpp
libgstwesterossink_la_CXXFLAGS=  \
   $(AM_CFLAGS) \
   $(GST_CFLAGS)
endif

libgstwesterossink_la_LDFLAGS= \
   $(AM_LDFLAGS) \
   $(GST_LIBS)  $(GSTBASE_LIBS) $(WAYLANDLIB) -avoid-version \
   -lwesteros_compositor \   
   -lwesteros_simplebuffer_client \
   -lwesteros_simpleshell_client \
   -lessosrmgr \
   -lwayland-egl \
   -lnxclient \
   -lnexus
   
distcleancheck_listfiles = *-libtool

## IPK Generation Support
libgstwesterossink_IPK_OUTDIR = $(abs_top_builddir)/ipk
libgstwesterossink_IPK_STAGING_DIR=$(abs_top_builddir)/staging_dir
libgstwesterossink_IPK_TMP_DIR=$(libgstwesterossink_IPK_STAGING_DIR)/ipk

libgstwesterossink_CONTROL_FILES = control
libgstwesterossink_IPK_FILES  = debian-binary control.tar.gz data.tar.gz
libgstwesterossink_IPK_OUTPUT = libgstwesterossink.ipk

package_ipk:
	@echo "Generating IPK Package"
	$(MAKE) prefix=$(libgstwesterossink_IPK_STAGING_DIR) install
	$(mkdir_p) $(libgstwesterossink_IPK_TMP_DIR)/usr/lib/$(LIB_SUB_DIR)
	cp -aRf $(libgstwesterossink_IPK_STAGING_DIR)/lib/$(LIB_SUB_DIR)/*.so $(libgstwesterossink_IPK_TMP_DIR)/usr/lib/$(LIB_SUB_DIR)
	tar -czvf $(libgstwesterossink_IPK_OUTDIR)/data.tar.gz -C $(libgstwesterossink_IPK_TMP_DIR) usr
	tar -czvf $(libgstwesterossink_IPK_OUTDIR)/control.tar.gz -C $(libgstwesterossink_IPK_OUTDIR) $(libgstwesterossink_CONTROL_FILES)
	cd $(libgstwesterossink_IPK_OUTDIR) && ar cr $(libgstwesterossink_IPK_OUTPUT) $(libgstwesterossink_IPK_FILES) && cd -
	
