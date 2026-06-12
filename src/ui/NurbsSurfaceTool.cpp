#include "NurbsSurfaceTool.hpp"

#include <QPainter>
#include <cmath>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/NurbsSurfaceCommands.hpp"
#include "entity/NurbsSurface.hpp"

namespace cadino::ui {

void NurbsSurfaceTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;
    if (!corner1_) {
        corner1_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    const QPointF c1 = *corner1_;
    const QPointF c2 = model_pos;
    const double minx = std::min(c1.x(), c2.x());
    const double miny = std::min(c1.y(), c2.y());
    const double maxx = std::max(c1.x(), c2.x());
    const double maxy = std::max(c1.y(), c2.y());
    if (maxx - minx < 1.0 || maxy - miny < 1.0) {
        corner1_.reset();
        view.update();
        return;
    }

    cadino::core::NurbsSurface s;
    s.degree_u = 3;
    s.degree_v = 3;
    s.rows = 4;
    s.cols = 4;
    s.control_points.reserve(static_cast<std::size_t>(s.rows * s.cols));
    for (int r = 0; r < s.rows; ++r) {
        const double u = static_cast<double>(r) / static_cast<double>(s.rows - 1);
        const double x = minx + u * (maxx - minx);
        for (int c = 0; c < s.cols; ++c) {
            const double v = static_cast<double>(c) / static_cast<double>(s.cols - 1);
            const double y = miny + v * (maxy - miny);
            s.control_points.emplace_back(x, y, default_base_z_);
        }
    }

    view.command_stack().execute(
        std::make_unique<cadino::core::AddNurbsSurfaceCommand>(std::move(s)));

    corner1_.reset();
    view.notify_document_modified();
}

void NurbsSurfaceTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (corner1_) view.update();
}

void NurbsSurfaceTool::on_release(PlanView&, QPointF, Qt::MouseButton) {}

void NurbsSurfaceTool::on_cancel(PlanView& view) {
    corner1_.reset();
    view.update();
}

void NurbsSurfaceTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (!corner1_) return;
    QPen pen(QColor(220, 170, 60), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(220, 170, 60, 40));
    const QPointF a = view.model_to_screen(*corner1_);
    const QPointF b = view.model_to_screen(hover_);
    p.drawRect(QRectF(a, b).normalized());
}

}  // namespace cadino::ui
