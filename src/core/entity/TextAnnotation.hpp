#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Free-floating text label in plan view coordinates. The text is
// anchored at `position`, drawn at `height` (mm) and rotated
// `rotation_z` radians around its anchor.
struct TextAnnotation {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    Eigen::Vector2d position{0.0, 0.0};
    std::string text{"Text"};
    double height{120.0};
    double rotation_z{0.0};
    Color color{1.0f, 1.0f, 1.0f};
};

}  // namespace cadino::core
