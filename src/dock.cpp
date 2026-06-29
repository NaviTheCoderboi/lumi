#include "dock.hpp"

#include <cmath>

#include "app.hpp"
#include "config.hpp"
#include "context.hpp"
#include "popup.hpp"

static bool isSeparator(const std::variant<bool, DockItem>& v) {
    return std::holds_alternative<bool>(v);
}

void drawGlassDock(NVGcontext* vg, float x, float y, float w, float h, float r,
                   NVGcolor bgColor) {
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
    nvgFillColor(vg, bgColor);
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

static void updateDockAnimations(
    std::vector<std::variant<bool, DockItem>>& items,
    const SeparatorConfig& separatorConfig, float mouseX, float mouseY,
    float dockStartX, float baseBottomY, float spacing, float itemSize,
    float dt) {
    DockConfig& config{DockConfig::get()};
    MouseContext& mouseCtx{MouseContext::get()};

    float influenceRadius{itemSize * 2.3f};

    mouseCtx.dndHoverIndex = -1;
    float currentX{dockStartX};
    int idx{0};

    for (auto& itemVar : items) {
        if (isSeparator(itemVar)) {
            currentX += separatorConfig.thickness + spacing;
            idx++;
            continue;
        }

        auto& item{std::get<DockItem>(itemVar)};
        float iconCenter{currentX + itemSize * 0.5f};

        float distance{std::abs(mouseX - iconCenter)};

        float iconSize{itemSize * item.scale()};
        float iconBottom{baseBottomY + item.lift()};
        float iconTop{iconBottom - iconSize};

        bool insideY{mouseY >= iconTop && mouseY <= iconBottom};
        float influence{
            (insideY ? std::max(0.f, 1.f - distance / influenceRadius) : 0.f)};
        influence *= influence;

        float targetScale{1};
        float targetLift{0};

        if (mouseCtx.dndActive) {
            float iconLeft{iconCenter - iconSize * 0.5f};
            float iconRight{iconCenter + iconSize * 0.5f};
            if (insideY && mouseX >= iconLeft && mouseX <= iconRight) {
                mouseCtx.dndHoverIndex = idx;
            }

            if (mouseCtx.dndHoverIndex == idx) {
                targetScale = config.maxScale;
                targetLift =
                    (config.maxScale == 1.f) ? 0.f : -config.maxLiftAmount;
            };
        } else {
            targetScale = 1.f + influence * (config.maxScale - 1.f);
            targetLift = (config.maxScale == 1.f)
                             ? 0.f
                             : -influence * config.maxLiftAmount;
        }

        item.scaleSpring.setTarget(targetScale);
        item.liftSpring.setTarget(targetLift);
        item.scaleSpring.update(dt);
        item.liftSpring.update(dt);

        currentX += itemSize + spacing;
        idx++;
    }
}

void handleDock(NVGcontext* vg, IconRenderer& iconRenderer, LayerSurface& ls,
                float dt) {
    int w{ls.width};
    int h{ls.height};

    auto& config{DockConfig::get()};
    auto& mouseCtx{MouseContext::get()};
    auto& iconIndex{IconIndex::get()};
    auto& items{config.items};
    auto& popup{Popup::get()};
    int itemCount{static_cast<int>(items.size())};

    int appCount{0};
    for (const auto& itemVar : items) {
        if (!isSeparator(itemVar)) appCount++;
    }
    if (appCount == 0) return;

    float baseSize{config.itemSize};
    float spacing{config.itemSpacing};
    float sepThickness{config.separator.thickness};

    float estimatedWidth{config.padding.horizontal() +
                         config.itemMargin.horizontal()};
    for (const auto& itemVar : items) {
        estimatedWidth += isSeparator(itemVar) ? sepThickness : baseSize;
    }
    if (itemCount > 0) estimatedWidth += (itemCount - 1) * spacing;

    float visualDockHeight{config.padding.vertical() +
                           config.itemMargin.vertical() + baseSize};

    float dockX{(w - estimatedWidth) * 0.5f};
    float dockY{h - visualDockHeight};
    float startX{dockX + config.padding.left + config.itemMargin.left};

    float hoverX{-9999.f};
    float hoverY{-9999.f};

    if (mouseCtx.dndActive) {
        hoverX = mouseCtx.x;
        hoverY = mouseCtx.y;
    } else if (popup.isOpen()) {
        hoverX = mouseCtx.rightClickX;
        hoverY = mouseCtx.rightClickY;
    } else if (mouseCtx.inside && mouseCtx.currentSurface == ls.surface) {
        hoverX = mouseCtx.x;
        hoverY = mouseCtx.y;
    }

    float baseBottomY{dockY + config.padding.top + config.itemMargin.top +
                      baseSize};

    updateDockAnimations(items, config.separator, hoverX, hoverY, startX,
                         baseBottomY, spacing, baseSize, dt);

    float animatedWidth{config.padding.horizontal() +
                        config.itemMargin.horizontal()};

    for (int i{0}; i < itemCount; i++) {
        if (isSeparator(items[i])) {
            animatedWidth += sepThickness;
        } else {
            animatedWidth += baseSize * std::get<DockItem>(items[i]).scale();
        }

        if (i != itemCount - 1) {
            animatedWidth += spacing;
        }
    }

    dockX = (w - animatedWidth) * 0.5f;

    static bool prevPressed{false};
    if (mouseCtx.pressed && !prevPressed) {
        if (popup.isOpen() && mouseCtx.currentSurface != popup.wlSurface()) {
            popup.destroy();
        }

        if (mouseCtx.currentSurface == ls.surface) {
            float cx{dockX + config.padding.left + config.itemMargin.left};

            mouseCtx.lastClickIndex = -1;

            for (int i{0}; i < itemCount; i++) {
                if (isSeparator(items[i])) {
                    cx += sepThickness + spacing;
                    continue;
                }

                auto& item{std::get<DockItem>(items[i])};
                float iconSize{baseSize * item.scale()};
                float iconBottom{baseBottomY + item.lift()};
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
                auto& clickedItem{
                    std::get<DockItem>(items.at(mouseCtx.lastClickIndex))};
                clickedItem.app.launch();
            }
        }
    }
    prevPressed = mouseCtx.pressed;

    static bool prevRightPressed{false};
    if (mouseCtx.rightPressed && !prevRightPressed) {
        if (mouseCtx.currentSurface == ls.surface) {
            float cx{dockX + config.padding.left + config.itemMargin.left};
            int clickedIndex{-1};
            float clickedIconX{0.f};
            float clickedIconY{0.f};
            float clickedIconSize{0.f};

            for (int i{0}; i < itemCount; i++) {
                if (isSeparator(items[i])) {
                    cx += sepThickness + spacing;
                    continue;
                }

                auto& item{std::get<DockItem>(items[i])};
                float iconSize{baseSize * item.scale()};
                float iconBottom{baseBottomY + item.lift()};
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
                auto& clickedItem{std::get<DockItem>(items.at(clickedIndex))};
                const auto& actions{clickedItem.app.actions};
                if (!actions.empty()) {
                    float menuItemHeight{
                        std::round(config.font.size * 2.f + 10.f)};
                    float padding{std::round(config.font.size * 0.6f + 8.f)};
                    int menuHeight{static_cast<int>(
                        (actions.size() * menuItemHeight) + padding)};

                    float maxTextWidth{0.f};
                    nvgSave(vg);
                    nvgFontSize(vg, config.font.size);
                    nvgFontFace(vg, config.font.name.c_str());
                    for (const auto& action : actions) {
                        float bounds[4];
                        nvgTextBounds(vg, 0.f, 0.f, action.displayName.c_str(),
                                      nullptr, bounds);

                        float w{bounds[2] - bounds[0]};
                        if (w > maxTextWidth) maxTextWidth = w;
                    }
                    nvgRestore(vg);

                    float paddingX{std::round(config.font.size * 0.8f + 20.f)};
                    int menuWidth{static_cast<int>(
                        std::max(180.f, maxTextWidth + paddingX))};

                    popup.create(ls, clickedIndex,
                                 static_cast<int>(clickedIconX),
                                 static_cast<int>(clickedIconY),
                                 static_cast<int>(clickedIconSize),
                                 static_cast<int>(clickedIconSize), menuWidth,
                                 menuHeight, mouseCtx.rightClickSerial);
                } else {
                    if (popup.isOpen()) popup.destroy();
                }
            } else {
                if (popup.isOpen()) popup.destroy();
            }
        }
    }
    prevRightPressed = mouseCtx.rightPressed;

    drawGlassDock(vg, dockX, dockY, animatedWidth, visualDockHeight,
                  config.cornerRadius, config.backgroundColor.toNVG());

    float currentX{dockX + config.padding.left + config.itemMargin.left};
    for (int i{0}; i < itemCount; i++) {
        if (isSeparator(items[i])) {
            float sepHeight{baseSize * 0.7f};
            float sepCenterY{baseBottomY - baseSize * 0.5f};
            float sepY{sepCenterY - sepHeight * 0.5f};

            nvgBeginPath(vg);
            nvgRoundedRect(vg, currentX, sepY, config.separator.thickness,
                           sepHeight, config.separator.borderRadius);
            nvgFillColor(vg, config.separator.color.toNVG());
            nvgFill(vg);

            currentX += sepThickness + spacing;
            continue;
        }

        auto& item{std::get<DockItem>(items[i])};
        auto iconPath{iconIndex.find(item.app)};

        float iconSize{baseSize * item.scale()};
        float x{currentX + iconSize * 0.5f};
        float y{baseBottomY - (iconSize * 0.5f) + item.lift()};

        if (popup.isOpen() && popup.sourceAppIndex() == i) {
            float iconBottom{baseBottomY + item.lift()};
            float iconTop{iconBottom - iconSize};
            popup.reposition(
                static_cast<int>(currentX), static_cast<int>(iconTop),
                static_cast<int>(iconSize), static_cast<int>(iconSize));
        }

        item.dotSpring.update(dt);

        if (iconPath) {
            iconRenderer.draw(*iconPath, x, y, iconSize, iconSize * 0.2f);
        }

        if (item.active) {
            float dotRadius{config.activeDotSize * 0.5f};
            float dotY{baseBottomY + dotRadius + 2.f + item.dotSpring.get()};

            nvgBeginPath(vg);
            nvgCircle(vg, x, dotY, dotRadius);
            nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.9f));
            nvgFill(vg);
        }

        currentX += iconSize + spacing;
    }
}
