#pragma once

#include <QPointF>

#include "Tool.hpp"
#include "entity/EntityId.hpp"

namespace cadino::ui {

// Two-click radial dimension. First click picks a cylinder under the
// cursor (center + radius captured); second click drops the label.
// Shift on the second click toggles between radius (R) and diameter
// (Ø) display.
class RadialDimensionTool : public Tool {
public:
    explicit RadialDimensionTool(bool diameter) : diameter_{diameter} {}

    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    bool diameter_{false};
    bool has_target_{false};
    QPointF target_center_{};
    double target_radius_{0.0};
    QPointF hover_;
};

}  // namespace cadino::ui
