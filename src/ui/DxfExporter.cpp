#include "DxfExporter.hpp"

#include <cmath>
#include <numbers>
#include <vector>

#include <QPointF>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include "OcctShapes.hpp"
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

    void text(QPointF p, double height, double rotation_deg, const QString& str,
              const QString& layer = "0") {
        code(0, QStringLiteral("TEXT"));
        code(8, layer);
        code(10, p.x()); code(20, p.y()); code(30, 0.0);
        code(40, height);
        code(1, str);
        code(50, rotation_deg);
        code(72, 1);  // horizontal align: center
        code(11, p.x()); code(21, p.y()); code(31, 0.0);
        code(73, 0);  // vertical align: baseline
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

    for (const auto& [id, t] : doc.texts()) {
        const double rot_deg = t.rotation_z * 180.0 / std::numbers::pi;
        dxf.text({t.position.x(), t.position.y()}, t.height, rot_deg,
                 QString::fromStdString(t.text), "TEXT");
    }

    for (const auto& [id, l] : doc.leaders()) {
        dxf.line({l.anchor.x(), l.anchor.y()},
                 {l.text_position.x(), l.text_position.y()}, "LEADERS");
        // Tail under text
        dxf.line({l.text_position.x(), l.text_position.y()},
                 {l.text_position.x() + 100.0, l.text_position.y()}, "LEADERS");
        dxf.text({l.text_position.x(), l.text_position.y()}, l.height, 0.0,
                 QString::fromStdString(l.text), "LEADERS");
    }

    for (const auto& [id, ad] : doc.angular_dims()) {
        const double a1 = std::atan2(ad.p1.y() - ad.vertex.y(),
                                     ad.p1.x() - ad.vertex.x());
        const double sweep = ad.angle_rad();
        // Arc samples emitted as a LWPOLYLINE.
        constexpr int kSamples = 36;
        std::vector<QPointF> arc;
        arc.reserve(kSamples + 1);
        for (int i = 0; i <= kSamples; ++i) {
            const double t = static_cast<double>(i) / kSamples;
            const double ang = a1 + sweep * t;
            arc.emplace_back(ad.vertex.x() + std::cos(ang) * ad.radius,
                             ad.vertex.y() + std::sin(ang) * ad.radius);
        }
        dxf.lwpolyline(arc, false, "DIMENSIONS");
        // Extension lines.
        dxf.line({ad.vertex.x(), ad.vertex.y()},
                 {ad.vertex.x() + std::cos(a1) * ad.radius * 1.1,
                  ad.vertex.y() + std::sin(a1) * ad.radius * 1.1}, "DIMENSIONS");
        dxf.line({ad.vertex.x(), ad.vertex.y()},
                 {ad.vertex.x() + std::cos(a1 + sweep) * ad.radius * 1.1,
                  ad.vertex.y() + std::sin(a1 + sweep) * ad.radius * 1.1},
                 "DIMENSIONS");
        // Label.
        const double mid = a1 + sweep * 0.5;
        const QString label = ad.text_override.empty()
            ? QString::number(sweep * 180.0 / std::numbers::pi, 'f', 1) + QStringLiteral(" deg")
            : QString::fromStdString(ad.text_override);
        dxf.text({ad.vertex.x() + std::cos(mid) * (ad.radius + ad.text_height * 0.6),
                  ad.vertex.y() + std::sin(mid) * (ad.radius + ad.text_height * 0.6)},
                 ad.text_height, 0.0, label, "DIMENSIONS");
    }

    for (const auto& [id, d] : doc.dimensions()) {
        const double dx = d.end.x() - d.start.x();
        const double dy = d.end.y() - d.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double nx = -dy / len;
        const double ny = dx / len;
        const QPointF ds{d.start.x() + nx * d.offset, d.start.y() + ny * d.offset};
        const QPointF de{d.end.x()   + nx * d.offset, d.end.y()   + ny * d.offset};
        const QPointF wit_a_far{ds.x() + nx * 30.0, ds.y() + ny * 30.0};
        const QPointF wit_b_far{de.x() + nx * 30.0, de.y() + ny * 30.0};
        dxf.line({d.start.x(), d.start.y()}, wit_a_far, "DIMENSIONS");
        dxf.line({d.end.x(),   d.end.y()},   wit_b_far, "DIMENSIONS");
        dxf.line(ds, de, "DIMENSIONS");

        const QString label = d.text_override.empty()
            ? QString::number(len, 'f', 1) + QStringLiteral(" mm")
            : QString::fromStdString(d.text_override);
        const QPointF mid{(ds.x() + de.x()) * 0.5 + nx * d.text_height * 0.6,
                          (ds.y() + de.y()) * 0.5 + ny * d.text_height * 0.6};
        const double rot = std::atan2(dy, dx) * 180.0 / std::numbers::pi;
        const double upright = (rot > 90.0 || rot < -90.0) ? rot + 180.0 : rot;
        dxf.text(mid, d.text_height, upright, label, "DIMENSIONS");
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

// Build a projector frame for one of the four standard elevation views.
// gp_Ax2's Z axis is the view direction (camera-to-scene); the X axis is the
// drawing horizontal; the Y axis (auto-derived) is the drawing vertical.
gp_Ax2 elevation_axes(ElevationPlane plane) {
    const gp_Pnt origin(0, 0, 0);
    switch (plane) {
        case ElevationPlane::Front:
            // Camera in +Y looking -Y, drawing X = world X, drawing Y = world Z.
            return gp_Ax2(origin, gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
        case ElevationPlane::Back:
            // Camera in -Y looking +Y, drawing X = -world X, drawing Y = world Z.
            return gp_Ax2(origin, gp_Dir(0, 1, 0), gp_Dir(-1, 0, 0));
        case ElevationPlane::Left:
            // Camera in +X looking -X, drawing X = -world Y, drawing Y = world Z.
            return gp_Ax2(origin, gp_Dir(-1, 0, 0), gp_Dir(0, -1, 0));
        case ElevationPlane::Right:
            // Camera in -X looking +X, drawing X = +world Y, drawing Y = world Z.
            return gp_Ax2(origin, gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
    }
    return gp_Ax2(origin, gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
}

}  // namespace

namespace {

void emit_hlr_edges(DxfWriter& dxf, const TopoDS_Shape& comp, const QString& layer,
                    int samples = 32) {
    if (comp.IsNull()) return;
    TopExp_Explorer ex(comp, TopAbs_EDGE);
    for (; ex.More(); ex.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(ex.Current());
        BRepAdaptor_Curve curve(edge);
        const double t0 = curve.FirstParameter();
        const double t1 = curve.LastParameter();
        if (!(t1 > t0)) continue;
        std::vector<QPointF> pts;
        pts.reserve(static_cast<std::size_t>(samples + 1));
        for (int i = 0; i <= samples; ++i) {
            const double t = t0 + (t1 - t0) * static_cast<double>(i)
                                              / static_cast<double>(samples);
            const gp_Pnt p = curve.Value(t);
            pts.emplace_back(p.X(), p.Y());
        }
        if (pts.size() >= 2) dxf.lwpolyline(pts, false, layer);
    }
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

    // Aggregate every Brep-able primitive into one compound for HLR.
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);

    auto add = [&](const TopoDS_Shape& s) {
        if (!s.IsNull()) builder.Add(compound, s);
    };

    for (const auto& [id, w] : doc.walls())     add(shape_from_wall(w));
    for (const auto& [id, b] : doc.boxes())     add(shape_from_box(b));
    for (const auto& [id, c] : doc.cylinders()) add(shape_from_cylinder(c));
    for (const auto& [id, s] : doc.slabs())     add(shape_from_slab(s));
    for (const auto& [id, bl] : doc.blocks()) {
        for (const auto& local_b : bl.boxes)     add(shape_from_box(bl.world_box(local_b)));
        for (const auto& local_c : bl.cylinders) add(shape_from_cylinder(bl.world_cylinder(local_c)));
    }

    // Run Hidden Line Removal.
    Handle(HLRBRep_Algo) hlr = new HLRBRep_Algo();
    hlr->Add(compound);
    HLRAlgo_Projector projector(elevation_axes(plane));
    hlr->Projector(projector);
    hlr->Update();
    hlr->Hide();

    HLRBRep_HLRToShape extractor(hlr);
    emit_hlr_edges(dxf, extractor.VCompound(),         "VISIBLE_SHARP");
    emit_hlr_edges(dxf, extractor.OutLineVCompound(),  "VISIBLE_OUTLINE");
    emit_hlr_edges(dxf, extractor.Rg1LineVCompound(),  "VISIBLE_SMOOTH");
    emit_hlr_edges(dxf, extractor.HCompound(),         "HIDDEN_SHARP");
    emit_hlr_edges(dxf, extractor.OutLineHCompound(),  "HIDDEN_OUTLINE");

    dxf.end_entities();
    dxf.eof();
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

}  // namespace cadino::ui
