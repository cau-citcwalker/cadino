#include "BoxTool.hpp"

#include <cmath>

#include <QPainter>

#include "PlanView.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"

namespace cadino::ui {

void BoxTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (!corner1_) {
        corner1_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    const QPointF c1 = *corner1_;
    const QPointF c2 = model_pos;
    const double w = std::abs(c2.x() - c1.x());
    const double d = std::abs(c2.y() - c1.y());
    if (w < 1.0 || d < 1.0) {
        corner1_.reset();
        view.update();
        return;
    }

    cadino::core::Box box;
    box.position = {(c1.x() + c2.x()) * 0.5, (c1.y() + c2.y()) * 0.5};
    box.size_xy = {w, d};
    box.height = default_height_;
    box.base_z = 0.0;

    view.command_stack().execute(
        std::make_unique<cadino::core::AddBoxCommand>(std::move(box)));

    corner1_.reset();
    view.notify_document_modified();
}

void BoxTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (corner1_) view.update();
}

void BoxTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void BoxTool::on_cancel(PlanView& view) {
    corner1_.reset();
    view.update();
}

void BoxTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!corner1_) return;

    QPen pen(QColor(220, 130, 60), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(220, 130, 60, 40));
    const QPointF a = view.model_to_screen(*corner1_);
    const QPointF b = view.model_to_screen(hover_);
    p.drawRect(QRectF(a, b).normalized());

    p.setBrush(QColor(220, 130, 60));
    p.drawEllipse(a, 4, 4);
}

}  // namespace cadino::ui
