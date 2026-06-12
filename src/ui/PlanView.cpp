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
    draw_doors_windows(p);
    draw_curves(p);
    draw_blocks(p);
    draw_block_instances(p);
    draw_surfaces(p);
    if (tool_) {
        tool_->paint_overlay(p, *this);
    }
    draw_snap_marker(p);
}

void PlanView::draw_slabs(QPainter& p) {
    QPen pen(QColor(120, 90, 60), 1, Qt::DotLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(190, 165, 130, 50));

    for (const auto& [id, s] : document_.slabs()) {
        if (s.outline.size() < 3) continue;
        QPolygonF poly;
        for (const auto& v : s.outline) {
            poly << model_to_screen({v.x(), v.y()});
        }
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
        p.setBrush(QColor::fromRgbF(c.color.r, c.color.g, c.color.b, 0.55f));
        const QPointF center_s = model_to_screen({c.position.x(), c.position.y()});
        const double r_s = c.radius * zoom_;
        p.drawEllipse(center_s, r_s, r_s);
        p.drawPoint(center_s);
    }
}

QPointF PlanView::apply_snap(QPointF model_pos) {
    const double tol = 12.0 / zoom_;
    last_snap_ = snap_.snap(model_pos, document_, tol);
    return last_snap_.found() ? last_snap_.position : model_pos;
}

void PlanView::draw_snap_marker(QPainter& p) {
    if (!last_snap_.found()) return;

    const QPointF s = model_to_screen(last_snap_.position);
    QColor color;
    switch (last_snap_.kind) {
        case SnapKind::Endpoint: color = QColor(220, 80, 80); break;
        case SnapKind::Midpoint: color = QColor(80, 180, 100); break;
        case SnapKind::Corner:   color = QColor(220, 160, 60); break;
        case SnapKind::Grid:     color = QColor(120, 120, 200); break;
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
        p.setBrush(QColor::fromRgbF(w.color.r, w.color.g, w.color.b, 0.55f));
        const QPointF start_m{w.start.x(), w.start.y()};
        const QPointF end_m{w.end.x(), w.end.y()};

        const QPointF dir = end_m - start_m;
        const double len = std::hypot(dir.x(), dir.y());
        if (len < 1e-6) continue;

        const QPointF unit = dir / len;
        const QPointF normal(-unit.y(), unit.x());
        const double half = w.thickness * 0.5;
        const QPointF off = normal * half;

        const QPointF p1 = start_m + off;
        const QPointF p2 = end_m + off;
        const QPointF p3 = end_m - off;
        const QPointF p4 = start_m - off;

        QPolygonF poly;
        poly << model_to_screen(p1) << model_to_screen(p2)
             << model_to_screen(p3) << model_to_screen(p4);
        p.drawPolygon(poly);
    }
}

void PlanView::draw_block_instances(QPainter& p) {
    QPen edge_pen(QColor(60, 60, 70));
    edge_pen.setCosmetic(true);
    edge_pen.setWidth(2);

    for (const auto& [id, inst] : document_.block_instances()) {
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
        const auto samples = curve.tessellate(128);
        if (samples.size() < 2) continue;

        const bool selected = is_selected({id, SelectKind::NurbsCurve});
        QPen pen(QColor::fromRgbF(curve.color.r, curve.color.g, curve.color.b),
                 selected ? curve.line_width + 1.5f : curve.line_width);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        for (std::size_t i = 1; i < samples.size(); ++i) {
            const QPointF a = model_to_screen({samples[i - 1].x(), samples[i - 1].y()});
            const QPointF b = model_to_screen({samples[i].x(), samples[i].y()});
            p.drawLine(a, b);
        }

        if (selected) {
            QPen poly_pen(QColor(150, 150, 160, 200), 1, Qt::DotLine);
            poly_pen.setCosmetic(true);
            p.setPen(poly_pen);
            for (std::size_t i = 1; i < curve.control_points.size(); ++i) {
                const QPointF a = model_to_screen(
                    {curve.control_points[i - 1].x(), curve.control_points[i - 1].y()});
                const QPointF b = model_to_screen(
                    {curve.control_points[i].x(), curve.control_points[i].y()});
                p.drawLine(a, b);
            }
            p.setBrush(QColor(80, 220, 240));
            p.setPen(Qt::NoPen);
            for (const auto& cp : curve.control_points) {
                p.drawEllipse(model_to_screen({cp.x(), cp.y()}), 4, 4);
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

        QPen dir_pen(QColor(120, 80, 40, 200), 1);
        dir_pen.setCosmetic(true);
        p.setPen(dir_pen);
        p.drawLine(model_to_screen({b.position.x(), b.position.y()}),
                   model_to_screen(rot(hx, 0.0)));
        p.setPen(box_pen);
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
    if (event->key() == Qt::Key_Escape && tool_) {
        tool_->on_cancel(*this);
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace cadino::ui
