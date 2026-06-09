#include "DoorTool.hpp"

#include <cmath>
#include <limits>

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/DoorWindowSlabCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

struct WallHit {
    cadino::core::EntityId id;
    double position_along{};
    QPointF projected{};
    double distance{};
};

WallHit nearest_wall(const cadino::core::Document& doc, QPointF p, double tolerance) {
    WallHit best{};
    best.distance = std::numeric_limits<double>::infinity();

    for (const auto& [id, w] : doc.walls()) {
        const QPointF a{w.start.x(), w.start.y()};
        const QPointF b{w.end.x(), w.end.y()};
        const QPointF ab = b - a;
        const double len_sq = ab.x() * ab.x() + ab.y() * ab.y();
        if (len_sq < 1e-9) continue;
        double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len_sq;
        t = std::clamp(t, 0.0, 1.0);
        const QPointF proj = a + ab * t;
        const double d = std::hypot(p.x() - proj.x(), p.y() - proj.y());
        if (d <= tolerance + w.thickness * 0.5 && d < best.distance) {
            best.id = id;
            best.position_along = t * std::sqrt(len_sq);
            best.projected = proj;
            best.distance = d;
        }
    }
    return best;
}

}  // namespace

void DoorTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    const auto hit = nearest_wall(view.document(), model_pos, 200.0 / view.zoom());
    if (!hit.id.valid()) return;

    if (window_mode_) {
        cadino::core::Window win;
        win.host_wall = hit.id;
        win.position_along = hit.position_along;
        view.command_stack().execute(
            std::make_unique<cadino::core::AddWindowCommand>(std::move(win)));
    } else {
        cadino::core::Door door;
        door.host_wall = hit.id;
        door.position_along = hit.position_along;
        view.command_stack().execute(
            std::make_unique<cadino::core::AddDoorCommand>(std::move(door)));
    }
    view.notify_document_modified();
}

void DoorTool::on_move(PlanView& view, QPointF model_pos) {
    const auto hit = nearest_wall(view.document(), model_pos, 200.0 / view.zoom());
    hover_ = hit.projected;
    hover_valid_ = hit.id.valid();
    view.update();
}

void DoorTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!hover_valid_) return;
    const QColor color = window_mode_ ? QColor(120, 180, 220) : QColor(220, 160, 100);
    QPen pen(color, 2);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(color);
    const auto s = view.model_to_screen(hover_);
    p.drawEllipse(s, 5, 5);
}

}  // namespace cadino::ui
