#include "gfx.hpp"

#include <EGL/egl.h>

#include <cstdlib>

#include "logger.hpp"

GfxContext* g_gfxContext = nullptr;

GfxContext::GfxContext(WaylandContext& wl, LayerSurface& ls) {
    g_gfxContext = this;
    display = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(wl.display));

    if (display == EGL_NO_DISPLAY ||
        !eglInitialize(display, nullptr, nullptr)) {
        logger::error("EGL init failed");
        std::exit(1);
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    static const EGLint configAttributes[]{EGL_RENDERABLE_TYPE,
                                           EGL_OPENGL_BIT,
                                           EGL_RED_SIZE,
                                           8,
                                           EGL_GREEN_SIZE,
                                           8,
                                           EGL_BLUE_SIZE,
                                           8,
                                           EGL_ALPHA_SIZE,
                                           8,
                                           EGL_STENCIL_SIZE,
                                           8,
                                           EGL_NONE};

    EGLint numConfigs;
    eglChooseConfig(display, configAttributes, &config, 1, &numConfigs);

    static const EGLint contextAttributes[]{
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);

    surface = eglCreateWindowSurface(
        display, config, reinterpret_cast<EGLNativeWindowType>(ls.eglWindow),
        nullptr);

    if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE) {
        logger::error("EGL surface or context creation failed");
        std::exit(1);
    }

    eglMakeCurrent(display, surface, surface, context);
}

GfxContext::~GfxContext() {
    g_gfxContext = nullptr;
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
    if (display != EGL_NO_DISPLAY) eglTerminate(display);
}
void GfxContext::swapBuffers() const { eglSwapBuffers(display, surface); }