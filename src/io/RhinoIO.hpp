#pragma once

#include <string>

namespace cadino::core {
class Document;
}

namespace cadino::io {

bool import_3dm(cadino::core::Document& doc, const std::string& path,
                std::string* error = nullptr);

bool export_3dm(const cadino::core::Document& doc, const std::string& path,
                std::string* error = nullptr);

}  // namespace cadino::io
