/*
* Copyright (C) 2016 RDK Management
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

/**
 * RPI and platform-specific stubs for linking westeros-sink binaries
 */
//github CI version
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>

/* OMX error type */
typedef int OMX_ERRORTYPE;
typedef int OMX_COMMANDTYPE;
typedef int OMX_STATETYPE;
typedef int OMX_INDEXTYPE;
typedef void* OMX_HANDLETYPE;
typedef void* OMX_PTR;
typedef const char* OMX_STRING;
typedef uint8_t OMX_U8;
typedef uint32_t OMX_U32;
typedef uint32_t OMX_VERSIONTYPE;
typedef void OMX_CALLBACKTYPE;

/* OMX parameter direction markers */
#define OMX_IN
#define OMX_OUT
#define OMX_INOUT

/* OMX error code */
#define OMX_ErrorNone 0

/* OMX state types */
#define OMX_StateIdle 1
#define OMX_StateExecuting 2

/* OMX buffer header type */
typedef struct {
    uint32_t nSize;
    uint32_t nVersion;
    uint8_t *pBuffer;
    uint32_t nAllocLen;
    uint32_t nFilledLen;
    uint32_t nOffset;
    void *pAppPrivate;
    void *pPlatformPrivate;
    void *pInputPortPrivate;
    void *pOutputPortPrivate;
} OMX_BUFFERHEADERTYPE;

#ifdef __cplusplus
extern "C" {
#endif

void* khrn_platform_get_wl_display(void) {
    return NULL;
}

/* Wayland EGL Stubs */
void* wl_egl_window_create(void *surface, int width, int height) {
    return NULL;
}

void wl_egl_window_destroy(void *egl_window) {
}

void wl_egl_window_get_attached_size(void *egl_window, int *width, int *height) {
    if (width) *width = 1280;
    if (height) *height = 720;
}

void wl_egl_window_resize(void *egl_window, int width, int height, int dx, int dy) {
}

#ifdef __cplusplus
}
#endif

static int mock_omx_handle_counter = 1000;

