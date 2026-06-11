#include "MeshExport.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include <QSaveFile>
#include <QTextStream>

#include <Eigen/Core>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

struct V3 { double x, y, z; };

struct Triangle {
    V3 a, b, c;
    V3 n;
};

V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 cross(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
double length(V3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
V3 normalize(V3 a) {
    const double l = length(a);
    return l > 1e-12 ? V3{a.x / l, a.y / l, a.z / l} : V3{0, 0, 1};
}

void push_tri(std::vector<Triangle>& out, V3 a, V3 b, V3 c) {
    Triangle t{a, b, c, normalize(cross(sub(b, a), sub(c, a)))};
    out.push_back(t);
}

void push_quad(std::vector<Triangle>& out, V3 a, V3 b, V3 c, V3 d) {
    push_tri(out, a, b, c);
    push_tri(out, a, c, d);
}

void tessellate_obb(std::vector<Triangle>& out, double cx, double cy,
                    double hx, double hy, double yaw, double z0, double z1) {
    const double cs = std::cos(yaw);
    const double sn = std::sin(yaw);
    auto P = [&](double lx, double ly, double z) {
        return V3{cx + cs * lx - sn * ly, cy + sn * lx + cs * ly, z};
    };
    const V3 b0 = P(-hx, -hy, z0);
    const V3 b1 = P( hx, -hy, z0);
    const V3 b2 = P( hx,  hy, z0);
    const V3 b3 = P(-hx,  hy, z0);
    const V3 t0 = P(-hx, -hy, z1);
    const V3 t1 = P( hx, -hy, z1);
    const V3 t2 = P( hx,  hy, z1);
    const V3 t3 = P(-hx,  hy, z1);
    push_quad(out, t0, t1, t2, t3);  // top
    push_quad(out, b3, b2, b1, b0);  // bottom (reverse winding)
    push_quad(out, b0, b1, t1, t0);  // -hy side
    push_quad(out, b1, b2, t2, t1);  // +hx side
    push_quad(out, b2, b3, t3, t2);  // +hy side
    push_quad(out, b3, b0, t0, t3);  // -hx side
}

void tessellate_cylinder(std::vector<Triangle>& out, double cx, double cy,
                         double r, double z0, double z1, int segments = 48) {
    const V3 top_c{cx, cy, z1};
    const V3 bot_c{cx, cy, z0};
    for (int i = 0; i < segments; ++i) {
        const double a0 = 2.0 * std::numbers::pi * static_cast<double>(i) / segments;
        const double a1 = 2.0 * std::numbers::pi * static_cast<double>(i + 1) / segments;
        const V3 bot0{cx + r * std::cos(a0), cy + r * std::sin(a0), z0};
        const V3 bot1{cx + r * std::cos(a1), cy + r * std::sin(a1), z0};
        const V3 top0{cx + r * std::cos(a0), cy + r * std::sin(a0), z1};
        const V3 top1{cx + r * std::cos(a1), cy + r * std::sin(a1), z1};
        push_quad(out, bot0, bot1, top1, top0);
        push_tri(out, top_c, top0, top1);
        push_tri(out, bot_c, bot1, bot0);
    }
}

void collect(const cadino::core::Document& doc, std::vector<Triangle>& out) {
    for (const auto& [id, w] : doc.walls()) {
        const double dx = w.end.x() - w.start.x();
        const double dy = w.end.y() - w.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double cx = (w.start.x() + w.end.x()) * 0.5;
        const double cy = (w.start.y() + w.end.y()) * 0.5;
        tessellate_obb(out, cx, cy, len * 0.5, w.thickness * 0.5,
                       std::atan2(dy, dx), 0.0, w.height);
    }
    for (const auto& [id, b] : doc.boxes()) {
        tessellate_obb(out, b.position.x(), b.position.y(),
                       b.size_xy.x() * 0.5, b.size_xy.y() * 0.5,
                       b.rotation_z, b.base_z, b.base_z + b.height);
    }
    for (const auto& [id, c] : doc.cylinders()) {
        tessellate_cylinder(out, c.position.x(), c.position.y(), c.radius,
                            c.base_z, c.base_z + c.height);
    }
    for (const auto& [id, s] : doc.slabs()) {
        if (s.outline.size() < 3) continue;
        double minx = s.outline[0].x(), miny = s.outline[0].y();
        double maxx = minx, maxy = miny;
        for (const auto& v : s.outline) {
            minx = std::min(minx, v.x()); miny = std::min(miny, v.y());
            maxx = std::max(maxx, v.x()); maxy = std::max(maxy, v.y());
        }
        tessellate_obb(out, (minx + maxx) * 0.5, (miny + maxy) * 0.5,
                       (maxx - minx) * 0.5, (maxy - miny) * 0.5,
                       0.0, s.level, s.level + s.thickness);
    }
    for (const auto& [id, m] : doc.meshes()) {
        for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const auto& p0 = m.positions[m.indices[i]];
            const auto& p1 = m.positions[m.indices[i + 1]];
            const auto& p2 = m.positions[m.indices[i + 2]];
            push_tri(out, {p0.x(), p0.y(), p0.z()},
                          {p1.x(), p1.y(), p1.z()},
                          {p2.x(), p2.y(), p2.z()});
        }
    }
    for (const auto& [id, bl] : doc.blocks()) {
        for (const auto& local_b : bl.boxes) {
            const auto b = bl.world_box(local_b);
            tessellate_obb(out, b.position.x(), b.position.y(),
                           b.size_xy.x() * 0.5, b.size_xy.y() * 0.5,
                           b.rotation_z, b.base_z, b.base_z + b.height);
        }
        for (const auto& local_c : bl.cylinders) {
            const auto c = bl.world_cylinder(local_c);
            tessellate_cylinder(out, c.position.x(), c.position.y(), c.radius,
                                c.base_z, c.base_z + c.height);
        }
    }
}

}  // namespace

