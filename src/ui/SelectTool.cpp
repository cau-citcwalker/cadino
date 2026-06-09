#include "SelectTool.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRectF>

#include "PlanView.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
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
    const double dx = p.x() - b.position.x();
    const double dy = p.y() - b.position.y();
    const double c = std::cos(-b.rotation_z);
    const double s = std::sin(-b.rotation_z);
    const double lx = c * dx - s * dy;
    const double ly = s * dx + c * dy;
    return std::abs(lx) <= hx && std::abs(ly) <= hy;
}

Selection pick_at(const cadino::core::Document& doc, QPointF model_pos, double pick_radius) {
    Selection best{};
    double best_dist = std::numeric_limits<double>::infinity();

    for (const auto& [id, c] : doc.cylinders()) {
        const double d = std::hypot(model_pos.x() - c.position.x(),
                                    model_pos.y() - c.position.y());
        if (d <= c.radius && d < best_dist) {
            best_dist = d;
            best = {id, SelectKind::Cylinder};
        }
    }
    for (const auto& [id, b] : doc.boxes()) {
        if (point_in_box_footprint(model_pos, b)) {
            best_dist = 0;
            best = {id, SelectKind::Box};
            break;
        }
    }
    for (const auto& [id, w] : doc.walls()) {
        const QPointF a{w.start.x(), w.start.y()};
        const QPointF b{w.end.x(), w.end.y()};
        const double tolerance = pick_radius + w.thickness * 0.5;
        const double d = distance_point_to_segment(model_pos, a, b);
        if (d <= tolerance && d < best_dist) {
            best_dist = d;
            best = {id, SelectKind::Wall};
        }
    }
    return best;
}

bool wall_in_rect(const cadino::core::Wall& w, const QRectF& r) {
    return r.contains(w.start.x(), w.start.y()) &&
           r.contains(w.end.x(), w.end.y());
}

bool box_in_rect(const cadino::core::Box& b, const QRectF& r) {
    return r.contains(b.position.x(), b.position.y());
}

bool cylinder_in_rect(const cadino::core::Cylinder& c, const QRectF& r) {
    return r.contains(c.position.x(), c.position.y());
}

}  // namespace

void SelectTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    const auto& doc = view.document();
    const double pick_radius = 8.0 / view.zoom();
    const Selection hit = pick_at(doc, model_pos, pick_radius);
    const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;

    if (!hit.valid()) {
        if (!shift) view.clear_selection();
        rubber_banding_ = true;
        rubber_start_ = model_pos;
        rubber_current_ = model_pos;
        return;
    }

    if (shift) {
        view.toggle_selection(hit);
        return;
    }

    if (!view.is_selected(hit)) {
        view.set_selections({hit});
    }

    drag_originals_.clear();
    for (const auto& sel : view.selections()) {
        if (sel.kind == SelectKind::Wall) {
            if (const auto* w = doc.find_wall(sel.id)) {
                drag_originals_.emplace(sel.id, *w);
            }
        } else if (sel.kind == SelectKind::Box) {
            if (const auto* b = doc.find_box(sel.id)) {
                drag_originals_.emplace(sel.id, *b);
            }
        } else if (sel.kind == SelectKind::Cylinder) {
            if (const auto* c = doc.find_cylinder(sel.id)) {
                drag_originals_.emplace(sel.id, *c);
            }
        }
    }
    drag_start_ = model_pos;
    dragging_ = true;
}

void SelectTool::on_move(PlanView& view, QPointF model_pos) {
    if (rubber_banding_) {
        rubber_current_ = model_pos;
        view.update();
        return;
    }
    if (!dragging_) return;

    const QPointF delta = model_pos - drag_start_;
    auto& doc = view.document();

    for (const auto& sel : view.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;

        if (sel.kind == SelectKind::Wall) {
            auto* w = doc.find_wall(sel.id);
            if (!w) continue;
            const auto& orig = std::get<cadino::core::Wall>(it->second);
            w->start = orig.start + Eigen::Vector2d{delta.x(), delta.y()};
            w->end = orig.end + Eigen::Vector2d{delta.x(), delta.y()};
        } else if (sel.kind == SelectKind::Box) {
            auto* b = doc.find_box(sel.id);
            if (!b) continue;
            const auto& orig = std::get<cadino::core::Box>(it->second);
            b->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
        } else if (sel.kind == SelectKind::Cylinder) {
            auto* c = doc.find_cylinder(sel.id);
            if (!c) continue;
            const auto& orig = std::get<cadino::core::Cylinder>(it->second);
            c->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
        }
    }
    view.update();
}

