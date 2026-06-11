#include "DxfExporter.hpp"

#include <cmath>
#include <numbers>
#include <vector>

#include <QPointF>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

class DxfWriter {
public:
    explicit DxfWriter(QTextStream& s) : s_{s} {}

    void code(int g, double v) { s_ << g << '\n' << QString::number(v, 'f', 4) << '\n'; }
    void code(int g, int v)    { s_ << g << '\n' << v << '\n'; }
    void code(int g, const QString& v) { s_ << g << '\n' << v << '\n'; }

    void header() {
        code(0, QStringLiteral("SECTION"));
        code(2, QStringLiteral("HEADER"));
        code(9, QStringLiteral("$INSUNITS"));
        code(70, 4);  // millimeters
        code(0, QStringLiteral("ENDSEC"));
    }

    void begin_entities() {
        code(0, QStringLiteral("SECTION"));
        code(2, QStringLiteral("ENTITIES"));
    }

    void end_entities() { code(0, QStringLiteral("ENDSEC")); }

    void eof() { code(0, QStringLiteral("EOF")); }

    void lwpolyline(const std::vector<QPointF>& pts, bool closed,
                    const QString& layer = "0") {
        if (pts.empty()) return;
        code(0, QStringLiteral("LWPOLYLINE"));
        code(8, layer);
        code(90, int(pts.size()));
        code(70, closed ? 1 : 0);
        for (const auto& p : pts) {
            code(10, p.x());
            code(20, p.y());
        }
    }

    void line(QPointF a, QPointF b, const QString& layer = "0") {
        code(0, QStringLiteral("LINE"));
        code(8, layer);
        code(10, a.x()); code(20, a.y()); code(30, 0.0);
        code(11, b.x()); code(21, b.y()); code(31, 0.0);
    }

    void circle(QPointF c, double r, const QString& layer = "0") {
        code(0, QStringLiteral("CIRCLE"));
        code(8, layer);
        code(10, c.x()); code(20, c.y()); code(30, 0.0);
        code(40, r);
    }

private:
    QTextStream& s_;
};

}  // namespace

