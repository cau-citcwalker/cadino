#include "DocumentIO.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

constexpr int kFormatVersion = 1;

QJsonArray vec2_array(double x, double y) {
    QJsonArray a;
    a << x << y;
    return a;
}

QJsonArray color_array(const cadino::core::Color& c) {
    QJsonArray a;
    a << c.r << c.g << c.b;
    return a;
}

cadino::core::Color color_from(const QJsonArray& a) {
    cadino::core::Color c;
    if (a.size() >= 3) {
        c.r = static_cast<float>(a[0].toDouble());
        c.g = static_cast<float>(a[1].toDouble());
        c.b = static_cast<float>(a[2].toDouble());
    }
    return c;
}

QJsonObject to_json(const cadino::core::Wall& w) {
    QJsonObject o;
    o["id"] = qint64(w.id.value);
    o["group_id"] = qint64(w.group_id.value);
    o["start"] = vec2_array(w.start.x(), w.start.y());
    o["end"] = vec2_array(w.end.x(), w.end.y());
    o["height"] = w.height;
    o["thickness"] = w.thickness;
    o["color"] = color_array(w.color);
    o["roughness"] = w.roughness;
    o["metallic"] = w.metallic;
    return o;
}

QJsonObject to_json(const cadino::core::Box& b) {
    QJsonObject o;
    o["id"] = qint64(b.id.value);
    o["group_id"] = qint64(b.group_id.value);
    o["position"] = vec2_array(b.position.x(), b.position.y());
    o["size_xy"] = vec2_array(b.size_xy.x(), b.size_xy.y());
    o["height"] = b.height;
    o["base_z"] = b.base_z;
    o["rotation_z"] = b.rotation_z;
    o["color"] = color_array(b.color);
    o["roughness"] = b.roughness;
    o["metallic"] = b.metallic;
    return o;
}

QJsonObject to_json(const cadino::core::Block& bl) {
    QJsonObject o;
    o["id"] = qint64(bl.id.value);
    o["group_id"] = qint64(bl.group_id.value);
    o["name"] = QString::fromStdString(bl.name);
    o["position"] = vec2_array(bl.position.x(), bl.position.y());
    o["rotation_z"] = bl.rotation_z;
    o["base_z"] = bl.base_z;
    QJsonArray boxes_arr;
    for (const auto& b : bl.boxes) {
        QJsonObject bo;
        bo["position"] = vec2_array(b.position.x(), b.position.y());
        bo["size_xy"] = vec2_array(b.size_xy.x(), b.size_xy.y());
        bo["height"] = b.height;
        bo["base_z"] = b.base_z;
        bo["rotation_z"] = b.rotation_z;
        bo["color"] = color_array(b.color);
        bo["roughness"] = b.roughness;
        bo["metallic"] = b.metallic;
        boxes_arr.append(bo);
    }
    o["boxes"] = boxes_arr;
    QJsonArray cyls_arr;
    for (const auto& c : bl.cylinders) {
        QJsonObject co;
        co["position"] = vec2_array(c.position.x(), c.position.y());
        co["radius"] = c.radius;
        co["height"] = c.height;
        co["base_z"] = c.base_z;
        co["color"] = color_array(c.color);
        co["roughness"] = c.roughness;
        co["metallic"] = c.metallic;
        cyls_arr.append(co);
    }
    o["cylinders"] = cyls_arr;
    return o;
}

