#pragma once

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Wall {
    EntityId id{};
    Eigen::Vector2d start{0.0, 0.0};
    Eigen::Vector2d end{0.0, 0.0};
    double height{2400.0};
    double thickness{200.0};
    Color color{0.78f, 0.78f, 0.80f};

    [[nodiscard]] double length() const noexcept;
    [[nodiscard]] Eigen::Vector2d direction() const noexcept;
    [[nodiscard]] Eigen::Vector2d normal() const noexcept;
};

}  // namespace cadino::core
