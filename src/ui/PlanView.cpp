#include "PlanView.hpp"

#include <cmath>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

#include "Tool.hpp"
#include "command/CommandStack.hpp"
#include "document/Document.hpp"
#include "entity/AngularDimension.hpp"
#include "entity/Dimension.hpp"
#include "entity/Leader.hpp"
#include "entity/RadialDimension.hpp"
#include "entity/TextAnnotation.hpp"

namespace cadino::ui {

namespace {
constexpr double kMinZoom = 0.005;
constexpr double kMaxZoom = 20.0;
constexpr double kGridStep = 100.0;    // mm
constexpr double kGridMajor = 1000.0;  // mm
}  // namespace

PlanView::PlanView(cadino::core::Document& doc, cadino::core::CommandStack& stack,
                   QWidget* parent)
    : QWidget(parent), document_(doc), stack_(stack) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    update_transform();
}

PlanView::~PlanView() = default;

void PlanView::notify_document_modified() {
    emit document_modified();
    update();
}

void PlanView::set_selections(std::vector<Selection> sel) {
    if (selections_ == sel) return;
    selections_ = std::move(sel);
    emit selection_changed();
    update();
}

void PlanView::add_to_selection(Selection sel) {
    if (is_selected(sel)) return;
    selections_.push_back(sel);
    emit selection_changed();
    update();
}

void PlanView::remove_from_selection(Selection sel) {
    const auto it = std::find(selections_.begin(), selections_.end(), sel);
    if (it == selections_.end()) return;
    selections_.erase(it);
    emit selection_changed();
    update();
}

void PlanView::toggle_selection(Selection sel) {
    if (is_selected(sel)) remove_from_selection(sel);
    else add_to_selection(sel);
}

void PlanView::clear_selection() {
    set_selections({});
}

bool PlanView::is_selected(Selection sel) const noexcept {
    return std::find(selections_.begin(), selections_.end(), sel) != selections_.end();
}

void PlanView::set_tool(std::unique_ptr<Tool> tool) {
    if (tool_) {
        tool_->on_cancel(*this);
    }
    tool_ = std::move(tool);
    update();
}

QPointF PlanView::screen_to_model(QPointF screen) const {
    return screen_to_model_.map(screen);
}

QPointF PlanView::model_to_screen(QPointF model) const {
    return model_to_screen_.map(model);
}

Eigen::Vector3d PlanView::plane_to_world(QPointF uv) const noexcept {
    switch (plane_) {
        case DrawPlane::Top:   return {uv.x(), uv.y(), 0.0};
        case DrawPlane::Front: return {uv.x(), 0.0, uv.y()};
        case DrawPlane::Right: return {0.0, uv.x(), uv.y()};
    }
    return {uv.x(), uv.y(), 0.0};
}

QPointF PlanView::world_to_plane(Eigen::Vector3d w) const noexcept {
    switch (plane_) {
        case DrawPlane::Top:   return {w.x(), w.y()};
        case DrawPlane::Front: return {w.x(), w.z()};
        case DrawPlane::Right: return {w.y(), w.z()};
    }
    return {w.x(), w.y()};
}

void PlanView::update_transform() {
    QTransform t;
    t.translate(view_offset_.x() + width() / 2.0, view_offset_.y() + height() / 2.0);
    t.scale(zoom_, -zoom_);
    model_to_screen_ = t;
    bool ok = false;
    screen_to_model_ = t.inverted(&ok);
    if (!ok) {
        screen_to_model_ = QTransform{};
    }
}

void PlanView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update_transform();
}

void PlanView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(248, 248, 248));
    p.setRenderHint(QPainter::Antialiasing);
    draw_grid(p);
    draw_slabs(p);
    draw_boxes(p);
    draw_cylinders(p);
    draw_walls(p);
    if (plane_ == DrawPlane::Top) {
        // These entity types are 2D-in-XY and only meaningful in the plan view
        // for now (they don't yet carry a plane tag).
        draw_doors_windows(p);
        draw_blocks(p);
        draw_block_instances(p);
        draw_surfaces(p);
        draw_dimensions(p);
        draw_texts(p);
        draw_leaders(p);
        draw_angular_dims(p);
        draw_radial_dims(p);
    }
    draw_curves(p);
    if (tool_) {
        tool_->paint_overlay(p, *this);
    }
    draw_snap_marker(p);
    draw_view_label(p);
}

void PlanView::draw_slabs(QPainter& p) {
    QPen border(QColor(120, 90, 60), 1, Qt::DotLine);
    border.setCosmetic(true);

    for (const auto& [id, s] : document_.slabs()) {
        if (!layer_visible(s.layer_id)) continue;
        if (s.outline.size() < 3) continue;
        QPolygonF poly;
        for (const auto& v : s.outline) {
            poly << model_to_screen({v.x(), v.y()});
        }

        QBrush brush;
        const QColor c = QColor::fromRgbF(s.hatch_color.r, s.hatch_color.g,
                                          s.hatch_color.b);
        switch (s.hatch_pattern) {
            case 1: brush = QBrush(c.lighter(150), Qt::SolidPattern); break;
            case 2: brush = QBrush(c, Qt::HorPattern); break;
            case 3: brush = QBrush(c, Qt::VerPattern); break;
            case 4: brush = QBrush(c, Qt::CrossPattern); break;
            case 5: brush = QBrush(c, Qt::BDiagPattern); break;
            case 6: brush = QBrush(c, Qt::FDiagPattern); break;
            case 7: brush = QBrush(c, Qt::DiagCrossPattern); break;
            default: brush = QBrush(QColor(190, 165, 130, 50)); break;
        }
        p.setPen(border);
        p.setBrush(brush);
        p.drawPolygon(poly);
    }
}

