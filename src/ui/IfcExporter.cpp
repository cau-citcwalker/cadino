#include "IfcExporter.hpp"

#include <cmath>
#include <vector>

#include <QDateTime>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

// IFC compressed-GUID alphabet. 22 chars of base64 = 132 bits, encoding the
// 128-bit RFC 4122 UUID. We just generate random base64-safe strings here;
// IFC tools accept any 22-character sequence from this alphabet.
constexpr char kIfcAlphabet[65] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$";

QString new_ifc_guid() {
    auto* rng = QRandomGenerator::global();
    QString g;
    g.reserve(22);
    for (int i = 0; i < 22; ++i) {
        g.append(QChar(kIfcAlphabet[rng->bounded(64)]));
    }
    return g;
}

class IfcWriter {
public:
    explicit IfcWriter(QTextStream& s) : s_{s} {}

    // Emit a fully-formed body and return the assigned line number. Named
    // `write_line` because Qt #defines `emit` as a macro for signal emission.
    int write_line(const QString& body) {
        ++counter_;
        s_ << '#' << counter_ << '=' << body << ";\n";
        return counter_;
    }

    QString ref(int id) const { return QStringLiteral("#%1").arg(id); }
    QString quoted(const QString& s) const {
        QString out = s;
        out.replace('\'', "''");
        return QStringLiteral("'") + out + QStringLiteral("'");
    }
    QString flt(double v) const { return QString::number(v, 'f', 4); }
    QString guid() { return quoted(new_ifc_guid()); }

    int point2(double x, double y) {
        return write_line(QStringLiteral("IFCCARTESIANPOINT((%1,%2))").arg(flt(x), flt(y)));
    }
    int point3(double x, double y, double z) {
        return write_line(QStringLiteral("IFCCARTESIANPOINT((%1,%2,%3))")
                        .arg(flt(x), flt(y), flt(z)));
    }
    int direction3(double x, double y, double z) {
        return write_line(QStringLiteral("IFCDIRECTION((%1,%2,%3))")
                        .arg(flt(x), flt(y), flt(z)));
    }

    int axis2_placement_3d_default() {
        const int p = point3(0.0, 0.0, 0.0);
        return write_line(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)").arg(ref(p)));
    }

    int local_placement(int parent_or_zero, int axis_placement) {
        return write_line(QStringLiteral("IFCLOCALPLACEMENT(%1,%2)")
                        .arg(parent_or_zero ? ref(parent_or_zero) : QStringLiteral("$"),
                             ref(axis_placement)));
    }

    int polyline_closed(const std::vector<int>& point_ids) {
        QStringList refs;
        for (int p : point_ids) refs << ref(p);
        if (!point_ids.empty()) refs << ref(point_ids.front());
        return write_line(QStringLiteral("IFCPOLYLINE((%1))").arg(refs.join(',')));
    }
    int arbitrary_closed_profile(int polyline_id) {
        return write_line(QStringLiteral("IFCARBITRARYCLOSEDPROFILEDEF(.AREA.,$,%1)")
                        .arg(ref(polyline_id)));
    }
    int circle_profile(double radius) {
        const int axis2d = write_line("IFCAXIS2PLACEMENT2D(" + ref(point2(0, 0)) + ",$)");
        return write_line(QStringLiteral("IFCCIRCLEPROFILEDEF(.AREA.,$,%1,%2)")
                        .arg(ref(axis2d), flt(radius)));
    }
    int extruded_area_solid(int profile, double depth, double base_z) {
        const int origin = point3(0.0, 0.0, base_z);
        const int axis = write_line(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)").arg(ref(origin)));
        const int dir = direction3(0.0, 0.0, 1.0);
        return write_line(QStringLiteral("IFCEXTRUDEDAREASOLID(%1,%2,%3,%4)")
                        .arg(ref(profile), ref(axis), ref(dir), flt(depth)));
    }
    int shape_representation(int context, int solid) {
        return write_line(QStringLiteral(
                        "IFCSHAPEREPRESENTATION(%1,'Body','SweptSolid',(%2))")
                        .arg(ref(context), ref(solid)));
    }
    int product_definition_shape(int rep) {
        return write_line(QStringLiteral("IFCPRODUCTDEFINITIONSHAPE($,$,(%1))").arg(ref(rep)));
    }

private:
    QTextStream& s_;
    int counter_{0};
};

}  // namespace

