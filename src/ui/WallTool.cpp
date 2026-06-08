#include "WallTool.hpp"

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/WallCommands.hpp"

namespace cadino::ui {

void WallTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (!start_) {
        start_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    cadino::core::Wall w;
    w.start = {start_->x(), start_->y()};
    w.end = {model_pos.x(), model_pos.y()};
    view.command_stack().execute(
        std::make_unique<cadino::core::AddWallCommand>(std::move(w)));

    start_.reset();
    view.notify_document_modified();
}

void WallTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (start_) view.update();
}

void WallTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void WallTool::on_cancel(PlanView& view) {
    start_.reset();
    view.update();
}

void WallTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!start_) return;

    QPen pen(QColor(60, 130, 220), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(view.model_to_screen(*start_), view.model_to_screen(hover_));

    const auto a = view.model_to_screen(*start_);
    p.setBrush(QColor(60, 130, 220));
    p.drawEllipse(a, 4, 4);
}

}  // namespace cadino::ui
