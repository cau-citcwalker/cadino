#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

struct Wall {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    Eigen::Vector2d start{0.0, 0.0};
    Eigen::Vector2d end{0.0, 0.0};
    double height{2400.0};
    double thickness{200.0};
    // Out-of-plane rotation around the wall's geometric centre. Walls
    // normally stay plumb (rotation_x = rotation_y = 0); these let the
    // gizmo tilt them in 3D. When either is non-zero, push_wall_box
    // renders the wall as a single tilted slab (cutouts are skipped).
    double rotation_x{0.0};
    double rotation_y{0.0};
    Color color{0.78f, 0.78f, 0.80f};
    float roughness{0.85f};
    float metallic{0.0f};
    int pattern{0};
    std::string texture_path;

    [[nodiscard]] double length() const noexcept;
    [[nodiscard]] Eigen::Vector2d direction() const noexcept;
    [[nodiscard]] Eigen::Vector2d normal() const noexcept;
};

}  // namespace cadino::core
