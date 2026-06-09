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

}  // namespace cadino::ui
