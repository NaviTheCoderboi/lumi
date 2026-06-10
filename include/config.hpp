#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <nanovg.h>

#include <filesystem>
#include <string>
#include <vector>

#include "app.hpp"
#include "spring.hpp"

namespace fs = std::filesystem;

struct SidesConfig {
    float left;
    float right;
    float top;
    float bottom;

    inline float horizontal() const { return left + right; }
    inline float vertical() const { return top + bottom; }
};

inline Spring makeDotSpring() {
    return Spring{20.f, SpringConfig{370.f, 18.f, 1.f}};
}

struct DockItem {
    App app;

    std::string label;
    bool active{false};

    Spring scaleSpring{1.f};
    Spring liftSpring{0.f};
    Spring dotSpring{makeDotSpring()};

    float scale() const { return scaleSpring.get(); }
    float lift() const { return liftSpring.get(); }
    float dotOffset() const { return dotSpring.get(); }
};

namespace DockDefaults {
constexpr float cornerRadius{28.f};

constexpr SidesConfig padding{10.f, 10.f, 10.f, 10.f};
constexpr SidesConfig margin{10.f, 10.f, 0.f, 0.f};

constexpr float itemSize{48.f};
constexpr float itemSpacing{14.f};
constexpr SidesConfig itemPadding{0.f, 0.f, 0.f, 0.f};
constexpr SidesConfig itemMargin{0.f, 0.f, 0.f, 0.f};

constexpr float activeDotSize{6.f};

constexpr float maxScale{1.5f};
constexpr float maxLiftAmount{2.f};
}  // namespace DockDefaults

struct DockConfig {
    float cornerRadius{DockDefaults::cornerRadius};

    SidesConfig padding{DockDefaults::padding};
    SidesConfig margin{DockDefaults::margin};

    float itemSize{DockDefaults::itemSize};
    float itemSpacing{DockDefaults::itemSpacing};
    SidesConfig itemPadding{DockDefaults::itemPadding};
    SidesConfig itemMargin{DockDefaults::itemMargin};

    float activeDotSize{DockDefaults::activeDotSize};

    float maxScale{DockDefaults::maxScale};
    float maxLiftAmount{DockDefaults::maxLiftAmount};

    std::vector<DockItem> items;

    inline float dockHeight() const {
        return padding.vertical() + itemMargin.vertical() + itemSize +
               itemPadding.vertical();
    }
    inline float dockWidth(int itemCount) const {
        if (itemCount <= 0) {
            return padding.horizontal() + itemMargin.horizontal();
        }

        float itemBlock{itemSize + itemPadding.horizontal()};

        return padding.horizontal() + itemMargin.horizontal() +
               itemCount * itemBlock + (itemCount - 1) * itemSpacing;
    }

    inline float surfaceHeight() const { return dockHeight() + margin.top; }

    inline float extraAnimationSpace() const {
        float scaleRise = (maxScale > 1.f) ? ((itemSize * maxScale) - itemSize) : 0.f;
        return maxLiftAmount + scaleRise;
    }

    inline float height() const {
        return surfaceHeight() + extraAnimationSpace();
    }

    static DockConfig& get() {
        static DockConfig instance;
        return instance;
    }

    bool reloadConfig();

   private:
    DockConfig() = default;

    DockConfig(DockConfig const&) = delete;
    DockConfig(DockConfig&&) = delete;
    DockConfig& operator=(DockConfig const&) = delete;
    DockConfig& operator=(DockConfig&&) = delete;

    static fs::path configFile();
};

DockItem makeItem(const std::string& className, bool active,
                  bool virtualApp = false);

#endif  // CONFIG_HPP