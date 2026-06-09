#include "RhinoIO.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <Eigen/Geometry>
#include <spdlog/spdlog.h>

// openNURBS pulls a lot of macros (notably min/max). Defend the rest of the
// translation unit by including it first.
#include <opennurbs.h>

#include "document/Document.hpp"

namespace cadino::io {

namespace {

class OpenNurbsGuard {
public:
    OpenNurbsGuard() {
        if (s_refcount++ == 0) {
            ON::Begin();
        }
    }
    ~OpenNurbsGuard() {
        if (--s_refcount == 0) {
            ON::End();
        }
    }
    OpenNurbsGuard(const OpenNurbsGuard&) = delete;
    OpenNurbsGuard& operator=(const OpenNurbsGuard&) = delete;

private:
    inline static int s_refcount = 0;
};

ON_Color color_to_on(const cadino::core::Color& c) {
    return ON_Color(static_cast<int>(c.r * 255.0f + 0.5f),
                    static_cast<int>(c.g * 255.0f + 0.5f),
                    static_cast<int>(c.b * 255.0f + 0.5f));
}

cadino::core::Color color_from_on(ON_Color c) {
    return {c.Red() / 255.0f, c.Green() / 255.0f, c.Blue() / 255.0f};
}

ON_3dmObjectAttributes* make_attrs(ON_Color color, const wchar_t* name = nullptr) {
    auto* attrs = new ON_3dmObjectAttributes();
    attrs->m_color = color;
    attrs->SetColorSource(ON::object_color_source::color_from_object);
    if (name && *name) attrs->m_name = name;
    return attrs;
}

// Tessellator that builds an ON_Mesh from a set of quads. The public openNURBS
// distribution does not ship a meshing engine for NURBS Breps, so we emit
// primitives directly as ON_Mesh to guarantee a round-trip with Rhino.
struct MeshBuilder {
    ON_Mesh* mesh = new ON_Mesh();
    int next_index = 0;

    int add_vertex(double x, double y, double z, ON_3fVector normal) {
        mesh->m_V.AppendNew() = ON_3fPoint(float(x), float(y), float(z));
        mesh->m_N.AppendNew() = normal;
        return next_index++;
    }

    void add_quad(int a, int b, int c, int d) {
        ON_MeshFace f;
        f.vi[0] = a; f.vi[1] = b; f.vi[2] = c; f.vi[3] = d;
        mesh->m_F.AppendNew() = f;
    }

