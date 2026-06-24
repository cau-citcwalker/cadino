#include "DimensionTool.hpp"

#include <cmath>

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/DimensionCommands.hpp"

namespace cadino::ui {

namespace {

// Signed perpendicular offset of `q` from the (a, b) line.
double perp_offset(QPointF a, QPointF b, QPointF q) {
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) return 0.0;
    const double nx = -dy / len;
    const double ny = dx / len;
    return (q.x() - a.x()) * nx + (q.y() - a.y()) * ny;
}

}  // namespace

void DimensionTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (!start_) {
        start_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }
    if (!end_) {
        end_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    cadino::core::Dimension d;
    d.start = {start_->x(), start_->y()};
    d.end = {end_->x(), end_->y()};
    d.offset = perp_offset(*start_, *end_, model_pos);
    view.command_stack().execute(
        std::make_unique<cadino::core::AddDimensionCommand>(std::move(d)));

    start_.reset();
    end_.reset();
    view.notify_document_modified();
}

void DimensionTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (start_) view.update();
}

void DimensionTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void DimensionTool::on_cancel(PlanView& view) {
    start_.reset();
    end_.reset();
    view.update();
}

void DimensionTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!start_) return;

    QPen pen(QColor(60, 130, 220), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(60, 130, 220));

    const QPointF a = view.model_to_screen(*start_);
    p.drawEllipse(a, 4, 4);

    if (!end_) {
        const QPointF b = view.model_to_screen(hover_);
        p.drawLine(a, b);
        const double len = std::hypot(hover_.x() - start_->x(), hover_.y() - start_->y());
        p.setPen(Qt::white);
        p.drawText(b + QPointF(8, -8),
                   QString::number(len, 'f', 1) + QStringLiteral(" mm"));
        return;
    }

    const QPointF b = view.model_to_screen(*end_);
    p.drawLine(a, b);
    p.drawEllipse(b, 4, 4);

    // Preview dimension line at the hover offset.
    const double off = perp_offset(*start_, *end_, hover_);
    const double dx = end_->x() - start_->x();
    const double dy = end_->y() - start_->y();
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) return;
    const double nx = -dy / len;
    const double ny = dx / len;
    const QPointF ds_model(start_->x() + nx * off, start_->y() + ny * off);
    const QPointF de_model(end_->x() + nx * off, end_->y() + ny * off);
    const QPointF ds = view.model_to_screen(ds_model);
    const QPointF de = view.model_to_screen(de_model);

    QPen prev(QColor(60, 130, 220, 200), 1.5);
    prev.setCosmetic(true);
    p.setPen(prev);
    p.drawLine(a, ds);
    p.drawLine(b, de);
    p.drawLine(ds, de);

    p.setPen(Qt::white);
    p.drawText(QPointF((ds.x() + de.x()) * 0.5, (ds.y() + de.y()) * 0.5 - 6),
               QString::number(len, 'f', 1) + QStringLiteral(" mm"));
}

}  // namespace cadino::ui
