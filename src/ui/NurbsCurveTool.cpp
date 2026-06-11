#include "NurbsCurveTool.hpp"

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/NurbsCurveCommands.hpp"
#include "entity/NurbsCurve.hpp"

namespace cadino::ui {

void NurbsCurveTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button == Qt::RightButton) {
        finish(view);
        return;
    }
    if (button != Qt::LeftButton) return;

    control_points_.push_back(model_pos);
    hover_ = model_pos;
    view.update();
}

void NurbsCurveTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (!control_points_.empty()) view.update();
}

void NurbsCurveTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void NurbsCurveTool::on_cancel(PlanView& view) {
    control_points_.clear();
    view.update();
}

void NurbsCurveTool::finish(PlanView& view) {
    if (control_points_.size() < 2) {
        control_points_.clear();
        view.update();
        return;
    }
    cadino::core::NurbsCurve curve;
    curve.degree = degree_;
    curve.control_points.reserve(control_points_.size());
    for (const QPointF& p : control_points_) {
        curve.control_points.emplace_back(p.x(), p.y(), 0.0);
    }
    view.command_stack().execute(
        std::make_unique<cadino::core::AddNurbsCurveCommand>(std::move(curve)));
    control_points_.clear();
    view.notify_document_modified();
}

void NurbsCurveTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (control_points_.empty()) return;

    // Control polygon in dashed gray.
    QPen poly_pen(QColor(150, 150, 160), 1, Qt::DashLine);
    poly_pen.setCosmetic(true);
    p.setPen(poly_pen);
    p.setBrush(Qt::NoBrush);

    std::vector<QPointF> screen_points;
    screen_points.reserve(control_points_.size() + 1);
    for (const QPointF& m : control_points_) {
        screen_points.push_back(view.model_to_screen(m));
    }
    screen_points.push_back(view.model_to_screen(hover_));
    for (std::size_t i = 1; i < screen_points.size(); ++i) {
        p.drawLine(screen_points[i - 1], screen_points[i]);
    }

    // Live tessellated preview.
    if (control_points_.size() >= 1) {
        cadino::core::NurbsCurve preview;
        preview.degree = degree_;
        preview.control_points.reserve(control_points_.size() + 1);
        for (const QPointF& m : control_points_) {
            preview.control_points.emplace_back(m.x(), m.y(), 0.0);
        }
        preview.control_points.emplace_back(hover_.x(), hover_.y(), 0.0);

        if (preview.control_points.size() >= 2) {
            QPen curve_pen(QColor(80, 220, 240), 2);
            curve_pen.setCosmetic(true);
            p.setPen(curve_pen);
            const auto samples = preview.tessellate(96);
            for (std::size_t i = 1; i < samples.size(); ++i) {
                const QPointF a = view.model_to_screen(
                    QPointF(samples[i - 1].x(), samples[i - 1].y()));
                const QPointF b = view.model_to_screen(
                    QPointF(samples[i].x(), samples[i].y()));
                p.drawLine(a, b);
            }
        }
    }

    // Anchor markers.
    p.setBrush(QColor(80, 220, 240));
    p.setPen(Qt::NoPen);
    for (std::size_t i = 0; i < control_points_.size(); ++i) {
        p.drawEllipse(view.model_to_screen(control_points_[i]), 4, 4);
    }

    // Status label.
    const QString label = QString("NURBS deg %1 · %2 pts · right-click to finish")
                              .arg(degree_).arg(control_points_.size());
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QFontMetrics fm(font);
    const QSize text_size = fm.size(0, label);
    const QPointF anchor = view.model_to_screen(control_points_.front());
    const QRectF bg(anchor.x() + 14, anchor.y() - text_size.height() - 8,
                    text_size.width() + 8, text_size.height() + 4);
    p.setBrush(QColor(40, 70, 100, 220));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bg, 3, 3);
    p.setPen(Qt::white);
    p.drawText(bg, Qt::AlignCenter, label);
}

}  // namespace cadino::ui