    void add_tri(int a, int b, int c) { add_quad(a, b, c, c); }
};

void add_oriented_box_mesh(ONX_Model& model, double cx, double cy, double cz_min, double cz_max,
                           double hx, double hy, double yaw, ON_Color color, const wchar_t* name) {
    const double cos_y = std::cos(yaw);
    const double sin_y = std::sin(yaw);
    auto pt = [&](double lx, double ly, double z) {
        return std::array<double, 3>{cx + cos_y * lx - sin_y * ly,
                                     cy + sin_y * lx + cos_y * ly, z};
    };

    MeshBuilder mb;
    auto face = [&](std::array<double, 3> p0, std::array<double, 3> p1,
                    std::array<double, 3> p2, std::array<double, 3> p3, ON_3fVector n) {
        const int a = mb.add_vertex(p0[0], p0[1], p0[2], n);
        const int b = mb.add_vertex(p1[0], p1[1], p1[2], n);
        const int c = mb.add_vertex(p2[0], p2[1], p2[2], n);
        const int d = mb.add_vertex(p3[0], p3[1], p3[2], n);
        mb.add_quad(a, b, c, d);
    };

    const ON_3fVector xn(float(cos_y), float(sin_y), 0.0f);
    const ON_3fVector yn(float(-sin_y), float(cos_y), 0.0f);

    auto b0 = pt(-hx, -hy, cz_min);
    auto b1 = pt( hx, -hy, cz_min);
    auto b2 = pt( hx,  hy, cz_min);
    auto b3 = pt(-hx,  hy, cz_min);
    auto t0 = pt(-hx, -hy, cz_max);
    auto t1 = pt( hx, -hy, cz_max);
    auto t2 = pt( hx,  hy, cz_max);
    auto t3 = pt(-hx,  hy, cz_max);

    face(t0, t1, t2, t3, ON_3fVector(0, 0, 1));
    face(b3, b2, b1, b0, ON_3fVector(0, 0, -1));
    face(b1, b2, t2, t1, xn);
    face(b3, b0, t0, t3, -xn);
    face(b2, b3, t3, t2, yn);
    face(b0, b1, t1, t0, -yn);

    model.AddManagedModelGeometryComponent(mb.mesh, make_attrs(color, name));
}

void add_box_to_model(ONX_Model& model, const cadino::core::Box& b) {
    add_oriented_box_mesh(model, b.position.x(), b.position.y(), b.base_z, b.base_z + b.height,
                          b.size_xy.x() * 0.5, b.size_xy.y() * 0.5, b.rotation_z,
                          color_to_on(b.color), L"box");
}

void add_wall_to_model(ONX_Model& model, const cadino::core::Wall& w) {
    const double sx = w.start.x(), sy = w.start.y();
    const double ex = w.end.x(), ey = w.end.y();
    const double dx = ex - sx, dy = ey - sy;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6) return;
    const double yaw = std::atan2(dy, dx);
    const double cx = (sx + ex) * 0.5;
    const double cy = (sy + ey) * 0.5;
    add_oriented_box_mesh(model, cx, cy, 0.0, w.height, len * 0.5, w.thickness * 0.5, yaw,
                          color_to_on(w.color), L"wall");
}

void add_slab_to_model(ONX_Model& model, const cadino::core::Slab& s) {
    if (s.outline.size() < 3) return;
    double minx = s.outline[0].x(), miny = s.outline[0].y();
    double maxx = minx, maxy = miny;
    for (const auto& v : s.outline) {
        minx = std::min(minx, v.x()); miny = std::min(miny, v.y());
        maxx = std::max(maxx, v.x()); maxy = std::max(maxy, v.y());
    }
    const double cx = (minx + maxx) * 0.5;
    const double cy = (miny + maxy) * 0.5;
    add_oriented_box_mesh(model, cx, cy, s.level, s.level + s.thickness,
                          (maxx - minx) * 0.5, (maxy - miny) * 0.5, 0.0,
                          ON_Color(184, 166, 140), L"slab");
}

void add_cylinder_to_model(ONX_Model& model, const cadino::core::Cylinder& cyl,
                           int segments = 48) {
    const double pi = 3.14159265358979323846;
    const double r = cyl.radius;
    const double z0 = cyl.base_z;
    const double z1 = cyl.base_z + cyl.height;

    MeshBuilder mb;
    const int top_center = mb.add_vertex(cyl.position.x(), cyl.position.y(), z1,
                                         ON_3fVector(0, 0, 1));
    const int bot_center = mb.add_vertex(cyl.position.x(), cyl.position.y(), z0,
                                         ON_3fVector(0, 0, -1));

    std::vector<int> side_bot(segments), side_top(segments);
    std::vector<int> cap_bot(segments), cap_top(segments);
    for (int i = 0; i < segments; ++i) {
        const double a = 2.0 * pi * i / segments;
        const double ca = std::cos(a), sa = std::sin(a);
        const double x = cyl.position.x() + r * ca;
        const double y = cyl.position.y() + r * sa;
        const ON_3fVector n_side(float(ca), float(sa), 0.0f);
        side_bot[i] = mb.add_vertex(x, y, z0, n_side);
        side_top[i] = mb.add_vertex(x, y, z1, n_side);
        cap_top[i] = mb.add_vertex(x, y, z1, ON_3fVector(0, 0, 1));
        cap_bot[i] = mb.add_vertex(x, y, z0, ON_3fVector(0, 0, -1));
    }
    for (int i = 0; i < segments; ++i) {
        const int j = (i + 1) % segments;
        mb.add_quad(side_bot[i], side_bot[j], side_top[j], side_top[i]);
        mb.add_tri(top_center, cap_top[i], cap_top[j]);
        mb.add_tri(bot_center, cap_bot[j], cap_bot[i]);
    }

    model.AddManagedModelGeometryComponent(mb.mesh,
                                           make_attrs(color_to_on(cyl.color), L"cylinder"));
}

void add_mesh_to_model(ONX_Model& model, const cadino::core::MeshGeometry& m) {
    if (m.indices.empty() || m.positions.empty()) return;

    ON_Mesh* mesh = new ON_Mesh(static_cast<int>(m.indices.size() / 3),
                                static_cast<int>(m.positions.size()),
                                /*has_vertex_normals*/ !m.normals.empty(),
                                /*has_texture_coords*/ false);

    for (const auto& p : m.positions) {
        mesh->m_V.AppendNew() = ON_3fPoint(p.x(), p.y(), p.z());
    }
    if (!m.normals.empty()) {
        for (const auto& n : m.normals) {
            mesh->m_N.AppendNew() = ON_3fVector(n.x(), n.y(), n.z());
        }
    }
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        ON_MeshFace f;
        f.vi[0] = static_cast<int>(m.indices[i]);
        f.vi[1] = static_cast<int>(m.indices[i + 1]);
        f.vi[2] = static_cast<int>(m.indices[i + 2]);
        f.vi[3] = f.vi[2];  // degenerate quad encoding for a triangle
        mesh->m_F.AppendNew() = f;
    }
    if (m.normals.empty()) mesh->ComputeVertexNormals();

