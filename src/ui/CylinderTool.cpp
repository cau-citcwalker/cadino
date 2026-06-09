#include "CylinderTool.hpp"

#include <cmath>

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"

namespace cadino::ui {

void CylinderTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (!center_) {
        center_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    const double radius = std::hypot(model_pos.x() - center_->x(),
                                     model_pos.y() - center_->y());
    if (radius < 1.0) {
        center_.reset();
        view.update();
        return;
    }

    cadino::core::Cylinder cyl;
    cyl.position = {center_->x(), center_->y()};
    cyl.radius = radius;
    cyl.height = default_height_;
    cyl.base_z = 0.0;

    view.command_stack().execute(
        std::make_unique<cadino::core::AddCylinderCommand>(std::move(cyl)));

    center_.reset();
    view.notify_document_modified();
}

void CylinderTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (center_) view.update();
}

void CylinderTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void CylinderTool::on_cancel(PlanView& view) {
    center_.reset();
    view.update();
}

void CylinderTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!center_) return;

    QPen pen(QColor(80, 160, 200), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(80, 160, 200, 40));

    const QPointF c_s = view.model_to_screen(*center_);
    const double r_m = std::hypot(hover_.x() - center_->x(), hover_.y() - center_->y());
    const double r_s = r_m * view.zoom();
    p.drawEllipse(c_s, r_s, r_s);

    p.setBrush(QColor(80, 160, 200));
    p.drawEllipse(c_s, 4, 4);

    const QString label = QString("R: %1 mm  (⌀ %2 mm)")
                              .arg(r_m, 0, 'f', 1).arg(r_m * 2.0, 0, 'f', 1);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QFontMetrics fm(font);
    const QSize text_size = fm.size(0, label);
    const QRectF bg(c_s.x() - text_size.width() / 2.0 - 4,
                    c_s.y() - r_s - text_size.height() - 12,
                    text_size.width() + 8, text_size.height() + 4);
    p.setBrush(QColor(80, 160, 200, 220));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg, 3, 3);
    p.setPen(Qt::white);
    p.drawText(bg, Qt::AlignCenter, label);
}

}  // namespace cadino::ui
