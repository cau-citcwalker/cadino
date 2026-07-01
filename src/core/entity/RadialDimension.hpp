#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Annotates a circular feature. The line runs from `center` past the
// circle edge to `label_position`; if `is_diameter` is true the label
// uses the diameter prefix.
struct RadialDimension {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    Eigen::Vector2d center{0.0, 0.0};
    double radius{500.0};
    Eigen::Vector2d label_position{0.0, 0.0};
    int plane{0};  // 0=Top, 1=Front, 2=Right
    bool is_diameter{false};
    double text_height{120.0};
    double arrow_size{60.0};
    std::string text_override{};
    Color color{0.10f, 0.10f, 0.12f};
};

}  // namespace cadino::core
