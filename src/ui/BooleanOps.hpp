#pragma once

#include <optional>

#include "Selection.hpp"
#include "entity/MeshGeometry.hpp"

namespace cadino::core {
class Document;
}

namespace cadino::ui {

std::optional<cadino::core::MeshGeometry>
subtract_entities(const cadino::core::Document& doc, Selection target, Selection tool);

std::optional<cadino::core::MeshGeometry>
union_entities(const cadino::core::Document& doc, Selection a, Selection b);

std::optional<cadino::core::MeshGeometry>
intersect_entities(const cadino::core::Document& doc, Selection a, Selection b);

}  // namespace cadino::ui
