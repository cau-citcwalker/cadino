#pragma once

#include <QString>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

bool export_document_as_dxf(const cadino::core::Document& doc, const QString& path,
                            QString* error = nullptr);

enum class ElevationPlane {
    Front,  // looking in +Y, project X horizontal / Z vertical
    Back,   // looking in -Y, project -X horizontal / Z vertical
    Left,   // looking in +X, project -Y horizontal / Z vertical
    Right,  // looking in -X, project Y horizontal / Z vertical
};

bool export_elevation_as_dxf(const cadino::core::Document& doc, ElevationPlane plane,
                             const QString& path, QString* error = nullptr);

}  // namespace cadino::ui