bool export_document_as_ifc(const cadino::core::Document& doc, const QString& path,
                            QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setRealNumberPrecision(6);

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    out << "ISO-10303-21;\n";
    out << "HEADER;\n";
    out << "FILE_DESCRIPTION(('ViewDefinition [CoordinationView]'),'2;1');\n";
    out << "FILE_NAME('" << QFileInfo(path).fileName() << "','" << timestamp
        << "',('Cadino'),('Cadino'),'IFC4 by Cadino','Cadino','');\n";
    out << "FILE_SCHEMA(('IFC4'));\n";
    out << "ENDSEC;\n";
    out << "DATA;\n";

    IfcWriter w(out);

    const int person = w.write_line("IFCPERSON($,$,'Cadino User',$,$,$,$,$)");
    const int organization = w.write_line("IFCORGANIZATION($,'Cadino',$,$,$)");
    const int person_org = w.write_line(QStringLiteral("IFCPERSONANDORGANIZATION(%1,%2,$)")
                                       .arg(w.ref(person), w.ref(organization)));
    const int application = w.write_line(QStringLiteral(
                                       "IFCAPPLICATION(%1,'0.1','Cadino','Cadino')")
                                       .arg(w.ref(organization)));
    const QString now_seconds = QString::number(QDateTime::currentSecsSinceEpoch());
    const int owner_history = w.write_line(QStringLiteral(
                                         "IFCOWNERHISTORY(%1,%2,$,.ADDED.,%3,%1,%2,%3)")
                                         .arg(w.ref(person_org), w.ref(application),
                                              now_seconds));

    // Units: millimetres, radians, square mm.
    const int unit_len = w.write_line("IFCSIUNIT(*,.LENGTHUNIT.,.MILLI.,.METRE.)");
    const int unit_area = w.write_line("IFCSIUNIT(*,.AREAUNIT.,.MILLI.,.SQUARE_METRE.)");
    const int unit_vol = w.write_line("IFCSIUNIT(*,.VOLUMEUNIT.,.MILLI.,.CUBIC_METRE.)");
    const int unit_ang = w.write_line("IFCSIUNIT(*,.PLANEANGLEUNIT.,$,.RADIAN.)");
    const int units = w.write_line(QStringLiteral(
                                 "IFCUNITASSIGNMENT((%1,%2,%3,%4))")
                                 .arg(w.ref(unit_len), w.ref(unit_area),
                                      w.ref(unit_vol), w.ref(unit_ang)));

    const int world_origin = w.point3(0, 0, 0);
    const int world_axis = w.write_line(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)")
                                       .arg(w.ref(world_origin)));
    const int model_context = w.write_line(QStringLiteral(
                                         "IFCGEOMETRICREPRESENTATIONCONTEXT($,'Model',3,1.0E-5,%1,$)")
                                         .arg(w.ref(world_axis)));

    const int project = w.write_line(QStringLiteral(
                                   "IFCPROJECT(%1,%2,'Cadino Project',$,$,$,$,(%3),%4)")
                                   .arg(w.guid(), w.ref(owner_history),
                                        w.ref(model_context), w.ref(units)));

    const int site_place = w.local_placement(0, w.axis2_placement_3d_default());
    const int site = w.write_line(QStringLiteral(
                                "IFCSITE(%1,%2,'Site',$,$,%3,$,$,.ELEMENT.,$,$,$,$,$)")
                                .arg(w.guid(), w.ref(owner_history),
                                     w.ref(site_place)));

    const int bldg_place = w.local_placement(site_place, w.axis2_placement_3d_default());
    const int building = w.write_line(QStringLiteral(
                                    "IFCBUILDING(%1,%2,'Building',$,$,%3,$,$,.ELEMENT.,$,$,$)")
                                    .arg(w.guid(), w.ref(owner_history),
                                         w.ref(bldg_place)));

    const int storey_place = w.local_placement(bldg_place, w.axis2_placement_3d_default());
    const int storey = w.write_line(QStringLiteral(
                                  "IFCBUILDINGSTOREY(%1,%2,'Storey',$,$,%3,$,$,.ELEMENT.,0.)")
                                  .arg(w.guid(), w.ref(owner_history),
                                       w.ref(storey_place)));

    w.write_line(QStringLiteral("IFCRELAGGREGATES(%1,%2,$,$,%3,(%4))")
               .arg(w.guid(), w.ref(owner_history), w.ref(project), w.ref(site)));
    w.write_line(QStringLiteral("IFCRELAGGREGATES(%1,%2,$,$,%3,(%4))")
               .arg(w.guid(), w.ref(owner_history), w.ref(site), w.ref(building)));
    w.write_line(QStringLiteral("IFCRELAGGREGATES(%1,%2,$,$,%3,(%4))")
               .arg(w.guid(), w.ref(owner_history), w.ref(building), w.ref(storey)));

    std::vector<int> hosted_products;

    auto emit_extruded_product = [&](const QString& ifc_kind,
                                     const std::vector<std::pair<double, double>>& world_poly,
                                     double base_z, double height,
                                     const QString& name) -> int {
        if (world_poly.size() < 3 || height <= 0.0) return 0;
        std::vector<int> pts;
        pts.reserve(world_poly.size());
        for (const auto& [x, y] : world_poly) pts.push_back(w.point2(x, y));
        const int poly = w.polyline_closed(pts);
        const int profile = w.arbitrary_closed_profile(poly);
        const int solid = w.extruded_area_solid(profile, height, base_z);
        const int rep = w.shape_representation(model_context, solid);
        const int prod_shape = w.product_definition_shape(rep);
        const int placement = w.local_placement(storey_place, w.axis2_placement_3d_default());
        return w.write_line(QStringLiteral("%1(%2,%3,%4,$,$,%5,%6,$,$)")
                          .arg(ifc_kind, w.guid(), w.ref(owner_history),
                               w.quoted(name), w.ref(placement), w.ref(prod_shape)));
    };

    auto emit_circle_product = [&](const QString& ifc_kind, double cx, double cy,
                                   double radius, double base_z, double height,
                                   const QString& name) -> int {
        if (radius <= 0.0 || height <= 0.0) return 0;
        const int profile = w.circle_profile(radius);
        const int origin = w.point3(cx, cy, base_z);
        const int axis = w.write_line(QStringLiteral("IFCAXIS2PLACEMENT3D(%1,$,$)").arg(w.ref(origin)));
        const int dir = w.direction3(0.0, 0.0, 1.0);
        const int solid = w.write_line(QStringLiteral("IFCEXTRUDEDAREASOLID(%1,%2,%3,%4)")
                                      .arg(w.ref(profile), w.ref(axis), w.ref(dir),
                                           w.flt(height)));
        const int rep = w.shape_representation(model_context, solid);
        const int prod_shape = w.product_definition_shape(rep);
        const int placement = w.local_placement(storey_place, w.axis2_placement_3d_default());
        return w.write_line(QStringLiteral("%1(%2,%3,%4,$,$,%5,%6,$,$)")
                          .arg(ifc_kind, w.guid(), w.ref(owner_history),
                               w.quoted(name), w.ref(placement), w.ref(prod_shape)));
    };

    // Walls
    for (const auto& [id, wall] : doc.walls()) {
        const double dx = wall.end.x() - wall.start.x();
        const double dy = wall.end.y() - wall.start.y();
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double nx = -dy / len;
        const double ny = dx / len;
        const double half_t = wall.thickness * 0.5;
        const std::vector<std::pair<double, double>> poly{
            {wall.start.x() + nx * half_t, wall.start.y() + ny * half_t},
            {wall.end.x()   + nx * half_t, wall.end.y()   + ny * half_t},
            {wall.end.x()   - nx * half_t, wall.end.y()   - ny * half_t},
            {wall.start.x() - nx * half_t, wall.start.y() - ny * half_t},
        };
        const int p = emit_extruded_product("IFCWALLSTANDARDCASE", poly, 0.0,
                                            wall.height, "Wall");
        if (p) hosted_products.push_back(p);
    }

    // Slabs
    for (const auto& [id, slab] : doc.slabs()) {
        if (slab.outline.size() < 3) continue;
        std::vector<std::pair<double, double>> poly;
        poly.reserve(slab.outline.size());
        for (const auto& v : slab.outline) poly.emplace_back(v.x(), v.y());
        const int p = emit_extruded_product("IFCSLAB", poly, slab.level,
                                            slab.thickness, "Slab");
        if (p) hosted_products.push_back(p);
    }

    // Boxes (treated as generic proxy elements — furniture / fixtures)
    for (const auto& [id, b] : doc.boxes()) {
        const double hx = b.size_xy.x() * 0.5;
        const double hy = b.size_xy.y() * 0.5;
        const double c = std::cos(b.rotation_z);
        const double s = std::sin(b.rotation_z);
        auto rot = [&](double x, double y) {
            return std::pair<double, double>{
                b.position.x() + c * x - s * y,
                b.position.y() + s * x + c * y};
        };
        const std::vector<std::pair<double, double>> poly{
            rot(-hx, -hy), rot(hx, -hy), rot(hx, hy), rot(-hx, hy)};
        const int p = emit_extruded_product("IFCBUILDINGELEMENTPROXY", poly,
                                            b.base_z, b.height, "Box");
        if (p) hosted_products.push_back(p);
    }

    // Cylinders -> columns (circular)
    for (const auto& [id, c] : doc.cylinders()) {
        const int p = emit_circle_product("IFCCOLUMN", c.position.x(), c.position.y(),
                                          c.radius, c.base_z, c.height, "Column");
        if (p) hosted_products.push_back(p);
    }

    if (!hosted_products.empty()) {
        QStringList refs;
        for (int p : hosted_products) refs << w.ref(p);
        w.write_line(QStringLiteral(
                   "IFCRELCONTAINEDINSPATIALSTRUCTURE(%1,%2,$,$,(%3),%4)")
                   .arg(w.guid(), w.ref(owner_history), refs.join(','), w.ref(storey)));
    }

    out << "ENDSEC;\n";
    out << "END-ISO-10303-21;\n";

    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

}  // namespace cadino::ui
