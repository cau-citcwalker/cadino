#pragma once

#include <QPointF>

#include "Tool.hpp"
#include "entity/EntityId.hpp"

namespace cadino::ui {

// Unified trim/extend tool for walls. First click picks a boundary
// (cutter) wall, second click picks the wall to modify and indicates
// which endpoint should snap to the intersection. If the intersection
// is inside the target wall the closer endpoint is pulled in (trim);
// if it's outside, the closer endpoint is pushed out (extend).
class TrimExtendTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    cadino::core::EntityId cutter_{};
    QPointF hover_;
};

}  // namespace cadino::ui
