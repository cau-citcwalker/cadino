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

QJsonObject to_json(const cadino::core::Layer& l) {
    QJsonObject o;
    o["id"] = qint64(l.id.value);
    o["name"] = QString::fromStdString(l.name);
    o["color"] = color_array(l.color);
    o["visible"] = l.visible;
    o["locked"] = l.locked;
    return o;
}

cadino::core::Layer layer_from(const QJsonObject& o) {
    cadino::core::Layer l;
    l.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    l.name = o["name"].toString().toStdString();
    if (o.contains("color")) l.color = color_from(o["color"].toArray());
    l.visible = o["visible"].toBool(true);
    l.locked = o["locked"].toBool(false);
    return l;
}

QJsonObject to_json(const cadino::core::Wall& w) {
    QJsonObject o;
    o["id"] = qint64(w.id.value);
    o["group_id"] = qint64(w.group_id.value);
    o["layer_id"] = qint64(w.layer_id.value);
    o["start"] = vec2_array(w.start.x(), w.start.y());
    o["end"] = vec2_array(w.end.x(), w.end.y());
    o["height"] = w.height;
    o["thickness"] = w.thickness;
    o["color"] = color_array(w.color);
    o["roughness"] = w.roughness;
    o["metallic"] = w.metallic;
    o["pattern"] = w.pattern;
    o["texture_path"] = QString::fromStdString(w.texture_path);
    return o;
}

QJsonObject to_json(const cadino::core::Box& b) {
    QJsonObject o;
    o["id"] = qint64(b.id.value);
    o["group_id"] = qint64(b.group_id.value);
    o["layer_id"] = qint64(b.layer_id.value);
    o["position"] = vec2_array(b.position.x(), b.position.y());
    o["size_xy"] = vec2_array(b.size_xy.x(), b.size_xy.y());
    o["height"] = b.height;
    o["base_z"] = b.base_z;
    o["rotation_z"] = b.rotation_z;
    o["color"] = color_array(b.color);
    o["roughness"] = b.roughness;
    o["metallic"] = b.metallic;
    o["pattern"] = b.pattern;
    o["texture_path"] = QString::fromStdString(b.texture_path);
    return o;
}

QJsonObject to_json(const cadino::core::Block& bl) {
    QJsonObject o;
    o["id"] = qint64(bl.id.value);
    o["group_id"] = qint64(bl.group_id.value);
    o["layer_id"] = qint64(bl.layer_id.value);
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
        bo["pattern"] = b.pattern;
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
        co["pattern"] = c.pattern;
        cyls_arr.append(co);
    }
    o["cylinders"] = cyls_arr;
    return o;
}

cadino::core::Block block_from(const QJsonObject& o) {
    cadino::core::Block bl;
    bl.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    bl.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    bl.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
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
        b.pattern = bo["pattern"].toInt(b.pattern);
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
        c.pattern = co["pattern"].toInt(c.pattern);
        bl.cylinders.push_back(c);
    }
    return bl;
}

QJsonObject box_to_json_local(const cadino::core::Box& b) {
    QJsonObject bo;
    bo["position"] = vec2_array(b.position.x(), b.position.y());
    bo["size_xy"] = vec2_array(b.size_xy.x(), b.size_xy.y());
    bo["height"] = b.height;
    bo["base_z"] = b.base_z;
    bo["rotation_z"] = b.rotation_z;
    bo["color"] = color_array(b.color);
    bo["roughness"] = b.roughness;
    bo["metallic"] = b.metallic;
    bo["pattern"] = b.pattern;
    return bo;
}

QJsonObject cyl_to_json_local(const cadino::core::Cylinder& c) {
    QJsonObject co;
    co["position"] = vec2_array(c.position.x(), c.position.y());
    co["radius"] = c.radius;
    co["height"] = c.height;
    co["base_z"] = c.base_z;
    co["color"] = color_array(c.color);
    co["roughness"] = c.roughness;
    co["metallic"] = c.metallic;
    co["pattern"] = c.pattern;
    return co;
}

cadino::core::Box box_local_from(const QJsonObject& bo) {
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
    b.pattern = bo["pattern"].toInt(b.pattern);
    return b;
}

