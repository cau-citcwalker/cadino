#include "Snap.hpp"

#include <cmath>
#include <limits>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

double dist(QPointF a, QPointF b) {
    return std::hypot(a.x() - b.x(), a.y() - b.y());
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
    }

    if (midpoint_enabled_) {
        for (const auto& [id, w] : doc.walls()) {
            const QPointF mid((w.start.x() + w.end.x()) * 0.5,
                              (w.start.y() + w.end.y()) * 0.5);
            consider(mid, SnapKind::Midpoint);
        }
    }

    if (corner_enabled_) {
        for (const auto& [id, b] : doc.boxes()) {
            const double hx = b.size_xy.x() * 0.5;
            const double hy = b.size_xy.y() * 0.5;
            consider({b.position.x() - hx, b.position.y() - hy}, SnapKind::Corner);
            consider({b.position.x() + hx, b.position.y() - hy}, SnapKind::Corner);
            consider({b.position.x() + hx, b.position.y() + hy}, SnapKind::Corner);
            consider({b.position.x() - hx, b.position.y() + hy}, SnapKind::Corner);
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