    model.AddManagedModelGeometryComponent(mesh, make_attrs(color_to_on(m.color), L"mesh"));
}

cadino::core::MeshGeometry mesh_from_on(const ON_Mesh& src, ON_Color color) {
    cadino::core::MeshGeometry out;
    out.color = color_from_on(color);

    out.positions.reserve(src.m_V.Count());
    for (int i = 0; i < src.m_V.Count(); ++i) {
        const ON_3fPoint& p = src.m_V[i];
        out.positions.emplace_back(p.x, p.y, p.z);
    }

    if (src.m_N.Count() == src.m_V.Count()) {
        out.normals.reserve(src.m_N.Count());
        for (int i = 0; i < src.m_N.Count(); ++i) {
            const ON_3fVector& n = src.m_N[i];
            out.normals.emplace_back(n.x, n.y, n.z);
        }
    }

    out.indices.reserve(src.m_F.Count() * 6);
    for (int i = 0; i < src.m_F.Count(); ++i) {
        const ON_MeshFace& f = src.m_F[i];
        out.indices.push_back(static_cast<std::uint32_t>(f.vi[0]));
        out.indices.push_back(static_cast<std::uint32_t>(f.vi[1]));
        out.indices.push_back(static_cast<std::uint32_t>(f.vi[2]));
        if (f.IsQuad()) {
            out.indices.push_back(static_cast<std::uint32_t>(f.vi[0]));
            out.indices.push_back(static_cast<std::uint32_t>(f.vi[2]));
            out.indices.push_back(static_cast<std::uint32_t>(f.vi[3]));
        }
    }

    if (out.normals.empty()) {
        out.normals.assign(out.positions.size(), Eigen::Vector3f::Zero());
        for (std::size_t i = 0; i + 2 < out.indices.size(); i += 3) {
            const Eigen::Vector3f v0 = out.positions[out.indices[i]];
            const Eigen::Vector3f v1 = out.positions[out.indices[i + 1]];
            const Eigen::Vector3f v2 = out.positions[out.indices[i + 2]];
            const Eigen::Vector3f e1 = v1 - v0;
            const Eigen::Vector3f e2 = v2 - v0;
            Eigen::Vector3f n = e1.cross(e2);
            const float l = n.norm();
            if (l > 1e-9f) n /= l;
            out.normals[out.indices[i]] += n;
            out.normals[out.indices[i + 1]] += n;
            out.normals[out.indices[i + 2]] += n;
        }
        for (auto& n : out.normals) {
            const float l = n.norm();
            if (l > 1e-9f) n /= l;
        }
    }
    return out;
}

}  // namespace

