#pragma once

#include <optional>

#include <QPointF>

#include "Tool.hpp"

namespace cadino::ui {

// Three-click workflow:
//   1. first endpoint
//   2. second endpoint
//   3. offset / dimension-line position
// After the third click a Dimension entity is committed.
class DimensionTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    std::optional<QPointF> start_;
    std::optional<QPointF> end_;
    QPointF hover_;
};

}  // namespace cadino::ui
