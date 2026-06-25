#pragma once

#include <vector>

#include <QPointF>

#include "Selection.hpp"
#include "Tool.hpp"

namespace cadino::ui {

// One-click polar array placement: select entities, trigger the tool,
// then click in the plan view to fix the rotation centre. The count
// and sweep angle are configured at activation time.
class PolarArrayTool : public Tool {
public:
    PolarArrayTool(int count, double sweep_rad)
        : count_{count}, sweep_rad_{sweep_rad} {}

    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    int count_{6};
    double sweep_rad_{6.28318530718};
    bool captured_{false};
    std::vector<Selection> initial_selection_;
    QPointF hover_;
};

}  // namespace cadino::ui
