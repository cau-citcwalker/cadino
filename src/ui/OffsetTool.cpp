#include "OffsetTool.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

}  // namespace

void OffsetTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    auto& doc = view.document();

    if (!target_wall_.valid()) {
        const double tol = 80.0 / view.zoom() + 50.0;
        double best = tol;
        cadino::core::EntityId hit_id{};
        for (const auto& [id, w] : doc.walls()) {
            const QPointF a{w.start.x(), w.start.y()};
            const QPointF b{w.end.x(), w.end.y()};
            const double d = dist_point_segment(model_pos, a, b);
            if (d < best) {
                best = d;
                hit_id = id;
            }
        }
        if (hit_id.valid()) {
            target_wall_ = hit_id;
            hover_ = model_pos;
            view.update();
        }
        return;
    }

    const auto* w = doc.find_wall(target_wall_);
    if (!w) {
        target_wall_ = {};
        return;
    }
    const Eigen::Vector2d dir = w->end - w->start;
    const double len = dir.norm();
    if (len < 1e-6) {
        target_wall_ = {};
        return;
    }
    const Eigen::Vector2d unit = dir / len;
    const Eigen::Vector2d normal{-unit.y(), unit.x()};
    const Eigen::Vector2d clicked{model_pos.x(), model_pos.y()};
    const double side = (clicked - w->start).dot(normal);
    const double sign = side >= 0.0 ? 1.0 : -1.0;
    const Eigen::Vector2d delta = normal * (sign * distance_);

    cadino::core::Wall copy = *w;
    copy.id = {};
    copy.group_id = {};
    copy.start += delta;
    copy.end += delta;
    view.command_stack().execute(
        std::make_unique<cadino::core::AddWallCommand>(std::move(copy)));

    target_wall_ = {};
    view.notify_document_modified();
}

void OffsetTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (target_wall_.valid()) view.update();
}

void OffsetTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void OffsetTool::on_cancel(PlanView& view) {
    target_wall_ = {};
    view.update();
}

void OffsetTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!target_wall_.valid()) return;
    const auto* w = view.document().find_wall(target_wall_);
    if (!w) return;

    const Eigen::Vector2d dir = w->end - w->start;
    const double len = dir.norm();
    if (len < 1e-6) return;
    const Eigen::Vector2d unit = dir / len;
    const Eigen::Vector2d normal{-unit.y(), unit.x()};
    const Eigen::Vector2d clicked{hover_.x(), hover_.y()};
    const double side = (clicked - w->start).dot(normal);
    const double sign = side >= 0.0 ? 1.0 : -1.0;
    const Eigen::Vector2d delta = normal * (sign * distance_);
    const Eigen::Vector2d a = w->start + delta;
    const Eigen::Vector2d b = w->end + delta;

    QPen base(QColor(60, 180, 200), 2);
    base.setCosmetic(true);
    p.setPen(base);
    p.drawLine(view.model_to_screen({w->start.x(), w->start.y()}),
               view.model_to_screen({w->end.x(),   w->end.y()}));

    QPen ghost(QColor(60, 180, 200, 200), 2, Qt::DashLine);
    ghost.setCosmetic(true);
    p.setPen(ghost);
    p.drawLine(view.model_to_screen({a.x(), a.y()}),
               view.model_to_screen({b.x(), b.y()}));

    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    p.drawText(view.model_to_screen({(a.x() + b.x()) * 0.5, (a.y() + b.y()) * 0.5}),
               QString("Offset %1 mm — click side to commit").arg(distance_, 0, 'f', 0));
}

}  // namespace cadino::ui
