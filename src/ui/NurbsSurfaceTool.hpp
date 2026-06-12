#pragma once

#include <optional>

#include <QPointF>

#include "Tool.hpp"

namespace cadino::ui {

// Two-click tool: clicks two corners of an XY rectangle, the tool seeds a
// 4×4 control grid at base_z = 1500 mm so the resulting surface sits at
// roughly waist height. Control points can be edited afterwards.
class NurbsSurfaceTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

    void set_default_base_z(double z) noexcept { default_base_z_ = z; }

private:
    std::optional<QPointF> corner1_;
    QPointF hover_;
    double default_base_z_{1500.0};
};

}  // namespace cadino::ui
