#include "Viewport3D.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>

#include <spdlog/spdlog.h>

#include "PlanView.hpp"
#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "command/CylinderCommands.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

namespace cadino::ui {

namespace {

constexpr const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;

uniform mat4 u_view_proj;
uniform mat4 u_model;

out vec3 v_normal;
out vec3 v_color;

void main() {
    vec4 wp = u_model * vec4(a_pos, 1.0);
    v_normal = a_normal;
    v_color = a_color;
    gl_Position = u_view_proj * wp;
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core
in vec3 v_normal;
in vec3 v_color;
out vec4 frag_color;

uniform vec3 u_light_dir;
uniform int u_shadow_mode;

void main() {
    if (u_shadow_mode == 1) {
        frag_color = vec4(0.02, 0.02, 0.04, 0.35);
        return;
    }
    vec3 N = normalize(v_normal);
    if (!gl_FrontFacing) N = -N;
    float diff = max(dot(N, -normalize(u_light_dir)), 0.0);
    vec3 col = v_color * (0.25 + 0.75 * diff);
    frag_color = vec4(col, 1.0);
}
)";

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float cr, cg, cb;
};

void push_quad(std::vector<Vertex>& verts, QVector3D a, QVector3D b, QVector3D c,
               QVector3D d, QVector3D n, QVector3D color) {
    auto v = [&](QVector3D p) {
        verts.push_back({p.x(), p.y(), p.z(), n.x(), n.y(), n.z(),
                          color.x(), color.y(), color.z()});
    };
    v(a); v(b); v(c);
    v(a); v(c); v(d);
}

void push_cylinder(std::vector<Vertex>& verts, QVector3D center, float radius,
                   float zmin, float zmax, QVector3D color, int segments = 32) {
    const float pi = std::numbers::pi_v<float>;
    const auto cv = [&](QVector3D p, QVector3D n) {
        verts.push_back({p.x(), p.y(), p.z(), n.x(), n.y(), n.z(),
                         color.x(), color.y(), color.z()});
    };

    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * pi * i / segments;
        const float a1 = 2.0f * pi * (i + 1) / segments;
        const float c0 = std::cos(a0), s0 = std::sin(a0);
        const float c1 = std::cos(a1), s1 = std::sin(a1);

        const QVector3D b0(center.x() + radius * c0, center.y() + radius * s0, zmin);
        const QVector3D b1(center.x() + radius * c1, center.y() + radius * s1, zmin);
        const QVector3D t0(center.x() + radius * c0, center.y() + radius * s0, zmax);
        const QVector3D t1(center.x() + radius * c1, center.y() + radius * s1, zmax);
        const QVector3D n0(c0, s0, 0.0f);
        const QVector3D n1(c1, s1, 0.0f);
        cv(b0, n0); cv(b1, n1); cv(t1, n1);
        cv(b0, n0); cv(t1, n1); cv(t0, n0);
    }

    const QVector3D top_c(center.x(), center.y(), zmax);
    const QVector3D bot_c(center.x(), center.y(), zmin);
    const QVector3D zp(0.0f, 0.0f, 1.0f);
    const QVector3D zn(0.0f, 0.0f, -1.0f);
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * pi * i / segments;
        const float a1 = 2.0f * pi * (i + 1) / segments;
        const QVector3D pt0(center.x() + radius * std::cos(a0),
                            center.y() + radius * std::sin(a0), zmax);
        const QVector3D pt1(center.x() + radius * std::cos(a1),
                            center.y() + radius * std::sin(a1), zmax);
        cv(top_c, zp); cv(pt0, zp); cv(pt1, zp);

        const QVector3D pb0(center.x() + radius * std::cos(a0),
                            center.y() + radius * std::sin(a0), zmin);
        const QVector3D pb1(center.x() + radius * std::cos(a1),
                            center.y() + radius * std::sin(a1), zmin);
        cv(bot_c, zn); cv(pb1, zn); cv(pb0, zn);
    }
}

void push_oriented_box(std::vector<Vertex>& verts, QVector3D center, float hx, float hy,
                       float zmin, float zmax, float yaw, QVector3D color) {
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

    push_quad(verts, t0, t1, t2, t3, zp, color);
    push_quad(verts, b3, b2, b1, b0, -zp, color);
    push_quad(verts, b1, b2, t2, t1, xp, color);
    push_quad(verts, b3, b0, t0, t3, -xp, color);
    push_quad(verts, b2, b3, t3, t2, yp, color);
    push_quad(verts, b0, b1, t1, t0, -yp, color);
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

    const QVector3D color(w.color.r, w.color.g, w.color.b);
    push_quad(verts, t0, t1, t2, t3, up, color);
    push_quad(verts, b3, b2, b1, b0, -up, color);
    push_quad(verts, b0, b1, t1, t0, normal, color);
    push_quad(verts, b2, b3, t3, t2, -normal, color);
    push_quad(verts, b1, b2, t2, t1, unit, color);
    push_quad(verts, b3, b0, t0, t3, -unit, color);
}