void PlanView::draw_doors_windows(QPainter& p) {
    for (const auto& [id, d] : document_.doors()) {
        const auto* w = document_.find_wall(d.host_wall);
        if (!w) continue;
        const QPointF a{w->start.x(), w->start.y()};
        const QPointF b{w->end.x(), w->end.y()};
        const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
        if (len < 1e-6) continue;
        const QPointF unit{(b.x() - a.x()) / len, (b.y() - a.y()) / len};
        const QPointF normal{-unit.y(), unit.x()};
        const QPointF center = a + unit * d.position_along;
        const QPointF p1 = center - unit * (d.width * 0.5);
        const QPointF p2 = center + unit * (d.width * 0.5);
        const QPointF hinge = p1;
        const QPointF swing_end = hinge + normal * d.width;

        QPen pen(QColor(220, 130, 60), 2);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::white);
        p.drawLine(model_to_screen(p1), model_to_screen(p2));
        QPen arc_pen(QColor(220, 130, 60, 180), 1, Qt::DashLine);
        arc_pen.setCosmetic(true);
        p.setPen(arc_pen);
        const QPointF hinge_s = model_to_screen(hinge);
        const QPointF tip_s = model_to_screen(swing_end);
        const double r_s = std::hypot(tip_s.x() - hinge_s.x(), tip_s.y() - hinge_s.y());
        p.drawArc(QRectF(hinge_s.x() - r_s, hinge_s.y() - r_s, 2 * r_s, 2 * r_s),
                  0, 90 * 16);
    }

    for (const auto& [id, win] : document_.windows()) {
        const auto* w = document_.find_wall(win.host_wall);
        if (!w) continue;
        const QPointF a{w->start.x(), w->start.y()};
        const QPointF b{w->end.x(), w->end.y()};
        const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
        if (len < 1e-6) continue;
        const QPointF unit{(b.x() - a.x()) / len, (b.y() - a.y()) / len};
        const QPointF normal{-unit.y(), unit.x()};
        const QPointF center = a + unit * win.position_along;
        const QPointF p1 = center - unit * (win.width * 0.5);
        const QPointF p2 = center + unit * (win.width * 0.5);
        const QPointF off = normal * (w->thickness * 0.5);
        QPen pen(QColor(80, 150, 200), 2);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.drawLine(model_to_screen(p1 + off), model_to_screen(p2 + off));
        p.drawLine(model_to_screen(p1 - off), model_to_screen(p2 - off));
        p.drawLine(model_to_screen(p1), model_to_screen(p2));
    }
}

void PlanView::draw_cylinders(QPainter& p) {
    QPen pen(QColor(40, 40, 40));
    pen.setCosmetic(true);
    pen.setWidth(2);
    p.setPen(pen);

    for (const auto& [id, c] : document_.cylinders()) {
        if (!layer_visible(c.layer_id)) continue;
        p.setBrush(QColor::fromRgbF(c.color.r, c.color.g, c.color.b, 0.55f));
        if (plane_ == DrawPlane::Top) {
            const QPointF center_s = model_to_screen({c.position.x(), c.position.y()});
            const double r_s = c.radius * zoom_;
            p.drawEllipse(center_s, r_s, r_s);
            p.drawPoint(center_s);
        } else {
            // Elevation: cylinder appears as a rectangle of width 2r and
            // height `height` starting at base_z.
            const double u_center = (plane_ == DrawPlane::Front)
                ? c.position.x() : c.position.y();
            QPolygonF poly;
            poly << model_to_screen({u_center - c.radius, c.base_z})
                 << model_to_screen({u_center + c.radius, c.base_z})
                 << model_to_screen({u_center + c.radius, c.base_z + c.height})
                 << model_to_screen({u_center - c.radius, c.base_z + c.height});
            p.drawPolygon(poly);
        }
    }
}

bool PlanView::layer_visible(cadino::core::EntityId layer_id) const {
    if (!layer_id.valid()) return true;
    const auto* layer = document_.find_layer(layer_id);
    return !layer || layer->visible;
}

bool PlanView::layer_locked(cadino::core::EntityId layer_id) const {
    if (!layer_id.valid()) return false;
    const auto* layer = document_.find_layer(layer_id);
    return layer && layer->locked;
}

