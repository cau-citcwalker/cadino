#include "SelectTool.hpp"

#include <cmath>
#include <limits>

#include <QPainter>

#include "PlanView.hpp"
#include "command/BoxCommands.hpp"
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

bool point_in_box_footprint(QPointF p, const cadino::core::Box& b) {
    const double hx = b.size_xy.x() * 0.5;
    const double hy = b.size_xy.y() * 0.5;
    return std::abs(p.x() - b.position.x()) <= hx &&
           std::abs(p.y() - b.position.y()) <= hy;
}

}  // namespace

void SelectTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    const auto& doc = view.document();
    const double pick_radius = 8.0 / view.zoom();

    kind_ = SelectKind::None;
    selected_ = {};

    cadino::core::EntityId best_wall{};
    double best_wall_dist = std::numeric_limits<double>::infinity();
    for (const auto& [id, w] : doc.walls()) {
        const QPointF a{w.start.x(), w.start.y()};
        const QPointF b{w.end.x(), w.end.y()};
        const double tolerance = pick_radius + w.thickness * 0.5;
        const double d = distance_point_to_segment(model_pos, a, b);
        if (d <= tolerance && d < best_wall_dist) {
            best_wall_dist = d;
            best_wall = id;
        }
    }

    cadino::core::EntityId best_box{};
    for (const auto& [id, b] : doc.boxes()) {
        if (point_in_box_footprint(model_pos, b)) {
            best_box = id;
            break;
        }
    }

    if (best_box.valid()) {
        selected_ = best_box;
        kind_ = SelectKind::Box;
        original_box_ = *doc.find_box(best_box);
        drag_start_ = model_pos;
        dragging_ = true;
    } else if (best_wall.valid()) {
        selected_ = best_wall;
        kind_ = SelectKind::Wall;
        original_wall_ = *doc.find_wall(best_wall);
        drag_start_ = model_pos;
        dragging_ = true;
    }
    view.update();
}

void SelectTool::on_move(PlanView& view, QPointF model_pos) {
    if (!dragging_ || !selected_.valid()) return;

    const QPointF delta = model_pos - drag_start_;

    if (kind_ == SelectKind::Wall) {
        auto* w = view.document().find_wall(selected_);
        if (!w) return;
        w->start = original_wall_.start + Eigen::Vector2d{delta.x(), delta.y()};
        w->end = original_wall_.end + Eigen::Vector2d{delta.x(), delta.y()};
    } else if (kind_ == SelectKind::Box) {
        auto* b = view.document().find_box(selected_);
        if (!b) return;
        b->position = original_box_.position + Eigen::Vector2d{delta.x(), delta.y()};
    }
    view.update();
}

void SelectTool::on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton || !dragging_) return;
    dragging_ = false;

    const QPointF delta = model_pos - drag_start_;
    if (std::hypot(delta.x(), delta.y()) < 1e-6) return;

    if (kind_ == SelectKind::Wall) {
        auto* w = view.document().find_wall(selected_);
        if (!w) return;
        cadino::core::Wall after = *w;
        *w = original_wall_;
        view.command_stack().execute(
            std::make_unique<cadino::core::ModifyWallCommand>(selected_, std::move(after)));
    } else if (kind_ == SelectKind::Box) {
        auto* b = view.document().find_box(selected_);
        if (!b) return;
        cadino::core::Box after = *b;
        *b = original_box_;
        view.command_stack().execute(
            std::make_unique<cadino::core::ModifyBoxCommand>(selected_, std::move(after)));
    }
    view.notify_document_modified();
}

void SelectTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!selected_.valid()) return;

    QPen pen(QColor(60, 130, 220), 3);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (kind_ == SelectKind::Wall) {
        const auto* w = view.document().find_wall(selected_);
        if (!w) return;
        p.drawLine(view.model_to_screen({w->start.x(), w->start.y()}),
                   view.model_to_screen({w->end.x(), w->end.y()}));
    } else if (kind_ == SelectKind::Box) {
        const auto* b = view.document().find_box(selected_);
        if (!b) return;
        const QPointF a_m(b->position.x() - b->size_xy.x() * 0.5,
                          b->position.y() - b->size_xy.y() * 0.5);
        const QPointF c_m(b->position.x() + b->size_xy.x() * 0.5,
                          b->position.y() + b->size_xy.y() * 0.5);
        p.drawRect(QRectF(view.model_to_screen(a_m),
                          view.model_to_screen(c_m)).normalized());
    }
}

}  // namespace cadino::ui