void SelectTool::on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (rubber_banding_) {
        rubber_banding_ = false;
        const QRectF rect = QRectF(rubber_start_, model_pos).normalized();
        if (rect.width() < 1.0 || rect.height() < 1.0) {
            view.update();
            return;
        }
        std::vector<Selection> hits;
        const auto& doc = view.document();
        for (const auto& [id, w] : doc.walls()) {
            if (wall_in_rect(w, rect)) hits.push_back({id, SelectKind::Wall});
        }
        for (const auto& [id, b] : doc.boxes()) {
            if (box_in_rect(b, rect)) hits.push_back({id, SelectKind::Box});
        }
        for (const auto& [id, c] : doc.cylinders()) {
            if (cylinder_in_rect(c, rect)) hits.push_back({id, SelectKind::Cylinder});
        }
        const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
        if (shift) {
            for (const auto& h : hits) view.add_to_selection(h);
        } else {
            view.set_selections(std::move(hits));
        }
        view.update();
        return;
    }

    if (!dragging_) return;
    dragging_ = false;

    const QPointF delta = model_pos - drag_start_;
    if (std::hypot(delta.x(), delta.y()) < 1e-6) {
        drag_originals_.clear();
        return;
    }

    auto& doc = view.document();
    for (const auto& sel : view.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;

        if (sel.kind == SelectKind::Wall) {
            auto* w = doc.find_wall(sel.id);
            if (!w) continue;
            const auto& orig = std::get<cadino::core::Wall>(it->second);
            cadino::core::Wall after = *w;
            *w = orig;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyWallCommand>(sel.id, std::move(after)));
        } else if (sel.kind == SelectKind::Box) {
            auto* b = doc.find_box(sel.id);
            if (!b) continue;
            const auto& orig = std::get<cadino::core::Box>(it->second);
            cadino::core::Box after = *b;
            *b = orig;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyBoxCommand>(sel.id, std::move(after)));
        } else if (sel.kind == SelectKind::Cylinder) {
            auto* c = doc.find_cylinder(sel.id);
            if (!c) continue;
            const auto& orig = std::get<cadino::core::Cylinder>(it->second);
            cadino::core::Cylinder after = *c;
            *c = orig;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyCylinderCommand>(sel.id, std::move(after)));
        }
    }
    drag_originals_.clear();
    view.notify_document_modified();
}

void SelectTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (rubber_banding_) {
        QPen pen(QColor(60, 130, 220, 200), 1, Qt::DashLine);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(QColor(60, 130, 220, 40));
        const QPointF a = view.model_to_screen(rubber_start_);
        const QPointF b = view.model_to_screen(rubber_current_);
        p.drawRect(QRectF(a, b).normalized());
    }

    QPen pen(QColor(60, 130, 220), 3);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    for (const auto& sel : view.selections()) {
        if (sel.kind == SelectKind::Wall) {
            const auto* w = view.document().find_wall(sel.id);
            if (!w) continue;
            p.drawLine(view.model_to_screen({w->start.x(), w->start.y()}),
                       view.model_to_screen({w->end.x(), w->end.y()}));
        } else if (sel.kind == SelectKind::Box) {
            const auto* b = view.document().find_box(sel.id);
            if (!b) continue;
            const double hx = b->size_xy.x() * 0.5;
            const double hy = b->size_xy.y() * 0.5;
            const double c = std::cos(b->rotation_z);
            const double s = std::sin(b->rotation_z);
            const auto rot = [&](double x, double y) {
                return QPointF(b->position.x() + c * x - s * y,
                               b->position.y() + s * x + c * y);
            };
            QPolygonF poly;
            poly << view.model_to_screen(rot(-hx, -hy))
                 << view.model_to_screen(rot( hx, -hy))
                 << view.model_to_screen(rot( hx,  hy))
                 << view.model_to_screen(rot(-hx,  hy));
            p.drawPolygon(poly);
        } else if (sel.kind == SelectKind::Cylinder) {
            const auto* c = view.document().find_cylinder(sel.id);
            if (!c) continue;
            const QPointF center_s = view.model_to_screen({c->position.x(), c->position.y()});
            const double r_s = c->radius * view.zoom();
            p.drawEllipse(center_s, r_s, r_s);
        }
    }
}

}  // namespace cadino::ui