QPointF PlanView::apply_snap(QPointF model_pos) {
    const double tol = 12.0 / zoom_;
    if (plane_ == DrawPlane::Top) {
        last_snap_ = snap_.snap(model_pos, document_, tol);
        return last_snap_.found() ? last_snap_.position : model_pos;
    }

    // Elevation: run a simplified snap ourselves — project every entity key
    // point into this plane's (u, v) coords, then pick the closest within
    // tolerance. Falls back to grid if nothing is close.
    last_snap_ = {};
    double best_d = tol;
    SnapResult best{};

    auto consider = [&](QPointF candidate, SnapKind kind) {
        const double d = std::hypot(candidate.x() - model_pos.x(),
                                    candidate.y() - model_pos.y());
        if (d < best_d) {
            best_d = d;
            best = {candidate, kind};
        }
    };

    if (snap_.endpoint_enabled()) {
        // Wall base/top endpoints and corner extents projected.
        for (const auto& [id, w] : document_.walls()) {
            if (!layer_visible(w.layer_id)) continue;
            const QPointF s = world_to_plane({w.start.x(), w.start.y(), 0.0});
            const QPointF e = world_to_plane({w.end.x(),   w.end.y(),   0.0});
            const QPointF s_top = world_to_plane({w.start.x(), w.start.y(), w.height});
            const QPointF e_top = world_to_plane({w.end.x(),   w.end.y(),   w.height});
            consider(s, SnapKind::Endpoint);
            consider(e, SnapKind::Endpoint);
            consider(s_top, SnapKind::Endpoint);
            consider(e_top, SnapKind::Endpoint);
        }
        for (const auto& [id, curve] : document_.curves()) {
            if (!layer_visible(curve.layer_id)) continue;
            for (const auto& cp : curve.control_points) {
                consider(world_to_plane(cp), SnapKind::Endpoint);
            }
        }
    }
    if (snap_.corner_enabled()) {
        for (const auto& [id, b] : document_.boxes()) {
            if (!layer_visible(b.layer_id)) continue;
            const double hx = b.size_xy.x() * 0.5;
            const double hy = b.size_xy.y() * 0.5;
            const double c = std::cos(b.rotation_z);
            const double s = std::sin(b.rotation_z);
            auto corner = [&](double lx, double ly, double lz) {
                return world_to_plane({
                    b.position.x() + c * lx - s * ly,
                    b.position.y() + s * lx + c * ly,
                    b.base_z + lz});
            };
            for (double sx : {-hx, hx})
                for (double sy : {-hy, hy})
                    for (double sz : {0.0, b.height})
                        consider(corner(sx, sy, sz), SnapKind::Corner);
        }
    }
    if (snap_.center_enabled()) {
        for (const auto& [id, c] : document_.cylinders()) {
            if (!layer_visible(c.layer_id)) continue;
            const QPointF p_bot = world_to_plane({c.position.x(), c.position.y(), c.base_z});
            const QPointF p_top = world_to_plane({c.position.x(), c.position.y(),
                                                   c.base_z + c.height});
            consider(p_bot, SnapKind::Center);
            consider(p_top, SnapKind::Center);
        }
    }

    if (best.found()) {
        last_snap_ = best;
        return best.position;
    }
    if (snap_.grid_enabled()) {
        const double step = snap_.grid_step();
        const QPointF snapped{std::round(model_pos.x() / step) * step,
                              std::round(model_pos.y() / step) * step};
        if (std::hypot(model_pos.x() - snapped.x(),
                       model_pos.y() - snapped.y()) < tol) {
            last_snap_ = {snapped, SnapKind::Grid};
            return snapped;
        }
    }
    return model_pos;
}

