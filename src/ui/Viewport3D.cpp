#include "Viewport3D.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <vector>

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QWheelEvent>

#include <spdlog/spdlog.h>

#include "PlanView.hpp"
#include "command/BlockCommands.hpp"
#include "command/BlockInstanceCommands.hpp"
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
layout(location = 3) in vec2 a_material;
layout(location = 4) in vec3 a_uv_pattern;  // uv.xy + pattern_id encoded in z

uniform mat4 u_view_proj;
uniform mat4 u_model;

out vec3 v_normal;
out vec3 v_color;
out vec3 v_world;
out vec2 v_material;
out vec2 v_uv;
flat out int v_pattern;

void main() {
    vec4 wp = u_model * vec4(a_pos, 1.0);
    v_normal = a_normal;
    v_color = a_color;
    v_world = wp.xyz;
    v_material = a_material;
    v_uv = a_uv_pattern.xy;
    v_pattern = int(a_uv_pattern.z + 0.5);
    gl_Position = u_view_proj * wp;
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world;
in vec2 v_material;
in vec2 v_uv;
flat in int v_pattern;
out vec4 frag_color;

uniform vec3 u_light_dir;
uniform vec3 u_eye_pos;
uniform int u_shadow_mode;
uniform int u_has_texture;
uniform sampler2D u_albedo;

const float PI = 3.14159265358979323846;

vec3 apply_pattern(vec3 base, int pattern, vec2 uv) {
    if (pattern == 1) {
        // Checker — 500 mm cells.
        vec2 cell = floor(uv / 500.0);
        float dark = mod(cell.x + cell.y, 2.0);
        return mix(base, base * 0.55, dark);
    }
    if (pattern == 2) {
        // Stripes — 100 mm spacing along U.
        float band = mod(floor(uv.x / 100.0), 2.0);
        return mix(base, base * 0.70, band);
    }
    if (pattern == 3) {
        // Wood grain — sine bands along U with secondary modulation.
        float b1 = sin(uv.x * 0.02) * 0.5 + 0.5;
        float b2 = sin(uv.y * 0.005 + uv.x * 0.005) * 0.5 + 0.5;
        float t = mix(b1, b2, 0.35);
        return mix(base * 0.65, base * 1.10, t);
    }
    return base;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(N, H), 0.0);
    float ndoth2 = ndoth * ndoth;
    float denom = ndoth2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom + 1e-7);
}

float geometry_schlick_ggx(float ndotv, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return ndotv / (ndotv * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float ndotv = max(dot(N, V), 0.0);
    float ndotl = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(ndotv, roughness) * geometry_schlick_ggx(ndotl, roughness);
}

vec3 brdf_lobe(vec3 N, vec3 V, vec3 L, vec3 light_color, vec3 albedo, float roughness, float metallic) {
    vec3 H = normalize(V + L);
    float ndotl = max(dot(N, L), 0.0);
    if (ndotl <= 0.0) return vec3(0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NDF = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * ndotl + 1e-5;
    vec3 specular = numerator / denominator;

    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
    return (kd * albedo / PI + specular) * light_color * ndotl;
}

void main() {
    if (u_shadow_mode == 1) {
        frag_color = vec4(0.02, 0.02, 0.04, 0.30);
        return;
    }
    if (u_shadow_mode == 2) {
        frag_color = vec4(v_color, 1.0);
        return;
    }
    vec3 N = normalize(v_normal);
    if (!gl_FrontFacing) N = -N;

    float roughness = clamp(v_material.x, 0.04, 1.0);
    float metallic = clamp(v_material.y, 0.0, 1.0);

    vec3 V = normalize(u_eye_pos - v_world);

    vec3 key_dir = -normalize(u_light_dir);
    vec3 fill_dir = normalize(vec3(0.5, 0.7, 0.6));
    vec3 sky_dir = vec3(0.0, 0.0, 1.0);

    vec3 key_color = vec3(1.00, 0.96, 0.88) * 2.4;
    vec3 fill_color = vec3(0.55, 0.65, 0.75) * 0.6;
    vec3 sky_color = vec3(0.45, 0.55, 0.70) * 0.4;

    vec3 albedo = apply_pattern(v_color, v_pattern, v_uv);
    if (u_has_texture == 1) {
        // UV are world mm; tile so the image repeats every ~1 metre.
        vec3 tex = texture(u_albedo, v_uv / 1000.0).rgb;
        albedo *= tex;
    }

    vec3 Lo = vec3(0.0);
    Lo += brdf_lobe(N, V, key_dir, key_color, albedo, roughness, metallic);
    Lo += brdf_lobe(N, V, fill_dir, fill_color, albedo, roughness, metallic);
    Lo += brdf_lobe(N, V, sky_dir, sky_color, albedo, roughness, metallic);

    // Ambient term tinted with hemisphere lighting
    float up = N.z * 0.5 + 0.5;
    vec3 ambient_color = mix(vec3(0.12, 0.10, 0.09), vec3(0.30, 0.34, 0.40), up);
    vec3 ambient = albedo * ambient_color * (1.0 - metallic * 0.6);

    vec3 col = ambient + Lo;
    // Reinhard tone mapping + slight gamma compensation
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0 / 2.2));
    frag_color = vec4(col, 1.0);
}
)";

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float cr, cg, cb;
    float rough, metal;
    float u, v, pattern;
};

void push_quad_uv(std::vector<Vertex>& verts,
                  QVector3D a, QVector3D b, QVector3D c, QVector3D d,
                  QVector3D n, QVector3D color, float rough, float metal,
                  float ua, float va, float ub, float vb,
                  float uc, float vc, float ud, float vd,
                  float pattern) {
    auto push = [&](QVector3D p, float uu, float vv) {
        verts.push_back({p.x(), p.y(), p.z(), n.x(), n.y(), n.z(),
                         color.x(), color.y(), color.z(), rough, metal,
                         uu, vv, pattern});
    };
    push(a, ua, va); push(b, ub, vb); push(c, uc, vc);
    push(a, ua, va); push(c, uc, vc); push(d, ud, vd);
}

void push_quad(std::vector<Vertex>& verts, QVector3D a, QVector3D b, QVector3D c,
               QVector3D d, QVector3D n, QVector3D color, float rough, float metal,
               float pattern = 0.0f) {
    // Planar mapping aligned to the quad in world units; tessellator passes
    // matching coordinates via push_quad_uv when it needs face-specific UVs.
    push_quad_uv(verts, a, b, c, d, n, color, rough, metal,
                 a.x(), a.y(), b.x(), b.y(),
                 c.x(), c.y(), d.x(), d.y(), pattern);
}

void push_cylinder(std::vector<Vertex>& verts, QVector3D center, float radius,
                   float zmin, float zmax, QVector3D color, float rough, float metal,
                   float pattern = 0.0f, int segments = 32) {
    const float pi = std::numbers::pi_v<float>;
    const auto cv = [&](QVector3D p, QVector3D n, float u, float v) {
        verts.push_back({p.x(), p.y(), p.z(), n.x(), n.y(), n.z(),
                         color.x(), color.y(), color.z(), rough, metal,
                         u, v, pattern});
    };

    const float circumference = 2.0f * pi * radius;
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
        const float u0 = a0 / (2.0f * pi) * circumference;
        const float u1 = a1 / (2.0f * pi) * circumference;
        cv(b0, n0, u0, zmin);
        cv(b1, n1, u1, zmin);
        cv(t1, n1, u1, zmax);
        cv(b0, n0, u0, zmin);
        cv(t1, n1, u1, zmax);
        cv(t0, n0, u0, zmax);
    }

    const QVector3D top_c(center.x(), center.y(), zmax);
    const QVector3D bot_c(center.x(), center.y(), zmin);
    const QVector3D zp(0.0f, 0.0f, 1.0f);
    const QVector3D zn(0.0f, 0.0f, -1.0f);
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * pi * i / segments;
        const float a1 = 2.0f * pi * (i + 1) / segments;
        const float c0 = std::cos(a0), s0 = std::sin(a0);
        const float c1 = std::cos(a1), s1 = std::sin(a1);
        const QVector3D pt0(center.x() + radius * c0, center.y() + radius * s0, zmax);
        const QVector3D pt1(center.x() + radius * c1, center.y() + radius * s1, zmax);
        cv(top_c, zp, center.x(), center.y());
        cv(pt0,   zp, pt0.x(),    pt0.y());
        cv(pt1,   zp, pt1.x(),    pt1.y());

        const QVector3D pb0(center.x() + radius * c0, center.y() + radius * s0, zmin);
        const QVector3D pb1(center.x() + radius * c1, center.y() + radius * s1, zmin);
        cv(bot_c, zn, center.x(), center.y());
        cv(pb1,   zn, pb1.x(),    pb1.y());
        cv(pb0,   zn, pb0.x(),    pb0.y());
    }
}

