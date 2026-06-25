#include "SelectTool.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QRectF>

#include "PlanView.hpp"
#include "command/BlockCommands.hpp"
#include "command/BlockInstanceCommands.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
#include "command/NurbsCurveCommands.hpp"
#include "command/NurbsSurfaceCommands.hpp"
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

cadino::core::EntityId group_of(const cadino::core::Document& doc, Selection sel) {
    if (sel.kind == SelectKind::Wall) {
        if (const auto* w = doc.find_wall(sel.id)) return w->group_id;
    } else if (sel.kind == SelectKind::Box) {
        if (const auto* b = doc.find_box(sel.id)) return b->group_id;
    } else if (sel.kind == SelectKind::Cylinder) {
        if (const auto* c = doc.find_cylinder(sel.id)) return c->group_id;
    }
    return {};
}

std::vector<Selection> expand_to_group(const cadino::core::Document& doc, Selection sel) {
    const auto gid = group_of(doc, sel);
    if (!gid.valid()) return {sel};

    std::vector<Selection> result;
    for (const auto& [id, w] : doc.walls()) {
        if (w.group_id == gid) result.push_back({id, SelectKind::Wall});
    }
    for (const auto& [id, b] : doc.boxes()) {
        if (b.group_id == gid) result.push_back({id, SelectKind::Box});
    }
    for (const auto& [id, c] : doc.cylinders()) {
        if (c.group_id == gid) result.push_back({id, SelectKind::Cylinder});
    }
    return result.empty() ? std::vector<Selection>{sel} : result;
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
    for (const auto& [id, curve] : doc.curves()) {
        const auto samples = curve.tessellate(96);
        if (samples.size() < 2) continue;
        for (std::size_t i = 1; i < samples.size(); ++i) {
            const QPointF a{samples[i - 1].x(), samples[i - 1].y()};
            const QPointF b{samples[i].x(), samples[i].y()};
            const double d = distance_point_to_segment(model_pos, a, b);
            if (d <= pick_radius && d < best_dist) {
                best_dist = d;
                best = {id, SelectKind::NurbsCurve};
            }
        }
    }
    for (const auto& [id, surf] : doc.surfaces()) {
        if (surf.rows < 2 || surf.cols < 2) continue;
        // Quick axis-aligned bbox of the control polygon footprint.
        double minx = surf.control_points[0].x();
        double miny = surf.control_points[0].y();
        double maxx = minx, maxy = miny;
        for (const auto& cp : surf.control_points) {
            minx = std::min(minx, cp.x()); miny = std::min(miny, cp.y());
            maxx = std::max(maxx, cp.x()); maxy = std::max(maxy, cp.y());
        }
        if (model_pos.x() >= minx && model_pos.x() <= maxx &&
            model_pos.y() >= miny && model_pos.y() <= maxy) {
            if (best_dist > 0) {
                best_dist = 0;
                best = {id, SelectKind::NurbsSurface};
            }
        }
    }
    for (const auto& [id, block] : doc.blocks()) {
        bool hit = false;
        for (const auto& local_b : block.boxes) {
            if (point_in_box_footprint(model_pos, block.world_box(local_b))) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            for (const auto& local_c : block.cylinders) {
                const auto c = block.world_cylinder(local_c);
                if (std::hypot(model_pos.x() - c.position.x(),
                               model_pos.y() - c.position.y()) <= c.radius) {
                    hit = true;
                    break;
                }
            }
        }
        if (hit) {
            best_dist = 0;
            best = {id, SelectKind::Block};
            break;
        }
    }
    for (const auto& [id, inst] : doc.block_instances()) {
        const auto* def = doc.find_block_def(inst.definition_id);
        if (!def) continue;
        bool hit = false;
        for (const auto& local_b : def->boxes) {
            if (point_in_box_footprint(model_pos, inst.world_box(local_b))) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            for (const auto& local_c : def->cylinders) {
                const auto c = inst.world_cylinder(local_c);
                if (std::hypot(model_pos.x() - c.position.x(),
                               model_pos.y() - c.position.y()) <= c.radius) {
                    hit = true;
                    break;
                }
            }
        }
        if (hit) {
            best_dist = 0;
            best = {id, SelectKind::BlockInstance};
            break;
        }
    }
    for (const auto& [id, dim] : doc.dimensions()) {
        const double dx = dim.end.x() - dim.start.x();
        const double dy = dim.end.y() - dim.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double nx = -dy / len;
        const double ny = dx / len;
        const QPointF ds(dim.start.x() + nx * dim.offset,
                         dim.start.y() + ny * dim.offset);
        const QPointF de(dim.end.x() + nx * dim.offset,
                         dim.end.y() + ny * dim.offset);
        const double d = distance_point_to_segment(model_pos, ds, de);
        if (d <= pick_radius && d < best_dist) {
            best_dist = d;
            best = {id, SelectKind::Dimension};
        }
    }
    for (const auto& [id, t] : doc.texts()) {
        const double d = std::hypot(model_pos.x() - t.position.x(),
                                    model_pos.y() - t.position.y());
        if (d <= pick_radius && d < best_dist) {
            best_dist = d;
            best = {id, SelectKind::Text};
        }
    }
    for (const auto& [id, l] : doc.leaders()) {
        const QPointF a{l.anchor.x(), l.anchor.y()};
        const QPointF b{l.text_position.x(), l.text_position.y()};
        const double d = distance_point_to_segment(model_pos, a, b);
        if (d <= pick_radius && d < best_dist) {
            best_dist = d;
            best = {id, SelectKind::Leader};
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

    // Highest priority: if exactly one NURBS curve is selected, attempt a hit
    // on its control points. A successful hit enters per-point drag mode and
    // we skip the regular pick path.
    if (view.selections().size() == 1 &&
        view.selections().front().kind == SelectKind::NurbsCurve) {
        const auto sel = view.selections().front();
        if (const auto* curve = doc.find_curve(sel.id)) {
            int hit_idx = -1;
            double best = pick_radius * 1.2;
            for (std::size_t i = 0; i < curve->control_points.size(); ++i) {
                const double d = std::hypot(
                    model_pos.x() - curve->control_points[i].x(),
                    model_pos.y() - curve->control_points[i].y());
                if (d <= best) {
                    best = d;
                    hit_idx = static_cast<int>(i);
                }
            }
            if (hit_idx >= 0) {
                curve_point_id_ = sel.id;
                curve_point_index_ = hit_idx;
                curve_point_snapshot_ = *curve;
                return;
            }
        }
    }

    if (view.selections().size() == 1 &&
        view.selections().front().kind == SelectKind::NurbsSurface) {
        const auto sel = view.selections().front();
        if (const auto* surf = doc.find_surface(sel.id)) {
            int hit_idx = -1;
            double best = pick_radius * 1.2;
            for (std::size_t i = 0; i < surf->control_points.size(); ++i) {
                const double d = std::hypot(
                    model_pos.x() - surf->control_points[i].x(),
                    model_pos.y() - surf->control_points[i].y());
                if (d <= best) {
                    best = d;
                    hit_idx = static_cast<int>(i);
                }
            }
            if (hit_idx >= 0) {
                surface_point_id_ = sel.id;
                surface_point_index_ = hit_idx;
                surface_point_snapshot_ = *surf;
                return;
            }
        }
    }

    Selection hit = pick_at(doc, model_pos, pick_radius);
    if (hit.valid()) {
        cadino::core::EntityId layer_id{};
        switch (hit.kind) {
            case SelectKind::Wall:
                if (auto* w = doc.find_wall(hit.id)) layer_id = w->layer_id;
                break;
            case SelectKind::Box:
                if (auto* b = doc.find_box(hit.id)) layer_id = b->layer_id;
                break;
            case SelectKind::Cylinder:
                if (auto* c = doc.find_cylinder(hit.id)) layer_id = c->layer_id;
                break;
            case SelectKind::NurbsCurve:
                if (auto* c = doc.find_curve(hit.id)) layer_id = c->layer_id;
                break;
            case SelectKind::NurbsSurface:
                if (auto* s = doc.find_surface(hit.id)) layer_id = s->layer_id;
                break;
            case SelectKind::Block:
                if (auto* b = doc.find_block(hit.id)) layer_id = b->layer_id;
                break;
            case SelectKind::BlockInstance:
                if (auto* i = doc.find_block_instance(hit.id)) layer_id = i->layer_id;
                break;
            case SelectKind::Dimension:
                if (auto* d = doc.find_dimension(hit.id)) layer_id = d->layer_id;
                break;
            case SelectKind::Text:
                if (auto* t = doc.find_text(hit.id)) layer_id = t->layer_id;
                break;
            case SelectKind::Leader:
                if (auto* l = doc.find_leader(hit.id)) layer_id = l->layer_id;
                break;
            default: break;
        }
        if (view.layer_locked(layer_id)) hit = {};
    }
    const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;

    if (!hit.valid()) {
        if (!shift) view.clear_selection();
        rubber_banding_ = true;
        rubber_start_ = model_pos;
        rubber_current_ = model_pos;
        return;
    }

    const auto group_members = expand_to_group(doc, hit);

    if (shift) {
        if (view.is_selected(hit)) {
            for (const auto& m : group_members) view.remove_from_selection(m);
        } else {
            for (const auto& m : group_members) view.add_to_selection(m);
        }
        return;
    }

    if (!view.is_selected(hit)) {
        view.set_selections(group_members);
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
        } else if (sel.kind == SelectKind::Block) {
            if (const auto* b = doc.find_block(sel.id)) {
                drag_originals_.emplace(sel.id, *b);
            }
        } else if (sel.kind == SelectKind::BlockInstance) {
            if (const auto* i = doc.find_block_instance(sel.id)) {
                drag_originals_.emplace(sel.id, *i);
            }
        }
    }
    drag_start_ = model_pos;
    dragging_ = true;
}

void SelectTool::on_move(PlanView& view, QPointF model_pos) {
    if (curve_point_index_ >= 0) {
        auto& doc = view.document();
        if (auto* curve = doc.find_curve(curve_point_id_)) {
            if (curve_point_index_ < static_cast<int>(curve->control_points.size())) {
                curve->control_points[static_cast<std::size_t>(curve_point_index_)] = {
                    model_pos.x(), model_pos.y(), 0.0};
                view.update();
            }
        }
        return;
    }
    if (surface_point_index_ >= 0) {
        auto& doc = view.document();
        if (auto* surf = doc.find_surface(surface_point_id_)) {
            if (surface_point_index_ < static_cast<int>(surf->control_points.size())) {
                const double old_z =
                    surf->control_points[static_cast<std::size_t>(surface_point_index_)].z();
                surf->control_points[static_cast<std::size_t>(surface_point_index_)] = {
                    model_pos.x(), model_pos.y(), old_z};
                view.update();
            }
        }
        return;
    }
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
        } else if (sel.kind == SelectKind::Block) {
            auto* bl = doc.find_block(sel.id);
            if (!bl) continue;
            const auto& orig = std::get<cadino::core::Block>(it->second);
            bl->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
        } else if (sel.kind == SelectKind::BlockInstance) {
            auto* inst = doc.find_block_instance(sel.id);
            if (!inst) continue;
            const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
            inst->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
        }
    }
    view.update();
}