void PlanView::draw_view_label(QPainter& p) {
    QString title;
    QString axes;
    QColor accent;
    switch (plane_) {
        case DrawPlane::Top:
            title = QStringLiteral("TOP  (Plan)");
            axes  = QStringLiteral("X →   Y ↑");
            accent = QColor(60, 130, 220);
            break;
        case DrawPlane::Front:
            title = QStringLiteral("FRONT  (Elevation)");
            axes  = QStringLiteral("X →   Z ↑");
            accent = QColor(200, 90, 60);
            break;
        case DrawPlane::Right:
            title = QStringLiteral("RIGHT  (Elevation)");
            axes  = QStringLiteral("Y →   Z ↑");
            accent = QColor(60, 170, 100);
            break;
    }

    QFont title_font = p.font();
    title_font.setPointSizeF(10.0);
    title_font.setBold(true);
    QFont axes_font = p.font();
    axes_font.setPointSizeF(9.0);
    const QFontMetrics tm(title_font);
    const QFontMetrics am(axes_font);

    const int pad = 6;
    const int y = pad;
    const int title_w = tm.horizontalAdvance(title);
    const int axes_w  = am.horizontalAdvance(axes);
    const int box_w = std::max(title_w, axes_w) + pad * 2;
    const int box_h = tm.height() + am.height() + pad;

    const QRectF bg(pad, y, box_w, box_h);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawRoundedRect(bg, 4, 4);
    p.setPen(QPen(accent, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(bg, 4, 4);

    p.setFont(title_font);
    p.setPen(accent);
    p.drawText(QRectF(bg.left() + pad, bg.top() + pad / 2.0,
                      bg.width() - pad * 2, tm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, title);
    p.setFont(axes_font);
    p.setPen(QColor(90, 90, 100));
    p.drawText(QRectF(bg.left() + pad, bg.top() + pad / 2.0 + tm.height(),
                      bg.width() - pad * 2, am.height()),
               Qt::AlignLeft | Qt::AlignVCenter, axes);
}

void PlanView::draw_snap_marker(QPainter& p) {
    if (!last_snap_.found()) return;

    const QPointF s = model_to_screen(last_snap_.position);
    QColor color;
    switch (last_snap_.kind) {
        case SnapKind::Endpoint:      color = QColor(220, 80, 80); break;
        case SnapKind::Midpoint:      color = QColor(80, 180, 100); break;
        case SnapKind::Corner:        color = QColor(220, 160, 60); break;
        case SnapKind::Center:        color = QColor(200, 100, 200); break;
        case SnapKind::Intersection:  color = QColor(60, 200, 220); break;
        case SnapKind::Perpendicular: color = QColor(160, 200, 80); break;
        case SnapKind::Nearest:       color = QColor(245, 175, 60); break;
        case SnapKind::Grid:          color = QColor(120, 120, 200); break;
        default: return;
    }

    QPen pen(color, 2);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal r = 6.0;
    switch (last_snap_.kind) {
        case SnapKind::Endpoint:
            p.drawRect(QRectF(s.x() - r, s.y() - r, 2 * r, 2 * r));
            break;
        case SnapKind::Midpoint:
            p.drawPolygon(QPolygonF{
                {s.x(), s.y() - r},
                {s.x() + r, s.y() + r},
                {s.x() - r, s.y() + r}});
            break;
        case SnapKind::Corner:
            p.drawLine(s.x() - r, s.y() - r, s.x() + r, s.y() + r);
            p.drawLine(s.x() - r, s.y() + r, s.x() + r, s.y() - r);
            break;
        case SnapKind::Center:
            p.drawEllipse(s, r, r);
            p.drawPoint(s);
            break;
        case SnapKind::Intersection:
            p.drawLine(s.x() - r, s.y(), s.x() + r, s.y());
            p.drawLine(s.x(), s.y() - r, s.x(), s.y() + r);
            p.drawLine(s.x() - r * 0.7, s.y() - r * 0.7,
                       s.x() + r * 0.7, s.y() + r * 0.7);
            p.drawLine(s.x() - r * 0.7, s.y() + r * 0.7,
                       s.x() + r * 0.7, s.y() - r * 0.7);
            break;
        case SnapKind::Perpendicular: {
            QPolygonF poly;
            poly << QPointF(s.x() - r, s.y() + r)
                 << QPointF(s.x() - r, s.y() - r)
                 << QPointF(s.x() + r, s.y() - r);
            p.drawPolyline(poly);
            p.drawLine(QPointF(s.x() - r, s.y()), QPointF(s.x(), s.y()));
            p.drawLine(QPointF(s.x(), s.y()), QPointF(s.x(), s.y() - r));
            break;
        }
        case SnapKind::Nearest: {
            QPolygonF hg;
            hg << QPointF(s.x() - r, s.y() + r * 0.4)
               << QPointF(s.x(),     s.y() - r * 0.6)
               << QPointF(s.x() + r, s.y() + r * 0.4);
            p.drawPolyline(hg);
            break;
        }
        case SnapKind::Grid:
            p.drawEllipse(s, r - 1, r - 1);
            break;
        default: break;
    }
}

void PlanView::draw_grid(QPainter& p) {
    const auto top_left = screen_to_model(QPointF(0, 0));
    const auto bottom_right = screen_to_model(QPointF(width(), height()));

    const double min_x = std::min(top_left.x(), bottom_right.x());
    const double max_x = std::max(top_left.x(), bottom_right.x());
    const double min_y = std::min(top_left.y(), bottom_right.y());
    const double max_y = std::max(top_left.y(), bottom_right.y());

    if ((max_x - min_x) / kGridStep > 2000.0) {
        return;  // too zoomed out, skip minor grid
    }

    QPen minor(QColor(225, 225, 225));
    minor.setCosmetic(true);
    p.setPen(minor);

    const double start_x = std::floor(min_x / kGridStep) * kGridStep;
    for (double x = start_x; x <= max_x; x += kGridStep) {
        if (std::fmod(std::abs(x), kGridMajor) < 0.5) continue;
        const auto a = model_to_screen({x, min_y});
        const auto b = model_to_screen({x, max_y});
        p.drawLine(a, b);
    }
    const double start_y = std::floor(min_y / kGridStep) * kGridStep;
    for (double y = start_y; y <= max_y; y += kGridStep) {
        if (std::fmod(std::abs(y), kGridMajor) < 0.5) continue;
        const auto a = model_to_screen({min_x, y});
        const auto b = model_to_screen({max_x, y});
        p.drawLine(a, b);
    }

    QPen major(QColor(200, 200, 200));
    major.setCosmetic(true);
    p.setPen(major);
    for (double x = std::floor(min_x / kGridMajor) * kGridMajor; x <= max_x; x += kGridMajor) {
        const auto a = model_to_screen({x, min_y});
        const auto b = model_to_screen({x, max_y});
        p.drawLine(a, b);
    }
    for (double y = std::floor(min_y / kGridMajor) * kGridMajor; y <= max_y; y += kGridMajor) {
        const auto a = model_to_screen({min_x, y});
        const auto b = model_to_screen({max_x, y});
        p.drawLine(a, b);
    }

    QPen axis(QColor(170, 170, 170));
    axis.setCosmetic(true);
    axis.setWidth(2);
    p.setPen(axis);
    p.drawLine(model_to_screen({min_x, 0}), model_to_screen({max_x, 0}));
    p.drawLine(model_to_screen({0, min_y}), model_to_screen({0, max_y}));
}

void PlanView::draw_walls(QPainter& p) {
    QPen wall_pen(QColor(40, 40, 40));
    wall_pen.setCosmetic(true);
    wall_pen.setWidth(2);
    p.setPen(wall_pen);

    for (const auto& [id, w] : document_.walls()) {
        if (!layer_visible(w.layer_id)) continue;
        p.setBrush(QColor::fromRgbF(w.color.r, w.color.g, w.color.b, 0.55f));

        if (plane_ == DrawPlane::Top) {
            const QPointF start_m{w.start.x(), w.start.y()};
            const QPointF end_m{w.end.x(), w.end.y()};

            const QPointF dir = end_m - start_m;
            const double len = std::hypot(dir.x(), dir.y());
            if (len < 1e-6) continue;

            const QPointF unit = dir / len;
            const QPointF normal(-unit.y(), unit.x());
            const double half = w.thickness * 0.5;
            const QPointF off = normal * half;

            QPolygonF poly;
            poly << model_to_screen(start_m + off)
                 << model_to_screen(end_m + off)
                 << model_to_screen(end_m - off)
                 << model_to_screen(start_m - off);
            p.drawPolygon(poly);
        } else {
            // Elevation views: project the wall footprint's extent onto the
            // horizontal axis of this plane, stretched vertically from 0 to h.
            double u0, u1;
            if (plane_ == DrawPlane::Front) {
                u0 = std::min(w.start.x(), w.end.x());
                u1 = std::max(w.start.x(), w.end.x());
            } else {
                u0 = std::min(w.start.y(), w.end.y());
                u1 = std::max(w.start.y(), w.end.y());
            }
            QPolygonF poly;
            poly << model_to_screen({u0, 0.0})
                 << model_to_screen({u1, 0.0})
                 << model_to_screen({u1, w.height})
                 << model_to_screen({u0, w.height});
            p.drawPolygon(poly);
        }
    }
}

void PlanView::draw_block_instances(QPainter& p) {
    QPen edge_pen(QColor(60, 60, 70));
    edge_pen.setCosmetic(true);
    edge_pen.setWidth(2);

    for (const auto& [id, inst] : document_.block_instances()) {
        if (!layer_visible(inst.layer_id)) continue;
        const auto* def = document_.find_block_def(inst.definition_id);
        if (!def) continue;
        const bool selected = is_selected({id, SelectKind::BlockInstance});

        for (const auto& local_b : def->boxes) {
            const auto b = inst.world_box(local_b);
            p.setPen(edge_pen);
            p.setBrush(QColor::fromRgbF(b.color.r, b.color.g, b.color.b, 0.55f));
            const double hx = b.size_xy.x() * 0.5;
            const double hy = b.size_xy.y() * 0.5;
            const double c = std::cos(b.rotation_z);
            const double s = std::sin(b.rotation_z);
            const auto rot = [&](double x, double y) {
                return QPointF(b.position.x() + c * x - s * y,
                               b.position.y() + s * x + c * y);
            };
            QPolygonF poly;
            poly << model_to_screen(rot(-hx, -hy))
                 << model_to_screen(rot( hx, -hy))
                 << model_to_screen(rot( hx,  hy))
                 << model_to_screen(rot(-hx,  hy));
            p.drawPolygon(poly);
        }
        for (const auto& local_c : def->cylinders) {
            const auto c = inst.world_cylinder(local_c);
            p.setPen(edge_pen);
            p.setBrush(QColor::fromRgbF(c.color.r, c.color.g, c.color.b, 0.55f));
            const QPointF center_s = model_to_screen({c.position.x(), c.position.y()});
            const double r_s = c.radius * zoom_;
            p.drawEllipse(center_s, r_s, r_s);
        }

        const QPointF anchor = model_to_screen({inst.position.x(), inst.position.y()});
        QPen marker_pen(selected ? QColor(60, 200, 140) : QColor(120, 200, 160),
                        selected ? 3 : 1);
        marker_pen.setCosmetic(true);
        p.setPen(marker_pen);
        p.setBrush(Qt::NoBrush);
        const QRectF diamond(anchor.x() - 6, anchor.y() - 6, 12, 12);
        QPolygonF diamondPoly;
        diamondPoly << QPointF(anchor.x(), anchor.y() - 7)
                    << QPointF(anchor.x() + 7, anchor.y())
                    << QPointF(anchor.x(), anchor.y() + 7)
                    << QPointF(anchor.x() - 7, anchor.y());
        p.drawPolygon(diamondPoly);
        (void)diamond;

        if (!def->name.empty()) {
            QFont font = p.font();
            font.setBold(true);
            p.setFont(font);
            p.setPen(QColor(40, 80, 60));
            p.drawText(anchor + QPointF(10, -10),
                       QString::fromStdString(def->name));
        }
    }
}

void PlanView::draw_surfaces(QPainter& p) {
    for (const auto& [id, surf] : document_.surfaces()) {
        if (!layer_visible(surf.layer_id)) continue;
        if (surf.rows < 2 || surf.cols < 2) continue;
        const bool selected = is_selected({id, SelectKind::NurbsSurface});

        QPen edge_pen(QColor::fromRgbF(surf.color.r, surf.color.g, surf.color.b),
                      selected ? 2 : 1, Qt::DashLine);
        edge_pen.setCosmetic(true);
        p.setPen(edge_pen);
        p.setBrush(QColor::fromRgbF(surf.color.r, surf.color.g, surf.color.b, 0.25f));

        QPolygonF poly;
        const auto& cp00 = surf.at(0, 0);
        const auto& cp01 = surf.at(0, surf.cols - 1);
        const auto& cp10 = surf.at(surf.rows - 1, 0);
        const auto& cp11 = surf.at(surf.rows - 1, surf.cols - 1);
        poly << model_to_screen({cp00.x(), cp00.y()})
             << model_to_screen({cp01.x(), cp01.y()})
             << model_to_screen({cp11.x(), cp11.y()})
             << model_to_screen({cp10.x(), cp10.y()});
        p.drawPolygon(poly);

        if (selected) {
            QPen grid_pen(QColor(150, 150, 160, 180), 1, Qt::DotLine);
            grid_pen.setCosmetic(true);
            p.setPen(grid_pen);
            p.setBrush(Qt::NoBrush);
            for (int r = 0; r < surf.rows; ++r) {
                QPolygonF row;
                for (int c = 0; c < surf.cols; ++c) {
                    const auto& cp = surf.at(r, c);
                    row << model_to_screen({cp.x(), cp.y()});
                }
                p.drawPolyline(row);
            }
            for (int c = 0; c < surf.cols; ++c) {
                QPolygonF col;
                for (int r = 0; r < surf.rows; ++r) {
                    const auto& cp = surf.at(r, c);
                    col << model_to_screen({cp.x(), cp.y()});
                }
                p.drawPolyline(col);
            }
            p.setBrush(QColor(220, 170, 60));
            p.setPen(Qt::NoPen);
            for (const auto& cp : surf.control_points) {
                p.drawEllipse(model_to_screen({cp.x(), cp.y()}), 4, 4);
            }
        }
    }
}

void PlanView::draw_blocks(QPainter& p) {
    QPen block_pen(QColor(60, 60, 70));
    block_pen.setCosmetic(true);
    block_pen.setWidth(2);

    for (const auto& [id, block] : document_.blocks()) {
        if (!layer_visible(block.layer_id)) continue;
        const bool selected = is_selected({id, SelectKind::Block});

        // Boxes
        for (const auto& local_b : block.boxes) {
            const auto b = block.world_box(local_b);
            p.setPen(block_pen);
            p.setBrush(QColor::fromRgbF(b.color.r, b.color.g, b.color.b, 0.55f));
            const double hx = b.size_xy.x() * 0.5;
            const double hy = b.size_xy.y() * 0.5;
            const double c = std::cos(b.rotation_z);
            const double s = std::sin(b.rotation_z);
            const auto rot = [&](double x, double y) {
                return QPointF(b.position.x() + c * x - s * y,
                               b.position.y() + s * x + c * y);
            };
            QPolygonF poly;
            poly << model_to_screen(rot(-hx, -hy))
                 << model_to_screen(rot( hx, -hy))
                 << model_to_screen(rot( hx,  hy))
                 << model_to_screen(rot(-hx,  hy));
            p.drawPolygon(poly);
        }

        // Cylinders
        for (const auto& local_c : block.cylinders) {
            const auto c = block.world_cylinder(local_c);
            p.setPen(block_pen);
            p.setBrush(QColor::fromRgbF(c.color.r, c.color.g, c.color.b, 0.55f));
            const QPointF center_s = model_to_screen({c.position.x(), c.position.y()});
            const double r_s = c.radius * zoom_;
            p.drawEllipse(center_s, r_s, r_s);
        }

        // Origin marker + name + selection halo
        const QPointF anchor = model_to_screen({block.position.x(), block.position.y()});
        QPen marker_pen(selected ? QColor(60, 130, 220) : QColor(120, 120, 140),
                        selected ? 3 : 1);
        marker_pen.setCosmetic(true);
        p.setPen(marker_pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(anchor, 6, 6);
        p.drawLine(anchor + QPointF(-9, 0), anchor + QPointF(9, 0));
        p.drawLine(anchor + QPointF(0, -9), anchor + QPointF(0, 9));

        if (!block.name.empty()) {
            QFont font = p.font();
            font.setBold(true);
            p.setFont(font);
            p.setPen(QColor(60, 60, 80));
            p.drawText(anchor + QPointF(10, -10),
                       QString::fromStdString(block.name));
        }
    }
}

void PlanView::draw_curves(QPainter& p) {
    for (const auto& [id, curve] : document_.curves()) {
        if (!layer_visible(curve.layer_id)) continue;
        const auto samples = curve.tessellate(128);
        if (samples.size() < 2) continue;

        const bool selected = is_selected({id, SelectKind::NurbsCurve});
        QPen pen(QColor::fromRgbF(curve.color.r, curve.color.g, curve.color.b),
                 selected ? curve.line_width + 1.5f : curve.line_width);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        for (std::size_t i = 1; i < samples.size(); ++i) {
            const QPointF a = model_to_screen(world_to_plane(samples[i - 1]));
            const QPointF b = model_to_screen(world_to_plane(samples[i]));
            p.drawLine(a, b);
        }

        if (selected) {
            QPen poly_pen(QColor(150, 150, 160, 200), 1, Qt::DotLine);
            poly_pen.setCosmetic(true);
            p.setPen(poly_pen);
            for (std::size_t i = 1; i < curve.control_points.size(); ++i) {
                const QPointF a = model_to_screen(world_to_plane(curve.control_points[i - 1]));
                const QPointF b = model_to_screen(world_to_plane(curve.control_points[i]));
                p.drawLine(a, b);
            }
            p.setBrush(QColor(80, 220, 240));
            p.setPen(Qt::NoPen);
            for (const auto& cp : curve.control_points) {
                p.drawEllipse(model_to_screen(world_to_plane(cp)), 4, 4);
            }
        }
    }
}

void PlanView::draw_boxes(QPainter& p) {
    QPen box_pen(QColor(40, 40, 40));
    box_pen.setCosmetic(true);
    box_pen.setWidth(2);
    p.setPen(box_pen);

    for (const auto& [id, b] : document_.boxes()) {
        if (!layer_visible(b.layer_id)) continue;
        p.setBrush(QColor::fromRgbF(b.color.r, b.color.g, b.color.b, 0.55f));

        if (plane_ == DrawPlane::Top) {
            const double hx = b.size_xy.x() * 0.5;
            const double hy = b.size_xy.y() * 0.5;
            const double c = std::cos(b.rotation_z);
            const double s = std::sin(b.rotation_z);
            const auto rot = [&](double x, double y) {
                return QPointF(b.position.x() + c * x - s * y,
                               b.position.y() + s * x + c * y);
            };

            QPolygonF poly;
            poly << model_to_screen(rot(-hx, -hy))
                 << model_to_screen(rot( hx, -hy))
                 << model_to_screen(rot( hx,  hy))
                 << model_to_screen(rot(-hx,  hy));
            p.drawPolygon(poly);

            QPen dir_pen(QColor(120, 80, 40, 200), 1);
            dir_pen.setCosmetic(true);
            p.setPen(dir_pen);
            p.drawLine(model_to_screen({b.position.x(), b.position.y()}),
                       model_to_screen(rot(hx, 0.0)));
            p.setPen(box_pen);
        } else {
            // Elevation projection — axis-aligned rectangle at (u_center ± half, z0..z1).
            const double u_center = (plane_ == DrawPlane::Front)
                ? b.position.x() : b.position.y();
            const double half = 0.5 * ((plane_ == DrawPlane::Front)
                ? b.size_xy.x() : b.size_xy.y());
            const double z0 = b.base_z;
            const double z1 = b.base_z + b.height;
            QPolygonF poly;
            poly << model_to_screen({u_center - half, z0})
                 << model_to_screen({u_center + half, z0})
                 << model_to_screen({u_center + half, z1})
                 << model_to_screen({u_center - half, z1});
            p.drawPolygon(poly);
        }
    }
}

void PlanView::draw_dimensions(QPainter& p) {
    for (const auto& [id, d] : document_.dimensions()) {
        if (!layer_visible(d.layer_id)) continue;
        const Eigen::Vector2d v = d.end - d.start;
        const double len = v.norm();
        if (len < 1e-6) continue;
        const Eigen::Vector2d unit = v / len;
        const Eigen::Vector2d normal(-unit.y(), unit.x());
        const Eigen::Vector2d off = normal * d.offset;
        const Eigen::Vector2d ds = d.start + off;
        const Eigen::Vector2d de = d.end + off;

        const QPointF p_s = model_to_screen({d.start.x(), d.start.y()});
        const QPointF p_e = model_to_screen({d.end.x(), d.end.y()});
        const QPointF p_ds = model_to_screen({ds.x(), ds.y()});
        const QPointF p_de = model_to_screen({de.x(), de.y()});

        QPen pen(QColor::fromRgbF(d.color.r, d.color.g, d.color.b), 1.5);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        const Eigen::Vector2d ext_dir = (off.norm() > 1e-6) ? off.normalized() : normal;
        const QPointF p_ext_s = model_to_screen({d.start.x() + ext_dir.x() * 30.0,
                                                  d.start.y() + ext_dir.y() * 30.0});
        const QPointF p_ext_e = model_to_screen({d.end.x() + ext_dir.x() * 30.0,
                                                  d.end.y() + ext_dir.y() * 30.0});
        p.drawLine(p_ext_s, p_ds);
        p.drawLine(p_ext_e, p_de);
        p.drawLine(p_ds, p_de);

        const double a_scale = d.arrow_size * zoom_;
        auto arrowhead = [&](QPointF tip, Eigen::Vector2d along) {
            const Eigen::Vector2d perp(-along.y(), along.x());
            const QPointF p1(tip.x() + (-along.x() * a_scale + perp.x() * a_scale * 0.4),
                             tip.y() - (-along.y() * a_scale + perp.y() * a_scale * 0.4));
            const QPointF p2(tip.x() + (-along.x() * a_scale - perp.x() * a_scale * 0.4),
                             tip.y() - (-along.y() * a_scale - perp.y() * a_scale * 0.4));
            QPolygonF tri;
            tri << tip << p1 << p2;
            p.setBrush(QColor::fromRgbF(d.color.r, d.color.g, d.color.b));
            p.drawPolygon(tri);
            p.setBrush(Qt::NoBrush);
        };
        arrowhead(p_ds, unit);
        arrowhead(p_de, -unit);

        const QString label = d.text_override.empty()
            ? QString::number(len, 'f', 1) + QStringLiteral(" mm")
            : QString::fromStdString(d.text_override);

        const QPointF mid_screen((p_ds.x() + p_de.x()) * 0.5,
                                 (p_ds.y() + p_de.y()) * 0.5);
        const double yaw = std::atan2(p_de.y() - p_ds.y(), p_de.x() - p_ds.x());
        double text_deg = yaw * 180.0 / 3.14159265358979323846;
        // Keep text upright — flip 180° when it would be upside-down.
        if (text_deg > 90.0 || text_deg < -90.0) text_deg += 180.0;

        QFont font = p.font();
        font.setPointSizeF(std::max(8.0, d.text_height * zoom_ * 0.6));
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor::fromRgbF(d.color.r, d.color.g, d.color.b));

        p.save();
        p.translate(mid_screen);
        p.rotate(text_deg);
        const QFontMetrics fm(font);
        const QSize ts = fm.size(0, label);
        const QRectF rect(-ts.width() / 2.0, -ts.height() - 4,
                          static_cast<qreal>(ts.width()),
                          static_cast<qreal>(ts.height()));
        p.fillRect(rect.adjusted(-3, -1, 3, 1), QColor(248, 248, 248));
        p.drawText(rect, Qt::AlignCenter, label);
        p.restore();
    }
}

void PlanView::draw_radial_dims(QPainter& p) {
    for (const auto& [id, rd] : document_.radial_dims()) {
        if (!layer_visible(rd.layer_id)) continue;

        QPen pen(QColor::fromRgbF(rd.color.r, rd.color.g, rd.color.b), 1.5);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        const QPointF c = model_to_screen({rd.center.x(), rd.center.y()});
        const QPointF lp = model_to_screen({rd.label_position.x(), rd.label_position.y()});
        p.drawLine(c, lp);

        QFont font = p.font();
        font.setPointSizeF(std::max(8.0, rd.text_height * zoom_ * 0.6));
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor::fromRgbF(rd.color.r, rd.color.g, rd.color.b));
        const QString label = !rd.text_override.empty()
            ? QString::fromStdString(rd.text_override)
            : rd.is_diameter
                ? QString(QStringLiteral("Ø %1 mm")).arg(rd.radius * 2.0, 0, 'f', 1)
                : QString(QStringLiteral("R %1 mm")).arg(rd.radius, 0, 'f', 1);
        p.drawText(lp + QPointF(6, -4), label);
    }
}

void PlanView::draw_angular_dims(QPainter& p) {
    for (const auto& [id, ad] : document_.angular_dims()) {
        if (!layer_visible(ad.layer_id)) continue;

        const double a1 = std::atan2(ad.p1.y() - ad.vertex.y(),
                                     ad.p1.x() - ad.vertex.x());
        const double sweep = ad.angle_rad();
        const double r = ad.radius;
        const QPointF v = model_to_screen({ad.vertex.x(), ad.vertex.y()});

        QPen pen(QColor::fromRgbF(ad.color.r, ad.color.g, ad.color.b), 1.5);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        // Extension lines from vertex out past the arc radius along both arms.
        const QPointF arm1_far_m{ad.vertex.x() + std::cos(a1) * r * 1.1,
                                 ad.vertex.y() + std::sin(a1) * r * 1.1};
        const QPointF arm2_far_m{ad.vertex.x() + std::cos(a1 + sweep) * r * 1.1,
                                 ad.vertex.y() + std::sin(a1 + sweep) * r * 1.1};
        p.drawLine(v, model_to_screen(arm1_far_m));
        p.drawLine(v, model_to_screen(arm2_far_m));

        // Arc — sample as a polyline.
        constexpr int kSamples = 40;
        QPolygonF arc;
        for (int i = 0; i <= kSamples; ++i) {
            const double t = static_cast<double>(i) / kSamples;
            const double ang = a1 + sweep * t;
            arc << model_to_screen({ad.vertex.x() + std::cos(ang) * r,
                                    ad.vertex.y() + std::sin(ang) * r});
        }
        p.drawPolyline(arc);

        // Angle label centered on the arc midpoint.
        const double mid = a1 + sweep * 0.5;
        const QPointF label_pt = model_to_screen(
            {ad.vertex.x() + std::cos(mid) * (r + ad.text_height * 0.6),
             ad.vertex.y() + std::sin(mid) * (r + ad.text_height * 0.6)});

        const QString label = ad.text_override.empty()
            ? QString::number(sweep * 180.0 / 3.14159265358979323846, 'f', 1) + QStringLiteral(" deg")
            : QString::fromStdString(ad.text_override);

        QFont font = p.font();
        font.setPointSizeF(std::max(8.0, ad.text_height * zoom_ * 0.6));
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor::fromRgbF(ad.color.r, ad.color.g, ad.color.b));
        p.drawText(label_pt, label);
    }
}

void PlanView::draw_leaders(QPainter& p) {
    for (const auto& [id, l] : document_.leaders()) {
        if (!layer_visible(l.layer_id)) continue;
        const QPointF a = model_to_screen({l.anchor.x(), l.anchor.y()});
        const QPointF b = model_to_screen({l.text_position.x(), l.text_position.y()});

        QPen pen(QColor::fromRgbF(l.color.r, l.color.g, l.color.b), 1.5);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.drawLine(a, b);
        // Short underline tail under the text
        const QPointF tail = b + QPointF(40, 0);
        p.drawLine(b, tail);

        // Arrowhead at anchor pointing from b towards a
        const QPointF dir = (a - b);
        const double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            const QPointF u = dir / len;
            const QPointF n(-u.y(), u.x());
            const double sz = l.arrow_size * zoom_;
            QPolygonF tri;
            tri << a
                << a - u * sz + n * sz * 0.45
                << a - u * sz - n * sz * 0.45;
            p.setBrush(QColor::fromRgbF(l.color.r, l.color.g, l.color.b));
            p.drawPolygon(tri);
            p.setBrush(Qt::NoBrush);
        }

        QFont font = p.font();
        font.setPointSizeF(std::max(8.0, l.height * zoom_ * 0.6));
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor::fromRgbF(l.color.r, l.color.g, l.color.b));
        p.drawText(b + QPointF(4, -4), QString::fromStdString(l.text));
    }
}

