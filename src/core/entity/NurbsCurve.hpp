#pragma once

#include <vector>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Open / clamped uniform B-spline curve in 3D.
//
// The knot vector is generated automatically (clamped, uniform) whenever the
// control points or degree change, so callers only need to manage the control
// polygon and the desired polynomial degree.
struct NurbsCurve {
    EntityId id{};
    EntityId group_id{};
    int degree{3};
    std::vector<Eigen::Vector3d> control_points;
    Color color{0.20f, 0.85f, 0.95f};
    float line_width{2.0f};

    [[nodiscard]] int effective_degree() const noexcept;
    [[nodiscard]] std::vector<double> knot_vector() const;

    // Evaluate the curve at parameter t in [0, 1]. Returns a zero vector when
    // the curve does not have enough control points to be drawable.
    [[nodiscard]] Eigen::Vector3d evaluate(double t) const;

    // Uniformly samples the curve into `sample_count` points.
    [[nodiscard]] std::vector<Eigen::Vector3d> tessellate(int sample_count = 128) const;
};

}  // namespace cadino::core
