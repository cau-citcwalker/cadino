#pragma once

#include <optional>
#include <vector>

#include <QPointF>
#include <QString>

#include "Tool.hpp"

namespace cadino::ui {

// CAD-style continuous line tool.
//   Left-click        → drop a vertex (segment ends at cursor, next one starts here)
//   Right-click/Enter → finish and commit the polyline
//   Esc               → cancel without committing
//   Digits / '.'      → begin/continue direct distance entry; Enter commits at
//                       that distance in the current cursor direction
//   Backspace         → edit the pending distance
//   Ortho (PlanView.ortho_enabled() or Shift held) constrains direction to
//   the nearest world axis before committing.
class LineTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;
    void on_cancel(PlanView& view) override;
    bool on_key(PlanView& view, QKeyEvent* event) override;

private:
    void finish(PlanView& view);
    [[nodiscard]] QPointF apply_ortho(QPointF from, QPointF to, bool ortho_active) const;

    std::vector<QPointF> anchors_;  // committed vertices, size>=1 while in-flight
    QPointF hover_{};
    QString distance_input_;  // pending numeric direct distance entry
};

}  // namespace cadino::ui
