#include "Viewport3D.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <QMouseEvent>
#include <QWheelEvent>

#include <spdlog/spdlog.h>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

constexpr const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_view_proj;

out vec3 v_normal;

void main() {
    v_normal = a_normal;
    gl_Position = u_view_proj * vec4(a_pos, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core
in vec3 v_normal;
out vec4 frag_color;

uniform vec3 u_light_dir;
uniform vec3 u_base_color;

void main() {
    vec3 N = normalize(v_normal);
    if (!gl_FrontFacing) N = -N;
    float diff = max(dot(N, -normalize(u_light_dir)), 0.0);
    vec3 col = u_base_color * (0.25 + 0.75 * diff);
    frag_color = vec4(col, 1.0);
}
)";

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
};

void push_quad(std::vector<Vertex>& verts, QVector3D a, QVector3D b, QVector3D c,
               QVector3D d, QVector3D n) {
    auto v = [&](QVector3D p) {
        verts.push_back({p.x(), p.y(), p.z(), n.x(), n.y(), n.z()});
    };
    v(a); v(b); v(c);
    v(a); v(c); v(d);
}

void push_oriented_box(std::vector<Vertex>& verts, QVector3D center, float hx, float hy,
                       float zmin, float zmax, float yaw) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const QVector3D xp(c, s, 0.0f);
    const QVector3D yp(-s, c, 0.0f);
    const QVector3D zp(0.0f, 0.0f, 1.0f);

    const QVector3D b0 = center + (-hx) * xp + (-hy) * yp + QVector3D(0, 0, zmin);
    const QVector3D b1 = center + ( hx) * xp + (-hy) * yp + QVector3D(0, 0, zmin);
    const QVector3D b2 = center + ( hx) * xp + ( hy) * yp + QVector3D(0, 0, zmin);
    const QVector3D b3 = center + (-hx) * xp + ( hy) * yp + QVector3D(0, 0, zmin);
    const QVector3D t0(b0.x(), b0.y(), zmax);
    const QVector3D t1(b1.x(), b1.y(), zmax);
    const QVector3D t2(b2.x(), b2.y(), zmax);
    const QVector3D t3(b3.x(), b3.y(), zmax);

    push_quad(verts, t0, t1, t2, t3, zp);
    push_quad(verts, b3, b2, b1, b0, -zp);
    push_quad(verts, b1, b2, t2, t1, xp);
    push_quad(verts, b3, b0, t0, t3, -xp);
    push_quad(verts, b2, b3, t3, t2, yp);
    push_quad(verts, b0, b1, t1, t0, -yp);
}

void push_wall_box(std::vector<Vertex>& verts, const cadino::core::Wall& w) {
    const QVector3D start(static_cast<float>(w.start.x()),
                          static_cast<float>(w.start.y()), 0.0f);
    const QVector3D end(static_cast<float>(w.end.x()),
                        static_cast<float>(w.end.y()), 0.0f);
    const QVector3D dir = end - start;
    const float len = dir.length();
    if (len < 1e-5f) return;

    const QVector3D unit = dir / len;
    const QVector3D normal(-unit.y(), unit.x(), 0.0f);
    const QVector3D up(0.0f, 0.0f, 1.0f);
    const QVector3D off = normal * static_cast<float>(w.thickness * 0.5);
    const QVector3D h = up * static_cast<float>(w.height);

    const QVector3D b0 = start + off;
    const QVector3D b1 = end + off;
    const QVector3D b2 = end - off;
    const QVector3D b3 = start - off;
    const QVector3D t0 = b0 + h;
    const QVector3D t1 = b1 + h;
    const QVector3D t2 = b2 + h;
    const QVector3D t3 = b3 + h;

    push_quad(verts, t0, t1, t2, t3, up);
    push_quad(verts, b3, b2, b1, b0, -up);
    push_quad(verts, b0, b1, t1, t0, normal);
    push_quad(verts, b2, b3, t3, t2, -normal);
    push_quad(verts, b1, b2, t2, t1, unit);
    push_quad(verts, b3, b0, t0, t3, -unit);
}

void push_ground_grid(std::vector<Vertex>& verts, float size) {
    const QVector3D up(0.0f, 0.0f, 1.0f);
    const QVector3D a(-size, -size, 0.0f);
    const QVector3D b(size, -size, 0.0f);
    const QVector3D c(size, size, 0.0f);
    const QVector3D d(-size, size, 0.0f);
    push_quad(verts, a, b, c, d, up);
}

}  // namespace

Viewport3D::Viewport3D(cadino::core::Document& doc, QWidget* parent)
    : QOpenGLWidget(parent), document_(doc) {
    setFocusPolicy(Qt::StrongFocus);
}

Viewport3D::~Viewport3D() {
    makeCurrent();
    vbo_.destroy();
    vao_.destroy();
    program_.reset();
    doneCurrent();
}

void Viewport3D::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.12f, 0.13f, 0.16f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    program_ = std::make_unique<QOpenGLShaderProgram>();
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)) {
        spdlog::error("Vertex shader compile failed: {}", program_->log().toStdString());
    }
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)) {
        spdlog::error("Fragment shader compile failed: {}", program_->log().toStdString());
    }
    if (!program_->link()) {
        spdlog::error("Shader link failed: {}", program_->log().toStdString());
    }

    vao_.create();
    vbo_.create();
    vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
}

