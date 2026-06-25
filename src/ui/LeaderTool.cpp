#include "LeaderTool.hpp"

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/LeaderCommands.hpp"

namespace cadino::ui {

void LeaderTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    if (!anchor_) {
        anchor_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }
    cadino::core::Leader l;
    l.anchor = {anchor_->x(), anchor_->y()};
    l.text_position = {model_pos.x(), model_pos.y()};
    l.text = text_.toStdString();
    l.height = height_;
    view.command_stack().execute(
        std::make_unique<cadino::core::AddLeaderCommand>(std::move(l)));
    anchor_.reset();
    view.notify_document_modified();
}

void LeaderTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (anchor_) view.update();
}

void LeaderTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void LeaderTool::on_cancel(PlanView& view) {
    anchor_.reset();
    view.update();
}

void LeaderTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!anchor_) return;
    QPen pen(QColor(60, 200, 220), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(60, 200, 220));

    const QPointF a = view.model_to_screen(*anchor_);
    const QPointF b = view.model_to_screen(hover_);
    p.drawEllipse(a, 4, 4);
    p.drawLine(a, b);

    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    p.drawText(b + QPointF(8, -8), text_);
}

}  // namespace cadino::ui
