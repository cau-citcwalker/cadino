#pragma once

#include <QPointF>

#include "Tool.hpp"
#include "entity/EntityId.hpp"

namespace cadino::ui {

// Two-click offset tool for walls. Activation sets the offset distance;
// the first click selects a wall under the cursor, the second click
// chooses which side of the wall the parallel copy should appear on.
class OffsetTool : public Tool {
public:
    explicit OffsetTool(double distance) : distance_{distance} {}

    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    double distance_{500.0};
    cadino::core::EntityId target_wall_{};
    QPointF hover_;
};

}  // namespace cadino::ui
