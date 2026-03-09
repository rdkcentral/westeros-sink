#
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

# Process this file with autoconf to produce a configure script.

dnl AC_PREREQ([2.65])
AC_INIT(westeros, 1.0.0, jeff_wannamaker@cable.comcast.com,westeros)
AC_CONFIG_SRCDIR([])
AC_CONFIG_HEADERS([cfg/config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([cfg])
AC_CONFIG_SUBDIRS([essos])
#AC_CONFIG_SUBDIRS([linux-dmabuf])
AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-bzip2 1.9])
LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CXX

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_HEADER_STDBOOL

IARM_CFLAGS=" "
ENABLE_WESTEROS_APP=" "
ENABLE_WESTEROS_TEST=" "
ENABLE_WESTEROS_PLAYER=" "
ENABLE_PLATFORM_EMBEDDED=" "
EGL_DETECTED=" "
GLESV2_DETECTED=" "
GLEW_DETECTED=" "
WAYLAND_EGL_DETECTED=" "
ENABLE_XDG_V4=" "
ENABLE_XDG_V5=" "
ENABLE_XDG_STABLE=" "
ENABLE_ESSOS=" "

# Checks for library functions.
#Add the subdirectories to be considered for building.
SUBDIRS=" "
#to setup m4 macros
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
[AM_DEFAULT_VERBOSITY=1
AC_SUBST(AM_DEFAULT_VERBOSITY)])

PKG_CHECK_MODULES([GLIB],[glib-2.0 >= 2.22.5])
PKG_CHECK_MODULES([GTHREAD],[gthread-2.0 >= 2.38.2])
PKG_CHECK_MODULES([WAYLAND_CLIENT],[wayland-client >= 1.6.0])
PKG_CHECK_MODULES([WAYLAND_SERVER],[wayland-server >= 1.6.0])
PKG_CHECK_MODULES([WAYLAND_EGL],[wayland-egl >= 0.0],[WAYLAND_EGL_DETECTED=true],[WAYLAND_EGL_DETECTED=false])
PKG_CHECK_MODULES([XKBCOMMON],[xkbcommon >= 0.4])
PKG_CHECK_MODULES([EGL],[egl >= 0.0],[EGL_DETECTED=true],[EGL_DETECTED=false])
PKG_CHECK_MODULES([GLESV2],[glesv2 >= 0.0],[GLESV2_DETECTED=true],[GLESV2_DETECTED=false])

AM_CONDITIONAL([HAVE_WAYLAND_EGL], [test x$WAYLAND_EGL_DETECTED = xtrue])              
AM_CONDITIONAL([HAVE_EGL], [test x$EGL_DETECTED = xtrue])              
AM_CONDITIONAL([HAVE_GLESV2], [test x$GLESV2_DETECTED = xtrue])              

