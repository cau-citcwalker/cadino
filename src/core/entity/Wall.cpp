#include "entity/Wall.hpp"

namespace cadino::core {

double Wall::length() const noexcept {
    return (end - start).norm();
}

Eigen::Vector2d Wall::direction() const noexcept {
    const double l = length();
    if (l < 1e-9) {
        return {1.0, 0.0};
    }
    return (end - start) / l;
}

Eigen::Vector2d Wall::normal() const noexcept {
    const auto d = direction();
    return {-d.y(), d.x()};
}

}  // namespace cadino::core
