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
    const auto a = view.model_to_screen(*start_);
    const auto b = view.model_to_screen(hover_);
    p.drawLine(a, b);

    p.setBrush(QColor(60, 130, 220));
    p.drawEllipse(a, 4, 4);

    const double length_mm = std::hypot(hover_.x() - start_->x(), hover_.y() - start_->y());
    const QString label = QString("L: %1 mm").arg(length_mm, 0, 'f', 1);

    QPen text_pen(Qt::white);
    p.setPen(text_pen);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QFontMetrics fm(font);
    const QSize text_size = fm.size(0, label);
    const QPointF mid((a.x() + b.x()) * 0.5, (a.y() + b.y()) * 0.5);
    const QRectF bg(mid.x() - text_size.width() / 2.0 - 4,
                    mid.y() - text_size.height() - 12,
                    text_size.width() + 8, text_size.height() + 4);
    p.setBrush(QColor(60, 130, 220, 220));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg, 3, 3);
    p.setPen(Qt::white);
    p.drawText(bg, Qt::AlignCenter, label);
}

}  // namespace cadino::ui