void push_oriented_box(std::vector<Vertex>& verts, QVector3D center, float hx, float hy,
                       float zmin, float zmax, float yaw, QVector3D color,
                       float rough, float metal, float pattern = 0.0f) {
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

    // Per-face planar UV: top/bottom use the box's local XY (in mm), the four
    // sides use (along-face arc length, height).
    const float sx = hx * 2.0f, sy = hy * 2.0f, sz = zmax - zmin;
    push_quad_uv(verts, t0, t1, t2, t3, zp, color, rough, metal,
                 -hx, -hy,  hx, -hy,  hx,  hy, -hx,  hy, pattern);
    push_quad_uv(verts, b3, b2, b1, b0, -zp, color, rough, metal,
                 -hx,  hy,  hx,  hy,  hx, -hy, -hx, -hy, pattern);
    push_quad_uv(verts, b1, b2, t2, t1, xp, color, rough, metal,
                 0, zmin, sy, zmin, sy, zmax, 0, zmax, pattern);
    push_quad_uv(verts, b3, b0, t0, t3, -xp, color, rough, metal,
                 0, zmin, sy, zmin, sy, zmax, 0, zmax, pattern);
    push_quad_uv(verts, b2, b3, t3, t2, yp, color, rough, metal,
                 0, zmin, sx, zmin, sx, zmax, 0, zmax, pattern);
    push_quad_uv(verts, b0, b1, t1, t0, -yp, color, rough, metal,
                 0, zmin, sx, zmin, sx, zmax, 0, zmax, pattern);
    (void)sz;
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
    const float r = w.roughness;
    const float m = w.metallic;
    const float pat = static_cast<float>(w.pattern);
    const float thk = static_cast<float>(w.thickness);
    const float h_mm = static_cast<float>(w.height);
    push_quad_uv(verts, t0, t1, t2, t3, up, color, r, m,
                 0, 0, len, 0, len, thk, 0, thk, pat);
    push_quad_uv(verts, b3, b2, b1, b0, -up, color, r, m,
                 0, thk, len, thk, len, 0, 0, 0, pat);
    push_quad_uv(verts, b0, b1, t1, t0, normal, color, r, m,
                 0, 0, len, 0, len, h_mm, 0, h_mm, pat);
    push_quad_uv(verts, b2, b3, t3, t2, -normal, color, r, m,
                 0, 0, len, 0, len, h_mm, 0, h_mm, pat);
    push_quad_uv(verts, b1, b2, t2, t1, unit, color, r, m,
                 0, 0, thk, 0, thk, h_mm, 0, h_mm, pat);
    push_quad_uv(verts, b3, b0, t0, t3, -unit, color, r, m,
                 0, 0, thk, 0, thk, h_mm, 0, h_mm, pat);
}

void push_ground_grid(std::vector<Vertex>& verts, float size) {
    const QVector3D up(0.0f, 0.0f, 1.0f);
    const QVector3D a(-size, -size, 0.0f);
    const QVector3D b(size, -size, 0.0f);
    const QVector3D c(size, size, 0.0f);
    const QVector3D d(-size, size, 0.0f);
    const QVector3D ground_color(0.18f, 0.20f, 0.24f);
    push_quad(verts, a, b, c, d, up, ground_color, 0.95f, 0.0f);
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
    line_vbo_.destroy();
    line_vao_.destroy();
    gizmo_vbo_.destroy();
    gizmo_vao_.destroy();
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

    line_vao_.create();
    line_vbo_.create();
    line_vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    gizmo_vao_.create();
    gizmo_vbo_.create();
    gizmo_vbo_.setUsagePattern(QOpenGLBuffer::DynamicDraw);
}