cadino::core::Block block_from(const QJsonObject& o) {
    cadino::core::Block bl;
    bl.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    bl.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    bl.name = o["name"].toString().toStdString();
    const auto p = o["position"].toArray();
    if (p.size() >= 2) bl.position = {p[0].toDouble(), p[1].toDouble()};
    bl.rotation_z = o["rotation_z"].toDouble(bl.rotation_z);
    bl.base_z = o["base_z"].toDouble(bl.base_z);
    for (const auto& v : o["boxes"].toArray()) {
        const auto bo = v.toObject();
        cadino::core::Box b;
        const auto pp = bo["position"].toArray();
        const auto sz = bo["size_xy"].toArray();
        if (pp.size() >= 2) b.position = {pp[0].toDouble(), pp[1].toDouble()};
        if (sz.size() >= 2) b.size_xy = {sz[0].toDouble(), sz[1].toDouble()};
        b.height = bo["height"].toDouble(b.height);
        b.base_z = bo["base_z"].toDouble(b.base_z);
        b.rotation_z = bo["rotation_z"].toDouble(b.rotation_z);
        if (bo.contains("color")) b.color = color_from(bo["color"].toArray());
        b.roughness = static_cast<float>(bo["roughness"].toDouble(b.roughness));
        b.metallic = static_cast<float>(bo["metallic"].toDouble(b.metallic));
        bl.boxes.push_back(b);
    }
    for (const auto& v : o["cylinders"].toArray()) {
        const auto co = v.toObject();
        cadino::core::Cylinder c;
        const auto pp = co["position"].toArray();
        if (pp.size() >= 2) c.position = {pp[0].toDouble(), pp[1].toDouble()};
        c.radius = co["radius"].toDouble(c.radius);
        c.height = co["height"].toDouble(c.height);
        c.base_z = co["base_z"].toDouble(c.base_z);
        if (co.contains("color")) c.color = color_from(co["color"].toArray());
        c.roughness = static_cast<float>(co["roughness"].toDouble(c.roughness));
        c.metallic = static_cast<float>(co["metallic"].toDouble(c.metallic));
        bl.cylinders.push_back(c);
    }
    return bl;
}

QJsonObject to_json(const cadino::core::NurbsCurve& nc) {
    QJsonObject o;
    o["id"] = qint64(nc.id.value);
    o["group_id"] = qint64(nc.group_id.value);
    o["degree"] = nc.degree;
    o["color"] = color_array(nc.color);
    o["line_width"] = nc.line_width;
    QJsonArray pts;
    for (const auto& p : nc.control_points) {
        QJsonArray a;
        a << p.x() << p.y() << p.z();
        pts.append(a);
    }
    o["control_points"] = pts;
    return o;
}

cadino::core::NurbsCurve curve_from(const QJsonObject& o) {
    cadino::core::NurbsCurve nc;
    nc.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    nc.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    nc.degree = o["degree"].toInt(nc.degree);
    if (o.contains("color")) nc.color = color_from(o["color"].toArray());
    nc.line_width = static_cast<float>(o["line_width"].toDouble(nc.line_width));
    for (const auto& v : o["control_points"].toArray()) {
        const auto a = v.toArray();
        if (a.size() >= 3) {
            nc.control_points.emplace_back(a[0].toDouble(), a[1].toDouble(), a[2].toDouble());
        }
    }
    return nc;
}

QJsonObject to_json(const cadino::core::Cylinder& c) {
    QJsonObject o;
    o["id"] = qint64(c.id.value);
    o["group_id"] = qint64(c.group_id.value);
    o["position"] = vec2_array(c.position.x(), c.position.y());
    o["radius"] = c.radius;
    o["height"] = c.height;
    o["base_z"] = c.base_z;
    o["color"] = color_array(c.color);
    o["roughness"] = c.roughness;
    o["metallic"] = c.metallic;
    return o;
}

cadino::core::Wall wall_from(const QJsonObject& o) {
    cadino::core::Wall w;
    w.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    w.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    const auto s = o["start"].toArray();
    const auto e = o["end"].toArray();
    if (s.size() >= 2) w.start = {s[0].toDouble(), s[1].toDouble()};
    if (e.size() >= 2) w.end = {e[0].toDouble(), e[1].toDouble()};
    w.height = o["height"].toDouble(w.height);
    w.thickness = o["thickness"].toDouble(w.thickness);
    if (o.contains("color")) w.color = color_from(o["color"].toArray());
    w.roughness = static_cast<float>(o["roughness"].toDouble(w.roughness));
    w.metallic = static_cast<float>(o["metallic"].toDouble(w.metallic));
    return w;
}

