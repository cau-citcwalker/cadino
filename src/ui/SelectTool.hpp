#pragma once

#include <optional>
#include <unordered_map>
#include <variant>

#include <QPointF>

#include "Selection.hpp"
#include "Tool.hpp"
#include "entity/Box.hpp"
#include "entity/Cylinder.hpp"
#include "entity/EntityId.hpp"
#include "entity/Wall.hpp"

namespace cadino::ui {

class SelectTool : public Tool {
public:
    void on_press(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void on_move(PlanView& view, QPointF model_pos) override;
    void on_release(PlanView& view, QPointF model_pos, Qt::MouseButton button) override;
    void paint_overlay(QPainter& p, const PlanView& view) const override;

private:
    using EntitySnapshot = std::variant<cadino::core::Wall, cadino::core::Box,
                                         cadino::core::Cylinder>;
    std::unordered_map<cadino::core::EntityId, EntitySnapshot> drag_originals_;

    bool dragging_{false};
    QPointF drag_start_{};

    bool rubber_banding_{false};
    QPointF rubber_start_{};
    QPointF rubber_current_{};
};

}  // namespace cadino::ui
