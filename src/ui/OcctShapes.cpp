#include "OcctShapes.hpp"

#include <algorithm>
#include <cmath>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "entity/Box.hpp"
#include "entity/Cylinder.hpp"
#include "entity/Slab.hpp"
#include "entity/Wall.hpp"

namespace cadino::ui {

TopoDS_Shape shape_from_box(const cadino::core::Box& b) {
    const double hx = b.size_xy.x() * 0.5;
    const double hy = b.size_xy.y() * 0.5;
    BRepPrimAPI_MakeBox maker(gp_Pnt(-hx, -hy, b.base_z),
                              gp_Pnt(hx, hy, b.base_z + b.height));
    TopoDS_Shape s = maker.Shape();

    gp_Trsf rot;
    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), b.rotation_z);
    gp_Trsf trans;
    trans.SetTranslation(gp_Vec(b.position.x(), b.position.y(), 0.0));
    gp_Trsf combined = trans * rot;

    BRepBuilderAPI_Transform t(s, combined);
    return t.Shape();
}

TopoDS_Shape shape_from_cylinder(const cadino::core::Cylinder& c) {
    gp_Ax2 axis(gp_Pnt(c.position.x(), c.position.y(), c.base_z), gp_Dir(0, 0, 1));
    BRepPrimAPI_MakeCylinder maker(axis, c.radius, c.height);
    return maker.Shape();
}

TopoDS_Shape shape_from_wall(const cadino::core::Wall& w) {
    const double dx = w.end.x() - w.start.x();
    const double dy = w.end.y() - w.start.y();
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6) return {};

    const double hx = len * 0.5;
    const double hy = w.thickness * 0.5;
    BRepPrimAPI_MakeBox maker(gp_Pnt(-hx, -hy, 0.0), gp_Pnt(hx, hy, w.height));

    gp_Trsf rot;
    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), std::atan2(dy, dx));
    gp_Trsf trans;
    trans.SetTranslation(gp_Vec((w.start.x() + w.end.x()) * 0.5,
                                (w.start.y() + w.end.y()) * 0.5, 0.0));
    BRepBuilderAPI_Transform t(maker.Shape(), trans * rot);
    return t.Shape();
}

TopoDS_Shape shape_from_slab(const cadino::core::Slab& s) {
    if (s.outline.size() < 3) return {};
    double minx = s.outline[0].x();
    double miny = s.outline[0].y();
    double maxx = minx, maxy = miny;
    for (const auto& v : s.outline) {
        minx = std::min(minx, v.x()); miny = std::min(miny, v.y());
        maxx = std::max(maxx, v.x()); maxy = std::max(maxy, v.y());
    }
    BRepPrimAPI_MakeBox maker(gp_Pnt(minx, miny, s.level),
                              gp_Pnt(maxx, maxy, s.level + s.thickness));
    return maker.Shape();
}

}  // namespace cadino::ui
