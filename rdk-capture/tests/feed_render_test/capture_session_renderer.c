/*
 * Copyright (C) 2016 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * capture_session_renderer: capture "client" for the SoC video frame capture spec.
 *
 * Connects to capture_session_feeder, receives frame descriptions plus dmabuf FDs
 * (SCM_RIGHTS), imports them as EGLImages (EGL_LINUX_DMA_BUF_EXT), binds each
 * to a GL_TEXTURE_EXTERNAL_OES texture with glEGLImageTargetTexture2DOES, and
 * redraws every time a frame arrives.  Images are cached by the server's
 * bufferId; when a frame arrives with dmaBufObjectsFlushed the whole cache
 * (textures, EGLImages, any FDs still held) is destroyed before the lookup.
 *
 * Each frame is drawn and glFinish()ed before the ack is sent, so the server
 * only releases the frame after GPU reads are complete (spec 6.1).  Imports
 * are not kept across frames with FDs: EGL does not take ownership of the FDs,
 * so they are closed right after eglCreateImage (spec 6.3); the cache entry
 * keeps only the image and texture.
 *
 * Build (Wayland/Westeros):
 *   gcc capture_session_renderer.c -o capture_session_renderer \
 *       $(pkg-config --cflags --libs wayland-client) -lwayland-egl -Wl,--no-as-needed -lEGL -lGLESv2
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "capture_session_ipc.h"

/* ------------------------------------------------------------------ */

/* -verbose: log every frame in full; otherwise recv/drawn only every LOG_EVERY frames */
static int g_verbose;
#define LOG_EVERY 60

typedef struct Entry {
    uint64_t bufferId;
    EGLImageKHR image;
    GLuint tex;
    int fds[CAPTURE_SESSION_MAX_FDS];      /* -1 once closed (normally right after import) */
    uint64_t lastUse;
    CaptureSessionFrameMsg last;           /* last description drawn with this entry (for replay) */
    struct Entry *next;
} Entry;

typedef struct {
    /* wayland */
    struct wl_display *wl_display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_egl_window *egl_window;
    int width, height;

    /* egl / gl */
    EGLDisplay dpy;
    EGLContext ctx;
    EGLSurface egl_surface;
    /* EGL 1.5 core (eglCreateImage, EGLAttrib list) or EGL_KHR_image_base. */
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    void *(*eglCreateImage15)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const intptr_t *);
    EGLBoolean (*eglDestroyImage15)(EGLDisplay, void *);
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    GLuint program;
    GLint loc_pos, loc_tc;

    /* cache */
    Entry *cache;
    unsigned cache_count;
    uint64_t tick;

    int sock;
    uint64_t frames;
} Client;

static Client cl;

/* ------------------------------------------------------------------ */
/* wayland                                                             */
/* ------------------------------------------------------------------ */

static void shell_ping(void *d, struct wl_shell_surface *s, uint32_t serial)
{ (void)d; wl_shell_surface_pong(s, serial); }
static void shell_configure(void *d, struct wl_shell_surface *s, uint32_t e, int32_t w, int32_t h)
{
    (void)s; (void)e;
    Client *c = (Client *)d;
    if (w > 0 && h > 0) {
        c->width = w; c->height = h;
        if (c->egl_window) wl_egl_window_resize(c->egl_window, w, h, 0, 0);
    }
}
static void shell_popup_done(void *d, struct wl_shell_surface *s) { (void)d; (void)s; }
static const struct wl_shell_surface_listener shell_listener = { shell_ping, shell_configure, shell_popup_done };

static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver)
{
    Client *c = (Client *)d;
    (void)ver;
    if (!strcmp(iface, "wl_compositor"))
        c->compositor = wl_registry_bind(r, name, &wl_compositor_interface, 1);
    else if (!strcmp(iface, "wl_shell"))
        c->shell = wl_registry_bind(r, name, &wl_shell_interface, 1);
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n) { (void)d; (void)r; (void)n; }
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

/* ------------------------------------------------------------------ */
/* egl / gl setup                                                      */
/* ------------------------------------------------------------------ */

