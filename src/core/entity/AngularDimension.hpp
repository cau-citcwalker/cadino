#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Three-point angular dimension: arms p1 and p2 share the vertex, the
// dimension arc sits at `radius` mm out from the vertex. Sweep direction
// is from p1 to p2.
struct AngularDimension {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    Eigen::Vector2d vertex{0.0, 0.0};
    Eigen::Vector2d p1{0.0, 0.0};
    Eigen::Vector2d p2{0.0, 0.0};
    int plane{0};  // 0=Top, 1=Front, 2=Right
    double radius{600.0};
    double text_height{120.0};
    double arrow_size{60.0};
    std::string text_override{};
    Color color{0.10f, 0.10f, 0.12f};

    [[nodiscard]] double angle_rad() const noexcept;
};

}  // namespace cadino::core
