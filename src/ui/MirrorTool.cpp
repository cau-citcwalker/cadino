#include "MirrorTool.hpp"

#include <QApplication>
#include <QPainter>

#include "Mirror.hpp"
#include "PlanView.hpp"
#include "command/CommandStack.hpp"

namespace cadino::ui {

void MirrorTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (!captured_) {
        initial_selection_ = view.selections();
        captured_ = true;
    }
    if (initial_selection_.empty()) return;

    if (!axis_p1_) {
        axis_p1_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    const bool copy = (QApplication::keyboardModifiers() & Qt::ShiftModifier) == 0;
    const Eigen::Vector2d p{axis_p1_->x(), axis_p1_->y()};
    const Eigen::Vector2d d{model_pos.x() - axis_p1_->x(),
                            model_pos.y() - axis_p1_->y()};
    if (d.squaredNorm() < 1e-9) return;

    mirror_selection(view.document(), view.command_stack(),
                     initial_selection_, p, d, copy);

    axis_p1_.reset();
    captured_ = false;
    initial_selection_.clear();
    view.notify_document_modified();
}

void MirrorTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (axis_p1_) view.update();
}

void MirrorTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void MirrorTool::on_cancel(PlanView& view) {
    axis_p1_.reset();
    captured_ = false;
    initial_selection_.clear();
    view.update();
}

void MirrorTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!axis_p1_) return;

    QPen pen(QColor(200, 100, 220), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(200, 100, 220));

    const QPointF a = view.model_to_screen(*axis_p1_);
    const QPointF b = view.model_to_screen(hover_);
    p.drawEllipse(a, 4, 4);
    p.drawLine(a, b);

    const bool copy = (QApplication::keyboardModifiers() & Qt::ShiftModifier) == 0;
    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    p.drawText(b + QPointF(8, -8),
               copy ? QStringLiteral("Mirror (copy)") : QStringLiteral("Mirror (move, Shift)"));
}

}  // namespace cadino::ui