bool export_3dm(const cadino::core::Document& doc, const std::string& path,
                std::string* error) {
    OpenNurbsGuard guard;

    ONX_Model model;
    model.m_properties.m_Application.m_application_name = L"Cadino";
    model.m_properties.m_Application.m_application_URL = L"https://github.com/cau-citcwalker/cadino";
    model.m_properties.m_Application.m_application_details = L"Unified CAD + NURBS modeling for interior design";

    for (const auto& [id, w] : doc.walls()) add_wall_to_model(model, w);
    for (const auto& [id, b] : doc.boxes()) add_box_to_model(model, b);
    for (const auto& [id, c] : doc.cylinders()) add_cylinder_to_model(model, c);
    for (const auto& [id, s] : doc.slabs()) add_slab_to_model(model, s);
    for (const auto& [id, m] : doc.meshes()) add_mesh_to_model(model, m);

    if (!model.Write(path.c_str(), 70 /* Rhino 7 file version */)) {
        if (error) *error = "openNURBS failed to write " + path;
        return false;
    }
    const unsigned int count = model.ActiveComponentCount(ON_ModelComponent::Type::ModelGeometry);
    spdlog::info("Exported {} objects to {}", count, path.c_str());
    return true;
}

bool import_3dm(cadino::core::Document& doc, const std::string& path,
                std::string* error) {
    OpenNurbsGuard guard;

    ONX_Model model;
    if (!model.Read(path.c_str())) {
        if (error) *error = "openNURBS failed to read " + path;
        return false;
    }

    int imported = 0;
    int skipped_no_mesh = 0;
    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::ModelGeometry);
    for (const ON_ModelComponent* mc = it.FirstComponent(); mc; mc = it.NextComponent()) {
        const auto* mg = ON_ModelGeometryComponent::Cast(mc);
        if (!mg) continue;
        const ON_Geometry* geom = mg->Geometry(nullptr);
        if (!geom) continue;

        ON_Color color = ON_Color(180, 180, 184);
        if (const auto* attrs = mg->Attributes(nullptr)) {
            color = attrs->m_color;
        }

        if (const ON_Mesh* mesh = ON_Mesh::Cast(geom)) {
            doc.add_mesh(mesh_from_on(*mesh, color));
            ++imported;
            continue;
        }
        // openNURBS public has no meshing engine, so we can only use cached
        // render meshes embedded in Brep / Extrusion objects (Rhino bakes them
        // on save). Geometry without a cached render mesh is silently skipped.
        const ON_Brep* brep = ON_Brep::Cast(geom);
        ON_Brep* tmp_brep = nullptr;
        if (!brep) {
            if (const ON_Extrusion* ext = ON_Extrusion::Cast(geom)) {
                tmp_brep = ext->BrepForm(nullptr);
                brep = tmp_brep;
            }
        }
        if (brep) {
            ON_SimpleArray<const ON_Mesh*> meshes;
            const int n = brep->GetMesh(ON::mesh_type::render_mesh, meshes);
            if (n == 0) {
                ++skipped_no_mesh;
            } else {
                for (int i = 0; i < n; ++i) {
                    if (meshes[i]) {
                        doc.add_mesh(mesh_from_on(*meshes[i], color));
                        ++imported;
                    }
                }
            }
        }
        delete tmp_brep;
    }

    if (skipped_no_mesh > 0) {
        spdlog::warn("Skipped {} Brep objects without cached render meshes", skipped_no_mesh);
    }
    spdlog::info("Imported {} mesh entities from {}", imported, path.c_str());
    return true;
}

}  // namespace cadino::io