OMX_ERRORTYPE OMX_Init(void)
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_Deinit(void)
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE* pHandle,
    OMX_IN OMX_STRING cComponentName,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_CALLBACKTYPE* pCallBacks)
{
    (void)cComponentName;
    (void)pAppData;
    (void)pCallBacks;

    if (pHandle) {
        *pHandle = (OMX_HANDLETYPE)(intptr_t)mock_omx_handle_counter++;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent)
{
    (void)hComponent;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nOutputPort,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nInputPort)
{
    (void)hOutput;
    (void)nOutputPort;
    (void)hInput;
    (void)nInputPort;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetState(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STATETYPE* pState)
{
    (void)hComponent;
    if (pState) {
        *pState = OMX_StateIdle;  
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_SendCommand(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_COMMANDTYPE Cmd,
    OMX_IN OMX_U32 nParam1,
    OMX_IN OMX_PTR pCmdData)
{
    (void)hComponent;
    (void)Cmd;
    (void)nParam1;
    (void)pCmdData;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR pComponentParameterStructure)
{
    (void)hComponent;
    (void)nParamIndex;
    (void)pComponentParameterStructure;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nParamIndex,
    OMX_IN OMX_PTR pComponentParameterStructure)
{
    (void)hComponent;
    (void)nParamIndex;
    (void)pComponentParameterStructure;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nConfigIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    (void)hComponent;
    (void)nConfigIndex;
    (void)pComponentConfigStructure;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE nConfigIndex,
    OMX_IN OMX_PTR pComponentConfigStructure)
{
    (void)hComponent;
    (void)nConfigIndex;
    (void)pComponentConfigStructure;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_STRING cParameterName,
    OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    (void)hComponent;
    (void)cParameterName;
    if (pIndexType) {
        *pIndexType = 0;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes)
{
    (void)hComponent;
    (void)nPortIndex;
    (void)pAppPrivate;
    (void)nSizeBytes;

    if (ppBuffer) {
        *ppBuffer = (OMX_BUFFERHEADERTYPE*)malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if (*ppBuffer) {
            memset(*ppBuffer, 0, sizeof(OMX_BUFFERHEADERTYPE));
        }
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
    (void)hComponent;
    (void)nPortIndex;

    if (pBuffer) {
        free(pBuffer);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer)
{
    (void)hComponent;
    (void)nPortIndex;
    (void)pAppPrivate;
    (void)nSizeBytes;
    (void)pBuffer;

    if (ppBufferHdr) {
        *ppBufferHdr = (OMX_BUFFERHEADERTYPE*)malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if (*ppBufferHdr) {
            memset(*ppBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
            (*ppBufferHdr)->pBuffer = pBuffer;
            (*ppBufferHdr)->nAllocLen = nSizeBytes;
        }
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_EmptyThisBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
    (void)hComponent;
    (void)pBuffer;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_FillThisBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
    (void)hComponent;
    (void)pBuffer;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_UseEGLImage(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_PTR eglImage)
{
    (void)hComponent;
    (void)nPortIndex;
    (void)pAppPrivate;
    (void)eglImage;

    if (ppBufferHdr) {
        *ppBufferHdr = (OMX_BUFFERHEADERTYPE*)malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if (*ppBufferHdr) {
            memset(*ppBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
        }
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetComponentsOfRole(
    OMX_IN OMX_STRING role,
    OMX_INOUT OMX_U32* pNumComps,
    OMX_INOUT OMX_U8** ppCompNames)
{
    (void)role;
    if (pNumComps) {
        *pNumComps = 0;
    }
    (void)ppCompNames;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_GetRolesOfComponent(
    OMX_IN OMX_U8* compName,
    OMX_INOUT OMX_U32* pNumRoles,
    OMX_INOUT OMX_U8** roles)
{
    (void)compName;
    if (pNumRoles) {
        *pNumRoles = 0;
    }
    (void)roles;
    return OMX_ErrorNone;
}

static void mock_dummy_function(void) {}

void *dlopen(const char *filename, int flags)
{
    (void)filename;
    (void)flags;
    return NULL;
}

void *dlsym(void *restrict handle, const char *restrict symbol)
{
    (void)handle;
    (void)symbol;
    return (void*)&mock_dummy_function;
}

int dlclose(void *handle)
{
    (void)handle;
    return 0;
}

char *dlerror(void)
{
    return "Library not found in test environment";
}

void bcm_host_init(void)
{
}

void bcm_host_deinit(void)
{
}

uint32_t bcm_host_get_sdram_address(void)
{
    return 0;
}

void graphics_get_display_size(const uint16_t display_number,
                               uint32_t *width,
                               uint32_t *height)
{
    (void)display_number;
    if (width) *width = 1280;
    if (height) *height = 720;
}

typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef void* EGLConfig;
typedef int EGLBoolean;
typedef int EGLint;
typedef int EGLenum;

#define EGL_TRUE 1
#define EGL_FALSE 0
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)

static EGLDisplay mock_egl_display = (EGLDisplay)0x1;
static EGLContext mock_egl_context = (EGLContext)0x2;
static EGLSurface mock_egl_surface = (EGLSurface)0x3;

EGLDisplay __wrap_eglGetDisplay(void *native_display)
{
    (void)native_display;
    return mock_egl_display;
}

EGLBoolean __wrap_eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    (void)dpy;
    if (major) *major = 1;
    if (minor) *minor = 4;
    return EGL_TRUE;
}

EGLBoolean __wrap_eglTerminate(EGLDisplay dpy)
{
    (void)dpy;
    return EGL_TRUE;
}

EGLBoolean __wrap_eglReleaseThread(void)
{
    return EGL_TRUE;
}

EGLContext __wrap_eglGetCurrentContext(void)
{
    return mock_egl_context;
}

EGLDisplay __wrap_eglGetCurrentDisplay(void)
{
    return mock_egl_display;
}

EGLSurface __wrap_eglGetCurrentSurface(EGLint readdraw)
{
    (void)readdraw;
    return mock_egl_surface;
}

EGLBoolean __wrap_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    (void)dpy;
    (void)draw;
    (void)read;
    (void)ctx;
    return EGL_TRUE;
}

EGLBoolean __wrap_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    (void)dpy;
    (void)surface;
    return EGL_TRUE;
}

EGLBoolean __wrap_eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    (void)dpy;
    (void)interval;
    return EGL_TRUE;
}

const char *__wrap_eglQueryString(EGLDisplay dpy, EGLint name)
{
    (void)dpy;
    (void)name;
    return "";
}

EGLBoolean __wrap_eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    (void)dpy;
    (void)configs;
    (void)config_size;
    if (num_config) *num_config = 0;
    return EGL_TRUE;
}

EGLBoolean __wrap_eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, 
                          EGLint config_size, EGLint *num_config)
{
    (void)dpy;
    (void)attrib_list;
    (void)configs;
    (void)config_size;
    if (num_config) *num_config = 0;
    return EGL_TRUE;
}

EGLBoolean __wrap_eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    (void)dpy;
    (void)config;
    (void)attribute;
    if (value) *value = 0;
    return EGL_TRUE;
}

EGLContext __wrap_eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, 
                           const EGLint *attrib_list)
{
    (void)dpy;
    (void)config;
    (void)share_context;
    (void)attrib_list;
    return mock_egl_context;
}

EGLBoolean __wrap_eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    (void)dpy;
    (void)ctx;
    return EGL_TRUE;
}

EGLSurface __wrap_eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, void *native_window, 
                                 const EGLint *attrib_list)
{
    (void)dpy;
    (void)config;
    (void)native_window;
    (void)attrib_list;
    return mock_egl_surface;
}

EGLSurface __wrap_eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    (void)dpy;
    (void)config;
    (void)attrib_list;
    return mock_egl_surface;
}

EGLBoolean __wrap_eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    (void)dpy;
    (void)surface;
    return EGL_TRUE;
}

EGLint __wrap_eglGetError(void)
{
    return 0;
}

void __wrap_glXDestroyContext(void *display, void *context)
{
    (void)display;
    (void)context;
}

void __wrap_glXDestroyWindow(void *display, void *window)
{
    (void)display;
    (void)window;
}

void __wrap_glXMakeCurrent(void *display, void *drawable, void *ctx)
{
    (void)display;
    (void)drawable;
    (void)ctx;
}

void __wrap_glDeleteProgram(unsigned int program)
{
    (void)program;
}

void __wrap_glDeleteShader(unsigned int shader)
{
    (void)shader;
}

void __wrap_glDeleteBuffers(int n, const unsigned int *buffers)
{
    (void)n;
    (void)buffers;
}

void __wrap_glDeleteTextures(int n, const unsigned int *textures)
{
    (void)n;
    (void)textures;
}

void __wrap_glDeleteFramebuffersEXT(int n, const unsigned int *framebuffers)
{
    (void)n;
    (void)framebuffers;
}

void __wrap_glDeleteRenderbuffersEXT(int n, const unsigned int *renderbuffers)
{
    (void)n;
    (void)renderbuffers;
}

void __wrap_glDeleteVertexArrays(int n, const unsigned int *arrays)
{
    (void)n;
    (void)arrays;
}

int __wrap_drmClose(int fd)
{
    (void)fd;
    return 0;
}

void __wrap_gbm_device_destroy(void *device)
{
    (void)device;
}

void __wrap_gbm_surface_destroy(void *surface)
{
    (void)surface;
}

void wl_proxy_marshal(void *proxy, unsigned int opcode, ...)
{
    (void)proxy;
    (void)opcode;
}

void wl_proxy_marshal_flags(void *proxy, unsigned int opcode, const void *interface, unsigned int version, unsigned int flags, ...)
{
    (void)proxy;
    (void)opcode;
    (void)interface;
    (void)version;
    (void)flags;
}

static void sigsegv_handler(int sig)
{
    (void)sig;
    // Use exit() instead of _exit() to allow coverage data flush through atexit handlers
    exit(0);
}

__attribute__((constructor))
static void setup_crash_prevention(void)
{
    struct sigaction sa;
    sa.sa_handler = sigsegv_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, NULL);
}

