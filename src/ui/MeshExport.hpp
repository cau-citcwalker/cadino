#pragma once

#include <QString>

namespace cadino::core {
class Document;
}

namespace cadino::ui {

bool export_as_obj(const cadino::core::Document& doc, const QString& path,
                   QString* error = nullptr);

bool export_as_stl(const cadino::core::Document& doc, const QString& path,
                   QString* error = nullptr);

}  // namespace cadino::ui