void PlanView::draw_texts(QPainter& p) {
    for (const auto& [id, t] : document_.texts()) {
        if (!layer_visible(t.layer_id)) continue;
        const QPointF anchor = model_to_screen({t.position.x(), t.position.y()});

        QFont font = p.font();
        font.setPointSizeF(std::max(8.0, t.height * zoom_ * 0.6));
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor::fromRgbF(t.color.r, t.color.g, t.color.b));

        p.save();
        p.translate(anchor);
        p.rotate(t.rotation_z * 180.0 / 3.14159265358979323846);
        p.drawText(QPointF(0, 0), QString::fromStdString(t.text));
        p.restore();
    }
}

void PlanView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::RightButton && event->modifiers() & Qt::AltModifier)) {
        panning_ = true;
        last_pan_screen_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (tool_) {
        const auto raw = screen_to_model(event->position());
        const auto mp = apply_snap(raw);
        tool_->on_press(*this, mp, event->button());
    }
}

void PlanView::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const auto delta = event->position() - last_pan_screen_;
        view_offset_ += delta;
        last_pan_screen_ = event->position();
        update_transform();
        update();
        return;
    }
    if (tool_) {
        const auto raw = screen_to_model(event->position());
        const auto mp = apply_snap(raw);
        tool_->on_move(*this, mp);
        update();
    } else {
        last_snap_ = {};
    }
}

void PlanView::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
        panning_ = false;
        unsetCursor();
        return;
    }
    if (tool_) {
        const auto raw = screen_to_model(event->position());
        const auto mp = apply_snap(raw);
        tool_->on_release(*this, mp, event->button());
    }
}

void PlanView::wheelEvent(QWheelEvent* event) {
    const double factor = std::pow(1.0015, event->angleDelta().y());
    const double new_zoom = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
    if (std::abs(new_zoom - zoom_) < 1e-12) return;

    const QPointF cursor_screen = event->position();
    const QPointF cursor_model_before = screen_to_model(cursor_screen);
    zoom_ = new_zoom;
    update_transform();
    const QPointF cursor_screen_after = model_to_screen(cursor_model_before);
    view_offset_ += cursor_screen - cursor_screen_after;
    update_transform();
    update();
}

void PlanView::keyPressEvent(QKeyEvent* event) {
    if (tool_ && tool_->on_key(*this, event)) {
        update();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_) {
        tool_->on_cancel(*this);
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace cadino::ui
