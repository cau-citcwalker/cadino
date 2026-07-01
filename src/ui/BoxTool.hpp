#pragma once

#include <optional>

#include <QPointF>

#include "Tool.hpp"

namespace cadino::ui {

class BoxTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

    void set_default_height(double h) noexcept { default_height_ = h; }

private:
    std::optional<QPointF> corner1_;
    QPointF hover_;
    double default_height_{750.0};
    // When drawing in elevation views the click plane defines two of the
    // box's three dimensions; this fills in the axis pointing out of the
    // plane so the box has volume.
    double default_depth_{400.0};
};

}  // namespace cadino::ui
