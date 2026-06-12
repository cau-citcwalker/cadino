#pragma once

#include <TopoDS_Shape.hxx>

namespace cadino::core {
class Document;
struct Box;
struct Cylinder;
struct Wall;
struct Slab;
}  // namespace cadino::core

namespace cadino::ui {

TopoDS_Shape shape_from_box(const cadino::core::Box& b);
TopoDS_Shape shape_from_cylinder(const cadino::core::Cylinder& c);
TopoDS_Shape shape_from_wall(const cadino::core::Wall& w);
TopoDS_Shape shape_from_slab(const cadino::core::Slab& s);

}  // namespace cadino::ui