cadino::core::Cylinder cyl_local_from(const QJsonObject& co) {
    cadino::core::Cylinder c;
    const auto pp = co["position"].toArray();
    if (pp.size() >= 2) c.position = {pp[0].toDouble(), pp[1].toDouble()};
    c.radius = co["radius"].toDouble(c.radius);
    c.height = co["height"].toDouble(c.height);
    c.base_z = co["base_z"].toDouble(c.base_z);
    if (co.contains("color")) c.color = color_from(co["color"].toArray());
    c.roughness = static_cast<float>(co["roughness"].toDouble(c.roughness));
    c.metallic = static_cast<float>(co["metallic"].toDouble(c.metallic));
    c.pattern = co["pattern"].toInt(c.pattern);
    return c;
}

QJsonObject to_json(const cadino::core::BlockDefinition& def) {
    QJsonObject o;
    o["id"] = qint64(def.id.value);
    o["name"] = QString::fromStdString(def.name);
    QJsonArray bs;
    for (const auto& b : def.boxes) bs.append(box_to_json_local(b));
    o["boxes"] = bs;
    QJsonArray cs;
    for (const auto& c : def.cylinders) cs.append(cyl_to_json_local(c));
    o["cylinders"] = cs;
    return o;
}

cadino::core::BlockDefinition block_def_from(const QJsonObject& o) {
    cadino::core::BlockDefinition def;
    def.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    def.name = o["name"].toString().toStdString();
    for (const auto& v : o["boxes"].toArray()) def.boxes.push_back(box_local_from(v.toObject()));
    for (const auto& v : o["cylinders"].toArray()) def.cylinders.push_back(cyl_local_from(v.toObject()));
    return def;
}

QJsonObject to_json(const cadino::core::BlockInstance& inst) {
    QJsonObject o;
    o["id"] = qint64(inst.id.value);
    o["group_id"] = qint64(inst.group_id.value);
    o["layer_id"] = qint64(inst.layer_id.value);
    o["definition_id"] = qint64(inst.definition_id.value);
    o["position"] = vec2_array(inst.position.x(), inst.position.y());
    o["rotation_z"] = inst.rotation_z;
    o["base_z"] = inst.base_z;
    return o;
}

