#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Text label with a leader line + arrowhead pointing back at a target
// point. The arrow lives at `anchor`; the text sits at `text_position`.
struct Leader {
    EntityId id{};
    EntityId group_id{};
    EntityId layer_id{};
    Eigen::Vector2d anchor{0.0, 0.0};
    Eigen::Vector2d text_position{0.0, 0.0};
    int plane{0};  // 0=Top, 1=Front, 2=Right — see Dimension for details
    std::string text{"Note"};
    double height{120.0};
    double arrow_size{80.0};
    Color color{0.10f, 0.10f, 0.12f};
};

}  // namespace cadino::core
