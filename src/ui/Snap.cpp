#include "Snap.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

double dist(QPointF a, QPointF b) {
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

struct Segment {
    QPointF a, b;
};

// Closest point on infinite line ab to p, returned as the parameter t along ab
// (0 = a, 1 = b). The point itself is `a + (b-a) * t`.
double project_onto_line(QPointF p, QPointF a, QPointF b) {
    const QPointF d = b - a;
    const double ll = d.x() * d.x() + d.y() * d.y();
    if (ll < 1e-12) return 0.0;
    return ((p.x() - a.x()) * d.x() + (p.y() - a.y()) * d.y()) / ll;
}

// Intersection of two infinite lines a0->a1 and b0->b1. Returns nullopt when
// the segments are (numerically) parallel or when the intersection falls
// outside both segments.
std::optional<QPointF> segment_intersection(QPointF a0, QPointF a1,
                                            QPointF b0, QPointF b1) {
    const double ax = a1.x() - a0.x();
    const double ay = a1.y() - a0.y();
    const double bx = b1.x() - b0.x();
    const double by = b1.y() - b0.y();
    const double denom = ax * by - ay * bx;
    if (std::abs(denom) < 1e-9) return std::nullopt;
    const double dx = b0.x() - a0.x();
    const double dy = b0.y() - a0.y();
    const double t = (dx * by - dy * bx) / denom;
    const double s = (dx * ay - dy * ax) / denom;
    constexpr double tol = 1e-3;
    if (t < -tol || t > 1.0 + tol || s < -tol || s > 1.0 + tol) {
        return std::nullopt;
    }
    return QPointF(a0.x() + t * ax, a0.y() + t * ay);
}

// Collect the line segments we'll use for intersection / perpendicular snaps:
// walls, dimension lines (offset edge), box footprint edges, slab outline
// edges.
std::vector<Segment> collect_segments(const cadino::core::Document& doc) {
    std::vector<Segment> out;
    out.reserve(doc.walls().size() + doc.boxes().size() * 4 + doc.slabs().size() * 4 +
                doc.dimensions().size());
    for (const auto& [id, w] : doc.walls()) {
        out.push_back({QPointF(w.start.x(), w.start.y()),
                       QPointF(w.end.x(), w.end.y())});
    }
    for (const auto& [id, b] : doc.boxes()) {
        const double hx = b.size_xy.x() * 0.5;
        const double hy = b.size_xy.y() * 0.5;
        const double c = std::cos(b.rotation_z);
        const double s = std::sin(b.rotation_z);
        const auto rot = [&](double x, double y) {
            return QPointF(b.position.x() + c * x - s * y,
                           b.position.y() + s * x + c * y);
        };
        const QPointF a = rot(-hx, -hy);
        const QPointF c2 = rot(hx, -hy);
        const QPointF d = rot(hx, hy);
        const QPointF e = rot(-hx, hy);
        out.push_back({a, c2});
        out.push_back({c2, d});
        out.push_back({d, e});
        out.push_back({e, a});
    }
    for (const auto& [id, s] : doc.slabs()) {
        const auto& outline = s.outline;
        for (std::size_t i = 0; i < outline.size(); ++i) {
            const auto& p0 = outline[i];
            const auto& p1 = outline[(i + 1) % outline.size()];
            out.push_back({QPointF(p0.x(), p0.y()), QPointF(p1.x(), p1.y())});
        }
    }
    for (const auto& [id, d] : doc.dimensions()) {
        const double dx = d.end.x() - d.start.x();
        const double dy = d.end.y() - d.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-9) continue;
        const double nx = -dy / len;
        const double ny = dx / len;
        out.push_back({QPointF(d.start.x() + nx * d.offset, d.start.y() + ny * d.offset),
                       QPointF(d.end.x() + nx * d.offset, d.end.y() + ny * d.offset)});
    }
    return out;
}

}  // namespace

