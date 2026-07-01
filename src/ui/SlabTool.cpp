#include "SlabTool.hpp"

#include <cmath>

#include <QPainter>

#include "PlanView.hpp"
#include "command/CommandStack.hpp"
#include "command/DoorWindowSlabCommands.hpp"

namespace cadino::ui {

void SlabTool::on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) {
    if (button != Qt::LeftButton) return;

    if (view.plane() != DrawPlane::Top) {
        // Slabs are horizontal by definition — clicks in elevation don't map
        // to a sensible XY footprint. Bounce the click and hint at the user.
        view.update();
        return;
    }

    if (!corner1_) {
        corner1_ = model_pos;
        hover_ = model_pos;
        view.update();
        return;
    }

    const QPointF c1 = *corner1_;
    const QPointF c2 = model_pos;
    if (std::abs(c2.x() - c1.x()) < 1.0 || std::abs(c2.y() - c1.y()) < 1.0) {
        corner1_.reset();
        view.update();
        return;
    }

    cadino::core::Slab slab;
    slab.outline = {
        {std::min(c1.x(), c2.x()), std::min(c1.y(), c2.y())},
        {std::max(c1.x(), c2.x()), std::min(c1.y(), c2.y())},
        {std::max(c1.x(), c2.x()), std::max(c1.y(), c2.y())},
        {std::min(c1.x(), c2.x()), std::max(c1.y(), c2.y())},
    };
    slab.level = 0.0;
    slab.thickness = 200.0;

    view.command_stack().execute(
        std::make_unique<cadino::core::AddSlabCommand>(std::move(slab)));

    corner1_.reset();
    view.notify_document_modified();
}

void SlabTool::on_move(PlanView& view, QPointF model_pos) {
    hover_ = model_pos;
    if (corner1_) view.update();
}

void SlabTool::on_cancel(PlanView& view) {
    corner1_.reset();
    view.update();
}

void SlabTool::paint_overlay(QPainter& p, const PlanView& view) const {
    if (view.plane() != DrawPlane::Top) {
        // Elevation hint — slab is plan-only.
        QFont font = p.font();
        font.setBold(true);
        p.setFont(font);
        const QFontMetrics fm(font);
        const QString msg = QStringLiteral("Slab tool works in the plan (TOP) view only");
        const QSize ts = fm.size(0, msg);
        const int pad = 8;
        const QRectF bg(view.width() / 2.0 - ts.width() / 2.0 - pad,
                        view.height() - ts.height() - pad * 4,
                        ts.width() + pad * 2, ts.height() + pad);
        p.setBrush(QColor(140, 90, 70, 230));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(bg, 4, 4);
        p.setPen(Qt::white);
        p.drawText(bg, Qt::AlignCenter, msg);
        return;
    }
    if (!corner1_) return;
    QPen pen(QColor(140, 110, 80), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(QColor(140, 110, 80, 70));
    const QPointF a = view.model_to_screen(*corner1_);
    const QPointF b = view.model_to_screen(hover_);
    p.drawRect(QRectF(a, b).normalized());
}

}  // namespace cadino::ui