static const char *vs_src =
    "attribute vec2 pos;\n"
    "attribute vec2 tc;\n"
    "varying vec2 v_tc;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); v_tc = tc; }\n";

static const char *fs_src =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_tc;\n"
    "uniform samplerExternalOES tex;\n"
    "void main() { gl_FragColor = texture2D(tex, v_tc); }\n";

static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "shader error: %s\n", log);
        return 0;
    }
    return s;
}

static int init_gl(Client *c)
{
    /* Same config request as westeros-test: RGBA8888, no depth/stencil. */
    static const EGLint cfg_attr[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLConfig cfgs[64];
    static const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLConfig cfg;
    EGLint n = 0, maj, min;

    /* Which native display the EGL driver accepts is platform specific: try the
     * Wayland platform display, the wl_display, then the default display. */
    const char *cext = eglQueryString(EGL_DEFAULT_DISPLAY, EGL_EXTENSIONS);
    printf("client EGL extensions: %s\n", cext ? cext : "(none)");
    c->dpy = EGL_NO_DISPLAY;
    for (int attempt = 0; attempt < 3; attempt++) {
        EGLDisplay d = EGL_NO_DISPLAY;
        const char *what = "";
        if (attempt == 0) {
            PFNEGLGETPLATFORMDISPLAYEXTPROC getpd =
                (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
            what = "eglGetPlatformDisplayEXT(WAYLAND)";
            if (getpd) d = getpd(EGL_PLATFORM_WAYLAND_EXT, c->wl_display, NULL);
        } else if (attempt == 1) {
            what = "eglGetDisplay(wl_display)";
            d = eglGetDisplay((EGLNativeDisplayType)c->wl_display);
        } else {
            what = "eglGetDisplay(EGL_DEFAULT_DISPLAY)";
            d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        }
        if (d == EGL_NO_DISPLAY) { printf("%s: no display (0x%x)\n", what, eglGetError()); continue; }
        if (eglInitialize(d, &maj, &min)) {
            printf("%s: EGL %d.%d initialized\n", what, maj, min);
            c->dpy = d;
            break;
        }
        printf("%s: eglInitialize failed (0x%x)\n", what, eglGetError());
    }
    if (c->dpy == EGL_NO_DISPLAY) { fprintf(stderr, "eglInitialize failed\n"); return -1; }
    eglBindAPI(EGL_OPENGL_ES_API);
    if (!eglChooseConfig(c->dpy, cfg_attr, cfgs, 64, &n) || n < 1) { fprintf(stderr, "no EGL config (0x%x)\n", eglGetError()); return -1; }
    cfg = cfgs[0];
    for (EGLint i = 0; i < n; i++) {
        EGLint r = 0, g = 0, b = 0, al = 0;
        eglGetConfigAttrib(c->dpy, cfgs[i], EGL_RED_SIZE, &r);
        eglGetConfigAttrib(c->dpy, cfgs[i], EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(c->dpy, cfgs[i], EGL_BLUE_SIZE, &b);
        eglGetConfigAttrib(c->dpy, cfgs[i], EGL_ALPHA_SIZE, &al);
        if (r == 8 && g == 8 && b == 8 && al == 8) { cfg = cfgs[i]; break; }
    }

    const char *ext = eglQueryString(c->dpy, EGL_EXTENSIONS);
    printf("EGL %d.%d display extensions (len %zu):\n", maj, min, ext ? strlen(ext) : (size_t)0);
    if (ext) {
        /* one per line: long single-line output is easy to truncate when copied */
        char *dup = strdup(ext), *save = NULL;
        for (char *t = strtok_r(dup, " ", &save); t; t = strtok_r(NULL, " ", &save)) printf("  %s\n", t);
        free(dup);
    }
    if (!ext || !strstr(ext, "EGL_EXT_image_dma_buf_import")) {
        fprintf(stderr, "EGL_EXT_image_dma_buf_import missing\n");
        return -1;
    }
    if (!strstr(ext, "EGL_EXT_image_dma_buf_import_modifiers"))
        fprintf(stderr, "warning: EGL_EXT_image_dma_buf_import_modifiers not advertised\n");
    /* EGL_KHR_image_base is not required when EGL 1.5 core image entry points exist. */
    c->eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    c->eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (!c->eglCreateImageKHR || !c->eglDestroyImageKHR) {
        c->eglCreateImageKHR = NULL;
        c->eglDestroyImageKHR = NULL;
        if (maj > 1 || min >= 5) {
            c->eglCreateImage15 = (void *(*)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const intptr_t *))
                eglGetProcAddress("eglCreateImage");
            c->eglDestroyImage15 = (EGLBoolean (*)(EGLDisplay, void *))eglGetProcAddress("eglDestroyImage");
        }
        if (!c->eglCreateImage15 || !c->eglDestroyImage15) {
            fprintf(stderr, "no eglCreateImage/eglCreateImageKHR entry point\n");
            return -1;
        }
    }
    c->glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (!c->glEGLImageTargetTexture2DOES) {
        fprintf(stderr, "missing glEGLImageTargetTexture2DOES\n");
        return -1;
    }

    c->ctx = eglCreateContext(c->dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    c->egl_window = wl_egl_window_create(c->surface, c->width, c->height);
    c->egl_surface = eglCreateWindowSurface(c->dpy, cfg, (EGLNativeWindowType)c->egl_window, NULL);
    if (c->egl_surface == EGL_NO_SURFACE) {     /* westeros-test retries with a NULL window */
        fprintf(stderr, "eglCreateWindowSurface failed (0x%x), retrying with NULL window\n", eglGetError());
        c->egl_surface = eglCreateWindowSurface(c->dpy, cfg, (EGLNativeWindowType)NULL, NULL);
    }
    if (c->ctx == EGL_NO_CONTEXT || c->egl_surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(c->dpy, c->egl_surface, c->egl_surface, c->ctx)) {
        fprintf(stderr, "EGL context/surface setup failed\n");
        return -1;
    }
    eglSwapInterval(c->dpy, 0);     /* never block the ack path on vsync */
    {
        EGLint sw = 0, sh = 0;
        eglQuerySurface(c->dpy, c->egl_surface, EGL_WIDTH, &sw);
        eglQuerySurface(c->dpy, c->egl_surface, EGL_HEIGHT, &sh);
        printf("EGL surface %p size %dx%d (requested %dx%d), ctx %p\n",
               (void *)c->egl_surface, sw, sh, c->width, c->height, (void *)c->ctx);
        printf("GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n",
               (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER),
               (const char *)glGetString(GL_VERSION));
    }

    const char *glext = (const char *)glGetString(GL_EXTENSIONS);
    if (!glext || !strstr(glext, "GL_OES_EGL_image_external")) {
        fprintf(stderr, "GL_OES_EGL_image_external missing; GL extensions: %s\n", glext ? glext : "(none)");
        return -1;
    }

    GLuint vs = compile(GL_VERTEX_SHADER, vs_src), fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return -1;
    c->program = glCreateProgram();
    glAttachShader(c->program, vs);
    glAttachShader(c->program, fs);
    glLinkProgram(c->program);
    {
        GLint ok = 0;
        glGetProgramiv(c->program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(c->program, sizeof(log), NULL, log);
            fprintf(stderr, "program link error: %s\n", log);
            return -1;
        }
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    c->loc_pos = glGetAttribLocation(c->program, "pos");
    c->loc_tc = glGetAttribLocation(c->program, "tc");
    glUseProgram(c->program);
    glUniform1i(glGetUniformLocation(c->program, "tex"), 0);
    printf("shader program %u linked, loc_pos=%d loc_tc=%d tex=%d\n", c->program, c->loc_pos, c->loc_tc,
           glGetUniformLocation(c->program, "tex"));
    return 0;
}

/* ------------------------------------------------------------------ */
/* cache                                                               */
/* ------------------------------------------------------------------ */

static EGLImageKHR create_image(Client *c, const EGLint *attr32)
{
    if (c->eglCreateImageKHR)
        return c->eglCreateImageKHR(c->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attr32);
    /* eglCreateImage takes an EGLAttrib (pointer-sized) list */
    intptr_t a[64];
    int i = 0;
    do { a[i] = attr32[i]; } while (attr32[i++] != EGL_NONE && i < 64);
    return (EGLImageKHR)c->eglCreateImage15(c->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, a);
}

static void destroy_image(Client *c, EGLImageKHR img)
{
    if (c->eglDestroyImageKHR) c->eglDestroyImageKHR(c->dpy, img);
    else c->eglDestroyImage15(c->dpy, img);
}

static void entry_destroy(Client *c, Entry *e)
{
    /* texture first, then the EGLImage backing it (spec 6.4) */
    if (e->tex) glDeleteTextures(1, &e->tex);
    if (e->image != EGL_NO_IMAGE_KHR) destroy_image(c, e->image);
    for (int i = 0; i < CAPTURE_SESSION_MAX_FDS; i++)
        if (e->fds[i] >= 0) close(e->fds[i]);
    free(e);
}

static void cache_flush(Client *c)
{
    /* Every frame is finished (glFinish + ack) before the next one is read, so
     * nothing is retained by an older frame and all entries can go now. */
    Entry *e = c->cache;
    printf("dmaBufObjectsFlushed: dropping %u cached image(s)\n", c->cache_count);
    while (e) { Entry *n = e->next; entry_destroy(c, e); e = n; }
    c->cache = NULL;
    c->cache_count = 0;
}

static Entry *cache_lookup(Client *c, uint64_t id)
{
    for (Entry *e = c->cache; e; e = e->next)
        if (e->bufferId == id) return e;
    return NULL;
}

static void cache_evict_to(Client *c, unsigned limit)
{
    while (c->cache_count > limit) {
        Entry **pp = &c->cache, **oldest = pp;
        for (; *pp; pp = &(*pp)->next)
            if ((*pp)->lastUse < (*oldest)->lastUse) oldest = pp;
        Entry *e = *oldest;
        *oldest = e->next;
        entry_destroy(c, e);
        c->cache_count--;
    }
}

static void close_fds(int *fds, unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        if (fds[i] >= 0) { close(fds[i]); fds[i] = -1; }
}

/* Builds EGL_LINUX_DMA_BUF_EXT attributes from the message (spec 6.3). */
static Entry *import_frame(Client *c, const CaptureSessionFrameMsg *m, int *fds)
{
    static const EGLint plane_attr[CAPTURE_SESSION_MAX_PLANES][5] = {
        { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE0_PITCH_EXT,
          EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT },
        { EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
          EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT },
        { EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT,
          EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT },
        { EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT,
          EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT },
    };
    EGLint a[64];
    int n = 0;
    Entry *e;

    e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->bufferId = m->bufferId;
    e->image = EGL_NO_IMAGE_KHR;
    for (int i = 0; i < CAPTURE_SESSION_MAX_FDS; i++) e->fds[i] = -1;

    a[n++] = EGL_WIDTH;  a[n++] = (EGLint)m->backingWidth;
    a[n++] = EGL_HEIGHT; a[n++] = (EGLint)m->backingHeight;
    a[n++] = EGL_LINUX_DRM_FOURCC_EXT; a[n++] = (EGLint)m->drmFormat;
    for (uint32_t i = 0; i < m->nplanes; i++) {
        const CaptureSessionPlane *p = &m->planes[i];
        a[n++] = plane_attr[i][0]; a[n++] = fds[p->fdIndex];
        a[n++] = plane_attr[i][1]; a[n++] = (EGLint)p->offsetBytes;
        a[n++] = plane_attr[i][2]; a[n++] = (EGLint)p->strideBytes;
        a[n++] = plane_attr[i][3]; a[n++] = (EGLint)(m->drmModifier & 0xffffffffu);
        a[n++] = plane_attr[i][4]; a[n++] = (EGLint)(m->drmModifier >> 32);
    }

    /* effective per-frame colour interpretation (spec 6.3) */
    switch (m->matrix) {
        case 1: a[n++] = EGL_YUV_COLOR_SPACE_HINT_EXT; a[n++] = EGL_ITU_REC601_EXT; break;
        case 2: a[n++] = EGL_YUV_COLOR_SPACE_HINT_EXT; a[n++] = EGL_ITU_REC709_EXT; break;
        case 3: a[n++] = EGL_YUV_COLOR_SPACE_HINT_EXT; a[n++] = EGL_ITU_REC2020_EXT; break;
        default: break;
    }
    switch (m->range) {
        case 1: a[n++] = EGL_SAMPLE_RANGE_HINT_EXT; a[n++] = EGL_YUV_FULL_RANGE_EXT; break;
        case 2: a[n++] = EGL_SAMPLE_RANGE_HINT_EXT; a[n++] = EGL_YUV_NARROW_RANGE_EXT; break;
        default: break;
    }
    if (m->hSiting) { a[n++] = EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT;
                      a[n++] = m->hSiting == 1 ? EGL_YUV_CHROMA_SITING_0_EXT : EGL_YUV_CHROMA_SITING_0_5_EXT; }
    if (m->vSiting) { a[n++] = EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT;
                      a[n++] = m->vSiting == 1 ? EGL_YUV_CHROMA_SITING_0_EXT : EGL_YUV_CHROMA_SITING_0_5_EXT; }
    a[n++] = EGL_NONE;

    printf("import: bufferId=%016llx %ux%u fourcc=0x%08x (%.4s) modifier=0x%llx nfds=%u nplanes=%u\n",
           (unsigned long long)m->bufferId, m->backingWidth, m->backingHeight, m->drmFormat,
           (const char *)&m->drmFormat, (unsigned long long)m->drmModifier, m->nfds, m->nplanes);
    if (g_verbose) {
    for (uint32_t i = 0; i < m->nplanes; i++)
        printf("  plane %u: fdIndex=%u fd=%d offset=%u stride=%u\n", i, m->planes[i].fdIndex,
               fds[m->planes[i].fdIndex], m->planes[i].offsetBytes, m->planes[i].strideBytes);
    printf("  color matrix=%u range=%u hSiting=%u vSiting=%u; attr list (%d ints):", m->matrix, m->range,
           m->hSiting, m->vSiting, n);
    for (int i = 0; i < n; i++) printf(" 0x%x", (unsigned)a[i]);
    printf("\n");
    }

    e->image = create_image(c, a);
    /* EGL does not take FD ownership: close our copies on success and failure. */
    close_fds(fds, m->nfds);
    if (e->image == EGL_NO_IMAGE_KHR) {
        fprintf(stderr, "eglCreateImage failed: 0x%x (0x3008 BAD_DISPLAY, 0x300c BAD_PARAMETER, "
                        "0x3009 BAD_MATCH, 0x3002 BAD_ACCESS, 0x3003 BAD_ALLOC, 0x3004 BAD_ATTRIBUTE)\n",
                eglGetError());
        free(e);
        return NULL;
    }

    if (g_verbose) printf("  image=%p created\n", (void *)e->image);
    while (glGetError() != GL_NO_ERROR) {}      /* drop stale errors */
    glGenTextures(1, &e->tex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, e->tex);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    c->glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)e->image);
    {
        GLenum ge = glGetError();
        if (g_verbose) printf("  tex=%u glEGLImageTargetTexture2DOES -> glError 0x%x\n", e->tex, ge);
        if (ge != GL_NO_ERROR) {
        fprintf(stderr, "glEGLImageTargetTexture2DOES failed\n");
        entry_destroy(c, e);        /* full rollback: texture, image */
        return NULL;
        }
    }
    return e;
}

/* ------------------------------------------------------------------ */
/* drawing / frame handling                                            */
/* ------------------------------------------------------------------ */

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void draw(Client *c, const CaptureSessionFrameMsg *m, Entry *e)
{
    /* sample only the visible region of the backing */
    float u0 = (float)m->visX / m->backingWidth;
    float u1 = (float)(m->visX + m->visWidth) / m->backingWidth;
    float v0 = (float)m->visY / m->backingHeight;
    float v1 = (float)(m->visY + m->visHeight) / m->backingHeight;
    const GLfloat pos[] = { -1, 1,  -1, -1,  1, 1,  1, -1 };   /* top-left first */
    const GLfloat tc[]  = { u0, v0,  u0, v1,  u1, v0,  u1, v1 };

    glViewport(0, 0, c->width, c->height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(c->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, e->tex);
    glVertexAttribPointer(c->loc_pos, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer(c->loc_tc, 2, GL_FLOAT, GL_FALSE, 0, tc);
    glEnableVertexAttribArray(c->loc_pos);
    glEnableVertexAttribArray(c->loc_tc);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLenum ge_draw = glGetError();
    glFinish();                     /* GPU reads done before the server may release */
    {
        GLubyte px[4] = { 0, 0, 0, 0 };
        glReadPixels(c->width / 2, c->height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        EGLBoolean sw = eglSwapBuffers(c->dpy, c->egl_surface);
        if (c->frames < 5 || ge_draw != GL_NO_ERROR || !sw || (c->frames % 120) == 0)
            printf("draw #%llu: tex=%u uv=[%.3f,%.3f]-[%.3f,%.3f] viewport %dx%d glError=0x%x "
                   "centerRGBA=%u,%u,%u,%u swap=%d eglError=0x%x\n",
                   (unsigned long long)c->frames, e->tex, u0, v0, u1, v1, c->width, c->height, ge_draw,
                   px[0], px[1], px[2], px[3], (int)sw, eglGetError());
    }
    c->frames++;
}

static void send_ack(Client *c, uint64_t seq)
{
    CaptureSessionAckMsg ack = { CAPTURE_SESSION_MAGIC, 0, seq };
    if (send(c->sock, &ack, sizeof(ack), MSG_NOSIGNAL) < 0)
        perror("send ack");
}

static int msg_valid(const CaptureSessionFrameMsg *m)
{
    if (m->magic != CAPTURE_SESSION_MAGIC) return 0;
    if (m->nfds == 0 || m->nfds > CAPTURE_SESSION_MAX_FDS) return 0;
    if (m->nplanes == 0 || m->nplanes > CAPTURE_SESSION_MAX_PLANES) return 0;
    if (!m->backingWidth || !m->backingHeight) return 0;
    if (m->visX > m->backingWidth || m->visWidth > m->backingWidth - m->visX) return 0;
    if (m->visY > m->backingHeight || m->visHeight > m->backingHeight - m->visY) return 0;
    for (uint32_t i = 0; i < m->nplanes; i++)
        if (m->planes[i].fdIndex >= m->nfds) return 0;
    return 1;
}

static void handle_frame(Client *c, const CaptureSessionFrameMsg *m, int *fds)
{
    Entry *e;
    static long long prev_recv_ms;
    const long long t_recv = now_ms();

    const int log_frame = g_verbose || (m->seq % LOG_EVERY) == 0;

    if (log_frame)
    printf("[%lld +%lld] recv: seq=%llu bufferId=%016llx flushed=%u slots=%u backing=%ux%u vis=(%u,%u %ux%u) pts=%7lld ms cache=%u\n",
           t_recv, prev_recv_ms ? t_recv - prev_recv_ms : 0,
           (unsigned long long)m->seq, (unsigned long long)m->bufferId, m->flushed, m->slotCount,
           m->backingWidth, m->backingHeight, m->visX, m->visY, m->visWidth, m->visHeight,
           (long long)m->ptsNs/1000000LL, c->cache_count);

    /* Invalidate before any id lookup (spec 6.2).  Never restored, even if
     * the import below fails. */
    if (m->flushed) cache_flush(c);

    e = cache_lookup(c, m->bufferId);
    if (e) {
        if (g_verbose) printf("  cache hit (tex=%u)\n", e->tex);
        close_fds(fds, m->nfds);        /* hit: no import consumes the new FDs */
    } else {
        cache_evict_to(c, m->slotCount > 0 ? m->slotCount - 1 : 0);
        if (g_verbose) printf("  cache miss, importing\n");
        e = import_frame(c, m, fds);    /* closes fds either way */
        if (!e) fprintf(stderr, "  import FAILED for bufferId=%016llx, frame not drawn\n",
                        (unsigned long long)m->bufferId);
        if (e) {
            e->next = c->cache;
            c->cache = e;
            c->cache_count++;
        }
    }

    if (e) {
        e->lastUse = ++c->tick;
        e->last = *m;
        const long long t_draw = now_ms();
        draw(c, m, e);
        if (log_frame)
        printf("[%lld] drawn seq=%llu pts=%lld ms (import %lld ms, draw %lld ms)\n", now_ms(),
               (unsigned long long)m->seq, (long long)m->ptsNs/1000000LL, t_draw - t_recv, now_ms() - t_draw);
    }
    prev_recv_ms = t_recv;
    send_ack(c, m->seq);                /* always: the server owes a release */
}

/* Returns 0 ok, -1 on disconnect. */
static int recv_frame(Client *c)
{
    CaptureSessionFrameMsg m;
    union { char buf[CMSG_SPACE(sizeof(int) * CAPTURE_SESSION_MAX_FDS)]; struct cmsghdr align; } u;
    struct iovec iov = { &m, sizeof(m) };
    struct msghdr mh;
    int fds[CAPTURE_SESSION_MAX_FDS];

    memset(&mh, 0, sizeof(mh));
    memset(&u, 0, sizeof(u));
    for (int i = 0; i < CAPTURE_SESSION_MAX_FDS; i++) fds[i] = -1;
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    mh.msg_control = u.buf;
    mh.msg_controllen = sizeof(u.buf);

    ssize_t n = recvmsg(c->sock, &mh, MSG_CMSG_CLOEXEC);
    if (n <= 0) return (n < 0 && (errno == EAGAIN || errno == EINTR)) ? 0 : -1;

    unsigned nfd = 0;
    for (struct cmsghdr *h = CMSG_FIRSTHDR(&mh); h; h = CMSG_NXTHDR(&mh, h)) {
        if (h->cmsg_level == SOL_SOCKET && h->cmsg_type == SCM_RIGHTS) {
            unsigned cnt = (h->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            int *d = (int *)CMSG_DATA(h);
            for (unsigned i = 0; i < cnt; i++) {
                if (nfd < CAPTURE_SESSION_MAX_FDS) fds[nfd++] = d[i]; else close(d[i]);
            }
        }
    }

    if (n != (ssize_t)sizeof(m) || !msg_valid(&m) || nfd != m.nfds) {
        fprintf(stderr, "bad frame message: n=%zd (want %zu) magic=0x%x nfd=%u (msg says %u) valid=%d\n",
                n, sizeof(m), m.magic, nfd, m.nfds, n == (ssize_t)sizeof(m) ? msg_valid(&m) : -1);
        close_fds(fds, nfd);
        if (n == (ssize_t)sizeof(m) && m.magic == CAPTURE_SESSION_MAGIC) send_ack(c, m.seq);
        return 0;
    }
    handle_frame(c, &m, fds);
    return 0;
}

/* ------------------------------------------------------------------ */

static void print_usage(void)
{
    printf("Usage: capture_session_renderer [-socket PATH] [-size WxH] [-verbose]\n"
           "  -socket PATH   server socket (default %s)\n"
           "  -size WxH      window size (default 1920x1080)\n"
           "  -verbose       log every frame in full (default: recv/drawn every 60 frames)\n", CAPTURE_SESSION_DEFAULT_SOCKET);
}

/* Block until one key is pressed (any line if stdin is not a terminal). */
static void wait_for_key(void)
{
    struct termios old, raw;
    int tty = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old) == 0;
    char ch;

    if (tty) {
        raw = old;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    if (read(STDIN_FILENO, &ch, 1) <= 0) usleep(500 * 1000);    /* no stdin: do not spin */
    if (tty) tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

/* The feed is gone but the imported EGL images and textures are still valid:
 * redraw every cached frame, oldest first, one per key press, to prove the client
 * can render without the server. */
static void replay_cached_frames(Client *c)
{
    uint64_t prev = 0;
    unsigned idx = 0;

    printf("Server disconnected\n");
    for (;;) {
        Entry *best = NULL;
        for (Entry *e = c->cache; e; e = e->next)
            if (e->lastUse > prev && (!best || e->lastUse < best->lastUse)) best = e;
        if (!best) break;
        prev = best->lastUse;
        idx++;
        printf("Press any key to draw frames (%u/%u: bufferId=%016llx tex=%u)\n", idx, c->cache_count,
               (unsigned long long)best->bufferId, best->tex);
        wait_for_key();
        draw(c, &best->last, best);
        printf("  redrawn from cache without server\n");
    }
}

/* Connect (optionally retrying) to the feed.  Returns 0 on success. */
static int connect_server(Client *c, const char *path, int retry)
{
    for (;;) {
        struct sockaddr_un addr;
        c->sock = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
        if (c->sock >= 0 && connect(c->sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            printf("connected to %s\n", path);
            return 0;
        }
        if (!retry) { perror("connect"); return -1; }
        if (c->sock >= 0) close(c->sock);
        c->sock = -1;
        usleep(200 * 1000);
    }
}

int main(int argc, char *argv[])
{
    Client *c = &cl;
    const char *path = CAPTURE_SESSION_DEFAULT_SOCKET;

    setvbuf(stdout, NULL, _IOLBF, 0);   /* keep logs ordered/complete when redirected */
    c->width = 1920;
    c->height = 1080;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-socket") && i + 1 < argc) path = argv[++i];
        else if (!strcmp(argv[i], "-size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &c->width, &c->height);
        else if (!strcmp(argv[i], "-verbose")) g_verbose = 1;
        else { print_usage(); return 0; }
    }

    c->wl_display = wl_display_connect(NULL);
    if (!c->wl_display) { fprintf(stderr, "cannot connect to wayland display\n"); return -1; }
    c->registry = wl_display_get_registry(c->wl_display);
    wl_registry_add_listener(c->registry, &reg_listener, c);
    wl_display_roundtrip(c->wl_display);
    if (!c->compositor) { fprintf(stderr, "missing wl_compositor\n"); return -1; }

    c->surface = wl_compositor_create_surface(c->compositor);
    /* Like westeros-test, the plain wl_surface is enough on Westeros; wl_shell is
     * only used if the compositor offers it and WL_SHELL_TOPLEVEL is set. */
    if (c->shell && getenv("CAPTURE_SESSION_RENDER_USE_WL_SHELL")) {
        c->shell_surface = wl_shell_get_shell_surface(c->shell, c->surface);
        wl_shell_surface_add_listener(c->shell_surface, &shell_listener, c);
        wl_shell_surface_set_toplevel(c->shell_surface);
        wl_shell_surface_set_title(c->shell_surface, "capture_session_renderer");
    }

    if (init_gl(c) < 0) return -1;

    if (connect_server(c, path, 0) < 0) return -1;

    for (;;) {
        struct pollfd pfd[2];
        while (wl_display_prepare_read(c->wl_display) != 0) wl_display_dispatch_pending(c->wl_display);
        wl_display_flush(c->wl_display);
        pfd[0].fd = wl_display_get_fd(c->wl_display); pfd[0].events = POLLIN;
        pfd[1].fd = c->sock; pfd[1].events = POLLIN;

        if (poll(pfd, 2, -1) < 0 && errno != EINTR) { wl_display_cancel_read(c->wl_display); break; }
        if (pfd[0].revents & POLLIN) wl_display_read_events(c->wl_display);
        else wl_display_cancel_read(c->wl_display);
        wl_display_dispatch_pending(c->wl_display);

        if (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            if (recv_frame(c) < 0) {
                /* Feed went away (e.g. it is looping/restarting): drop everything
                 * imported from it and wait for it to come back. */
                close(c->sock);
                c->sock = -1;
                replay_cached_frames(c);
                printf("flushing cache and reconnecting\n");
                cache_flush(c);
                connect_server(c, path, 1);
            }
        }
    }

    cache_flush(c);
    eglMakeCurrent(c->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(c->dpy, c->egl_surface);
    eglDestroyContext(c->dpy, c->ctx);
    eglTerminate(c->dpy);
    wl_egl_window_destroy(c->egl_window);
    if (c->shell_surface) wl_shell_surface_destroy(c->shell_surface);
    wl_surface_destroy(c->surface);
    close(c->sock);
    wl_display_disconnect(c->wl_display);
    return 0;
}
