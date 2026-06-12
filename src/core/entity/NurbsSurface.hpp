#pragma once

#include <vector>

#include <Eigen/Core>

#include "Color.hpp"
#include "EntityId.hpp"

namespace cadino::core {

// Tensor-product B-spline surface with clamped uniform knot vectors in both
// directions. Control points are stored row-major in a `rows × cols` grid.
struct NurbsSurface {
    EntityId id{};
    EntityId group_id{};
    int degree_u{3};
    int degree_v{3};
    int rows{0};  // number of control points along U (along the v knot direction)
    int cols{0};  // number of control points along V
    std::vector<Eigen::Vector3d> control_points;  // size = rows * cols, row-major
    Color color{0.95f, 0.78f, 0.42f};
    float roughness{0.55f};
    float metallic{0.0f};
    int pattern{0};

    [[nodiscard]] const Eigen::Vector3d& at(int r, int c) const {
        return control_points[static_cast<std::size_t>(r * cols + c)];
    }
    [[nodiscard]] Eigen::Vector3d& at(int r, int c) {
        return control_points[static_cast<std::size_t>(r * cols + c)];
    }

    [[nodiscard]] int effective_degree_u() const noexcept;
    [[nodiscard]] int effective_degree_v() const noexcept;
    [[nodiscard]] std::vector<double> knots_u() const;
    [[nodiscard]] std::vector<double> knots_v() const;

    [[nodiscard]] Eigen::Vector3d evaluate(double u, double v) const;

    struct Tessellation {
        std::vector<Eigen::Vector3f> positions;
        std::vector<Eigen::Vector3f> normals;
        std::vector<std::uint32_t> indices;
    };
    [[nodiscard]] Tessellation tessellate(int samples_u = 24, int samples_v = 24) const;
};

}  // namespace cadino::core
