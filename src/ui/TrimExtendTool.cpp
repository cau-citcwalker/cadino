#include "TrimExtendTool.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

double dist_point_segment(QPointF p, QPointF a, QPointF b) {
    const double ax = b.x() - a.x();
    const double ay = b.y() - a.y();
    const double ll = ax * ax + ay * ay;
    if (ll < 1e-12) return std::hypot(p.x() - a.x(), p.y() - a.y());
    double t = ((p.x() - a.x()) * ax + (p.y() - a.y()) * ay) / ll;
    t = std::clamp(t, 0.0, 1.0);
    const double qx = a.x() + t * ax;
    const double qy = a.y() + t * ay;
    return std::hypot(p.x() - qx, p.y() - qy);
}

cadino::core::EntityId pick_wall(const cadino::core::Document& doc,
                                 QPointF model_pos, double tol) {
    cadino::core::EntityId best{};
    double best_d = tol;
    for (const auto& [id, w] : doc.walls()) {
        const QPointF a{w.start.x(), w.start.y()};
        const QPointF b{w.end.x(), w.end.y()};
        const double d = dist_point_segment(model_pos, a, b);
        if (d < best_d) {
            best_d = d;
            best = id;
        }
    }
    return best;
}

// Intersection of the infinite lines (a0,a1) and (b0,b1).
std::optional<Eigen::Vector2d> line_intersection(Eigen::Vector2d a0,
                                                 Eigen::Vector2d a1,
                                                 Eigen::Vector2d b0,
                                                 Eigen::Vector2d b1) {
    const double ax = a1.x() - a0.x();
    const double ay = a1.y() - a0.y();
    const double bx = b1.x() - b0.x();
    const double by = b1.y() - b0.y();
    const double det = ax * by - ay * bx;
    if (std::abs(det) < 1e-9) return std::nullopt;
    const double dx = b0.x() - a0.x();
    const double dy = b0.y() - a0.y();
    const double t = (dx * by - dy * bx) / det;
    return Eigen::Vector2d{a0.x() + t * ax, a0.y() + t * ay};
}

}  // namespace

void TrimExtendTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    auto& doc = view.document();
    const double tol = 80.0 / view.zoom() + 50.0;

    if (!cutter_.valid()) {
        cutter_ = pick_wall(doc, model_pos, tol);
        hover_ = model_pos;
        view.update();
        return;
    }

    const auto target_id = pick_wall(doc, model_pos, tol);
    if (!target_id.valid() || target_id == cutter_) {
        return;
    }

    const auto* cutter = doc.find_wall(cutter_);
    const auto* target = doc.find_wall(target_id);
    if (!cutter || !target) {
        cutter_ = {};
        return;
    }

    const auto inter = line_intersection(cutter->start, cutter->end,
                                         target->start, target->end);
    if (!inter) {
        // Parallel walls — nothing to do.
        cutter_ = {};
        return;
    }

    const Eigen::Vector2d clicked{model_pos.x(), model_pos.y()};
    const double d_start = (target->start - clicked).norm();
    const double d_end = (target->end - clicked).norm();

    cadino::core::Wall after = *target;
    if (d_start < d_end) {
        after.start = *inter;
    } else {
        after.end = *inter;
    }
    view.command_stack().execute(
        std::make_unique<cadino::core::ModifyWallCommand>(target_id, std::move(after)));

    cutter_ = {};
    view.notify_document_modified();
}

void TrimExtendTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (cutter_.valid()) view.update();
}

void TrimExtendTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void TrimExtendTool::on_cancel(PlanView& view) {
    cutter_ = {};
    view.update();
}

void TrimExtendTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!cutter_.valid()) return;
    const auto* w = view.document().find_wall(cutter_);
    if (!w) return;

    QPen pen(QColor(220, 80, 80), 3);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.drawLine(view.model_to_screen({w->start.x(), w->start.y()}),
               view.model_to_screen({w->end.x(),   w->end.y()}));

    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    p.drawText(view.model_to_screen(hover_) + QPointF(8, -8),
               QStringLiteral("Trim/Extend — click target wall near the endpoint to snap"));
}

}  // namespace cadino::ui
