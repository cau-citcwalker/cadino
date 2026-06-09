#pragma once

#include <QString>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

bool export_document_as_dxf(const cadino::core::Document& doc, const QString& path,
                            QString* error = nullptr);

}  // namespace cadino::ui
