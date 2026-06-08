#include "PlanView.hpp"

#include <cmath>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

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

void PlanView::set_selection(Selection sel) {
    if (selection_ == sel) return;
    selection_ = sel;
    emit selection_changed(selection_);
    update();
}

void PlanView::clear_selection() {
    set_selection(Selection{});
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
    draw_boxes(p);
    draw_cylinders(p);
    draw_walls(p);
    if (tool_) {
        tool_->paint_overlay(p, *this);
    }
    draw_snap_marker(p);
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
