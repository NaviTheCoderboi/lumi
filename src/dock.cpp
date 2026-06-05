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
        float targetLift{-influence * config.maxLiftAmount};

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
    auto& iconIndex{IconIndex::get()};
    auto& menuState{ContextMenuState::get()};

    auto& items{config.items};
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

    // Calculate menu Y-coordinate if open so that clicks are evaluated with the correct geometry
    if (menuState.isOpen) {
        menuState.y = dockY - menuState.height - 12.f;
    }

    float hoverX{mouseCtx.inside ? mouseCtx.x : -9999.f};
    float hoverY{mouseCtx.inside ? mouseCtx.y : -9999.f};
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

    // Handle Left Clicks
    static bool prevPressed{false};
    if (mouseCtx.pressed && !prevPressed) {
        if (menuState.isOpen) {
            float mx = mouseCtx.clickX;
            float my = mouseCtx.clickY;

            if (mx >= menuState.x && mx <= menuState.x + menuState.width &&
                my >= menuState.y && my <= menuState.y + menuState.height) {
                // Clicked inside context menu
                float relativeY = my - menuState.y - 8.f;
                int actionIndex = static_cast<int>(relativeY / 36.f);

                auto& clickedItem{items.at(menuState.sourceAppIndex)};
                const auto& actions = clickedItem.app.actions;
                if (actionIndex >= 0 && actionIndex < static_cast<int>(actions.size())) {
                    clickedItem.app.launchAction(actions[actionIndex]);
                }
            }

            // Always close menu on click when open
            menuState.isOpen = false;
            menuState.sourceAppIndex = -1;
            ls.isResizing = true;
            zwlr_layer_surface_v1_set_size(ls.layerSurface, 0, static_cast<int>(config.height()));
            wl_surface_commit(ls.surface);
        } else {
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
        float cx{dockX + config.padding.left + config.itemMargin.left};
        int clickedIndex = -1;

        for (int i{0}; i < itemCount; i++) {
            float iconSize{baseSize * items[i].scale()};
            float iconBottom{baseBottomY + items[i].lift()};
            float iconTop{iconBottom - iconSize};
            bool insideY{mouseCtx.rightClickY >= iconTop &&
                         mouseCtx.rightClickY <= iconBottom};

            if (insideY && mouseCtx.rightClickX >= cx &&
                mouseCtx.rightClickX <= cx + iconSize) {
                clickedIndex = i;
                break;
            }

            cx += iconSize + spacing;
        }

        if (clickedIndex != -1) {
            auto& clickedItem{items.at(clickedIndex)};
            const auto& actions = clickedItem.app.actions;
            if (!actions.empty()) {
                menuState.isOpen = true;
                menuState.sourceAppIndex = clickedIndex;

                float menuItemHeight = 36.f;
                float padding = 16.f;
                menuState.height = (actions.size() * menuItemHeight) + padding;
                menuState.width = 180.f;

                // Re-calculate the horizontal start of the clicked icon to center the menu
                float clickCx = dockX + config.padding.left + config.itemMargin.left;
                for (int i{0}; i < clickedIndex; i++) {
                    clickCx += baseSize * items[i].scale() + spacing;
                }
                float iconSize{baseSize * clickedItem.scale()};
                float iconCenterX = clickCx + iconSize * 0.5f;
                menuState.x = iconCenterX - menuState.width * 0.5f;

                if (menuState.x < 10.f) menuState.x = 10.f;
                if (menuState.x + menuState.width > w - 10.f) {
                    menuState.x = w - menuState.width - 10.f;
                }

                int totalHeight = static_cast<int>(config.height() + menuState.height + 12.f);
                ls.isResizing = true;
                zwlr_layer_surface_v1_set_size(ls.layerSurface, 0, totalHeight);
                wl_surface_commit(ls.surface);
            } else {
                if (menuState.isOpen) {
                    menuState.isOpen = false;
                    menuState.sourceAppIndex = -1;
                    ls.isResizing = true;
                    zwlr_layer_surface_v1_set_size(ls.layerSurface, 0, static_cast<int>(config.height()));
                    wl_surface_commit(ls.surface);
                }
            }
        } else {
            if (menuState.isOpen) {
                menuState.isOpen = false;
                menuState.sourceAppIndex = -1;
                ls.isResizing = true;
                zwlr_layer_surface_v1_set_size(ls.layerSurface, 0, static_cast<int>(config.height()));
                wl_surface_commit(ls.surface);
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

    // Draw the Context Menu (if open)
    if (menuState.isOpen && menuState.sourceAppIndex >= 0 && menuState.sourceAppIndex < itemCount) {
        drawGlassDock(vg, menuState.x, menuState.y, menuState.width, menuState.height, 12.f);

        auto& clickedItem{items.at(menuState.sourceAppIndex)};
        const auto& actions = clickedItem.app.actions;

        float itemY = menuState.y + 8.f;
        for (std::size_t i = 0; i < actions.size(); i++) {
            float rowY = itemY + i * 36.f;

            bool hovered = false;
            if (mouseCtx.inside &&
                mouseCtx.x >= menuState.x && mouseCtx.x <= menuState.x + menuState.width &&
                mouseCtx.y >= rowY && mouseCtx.y < rowY + 36.f) {
                hovered = true;
            }

            if (hovered) {
                nvgBeginPath(vg);
                nvgRoundedRect(vg, menuState.x + 6.f, rowY + 2.f, menuState.width - 12.f, 32.f, 6.f);
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

            nvgText(vg, menuState.x + 16.f, rowY + 18.f, actions[i].name.c_str(), nullptr);
        }
    }
}
