#pragma once

#include <optional>
#include <vector>

#include <QPointF>

#include "Selection.hpp"
#include "Tool.hpp"

namespace cadino::ui {

// Two-click mirror tool. Captures the current selection at activation,
// then the next two clicks define the mirror axis. Hold Shift on the
// second click to mirror the originals in place (move-mirror) instead of
// adding copies. Esc cancels.
class MirrorTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;

private:
    bool captured_{false};
    std::vector<Selection> initial_selection_;
    std::optional<QPointF> axis_p1_;
    QPointF hover_;
};

}  // namespace cadino::ui