SnapResult SnapEngine::snap(QPointF model_pos,
                            const cadino::core::Document& doc,
                            double tol) const {
    SnapResult best{};
    double best_dist = std::numeric_limits<double>::infinity();

    auto consider = [&](QPointF candidate, SnapKind kind) {
        const double d = dist(model_pos, candidate);
        if (d > tol) return;
        if (d < best_dist) {
            best_dist = d;
            best.position = candidate;
            best.kind = kind;
        }
    };

    if (endpoint_enabled_) {
        for (const auto& [id, w] : doc.walls()) {
            consider({w.start.x(), w.start.y()}, SnapKind::Endpoint);
            consider({w.end.x(), w.end.y()}, SnapKind::Endpoint);
        }
        for (const auto& [id, c] : doc.cylinders()) {
            consider({c.position.x(), c.position.y()}, SnapKind::Endpoint);
        }
        for (const auto& [id, curve] : doc.curves()) {
            if (curve.control_points.empty()) continue;
            const auto& f = curve.control_points.front();
            const auto& b = curve.control_points.back();
            consider({f.x(), f.y()}, SnapKind::Endpoint);
            consider({b.x(), b.y()}, SnapKind::Endpoint);
        }
        for (const auto& [id, d] : doc.dimensions()) {
            consider({d.start.x(), d.start.y()}, SnapKind::Endpoint);
            consider({d.end.x(), d.end.y()}, SnapKind::Endpoint);
        }
    }

    if (midpoint_enabled_) {
        for (const auto& [id, w] : doc.walls()) {
            const QPointF mid((w.start.x() + w.end.x()) * 0.5,
                              (w.start.y() + w.end.y()) * 0.5);
            consider(mid, SnapKind::Midpoint);
        }
    }

    if (center_enabled_) {
        for (const auto& [id, c] : doc.cylinders()) {
            consider({c.position.x(), c.position.y()}, SnapKind::Center);
        }
        for (const auto& [id, b] : doc.boxes()) {
            consider({b.position.x(), b.position.y()}, SnapKind::Center);
        }
    }

    if (corner_enabled_) {
        for (const auto& [id, b] : doc.boxes()) {
            const double hx = b.size_xy.x() * 0.5;
            const double hy = b.size_xy.y() * 0.5;
            const double c = std::cos(b.rotation_z);
            const double s = std::sin(b.rotation_z);
            const auto rot = [&](double x, double y) {
                return QPointF(b.position.x() + c * x - s * y,
                               b.position.y() + s * x + c * y);
            };
            consider(rot(-hx, -hy), SnapKind::Corner);
            consider(rot( hx, -hy), SnapKind::Corner);
            consider(rot( hx,  hy), SnapKind::Corner);
            consider(rot(-hx,  hy), SnapKind::Corner);
        }
        for (const auto& [id, s] : doc.slabs()) {
            for (const auto& v : s.outline) {
                consider({v.x(), v.y()}, SnapKind::Corner);
            }
        }
    }

    const auto segments = collect_segments(doc);

    if (perpendicular_enabled_) {
        for (const auto& seg : segments) {
            const double t = project_onto_line(model_pos, seg.a, seg.b);
            if (t < 0.0 || t > 1.0) continue;
            const QPointF foot(seg.a.x() + t * (seg.b.x() - seg.a.x()),
                               seg.a.y() + t * (seg.b.y() - seg.a.y()));
            consider(foot, SnapKind::Perpendicular);
        }
    }

    if (intersection_enabled_) {
        for (std::size_t i = 0; i < segments.size(); ++i) {
            for (std::size_t j = i + 1; j < segments.size(); ++j) {
                const auto p = segment_intersection(segments[i].a, segments[i].b,
                                                    segments[j].a, segments[j].b);
                if (p) consider(*p, SnapKind::Intersection);
            }
        }
    }

    if (best.kind != SnapKind::None) {
        return best;
    }

    if (grid_enabled_ && grid_step_ > 0.0) {
        const QPointF g(std::round(model_pos.x() / grid_step_) * grid_step_,
                        std::round(model_pos.y() / grid_step_) * grid_step_);
        if (dist(model_pos, g) <= tol) {
            return SnapResult{g, SnapKind::Grid};
        }
    }

    return {};
}

}  // namespace cadino::ui