bool export_document_as_dxf(const cadino::core::Document& doc, const QString& path,
                            QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setRealNumberPrecision(6);
    DxfWriter dxf(out);

    dxf.header();
    dxf.begin_entities();

    for (const auto& [id, w] : doc.walls()) {
        const double dx = w.end.x() - w.start.x();
        const double dy = w.end.y() - w.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double nx = -dy / len;
        const double ny = dx / len;
        const double off = w.thickness * 0.5;
        const QPointF s1{w.start.x() + nx * off, w.start.y() + ny * off};
        const QPointF e1{w.end.x() + nx * off, w.end.y() + ny * off};
        const QPointF s2{w.start.x() - nx * off, w.start.y() - ny * off};
        const QPointF e2{w.end.x() - nx * off, w.end.y() - ny * off};
        dxf.lwpolyline({s1, e1, e2, s2}, true, "WALLS");
    }

    for (const auto& [id, s] : doc.slabs()) {
        if (s.outline.size() < 3) continue;
        std::vector<QPointF> pts;
        pts.reserve(s.outline.size());
        for (const auto& v : s.outline) pts.emplace_back(v.x(), v.y());
        dxf.lwpolyline(pts, true, "SLABS");
    }

    for (const auto& [id, b] : doc.boxes()) {
        const double hx = b.size_xy.x() * 0.5;
        const double hy = b.size_xy.y() * 0.5;
        const double c = std::cos(b.rotation_z);
        const double s = std::sin(b.rotation_z);
        const auto rot = [&](double x, double y) {
            return QPointF(b.position.x() + c * x - s * y,
                           b.position.y() + s * x + c * y);
        };
        dxf.lwpolyline({rot(-hx, -hy), rot(hx, -hy), rot(hx, hy), rot(-hx, hy)},
                       true, "FURNITURE");
    }

    for (const auto& [id, c] : doc.cylinders()) {
        dxf.circle({c.position.x(), c.position.y()}, c.radius, "FURNITURE");
    }

    for (const auto& [id, d] : doc.doors()) {
        const auto* w = doc.find_wall(d.host_wall);
        if (!w) continue;
        const double dx = w->end.x() - w->start.x();
        const double dy = w->end.y() - w->start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double ux = dx / len, uy = dy / len;
        const double cx = w->start.x() + ux * d.position_along;
        const double cy = w->start.y() + uy * d.position_along;
        const QPointF p1{cx - ux * d.width * 0.5, cy - uy * d.width * 0.5};
        const QPointF p2{cx + ux * d.width * 0.5, cy + uy * d.width * 0.5};
        dxf.line(p1, p2, "DOORS");
    }

    for (const auto& [id, win] : doc.windows()) {
        const auto* w = doc.find_wall(win.host_wall);
        if (!w) continue;
        const double dx = w->end.x() - w->start.x();
        const double dy = w->end.y() - w->start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double ux = dx / len, uy = dy / len;
        const double cx = w->start.x() + ux * win.position_along;
        const double cy = w->start.y() + uy * win.position_along;
        const QPointF p1{cx - ux * win.width * 0.5, cy - uy * win.width * 0.5};
        const QPointF p2{cx + ux * win.width * 0.5, cy + uy * win.width * 0.5};
        dxf.line(p1, p2, "WINDOWS");
    }

    dxf.end_entities();
    dxf.eof();

    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

namespace {

// Projects a world-space point (x, y, z) to a 2D drawing point depending on
// which elevation we are looking at. The conventions mirror standard
// orthographic CAD views: the horizontal axis is to the right, the vertical
// axis points up (Z is always vertical).
QPointF project(double x, double y, double z, ElevationPlane plane) {
    switch (plane) {
        case ElevationPlane::Front: return { x,  z};
        case ElevationPlane::Back:  return {-x,  z};
        case ElevationPlane::Left:  return {-y,  z};
        case ElevationPlane::Right: return { y,  z};
    }
    return { x, z};
}

struct OBB {
    double cx, cy;
    double hx, hy;
    double yaw;
    double z0, z1;
    QString layer;
};

void emit_obb(DxfWriter& dxf, const OBB& b, ElevationPlane plane) {
    const double c = std::cos(b.yaw);
    const double s = std::sin(b.yaw);
    auto P = [&](double lx, double ly, double z) {
        const double wx = b.cx + c * lx - s * ly;
        const double wy = b.cy + s * lx + c * ly;
        return project(wx, wy, z, plane);
    };
    const QPointF b0 = P(-b.hx, -b.hy, b.z0);
    const QPointF b1 = P( b.hx, -b.hy, b.z0);
    const QPointF b2 = P( b.hx,  b.hy, b.z0);
    const QPointF b3 = P(-b.hx,  b.hy, b.z0);
    const QPointF t0 = P(-b.hx, -b.hy, b.z1);
    const QPointF t1 = P( b.hx, -b.hy, b.z1);
    const QPointF t2 = P( b.hx,  b.hy, b.z1);
    const QPointF t3 = P(-b.hx,  b.hy, b.z1);
    // 12 edges
    dxf.line(b0, b1, b.layer); dxf.line(b1, b2, b.layer);
    dxf.line(b2, b3, b.layer); dxf.line(b3, b0, b.layer);
    dxf.line(t0, t1, b.layer); dxf.line(t1, t2, b.layer);
    dxf.line(t2, t3, b.layer); dxf.line(t3, t0, b.layer);
    dxf.line(b0, t0, b.layer); dxf.line(b1, t1, b.layer);
    dxf.line(b2, t2, b.layer); dxf.line(b3, t3, b.layer);
}

void emit_cylinder(DxfWriter& dxf, double cx, double cy, double r, double z0, double z1,
                   ElevationPlane plane, const QString& layer) {
    // Project bottom and top circles as ellipses approximated by 24-gon
    // polylines. The two silhouette generators come from the tangent points.
    const int segs = 24;
    std::vector<QPointF> bot, top;
    bot.reserve(segs + 1);
    top.reserve(segs + 1);
    for (int i = 0; i <= segs; ++i) {
        const double a = 2.0 * std::numbers::pi * static_cast<double>(i) / segs;
        const double px = cx + r * std::cos(a);
        const double py = cy + r * std::sin(a);
        bot.push_back(project(px, py, z0, plane));
        top.push_back(project(px, py, z1, plane));
    }
    dxf.lwpolyline(bot, false, layer);
    dxf.lwpolyline(top, false, layer);
    // Silhouette generators — project the leftmost/rightmost points of the
    // unrotated circle relative to the view plane.
    QPointF s_left{}, s_right{};
    switch (plane) {
        case ElevationPlane::Front:
            s_left  = project(cx - r, cy, 0.0, plane);
            s_right = project(cx + r, cy, 0.0, plane);
            break;
        case ElevationPlane::Back:
            s_left  = project(cx + r, cy, 0.0, plane);
            s_right = project(cx - r, cy, 0.0, plane);
            break;
        case ElevationPlane::Left:
            s_left  = project(cx, cy + r, 0.0, plane);
            s_right = project(cx, cy - r, 0.0, plane);
            break;
        case ElevationPlane::Right:
            s_left  = project(cx, cy - r, 0.0, plane);
            s_right = project(cx, cy + r, 0.0, plane);
            break;
    }
    dxf.line(QPointF(s_left.x(), z0),  QPointF(s_left.x(), z1),  layer);
    dxf.line(QPointF(s_right.x(), z0), QPointF(s_right.x(), z1), layer);
}

}  // namespace

bool export_elevation_as_dxf(const cadino::core::Document& doc, ElevationPlane plane,
                             const QString& path, QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setRealNumberPrecision(6);
    DxfWriter dxf(out);
    dxf.header();
    dxf.begin_entities();

    // Walls as oriented boxes from start to end with height.
    for (const auto& [id, w] : doc.walls()) {
        const double dx = w.end.x() - w.start.x();
        const double dy = w.end.y() - w.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        OBB b{(w.start.x() + w.end.x()) * 0.5, (w.start.y() + w.end.y()) * 0.5,
              len * 0.5, w.thickness * 0.5,
              std::atan2(dy, dx),
              0.0, w.height, "WALLS"};
        emit_obb(dxf, b, plane);
    }

    for (const auto& [id, b] : doc.boxes()) {
        OBB obb{b.position.x(), b.position.y(),
                b.size_xy.x() * 0.5, b.size_xy.y() * 0.5,
                b.rotation_z, b.base_z, b.base_z + b.height,
                "FURNITURE"};
        emit_obb(dxf, obb, plane);
    }

    for (const auto& [id, c] : doc.cylinders()) {
        emit_cylinder(dxf, c.position.x(), c.position.y(), c.radius,
                      c.base_z, c.base_z + c.height, plane, "FURNITURE");
    }

    for (const auto& [id, s] : doc.slabs()) {
        if (s.outline.size() < 3) continue;
        double minx = s.outline[0].x(), miny = s.outline[0].y();
        double maxx = minx, maxy = miny;
        for (const auto& v : s.outline) {
            minx = std::min(minx, v.x()); miny = std::min(miny, v.y());
            maxx = std::max(maxx, v.x()); maxy = std::max(maxy, v.y());
        }
        OBB obb{(minx + maxx) * 0.5, (miny + maxy) * 0.5,
                (maxx - minx) * 0.5, (maxy - miny) * 0.5,
                0.0, s.level, s.level + s.thickness, "SLABS"};
        emit_obb(dxf, obb, plane);
    }

    for (const auto& [id, bl] : doc.blocks()) {
        for (const auto& local_b : bl.boxes) {
            const auto wb = bl.world_box(local_b);
            OBB obb{wb.position.x(), wb.position.y(),
                    wb.size_xy.x() * 0.5, wb.size_xy.y() * 0.5,
                    wb.rotation_z, wb.base_z, wb.base_z + wb.height,
                    "FURNITURE"};
            emit_obb(dxf, obb, plane);
        }
        for (const auto& local_c : bl.cylinders) {
            const auto wc = bl.world_cylinder(local_c);
            emit_cylinder(dxf, wc.position.x(), wc.position.y(), wc.radius,
                          wc.base_z, wc.base_z + wc.height, plane, "FURNITURE");
        }
    }

    dxf.end_entities();
    dxf.eof();
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

}  // namespace cadino::ui