void Viewport3D::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void Viewport3D::rebuild_mesh() {
    // Group vertices by texture path so we can issue one draw call per unique
    // albedo. The empty string is the "no texture" bucket and stays first so
    // the ground / shadow split in paintGL keeps working.
    std::map<QString, std::vector<Vertex>> buckets;
    auto& untex = buckets[QString()];
    untex.reserve(36 * document_.walls().size() + 6);

    auto layer_visible = [this](cadino::core::EntityId lid) {
        if (!lid.valid()) return true;
        const auto* l = document_.find_layer(lid);
        return !l || l->visible;
    };

    push_ground_grid(untex, 20000.0f);
    for (const auto& [id, w] : document_.walls()) {
        if (!layer_visible(w.layer_id)) continue;
        auto& b = buckets[QString::fromStdString(w.texture_path)];
        push_wall_box(b, w);
    }
    const std::size_t walls_end = untex.size();
    for (const auto& [id, b] : document_.boxes()) {
        if (!layer_visible(b.layer_id)) continue;
        auto& bucket = buckets[QString::fromStdString(b.texture_path)];
        const QVector3D center(static_cast<float>(b.position.x()),
                               static_cast<float>(b.position.y()), 0.0f);
        push_oriented_box(bucket, center,
                          static_cast<float>(b.size_xy.x() * 0.5),
                          static_cast<float>(b.size_xy.y() * 0.5),
                          static_cast<float>(b.base_z),
                          static_cast<float>(b.base_z + b.height),
                          static_cast<float>(b.rotation_z),
                          QVector3D(b.color.r, b.color.g, b.color.b),
                          b.roughness, b.metallic, static_cast<float>(b.pattern));
    }
    for (const auto& [id, c] : document_.cylinders()) {
        if (!layer_visible(c.layer_id)) continue;
        auto& bucket = buckets[QString::fromStdString(c.texture_path)];
        const QVector3D center(static_cast<float>(c.position.x()),
                               static_cast<float>(c.position.y()), 0.0f);
        push_cylinder(bucket, center, static_cast<float>(c.radius),
                      static_cast<float>(c.base_z),
                      static_cast<float>(c.base_z + c.height),
                      QVector3D(c.color.r, c.color.g, c.color.b),
                      c.roughness, c.metallic, static_cast<float>(c.pattern));
    }
    for (const auto& [id, block] : document_.blocks()) {
        if (!layer_visible(block.layer_id)) continue;
        for (const auto& local_b : block.boxes) {
            const auto b = block.world_box(local_b);
            auto& bucket = buckets[QString::fromStdString(b.texture_path)];
            const QVector3D center(static_cast<float>(b.position.x()),
                                   static_cast<float>(b.position.y()), 0.0f);
            push_oriented_box(bucket, center,
                              static_cast<float>(b.size_xy.x() * 0.5),
                              static_cast<float>(b.size_xy.y() * 0.5),
                              static_cast<float>(b.base_z),
                              static_cast<float>(b.base_z + b.height),
                              static_cast<float>(b.rotation_z),
                              QVector3D(b.color.r, b.color.g, b.color.b),
                              b.roughness, b.metallic, static_cast<float>(b.pattern));
        }
        for (const auto& local_c : block.cylinders) {
            const auto c = block.world_cylinder(local_c);
            auto& bucket = buckets[QString::fromStdString(c.texture_path)];
            const QVector3D center(static_cast<float>(c.position.x()),
                                   static_cast<float>(c.position.y()), 0.0f);
            push_cylinder(bucket, center, static_cast<float>(c.radius),
                          static_cast<float>(c.base_z),
                          static_cast<float>(c.base_z + c.height),
                          QVector3D(c.color.r, c.color.g, c.color.b),
                          c.roughness, c.metallic, static_cast<float>(c.pattern));
        }
    }
    for (const auto& [id, inst] : document_.block_instances()) {
        if (!layer_visible(inst.layer_id)) continue;
        const auto* def = document_.find_block_def(inst.definition_id);
        if (!def) continue;
        for (const auto& local_b : def->boxes) {
            const auto b = inst.world_box(local_b);
            auto& bucket = buckets[QString::fromStdString(b.texture_path)];
            const QVector3D center(static_cast<float>(b.position.x()),
                                   static_cast<float>(b.position.y()), 0.0f);
            push_oriented_box(bucket, center,
                              static_cast<float>(b.size_xy.x() * 0.5),
                              static_cast<float>(b.size_xy.y() * 0.5),
                              static_cast<float>(b.base_z),
                              static_cast<float>(b.base_z + b.height),
                              static_cast<float>(b.rotation_z),
                              QVector3D(b.color.r, b.color.g, b.color.b),
                              b.roughness, b.metallic, static_cast<float>(b.pattern));
        }
        for (const auto& local_c : def->cylinders) {
            const auto c = inst.world_cylinder(local_c);
            auto& bucket = buckets[QString::fromStdString(c.texture_path)];
            const QVector3D center(static_cast<float>(c.position.x()),
                                   static_cast<float>(c.position.y()), 0.0f);
            push_cylinder(bucket, center, static_cast<float>(c.radius),
                          static_cast<float>(c.base_z),
                          static_cast<float>(c.base_z + c.height),
                          QVector3D(c.color.r, c.color.g, c.color.b),
                          c.roughness, c.metallic, static_cast<float>(c.pattern));
        }
    }
    for (const auto& [id, surf] : document_.surfaces()) {
        if (!layer_visible(surf.layer_id)) continue;
        auto& bucket = untex;
        const auto tess = surf.tessellate(24, 24);
        const QVector3D color(surf.color.r, surf.color.g, surf.color.b);
        const float rough = surf.roughness;
        const float metal = surf.metallic;
        const float pat = static_cast<float>(surf.pattern);
        for (std::size_t i = 0; i + 2 < tess.indices.size(); i += 3) {
            const auto& p0 = tess.positions[tess.indices[i]];
            const auto& p1 = tess.positions[tess.indices[i + 1]];
            const auto& p2 = tess.positions[tess.indices[i + 2]];
            const auto& n0 = tess.normals[tess.indices[i]];
            const auto& n1 = tess.normals[tess.indices[i + 1]];
            const auto& n2 = tess.normals[tess.indices[i + 2]];
            bucket.push_back({p0.x(), p0.y(), p0.z(), n0.x(), n0.y(), n0.z(),
                              color.x(), color.y(), color.z(), rough, metal,
                              p0.x(), p0.y(), pat});
            bucket.push_back({p1.x(), p1.y(), p1.z(), n1.x(), n1.y(), n1.z(),
                              color.x(), color.y(), color.z(), rough, metal,
                              p1.x(), p1.y(), pat});
            bucket.push_back({p2.x(), p2.y(), p2.z(), n2.x(), n2.y(), n2.z(),
                              color.x(), color.y(), color.z(), rough, metal,
                              p2.x(), p2.y(), pat});
        }
    }
    for (const auto& [id, m] : document_.meshes()) {
        auto& bucket = buckets[QString::fromStdString(m.texture_path)];
        const QVector3D color(m.color.r, m.color.g, m.color.b);
        const float rough = m.roughness;
        const float metal = m.metallic;
        for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const auto& p0 = m.positions[m.indices[i]];
            const auto& p1 = m.positions[m.indices[i + 1]];
            const auto& p2 = m.positions[m.indices[i + 2]];
            const auto& n0 = i < m.normals.size() ? m.normals[m.indices[i]] : Eigen::Vector3f::UnitZ();
            const auto& n1 = i + 1 < m.normals.size() ? m.normals[m.indices[i + 1]] : Eigen::Vector3f::UnitZ();
            const auto& n2 = i + 2 < m.normals.size() ? m.normals[m.indices[i + 2]] : Eigen::Vector3f::UnitZ();
            const float pat = static_cast<float>(m.pattern);
            bucket.push_back({p0.x(), p0.y(), p0.z(), n0.x(), n0.y(), n0.z(),
                              color.x(), color.y(), color.z(), rough, metal,
                              p0.x(), p0.y(), pat});
            bucket.push_back({p1.x(), p1.y(), p1.z(), n1.x(), n1.y(), n1.z(),
                              color.x(), color.y(), color.z(), rough, metal,
                              p1.x(), p1.y(), pat});
            bucket.push_back({p2.x(), p2.y(), p2.z(), n2.x(), n2.y(), n2.z(),
                              color.x(), color.y(), color.z(), rough, metal,
                              p2.x(), p2.y(), pat});
        }
    }
    for (const auto& [id, s] : document_.slabs()) {
        if (!layer_visible(s.layer_id)) continue;
        if (s.outline.size() < 3) continue;
        double minx = s.outline[0].x(), miny = s.outline[0].y();
        double maxx = minx, maxy = miny;
        for (const auto& v : s.outline) {
            minx = std::min(minx, v.x()); miny = std::min(miny, v.y());
            maxx = std::max(maxx, v.x()); maxy = std::max(maxy, v.y());
        }
        const QVector3D center((minx + maxx) * 0.5f, (miny + maxy) * 0.5f, 0.0f);
        push_oriented_box(untex, center,
                          static_cast<float>((maxx - minx) * 0.5),
                          static_cast<float>((maxy - miny) * 0.5),
                          static_cast<float>(s.level),
                          static_cast<float>(s.level + s.thickness),
                          0.0f,
                          QVector3D(0.72f, 0.65f, 0.55f), 0.75f, 0.0f);
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
        push_oriented_box(untex, center,
                          static_cast<float>(d.width * 0.5),
                          static_cast<float>(w->thickness * 0.5),
                          static_cast<float>(d.sill_height),
                          static_cast<float>(d.sill_height + d.height),
                          yaw,
                          QVector3D(0.55f, 0.35f, 0.20f), 0.55f, 0.0f);
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
        push_oriented_box(untex, center,
                          static_cast<float>(win.width * 0.5),
                          static_cast<float>(w->thickness * 0.5 + 1.0),
                          static_cast<float>(win.sill_height),
                          static_cast<float>(win.sill_height + win.height),
                          yaw,
                          QVector3D(0.65f, 0.80f, 0.90f), 0.10f, 0.0f);
    }
    walls_vertex_end_ = static_cast<int>(walls_end);

    // Concatenate buckets into a single VBO and record draw groups. The empty
    // "no texture" bucket goes first so it owns the ground plane that the
    // planar shadow pass keys off of.
    std::vector<Vertex> verts;
    mesh_groups_.clear();
    {
        auto append = [&](const QString& path, const std::vector<Vertex>& chunk) {
            if (chunk.empty()) return;
            DrawGroup g;
            g.offset = static_cast<int>(verts.size());
            g.count = static_cast<int>(chunk.size());
            g.texture_path = path;
            mesh_groups_.push_back(g);
            verts.insert(verts.end(), chunk.begin(), chunk.end());
        };
        // Untextured first.
        auto it_empty = buckets.find(QString());
        if (it_empty != buckets.end()) append(QString(), it_empty->second);
        for (const auto& [path, chunk] : buckets) {
            if (path.isEmpty()) continue;
            append(path, chunk);
        }
    }

    vao_.bind();
    vbo_.bind();
    vbo_.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(Vertex)));
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(Vertex));
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(1, GL_FLOAT, sizeof(float) * 3, 3, sizeof(Vertex));
    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(2, GL_FLOAT, sizeof(float) * 6, 3, sizeof(Vertex));
    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(3, GL_FLOAT, sizeof(float) * 9, 2, sizeof(Vertex));
    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(4, GL_FLOAT, sizeof(float) * 11, 3, sizeof(Vertex));
    program_->enableAttributeArray(4);
    vbo_.release();
    vao_.release();

    vertex_count_ = static_cast<int>(verts.size());

    // ---- Line geometry pass for NURBS curves ----
    std::vector<Vertex> line_verts;
    for (const auto& [id, curve] : document_.curves()) {
        const auto samples = curve.tessellate(128);
        if (samples.size() < 2) continue;
        const float cr = curve.color.r, cg = curve.color.g, cb = curve.color.b;
        for (std::size_t i = 1; i < samples.size(); ++i) {
            const auto& a = samples[i - 1];
            const auto& b = samples[i];
            line_verts.push_back({static_cast<float>(a.x()), static_cast<float>(a.y()),
                                  static_cast<float>(a.z()),
                                  0.0f, 0.0f, 1.0f, cr, cg, cb, 1.0f, 0.0f,
                                  0.0f, 0.0f, 0.0f});
            line_verts.push_back({static_cast<float>(b.x()), static_cast<float>(b.y()),
                                  static_cast<float>(b.z()),
                                  0.0f, 0.0f, 1.0f, cr, cg, cb, 1.0f, 0.0f,
                                  0.0f, 0.0f, 0.0f});
        }
    }
    line_vao_.bind();
    line_vbo_.bind();
    line_vbo_.allocate(line_verts.data(),
                       static_cast<int>(line_verts.size() * sizeof(Vertex)));
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(Vertex));
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(1, GL_FLOAT, sizeof(float) * 3, 3, sizeof(Vertex));
    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(2, GL_FLOAT, sizeof(float) * 6, 3, sizeof(Vertex));
    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(3, GL_FLOAT, sizeof(float) * 9, 2, sizeof(Vertex));
    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(4, GL_FLOAT, sizeof(float) * 11, 3, sizeof(Vertex));
    program_->enableAttributeArray(4);
    line_vbo_.release();
    line_vao_.release();
    line_vertex_count_ = static_cast<int>(line_verts.size());

    last_wall_count_ = document_.walls().size();
    last_curve_count_ = document_.curves().size();
    mesh_dirty_ = false;
}

