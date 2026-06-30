#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <QString>
#include <QVector3D>

#include "Selection.hpp"
#include "entity/Block.hpp"
#include "entity/BlockInstance.hpp"
#include "entity/Box.hpp"
#include "entity/Cylinder.hpp"
#include "entity/EntityId.hpp"
#include "entity/Wall.hpp"

namespace cadino::core {
class Document;
class CommandStack;
}  // namespace cadino::core

namespace cadino::ui {

class PlanView;

class Viewport3D : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    enum class CameraPreset { Iso, Top, Front, Back, Left, Right };

    Viewport3D(cadino::core::Document& doc, cadino::core::CommandStack& stack,
               PlanView& plan, QWidget* parent = nullptr);
    ~Viewport3D() override;

    void refresh() { update(); }
    void set_preset(CameraPreset preset);
    [[nodiscard]] CameraPreset preset() const noexcept { return preset_; }

    void set_section(bool enabled, QVector3D normal, QVector3D point) {
        section_enabled_ = enabled;
        section_normal_ = normal;
        section_point_ = point;
        update();
    }
    [[nodiscard]] bool section_enabled() const noexcept { return section_enabled_; }

    void set_sun(float azimuth_deg, float altitude_deg) {
        sun_azimuth_deg_ = azimuth_deg;
        sun_altitude_deg_ = altitude_deg;
        update();
    }
    [[nodiscard]] float sun_azimuth() const noexcept { return sun_azimuth_deg_; }
    [[nodiscard]] float sun_altitude() const noexcept { return sun_altitude_deg_; }

    // Project a world-space point onto current screen pixels. Used by tests
    // to locate gizmo handles for QTest mouse synthesis.
    [[nodiscard]] QPointF world_to_screen(QVector3D world) const;
    [[nodiscard]] QVector3D selection_centroid_for_test() const {
        QVector3D out;
        selection_centroid(out);
        return out;
    }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void rebuild_mesh();
    [[nodiscard]] QMatrix4x4 view_matrix() const;
    [[nodiscard]] QMatrix4x4 projection_matrix() const;
    [[nodiscard]] QVector3D eye_position() const;

    struct Ray { QVector3D origin; QVector3D direction; };
    [[nodiscard]] Ray ray_from_screen(QPointF screen_pos) const;
    [[nodiscard]] bool ray_ground_intersection(const Ray& ray, QVector3D& point_out) const;
    [[nodiscard]] Selection pick_at_screen(QPointF screen_pos, float* t_out = nullptr) const;
    void emit_drag_commands();

    enum class GizmoAxis {
        None,
        X, Y, Z,          // translate
        RotX, RotY, RotZ, // rotate
        ScaleX, ScaleY, ScaleZ, ScaleUniform,
    };
    [[nodiscard]] bool selection_centroid(QVector3D& out) const;
    [[nodiscard]] float gizmo_length() const;
    [[nodiscard]] GizmoAxis pick_gizmo_axis(QPointF screen_pos) const;
    [[nodiscard]] QVector3D axis_direction(GizmoAxis a) const;
    [[nodiscard]] bool axis_param(const Ray& ray, const QVector3D& pivot,
                                  const QVector3D& axis, float& s_out) const;
    [[nodiscard]] bool ray_plane_intersection(const Ray& ray, const QVector3D& pivot,
                                              const QVector3D& normal,
                                              QVector3D& hit_out) const;
    [[nodiscard]] double rotation_angle_for(GizmoAxis axis, QPointF screen_pos,
                                            bool* ok = nullptr) const;
    void render_gizmo();
    void render_gnomon();
    void capture_drag_originals();
    void apply_drag_delta(const QVector3D& delta);
    void apply_drag_rotation(GizmoAxis axis, double angle);
    void apply_drag_scale_uniform(double factor);
    void apply_drag_scale_axis(GizmoAxis axis, double factor);

    cadino::core::Document& document_;
    cadino::core::CommandStack& stack_;
    PlanView& plan_view_;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject line_vao_;
    QOpenGLBuffer line_vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject gizmo_vao_;
    QOpenGLBuffer gizmo_vbo_{QOpenGLBuffer::VertexBuffer};
    struct DrawGroup {
        int offset{0};
        int count{0};
        QString texture_path;
    };
    std::vector<DrawGroup> mesh_groups_;
    std::unordered_map<std::string, std::unique_ptr<QOpenGLTexture>> texture_cache_;
    int vertex_count_{0};
    int line_vertex_count_{0};
    int walls_vertex_end_{6};
    bool mesh_dirty_{true};
    std::size_t last_wall_count_{0};
    std::size_t last_curve_count_{0};

    QVector3D camera_target_{0.0f, 0.0f, 1200.0f};
    float camera_yaw_{-45.0f};
    float camera_pitch_{30.0f};
    float camera_distance_{8000.0f};
    CameraPreset preset_{CameraPreset::Iso};

    bool section_enabled_{false};
    QVector3D section_normal_{0.0f, 0.0f, 1.0f};
    QVector3D section_point_{0.0f, 0.0f, 1200.0f};

    float sun_azimuth_deg_{45.0f};
    float sun_altitude_deg_{55.0f};

    Qt::MouseButton drag_button_{Qt::NoButton};
    QPointF drag_last_;
    QPointF press_pos_{};
    bool pending_clear_on_release_{false};

    bool entity_dragging_{false};
    QVector3D drag_ground_start_{};
    GizmoAxis active_axis_{GizmoAxis::None};
    GizmoAxis hovered_axis_{GizmoAxis::None};
    QVector3D gizmo_pivot_{};
    QVector3D gizmo_axis_dir_{};
    float drag_axis_s0_{0.0f};
    double drag_rot_angle0_{0.0};
    double drag_scale_dist0_{0.0};
    using EntitySnapshot = std::variant<cadino::core::Wall, cadino::core::Box,
                                         cadino::core::Cylinder,
                                         cadino::core::Block,
                                         cadino::core::BlockInstance>;
    std::unordered_map<cadino::core::EntityId, EntitySnapshot> drag_originals_;
};

}  // namespace cadino::ui
