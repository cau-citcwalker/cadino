#pragma once

#include <memory>
#include <unordered_map>
#include <variant>

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <QVector3D>

#include "Selection.hpp"
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

    cadino::core::Document& document_;
    cadino::core::CommandStack& stack_;
    PlanView& plan_view_;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject line_vao_;
    QOpenGLBuffer line_vbo_{QOpenGLBuffer::VertexBuffer};
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

    Qt::MouseButton drag_button_{Qt::NoButton};
    QPointF drag_last_;

    bool entity_dragging_{false};
    QVector3D drag_ground_start_{};
    using EntitySnapshot = std::variant<cadino::core::Wall, cadino::core::Box,
                                         cadino::core::Cylinder>;
    std::unordered_map<cadino::core::EntityId, EntitySnapshot> drag_originals_;
};

}  // namespace cadino::ui