void Viewport3D::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void Viewport3D::rebuild_mesh() {
    std::vector<Vertex> verts;
    verts.reserve(36 * document_.walls().size() + 6);

    push_ground_grid(verts, 20000.0f);
    for (const auto& [id, w] : document_.walls()) {
        push_wall_box(verts, w);
    }
    const std::size_t walls_end = verts.size();
    for (const auto& [id, b] : document_.boxes()) {
        const QVector3D center(static_cast<float>(b.position.x()),
                               static_cast<float>(b.position.y()), 0.0f);
        push_oriented_box(verts, center,
                          static_cast<float>(b.size_xy.x() * 0.5),
                          static_cast<float>(b.size_xy.y() * 0.5),
                          static_cast<float>(b.base_z),
                          static_cast<float>(b.base_z + b.height),
                          static_cast<float>(b.rotation_z));
    }
    walls_vertex_end_ = static_cast<int>(walls_end);

    vao_.bind();
    vbo_.bind();
    vbo_.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(Vertex)));
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(Vertex));
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(1, GL_FLOAT, sizeof(float) * 3, 3, sizeof(Vertex));
    program_->enableAttributeArray(1);
    vbo_.release();
    vao_.release();

    vertex_count_ = static_cast<int>(verts.size());
    last_wall_count_ = document_.walls().size();
    mesh_dirty_ = false;
}

QMatrix4x4 Viewport3D::view_matrix() const {
    const float yaw_rad = camera_yaw_ * static_cast<float>(std::numbers::pi) / 180.0f;
    const float pitch_rad = camera_pitch_ * static_cast<float>(std::numbers::pi) / 180.0f;
    const float cp = std::cos(pitch_rad);
    const QVector3D offset(camera_distance_ * std::cos(yaw_rad) * cp,
                           camera_distance_ * std::sin(yaw_rad) * cp,
                           camera_distance_ * std::sin(pitch_rad));
    const QVector3D eye = camera_target_ + offset;
    QMatrix4x4 v;
    v.lookAt(eye, camera_target_, QVector3D(0.0f, 0.0f, 1.0f));
    return v;
}

QMatrix4x4 Viewport3D::projection_matrix() const {
    QMatrix4x4 p;
    const float aspect = height() > 0 ? static_cast<float>(width()) / height() : 1.0f;
    p.perspective(50.0f, aspect, 100.0f, 200000.0f);
    return p;
}

void Viewport3D::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!program_ || !program_->isLinked()) return;

    program_->bind();
    rebuild_mesh();
    vao_.bind();

    const QMatrix4x4 vp = projection_matrix() * view_matrix();
    program_->setUniformValue("u_view_proj", vp);
    program_->setUniformValue("u_light_dir", QVector3D(-0.4f, -0.3f, -1.0f));

    program_->setUniformValue("u_base_color", QVector3D(0.18f, 0.20f, 0.24f));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (walls_vertex_end_ > 6) {
        program_->setUniformValue("u_base_color", QVector3D(0.78f, 0.78f, 0.80f));
        glDrawArrays(GL_TRIANGLES, 6, walls_vertex_end_ - 6);
    }
    if (vertex_count_ > walls_vertex_end_) {
        program_->setUniformValue("u_base_color", QVector3D(0.78f, 0.62f, 0.40f));
        glDrawArrays(GL_TRIANGLES, walls_vertex_end_, vertex_count_ - walls_vertex_end_);
    }

    vao_.release();
    program_->release();
}

void Viewport3D::mousePressEvent(QMouseEvent* event) {
    drag_button_ = event->button();
    drag_last_ = event->position();
    setCursor(Qt::ClosedHandCursor);
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    if (drag_button_ == Qt::NoButton) return;

    const QPointF delta = event->position() - drag_last_;
    drag_last_ = event->position();

    if (drag_button_ == Qt::LeftButton) {
        camera_yaw_ -= static_cast<float>(delta.x()) * 0.4f;
        camera_pitch_ = std::clamp(camera_pitch_ + static_cast<float>(delta.y()) * 0.4f,
                                    -85.0f, 85.0f);
    } else if (drag_button_ == Qt::MiddleButton || drag_button_ == Qt::RightButton) {
        const float yaw_rad = camera_yaw_ * static_cast<float>(std::numbers::pi) / 180.0f;
        const QVector3D right(-std::sin(yaw_rad), std::cos(yaw_rad), 0.0f);
        const QVector3D forward(std::cos(yaw_rad), std::sin(yaw_rad), 0.0f);
        const float speed = camera_distance_ * 0.0015f;
        camera_target_ -= right * static_cast<float>(delta.x()) * speed;
        camera_target_ -= forward * static_cast<float>(delta.y()) * speed;
    }
    update();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent*) {
    drag_button_ = Qt::NoButton;
    unsetCursor();
}

void Viewport3D::wheelEvent(QWheelEvent* event) {
    const float factor = std::pow(0.9985f, static_cast<float>(event->angleDelta().y()));
    camera_distance_ = std::clamp(camera_distance_ * factor, 200.0f, 100000.0f);
    update();
}

}  // namespace cadino::ui
