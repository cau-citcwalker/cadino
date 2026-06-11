#include "entity/NurbsCurve.hpp"

#include <algorithm>

namespace cadino::core {

int NurbsCurve::effective_degree() const noexcept {
    const int n = static_cast<int>(control_points.size());
    if (n <= 1) return 0;
    return std::clamp(degree, 1, n - 1);
}

std::vector<double> NurbsCurve::knot_vector() const {
    const int n = static_cast<int>(control_points.size());
    if (n <= 1) return {};
    const int p = effective_degree();
    const int m = n + p + 1;  // total number of knots
    std::vector<double> U(static_cast<std::size_t>(m), 0.0);

    // Clamped uniform: first p+1 knots = 0, last p+1 knots = 1.
    const int interior = m - 2 * (p + 1);
    for (int i = 0; i < p + 1; ++i) U[i] = 0.0;
    for (int i = 0; i < interior; ++i) {
        U[p + 1 + i] = static_cast<double>(i + 1) / static_cast<double>(interior + 1);
    }
    for (int i = m - (p + 1); i < m; ++i) U[i] = 1.0;
    return U;
}

namespace {

// de Boor's recursive evaluation.
Eigen::Vector3d de_boor(double t, int p, const std::vector<Eigen::Vector3d>& P,
                        const std::vector<double>& U) {
    const int n = static_cast<int>(P.size());
    // Find span k such that U[k] <= t < U[k+1] (special-case the right endpoint).
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
        d[static_cast<std::size_t>(j)] = P[static_cast<std::size_t>(idx)];
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

Eigen::Vector3d NurbsCurve::evaluate(double t) const {
    if (control_points.empty()) return Eigen::Vector3d::Zero();
    if (control_points.size() == 1) return control_points.front();
    const int p = effective_degree();
    const auto U = knot_vector();
    if (U.empty()) return control_points.front();
    return de_boor(std::clamp(t, 0.0, 1.0), p, control_points, U);
}

std::vector<Eigen::Vector3d> NurbsCurve::tessellate(int sample_count) const {
    if (control_points.size() < 2) return {};
    if (sample_count < 2) sample_count = 2;
    const int p = effective_degree();
    const auto U = knot_vector();
    if (U.empty()) return {};

    std::vector<Eigen::Vector3d> out;
    out.reserve(static_cast<std::size_t>(sample_count));
    for (int i = 0; i < sample_count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sample_count - 1);
        out.push_back(de_boor(t, p, control_points, U));
    }
    return out;
}

}  // namespace cadino::core
