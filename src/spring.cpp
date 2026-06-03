#include "spring.hpp"

#include <cmath>

Spring::Spring(float value, SpringConfig config)
    : current{value}, target{value}, config{config} {}

void Spring::setTarget(float newTarget) { target = newTarget; }

void Spring::update(float dt) {
    if (!std::isfinite(current) || !std::isfinite(target) ||
        !std::isfinite(velocityValue)) {
        current = target;
        velocityValue = 0.f;
        return;
    }

    float displacement{current - target};

    float springForce{-config.stiffness * displacement};
    float dampingForce{-config.damping * velocityValue};

    float acceleration{(springForce + dampingForce) / config.mass};

    velocityValue += acceleration * dt;
    current += velocityValue * dt;

    if (!std::isfinite(current) || !std::isfinite(velocityValue)) {
        current = target;
        velocityValue = 0.f;
        return;
    }

    if (std::fabs(velocityValue) < 0.001f &&
        std::fabs(target - current) < 0.001f) {
        velocityValue = 0.f;
        current = target;
    }
}

float Spring::get() const { return current; }

float Spring::velocity() const { return velocityValue; }

bool Spring::resting() const {
    return velocityValue == 0.f && current == target;
}