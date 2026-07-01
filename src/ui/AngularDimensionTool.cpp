#include "AngularDimensionTool.hpp"

#include <QPainter>

#include "PlanView.hpp"
#include "command/AngularDimensionCommands.hpp"
#include "command/CommandStack.hpp"

namespace cadino::ui {

void AngularDimensionTool::on_press(PlanView& view, QPointF model_pos,
                                    Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    if (!vertex_) {
        vertex_ = model_pos;
        view.update();
        return;
    }
    if (!arm1_) {
        arm1_ = model_pos;
        view.update();
        return;
    }
    cadino::core::AngularDimension a;
    a.vertex = {vertex_->x(), vertex_->y()};
    a.p1 = {arm1_->x(), arm1_->y()};
    a.p2 = {model_pos.x(), model_pos.y()};
    a.plane = static_cast<int>(view.plane());
    a.radius = std::min(
        std::hypot(arm1_->x() - vertex_->x(), arm1_->y() - vertex_->y()),
        std::hypot(model_pos.x() - vertex_->x(), model_pos.y() - vertex_->y())) * 0.7;
    if (a.radius < 100.0) a.radius = 400.0;
    view.command_stack().execute(
        std::make_unique<cadino::core::AddAngularDimensionCommand>(std::move(a)));
    vertex_.reset();
    arm1_.reset();
    view.notify_document_modified();
}

void AngularDimensionTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (vertex_) view.update();
}
void AngularDimensionTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}
void AngularDimensionTool::on_cancel(PlanView& view) {
    vertex_.reset();
    arm1_.reset();
    view.update();
}

void AngularDimensionTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!vertex_) return;
    QPen pen(QColor(220, 200, 80), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(220, 200, 80));

    const QPointF v = view.model_to_screen(*vertex_);
    p.drawEllipse(v, 4, 4);
    if (arm1_) {
        const QPointF a = view.model_to_screen(*arm1_);
        const QPointF b = view.model_to_screen(hover_);
        p.drawLine(v, a);
        p.drawLine(v, b);
    } else {
        const QPointF b = view.model_to_screen(hover_);
        p.drawLine(v, b);
    }
}

}  // namespace cadino::ui
