#include "PolarArrayTool.hpp"

#include <QPainter>

#include "Array.hpp"
#include "PlanView.hpp"
#include "command/CommandStack.hpp"

namespace cadino::ui {

void PolarArrayTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    if (!captured_) {
        initial_selection_ = view.selections();
        captured_ = true;
    }
    if (initial_selection_.empty()) return;

    polar_array(view.document(), view.command_stack(), initial_selection_,
                {model_pos.x(), model_pos.y()}, count_, sweep_rad_);

    initial_selection_.clear();
    captured_ = false;
    view.notify_document_modified();
}

void PolarArrayTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (captured_) view.update();
}

void PolarArrayTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void PolarArrayTool::on_cancel(PlanView& view) {
    captured_ = false;
    initial_selection_.clear();
    view.update();
}

void PolarArrayTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!captured_ || initial_selection_.empty()) return;

    QPen pen(QColor(220, 160, 60), 1.5, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QPointF c = view.model_to_screen(hover_);
    p.drawEllipse(c, 8, 8);
    p.drawLine(c - QPointF(14, 0), c + QPointF(14, 0));
    p.drawLine(c - QPointF(0, 14), c + QPointF(0, 14));

    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    p.drawText(c + QPointF(16, -10),
               QStringLiteral("Polar array — click to set centre"));
}

}  // namespace cadino::ui
