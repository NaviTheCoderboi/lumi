#include "dock.hpp"

#include <cmath>
#include <cstddef>

#include "app.hpp"
#include "config.hpp"
#include "context.hpp"
#include "surface.hpp"

void drawGlassDock(NVGcontext* vg, float x, float y, float w, float h,
                   float r) {
    // shadow
    NVGpaint shadow{nvgBoxGradient(vg, x, y + h * 0.5f, w, h * 0.5f, r, 24.f,
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.22f),
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x - 20.f, y, w + 40.f, h + 32.f, r);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    // fill
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.08f));
    nvgFill(vg);

    // top half glow
    NVGpaint glow{nvgLinearGradient(vg, x, y, x, y + h,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.12f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    // outer border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, r);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.30f));
    nvgStroke(vg);

    float midX{x + w * 0.5f};

    // left half tint
    NVGpaint rimL{nvgLinearGradient(vg, x + r, y, midX, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.65f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + r, y + 1.f);
    nvgLineTo(vg, midX, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimL);
    nvgStroke(vg);

    // right half tint
    NVGpaint rimR{nvgLinearGradient(vg, midX, y, x + w - r, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.65f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, midX, y + 1.f);
    nvgLineTo(vg, x + w - r, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimR);
    nvgStroke(vg);
}

static void updateDockAnimations(std::vector<DockItem>& items, float mouseX,
                                 float mouseY, float dockStartX,
                                 float baseBottomY, float itemSpacing,
                                 float itemSize, float dt) {
    DockConfig& config{DockConfig::get()};

    float influenceRadius{itemSize * 2.3f};

    for (std::size_t i{0}; i < items.size(); i++) {
        float iconSize{itemSize * items[i].scale()};
        float iconCenter{dockStartX + i * itemSpacing + itemSize * 0.5f};

        float distance{std::abs(mouseX - iconCenter)};

        float iconBottom{baseBottomY + items[i].lift()};
        float iconTop{iconBottom - iconSize};

        bool insideY{mouseY >= iconTop && mouseY <= iconBottom};
        float influence{
            insideY ? std::max(0.f, 1.f - distance / influenceRadius) : 0.f};

        influence *= influence;

        float targetScale{1.f + influence * (config.maxScale - 1.f)};
        float targetLift{(config.maxScale == 1.f) ? 0.f : -influence * config.maxLiftAmount};

        items[i].scaleSpring.setTarget(targetScale);
        items[i].liftSpring.setTarget(targetLift);
        items[i].scaleSpring.update(dt);
        items[i].liftSpring.update(dt);
    }
}

void handleDock(NVGcontext* vg, IconRenderer& iconRenderer, LayerSurface& ls,
                float dt) {
    int w = ls.width;
    int h = ls.height;

    auto& config{DockConfig::get()};
    auto& mouseCtx{MouseContext::get()};
    auto& iconIndex{IconIndex::get()};    auto& items{config.items};
    int itemCount{static_cast<int>(items.size())};

    if (itemCount == 0) return;

    float baseSize{config.itemSize};
    float spacing{config.itemSpacing};

    float estimatedWidth{config.padding.horizontal() +
                         config.itemMargin.horizontal() + itemCount * baseSize +
                         (itemCount - 1) * spacing};
    float visualDockHeight{config.padding.vertical() +
                           config.itemMargin.vertical() + baseSize};

    float dockX{(w - estimatedWidth) * 0.5f};
    float dockY{h - visualDockHeight};
    float startX{dockX + config.padding.left + config.itemMargin.left};

    float hoverX{(mouseCtx.inside && mouseCtx.currentSurface == ls.surface) ? mouseCtx.x : -9999.f};
    float hoverY{(mouseCtx.inside && mouseCtx.currentSurface == ls.surface) ? mouseCtx.y : -9999.f};
    float baseBottomY{dockY + config.padding.top + config.itemMargin.top +
                      baseSize};

    updateDockAnimations(items, hoverX, hoverY, startX, baseBottomY,
                         baseSize + spacing, baseSize, dt);

    float animatedWidth{config.padding.horizontal() +
                         config.itemMargin.horizontal()};

    for (int i{0}; i < itemCount; i++) {
        animatedWidth += baseSize * items[i].scale();

        if (i != itemCount - 1) {
            animatedWidth += spacing;
        }
    }

    dockX = (w - animatedWidth) * 0.5f;



    static bool prevPressed{false};
    if (mouseCtx.pressed && !prevPressed) {
        if (popupSurface.surface && mouseCtx.currentSurface != popupSurface.surface) {
            destroyPopup();
        }

        if (mouseCtx.currentSurface == ls.surface) {
            float cx{dockX + config.padding.left + config.itemMargin.left};

            mouseCtx.lastClickIndex = -1;

            for (int i{0}; i < itemCount; i++) {
                float iconSize{baseSize * items[i].scale()};
                float iconBottom{baseBottomY + items[i].lift()};
                float iconTop{iconBottom - iconSize};
                bool insideY{mouseCtx.clickY >= iconTop &&
                             mouseCtx.clickY <= iconBottom};

                if (insideY && mouseCtx.clickX >= cx &&
                    mouseCtx.clickX <= cx + iconSize) {
                    mouseCtx.lastClickIndex = i;
                    break;
                }

                cx += iconSize + spacing;
            }

            if (mouseCtx.lastClickIndex != -1) {
                auto& clickedItem{items.at(mouseCtx.lastClickIndex)};
                clickedItem.app.launch();
            }
        }
    }
    prevPressed = mouseCtx.pressed;

    // Handle Right Clicks
    static bool prevRightPressed{false};
    if (mouseCtx.rightPressed && !prevRightPressed) {
        if (mouseCtx.currentSurface == ls.surface) {
            float cx{dockX + config.padding.left + config.itemMargin.left};
            int clickedIndex = -1;
            float clickedIconX = 0.f;
            float clickedIconY = 0.f;
            float clickedIconSize = 0.f;

            for (int i{0}; i < itemCount; i++) {
                float iconSize{baseSize * items[i].scale()};
                float iconBottom{baseBottomY + items[i].lift()};
                float iconTop{iconBottom - iconSize};
                bool insideY{mouseCtx.rightClickY >= iconTop &&
                             mouseCtx.rightClickY <= iconBottom};

                if (insideY && mouseCtx.rightClickX >= cx &&
                    mouseCtx.rightClickX <= cx + iconSize) {
                    clickedIndex = i;
                    clickedIconX = cx;
                    clickedIconY = iconTop;
                    clickedIconSize = iconSize;
                    break;
                }

                cx += iconSize + spacing;
            }

            if (clickedIndex != -1) {
                auto& clickedItem{items.at(clickedIndex)};
                const auto& actions = clickedItem.app.actions;
                if (!actions.empty()) {
                    float menuItemHeight = 36.f;
                    float padding = 16.f;
                    int menuHeight = static_cast<int>((actions.size() * menuItemHeight) + padding);
                    int menuWidth = 180;
                    
                    createPopup(ls, clickedIndex, static_cast<int>(clickedIconX), static_cast<int>(clickedIconY), static_cast<int>(clickedIconSize), static_cast<int>(clickedIconSize), menuWidth, menuHeight, mouseCtx.rightClickSerial);
                } else {
                    if (popupSurface.surface) destroyPopup();
                }
            } else {
                if (popupSurface.surface) destroyPopup();
            }
        }
    }
    prevRightPressed = mouseCtx.rightPressed;

    // Draw the Dock Background
    drawGlassDock(vg, dockX, dockY, animatedWidth, visualDockHeight,
                  config.cornerRadius);

    // Draw the Dock Icons
    float currentX{dockX + config.padding.left + config.itemMargin.left};
    for (int i{0}; i < itemCount; i++) {
        auto& item{items[i]};
        auto iconPath{iconIndex.find(item.app)};

        float iconSize{baseSize * item.scale()};
        float x{currentX + iconSize * 0.5f};
        float y{baseBottomY - (iconSize * 0.5f) + item.lift()};

        item.dotSpring.update(dt);

        if (iconPath) {
            iconRenderer.draw(*iconPath, x, y, iconSize, iconSize * 0.2f);
        }

        if (item.active) {
            float dotRadius{config.activeDotSize * 0.5f};
            float dotY{baseBottomY + dotRadius + 2.f +
                       items[i].dotSpring.get()};

            nvgBeginPath(vg);
            nvgCircle(vg, x, dotY, dotRadius);
            nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.9f));
            nvgFill(vg);
        }

        currentX += iconSize + spacing;
    }

}

void handlePopup(NVGcontext* vg, PopupSurface& popup) {
    auto& config{DockConfig::get()};
    auto& mouseCtx{MouseContext::get()};
    auto& items{config.items};

    if (popup.sourceAppIndex < 0 || popup.sourceAppIndex >= static_cast<int>(items.size())) return;

    drawGlassDock(vg, 0.f, 0.f, popup.width, popup.height, 12.f);

    auto& clickedItem{items.at(popup.sourceAppIndex)};
    const auto& actions = clickedItem.app.actions;

    static bool prevPressed = false;
    
    float itemY = 8.f;
    for (std::size_t i = 0; i < actions.size(); i++) {
        float rowY = itemY + i * 36.f;

        bool hovered = false;
        if (mouseCtx.inside && mouseCtx.currentSurface == popup.surface &&
            mouseCtx.x >= 0 && mouseCtx.x <= popup.width &&
            mouseCtx.y >= rowY && mouseCtx.y < rowY + 36.f) {
            hovered = true;
        }

        if (hovered && mouseCtx.pressed && !prevPressed) {
            clickedItem.app.launchAction(actions[i]);
            destroyPopup();
            break;
        }

        if (hovered) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, 6.f, rowY + 2.f, popup.width - 12.f, 32.f, 6.f);
            nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.12f));
            nvgFill(vg);
        }

        nvgFontSize(vg, 13.f);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        if (hovered) {
            nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.95f));
        } else {
            nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.75f));
        }

        nvgText(vg, 16.f, rowY + 18.f, actions[i].displayName.c_str(), nullptr);
    }
    
    prevPressed = mouseCtx.pressed;
}