cadino::core::Box box_from(const QJsonObject& o) {
    cadino::core::Box b;
    b.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    b.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    const auto sz = o["size_xy"].toArray();
    if (p.size() >= 2) b.position = {p[0].toDouble(), p[1].toDouble()};
    if (sz.size() >= 2) b.size_xy = {sz[0].toDouble(), sz[1].toDouble()};
    b.height = o["height"].toDouble(b.height);
    b.base_z = o["base_z"].toDouble(b.base_z);
    b.rotation_z = o["rotation_z"].toDouble(b.rotation_z);
    if (o.contains("color")) b.color = color_from(o["color"].toArray());
    b.roughness = static_cast<float>(o["roughness"].toDouble(b.roughness));
    b.metallic = static_cast<float>(o["metallic"].toDouble(b.metallic));
    return b;
}

QJsonObject to_json(const cadino::core::Door& d) {
    QJsonObject o;
    o["id"] = qint64(d.id.value);
    o["host_wall"] = qint64(d.host_wall.value);
    o["position_along"] = d.position_along;
    o["width"] = d.width;
    o["height"] = d.height;
    o["sill_height"] = d.sill_height;
    return o;
}

QJsonObject to_json(const cadino::core::Window& w) {
    QJsonObject o;
    o["id"] = qint64(w.id.value);
    o["host_wall"] = qint64(w.host_wall.value);
    o["position_along"] = w.position_along;
    o["width"] = w.width;
    o["height"] = w.height;
    o["sill_height"] = w.sill_height;
    return o;
}

QJsonObject to_json(const cadino::core::Slab& s) {
    QJsonObject o;
    o["id"] = qint64(s.id.value);
    QJsonArray outline;
    for (const auto& v : s.outline) outline.append(vec2_array(v.x(), v.y()));
    o["outline"] = outline;
    o["level"] = s.level;
    o["thickness"] = s.thickness;
    return o;
}

cadino::core::Door door_from(const QJsonObject& o) {
    cadino::core::Door d;
    d.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    d.host_wall = cadino::core::EntityId{static_cast<std::uint64_t>(o["host_wall"].toVariant().toULongLong())};
    d.position_along = o["position_along"].toDouble(d.position_along);
    d.width = o["width"].toDouble(d.width);
    d.height = o["height"].toDouble(d.height);
    d.sill_height = o["sill_height"].toDouble(d.sill_height);
    return d;
}

cadino::core::Window window_from(const QJsonObject& o) {
    cadino::core::Window w;
    w.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    w.host_wall = cadino::core::EntityId{static_cast<std::uint64_t>(o["host_wall"].toVariant().toULongLong())};
    w.position_along = o["position_along"].toDouble(w.position_along);
    w.width = o["width"].toDouble(w.width);
    w.height = o["height"].toDouble(w.height);
    w.sill_height = o["sill_height"].toDouble(w.sill_height);
    return w;
}

cadino::core::Slab slab_from(const QJsonObject& o) {
    cadino::core::Slab s;
    s.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    const auto outline = o["outline"].toArray();
    for (const auto& v : outline) {
        const auto pt = v.toArray();
        if (pt.size() >= 2) s.outline.emplace_back(pt[0].toDouble(), pt[1].toDouble());
    }
    s.level = o["level"].toDouble(s.level);
    s.thickness = o["thickness"].toDouble(s.thickness);
    return s;
}

cadino::core::Cylinder cylinder_from(const QJsonObject& o) {
    cadino::core::Cylinder c;
    c.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    c.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    if (p.size() >= 2) c.position = {p[0].toDouble(), p[1].toDouble()};
    c.radius = o["radius"].toDouble(c.radius);
    c.height = o["height"].toDouble(c.height);
    c.base_z = o["base_z"].toDouble(c.base_z);
    if (o.contains("color")) c.color = color_from(o["color"].toArray());
    c.roughness = static_cast<float>(o["roughness"].toDouble(c.roughness));
    c.metallic = static_cast<float>(o["metallic"].toDouble(c.metallic));
    return c;
}

}  // namespace

