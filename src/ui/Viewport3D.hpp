#pragma once

#include <memory>

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <QVector3D>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

class Viewport3D : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit Viewport3D(cadino::core::Document& doc, QWidget* parent = nullptr);
    ~Viewport3D() override;

    void refresh() { update(); }

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

    cadino::core::Document& document_;

    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer vbo_{QOpenGLBuffer::VertexBuffer};
    int vertex_count_{0};
    int walls_vertex_end_{6};
    bool mesh_dirty_{true};
    std::size_t last_wall_count_{0};

    QVector3D camera_target_{0.0f, 0.0f, 1200.0f};
    float camera_yaw_{-45.0f};
    float camera_pitch_{30.0f};
    float camera_distance_{8000.0f};

    Qt::MouseButton drag_button_{Qt::NoButton};
    QPointF drag_last_;
};

}  // namespace cadino::ui
