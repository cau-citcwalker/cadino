#include "SelectTool.hpp"

#include <cmath>
#include <limits>

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

double distance_point_to_segment(QPointF p, QPointF a, QPointF b) {
    const QPointF ab = b - a;
    const double len_sq = ab.x() * ab.x() + ab.y() * ab.y();
    if (len_sq < 1e-12) {
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len_sq;
    t = std::clamp(t, 0.0, 1.0);
    const QPointF proj = a + ab * t;
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

cadino::core::EntityId pick_wall(const cadino::core::Document& doc, QPointF model_pos,
                                  double pick_radius) {
    cadino::core::EntityId best{};
    double best_dist = std::numeric_limits<double>::infinity();

    for (const auto& [id, w] : doc.walls()) {
        const QPointF a{w.start.x(), w.start.y()};
        const QPointF b{w.end.x(), w.end.y()};
        const double tolerance = pick_radius + w.thickness * 0.5;
        const double d = distance_point_to_segment(model_pos, a, b);
        if (d <= tolerance && d < best_dist) {
            best_dist = d;
            best = id;
        }
    }
    return best;
}

}  // namespace

void SelectTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    const double pick_radius = 8.0 / view.zoom();
    const auto id = pick_wall(view.document(), model_pos, pick_radius);
    selected_ = id;

    if (id.valid()) {
        if (const auto* w = view.document().find_wall(id)) {
            original_ = *w;
            drag_start_ = model_pos;
            dragging_ = true;
        }
    }
    view.update();
}

void SelectTool::on_move(PlanView& view, QPointF model_pos) {
    if (!dragging_ || !selected_.valid()) return;

    auto* w = view.document().find_wall(selected_);
    if (!w) return;

    const QPointF delta = model_pos - drag_start_;
    w->start = original_.start + Eigen::Vector2d{delta.x(), delta.y()};
    w->end = original_.end + Eigen::Vector2d{delta.x(), delta.y()};
    view.update();
}

void SelectTool::on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton || !dragging_) return;
    dragging_ = false;

    auto* w = view.document().find_wall(selected_);
    if (!w) return;

    const QPointF delta = model_pos - drag_start_;
    if (std::hypot(delta.x(), delta.y()) < 1e-6) return;

    cadino::core::Wall after = *w;
    *w = original_;

    view.command_stack().execute(
        std::make_unique<cadino::core::ModifyWallCommand>(selected_, std::move(after)));
    view.notify_document_modified();
}

void SelectTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!selected_.valid()) return;
    const auto* w = view.document().find_wall(selected_);
    if (!w) return;

    QPen pen(QColor(60, 130, 220), 3);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(view.model_to_screen({w->start.x(), w->start.y()}),
               view.model_to_screen({w->end.x(), w->end.y()}));
}

}  // namespace cadino::ui
