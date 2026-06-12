#include "entity/NurbsSurface.hpp"

#include <algorithm>
#include <cstdint>

#include <Eigen/Geometry>

namespace cadino::core {

namespace {

std::vector<double> clamped_uniform_knots(int n, int p) {
    if (n <= 1) return {};
    const int m = n + p + 1;
    std::vector<double> U(static_cast<std::size_t>(m), 0.0);
    const int interior = m - 2 * (p + 1);
    for (int i = 0; i < interior; ++i) {
        U[p + 1 + i] = static_cast<double>(i + 1) / static_cast<double>(interior + 1);
    }
    for (int i = m - (p + 1); i < m; ++i) U[i] = 1.0;
    return U;
}

Eigen::Vector3d de_boor_row(double t, int p, int n, const std::vector<double>& U,
                            const std::vector<Eigen::Vector3d>& row) {
    int k = p;
    if (t >= U[n]) {
        k = n - 1;
        t = U[n];
    } else {
        while (k < n && U[k + 1] <= t) ++k;
        if (k >= n) k = n - 1;
    }
    std::vector<Eigen::Vector3d> d(static_cast<std::size_t>(p + 1));
    for (int j = 0; j <= p; ++j) {
        const int idx = std::clamp(k - p + j, 0, n - 1);
        d[static_cast<std::size_t>(j)] = row[static_cast<std::size_t>(idx)];
    }
    for (int r = 1; r <= p; ++r) {
        for (int j = p; j >= r; --j) {
            const double denom = U[j + 1 + k - r] - U[j + k - p];
            const double alpha = denom > 1e-12 ? (t - U[j + k - p]) / denom : 0.0;
            d[static_cast<std::size_t>(j)] =
                (1.0 - alpha) * d[static_cast<std::size_t>(j - 1)] +
                alpha * d[static_cast<std::size_t>(j)];
        }
    }
    return d[static_cast<std::size_t>(p)];
}

}  // namespace

int NurbsSurface::effective_degree_u() const noexcept {
    if (rows <= 1) return 0;
    return std::clamp(degree_u, 1, rows - 1);
}

int NurbsSurface::effective_degree_v() const noexcept {
    if (cols <= 1) return 0;
    return std::clamp(degree_v, 1, cols - 1);
}

std::vector<double> NurbsSurface::knots_u() const {
    return clamped_uniform_knots(rows, effective_degree_u());
}

std::vector<double> NurbsSurface::knots_v() const {
    return clamped_uniform_knots(cols, effective_degree_v());
}

Eigen::Vector3d NurbsSurface::evaluate(double u, double v) const {
    if (rows <= 0 || cols <= 0 || control_points.empty()) {
        return Eigen::Vector3d::Zero();
    }
    if (rows == 1 && cols == 1) return control_points.front();

    const int pu = effective_degree_u();
    const int pv = effective_degree_v();
    const auto U = knots_u();
    const auto V = knots_v();
    if (U.empty() || V.empty()) return control_points.front();

    // First evaluate along v for every row, then along u over the resulting
    // intermediate control polygon. This is the standard tensor-product trick
    // for B-spline surface evaluation.
    std::vector<Eigen::Vector3d> intermediate(static_cast<std::size_t>(rows));
    std::vector<Eigen::Vector3d> row_buf(static_cast<std::size_t>(cols));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) row_buf[static_cast<std::size_t>(c)] = at(r, c);
        intermediate[static_cast<std::size_t>(r)] =
            de_boor_row(std::clamp(v, 0.0, 1.0), pv, cols, V, row_buf);
    }
    return de_boor_row(std::clamp(u, 0.0, 1.0), pu, rows, U, intermediate);
}

NurbsSurface::Tessellation NurbsSurface::tessellate(int samples_u, int samples_v) const {
    Tessellation t;
    if (rows < 2 || cols < 2) return t;
    samples_u = std::max(samples_u, 2);
    samples_v = std::max(samples_v, 2);

    t.positions.reserve(static_cast<std::size_t>(samples_u * samples_v));
    t.normals.reserve(static_cast<std::size_t>(samples_u * samples_v));
    for (int i = 0; i < samples_u; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(samples_u - 1);
        for (int j = 0; j < samples_v; ++j) {
            const double v = static_cast<double>(j) / static_cast<double>(samples_v - 1);
            const Eigen::Vector3d p = evaluate(u, v);
            t.positions.emplace_back(static_cast<float>(p.x()),
                                     static_cast<float>(p.y()),
                                     static_cast<float>(p.z()));
            t.normals.emplace_back(0.0f, 0.0f, 0.0f);  // filled below
        }
    }

    auto idx = [&](int i, int j) {
        return static_cast<std::uint32_t>(i * samples_v + j);
    };
    t.indices.reserve(static_cast<std::size_t>((samples_u - 1) * (samples_v - 1) * 6));
    for (int i = 0; i + 1 < samples_u; ++i) {
        for (int j = 0; j + 1 < samples_v; ++j) {
            const auto a = idx(i, j);
            const auto b = idx(i + 1, j);
            const auto c = idx(i + 1, j + 1);
            const auto d = idx(i, j + 1);
            t.indices.push_back(a);
            t.indices.push_back(b);
            t.indices.push_back(c);
            t.indices.push_back(a);
            t.indices.push_back(c);
            t.indices.push_back(d);
        }
    }

    for (std::size_t k = 0; k + 2 < t.indices.size(); k += 3) {
        const auto& v0 = t.positions[t.indices[k]];
        const auto& v1 = t.positions[t.indices[k + 1]];
        const auto& v2 = t.positions[t.indices[k + 2]];
        const Eigen::Vector3f e1 = v1 - v0;
        const Eigen::Vector3f e2 = v2 - v0;
        Eigen::Vector3f n = e1.cross(e2);
        const float l = n.norm();
        if (l > 1e-9f) n /= l;
        t.normals[t.indices[k]]     += n;
        t.normals[t.indices[k + 1]] += n;
        t.normals[t.indices[k + 2]] += n;
    }
    for (auto& n : t.normals) {
        const float l = n.norm();
        if (l > 1e-9f) n /= l;
    }
    return t;
}

}  // namespace cadino::core
