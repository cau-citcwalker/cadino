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
    const double du = std::abs(c2.x() - c1.x());
    const double dv = std::abs(c2.y() - c1.y());
    if (du < 1.0 || dv < 1.0) {
        corner1_.reset();
        view.update();
        return;
    }

    cadino::core::Box box;
    const double mid_u = (c1.x() + c2.x()) * 0.5;
    const double mid_v = (c1.y() + c2.y()) * 0.5;
    const double min_v = std::min(c1.y(), c2.y());
    switch (view.plane()) {
        case DrawPlane::Top:
            box.position = {mid_u, mid_v};
            box.size_xy  = {du, dv};
            box.height   = default_height_;
            box.base_z   = 0.0;
            break;
        case DrawPlane::Front:
            // (u, v) = (X, Z). Y=0 with a default thickness so the box has
            // volume; the width×depth footprint is (du, default_depth).
            box.position = {mid_u, 0.0};
            box.size_xy  = {du, default_depth_};
            box.height   = dv;
            box.base_z   = min_v;
            break;
        case DrawPlane::Right:
            // (u, v) = (Y, Z). X=0 with a default thickness.
            box.position = {0.0, mid_u};
            box.size_xy  = {default_depth_, du};
            box.height   = dv;
            box.base_z   = min_v;
            break;
    }

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
    const QRectF rect = QRectF(a, b).normalized();
    p.drawRect(rect);

    p.setBrush(QColor(220, 130, 60));
    p.drawEllipse(a, 4, 4);

    const double w_mm = std::abs(hover_.x() - corner1_->x());
    const double d_mm = std::abs(hover_.y() - corner1_->y());
    const QString label = QString("%1 × %2 mm")
                              .arg(w_mm, 0, 'f', 1).arg(d_mm, 0, 'f', 1);

    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QFontMetrics fm(font);
    const QSize text_size = fm.size(0, label);
    const QRectF bg(rect.center().x() - text_size.width() / 2.0 - 4,
                    rect.top() - text_size.height() - 12,
                    text_size.width() + 8, text_size.height() + 4);
    p.setBrush(QColor(220, 130, 60, 220));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg, 3, 3);
    p.setPen(Qt::white);
    p.drawText(bg, Qt::AlignCenter, label);
}

}  // namespace cadino::ui
