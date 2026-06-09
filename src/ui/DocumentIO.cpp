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
    o["start"] = vec2_array(w.start.x(), w.start.y());
    o["end"] = vec2_array(w.end.x(), w.end.y());
    o["height"] = w.height;
    o["thickness"] = w.thickness;
    o["color"] = color_array(w.color);
    return o;
}

QJsonObject to_json(const cadino::core::Box& b) {
    QJsonObject o;
    o["id"] = qint64(b.id.value);
    o["position"] = vec2_array(b.position.x(), b.position.y());
    o["size_xy"] = vec2_array(b.size_xy.x(), b.size_xy.y());
    o["height"] = b.height;
    o["base_z"] = b.base_z;
    o["rotation_z"] = b.rotation_z;
    o["color"] = color_array(b.color);
    return o;
}

QJsonObject to_json(const cadino::core::Cylinder& c) {
    QJsonObject o;
    o["id"] = qint64(c.id.value);
    o["position"] = vec2_array(c.position.x(), c.position.y());
    o["radius"] = c.radius;
    o["height"] = c.height;
    o["base_z"] = c.base_z;
    o["color"] = color_array(c.color);
    return o;
}

cadino::core::Wall wall_from(const QJsonObject& o) {
    cadino::core::Wall w;
    w.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    const auto s = o["start"].toArray();
    const auto e = o["end"].toArray();
    if (s.size() >= 2) w.start = {s[0].toDouble(), s[1].toDouble()};
    if (e.size() >= 2) w.end = {e[0].toDouble(), e[1].toDouble()};
    w.height = o["height"].toDouble(w.height);
    w.thickness = o["thickness"].toDouble(w.thickness);
    if (o.contains("color")) w.color = color_from(o["color"].toArray());
    return w;
}

cadino::core::Box box_from(const QJsonObject& o) {
    cadino::core::Box b;
    b.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    const auto sz = o["size_xy"].toArray();
    if (p.size() >= 2) b.position = {p[0].toDouble(), p[1].toDouble()};
    if (sz.size() >= 2) b.size_xy = {sz[0].toDouble(), sz[1].toDouble()};
    b.height = o["height"].toDouble(b.height);
    b.base_z = o["base_z"].toDouble(b.base_z);
    b.rotation_z = o["rotation_z"].toDouble(b.rotation_z);
    if (o.contains("color")) b.color = color_from(o["color"].toArray());
    return b;
}

cadino::core::Cylinder cylinder_from(const QJsonObject& o) {
    cadino::core::Cylinder c;
    c.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    if (p.size() >= 2) c.position = {p[0].toDouble(), p[1].toDouble()};
    c.radius = o["radius"].toDouble(c.radius);
    c.height = o["height"].toDouble(c.height);
    c.base_z = o["base_z"].toDouble(c.base_z);
    if (o.contains("color")) c.color = color_from(o["color"].toArray());
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

    cadino::core::seed_entity_id_at_least(max_id);
    doc = std::move(loaded);
    return true;
}

}  // namespace cadino::ui