AC_ARG_ENABLE([app],
              AS_HELP_STRING([--enable-app],[enable westeros app (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_APP=true ;;
                  no) AC_MSG_ERROR([westeros app is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-app]) ;;
                esac
              ],
              [echo "westeros app is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_APP], [test x$ENABLE_WESTEROS_APP = xtrue])              

AC_ARG_ENABLE([test],
              AS_HELP_STRING([--enable-test],[enable westeros_test (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_TEST=true ;;
                  no) AC_MSG_ERROR([westeros_test is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-test]) ;;
                esac
              ],
              [echo "westeros_test is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_TEST], [test x$ENABLE_WESTEROS_TEST = xtrue])              

AC_ARG_ENABLE([player],
              AS_HELP_STRING([--enable-player],[enable westeros_player (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_PLAYER=true ;;
                  no) AC_MSG_ERROR([westeros_player is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-player]) ;;
                esac
              ],
              [echo "westeros_player is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_PLAYER], [test x$ENABLE_WESTEROS_PLAYER = xtrue])              

AC_ARG_ENABLE([rendergl],
              AS_HELP_STRING([--enable-rendergl],[enable westeros opengl renderer (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_RENDERERGL=true ;;
                  no) AC_MSG_ERROR([opengl renderer is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-rendergl]) ;;
                esac
              ],
              [echo "westeros opengl renderer is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_RENDERERGL], [test x$ENABLE_WESTEROS_RENDERERGL = xtrue])              

AC_ARG_ENABLE([sbprotocol],
              AS_HELP_STRING([--enable-sbprotocol],[enable westeros wl_sb protocol (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_SBPROTOCOL=true ;;
                  no) AC_MSG_ERROR([westeros wl_sb protocol is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-sbprotocol]) ;;
                esac
              ],
              [echo "westeros wl_sb protocol is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_SBPROTOCOL], [test x$ENABLE_WESTEROS_SBPROTOCOL = xtrue])              

AC_ARG_ENABLE([ldbprotocol],
              AS_HELP_STRING([--enable-ldbprotocol],[enable zwp_linux_dmabuf_v1 protocol (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_LDBPROTOCOL=true ;;
                  no) ENABLE_WESTEROS_LDBPROTOCOL=false ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-ldbprotocol]) ;;
                esac
              ],
              [echo "zwp_linux_dmabuf_v1 protocol is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_LDBPROTOCOL], [test x$ENABLE_WESTEROS_LDBPROTOCOL = xtrue])

AC_ARG_ENABLE([lexpsyncprotocol],
              AS_HELP_STRING([--enable-lexpsyncprotocol],[enable linux_explicit_synchronization_unstable_v1 protocol (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_WESTEROS_LEXPSYNCPROTOCOL=true ;;
                  no) ENABLE_WESTEROS_LEXPSYNCPROTOCOL=false ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-lexpsyncprotocol]) ;;
                esac
              ],
              [echo "linux_explicit_synchronization_unstable_v1 protocol is disabled"])
AM_CONDITIONAL([ENABLE_WESTEROS_LEXPSYNCPROTOCOL], [test x$ENABLE_WESTEROS_LEXPSYNCPROTOCOL = xtrue])

AC_ARG_ENABLE([embedded],
              AS_HELP_STRING([--enable-embedded],[enable westeros platform embedded (default is yes)]),
              [
                case "${enableval}" in
                 yes) ENABLE_PLATFORM_EMBEDDED=true ;;
                  no) ENABLE_PLATFORM_EMBEDDED=no ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-embedded]) ;;
                esac
              ],
              [echo "westeros platform embedded is enabled"])
AM_CONDITIONAL([ENABLE_PLATFORM_EMBEDDED], [test x$ENABLE_PLATFORM_EMBEDDED != xno])

AC_ARG_ENABLE([xdgv4],
              AS_HELP_STRING([--enable-xdgv4],[enable xdg_shell protocol v4 (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_XDG_V4=true ENABLE_XDG_V5=false ;;
                  no) AC_MSG_NOTICE([xdg_shell protocol v4 is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-xdgv4]) ;;
                esac
              ],
              [echo "xdg_shell protocol v4 is disabled"])
AM_CONDITIONAL([ENABLE_XDG_V4], [test x$ENABLE_XDG_V4 = xtrue])              

AC_ARG_ENABLE([xdgv5],
              AS_HELP_STRING([--enable-xdgv5],[enable xdg_shell protocol v5 (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_XDG_V5=true ENABLE_XDG_V4=false ;;
                  no) AC_MSG_NOTICE([xdg_shell protocol v5 is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-xdgv5]) ;;
                esac
              ],
              [echo "xdg_shell protocol v5 is disabled"])
AM_CONDITIONAL([ENABLE_XDG_V5], [test x$ENABLE_XDG_V5 = xtrue])

AC_ARG_ENABLE([xdgstable],
              AS_HELP_STRING([--enable-xdgstable],[enable xdg_shell stable protocol (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_XDG_STABLE=true ENABLE_XDG_V5=false ENABLE_XDG_V4=false ;;
                  no) AC_MSG_NOTICE([xdg_shell stable protocol is disabled]) ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-xdgstable]) ;;
                esac
              ],
              [echo "xdg_shell stable protocol is disabled"])
AM_CONDITIONAL([ENABLE_XDG_STABLE], [test x$ENABLE_XDG_STABLE = xtrue])

AC_ARG_ENABLE([essos],
              AS_HELP_STRING([--enable-essos],[enable essos (default is yes)]),
              [
                case "${enableval}" in
                 yes) ENABLE_ESSOS=true ;;
                  no) ENABLE_ESSOS=no ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-essos]) ;;
                esac
              ],
              [echo "essos is enabled"])
AM_CONDITIONAL([ENABLE_ESSOS], [test x$ENABLE_ESSOS != xno])

AC_ARG_ENABLE([modules],
              AS_HELP_STRING([--enable-modules],[enable modules (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_MODULES=true ;;
                  no) ENABLE_MODULES=no ;;
                   *) AC_MSG_ERROR([bad value ${enableval} for --enable-modules]) ;;
                esac
              ],
              [echo "westeros modules build is disabled"])
AM_CONDITIONAL([ENABLE_MODULES], [test x$ENABLE_MODULES = xtrue])

AM_COND_IF([ENABLE_MODULES], [AC_CONFIG_SUBDIRS([drm/modules])])

GST_MAJORMINOR=1.0
PKG_CHECK_MODULES([GST], [gstreamer-1.0 >= 1.0], have_gst1="yes", have_gst1="no")

AS_IF([test "x$have_gst1" = "xyes"], [
  PKG_CHECK_MODULES([GSTBASE], [gstreamer-base-1.0 >= 1.0])
  AC_DEFINE(USE_GST1, 1, [Build with GStreamer 1.x])
], [])

AS_IF([test "x$enable_gstreamer0" != "xyes" -a "x$have_gst1" != "xyes"], [
   AC_MSG_ERROR([Could not find GStreamer 1.x dependencies:

$GST_PKG_ERRORS])], [])

WAYLANDLIB="-lwayland-server"
AC_SUBST(WAYLANDLIB)

AC_CONFIG_FILES([Makefile westeros-compositor.pc])
AC_SUBST(WAYLAND_CFLAGS)
AC_SUBST(GUPNP_VERSION)
AC_SUBST(ENABLE_WESTEROS_APP)
AC_SUBST(ENABLE_WESTEROS_TEST)
AC_SUBST(ENABLE_WESTEROS_PLAYER)
AC_OUTPUT
