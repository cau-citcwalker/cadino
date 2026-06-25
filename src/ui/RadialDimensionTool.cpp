#include "RadialDimensionTool.hpp"

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/RadialDimensionCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

void RadialDimensionTool::on_press(PlanView& view, QPointF model_pos,
                                   Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    auto& doc = view.document();
    if (!has_target_) {
        // Pick a cylinder under the cursor.
        for (const auto& [id, c] : doc.cylinders()) {
            const double d = std::hypot(model_pos.x() - c.position.x(),
                                        model_pos.y() - c.position.y());
            if (d <= c.radius + 50.0) {
                target_center_ = {c.position.x(), c.position.y()};
                target_radius_ = c.radius;
                has_target_ = true;
                hover_ = model_pos;
                view.update();
                return;
            }
        }
        return;
    }
    cadino::core::RadialDimension r;
    r.center = {target_center_.x(), target_center_.y()};
    r.radius = target_radius_;
    r.label_position = {model_pos.x(), model_pos.y()};
    r.is_diameter = diameter_;
    view.command_stack().execute(
        std::make_unique<cadino::core::AddRadialDimensionCommand>(std::move(r)));
    has_target_ = false;
    view.notify_document_modified();
}

void RadialDimensionTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (has_target_) view.update();
}
void RadialDimensionTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}
void RadialDimensionTool::on_cancel(PlanView& view) {
    has_target_ = false;
    view.update();
}

void RadialDimensionTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!has_target_) return;

    QPen pen(QColor(80, 200, 220), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);

    const QPointF c = view.model_to_screen(target_center_);
    const QPointF lp = view.model_to_screen(hover_);
    p.drawLine(c, lp);

    p.setPen(Qt::white);
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QString label = diameter_
        ? QString("Ø %1 mm").arg(target_radius_ * 2.0, 0, 'f', 1)
        : QString("R %1 mm").arg(target_radius_, 0, 'f', 1);
    p.drawText(lp + QPointF(8, -8), label);
}

}  // namespace cadino::ui
