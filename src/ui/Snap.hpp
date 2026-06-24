#pragma once

#include <QPointF>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

enum class SnapKind {
    None,
    Grid,
    Endpoint,
    Midpoint,
    Corner,
    Center,
    Intersection,
    Perpendicular,
};

struct SnapResult {
    QPointF position{};
    SnapKind kind{SnapKind::None};

    [[nodiscard]] bool found() const noexcept { return kind != SnapKind::None; }
};

class SnapEngine {
public:
    void set_grid_step(double mm) noexcept { grid_step_ = mm; }
    [[nodiscard]] double grid_step() const noexcept { return grid_step_; }

    void set_grid_enabled(bool on) noexcept { grid_enabled_ = on; }
    void set_endpoint_enabled(bool on) noexcept { endpoint_enabled_ = on; }
    void set_midpoint_enabled(bool on) noexcept { midpoint_enabled_ = on; }
    void set_corner_enabled(bool on) noexcept { corner_enabled_ = on; }
    void set_center_enabled(bool on) noexcept { center_enabled_ = on; }
    void set_intersection_enabled(bool on) noexcept { intersection_enabled_ = on; }
    void set_perpendicular_enabled(bool on) noexcept { perpendicular_enabled_ = on; }

    [[nodiscard]] bool grid_enabled() const noexcept { return grid_enabled_; }
    [[nodiscard]] bool endpoint_enabled() const noexcept { return endpoint_enabled_; }
    [[nodiscard]] bool midpoint_enabled() const noexcept { return midpoint_enabled_; }
    [[nodiscard]] bool corner_enabled() const noexcept { return corner_enabled_; }
    [[nodiscard]] bool center_enabled() const noexcept { return center_enabled_; }
    [[nodiscard]] bool intersection_enabled() const noexcept { return intersection_enabled_; }
    [[nodiscard]] bool perpendicular_enabled() const noexcept { return perpendicular_enabled_; }

    [[nodiscard]] SnapResult snap(QPointF model_pos,
                                  const cadino::core::Document& doc,
                                  double tolerance_model_units) const;

private:
    double grid_step_{100.0};
    bool grid_enabled_{true};
    bool endpoint_enabled_{true};
    bool midpoint_enabled_{true};
    bool corner_enabled_{true};
    bool center_enabled_{true};
    bool intersection_enabled_{true};
    bool perpendicular_enabled_{true};
};

}  // namespace cadino::ui
