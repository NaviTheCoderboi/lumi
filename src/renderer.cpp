#include "renderer.hpp"

#include <epoxy/gl.h>
#include <nanovg.h>
#include <nanovg_gl.h>

#include <cstdlib>

#include "logger.hpp"

Renderer::Renderer() {
    vg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!vg) {
        logger::error("NanoVG init failed");
        std::exit(1);
    }

    int font = nvgCreateFont(vg, "sans", "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf");
    if (font == -1) {
        font = nvgCreateFont(vg, "sans", "/usr/share/fonts/TTF/JetBrainsMono-Medium.ttf");
    }
    if (font == -1) {
        font = nvgCreateFont(vg, "sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    }
    if (font == -1) {
        font = nvgCreateFont(vg, "sans", "/usr/share/fonts/TTF/DejaVuSans.ttf");
    }

    if (font == -1) {
        logger::warning("Could not load a valid system font for context menu!");
    } else {
        logger::info("Loaded context menu font successfully");
    }
}

Renderer::~Renderer() {
    if (vg) nvgDeleteGLES3(vg);
}

void Renderer::clearViewport(int w, int h) const {
    glViewport(0, 0, w, h);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::beginFrame(int w, int h, float dpr) const {
    nvgBeginFrame(vg, static_cast<float>(w), static_cast<float>(h), dpr);
}

void Renderer::endFrame() const { nvgEndFrame(vg); }
