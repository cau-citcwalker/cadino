#pragma once

#include <optional>

#include <QPointF>

#include "Tool.hpp"

namespace cadino::ui {

class CylinderTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

    void set_default_height(double h) noexcept { default_height_ = h; }

private:
    std::optional<QPointF> center_;
    QPointF hover_;
    double default_height_{750.0};
};

}  // namespace cadino::ui