void push_ground_grid(std::vector<Vertex>& verts, float size) {
    const QVector3D up(0.0f, 0.0f, 1.0f);
    const QVector3D a(-size, -size, 0.0f);
    const QVector3D b(size, -size, 0.0f);
    const QVector3D c(size, size, 0.0f);
    const QVector3D d(-size, size, 0.0f);
    const QVector3D ground_color(0.18f, 0.20f, 0.24f);
    push_quad(verts, a, b, c, d, up, ground_color);
}

}  // namespace

Viewport3D::Viewport3D(cadino::core::Document& doc, cadino::core::CommandStack& stack,
                       PlanView& plan, QWidget* parent)
    : QOpenGLWidget(parent), document_(doc), stack_(stack), plan_view_(plan) {
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
                          static_cast<float>(b.rotation_z),
                          QVector3D(b.color.r, b.color.g, b.color.b));
    }
    for (const auto& [id, c] : document_.cylinders()) {
        const QVector3D center(static_cast<float>(c.position.x()),
                               static_cast<float>(c.position.y()), 0.0f);
        push_cylinder(verts, center, static_cast<float>(c.radius),
                      static_cast<float>(c.base_z),
                      static_cast<float>(c.base_z + c.height),
                      QVector3D(c.color.r, c.color.g, c.color.b));
    }
    for (const auto& [id, s] : document_.slabs()) {
        if (s.outline.size() < 3) continue;
        double minx = s.outline[0].x(), miny = s.outline[0].y();
        double maxx = minx, maxy = miny;
        for (const auto& v : s.outline) {
            minx = std::min(minx, v.x()); miny = std::min(miny, v.y());
            maxx = std::max(maxx, v.x()); maxy = std::max(maxy, v.y());
        }
        const QVector3D center((minx + maxx) * 0.5f, (miny + maxy) * 0.5f, 0.0f);
        push_oriented_box(verts, center,
                          static_cast<float>((maxx - minx) * 0.5),
                          static_cast<float>((maxy - miny) * 0.5),
                          static_cast<float>(s.level),
                          static_cast<float>(s.level + s.thickness),
                          0.0f,
                          QVector3D(0.72f, 0.65f, 0.55f));
    }
    for (const auto& [id, d] : document_.doors()) {
        const auto* w = document_.find_wall(d.host_wall);
        if (!w) continue;
        const QVector3D a(static_cast<float>(w->start.x()),
                          static_cast<float>(w->start.y()), 0.0f);
        const QVector3D b(static_cast<float>(w->end.x()),
                          static_cast<float>(w->end.y()), 0.0f);
        const QVector3D dir = b - a;
        const float len = dir.length();
        if (len < 1e-5f) continue;
        const QVector3D unit = dir / len;
        const float yaw = std::atan2(unit.y(), unit.x());
        const QVector3D center = a + unit * static_cast<float>(d.position_along);
        push_oriented_box(verts, center,
                          static_cast<float>(d.width * 0.5),
                          static_cast<float>(w->thickness * 0.5),
                          static_cast<float>(d.sill_height),
                          static_cast<float>(d.sill_height + d.height),
                          yaw,
                          QVector3D(0.55f, 0.35f, 0.20f));
    }
    for (const auto& [id, win] : document_.windows()) {
        const auto* w = document_.find_wall(win.host_wall);
        if (!w) continue;
        const QVector3D a(static_cast<float>(w->start.x()),
                          static_cast<float>(w->start.y()), 0.0f);
        const QVector3D b(static_cast<float>(w->end.x()),
                          static_cast<float>(w->end.y()), 0.0f);
        const QVector3D dir = b - a;
        const float len = dir.length();
        if (len < 1e-5f) continue;
        const QVector3D unit = dir / len;
        const float yaw = std::atan2(unit.y(), unit.x());
        const QVector3D center = a + unit * static_cast<float>(win.position_along);
        push_oriented_box(verts, center,
                          static_cast<float>(win.width * 0.5),
                          static_cast<float>(w->thickness * 0.5 + 1.0),
                          static_cast<float>(win.sill_height),
                          static_cast<float>(win.sill_height + win.height),
                          yaw,
                          QVector3D(0.65f, 0.80f, 0.90f));
    }
    walls_vertex_end_ = static_cast<int>(walls_end);

    vao_.bind();
    vbo_.bind();
    vbo_.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(Vertex)));
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(Vertex));
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(1, GL_FLOAT, sizeof(float) * 3, 3, sizeof(Vertex));
    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(2, GL_FLOAT, sizeof(float) * 6, 3, sizeof(Vertex));
    program_->enableAttributeArray(2);
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
    if (preset_ == CameraPreset::Iso) {
        p.perspective(50.0f, aspect, 100.0f, 200000.0f);
    } else {
        const float h = camera_distance_ * 0.5f;
        p.ortho(-h * aspect, h * aspect, -h, h, -200000.0f, 200000.0f);
    }
    return p;
}