void SelectTool::on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (curve_point_index_ >= 0) {
        auto& doc = view.document();
        if (auto* curve = doc.find_curve(curve_point_id_)) {
            cadino::core::NurbsCurve after = *curve;
            *curve = curve_point_snapshot_;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyNurbsCurveCommand>(
                    curve_point_id_, std::move(after)));
        }
        curve_point_id_ = {};
        curve_point_index_ = -1;
        curve_point_snapshot_ = {};
        view.notify_document_modified();
        return;
    }
    if (surface_point_index_ >= 0) {
        auto& doc = view.document();
        if (auto* surf = doc.find_surface(surface_point_id_)) {
            cadino::core::NurbsSurface after = *surf;
            *surf = surface_point_snapshot_;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyNurbsSurfaceCommand>(
                    surface_point_id_, std::move(after)));
        }
        surface_point_id_ = {};
        surface_point_index_ = -1;
        surface_point_snapshot_ = {};
        view.notify_document_modified();
        return;
    }

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
        std::vector<Selection> expanded;
        for (const auto& h : hits) {
            for (const auto& m : expand_to_group(doc, h)) {
                if (std::find(expanded.begin(), expanded.end(), m) == expanded.end()) {
                    expanded.push_back(m);
                }
            }
        }
        hits = std::move(expanded);
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
        } else if (sel.kind == SelectKind::Block) {
            auto* bl = doc.find_block(sel.id);
            if (!bl) continue;
            const auto& orig = std::get<cadino::core::Block>(it->second);
            cadino::core::Block after = *bl;
            *bl = orig;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyBlockCommand>(sel.id, std::move(after)));
        } else if (sel.kind == SelectKind::BlockInstance) {
            auto* inst = doc.find_block_instance(sel.id);
            if (!inst) continue;
            const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
            cadino::core::BlockInstance after = *inst;
            *inst = orig;
            view.command_stack().execute(
                std::make_unique<cadino::core::ModifyBlockInstanceCommand>(sel.id, std::move(after)));
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
        } else if (sel.kind == SelectKind::NurbsCurve) {
            const auto* curve = view.document().find_curve(sel.id);
            if (!curve) continue;
            const auto samples = curve->tessellate(96);
            for (std::size_t i = 1; i < samples.size(); ++i) {
                p.drawLine(
                    view.model_to_screen({samples[i - 1].x(), samples[i - 1].y()}),
                    view.model_to_screen({samples[i].x(), samples[i].y()}));
            }
        }
    }
}

}  // namespace cadino::ui