bool export_as_obj(const cadino::core::Document& doc, const QString& path,
                   QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setRealNumberPrecision(6);

    std::vector<Triangle> tris;
    collect(doc, tris);

    out << "# Cadino OBJ export\n";
    out << "# triangles: " << tris.size() << "\n";

    // Write all vertex positions, then all normals, then faces. Each triangle
    // owns three vertices (no deduplication for simplicity — meshes round-trip
    // cleanly without welding seams that span material boundaries).
    for (const auto& t : tris) {
        out << "v " << t.a.x << ' ' << t.a.y << ' ' << t.a.z << '\n';
        out << "v " << t.b.x << ' ' << t.b.y << ' ' << t.b.z << '\n';
        out << "v " << t.c.x << ' ' << t.c.y << ' ' << t.c.z << '\n';
    }
    for (const auto& t : tris) {
        out << "vn " << t.n.x << ' ' << t.n.y << ' ' << t.n.z << '\n';
    }
    for (std::size_t i = 0; i < tris.size(); ++i) {
        const auto v = static_cast<int>(i * 3 + 1);
        const auto n = static_cast<int>(i + 1);
        out << "f " << v     << "//" << n << ' '
                    << v + 1 << "//" << n << ' '
                    << v + 2 << "//" << n << '\n';
    }

    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool export_as_stl(const cadino::core::Document& doc, const QString& path,
                   QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setRealNumberPrecision(6);

    std::vector<Triangle> tris;
    collect(doc, tris);

    out << "solid Cadino\n";
    for (const auto& t : tris) {
        out << "  facet normal " << t.n.x << ' ' << t.n.y << ' ' << t.n.z << '\n';
        out << "    outer loop\n";
        out << "      vertex " << t.a.x << ' ' << t.a.y << ' ' << t.a.z << '\n';
        out << "      vertex " << t.b.x << ' ' << t.b.y << ' ' << t.b.z << '\n';
        out << "      vertex " << t.c.x << ' ' << t.c.y << ' ' << t.c.z << '\n';
        out << "    endloop\n";
        out << "  endfacet\n";
    }
    out << "endsolid Cadino\n";

    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

}  // namespace cadino::ui