void Viewport3D::set_preset(CameraPreset preset) {
    preset_ = preset;
    switch (preset) {
        case CameraPreset::Iso:
            camera_yaw_ = -45.0f; camera_pitch_ = 30.0f; break;
        case CameraPreset::Top:
            camera_yaw_ = 0.0f; camera_pitch_ = 89.9f; break;
        case CameraPreset::Front:
            camera_yaw_ = -90.0f; camera_pitch_ = 0.0f; break;
        case CameraPreset::Back:
            camera_yaw_ = 90.0f; camera_pitch_ = 0.0f; break;
        case CameraPreset::Left:
            camera_yaw_ = 180.0f; camera_pitch_ = 0.0f; break;
        case CameraPreset::Right:
            camera_yaw_ = 0.0f; camera_pitch_ = 0.0f; break;
    }
    update();
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

    QMatrix4x4 identity;
    program_->setUniformValue("u_model", identity);
    program_->setUniformValue("u_shadow_mode", 0);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count_);

    if (vertex_count_ > 6) {
        const QVector3D L(-0.4f, -0.3f, -1.0f);
        const float lz = L.z();
        QMatrix4x4 shadow(
            1.0f, 0.0f, -L.x() / lz, 0.0f,
            0.0f, 1.0f, -L.y() / lz, 0.0f,
            0.0f, 0.0f,  0.0f,       1.0f,
            0.0f, 0.0f,  0.0f,       1.0f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glPolygonOffset(-1.0f, -1.0f);
        glEnable(GL_POLYGON_OFFSET_FILL);

        program_->setUniformValue("u_model", shadow);
        program_->setUniformValue("u_shadow_mode", 1);
        glDrawArrays(GL_TRIANGLES, 6, vertex_count_ - 6);

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    vao_.release();
    program_->release();
}

Viewport3D::Ray Viewport3D::ray_from_screen(QPointF screen_pos) const {
    const QMatrix4x4 vp = projection_matrix() * view_matrix();
    bool ok = false;
    const QMatrix4x4 inv = vp.inverted(&ok);
    Ray ray;
    if (!ok) return ray;

    const float ndc_x = 2.0f * float(screen_pos.x()) / float(std::max(1, width())) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * float(screen_pos.y()) / float(std::max(1, height()));

    QVector4D near_clip(ndc_x, ndc_y, -1.0f, 1.0f);
    QVector4D far_clip(ndc_x, ndc_y, 1.0f, 1.0f);
    QVector4D near_world = inv * near_clip;
    QVector4D far_world = inv * far_clip;
    if (std::abs(near_world.w()) > 1e-9f) near_world /= near_world.w();
    if (std::abs(far_world.w()) > 1e-9f) far_world /= far_world.w();

    ray.origin = near_world.toVector3D();
    ray.direction = (far_world.toVector3D() - ray.origin).normalized();
    return ray;
}

bool Viewport3D::ray_ground_intersection(const Ray& ray, QVector3D& point_out) const {
    if (std::abs(ray.direction.z()) < 1e-6f) return false;
    const float t = -ray.origin.z() / ray.direction.z();
    if (t < 0) return false;
    point_out = ray.origin + t * ray.direction;
    return true;
}

namespace {

bool ray_vs_obox(QVector3D ro, QVector3D rd, QVector3D center, float hx, float hy,
                 float zmin, float zmax, float yaw, float& t_out) {
    const float c = std::cos(-yaw);
    const float s = std::sin(-yaw);
    const QVector3D lo(c * (ro.x() - center.x()) - s * (ro.y() - center.y()),
                       s * (ro.x() - center.x()) + c * (ro.y() - center.y()),
                       ro.z() - center.z());
    const QVector3D ld(c * rd.x() - s * rd.y(),
                       s * rd.x() + c * rd.y(),
                       rd.z());

    const float mins[3] = {-hx, -hy, zmin};
    const float maxs[3] = { hx,  hy, zmax};
    const float orig[3] = {lo.x(), lo.y(), lo.z()};
    const float dir[3] = {ld.x(), ld.y(), ld.z()};

    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-9f) {
            if (orig[i] < mins[i] || orig[i] > maxs[i]) return false;
        } else {
            float t1 = (mins[i] - orig[i]) / dir[i];
            float t2 = (maxs[i] - orig[i]) / dir[i];
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    if (tmax < 0) return false;
    t_out = tmin >= 0 ? tmin : tmax;
    return true;
}

bool ray_vs_cyl(QVector3D ro, QVector3D rd, QVector3D center, float r,
                float zmin, float zmax, float& t_out) {
    const float ox = ro.x() - center.x();
    const float oy = ro.y() - center.y();
    const float dx = rd.x();
    const float dy = rd.y();

    const float a = dx * dx + dy * dy;
    const float b = 2.0f * (ox * dx + oy * dy);
    const float c = ox * ox + oy * oy - r * r;

    float best = std::numeric_limits<float>::infinity();
    bool found = false;

    if (a > 1e-9f) {
        const float disc = b * b - 4 * a * c;
        if (disc >= 0) {
            const float sd = std::sqrt(disc);
            for (float t : {(-b - sd) / (2 * a), (-b + sd) / (2 * a)}) {
                if (t < 0) continue;
                const float z = ro.z() + t * rd.z();
                if (z >= zmin && z <= zmax && t < best) {
                    best = t;
                    found = true;
                }
            }
        }
    }
    if (std::abs(rd.z()) > 1e-9f) {
        for (float z_plane : {zmin, zmax}) {
            const float t = (z_plane - ro.z()) / rd.z();
            if (t < 0) continue;
            const float px = ox + t * dx;
            const float py = oy + t * dy;
            if (px * px + py * py <= r * r && t < best) {
                best = t;
                found = true;
            }
        }
    }
    if (!found) return false;
    t_out = best;
    return true;
}

}  // namespace

Selection Viewport3D::pick_at_screen(QPointF screen_pos, float* t_out) const {
    const Ray ray = ray_from_screen(screen_pos);
    Selection best{};
    float best_t = std::numeric_limits<float>::infinity();

    for (const auto& [id, b] : document_.boxes()) {
        const QVector3D center(static_cast<float>(b.position.x()),
                               static_cast<float>(b.position.y()), 0.0f);
        float t;
        if (ray_vs_obox(ray.origin, ray.direction, center,
                        static_cast<float>(b.size_xy.x() * 0.5),
                        static_cast<float>(b.size_xy.y() * 0.5),
                        static_cast<float>(b.base_z),
                        static_cast<float>(b.base_z + b.height),
                        static_cast<float>(b.rotation_z), t)) {
            if (t < best_t) {
                best_t = t;
                best = {id, SelectKind::Box};
            }
        }
    }

    for (const auto& [id, c] : document_.cylinders()) {
        const QVector3D center(static_cast<float>(c.position.x()),
                               static_cast<float>(c.position.y()), 0.0f);
        float t;
        if (ray_vs_cyl(ray.origin, ray.direction, center,
                       static_cast<float>(c.radius),
                       static_cast<float>(c.base_z),
                       static_cast<float>(c.base_z + c.height), t)) {
            if (t < best_t) {
                best_t = t;
                best = {id, SelectKind::Cylinder};
            }
        }
    }

    for (const auto& [id, w] : document_.walls()) {
        const QVector3D s(static_cast<float>(w.start.x()),
                          static_cast<float>(w.start.y()), 0.0f);
        const QVector3D e(static_cast<float>(w.end.x()),
                          static_cast<float>(w.end.y()), 0.0f);
        const QVector3D dir = e - s;
        const float len = dir.length();
        if (len < 1e-5f) continue;
        const QVector3D unit = dir / len;
        const float yaw = std::atan2(unit.y(), unit.x());
        const QVector3D center = (s + e) * 0.5f;
        float t;
        if (ray_vs_obox(ray.origin, ray.direction, center,
                        len * 0.5f, static_cast<float>(w.thickness * 0.5),
                        0.0f, static_cast<float>(w.height), yaw, t)) {
            if (t < best_t) {
                best_t = t;
                best = {id, SelectKind::Wall};
            }
        }
    }

    if (t_out) *t_out = best_t;
    return best;
}

void Viewport3D::emit_drag_commands() {
    for (const auto& sel : plan_view_.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;

        if (sel.kind == SelectKind::Wall) {
            auto* w = document_.find_wall(sel.id);
            if (!w) continue;
            const auto& orig = std::get<cadino::core::Wall>(it->second);
            cadino::core::Wall after = *w;
            *w = orig;
            stack_.execute(std::make_unique<cadino::core::ModifyWallCommand>(sel.id, std::move(after)));
        } else if (sel.kind == SelectKind::Box) {
            auto* b = document_.find_box(sel.id);
            if (!b) continue;
            const auto& orig = std::get<cadino::core::Box>(it->second);
            cadino::core::Box after = *b;
            *b = orig;
            stack_.execute(std::make_unique<cadino::core::ModifyBoxCommand>(sel.id, std::move(after)));
        } else if (sel.kind == SelectKind::Cylinder) {
            auto* c = document_.find_cylinder(sel.id);
            if (!c) continue;
            const auto& orig = std::get<cadino::core::Cylinder>(it->second);
            cadino::core::Cylinder after = *c;
            *c = orig;
            stack_.execute(std::make_unique<cadino::core::ModifyCylinderCommand>(sel.id, std::move(after)));
        }
    }
    drag_originals_.clear();
}

void Viewport3D::mousePressEvent(QMouseEvent* event) {
    drag_button_ = event->button();
    drag_last_ = event->position();

    if (event->button() == Qt::LeftButton) {
        const Selection hit = pick_at_screen(event->position());
        const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
        if (hit.valid()) {
            if (shift) {
                plan_view_.toggle_selection(hit);
            } else if (!plan_view_.is_selected(hit)) {
                plan_view_.set_selections({hit});
            }
            entity_dragging_ = true;
            QVector3D ground;
            if (ray_ground_intersection(ray_from_screen(event->position()), ground)) {
                drag_ground_start_ = ground;
            }
            drag_originals_.clear();
            for (const auto& sel : plan_view_.selections()) {
                if (sel.kind == SelectKind::Wall) {
                    if (const auto* w = document_.find_wall(sel.id))
                        drag_originals_.emplace(sel.id, *w);
                } else if (sel.kind == SelectKind::Box) {
                    if (const auto* b = document_.find_box(sel.id))
                        drag_originals_.emplace(sel.id, *b);
                } else if (sel.kind == SelectKind::Cylinder) {
                    if (const auto* c = document_.find_cylinder(sel.id))
                        drag_originals_.emplace(sel.id, *c);
                }
            }
            update();
            return;
        }
        if (!shift) plan_view_.clear_selection();
    }
    setCursor(Qt::ClosedHandCursor);
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    if (entity_dragging_) {
        QVector3D ground_now;
        if (ray_ground_intersection(ray_from_screen(event->position()), ground_now)) {
            const QVector3D delta = ground_now - drag_ground_start_;
            for (const auto& sel : plan_view_.selections()) {
                const auto it = drag_originals_.find(sel.id);
                if (it == drag_originals_.end()) continue;
                if (sel.kind == SelectKind::Wall) {
                    auto* w = document_.find_wall(sel.id);
                    if (!w) continue;
                    const auto& orig = std::get<cadino::core::Wall>(it->second);
                    w->start = orig.start + Eigen::Vector2d{delta.x(), delta.y()};
                    w->end = orig.end + Eigen::Vector2d{delta.x(), delta.y()};
                } else if (sel.kind == SelectKind::Box) {
                    auto* b = document_.find_box(sel.id);
                    if (!b) continue;
                    const auto& orig = std::get<cadino::core::Box>(it->second);
                    b->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
                } else if (sel.kind == SelectKind::Cylinder) {
                    auto* c = document_.find_cylinder(sel.id);
                    if (!c) continue;
                    const auto& orig = std::get<cadino::core::Cylinder>(it->second);
                    c->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
                }
            }
            update();
            plan_view_.update();
        }
        return;
    }

    if (drag_button_ == Qt::NoButton) return;

    const QPointF delta = event->position() - drag_last_;
    drag_last_ = event->position();

    if (drag_button_ == Qt::LeftButton && preset_ == CameraPreset::Iso) {
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
    if (entity_dragging_) {
        entity_dragging_ = false;
        if (!drag_originals_.empty()) {
            emit_drag_commands();
            plan_view_.notify_document_modified();
        }
    }
    drag_button_ = Qt::NoButton;
    unsetCursor();
}

void Viewport3D::wheelEvent(QWheelEvent* event) {
    const float factor = std::pow(0.9985f, static_cast<float>(event->angleDelta().y()));
    camera_distance_ = std::clamp(camera_distance_ * factor, 200.0f, 100000.0f);
    update();
}

}  // namespace cadino::ui
