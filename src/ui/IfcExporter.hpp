#pragma once

#include <QString>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

// Writes a minimal IFC4 file describing the document's walls, slabs, boxes
// and cylinders. The export uses a single building storey at z = 0 and emits
// extruded-solid representations in millimetres.
bool export_document_as_ifc(const cadino::core::Document& doc, const QString& path,
                            QString* error = nullptr);

}  // namespace cadino::ui