bool save_document_to_file(const cadino::core::Document& doc, const QString& path,
                           QString* error) {
    QJsonObject root;
    root["format"] = "cadino";
    root["version"] = kFormatVersion;

    QJsonArray walls_json;
    for (const auto& [id, w] : doc.walls()) walls_json.append(to_json(w));
    root["walls"] = walls_json;

    QJsonArray boxes_json;
    for (const auto& [id, b] : doc.boxes()) boxes_json.append(to_json(b));
    root["boxes"] = boxes_json;

    QJsonArray cyls_json;
    for (const auto& [id, c] : doc.cylinders()) cyls_json.append(to_json(c));
    root["cylinders"] = cyls_json;

    QJsonArray doors_json;
    for (const auto& [id, d] : doc.doors()) doors_json.append(to_json(d));
    root["doors"] = doors_json;

    QJsonArray windows_json;
    for (const auto& [id, w] : doc.windows()) windows_json.append(to_json(w));
    root["windows"] = windows_json;

    QJsonArray slabs_json;
    for (const auto& [id, s] : doc.slabs()) slabs_json.append(to_json(s));
    root["slabs"] = slabs_json;

    QJsonArray curves_json;
    for (const auto& [id, c] : doc.curves()) curves_json.append(to_json(c));
    root["curves"] = curves_json;

    QJsonArray blocks_json;
    for (const auto& [id, b] : doc.blocks()) blocks_json.append(to_json(b));
    root["blocks"] = blocks_json;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool load_document_from_file(cadino::core::Document& doc, const QString& path,
                             QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parse_error{};
    const auto json = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        if (error) *error = parse_error.errorString();
        return false;
    }
    const auto root = json.object();
    if (root.value("format").toString() != "cadino") {
        if (error) *error = "Not a Cadino document";
        return false;
    }

    cadino::core::Document loaded;
    std::uint64_t max_id = 0;
    for (const auto& v : root["walls"].toArray()) {
        auto w = wall_from(v.toObject());
        max_id = std::max(max_id, w.id.value);
        loaded.add_wall(std::move(w));
    }
    for (const auto& v : root["boxes"].toArray()) {
        auto b = box_from(v.toObject());
        max_id = std::max(max_id, b.id.value);
        loaded.add_box(std::move(b));
    }
    for (const auto& v : root["cylinders"].toArray()) {
        auto c = cylinder_from(v.toObject());
        max_id = std::max(max_id, c.id.value);
        loaded.add_cylinder(std::move(c));
    }
    for (const auto& v : root["doors"].toArray()) {
        auto d = door_from(v.toObject());
        max_id = std::max(max_id, d.id.value);
        loaded.add_door(std::move(d));
    }
    for (const auto& v : root["windows"].toArray()) {
        auto w = window_from(v.toObject());
        max_id = std::max(max_id, w.id.value);
        loaded.add_window(std::move(w));
    }
    for (const auto& v : root["slabs"].toArray()) {
        auto s = slab_from(v.toObject());
        max_id = std::max(max_id, s.id.value);
        loaded.add_slab(std::move(s));
    }
    for (const auto& v : root["curves"].toArray()) {
        auto c = curve_from(v.toObject());
        max_id = std::max(max_id, c.id.value);
        loaded.add_curve(std::move(c));
    }
    for (const auto& v : root["blocks"].toArray()) {
        auto bl = block_from(v.toObject());
        max_id = std::max(max_id, bl.id.value);
        loaded.add_block(std::move(bl));
    }

    cadino::core::seed_entity_id_at_least(max_id);
    doc = std::move(loaded);
    return true;
}

}  // namespace cadino::ui
