#pragma once

#include <QString>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

bool save_document_to_file(const cadino::core::Document& doc, const QString& path,
                           QString* error = nullptr);

bool load_document_from_file(cadino::core::Document& doc, const QString& path,
                             QString* error = nullptr);

}  // namespace cadino::ui
