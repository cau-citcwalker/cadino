#pragma once

#include <memory>
#include <vector>

#include <Eigen/Core>
#include <QPointF>
#include <QTransform>
#include <QWidget>

#include "Selection.hpp"
#include "Snap.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

class Tool;

// Which world plane this 2D view represents.
//   Top    — draws the XY plane (u=X, v=Y). This is the classic plan view.
//   Front  — draws the XZ plane (u=X, v=Z, +v = up in world Z).
//   Right  — draws the YZ plane (u=Y, v=Z, +v = up in world Z).
// Elevation views (Front/Right) let users draw at Z != 0 so 3D content can
// be authored from any side.
enum class DrawPlane { Top, Front, Right };

class PlanView : public QWidget {
    Q_OBJECT

public:
    PlanView(cadino::core::Document& doc, cadino::core::CommandStack& stack,
             QWidget* parent = nullptr);
    ~PlanView() override;

    void set_plane(DrawPlane p) noexcept { plane_ = p; }
    [[nodiscard]] DrawPlane plane() const noexcept { return plane_; }

    // Map a plane-local (u, v) point to a world-space (X, Y, Z) coordinate.
    //   Top    (u, v) -> (u, v, 0)
    //   Front  (u, v) -> (u, 0, v)
    //   Right  (u, v) -> (0, u, v)
    [[nodiscard]] Eigen::Vector3d plane_to_world(QPointF uv) const noexcept;
    // Inverse: project a world point onto this plane's (u, v).
    [[nodiscard]] QPointF world_to_plane(Eigen::Vector3d w) const noexcept;

    void set_tool(std::unique_ptr<Tool> tool);
    [[nodiscard]] Tool* tool() noexcept { return tool_.get(); }

    [[nodiscard]] cadino::core::Document& document() noexcept { return document_; }
    [[nodiscard]] const cadino::core::Document& document() const noexcept { return document_; }
    [[nodiscard]] cadino::core::CommandStack& command_stack() noexcept { return stack_; }

    [[nodiscard]] QPointF screen_to_model(QPointF screen) const;
    [[nodiscard]] QPointF model_to_screen(QPointF model) const;
    [[nodiscard]] double zoom() const noexcept { return zoom_; }

    void notify_document_modified();

    [[nodiscard]] const std::vector<Selection>& selections() const noexcept {
        return selections_;
    }
    [[nodiscard]] Selection primary_selection() const noexcept {
        return selections_.empty() ? Selection{} : selections_.front();
    }
    void set_selections(std::vector<Selection> sel);
    void add_to_selection(Selection sel);
    void remove_from_selection(Selection sel);
    void toggle_selection(Selection sel);
    void clear_selection();
    [[nodiscard]] bool is_selected(Selection sel) const noexcept;

    [[nodiscard]] SnapEngine& snap_engine() noexcept { return snap_; }
    [[nodiscard]] const SnapResult& last_snap() const noexcept { return last_snap_; }

    // Ortho: when enabled (F8), tools should constrain their in-flight
    // direction to the nearest world axis. Shift can be checked by tools
    // for a temporary override in either direction.
    void set_ortho_enabled(bool on) noexcept { ortho_enabled_ = on; }
    [[nodiscard]] bool ortho_enabled() const noexcept { return ortho_enabled_; }

    [[nodiscard]] bool layer_visible(cadino::core::EntityId layer_id) const;
    [[nodiscard]] bool layer_locked(cadino::core::EntityId layer_id) const;

signals:
    void document_modified();
    void selection_changed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void update_transform();
    void draw_grid(QPainter& p);
    void draw_walls(QPainter& p);
    void draw_boxes(QPainter& p);
    void draw_cylinders(QPainter& p);
    void draw_slabs(QPainter& p);
    void draw_doors_windows(QPainter& p);
    void draw_curves(QPainter& p);
    void draw_blocks(QPainter& p);
    void draw_block_instances(QPainter& p);
    void draw_surfaces(QPainter& p);
    void draw_dimensions(QPainter& p);
    void draw_texts(QPainter& p);
    void draw_leaders(QPainter& p);
    void draw_angular_dims(QPainter& p);
    void draw_radial_dims(QPainter& p);
    void draw_snap_marker(QPainter& p);
    void draw_view_label(QPainter& p);
    QPointF apply_snap(QPointF model_pos);

    cadino::core::Document& document_;
    cadino::core::CommandStack& stack_;
    std::unique_ptr<Tool> tool_;

    double zoom_{0.1};
    QPointF view_offset_{0.0, 0.0};
    QTransform model_to_screen_;
    QTransform screen_to_model_;

    bool panning_{false};
    QPointF last_pan_screen_{};

    std::vector<Selection> selections_;

    SnapEngine snap_;
    SnapResult last_snap_{};
    bool ortho_enabled_{false};
    DrawPlane plane_{DrawPlane::Top};
};

}  // namespace cadino::ui