QVector3D Viewport3D::eye_position() const {
    const float yaw_rad = camera_yaw_ * static_cast<float>(std::numbers::pi) / 180.0f;
    const float pitch_rad = camera_pitch_ * static_cast<float>(std::numbers::pi) / 180.0f;
    const float cp = std::cos(pitch_rad);
    const QVector3D offset(camera_distance_ * std::cos(yaw_rad) * cp,
                           camera_distance_ * std::sin(yaw_rad) * cp,
                           camera_distance_ * std::sin(pitch_rad));
    return camera_target_ + offset;
}

QMatrix4x4 Viewport3D::view_matrix() const {
    QMatrix4x4 v;
    v.lookAt(eye_position(), camera_target_, QVector3D(0.0f, 0.0f, 1.0f));
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
    program_->setUniformValue("u_eye_pos", eye_position());

    QMatrix4x4 identity;
    program_->setUniformValue("u_model", identity);
    program_->setUniformValue("u_shadow_mode", 0);
    program_->setUniformValue("u_albedo", 0);
    if (mesh_groups_.empty()) {
        program_->setUniformValue("u_has_texture", 0);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
    } else {
        for (const auto& g : mesh_groups_) {
            QOpenGLTexture* tex = nullptr;
            if (!g.texture_path.isEmpty()) {
                const std::string key = g.texture_path.toStdString();
                auto it = texture_cache_.find(key);
                if (it == texture_cache_.end()) {
                    QImage img(g.texture_path);
                    if (!img.isNull()) {
                        auto t = std::make_unique<QOpenGLTexture>(img.flipped(Qt::Vertical));
                        t->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
                        t->setMagnificationFilter(QOpenGLTexture::Linear);
                        t->setWrapMode(QOpenGLTexture::Repeat);
                        it = texture_cache_.emplace(key, std::move(t)).first;
                    }
                }
                if (it != texture_cache_.end()) tex = it->second.get();
            }
            if (tex) {
                tex->bind(0);
                program_->setUniformValue("u_has_texture", 1);
            } else {
                program_->setUniformValue("u_has_texture", 0);
            }
            glDrawArrays(GL_TRIANGLES, g.offset, g.count);
        }
    }

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
        program_->setUniformValue("u_has_texture", 0);
        glDrawArrays(GL_TRIANGLES, 6, vertex_count_ - 6);

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    vao_.release();

    if (line_vertex_count_ > 0) {
        line_vao_.bind();
        program_->setUniformValue("u_model", identity);
        program_->setUniformValue("u_shadow_mode", 2);
        glLineWidth(2.5f);
        glDrawArrays(GL_LINES, 0, line_vertex_count_);
        line_vao_.release();
    }

    render_gizmo();

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

    auto layer_pickable = [this](cadino::core::EntityId lid) {
        if (!lid.valid()) return true;
        const auto* l = document_.find_layer(lid);
        return !l || (l->visible && !l->locked);
    };

    for (const auto& [id, b] : document_.boxes()) {
        if (!layer_pickable(b.layer_id)) continue;
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
        if (!layer_pickable(c.layer_id)) continue;
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
        if (!layer_pickable(w.layer_id)) continue;
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

    auto pick_box_world = [&](const cadino::core::Box& b, float& t) -> bool {
        const QVector3D center(static_cast<float>(b.position.x()),
                               static_cast<float>(b.position.y()), 0.0f);
        return ray_vs_obox(ray.origin, ray.direction, center,
                           static_cast<float>(b.size_xy.x() * 0.5),
                           static_cast<float>(b.size_xy.y() * 0.5),
                           static_cast<float>(b.base_z),
                           static_cast<float>(b.base_z + b.height),
                           static_cast<float>(b.rotation_z), t);
    };
    auto pick_cyl_world = [&](const cadino::core::Cylinder& c, float& t) -> bool {
        const QVector3D center(static_cast<float>(c.position.x()),
                               static_cast<float>(c.position.y()), 0.0f);
        return ray_vs_cyl(ray.origin, ray.direction, center,
                          static_cast<float>(c.radius),
                          static_cast<float>(c.base_z),
                          static_cast<float>(c.base_z + c.height), t);
    };

    for (const auto& [id, bk] : document_.blocks()) {
        if (!layer_pickable(bk.layer_id)) continue;
        for (const auto& local_b : bk.boxes) {
            float t;
            if (pick_box_world(bk.world_box(local_b), t) && t < best_t) {
                best_t = t;
                best = {id, SelectKind::Block};
            }
        }
        for (const auto& local_c : bk.cylinders) {
            float t;
            if (pick_cyl_world(bk.world_cylinder(local_c), t) && t < best_t) {
                best_t = t;
                best = {id, SelectKind::Block};
            }
        }
    }
    for (const auto& [id, inst] : document_.block_instances()) {
        if (!layer_pickable(inst.layer_id)) continue;
        const auto* def = document_.find_block_def(inst.definition_id);
        if (!def) continue;
        for (const auto& local_b : def->boxes) {
            float t;
            if (pick_box_world(inst.world_box(local_b), t) && t < best_t) {
                best_t = t;
                best = {id, SelectKind::BlockInstance};
            }
        }
        for (const auto& local_c : def->cylinders) {
            float t;
            if (pick_cyl_world(inst.world_cylinder(local_c), t) && t < best_t) {
                best_t = t;
                best = {id, SelectKind::BlockInstance};
            }
        }
    }
    for (const auto& [id, surf] : document_.surfaces()) {
        if (!layer_pickable(surf.layer_id)) continue;
        if (surf.control_points.empty()) continue;
        Eigen::Vector3d lo = surf.control_points.front();
        Eigen::Vector3d hi = lo;
        for (const auto& p : surf.control_points) {
            lo = lo.cwiseMin(p);
            hi = hi.cwiseMax(p);
        }
        const QVector3D center(static_cast<float>((lo.x() + hi.x()) * 0.5),
                               static_cast<float>((lo.y() + hi.y()) * 0.5),
                               static_cast<float>((lo.z() + hi.z()) * 0.5));
        const float hx = static_cast<float>((hi.x() - lo.x()) * 0.5);
        const float hy = static_cast<float>((hi.y() - lo.y()) * 0.5);
        const float zmin = static_cast<float>(lo.z());
        const float zmax = static_cast<float>(hi.z());
        float t;
        if (ray_vs_obox(ray.origin, ray.direction,
                        QVector3D(center.x(), center.y(), 0.0f), hx, hy,
                        zmin, zmax, 0.0f, t) && t < best_t) {
            best_t = t;
            best = {id, SelectKind::NurbsSurface};
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
        } else if (sel.kind == SelectKind::Block) {
            auto* bk = document_.find_block(sel.id);
            if (!bk) continue;
            const auto& orig = std::get<cadino::core::Block>(it->second);
            cadino::core::Block after = *bk;
            *bk = orig;
            stack_.execute(std::make_unique<cadino::core::ModifyBlockCommand>(sel.id, std::move(after)));
        } else if (sel.kind == SelectKind::BlockInstance) {
            auto* bi = document_.find_block_instance(sel.id);
            if (!bi) continue;
            const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
            cadino::core::BlockInstance after = *bi;
            *bi = orig;
            stack_.execute(std::make_unique<cadino::core::ModifyBlockInstanceCommand>(sel.id, std::move(after)));
        }
    }
    drag_originals_.clear();
}

void Viewport3D::capture_drag_originals() {
    drag_originals_.clear();
    for (const auto& sel : plan_view_.selections()) {
        switch (sel.kind) {
            case SelectKind::Wall:
                if (const auto* w = document_.find_wall(sel.id))
                    drag_originals_.emplace(sel.id, *w);
                break;
            case SelectKind::Box:
                if (const auto* b = document_.find_box(sel.id))
                    drag_originals_.emplace(sel.id, *b);
                break;
            case SelectKind::Cylinder:
                if (const auto* c = document_.find_cylinder(sel.id))
                    drag_originals_.emplace(sel.id, *c);
                break;
            case SelectKind::Block:
                if (const auto* bk = document_.find_block(sel.id))
                    drag_originals_.emplace(sel.id, *bk);
                break;
            case SelectKind::BlockInstance:
                if (const auto* bi = document_.find_block_instance(sel.id))
                    drag_originals_.emplace(sel.id, *bi);
                break;
            default: break;
        }
    }
}

void Viewport3D::apply_drag_delta(const QVector3D& delta) {
    for (const auto& sel : plan_view_.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;
        switch (sel.kind) {
            case SelectKind::Wall: {
                auto* w = document_.find_wall(sel.id);
                if (!w) break;
                const auto& orig = std::get<cadino::core::Wall>(it->second);
                w->start = orig.start + Eigen::Vector2d{delta.x(), delta.y()};
                w->end = orig.end + Eigen::Vector2d{delta.x(), delta.y()};
                break;
            }
            case SelectKind::Box: {
                auto* b = document_.find_box(sel.id);
                if (!b) break;
                const auto& orig = std::get<cadino::core::Box>(it->second);
                b->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
                b->base_z = orig.base_z + delta.z();
                break;
            }
            case SelectKind::Cylinder: {
                auto* c = document_.find_cylinder(sel.id);
                if (!c) break;
                const auto& orig = std::get<cadino::core::Cylinder>(it->second);
                c->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
                c->base_z = orig.base_z + delta.z();
                break;
            }
            case SelectKind::Block: {
                auto* bk = document_.find_block(sel.id);
                if (!bk) break;
                const auto& orig = std::get<cadino::core::Block>(it->second);
                bk->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
                bk->base_z = orig.base_z + delta.z();
                break;
            }
            case SelectKind::BlockInstance: {
                auto* bi = document_.find_block_instance(sel.id);
                if (!bi) break;
                const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
                bi->position = orig.position + Eigen::Vector2d{delta.x(), delta.y()};
                bi->base_z = orig.base_z + delta.z();
                break;
            }
            default: break;
        }
    }
}

bool Viewport3D::selection_centroid(QVector3D& out) const {
    const auto& sels = plan_view_.selections();
    if (sels.empty()) return false;
    QVector3D sum;
    int n = 0;
    for (const auto& sel : sels) {
        QVector3D c;
        bool ok = false;
        switch (sel.kind) {
            case SelectKind::Wall:
                if (auto* w = document_.find_wall(sel.id)) {
                    c = QVector3D(static_cast<float>((w->start.x() + w->end.x()) * 0.5),
                                  static_cast<float>((w->start.y() + w->end.y()) * 0.5),
                                  static_cast<float>(w->height * 0.5));
                    ok = true;
                }
                break;
            case SelectKind::Box:
                if (auto* b = document_.find_box(sel.id)) {
                    c = QVector3D(static_cast<float>(b->position.x()),
                                  static_cast<float>(b->position.y()),
                                  static_cast<float>(b->base_z + b->height * 0.5));
                    ok = true;
                }
                break;
            case SelectKind::Cylinder:
                if (auto* cy = document_.find_cylinder(sel.id)) {
                    c = QVector3D(static_cast<float>(cy->position.x()),
                                  static_cast<float>(cy->position.y()),
                                  static_cast<float>(cy->base_z + cy->height * 0.5));
                    ok = true;
                }
                break;
            case SelectKind::Block:
                if (auto* bk = document_.find_block(sel.id)) {
                    c = QVector3D(static_cast<float>(bk->position.x()),
                                  static_cast<float>(bk->position.y()),
                                  static_cast<float>(bk->base_z));
                    ok = true;
                }
                break;
            case SelectKind::BlockInstance:
                if (auto* bi = document_.find_block_instance(sel.id)) {
                    c = QVector3D(static_cast<float>(bi->position.x()),
                                  static_cast<float>(bi->position.y()),
                                  static_cast<float>(bi->base_z));
                    ok = true;
                }
                break;
            default: break;
        }
        if (ok) {
            sum += c;
            ++n;
        }
    }
    if (n == 0) return false;
    out = sum / static_cast<float>(n);
    return true;
}

float Viewport3D::gizmo_length() const {
    const float h = static_cast<float>(std::max(1, height()));
    if (preset_ == CameraPreset::Iso) {
        const float dist = (eye_position() - gizmo_pivot_).length();
        const float fov = 50.0f * static_cast<float>(std::numbers::pi) / 180.0f;
        const float world_per_px = 2.0f * std::tan(fov * 0.5f) * dist / h;
        return world_per_px * 80.0f;
    }
    return (camera_distance_ / h) * 80.0f;
}

QVector3D Viewport3D::axis_direction(GizmoAxis a) const {
    switch (a) {
        case GizmoAxis::X: case GizmoAxis::ScaleX: return {1.0f, 0.0f, 0.0f};
        case GizmoAxis::Y: case GizmoAxis::ScaleY: return {0.0f, 1.0f, 0.0f};
        case GizmoAxis::Z: case GizmoAxis::ScaleZ: return {0.0f, 0.0f, 1.0f};
        default: return {};
    }
}

bool Viewport3D::ray_plane_intersection(const Ray& ray, const QVector3D& pivot,
                                        const QVector3D& normal,
                                        QVector3D& hit_out) const {
    const float denom = QVector3D::dotProduct(ray.direction, normal);
    if (std::abs(denom) < 1e-6f) return false;
    const float t = QVector3D::dotProduct(pivot - ray.origin, normal) / denom;
    if (t < 0.0f) return false;
    hit_out = ray.origin + ray.direction * t;
    return true;
}

double Viewport3D::rotation_angle_for(GizmoAxis axis, QPointF screen_pos, bool* ok) const {
    if (ok) *ok = false;
    QVector3D n;
    switch (axis) {
        case GizmoAxis::RotX: n = {1, 0, 0}; break;
        case GizmoAxis::RotY: n = {0, 1, 0}; break;
        case GizmoAxis::RotZ: n = {0, 0, 1}; break;
        default: return 0.0;
    }
    QVector3D hit;
    if (!ray_plane_intersection(ray_from_screen(screen_pos), gizmo_pivot_, n, hit)) {
        return 0.0;
    }
    const QVector3D d = hit - gizmo_pivot_;
    if (ok) *ok = true;
    switch (axis) {
        case GizmoAxis::RotX: return std::atan2(d.z(), d.y());
        case GizmoAxis::RotY: return std::atan2(d.x(), d.z());
        case GizmoAxis::RotZ: return std::atan2(d.y(), d.x());
        default: return 0.0;
    }
}

void Viewport3D::apply_drag_rotation(GizmoAxis axis, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const Eigen::Vector3d pv{gizmo_pivot_.x(), gizmo_pivot_.y(), gizmo_pivot_.z()};
    auto rotate3 = [&](Eigen::Vector3d p) {
        p -= pv;
        Eigen::Vector3d r = p;
        switch (axis) {
            case GizmoAxis::RotX:
                r.x() = p.x();
                r.y() = c * p.y() - s * p.z();
                r.z() = s * p.y() + c * p.z();
                break;
            case GizmoAxis::RotY:
                r.x() = c * p.x() + s * p.z();
                r.y() = p.y();
                r.z() = -s * p.x() + c * p.z();
                break;
            case GizmoAxis::RotZ:
                r.x() = c * p.x() - s * p.y();
                r.y() = s * p.x() + c * p.y();
                r.z() = p.z();
                break;
            default: break;
        }
        return pv + r;
    };
    auto rotate_xy_anchor = [&](const Eigen::Vector2d& pos, double z) {
        const Eigen::Vector3d r = rotate3({pos.x(), pos.y(), z});
        return std::pair<Eigen::Vector2d, double>{{r.x(), r.y()}, r.z()};
    };

    for (const auto& sel : plan_view_.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;
        switch (sel.kind) {
            case SelectKind::Wall: {
                if (axis != GizmoAxis::RotZ) break;
                auto* w = document_.find_wall(sel.id);
                if (!w) break;
                const auto& orig = std::get<cadino::core::Wall>(it->second);
                auto [s0, z0] = rotate_xy_anchor(orig.start, 0.0);
                auto [s1, z1] = rotate_xy_anchor(orig.end, 0.0);
                w->start = s0;
                w->end = s1;
                (void)z0; (void)z1;
                break;
            }
            case SelectKind::Box: {
                auto* b = document_.find_box(sel.id);
                if (!b) break;
                const auto& orig = std::get<cadino::core::Box>(it->second);
                auto [p, z] = rotate_xy_anchor(orig.position, orig.base_z);
                b->position = p;
                b->base_z = z;
                if (axis == GizmoAxis::RotZ) b->rotation_z = orig.rotation_z + angle;
                break;
            }
            case SelectKind::Cylinder: {
                auto* cy = document_.find_cylinder(sel.id);
                if (!cy) break;
                const auto& orig = std::get<cadino::core::Cylinder>(it->second);
                auto [p, z] = rotate_xy_anchor(orig.position, orig.base_z);
                cy->position = p;
                cy->base_z = z;
                break;
            }
            case SelectKind::Block: {
                auto* bk = document_.find_block(sel.id);
                if (!bk) break;
                const auto& orig = std::get<cadino::core::Block>(it->second);
                auto [p, z] = rotate_xy_anchor(orig.position, orig.base_z);
                bk->position = p;
                bk->base_z = z;
                if (axis == GizmoAxis::RotZ) bk->rotation_z = orig.rotation_z + angle;
                break;
            }
            case SelectKind::BlockInstance: {
                auto* bi = document_.find_block_instance(sel.id);
                if (!bi) break;
                const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
                auto [p, z] = rotate_xy_anchor(orig.position, orig.base_z);
                bi->position = p;
                bi->base_z = z;
                if (axis == GizmoAxis::RotZ) bi->rotation_z = orig.rotation_z + angle;
                break;
            }
            default: break;
        }
    }
}

void Viewport3D::apply_drag_scale_uniform(double factor) {
    if (factor < 1e-4) factor = 1e-4;
    const Eigen::Vector2d pv{gizmo_pivot_.x(), gizmo_pivot_.y()};
    const double pz = gizmo_pivot_.z();
    auto scale2 = [&](const Eigen::Vector2d& p) {
        return pv + (p - pv) * factor;
    };
    auto scale_z = [&](double z) {
        return pz + (z - pz) * factor;
    };

    for (const auto& sel : plan_view_.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;
        switch (sel.kind) {
            case SelectKind::Wall: {
                auto* w = document_.find_wall(sel.id);
                if (!w) break;
                const auto& orig = std::get<cadino::core::Wall>(it->second);
                w->start = scale2(orig.start);
                w->end = scale2(orig.end);
                w->thickness = orig.thickness * factor;
                w->height = orig.height * factor;
                break;
            }
            case SelectKind::Box: {
                auto* b = document_.find_box(sel.id);
                if (!b) break;
                const auto& orig = std::get<cadino::core::Box>(it->second);
                b->position = scale2(orig.position);
                b->base_z = scale_z(orig.base_z);
                b->size_xy = orig.size_xy * factor;
                b->height = orig.height * factor;
                break;
            }
            case SelectKind::Cylinder: {
                auto* cy = document_.find_cylinder(sel.id);
                if (!cy) break;
                const auto& orig = std::get<cadino::core::Cylinder>(it->second);
                cy->position = scale2(orig.position);
                cy->base_z = scale_z(orig.base_z);
                cy->radius = orig.radius * factor;
                cy->height = orig.height * factor;
                break;
            }
            case SelectKind::Block: {
                auto* bk = document_.find_block(sel.id);
                if (!bk) break;
                const auto& orig = std::get<cadino::core::Block>(it->second);
                bk->position = scale2(orig.position);
                bk->base_z = scale_z(orig.base_z);
                break;
            }
            case SelectKind::BlockInstance: {
                auto* bi = document_.find_block_instance(sel.id);
                if (!bi) break;
                const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
                bi->position = scale2(orig.position);
                bi->base_z = scale_z(orig.base_z);
                break;
            }
            default: break;
        }
    }
}

void Viewport3D::apply_drag_scale_axis(GizmoAxis axis, double factor) {
    if (factor < 1e-4) factor = 1e-4;
    const Eigen::Vector3d pv{gizmo_pivot_.x(), gizmo_pivot_.y(), gizmo_pivot_.z()};
    auto scale_xyz = [&](Eigen::Vector3d p) {
        p -= pv;
        switch (axis) {
            case GizmoAxis::ScaleX: p.x() *= factor; break;
            case GizmoAxis::ScaleY: p.y() *= factor; break;
            case GizmoAxis::ScaleZ: p.z() *= factor; break;
            default: break;
        }
        return pv + p;
    };

    for (const auto& sel : plan_view_.selections()) {
        const auto it = drag_originals_.find(sel.id);
        if (it == drag_originals_.end()) continue;
        switch (sel.kind) {
            case SelectKind::Wall: {
                auto* w = document_.find_wall(sel.id);
                if (!w) break;
                const auto& orig = std::get<cadino::core::Wall>(it->second);
                const Eigen::Vector3d s = scale_xyz({orig.start.x(), orig.start.y(), 0});
                const Eigen::Vector3d e = scale_xyz({orig.end.x(), orig.end.y(), 0});
                w->start = {s.x(), s.y()};
                w->end = {e.x(), e.y()};
                if (axis == GizmoAxis::ScaleZ) w->height = orig.height * factor;
                break;
            }
            case SelectKind::Box: {
                auto* b = document_.find_box(sel.id);
                if (!b) break;
                const auto& orig = std::get<cadino::core::Box>(it->second);
                const Eigen::Vector3d p = scale_xyz({orig.position.x(), orig.position.y(), orig.base_z});
                b->position = {p.x(), p.y()};
                b->base_z = p.z();
                if (axis == GizmoAxis::ScaleZ) b->height = orig.height * factor;
                break;
            }
            case SelectKind::Cylinder: {
                auto* cy = document_.find_cylinder(sel.id);
                if (!cy) break;
                const auto& orig = std::get<cadino::core::Cylinder>(it->second);
                const Eigen::Vector3d p = scale_xyz({orig.position.x(), orig.position.y(), orig.base_z});
                cy->position = {p.x(), p.y()};
                cy->base_z = p.z();
                if (axis == GizmoAxis::ScaleZ) cy->height = orig.height * factor;
                break;
            }
            case SelectKind::Block: {
                auto* bk = document_.find_block(sel.id);
                if (!bk) break;
                const auto& orig = std::get<cadino::core::Block>(it->second);
                const Eigen::Vector3d p = scale_xyz({orig.position.x(), orig.position.y(), orig.base_z});
                bk->position = {p.x(), p.y()};
                bk->base_z = p.z();
                break;
            }
            case SelectKind::BlockInstance: {
                auto* bi = document_.find_block_instance(sel.id);
                if (!bi) break;
                const auto& orig = std::get<cadino::core::BlockInstance>(it->second);
                const Eigen::Vector3d p = scale_xyz({orig.position.x(), orig.position.y(), orig.base_z});
                bi->position = {p.x(), p.y()};
                bi->base_z = p.z();
                break;
            }
            default: break;
        }
    }
}

bool Viewport3D::axis_param(const Ray& r, const QVector3D& pivot,
                            const QVector3D& axis, float& s_out) const {
    const QVector3D w = r.origin - pivot;
    const float b = QVector3D::dotProduct(r.direction, axis);
    const float denom = 1.0f - b * b;
    if (std::abs(denom) < 1e-4f) return false;
    const float d = QVector3D::dotProduct(r.direction, w);
    const float e = QVector3D::dotProduct(axis, w);
    s_out = (e - b * d) / denom;
    return true;
}

Viewport3D::GizmoAxis Viewport3D::pick_gizmo_axis(QPointF screen_pos) const {
    QVector3D pivot;
    if (!selection_centroid(pivot)) return GizmoAxis::None;

    const QMatrix4x4 vp = projection_matrix() * view_matrix();
    auto project = [&](const QVector3D& world) -> QPointF {
        QVector4D clip = vp * QVector4D(world.x(), world.y(), world.z(), 1.0f);
        if (clip.w() < 1e-4f) return {-1e6f, -1e6f};
        clip /= clip.w();
        const float sx = (clip.x() + 1.0f) * 0.5f * static_cast<float>(width());
        const float sy = (1.0f - clip.y()) * 0.5f * static_cast<float>(height());
        return {sx, sy};
    };
    auto seg_dist = [](QPointF p, QPointF a, QPointF b) {
        const QPointF ab = b - a;
        const float ll = static_cast<float>(ab.x() * ab.x() + ab.y() * ab.y());
        if (ll < 1e-6f) {
            const QPointF d = p - a;
            return static_cast<float>(std::hypot(d.x(), d.y()));
        }
        const QPointF ap = p - a;
        float t = static_cast<float>((ap.x() * ab.x() + ap.y() * ab.y()) / ll);
        t = std::clamp(t, 0.0f, 1.0f);
        const QPointF q = a + ab * t;
        const QPointF d = p - q;
        return static_cast<float>(std::hypot(d.x(), d.y()));
    };

    const float len = gizmo_length();
    const QPointF p0 = project(pivot);
    const QPointF px = project(pivot + QVector3D(len, 0, 0));
    const QPointF py = project(pivot + QVector3D(0, len, 0));
    const QPointF pz = project(pivot + QVector3D(0, 0, len));

    constexpr float kHit = 9.0f;  // pixels
    GizmoAxis best = GizmoAxis::None;
    float best_d = kHit;
    const float dx = seg_dist(screen_pos, p0, px);
    const float dy = seg_dist(screen_pos, p0, py);
    const float dz = seg_dist(screen_pos, p0, pz);
    if (dx < best_d) { best_d = dx; best = GizmoAxis::X; }
    if (dy < best_d) { best_d = dy; best = GizmoAxis::Y; }
    if (dz < best_d) { best_d = dz; best = GizmoAxis::Z; }

    // Rotation rings — sample points along the ring in its plane.
    const float ring_r = len * 1.15f;
    constexpr int kRing = 48;
    auto ring_hit_test = [&](GizmoAxis kind, auto point_at) {
        for (int i = 0; i < kRing; ++i) {
            const float a = float(i) / float(kRing) * 6.28318530718f;
            const QPointF rp = project(point_at(a));
            const float d = static_cast<float>(std::hypot(screen_pos.x() - rp.x(),
                                                           screen_pos.y() - rp.y()));
            if (d < best_d) { best_d = d; best = kind; }
        }
    };
    ring_hit_test(GizmoAxis::RotZ, [&](float a) {
        return pivot + QVector3D(std::cos(a) * ring_r, std::sin(a) * ring_r, 0.0f);
    });
    ring_hit_test(GizmoAxis::RotX, [&](float a) {
        return pivot + QVector3D(0.0f, std::cos(a) * ring_r, std::sin(a) * ring_r);
    });
    ring_hit_test(GizmoAxis::RotY, [&](float a) {
        return pivot + QVector3D(std::sin(a) * ring_r, 0.0f, std::cos(a) * ring_r);
    });

    // Per-axis non-uniform scale handles at the tip-plus of each arrow.
    auto scale_hit = [&](GizmoAxis kind, QVector3D dir) {
        const QPointF sp = project(pivot + dir * (len * 1.35f));
        const float d = static_cast<float>(std::hypot(screen_pos.x() - sp.x(),
                                                       screen_pos.y() - sp.y()));
        if (d < std::max(best_d, 11.0f)) { best_d = d; best = kind; }
    };
    scale_hit(GizmoAxis::ScaleX, {1.0f, 0.0f, 0.0f});
    scale_hit(GizmoAxis::ScaleY, {0.0f, 1.0f, 0.0f});
    scale_hit(GizmoAxis::ScaleZ, {0.0f, 0.0f, 1.0f});

    // Uniform scale handle: small cross at the center.
    const QPointF up = project(pivot);
    const float du = static_cast<float>(std::hypot(screen_pos.x() - up.x(),
                                                    screen_pos.y() - up.y()));
    if (du < 8.0f && du < best_d) { best = GizmoAxis::ScaleUniform; }

    return best;
}

void Viewport3D::render_gizmo() {
    QVector3D pivot;
    if (!selection_centroid(pivot)) return;
    gizmo_pivot_ = pivot;
    const float len = gizmo_length();

    auto vert = [](QVector3D p, QVector3D color) {
        return Vertex{p.x(), p.y(), p.z(), 0.0f, 0.0f, 1.0f,
                      color.x(), color.y(), color.z(),
                      1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    };
    auto push_axis = [&](std::vector<Vertex>& v, QVector3D dir, QVector3D color, bool active) {
        const QVector3D tip = pivot + dir * len;
        const QVector3D c = active ? QVector3D(1.0f, 0.95f, 0.20f) : color;
        v.push_back(vert(pivot, c));
        v.push_back(vert(tip, c));
        // arrowhead: short crossbar at tip perpendicular to axis
        const QVector3D perp1 =
            std::abs(dir.z()) < 0.9f ? QVector3D::crossProduct(dir, {0, 0, 1}).normalized()
                                     : QVector3D{1, 0, 0};
        const QVector3D perp2 = QVector3D::crossProduct(dir, perp1).normalized();
        const float head = len * 0.18f;
        const QVector3D base = tip - dir * head;
        const QVector3D h1 = base + perp1 * (head * 0.5f);
        const QVector3D h2 = base - perp1 * (head * 0.5f);
        const QVector3D h3 = base + perp2 * (head * 0.5f);
        const QVector3D h4 = base - perp2 * (head * 0.5f);
        v.push_back(vert(tip, c)); v.push_back(vert(h1, c));
        v.push_back(vert(tip, c)); v.push_back(vert(h2, c));
        v.push_back(vert(tip, c)); v.push_back(vert(h3, c));
        v.push_back(vert(tip, c)); v.push_back(vert(h4, c));
    };

    std::vector<Vertex> verts;
    verts.reserve(160);
    push_axis(verts, {1, 0, 0}, {0.95f, 0.25f, 0.25f}, active_axis_ == GizmoAxis::X);
    push_axis(verts, {0, 1, 0}, {0.25f, 0.90f, 0.30f}, active_axis_ == GizmoAxis::Y);
    push_axis(verts, {0, 0, 1}, {0.25f, 0.45f, 0.95f}, active_axis_ == GizmoAxis::Z);

    // Rotation rings — one per axis, lying in the plane orthogonal to that axis.
    constexpr int kRing = 48;
    const float rr = len * 1.15f;
    auto push_ring = [&](GizmoAxis kind, QVector3D base_color, auto point_at) {
        const QVector3D c = active_axis_ == kind
            ? QVector3D(1.0f, 0.95f, 0.20f) : base_color;
        QVector3D prev = point_at(0.0f);
        for (int i = 1; i <= kRing; ++i) {
            const float a = float(i) / float(kRing) * 6.28318530718f;
            const QVector3D p = point_at(a);
            verts.push_back(vert(prev, c));
            verts.push_back(vert(p, c));
            prev = p;
        }
    };
    push_ring(GizmoAxis::RotZ, {0.30f, 0.85f, 0.85f}, [&](float a) {
        return pivot + QVector3D(std::cos(a) * rr, std::sin(a) * rr, 0.0f);
    });
    push_ring(GizmoAxis::RotX, {0.95f, 0.45f, 0.45f}, [&](float a) {
        return pivot + QVector3D(0.0f, std::cos(a) * rr, std::sin(a) * rr);
    });
    push_ring(GizmoAxis::RotY, {0.45f, 0.95f, 0.55f}, [&](float a) {
        return pivot + QVector3D(std::sin(a) * rr, 0.0f, std::cos(a) * rr);
    });

    // Per-axis scale handles: a small wireframe cube past each arrow tip.
    auto push_scale_handle = [&](GizmoAxis kind, QVector3D dir, QVector3D base_color) {
        const QVector3D c = active_axis_ == kind
            ? QVector3D(1.0f, 0.95f, 0.20f) : base_color;
        const QVector3D h = pivot + dir * (len * 1.35f);
        const float sz = len * 0.10f;
        verts.push_back(vert(h + QVector3D(-sz, 0, 0), c));
        verts.push_back(vert(h + QVector3D( sz, 0, 0), c));
        verts.push_back(vert(h + QVector3D(0, -sz, 0), c));
        verts.push_back(vert(h + QVector3D(0,  sz, 0), c));
        verts.push_back(vert(h + QVector3D(0, 0, -sz), c));
        verts.push_back(vert(h + QVector3D(0, 0,  sz), c));
    };
    push_scale_handle(GizmoAxis::ScaleX, {1, 0, 0}, {0.95f, 0.45f, 0.45f});
    push_scale_handle(GizmoAxis::ScaleY, {0, 1, 0}, {0.45f, 0.95f, 0.55f});
    push_scale_handle(GizmoAxis::ScaleZ, {0, 0, 1}, {0.55f, 0.65f, 0.95f});

    // Uniform scale handle: small 3D cross at the pivot itself.
    {
        const QVector3D c = active_axis_ == GizmoAxis::ScaleUniform
            ? QVector3D(1.0f, 0.95f, 0.20f) : QVector3D(0.95f, 0.85f, 0.40f);
        const float sz = len * 0.07f;
        verts.push_back(vert(pivot + QVector3D(-sz, 0, 0), c));
        verts.push_back(vert(pivot + QVector3D( sz, 0, 0), c));
        verts.push_back(vert(pivot + QVector3D(0, -sz, 0), c));
        verts.push_back(vert(pivot + QVector3D(0,  sz, 0), c));
        verts.push_back(vert(pivot + QVector3D(0, 0, -sz), c));
        verts.push_back(vert(pivot + QVector3D(0, 0,  sz), c));
    }

    gizmo_vao_.bind();
    gizmo_vbo_.bind();
    gizmo_vbo_.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(Vertex)));
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(Vertex));
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(1, GL_FLOAT, sizeof(float) * 3, 3, sizeof(Vertex));
    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(2, GL_FLOAT, sizeof(float) * 6, 3, sizeof(Vertex));
    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(3, GL_FLOAT, sizeof(float) * 9, 2, sizeof(Vertex));
    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(4, GL_FLOAT, sizeof(float) * 11, 3, sizeof(Vertex));
    program_->enableAttributeArray(4);
    gizmo_vbo_.release();

    QMatrix4x4 identity;
    program_->setUniformValue("u_model", identity);
    program_->setUniformValue("u_shadow_mode", 2);
    program_->setUniformValue("u_has_texture", 0);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, static_cast<int>(verts.size()));
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    gizmo_vao_.release();
}

