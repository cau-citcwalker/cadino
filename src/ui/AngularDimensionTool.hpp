#pragma once

#include <optional>

#include <QPointF>

#include "Tool.hpp"

namespace cadino::ui {

// Three-click angular dimension: vertex, point on first arm, point on
// second arm. The dimension arc sits at a default radius which the
// user can edit later in PropertiesPanel.
class AngularDimensionTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    std::optional<QPointF> vertex_;
    std::optional<QPointF> arm1_;
    QPointF hover_;
};

}  // namespace cadino::ui