cadino::core::BlockInstance block_instance_from(const QJsonObject& o) {
    cadino::core::BlockInstance inst;
    inst.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    inst.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    inst.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    inst.definition_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["definition_id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    if (p.size() >= 2) inst.position = {p[0].toDouble(), p[1].toDouble()};
    inst.rotation_z = o["rotation_z"].toDouble(inst.rotation_z);
    inst.base_z = o["base_z"].toDouble(inst.base_z);
    return inst;
}

QJsonObject to_json(const cadino::core::NurbsSurface& ns) {
    QJsonObject o;
    o["id"] = qint64(ns.id.value);
    o["group_id"] = qint64(ns.group_id.value);
    o["layer_id"] = qint64(ns.layer_id.value);
    o["degree_u"] = ns.degree_u;
    o["degree_v"] = ns.degree_v;
    o["rows"] = ns.rows;
    o["cols"] = ns.cols;
    o["color"] = color_array(ns.color);
    o["roughness"] = ns.roughness;
    o["metallic"] = ns.metallic;
    o["pattern"] = ns.pattern;
    QJsonArray pts;
    for (const auto& p : ns.control_points) {
        QJsonArray a;
        a << p.x() << p.y() << p.z();
        pts.append(a);
    }
    o["control_points"] = pts;
    return o;
}

cadino::core::NurbsSurface surface_from(const QJsonObject& o) {
    cadino::core::NurbsSurface ns;
    ns.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    ns.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    ns.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    ns.degree_u = o["degree_u"].toInt(ns.degree_u);
    ns.degree_v = o["degree_v"].toInt(ns.degree_v);
    ns.rows = o["rows"].toInt(0);
    ns.cols = o["cols"].toInt(0);
    if (o.contains("color")) ns.color = color_from(o["color"].toArray());
    ns.roughness = static_cast<float>(o["roughness"].toDouble(ns.roughness));
    ns.metallic = static_cast<float>(o["metallic"].toDouble(ns.metallic));
    ns.pattern = o["pattern"].toInt(ns.pattern);
    for (const auto& v : o["control_points"].toArray()) {
        const auto a = v.toArray();
        if (a.size() >= 3) {
            ns.control_points.emplace_back(a[0].toDouble(), a[1].toDouble(), a[2].toDouble());
        }
    }
    return ns;
}

QJsonObject to_json(const cadino::core::NurbsCurve& nc) {
    QJsonObject o;
    o["id"] = qint64(nc.id.value);
    o["group_id"] = qint64(nc.group_id.value);
    o["layer_id"] = qint64(nc.layer_id.value);
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
    nc.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
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
    o["layer_id"] = qint64(c.layer_id.value);
    o["position"] = vec2_array(c.position.x(), c.position.y());
    o["radius"] = c.radius;
    o["height"] = c.height;
    o["base_z"] = c.base_z;
    o["color"] = color_array(c.color);
    o["roughness"] = c.roughness;
    o["metallic"] = c.metallic;
    o["pattern"] = c.pattern;
    o["texture_path"] = QString::fromStdString(c.texture_path);
    return o;
}

cadino::core::Wall wall_from(const QJsonObject& o) {
    cadino::core::Wall w;
    w.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    w.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    w.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    const auto s = o["start"].toArray();
    const auto e = o["end"].toArray();
    if (s.size() >= 2) w.start = {s[0].toDouble(), s[1].toDouble()};
    if (e.size() >= 2) w.end = {e[0].toDouble(), e[1].toDouble()};
    w.height = o["height"].toDouble(w.height);
    w.thickness = o["thickness"].toDouble(w.thickness);
    if (o.contains("color")) w.color = color_from(o["color"].toArray());
    w.roughness = static_cast<float>(o["roughness"].toDouble(w.roughness));
    w.metallic = static_cast<float>(o["metallic"].toDouble(w.metallic));
    w.pattern = o["pattern"].toInt(w.pattern);
    w.texture_path = o["texture_path"].toString().toStdString();
    return w;
}

cadino::core::Box box_from(const QJsonObject& o) {
    cadino::core::Box b;
    b.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    b.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    b.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
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
    b.pattern = o["pattern"].toInt(b.pattern);
    b.texture_path = o["texture_path"].toString().toStdString();
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
    o["layer_id"] = qint64(s.layer_id.value);
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
    s.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    const auto outline = o["outline"].toArray();
    for (const auto& v : outline) {
        const auto pt = v.toArray();
        if (pt.size() >= 2) s.outline.emplace_back(pt[0].toDouble(), pt[1].toDouble());
    }
    s.level = o["level"].toDouble(s.level);
    s.thickness = o["thickness"].toDouble(s.thickness);
    return s;
}

QJsonObject to_json(const cadino::core::Dimension& d) {
    QJsonObject o;
    o["id"] = qint64(d.id.value);
    o["group_id"] = qint64(d.group_id.value);
    o["layer_id"] = qint64(d.layer_id.value);
    o["start"] = vec2_array(d.start.x(), d.start.y());
    o["end"] = vec2_array(d.end.x(), d.end.y());
    o["offset"] = d.offset;
    o["text_override"] = QString::fromStdString(d.text_override);
    o["color"] = color_array(d.color);
    o["text_height"] = d.text_height;
    o["arrow_size"] = d.arrow_size;
    return o;
}

QJsonObject to_json(const cadino::core::TextAnnotation& t) {
    QJsonObject o;
    o["id"] = qint64(t.id.value);
    o["group_id"] = qint64(t.group_id.value);
    o["layer_id"] = qint64(t.layer_id.value);
    o["position"] = vec2_array(t.position.x(), t.position.y());
    o["text"] = QString::fromStdString(t.text);
    o["height"] = t.height;
    o["rotation_z"] = t.rotation_z;
    o["color"] = color_array(t.color);
    return o;
}

cadino::core::TextAnnotation text_from(const QJsonObject& o) {
    cadino::core::TextAnnotation t;
    t.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    t.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    t.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    if (p.size() >= 2) t.position = {p[0].toDouble(), p[1].toDouble()};
    t.text = o["text"].toString().toStdString();
    t.height = o["height"].toDouble(t.height);
    t.rotation_z = o["rotation_z"].toDouble(t.rotation_z);
    if (o.contains("color")) t.color = color_from(o["color"].toArray());
    return t;
}

cadino::core::Dimension dimension_from(const QJsonObject& o) {
    cadino::core::Dimension d;
    d.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    d.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    d.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    const auto s = o["start"].toArray();
    const auto e = o["end"].toArray();
    if (s.size() >= 2) d.start = {s[0].toDouble(), s[1].toDouble()};
    if (e.size() >= 2) d.end = {e[0].toDouble(), e[1].toDouble()};
    d.offset = o["offset"].toDouble(d.offset);
    d.text_override = o["text_override"].toString().toStdString();
    if (o.contains("color")) d.color = color_from(o["color"].toArray());
    d.text_height = o["text_height"].toDouble(d.text_height);
    d.arrow_size = o["arrow_size"].toDouble(d.arrow_size);
    return d;
}

cadino::core::Cylinder cylinder_from(const QJsonObject& o) {
    cadino::core::Cylinder c;
    c.id = cadino::core::EntityId{static_cast<std::uint64_t>(o["id"].toVariant().toULongLong())};
    c.group_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["group_id"].toVariant().toULongLong())};
    c.layer_id = cadino::core::EntityId{static_cast<std::uint64_t>(o["layer_id"].toVariant().toULongLong())};
    const auto p = o["position"].toArray();
    if (p.size() >= 2) c.position = {p[0].toDouble(), p[1].toDouble()};
    c.radius = o["radius"].toDouble(c.radius);
    c.height = o["height"].toDouble(c.height);
    c.base_z = o["base_z"].toDouble(c.base_z);
    if (o.contains("color")) c.color = color_from(o["color"].toArray());
    c.roughness = static_cast<float>(o["roughness"].toDouble(c.roughness));
    c.metallic = static_cast<float>(o["metallic"].toDouble(c.metallic));
    c.pattern = o["pattern"].toInt(c.pattern);
    c.texture_path = o["texture_path"].toString().toStdString();
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

    QJsonArray surfaces_json;
    for (const auto& [id, s] : doc.surfaces()) surfaces_json.append(to_json(s));
    root["surfaces"] = surfaces_json;

    QJsonArray block_defs_json;
    for (const auto& [id, d] : doc.block_defs()) block_defs_json.append(to_json(d));
    root["block_defs"] = block_defs_json;

    QJsonArray block_instances_json;
    for (const auto& [id, i] : doc.block_instances()) block_instances_json.append(to_json(i));
    root["block_instances"] = block_instances_json;

    QJsonArray dims_json;
    for (const auto& [id, d] : doc.dimensions()) dims_json.append(to_json(d));
    root["dimensions"] = dims_json;

    QJsonArray texts_json;
    for (const auto& [id, t] : doc.texts()) texts_json.append(to_json(t));
    root["texts"] = texts_json;

    QJsonArray layers_json;
    for (const auto& [id, l] : doc.layers()) layers_json.append(to_json(l));
    root["layers"] = layers_json;
    root["active_layer"] = qint64(doc.active_layer().value);
    root["default_layer"] = qint64(doc.default_layer().value);

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
    if (root.contains("layers")) {
        loaded.reset_layers();
        for (const auto& v : root["layers"].toArray()) {
            auto l = layer_from(v.toObject());
            max_id = std::max(max_id, l.id.value);
            loaded.add_layer(std::move(l));
        }
        const auto def_id = cadino::core::EntityId{
            static_cast<std::uint64_t>(root["default_layer"].toVariant().toULongLong())};
        const auto act_id = cadino::core::EntityId{
            static_cast<std::uint64_t>(root["active_layer"].toVariant().toULongLong())};
        loaded.set_default_layer(def_id);
        loaded.set_active_layer(act_id.valid() ? act_id : def_id);
    }
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
    for (const auto& v : root["surfaces"].toArray()) {
        auto s = surface_from(v.toObject());
        max_id = std::max(max_id, s.id.value);
        loaded.add_surface(std::move(s));
    }
    for (const auto& v : root["block_defs"].toArray()) {
        auto d = block_def_from(v.toObject());
        max_id = std::max(max_id, d.id.value);
        loaded.add_block_def(std::move(d));
    }
    for (const auto& v : root["block_instances"].toArray()) {
        auto i = block_instance_from(v.toObject());
        max_id = std::max(max_id, i.id.value);
        loaded.add_block_instance(std::move(i));
    }
    for (const auto& v : root["dimensions"].toArray()) {
        auto d = dimension_from(v.toObject());
        max_id = std::max(max_id, d.id.value);
        loaded.add_dimension(std::move(d));
    }
    for (const auto& v : root["texts"].toArray()) {
        auto t = text_from(v.toObject());
        max_id = std::max(max_id, t.id.value);
        loaded.add_text(std::move(t));
    }

    cadino::core::seed_entity_id_at_least(max_id);
    doc = std::move(loaded);
    return true;
}

}  // namespace cadino::ui