void Viewport3D::mousePressEvent(QMouseEvent* event) {
    drag_button_ = event->button();
    drag_last_ = event->position();

    if (event->button() == Qt::LeftButton) {
        // Gizmo first: if a selection exists and the click lands on a handle,
        // start a gizmo-constrained drag and bypass entity picking.
        const GizmoAxis axis = pick_gizmo_axis(event->position());
        if (axis != GizmoAxis::None) {
            active_axis_ = axis;
            QVector3D centroid;
            if (selection_centroid(centroid)) gizmo_pivot_ = centroid;
            const Ray ray = ray_from_screen(event->position());
            switch (axis) {
                case GizmoAxis::RotX:
                case GizmoAxis::RotY:
                case GizmoAxis::RotZ:
                    drag_rot_angle0_ = rotation_angle_for(axis, event->position());
                    break;
                case GizmoAxis::ScaleX:
                case GizmoAxis::ScaleY:
                case GizmoAxis::ScaleZ: {
                    gizmo_axis_dir_ = axis_direction(axis);
                    float s0 = 0.0f;
                    axis_param(ray, gizmo_pivot_, gizmo_axis_dir_, s0);
                    // Keep the signed initial param so the scale factor preserves
                    // direction even when the user clicks on the negative side.
                    drag_axis_s0_ = (std::abs(s0) < 1.0f) ? std::copysign(1.0f, s0 == 0 ? 1.0f : s0) : s0;
                    break;
                }
                case GizmoAxis::ScaleUniform: {
                    QVector3D ground;
                    if (ray_ground_intersection(ray, ground)) {
                        const double d = std::hypot(
                            static_cast<double>(ground.x() - gizmo_pivot_.x()),
                            static_cast<double>(ground.y() - gizmo_pivot_.y()));
                        drag_scale_dist0_ = std::max(d, 1.0);
                    }
                    break;
                }
                case GizmoAxis::X:
                case GizmoAxis::Y:
                case GizmoAxis::Z: {
                    gizmo_axis_dir_ = axis_direction(axis);
                    float s0 = 0.0f;
                    axis_param(ray, gizmo_pivot_, gizmo_axis_dir_, s0);
                    drag_axis_s0_ = s0;
                    break;
                }
                default: break;
            }
            entity_dragging_ = true;
            capture_drag_originals();
            update();
            return;
        }

        const Selection hit = pick_at_screen(event->position());
        const bool shift = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
        if (hit.valid()) {
            if (shift) {
                plan_view_.toggle_selection(hit);
            } else if (!plan_view_.is_selected(hit)) {
                plan_view_.set_selections({hit});
            }
            entity_dragging_ = true;
            active_axis_ = GizmoAxis::None;
            QVector3D ground;
            if (ray_ground_intersection(ray_from_screen(event->position()), ground)) {
                drag_ground_start_ = ground;
            }
            capture_drag_originals();
            update();
            return;
        }
        if (!shift) plan_view_.clear_selection();
    }
    setCursor(Qt::ClosedHandCursor);
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event) {
    if (entity_dragging_ &&
        (active_axis_ == GizmoAxis::RotX || active_axis_ == GizmoAxis::RotY ||
         active_axis_ == GizmoAxis::RotZ)) {
        bool ok = false;
        const double a = rotation_angle_for(active_axis_, event->position(), &ok);
        if (ok) {
            apply_drag_rotation(active_axis_, a - drag_rot_angle0_);
            update();
            plan_view_.update();
        }
        return;
    }
    if (entity_dragging_ && active_axis_ == GizmoAxis::ScaleUniform) {
        QVector3D ground;
        if (ray_ground_intersection(ray_from_screen(event->position()), ground)) {
            const double d = std::hypot(ground.x() - gizmo_pivot_.x(),
                                        ground.y() - gizmo_pivot_.y());
            apply_drag_scale_uniform(d / drag_scale_dist0_);
            update();
            plan_view_.update();
        }
        return;
    }
    if (entity_dragging_ &&
        (active_axis_ == GizmoAxis::ScaleX || active_axis_ == GizmoAxis::ScaleY ||
         active_axis_ == GizmoAxis::ScaleZ)) {
        float s_now = 0.0f;
        if (axis_param(ray_from_screen(event->position()), gizmo_pivot_,
                       gizmo_axis_dir_, s_now)) {
            const double factor = static_cast<double>(s_now) /
                                  static_cast<double>(drag_axis_s0_);
            apply_drag_scale_axis(active_axis_, factor);
            update();
            plan_view_.update();
        }
        return;
    }
    if (entity_dragging_ && active_axis_ != GizmoAxis::None) {
        float s_now;
        if (axis_param(ray_from_screen(event->position()), gizmo_pivot_,
                       gizmo_axis_dir_, s_now)) {
            const QVector3D delta = gizmo_axis_dir_ * (s_now - drag_axis_s0_);
            apply_drag_delta(delta);
            update();
            plan_view_.update();
        }
        return;
    }

    if (entity_dragging_) {
        QVector3D ground_now;
        if (ray_ground_intersection(ray_from_screen(event->position()), ground_now)) {
            const QVector3D delta = ground_now - drag_ground_start_;
            apply_drag_delta(QVector3D(delta.x(), delta.y(), 0.0f));
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
        active_axis_ = GizmoAxis::None;
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
