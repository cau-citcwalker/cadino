#include "BooleanOps.hpp"

#include <cmath>

#include <Eigen/Geometry>

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "document/Document.hpp"

namespace cadino::ui {

namespace {

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

TopoDS_Shape shape_from_selection(const cadino::core::Document& doc, Selection sel) {
    if (sel.kind == SelectKind::Box) {
        if (const auto* b = doc.find_box(sel.id)) return shape_from_box(*b);
    } else if (sel.kind == SelectKind::Cylinder) {
        if (const auto* c = doc.find_cylinder(sel.id)) return shape_from_cylinder(*c);
    }
    return {};
}

cadino::core::MeshGeometry triangulate(TopoDS_Shape shape, double deflection = 5.0) {
    BRepMesh_IncrementalMesh(shape, deflection);

    cadino::core::MeshGeometry mesh;
    TopExp_Explorer explorer(shape, TopAbs_FACE);
    for (; explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;

        const auto offset = static_cast<std::uint32_t>(mesh.positions.size());
        const gp_Trsf trsf = loc.Transformation();
        for (int i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(trsf);
            mesh.positions.emplace_back(static_cast<float>(p.X()),
                                        static_cast<float>(p.Y()),
                                        static_cast<float>(p.Z()));
        }
        const bool reverse = face.Orientation() == TopAbs_REVERSED;
        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            const Poly_Triangle t = tri->Triangle(i);
            int a, b, c;
            t.Get(a, b, c);
            if (reverse) std::swap(b, c);
            mesh.indices.push_back(offset + static_cast<std::uint32_t>(a - 1));
            mesh.indices.push_back(offset + static_cast<std::uint32_t>(b - 1));
            mesh.indices.push_back(offset + static_cast<std::uint32_t>(c - 1));
        }
    }

    mesh.normals.assign(mesh.positions.size(), Eigen::Vector3f::Zero());
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto& v0 = mesh.positions[mesh.indices[i]];
        const auto& v1 = mesh.positions[mesh.indices[i + 1]];
        const auto& v2 = mesh.positions[mesh.indices[i + 2]];
        const Eigen::Vector3f e1 = v1 - v0;
        const Eigen::Vector3f e2 = v2 - v0;
        Eigen::Vector3f n = e1.cross(e2);
        const float ln = n.norm();
        if (ln > 1e-9f) n /= ln;
        mesh.normals[mesh.indices[i]]     += n;
        mesh.normals[mesh.indices[i + 1]] += n;
        mesh.normals[mesh.indices[i + 2]] += n;
    }
    for (auto& n : mesh.normals) {
        const float ln = n.norm();
        if (ln > 1e-9f) n /= ln;
    }
    return mesh;
}

}  // namespace

std::optional<cadino::core::MeshGeometry>
subtract_entities(const cadino::core::Document& doc, Selection target, Selection tool) {
    TopoDS_Shape a = shape_from_selection(doc, target);
    TopoDS_Shape b = shape_from_selection(doc, tool);
    if (a.IsNull() || b.IsNull()) return std::nullopt;

    BRepAlgoAPI_Cut cut(a, b);
    cut.Build();
    if (!cut.IsDone()) return std::nullopt;

    return triangulate(cut.Shape());
}

std::optional<cadino::core::MeshGeometry>
union_entities(const cadino::core::Document& doc, Selection a_sel, Selection b_sel) {
    TopoDS_Shape a = shape_from_selection(doc, a_sel);
    TopoDS_Shape b = shape_from_selection(doc, b_sel);
    if (a.IsNull() || b.IsNull()) return std::nullopt;

    BRepAlgoAPI_Fuse fuse(a, b);
    fuse.Build();
    if (!fuse.IsDone()) return std::nullopt;

    return triangulate(fuse.Shape());
}

std::optional<cadino::core::MeshGeometry>
intersect_entities(const cadino::core::Document& doc, Selection a_sel, Selection b_sel) {
    TopoDS_Shape a = shape_from_selection(doc, a_sel);
    TopoDS_Shape b = shape_from_selection(doc, b_sel);
    if (a.IsNull() || b.IsNull()) return std::nullopt;

    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) return std::nullopt;

    return triangulate(common.Shape());
}

}  // namespace cadino::ui
