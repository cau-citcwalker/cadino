#pragma once

#include <string>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Linear distance annotation between two points in the XY plane.
//
// The dimension line sits parallel to the (start, end) segment, offset
// perpendicularly by `offset` mm — positive offsets push the line to the
// segment's left when walking from start to end. Short extension witness
// lines drop from each endpoint to the dimension line.
struct Dimension {
    EntityId id{};
    EntityId group_id{};
    Eigen::Vector2d start{0.0, 0.0};
    Eigen::Vector2d end{0.0, 0.0};
    double offset{300.0};           // perpendicular distance from segment (mm)
    std::string text_override{};    // empty → measured length is drawn
    Color color{1.0f, 1.0f, 1.0f};
    double text_height{120.0};      // mm
    double arrow_size{60.0};        // mm

    [[nodiscard]] double measured_length() const noexcept {
        return (end - start).norm();
    }
};

}  // namespace cadino::core
